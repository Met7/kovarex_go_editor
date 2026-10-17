// raylib backend for Agui: the Graphics / Font / Image / Input implementations
// the library needs to draw and receive events.
//
// raylib and Agui both define Color, Rectangle, Font and Image (raylib in the
// global namespace, Agui in agui::), so nothing here says `using namespace`.

#pragma once

#include <raylib.h>

#include <Agui/CursorProvider.hpp>
#include <Agui/Font.hpp>
#include <Agui/FontLoader.hpp>
#include <Agui/Graphics.hpp>
#include <Agui/Image.hpp>
#include <Agui/Input.hpp>

#include <functional>
#include <memory>
#include <string>

namespace agui_raylib {

// A region of a raylib texture. Several images can share one texture (a 9-slice
// is nine of these over a single sheet); only the owner unloads it.
//
// `scale` is how big one texel is on screen: a sheet drawn at double resolution
// uses 0.5, and Agui then sees the image at half its pixel size. `tint` is
// multiplied into every draw -- it is how a white glow sprite becomes an
// orange glow or a faint shadow.
class RaylibImage : public agui::Image {
public:
  RaylibImage(std::shared_ptr<Texture2D> texture, ::Rectangle region, float scale = 1.0f,
              agui::Color tint = agui::Color(1, 1, 1, 1));
  explicit RaylibImage(std::shared_ptr<Texture2D> texture);

  const Texture2D& texture() const { return *this->tex; }
  const ::Rectangle& region() const { return this->src; }
  const agui::Color& tint() const { return this->tintColor; }
  // The texture region under a sub-rectangle given in screen pixels, which
  // is how Agui addresses parts of an image.
  ::Rectangle texels(float x, float y, float w, float h) const
  {
    return { this->src.x + x / this->scale, this->src.y + y / this->scale, w / this->scale, h / this->scale };
  }

  void logic(double) override {}
  int  getWidth() const override;
  int  getHeight() const override;
  bool isAutoFreeing() const override { return false; }
  void free() override { this->tex.reset(); }
  bool operator==(const agui::Image& other) const override;

private:
  std::shared_ptr<Texture2D> tex;
  ::Rectangle src;
  float       scale;
  agui::Color tintColor;
};

// Uploads a raylib Image as a texture that is unloaded once the last
// RaylibImage using it goes away.
std::shared_ptr<Texture2D> MakeSharedTexture(const ::Image& image);

// Either loads its own font (an empty file name means raylib's built-in one,
// otherwise a TTF/OTF rasterised at `height` px), or borrows one that someone
// else loaded and will unload.
class RaylibFont : public agui::Font {
public:
  RaylibFont() = default;
  RaylibFont(const std::string& fileName, int height);
  RaylibFont(const ::Font& borrowed, int height, float spacing);
  ~RaylibFont() override;

  const ::Font& raylibFont() const { return this->font; }
  float size() const { return float(this->height); }
  float spacing() const { return this->spacingPx; }
  ::Vector2 measure(std::string_view text, double scale = 1.0) const;

  // Swaps a borrowed font's glyphs for the same face rasterised at another
  // size. Under a scaled view, text `height` tall covers height * scale screen
  // pixels, and glyphs rasterised for that many are drawn sharp rather than
  // stretched. Measurements are unchanged: raylib measures at `height`
  // whatever size the glyphs were made at.
  void setRaster(const ::Font& raster)
  {
    if (!this->ownsFont) this->font = raster;
  }

