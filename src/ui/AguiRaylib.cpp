#include <ui/AguiRaylib.hpp>

#include <ui/Input.hpp>

#include <Agui/KeyboardInput.hpp>
#include <Agui/MouseInput.hpp>
#include <Agui/ResizableText.hpp>
#include <Agui/SystemClipboard.hpp>
#include <Agui/UTF8.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <charconv>
#include <map>
#include <optional>
#include <string_view>
#include <vector>
#include <cmath>

namespace agui_raylib {

namespace {

::Color ToRaylib(const agui::Color& c, float opacity = 1.0f)
{
  const auto byte = [](float v) { return (unsigned char)std::clamp(int(std::lround(v * 255.0f)), 0, 255); };
  return ::Color{ byte(c.getR()), byte(c.getG()), byte(c.getB()), byte(c.getA() * opacity) };
}

agui::Color Multiply(const agui::Color& a, const agui::Color& b)
{
  return agui::Color(a.getR() * b.getR(), a.getG() * b.getG(), a.getB() * b.getB(), a.getA() * b.getA());
}

bool IsFullScreen(const agui::Rectangle& r) { return r == agui::Graphics::FULL_SCREEN_RECTANGLE; }

// Draws with each texel's colour inverted (its alpha kept): Agui's
// InvertColors, which frame_action_button asks of its white icon when hovered,
// turning it black. Made on first use, when the GL context is up.
const ::Shader& InvertShader()
{
  static const ::Shader shader = LoadShaderFromMemory(nullptr, R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
out vec4 finalColor;
void main()
{
  vec4 texel = texture(texture0, fragTexCoord);
  finalColor = vec4(1.0 - texel.rgb, texel.a) * colDiffuse * fragColor;
})");
  return shader;
}

// A piece drawn bigger than its texels (the 1-texel middle of a 9-slice, say)
// is sampled between texel centres all the way across, so bilinear filtering
// blends in the texels around it -- a flat panel turns into a gradient of its
// neighbours. Pulling the source in by up to half a texel keeps every sample
// inside the piece.
void KeepSamplesInside(float& start, float& size, float destSize)
{
  if (destSize <= size) return;
  const float inset = std::min(0.5f, size * 0.5f - 0.001f);
  start += inset;
  size -= 2 * inset;
}


// ------------------------------------------------------------ rich text ----

std::map<std::string, const RaylibFont*, std::less<>>& Fonts()
{
  static std::map<std::string, const RaylibFont*, std::less<>> fonts;
  return fonts;
}

bool HasTags(std::string_view text) { return text.find('[') != std::string_view::npos; }

// "#rrggbb", or "r,g,b" in 0-255.
std::optional<agui::Color> ParseColor(std::string_view spec)
{
  int rgb[3] = {};
  if (spec.size() == 7 && spec[0] == '#') {
    for (int i = 0; i < 3; ++i) {
      if (std::from_chars(spec.data() + 1 + 2 * i, spec.data() + 3 + 2 * i, rgb[i], 16).ec != std::errc()) return std::nullopt;
    }
  } else {
    const char* at  = spec.data();
    const char* end = spec.data() + spec.size();
    for (int i = 0; i < 3; ++i) {
      const auto [next, error] = std::from_chars(at, end, rgb[i]);
      if (error != std::errc() || (i < 2 ? next == end || *next != ',' : next != end)) return std::nullopt;
      at = next + 1;
    }
  }
  return agui::Color(float(rgb[0]) / 255.0f, float(rgb[1]) / 255.0f, float(rgb[2]) / 255.0f, 1.0f);
}

// Calls onTag(tag, at, end) for each tag of `text` that rich text knows --
// [font=...] naming a registered font, [color=...] that parses, [/font],
// [/color] -- with the tag's text between the brackets and where it is.
template<class OnTag>
void ForEachTag(std::string_view text, OnTag&& onTag)
{
  size_t at = 0;
  while ((at = text.find('[', at)) != std::string_view::npos) {
    const size_t close = text.find(']', at);
    if (close == std::string_view::npos) return;
    const std::string_view tag = text.substr(at + 1, close - at - 1);
    const bool known = (tag.starts_with("font=") && Fonts().contains(tag.substr(5))) ||
                       (tag.starts_with("color=") && ParseColor(tag.substr(6))) || tag == "/font" || tag == "/color";
    if (known) onTag(tag, at, close + 1);
    at = known ? close + 1 : at + 1;  // anything else is just text
  }
}

// Calls piece(text, font, colour) for each stretch of `text` between tags,
// with the font and the colour (if any) the tags around it ask for.
template<class Piece>
void ForEachRun(std::string_view text, const RaylibFont* base, Piece&& piece)
{
  std::vector<const RaylibFont*>          fonts{ base };
  std::vector<std::optional<agui::Color>> colors{ std::nullopt };
  size_t start = 0;
  ForEachTag(text, [&](std::string_view tag, size_t at, size_t end) {
    if (at > start) piece(text.substr(start, at - start), fonts.back(), colors.back());
    start = end;
    if (tag.starts_with("font=")) fonts.push_back(Fonts().find(tag.substr(5))->second);
    else if (tag.starts_with("color=")) colors.push_back(ParseColor(tag.substr(6)));
    else if (tag == "/font" && fonts.size() > 1) fonts.pop_back();
    else if (tag == "/color" && colors.size() > 1) colors.pop_back();
  });
  if (text.size() > start) piece(text.substr(start), fonts.back(), colors.back());
}

// The tags `text` leaves open, added to those open before it: what a line
// wrapped out of the middle of tagged text has to start with.
void TrackOpenTags(std::string_view text, std::vector<std::string>& open)
{
  ForEachTag(text, [&open](std::string_view tag, size_t, size_t) {
    if (tag.front() != '/') {
      open.push_back("[" + std::string(tag) + "]");
      return;
    }
    const std::string_view kind = tag.substr(1);  // "font" or "color"
    for (auto it = open.rbegin(); it != open.rend(); ++it) {
      if (std::string_view(*it).substr(1, kind.size()) == kind) {
        open.erase(std::next(it).base());
        break;
      }
    }
  });
}

// Agui's hook for rich text: text with tags gets RichTextData, and a label
// that has some is laid out with rich widths and drawn through
// Graphics::drawTextLines(ResizableText...), which keeps a line's tags open
// into the next.
class RichTextHandler : public agui::RichTextHandler {
public:
  void getTextDrawSections(std::string_view, const agui::Font*, agui::RichTextSetting, agui::RichTextData&) override {}
  void getTextDrawSections(std::string_view text, const agui::Font* font, agui::RichTextSetting setting,
                           std::unique_ptr<agui::RichTextData>& data) override
  {
    if (setting == agui::RichTextSetting::Disabled || !HasTags(text)) {
      data.reset();
      return;
    }
    if (!data) data = std::make_unique<agui::RichTextData>();
    data->lineHeight = font->getLineHeight();
  }
  void getTextDrawSections(std::string_view, const agui::Font*, agui::RichTextSetting) override {}
};

}  // namespace

// ---------------------------------------------------------------- images ----

RaylibImage::RaylibImage(std::shared_ptr<Texture2D> texture, ::Rectangle region, float scale,
                         agui::Color tint)
    : tex(std::move(texture)), src(region), scale(scale), tintColor(tint)
{
}

// Rounded, and never below one pixel, so a 1-texel stretch piece of a
// half-scale sheet still counts as something to draw.
int RaylibImage::getWidth() const
{
  return this->src.width > 0 ? std::max(1, int(std::lround(this->src.width * this->scale))) : 0;
}

int RaylibImage::getHeight() const
{
  return this->src.height > 0 ? std::max(1, int(std::lround(this->src.height * this->scale))) : 0;
}

RaylibImage::RaylibImage(std::shared_ptr<Texture2D> texture)
    : RaylibImage(texture, ::Rectangle{ 0, 0, float(texture->width), float(texture->height) })
{
}

bool RaylibImage::operator==(const agui::Image& other) const
{
  const auto* o = dynamic_cast<const RaylibImage*>(&other);
  return o && o->tex == this->tex && o->src.x == this->src.x && o->src.y == this->src.y
      && o->src.width == this->src.width && o->src.height == this->src.height && o->scale == this->scale
      && o->tintColor == this->tintColor;
}

std::shared_ptr<Texture2D> MakeSharedTexture(const ::Image& image)
{
  return std::shared_ptr<Texture2D>(new Texture2D(LoadTextureFromImage(image)), [](Texture2D* t) {
    if (IsWindowReady()) UnloadTexture(*t);
    delete t;
  });
}

// ----------------------------------------------------------------- fonts ----

RaylibFont::RaylibFont(const std::string& fileName, int height)
{
  this->reload(fileName, height, 0, 0.0f, agui::Color());
}

RaylibFont::RaylibFont(const ::Font& borrowed, int height, float spacing)
    : font(borrowed), height(height), spacingPx(spacing)
{
}

RaylibFont::~RaylibFont() { this->free(); }

void RaylibFont::free()
{
  if (this->ownsFont && IsWindowReady()) UnloadFont(this->font);
  this->ownsFont = false;
  this->font = ::Font{};
}

void RaylibFont::reload(const std::string& fileName, int newHeight, int, float, const agui::Color&)
{
  this->free();
  this->path = fileName;
  this->height = std::max(1, newHeight);
  if (fileName.empty()) {
    this->font = GetFontDefault();
    // Matches DrawText(), so Agui text sits comfortably next to the HUD.
    this->spacingPx = float(this->height) / 10.0f;
  } else {
    this->font = LoadFontEx(fileName.c_str(), this->height, nullptr, 0);
    this->ownsFont = true;
    this->spacingPx = 0.0f;
    SetTextureFilter(this->font.texture, TEXTURE_FILTER_BILINEAR);
  }
}

::Vector2 RaylibFont::measure(std::string_view text, double scale) const
{
  if (text.empty() || this->font.texture.id == 0) return { 0, float(this->height * scale) };
  const std::string s(text);  // raylib wants a terminated string
  return MeasureTextEx(this->font, s.c_str(), float(this->height * scale), float(this->spacingPx * scale));
}

int RaylibFont::getTextWidth(std::string_view text, agui::RichTextSetting setting, double scale) const
{
  if (setting == agui::RichTextSetting::Disabled || !HasTags(text)) return int(std::ceil(this->measure(text, scale).x));
  // Wrapping measures ever longer prefixes of a line, and one that stops
  // halfway through a tag mustn't count what there is of it as text: that
  // would make the prefix wider than the whole line.
  if (const size_t open = text.rfind('['); open != std::string_view::npos && text.find(']', open) == std::string_view::npos) {
    const std::string_view partial = text.substr(open);
    for (std::string_view tag : { "[font=", "[color=", "[/font]", "[/color]" }) {
      if (tag.starts_with(partial) || partial.starts_with(tag)) {
        text = text.substr(0, open);
        break;
      }
    }
  }
  float width = 0;
  ForEachRun(text, this, [&](std::string_view piece, const RaylibFont* font, const std::optional<agui::Color>&) {
    width += font->measure(piece, scale).x;
  });
  return int(std::ceil(width));
}

int RaylibFont::getSubstringWidth(std::string_view text, const agui::RichTextData&,
                                  agui::RichTextSetting setting, double scale) const
{
  return this->getTextWidth(text, setting, scale);
}

int RaylibFont::getStringIndexFromPosition(std::string_view text, int x, agui::RichTextSetting setting) const
{
  // The character boundary nearest to `x`.
  const int length = int(agui::UTF8::length(text));
  int       last   = 0;
  for (int i = 0; i <= length; ++i) {
    const int width = this->getTextWidth(agui::UTF8::subStr(text, 0, i), setting);
    if (width >= x) return (i == 0 || (last + width) / 2 < x) ? i : i - 1;
    last = width;
  }
  // Past the end: after the last character, but not after a line break.
  return !text.empty() && text.back() == '\n' ? length - 1 : length;
}

size_t RaylibFont::getWrapIndex(std::string_view text, int width, double scale) const
{
  // The longest prefix (in bytes, on a codepoint boundary) that fits in `width`.
  size_t fits = 0;
  size_t i    = 0;
  while (i < text.size()) {
    int bytes = 1;
    GetCodepoint(text.data() + i, &bytes);
    const size_t next = i + size_t(std::max(bytes, 1));
    if (this->getTextWidth(text.substr(0, next), agui::RichTextSetting::Disabled, scale) > width) break;
    fits = i = next;
  }
  return fits;
}

agui::RichTextHandler* RaylibFont::getRichTextHandler() const
{
  static RichTextHandler handler;
  return &handler;
}

void RegisterFont(const std::string& name, const RaylibFont* font)
{
  Fonts()[name] = font;
}

agui::Font* RaylibFontLoader::loadFont(const std::string& fileName, int height, int,
                                       float, const agui::Color&)
{
  return new RaylibFont(fileName, height);
}

// -------------------------------------------------------------- graphics ----

void RaylibGraphics::_beginPaint() {}

void RaylibGraphics::_endPaint()
{
  if (this->scissoring) EndScissorMode();
  this->scissoring = false;
}

agui::Dimension RaylibGraphics::getDisplaySize() const
{
  return agui::Dimension(int(float(GetScreenWidth()) / this->viewScale), int(float(GetScreenHeight()) / this->viewScale));
}

void RaylibGraphics::setClippingRectangle(const agui::Rectangle& rect, bool force)
{
  (force ? this->forcedClip : this->appliedClip) = rect;
  this->applyScissor();
}

agui::Rectangle RaylibGraphics::getClippingRectangle() { return this->clip; }

void RaylibGraphics::applyScissor()
{
  if (IsFullScreen(this->forcedClip))       this->clip = this->appliedClip;
  else if (IsFullScreen(this->appliedClip)) this->clip = this->forcedClip;
  else {
    const int l = std::max(this->forcedClip.getLeft(), this->appliedClip.getLeft());
    const int t = std::max(this->forcedClip.getTop(), this->appliedClip.getTop());
    const int r = std::min(this->forcedClip.getLeft() + this->forcedClip.getWidth(),
                           this->appliedClip.getLeft() + this->appliedClip.getWidth());
    const int b = std::min(this->forcedClip.getTop() + this->forcedClip.getHeight(),
                           this->appliedClip.getTop() + this->appliedClip.getHeight());
    this->clip = agui::Rectangle(l, t, std::max(0, r - l), std::max(0, b - t));
  }

  const agui::Rectangle& cut = this->forcedClip;
  if (IsFullScreen(cut)) {
    if (this->scissoring) EndScissorMode();
    this->scissoring = false;
    return;
  }
  // The clip is in GUI units and the scissor in screen pixels. Rounded
  // outwards, so an edge between two pixels keeps the one it half covers.
  const int left   = int(std::floor(float(cut.getLeft()) * this->viewScale));
  const int top    = int(std::floor(float(cut.getTop()) * this->viewScale));
  const int right  = int(std::ceil(float(cut.getLeft() + std::max(0, cut.getWidth())) * this->viewScale));
  const int bottom = int(std::ceil(float(cut.getTop() + std::max(0, cut.getHeight())) * this->viewScale));
  BeginScissorMode(left, top, right - left, bottom - top);
  this->scissoring = true;
}

void RaylibGraphics::blit(const agui::Image* bmp, ::Rectangle source, ::Rectangle dest,
                          const agui::Color& imageTint, float opacity, float rotationDeg,
                          ::Vector2 origin, agui::InvertColors invert)
{
  const auto* img = static_cast<const RaylibImage*>(bmp);
  if (!img || dest.width == 0 || dest.height == 0) return;
  const agui::Point o = this->getOffset();
  dest.x += float(o.x);
  dest.y += float(o.y);
  // Onto whole screen pixels. At 150% a whole GUI unit can be half a pixel,
  // and a piece whose edge lands there has its first pixels sampled right on
  // that edge -- half from whatever is next to it in the atlas, a bright seam
  // down the side of a frame. Pieces that meet share the edge they snap to,
  // so no gap opens between them.
  if (rotationDeg == 0.0f) {
    const auto snap = [this](float v) { return std::round(v * this->viewScale) / this->viewScale; };
    const float right  = snap(dest.x + dest.width);
    const float bottom = snap(dest.y + dest.height);
    dest.x      = snap(dest.x);
    dest.y      = snap(dest.y);
    dest.width  = right - dest.x;
    dest.height = bottom - dest.y;
    if (dest.width <= 0 || dest.height <= 0) return;
  }
  // Compared in screen pixels: that is where the stretching happens.
  KeepSamplesInside(source.x, source.width, dest.width * this->viewScale);
  KeepSamplesInside(source.y, source.height, dest.height * this->viewScale);
  const bool inverted = invert == agui::InvertColors::True;
  if (inverted) BeginShaderMode(InvertShader());
  DrawTexturePro(img->texture(), source, dest, origin, rotationDeg,
                 ToRaylib(Multiply(Multiply(imageTint, img->tint()), this->tint), opacity));
  if (inverted) EndShaderMode();
}

void RaylibGraphics::drawImage(const agui::Image* bmp, const agui::Point& position,
                               const agui::Point& regionStart, const agui::Dimension& regionSize,
                               const float& opacity)
{
  this->drawTintedImage(bmp, position, regionStart, regionSize, agui::Color(1, 1, 1, 1), opacity);
}

void RaylibGraphics::drawTintedImage(const agui::Image* bmp, const agui::Point& position,
                                     const agui::Point& regionStart,
                                     const agui::Dimension& regionSize, const agui::Color& t,
                                     const float& opacity)
{
  if (!bmp) return;
  const ::Rectangle src = static_cast<const RaylibImage*>(bmp)->texels(
      float(regionStart.x), float(regionStart.y), float(regionSize.width), float(regionSize.height));
  this->blit(bmp, src, { float(position.x), float(position.y), float(regionSize.width), float(regionSize.height) },
       t, opacity);
}

void RaylibGraphics::drawImage(const agui::Image* bmp, const agui::Point& position,
                               const float& opacity, agui::InvertColors invert)
{
  if (!bmp) return;
  this->drawScaledImage(bmp, position, bmp->getDimension(), opacity, invert);
}

void RaylibGraphics::drawScaledImage(const agui::Image* bmp, const agui::Point& position,
                                     const agui::Dimension& scale, const float& opacity,
                                     agui::InvertColors invert)
{
  this->drawScaledTintedImage(bmp, position, scale, agui::Color(1, 1, 1, 1), opacity, invert);
}

void RaylibGraphics::drawScaledImageRegion(const agui::Image* bmp, const agui::Point& position,
                                           const agui::Dimension& scale,
                                           const agui::Rectangle& imageRegion,
                                           const float& opacity, agui::InvertColors invert)
{
  if (!bmp) return;
  const ::Rectangle src = static_cast<const RaylibImage*>(bmp)->texels(
      float(imageRegion.x), float(imageRegion.y), float(imageRegion.width), float(imageRegion.height));
  this->blit(bmp, src, { float(position.x), float(position.y), float(scale.width), float(scale.height) },
       agui::Color(1, 1, 1, 1), opacity, 0.0f, { 0, 0 }, invert);
}

void RaylibGraphics::drawScaledTintedImage(const agui::Image* bmp, const agui::Point& position,
                                           const agui::Dimension& scale, const agui::Color& t,
                                           const float& opacity, agui::InvertColors invert)
{
  if (!bmp) return;
  this->blit(bmp, static_cast<const RaylibImage*>(bmp)->region(),
       { float(position.x), float(position.y), float(scale.width), float(scale.height) }, t,
       opacity, 0.0f, { 0, 0 }, invert);
}

void RaylibGraphics::drawScaledRotatedTintedImage(const agui::Image* bmp, double centerX,
                                                  double centerY, const float orientation,
                                                  const agui::Point& regionStart,
                                                  const agui::Dimension& regionScale,
                                                  const agui::Dimension& scale,
                                                  const agui::Color& t, const float& opacity)
{
  if (!bmp) return;
  const ::Rectangle src = static_cast<const RaylibImage*>(bmp)->texels(
      float(regionStart.x), float(regionStart.y), float(regionScale.width), float(regionScale.height));
  const ::Rectangle dst{ float(centerX), float(centerY), float(scale.width), float(scale.height) };
  // Agui orientations are fractions of a full turn.
  this->blit(bmp, src, dst, t, opacity, orientation * 360.0f,
       { float(scale.width) * 0.5f, float(scale.height) * 0.5f });
}

void RaylibGraphics::drawText(const agui::Point& position, const std::string& text,
                              const agui::Color& color, const agui::Font* font,
                              agui::RichTextSetting setting, agui::HorizontalAlign align,
                              const agui::TextHighlightErrorColors&, agui::HighlightedText,
                              float scale, agui::VerticalAlign verticalAlign, float orientation)
{
  const auto* f = static_cast<const RaylibFont*>(font);
  if (!f || text.empty()) return;
  const bool rich = setting != agui::RichTextSetting::Disabled && HasTags(text);

  const ::Vector2 size = rich ? ::Vector2{ float(f->getTextWidth(text, setting, scale)), f->size() * scale }
                              : f->measure(text, scale);
  float x = float(position.x + this->getOffset().x);
  float y = float(position.y + this->getOffset().y);
  if (align == agui::HorizontalAlign::Center)     x -= size.x * 0.5f;
  else if (align == agui::HorizontalAlign::Right) x -= size.x;
  if (verticalAlign == agui::VerticalAlign::Center)      y -= size.y * 0.5f;
  else if (verticalAlign == agui::VerticalAlign::Bottom) y -= size.y;

  // Onto whole screen pixels, not whole GUI units: at 125% the one isn't the
  // other, and glyphs that start between pixels come out soft.
  const auto snap = [this](float v) { return std::round(v * this->viewScale) / this->viewScale; };
  if (rich) {
    // One stretch after another, each in its own font and colour.
    ForEachRun(text, f, [&](std::string_view piece, const RaylibFont* runFont, const std::optional<agui::Color>& runColor) {
      // A tag's colour, but the widget's transparency (a disabled one's, say).
      const agui::Color c = runColor ? agui::Color(runColor->getR(), runColor->getG(), runColor->getB(), color.getA()) : color;
      const std::string s(piece);
      DrawTextPro(runFont->raylibFont(), s.c_str(), { snap(x), snap(y) }, { 0, 0 }, 0.0f, runFont->size() * scale,
                  runFont->spacing() * scale, ToRaylib(Multiply(c, this->tint)));
      x += runFont->measure(piece, scale).x;
    });
    return;
  }
  DrawTextPro(f->raylibFont(), text.c_str(), { snap(x), snap(y) }, { 0, 0 },
              orientation * 360.0f, f->size() * scale, f->spacing() * scale,
              ToRaylib(Multiply(color, this->tint)));
}

void RaylibGraphics::drawTextLines(const agui::Point& position, std::string_view text,
                                   const agui::Color& color, const agui::Font* font,
                                   agui::RichTextSetting setting, agui::HorizontalAlign align)
{
  if (!font) return;
  agui::Point p = position;
  size_t start = 0;
  while (start <= text.size()) {
    const size_t end = std::min(text.find('\n', start), text.size());
    this->drawText(p, std::string(text.substr(start, end - start)), color, font, setting, align);
    p.y += font->getLineHeight();
    start = end + 1;
  }
}

void RaylibGraphics::drawTextLines(const agui::ResizableText& text, std::vector<std::pair<size_t, agui::Point>>& linePositions,
                                   const agui::Font* font, const agui::Color& color,
                                   const agui::TextHighlightErrorColors& highlightColors)
{
  const std::vector<std::string_view>& lines = text.lines();
  std::vector<std::string> open;
  size_t tracked = 0;  // the lines whose tags are in `open`
  for (const auto& [index, position] : linePositions) {
    for (; tracked < index; ++tracked) TrackOpenTags(lines[tracked], open);
    std::string line;
    for (const std::string& tag : open) line += tag;
    line += lines[index];
    this->drawText(position, line, color, font, text.getRichTextSetting(), agui::HorizontalAlign::Left, highlightColors);
  }
}

void RaylibGraphics::drawRectangle(const agui::Rectangle& rect, const agui::Color& color, int width)
{
  const agui::Point o = this->getOffset();
  DrawRectangleLinesEx({ float(rect.x + o.x), float(rect.y + o.y), float(rect.width), float(rect.height) },
                       float(width), ToRaylib(Multiply(color, this->tint)));
}

void RaylibGraphics::drawFilledRectangle(const agui::Rectangle& rect, const agui::Color& color)
{
  const agui::Point o = this->getOffset();
  DrawRectangle(rect.x + o.x, rect.y + o.y, rect.width, rect.height, ToRaylib(Multiply(color, this->tint)));
}

void RaylibGraphics::drawFilledTriangle(const agui::Point& a, const agui::Point& b,
                                        const agui::Point& c, const agui::Color& color)
{
  const agui::Point o = this->getOffset();
  const ::Vector2 va{ float(a.x + o.x), float(a.y + o.y) };
  const ::Vector2 vb{ float(b.x + o.x), float(b.y + o.y) };
  const ::Vector2 vc{ float(c.x + o.x), float(c.y + o.y) };
  // raylib only fills counter-clockwise triangles; the cross product tells us which way this one winds.
  const float cross = (vb.x - va.x) * (vc.y - va.y) - (vb.y - va.y) * (vc.x - va.x);
  const ::Color col = ToRaylib(Multiply(color, this->tint));
  if (cross < 0) DrawTriangle(va, vb, vc, col);
  else           DrawTriangle(va, vc, vb, col);
}

void RaylibGraphics::drawCircle(const agui::Point& center, float radius, const agui::Color& color,
                                double width)
{
  const agui::Point o = this->getOffset();
  DrawRing({ float(center.x + o.x), float(center.y + o.y) }, radius - float(width), radius, 0, 360,
           36, ToRaylib(Multiply(color, this->tint)));
}

void RaylibGraphics::drawFilledCircle(const agui::Point& center, float radius, const agui::Color& color)
{
  const agui::Point o = this->getOffset();
  DrawCircleV({ float(center.x + o.x), float(center.y + o.y) }, radius, ToRaylib(Multiply(color, this->tint)));
}

void RaylibGraphics::drawFilledPieSlice(const agui::Point& center, float radius, float startTheta,
                                        float deltaTheta, const agui::Color& color)
{
  const agui::Point o = this->getOffset();
  const float start = startTheta * RAD2DEG;
  DrawCircleSector({ float(center.x + o.x), float(center.y + o.y) }, radius, start,
                   start + deltaTheta * RAD2DEG, 36, ToRaylib(Multiply(color, this->tint)));
}

void RaylibGraphics::drawPixel(const agui::Point& point, const agui::Color& color)
{
  const agui::Point o = this->getOffset();
  DrawPixel(point.x + o.x, point.y + o.y, ToRaylib(Multiply(color, this->tint)));
}

void RaylibGraphics::drawLine(const agui::Point& start, const agui::Point& end,
                              const agui::Color& color, float thickness)
{
  const agui::Point o = this->getOffset();
  DrawLineEx({ float(start.x + o.x), float(start.y + o.y) }, { float(end.x + o.x), float(end.y + o.y) },
             std::max(thickness, 1.0f), ToRaylib(Multiply(color, this->tint)));
}

void RaylibGraphics::drawPolyline(const std::vector<agui::Point>& points, const agui::Color& color,
                                  float width)
{
  for (size_t i = 1; i < points.size(); ++i) this->drawLine(points[i - 1], points[i], color, width);
}

// ----------------------------------------------------------------- input ----

namespace {

struct KeyMap {
  KeyboardKey          raylib;
  agui::KeyEnum        key;
  agui::ExtendedKeyEnum ext;
};

// Keys Agui cares about beyond printable characters (those come in through
// GetCharPressed so they carry the right unichar for the keyboard layout).
constexpr KeyMap kKeys[] = {
  { ::KEY_ENTER,         agui::KEY_ENTER,     agui::EXT_KEY_NONE },
  { ::KEY_KP_ENTER,      agui::KEY_ENTER,     agui::EXT_KEY_NONE },
  { ::KEY_ESCAPE,        agui::KEY_ESCAPE,    agui::EXT_KEY_NONE },
  { ::KEY_TAB,           agui::KEY_TAB,       agui::EXT_KEY_NONE },
  { ::KEY_BACKSPACE,     agui::KEY_BACKSPACE, agui::EXT_KEY_NONE },
  { ::KEY_DELETE,        agui::KEY_DELETE,    agui::EXT_KEY_NONE },
  { ::KEY_UP,            agui::KEY_NONE,      agui::EXT_KEY_UP },
  { ::KEY_DOWN,          agui::KEY_NONE,      agui::EXT_KEY_DOWN },
  { ::KEY_LEFT,          agui::KEY_NONE,      agui::EXT_KEY_LEFT },
  { ::KEY_RIGHT,         agui::KEY_NONE,      agui::EXT_KEY_RIGHT },
  { ::KEY_HOME,          agui::KEY_NONE,      agui::EXT_KEY_HOME },
  { ::KEY_END,           agui::KEY_NONE,      agui::EXT_KEY_END },
  { ::KEY_PAGE_UP,       agui::KEY_NONE,      agui::EXT_KEY_PAGE_UP },
  { ::KEY_PAGE_DOWN,     agui::KEY_NONE,      agui::EXT_KEY_PAGE_DOWN },
  { ::KEY_INSERT,        agui::KEY_NONE,      agui::EXT_KEY_INSERT },
  { ::KEY_LEFT_SHIFT,    agui::KEY_NONE,      agui::EXT_KEY_LEFT_SHIFT },
  { ::KEY_RIGHT_SHIFT,   agui::KEY_NONE,      agui::EXT_KEY_RIGHT_SHIFT },
  { ::KEY_LEFT_CONTROL,  agui::KEY_NONE,      agui::EXT_KEY_LEFT_CONTROL },
  { ::KEY_RIGHT_CONTROL, agui::KEY_NONE,      agui::EXT_KEY_RIGHT_CONTROL },
  { ::KEY_LEFT_ALT,      agui::KEY_NONE,      agui::EXT_KEY_ALT },
  { ::KEY_RIGHT_ALT,     agui::KEY_NONE,      agui::EXT_KEY_ALTGR },
};

struct Modifiers {
  bool alt, shift, ctrl, meta;
};

Modifiers CurrentModifiers()
{
  return { input::IsKeyDown(::KEY_LEFT_ALT) || input::IsKeyDown(::KEY_RIGHT_ALT),
           input::IsKeyDown(::KEY_LEFT_SHIFT) || input::IsKeyDown(::KEY_RIGHT_SHIFT),
           input::IsKeyDown(::KEY_LEFT_CONTROL) || input::IsKeyDown(::KEY_RIGHT_CONTROL),
           input::IsKeyDown(::KEY_LEFT_SUPER) || input::IsKeyDown(::KEY_RIGHT_SUPER) };
}

}  // namespace

void RaylibInput::pollInput()
{
  const double    now = GetTime();
  const Modifiers mod = CurrentModifiers();
  const ::Vector2 m   = input::GetMousePosition();
  const int       mx  = int(std::floor(m.x / this->viewScale));
  const int       my  = int(std::floor(m.y / this->viewScale));

  if (this->isMouseEnabled()) {
    const auto push = [&](agui::MouseEvent::Type type, agui::MouseButton button, int wheel) {
      this->pushMouseEvent(agui::MouseInput(type, button, mx, my, wheel, now, mod.alt, mod.shift, mod.ctrl));
    };

    if (m.x != this->lastMouse.x || m.y != this->lastMouse.y) {
      push(agui::MouseEvent::Type::MOUSE_MOVE, agui::MouseButton::NONE, 0);
      this->lastMouse = m;
    }

    constexpr std::pair<MouseButton, agui::MouseButton> kButtons[] = {
      { MOUSE_BUTTON_LEFT, agui::MouseButton::LEFT },
      { MOUSE_BUTTON_RIGHT, agui::MouseButton::RIGHT },
      { MOUSE_BUTTON_MIDDLE, agui::MouseButton::MIDDLE },
    };
    for (const auto& [rl, ag] : kButtons) {
      if (input::IsMouseButtonPressed(rl))  push(agui::MouseEvent::Type::MOUSE_DOWN, ag, 0);
      if (input::IsMouseButtonReleased(rl)) push(agui::MouseEvent::Type::MOUSE_UP, ag, 0);
    }

    const float wheel = input::GetMouseWheelMove();
    if (wheel > 0) push(agui::MouseEvent::Type::MOUSE_WHEEL_UP, agui::MouseButton::NONE, 1);
    if (wheel < 0) push(agui::MouseEvent::Type::MOUSE_WHEEL_DOWN, agui::MouseButton::NONE, -1);
  }

  if (this->isKeyboardEnabled()) {
    const auto push = [&](agui::KeyEvent::KeyboardEventEnum type, agui::KeyEnum key,
                          agui::ExtendedKeyEnum ext, uint32_t unichar, int rlKey) {
      this->pushKeyboardEvent(agui::KeyboardInput(type, key, ext, unichar, now, mod.alt, mod.shift,
                                            mod.ctrl, mod.meta, agui::BackendKeycode(rlKey), 0));
    };

    for (const KeyMap& k : kKeys) {
      if (this->keyFilter && this->keyFilter(k.raylib)) continue;
      const uint32_t uni = uint32_t(k.key);
      if (input::IsKeyPressed(k.raylib))       push(agui::KeyEvent::KEY_DOWN, k.key, k.ext, uni, k.raylib);
      if (input::IsKeyPressedRepeat(k.raylib)) push(agui::KeyEvent::KEY_REPEAT, k.key, k.ext, uni, k.raylib);
      if (input::IsKeyReleased(k.raylib))      push(agui::KeyEvent::KEY_UP, k.key, k.ext, uni, k.raylib);
    }

    // Printable text. Agui's KeyEnum values are ASCII, so an ASCII codepoint
    // doubles as the key (uppercased, which is how the enum spells letters).
    for (int ch = input::GetCharPressed(); ch > 0; ch = input::GetCharPressed()) {
      const int upper = (ch >= 'a' && ch <= 'z') ? ch - 'a' + 'A' : ch;
      const auto key  = upper < 128 ? agui::KeyEnum(upper) : agui::KEY_NONE;
      push(agui::KeyEvent::KEY_DOWN, key, agui::EXT_KEY_NONE, uint32_t(ch), 0);
      push(agui::KeyEvent::KEY_UP, key, agui::EXT_KEY_NONE, uint32_t(ch), 0);
    }

    // Ctrl and a letter is a shortcut -- select all, copy, cut, paste -- and
    // no character comes for it, so the letter keys go as keys of their own,
    // with no text. Not with Alt as well: that is AltGr, which types text.
    if ((mod.ctrl && !mod.alt) || mod.meta) {
      for (int k = ::KEY_A; k <= ::KEY_Z; ++k) {
        if (this->keyFilter && this->keyFilter(k)) continue;
        const auto key = agui::KeyEnum(k);  // both spell letters in uppercase ASCII
        if (input::IsKeyPressed(k))       push(agui::KeyEvent::KEY_DOWN, key, agui::EXT_KEY_NONE, 0, k);
        if (input::IsKeyPressedRepeat(k)) push(agui::KeyEvent::KEY_REPEAT, key, agui::EXT_KEY_NONE, 0, k);
        if (input::IsKeyReleased(k))      push(agui::KeyEvent::KEY_UP, key, agui::EXT_KEY_NONE, 0, k);
      }
    }
  }
}

namespace {

// The text cursor. Windows' own inverts what is under it, and over the text
// boxes' middle grey that is the same grey: it vanishes. This one is white
// with a black edge, seen on anything; made once a window, at the monitor's
// scale. GLFW takes it away with the window: see ReleaseCursors().
GLFWcursor* ibeamMade  = nullptr;
bool        ibeamTried = false;

GLFWcursor* IBeam()
{
  GLFWcursor*& made = ibeamMade;
  if (ibeamTried) return made;
  ibeamTried = true;

  // An I, in pixels at 100%: the stem, and a short bar across each end.
  constexpr int W = 9, H = 21, MIDDLE = 4;
  const auto white = [](int x, int y) {
    if (x < 0 || y < 1 || x >= W || y > H - 2) return false;
    return x == MIDDLE || ((y == 1 || y == H - 2) && x >= MIDDLE - 2 && x <= MIDDLE + 2);
  };
  const int scale = std::max(1, int(std::lround(GetWindowScaleDPI().y)));
  std::vector<unsigned char> pixels(size_t(W * scale) * size_t(H * scale) * 4, 0);
  for (int y = 0; y < H; ++y) {
    for (int x = 0; x < W; ++x) {
      bool edge = false;
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) edge = edge || white(x + dx, y + dy);
      }
      if (!edge) continue;
      const unsigned char shade = white(x, y) ? 255 : 0;
      for (int sy = 0; sy < scale; ++sy) {
        for (int sx = 0; sx < scale; ++sx) {
          unsigned char* p = &pixels[(size_t(y * scale + sy) * size_t(W * scale) + size_t(x * scale + sx)) * 4];
          p[0] = p[1] = p[2] = shade;
          p[3] = 255;
        }
      }
    }
  }
  const GLFWimage image{ W * scale, H * scale, pixels.data() };
  made = glfwCreateCursor(&image, W * scale / 2, H * scale / 2);
  return made;
}

}  // namespace