  void free() override;
  int   getLineHeight() const override { return this->height; }
  float getAscent() const override { return float(this->height) * 0.8f; }
  int   getTextWidth(std::string_view text, agui::RichTextSetting, double scale = 1) const override;
  size_t getWrapIndex(std::string_view text, int width, double scale = 1) const override;
  // Agui's takes every line to end in a line break, and a click past the end
  // of a text field's one line to be before its last character.
  int getStringIndexFromPosition(std::string_view text, int x, agui::RichTextSetting) const override;
  int   getSubstringWidth(std::string_view text, const agui::RichTextData&,
                          agui::RichTextSetting = agui::RichTextSetting::Enabled,
                          double scale = 1) const override;
  void reload(const std::string& fileName, int height, int fontFlags, float borderWidth,
              const agui::Color& borderColor) override;
  const std::string& getPath() const override { return this->path; }
  agui::RichTextHandler* getRichTextHandler() const override;

private:
  ::Font      font{};
  bool        ownsFont  = false;
  int         height    = 10;
  float       spacingPx = 1.0f;
  std::string path;
};

// Rich text, the part of Factorio's that the editor uses: where a widget's
// RichTextSetting isn't Disabled, text in [font=name]...[/font] is drawn in
// the font registered under that name, and text in [color=r,g,b]...[/color]
// (0-255, or #rrggbb) in that colour. Tags nest; anything else in brackets
// is just text. The names are style.lua's: "default", "default-semibold"...
void RegisterFont(const std::string& name, const RaylibFont* font);

class RaylibFontLoader : public agui::FontLoader {
public:
  agui::Font* loadFont(const std::string& fileName, int height, int fontFlags,
                       float borderWidth, const agui::Color& borderColor) override;
  agui::Font* loadEmptyFont() override { return new RaylibFont(); }
};

// The GUI can be drawn scaled: Agui lays out on a display `viewScale` times
// smaller than the window, and whoever calls render() scales the drawing back
// up (GuiLayer does, with rlgl's matrix). Everything handed to Agui here --
// the display size, and the mouse in RaylibInput -- is in those GUI units;
// only the scissor, which the matrix doesn't reach, is converted back.
class RaylibGraphics : public agui::Graphics {
protected:
  void setClippingRectangle(const agui::Rectangle& rect, bool force) override;

public:
  void  setViewScale(float value) { this->viewScale = value; }
  float getViewScale() const { return this->viewScale; }

  void _beginPaint() override;
  void _endPaint() override;
  agui::Dimension getDisplaySize() const override;
  agui::Rectangle getClippingRectangle() override;

  void drawImage(const agui::Image* bmp, const agui::Point& position,
                 const agui::Point& regionStart, const agui::Dimension& regionSize,
                 const float& opacity = 1.0f) override;
  void drawTintedImage(const agui::Image* bmp, const agui::Point& position,
                       const agui::Point& regionStart, const agui::Dimension& regionSize,
                       const agui::Color& tint, const float& opacity = 1.0f) override;
  void drawImage(const agui::Image* bmp, const agui::Point& position,
                 const float& opacity = 1.0f,
                 agui::InvertColors invertColors = agui::InvertColors::False) override;
  void drawScaledImage(const agui::Image* bmp, const agui::Point& position,
                       const agui::Dimension& scale, const float& opacity = 1.0f,
                       agui::InvertColors invertColors = agui::InvertColors::False) override;
  void drawScaledImageRegion(const agui::Image* bmp, const agui::Point& position,
                             const agui::Dimension& scale, const agui::Rectangle& imageRegion,
                             const float& opacity = 1.0f,
                             agui::InvertColors invertColors = agui::InvertColors::False) override;
  void drawScaledTintedImage(const agui::Image* bmp, const agui::Point& position,
                             const agui::Dimension& scale, const agui::Color& tint,
                             const float& opacity = 1.0f,
                             agui::InvertColors invertColors = agui::InvertColors::False) override;
  void drawScaledRotatedTintedImage(const agui::Image* bmp, double centerX, double centerY,
                                    const float orientation, const agui::Point& regionStart,
                                    const agui::Dimension& regionScale,
                                    const agui::Dimension& scale, const agui::Color& tint,
                                    const float& opacity = 1.0f) override;

  void drawText(const agui::Point& position, const std::string& text, const agui::Color& color,
                const agui::Font* font, agui::RichTextSetting richTextSetting,
                agui::HorizontalAlign align = agui::HorizontalAlign::Left,
                const agui::TextHighlightErrorColors& highlightColors =
                    agui::TextHighlightErrorColors::defaults(),
                agui::HighlightedText highlighted = agui::HighlightedText::False, float scale = 1.0f,
                agui::VerticalAlign verticalAlign = agui::VerticalAlign::Top,
                float orientation = 0.0f) override;
  void drawTextLines(const agui::Point& position, std::string_view text,
                     const agui::Color& color, const agui::Font* font,
                     agui::RichTextSetting richTextSetting = agui::RichTextSetting::Enabled,
                     agui::HorizontalAlign align = agui::HorizontalAlign::Left) override;
  // Rich text's wrapped lines, each started with the tags still open from
  // the one before.
  void drawTextLines(const agui::ResizableText& text, std::vector<std::pair<size_t, agui::Point>>& linePositions,
                     const agui::Font* font, const agui::Color& color,
                     const agui::TextHighlightErrorColors& highlightColors) override;
  using agui::Graphics::drawTextLines;

  void drawRectangle(const agui::Rectangle& rect, const agui::Color& color, int width = 1) override;
  void drawFilledRectangle(const agui::Rectangle& rect, const agui::Color& color) override;
  void drawFilledTriangle(const agui::Point& a, const agui::Point& b, const agui::Point& c,
                          const agui::Color& color) override;
  void drawCircle(const agui::Point& center, float radius, const agui::Color& color,
                  double width = 1) override;
  void drawFilledCircle(const agui::Point& center, float radius, const agui::Color& color) override;
  void drawFilledPieSlice(const agui::Point& center, float radius, float startTheta,
                          float deltaTheta, const agui::Color& color) override;
  void drawPixel(const agui::Point& point, const agui::Color& color) override;
  void drawLine(const agui::Point& start, const agui::Point& end, const agui::Color& color,
                float thickness = 1.0) override;
  void drawPolyline(const std::vector<agui::Point>& points, const agui::Color& color,
                    float width = 0) override;
  void blurRectangle(const agui::Rectangle&, float) override {}
  void drawGradientBorderOverlay(const agui::Rectangle&) override {}

  void setTint(const agui::Color& color) override { this->tint = color; }
  agui::Color getTint() override { return this->tint; }

private:
  void blit(const agui::Image* bmp, ::Rectangle source, ::Rectangle dest,
            const agui::Color& tint, float opacity, float rotationDeg = 0.0f,
            ::Vector2 origin = { 0, 0 }, agui::InvertColors invert = agui::InvertColors::False);

  void applyScissor();

  // Agui keeps two clip levels: "forced" ones (scroll panes, inset frames,
  // drop-down lists) that bound everything drawn inside them, and ordinary
  // per-widget ones. As in Factorio's GuiGraphics, only the forced one cuts
  // what reaches the screen -- a widget's glow and shadow reach past it, and
  // would be cut off by its own clip -- and the intersection is only what
  // getClippingRectangle() says. FULL_SCREEN_RECTANGLE means "no clip".
  agui::Rectangle forcedClip  = agui::Graphics::FULL_SCREEN_RECTANGLE;
  agui::Rectangle appliedClip = agui::Graphics::FULL_SCREEN_RECTANGLE;
  agui::Rectangle clip        = agui::Graphics::FULL_SCREEN_RECTANGLE;
  bool            scissoring  = false;
  agui::Color     tint        = agui::Color(1.0f, 1.0f, 1.0f, 1.0f);
  float           viewScale   = 1.0f;  // screen pixels per GUI unit
};

// Turns raylib's polled input state into Agui's queued events. Gui::logic()
// calls pollInput() once per frame.
class RaylibInput : public agui::Input {
public:
  // The mouse is reported in GUI units: see RaylibGraphics.
  void setViewScale(float value) { this->viewScale = value; }

  // Keys the filter says yes to are never passed on -- they were a shortcut,
  // and belong to no widget (see ui/Shortcuts.hpp). Takes raylib KEY_*s.
  void setKeyFilter(std::function<bool(int key)> filter) { this->keyFilter = std::move(filter); }

  void   pollInput() override;
  double getTime() const override { return GetTime(); }
  PlayerInputMethod getInputMethod() override { return PlayerInputMethod::KeyboardAndMouse; }

private:
  ::Vector2 lastMouse{ -1.0f, -1.0f };
  float     viewScale = 1.0f;

  std::function<bool(int key)> keyFilter;
};

class RaylibCursorProvider : public agui::CursorProvider {
public:
  bool setCursor(CursorEnum cursor) override;
};

// The cursors made for the window, before it closes. Another window -- the
// tests open one for each scenario -- makes its own.
void ReleaseCursors();

}  // namespace agui_raylib