void ReleaseCursors()
{
  if (ibeamMade) glfwDestroyCursor(ibeamMade);
  ibeamMade  = nullptr;
  ibeamTried = false;
}

bool RaylibCursorProvider::setCursor(CursorEnum cursor)
{
  switch (cursor) {
  case EDIT_CURSOR:
    if (GLFWcursor* ibeam = IBeam()) glfwSetCursor(glfwGetCurrentContext(), ibeam);
    else                             SetMouseCursor(MOUSE_CURSOR_IBEAM);
    break;
  case LINK_CURSOR:      SetMouseCursor(MOUSE_CURSOR_POINTING_HAND); break;
  case MOVE_CURSOR:      SetMouseCursor(MOUSE_CURSOR_RESIZE_ALL); break;
  case RESIZE_N_CURSOR:
  case RESIZE_S_CURSOR:  SetMouseCursor(MOUSE_CURSOR_RESIZE_NS); break;
  case RESIZE_E_CURSOR:
  case RESIZE_W_CURSOR:  SetMouseCursor(MOUSE_CURSOR_RESIZE_EW); break;
  case RESIZE_NW_CURSOR:
  case RESIZE_SE_CURSOR: SetMouseCursor(MOUSE_CURSOR_RESIZE_NWSE); break;
  case RESIZE_NE_CURSOR:
  case RESIZE_SW_CURSOR: SetMouseCursor(MOUSE_CURSOR_RESIZE_NESW); break;
  case BUSY_CURSOR:
  case QUESTION_CURSOR:
  case ARROW_CURSOR:
  case DEFAULT_CURSOR:   SetMouseCursor(MOUSE_CURSOR_DEFAULT); break;
  }
  return true;
}

}  // namespace agui_raylib

// Agui's TextBox copies and pastes through this; raylib has the platform code.
namespace agui::SystemClipboard {

void copy(const std::string& text) { SetClipboardText(text.c_str()); }

std::string paste(bool filterNewlines)
{
  const char* raw = GetClipboardText();
  std::string text = raw ? raw : "";
  if (filterNewlines) {
    std::replace(text.begin(), text.end(), '\n', ' ');
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
  }
  return text;
}

}  // namespace agui::SystemClipboard
