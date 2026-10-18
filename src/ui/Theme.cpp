#include <ui/Theme.hpp>

#include <ui/Fonts.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <utility>

#include <Agui/BorderImageSet.hpp>
#include <Agui/Widget/ActivityBar.hpp>
#include <Agui/Widget/BezierPlot.hpp>
#include <Agui/Widget/Button.hpp>
#include <Agui/Widget/CheckBox.hpp>
#include <Agui/Widget/DoubleSlider.hpp>
#include <Agui/Widget/DropDown.hpp>
#include <Agui/Widget/EmptyWidget.hpp>
#include <Agui/Widget/Flow.hpp>
#include <Agui/Widget/Frame.hpp>
#include <Agui/Widget/Graph.hpp>
#include <Agui/Widget/HorizontalFlow.hpp>
#include <Agui/Widget/ImageWidget.hpp>
#include <Agui/Widget/Label.hpp>
#include <Agui/Widget/Line.hpp>
#include <Agui/Widget/ListBox.hpp>
#include <Agui/Widget/ProgressBar.hpp>
#include <Agui/Widget/RadioButton.hpp>
#include <Agui/Widget/ScrollBar.hpp>
#include <Agui/Widget/ScrollPane.hpp>
#include <Agui/Widget/Slider.hpp>
#include <Agui/Widget/Switch.hpp>
#include <Agui/Widget/Tab.hpp>
#include <Agui/Widget/TabbedPane.hpp>
#include <Agui/Widget/Table.hpp>
#include <Agui/Widget/TextBox.hpp>
#include <Agui/Widget/TextField.hpp>
#include <Agui/Widget/ToolTip.hpp>
#include <Agui/Widget/VerticalFlow.hpp>

namespace ui {

// gui.png, embedded by tools/Embed.cpp at build time.
extern const unsigned char GUI_ATLAS_PNG[];
extern const std::size_t   GUI_ATLAS_PNG_SIZE;
// Factorio's utility sprites, embedded the same way. Each file is a mipmap
// strip: the full-size picture at its left, smaller copies beside it.
extern const unsigned char ICON_INFO_PNG[];
extern const std::size_t   ICON_INFO_PNG_SIZE;
extern const unsigned char ICON_RESET_PNG[];
extern const std::size_t   ICON_RESET_PNG_SIZE;
extern const unsigned char ICON_RESET_WHITE_PNG[];
extern const std::size_t   ICON_RESET_WHITE_PNG_SIZE;
extern const unsigned char ICON_SEARCH_PNG[];
extern const std::size_t   ICON_SEARCH_PNG_SIZE;
extern const unsigned char APP_ICON_PNG[];
extern const std::size_t   APP_ICON_PNG_SIZE;

struct Theme::Parts {
  std::optional<Piece> leftTop, top, rightTop;
  std::optional<Piece> left, center, right;
  std::optional<Piece> leftBottom, bottom, rightBottom;
};

struct Theme::Look {
  bool        outer = false;  // draw_type = "outer": around the widget, not inside it
  agui::Color tint  = agui::Color(1, 1, 1, 1);
  int         topShift = 0, bottomShift = 0, leftShift = 0, rightShift = 0;
  uint8_t     tiling  = 0;     // Layer::*_TILING
  bool        stretch = true;  // stretch_monolith_image_to_size
};

namespace {

using Parts = Theme::Parts;
using Piece = Theme::Piece;
using Look  = Theme::Look;

// style.lua: default_sprite_scale. The atlas is drawn at half its pixel size.
constexpr float SPRITE_SCALE = 0.5f;

// The pixels text `px` tall covers when the whole GUI is drawn at `scale`: what
// its font is rasterised for, so it is sharp rather than stretched.
int RasterPx(int px, float scale)
{
  return std::max(1, int(std::lround(float(px) * scale)));
}

// Colours in style.lua are 0-255 when any component is above 1, else 0-1.
agui::Color Rgb(int r, int g, int b, int a = 255)
{
  return agui::Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

const agui::Color PURE_WHITE      = Rgb(255, 255, 255);
const agui::Color WHITE_HALF      = Rgb(255, 255, 255, 128);   // gui_color.white_with_alpha
const agui::Color PURE_BLACK      = Rgb(0, 0, 0);              // button_default_font_color
const agui::Color BOLD_BLACK      = Rgb(28, 28, 28);           // button_default_bold_font_color
const agui::Color GREY            = Rgb(128, 128, 128);        // gui_color.grey
const agui::Color DISABLED_BUTTON = Rgb(179, 179, 179);
const agui::Color CAPTION         = Rgb(255, 230, 192);        // gui_color.caption
const agui::Color ORANGE_TEXT     = Rgb(250, 168, 56);         // gui_color.orange
const agui::Color GLOW            = Rgb(225, 177, 106);        // default_glow_color
const agui::Color GREEN_GLOW      = Rgb(135, 216, 139, 128);   // green_button_glow_color
const agui::Color RED_GLOW        = Rgb(254, 90, 90, 128);     // red_button_glow_color
const agui::Color SHADOW          = Rgb(0, 0, 0, 89);          // default_shadow_color
const agui::Color HARD_SHADOW     = Rgb(0, 0, 0);              // hard_shadow_color
const agui::Color DIRT            = Rgb(15, 7, 3, 100);        // default_dirt_color
const agui::Color DIRT_FILLER     = Rgb(15, 7, 3, 56);         // default_dirt_color_filler
const agui::Color TRANSPARENT     = Rgb(0, 0, 0, 0);

// The sheet drawn over the game while the main menu is up.
const agui::Color DIM = Rgb(6, 6, 8, 170);

// style.lua: expand_graphical_set -- a (2c+1)-square at (x, y): c-sized
// corners, 1px edges and a 1px centre that stretch.
Parts Expand(int x, int y, int c)
{
  Parts p;
  p.leftTop     = Piece{ x, y, c, c };
  p.top         = Piece{ x + c, y, 1, c };
  p.rightTop    = Piece{ x + c + 1, y, c, c };
  p.left        = Piece{ x, y + c, c, 1 };
  p.center      = Piece{ x + c, y + c, 1, 1 };
  p.right       = Piece{ x + c + 1, y + c, c, 1 };
  p.leftBottom  = Piece{ x, y + c + 1, c, c };
  p.bottom      = Piece{ x + c, y + c + 1, 1, c };
  p.rightBottom = Piece{ x + c + 1, y + c + 1, c, c };
  return p;
}

int Scaled(int atlasPx)
{
  return int(std::lround(float(atlasPx) * SPRITE_SCALE));
}

// Style lookups walk up the parent chain until some style defines the
// property, and nothing checks for running off the top. So every root style
// has to define all of the base properties, even as zeros.
void DefineBaseProperties(agui::Style& style, agui::StretchRule stretch)
{
  style.setHorizontalAlign(agui::HorizontalAlign::Left);
  style.setVerticalAlign(agui::VerticalAlign::Top);
  style.setMinimalWidth(0);
  style.setMinimalHeight(0);
  style.setMaximalWidth(0);  // 0 = unbounded
  style.setMaximalHeight(0);
  style.setNaturalWidth(0);
  style.setNaturalHeight(0);
  style.setPaddings(0, 0, 0, 0);
  style.setMargin(0);
  style.setHorizontallyStretchable(stretch);
  style.setVerticallyStretchable(stretch);
  style.setHorizontallySquashable(stretch);
  style.setVerticallySquashable(stretch);
  style.setEffect(nullptr);
  style.setEffectOpacity(1.0f);
}

void DefineAllRootStyles()
{
  // Containers size themselves from their children...
  for (agui::Style* s : std::initializer_list<agui::Style*>{
           &agui::VerticalFlow::defaultStyle, &agui::HorizontalFlow::defaultStyle,
           &agui::Flow::defaultStyle, &agui::ScrollPane::defaultStyle,
           &agui::Frame::defaultStyle, &agui::TabbedPane::defaultStyle,
           &agui::Table::defaultStyle })
      DefineBaseProperties(*s, agui::StretchRule::Auto);

  // ...leaf widgets keep the size they ask for.
  for (agui::Style* s : std::initializer_list<agui::Style*>{
           &agui::ActivityBar::defaultStyle, &agui::BezierPlot::defaultStyle,
           &agui::Button::defaultStyle, &agui::CheckBox::defaultStyle,
           &agui::DoubleSlider::defaultStyle, &agui::DropDown::defaultStyle,
           &agui::EmptyWidget::defaultStyle, &agui::Graph::defaultStyle,
           &agui::ImageWidget::defaultStyle, &agui::Label::defaultStyle,
           &agui::Line::defaultStyle, &agui::ListBox::defaultStyle,
           &agui::ProgressBar::defaultStyle, &agui::RadioButton::defaultStyle,
           &agui::HorizontalPolicy::defaultStyle, &agui::VerticalPolicy::defaultStyle,
           &agui::Slider::defaultStyle, &agui::Switch::defaultStyle,
           &agui::Tab::defaultStyle, &agui::TextBox::defaultStyle })
      DefineBaseProperties(*s, agui::StretchRule::Off);

  // Tooltips are frames, and their text is ordinary label text.
  agui::ToolTip::defaultToolTipStyle.setParent(&agui::Frame::defaultStyle);
  agui::ToolTip::defaultToolTipOuterStyle.setParent(&agui::Frame::defaultStyle);
  agui::ToolTip::defaultTitleStyle.setParent(&agui::Label::defaultStyle);
  agui::ToolTip::defaultLabelStyle.setParent(&agui::Label::defaultStyle);
}

// A sub-style made by initXxx() starts with no parent, which would send its
// property lookups off the end of the chain. Hook it back onto a root. (This
// is what a style.lua style without a `parent` gets implicitly.)
template<class S>
S* Under(S* style, const agui::Style* parent)
{
  style->setParent(parent);
  return style;
}

agui::ElementImageSet Set(agui::ElementImageSet::Layer base, agui::ElementImageSet::Layer shadow = {},
                          agui::ElementImageSet::Layer glow = {})
{
  agui::ElementImageSet set;
  set.base   = std::move(base);
  set.shadow = std::move(shadow);
  set.glow   = std::move(glow);
  return set;
}

// The derived sheet: a dark "v" for the drop-down button (Factorio draws its
// own icon file for it, which isn't in the atlas). Drawn at 2x like the atlas.
constexpr Piece DROPDOWN_ARROW{ 0, 0, 16, 16 };
// A flat opaque square, for anything that wants a plain rectangle of colour --
// the dimmer behind the menu takes this and tints it.
constexpr Piece SOLID{ 16, 0, 4, 4 };
// Its middle, clear of the edges -- where bilinear sampling would mix in the
// transparent texels round it and give anything drawn from it a pale rim.
constexpr Piece SOLID_INNER{ 17, 1, 2, 2 };

::Image ComposeDerived()
{
  ::Image img = GenImageColor(DROPDOWN_ARROW.w + SOLID.w, DROPDOWN_ARROW.h, ::Color{ 0, 0, 0, 0 });
  const ::Color dark{ 32, 32, 32, 255 };
  for (int row = 0; row < 6; ++row)  // an 11-wide triangle narrowing to a point
    ImageDrawRectangle(&img, 3 + row, 5 + row, 11 - 2 * row, 1, dark);
  ImageDrawRectangle(&img, SOLID.x, SOLID.y, SOLID.w, SOLID.h, ::Color{ 255, 255, 255, 255 });
  return img;
}

}  // namespace

// ----------------------------------------------------------- building blocks

agui_raylib::RaylibImage* Theme::picture(const Piece& piece, const agui::Color& tint)
{
  return this->picture(this->atlas, piece, tint);
}

agui_raylib::RaylibImage* Theme::picture(const std::shared_ptr<Texture2D>& sheet, const Piece& piece,
                                         const agui::Color& tint)
{
  this->images.push_back(std::make_unique<agui_raylib::RaylibImage>(
      sheet, ::Rectangle{ float(piece.x), float(piece.y), float(piece.w), float(piece.h) },
      SPRITE_SCALE, tint));
  return this->images.back().get();
}

Theme::Layer Theme::layer(const Parts& parts, const Look& look)
{
  const auto part = [&](const std::optional<Piece>& piece) -> agui::Image* {
    return piece ? this->picture(*piece, look.tint) : nullptr;
  };

  Layer l;
  l.type     = Layer::Type::Composition;
  l.drawType = look.outer ? Layer::DrawType::Outer : Layer::DrawType::Inner;
  l.opacity  = 1.0f;  // defaults to 0, i.e. invisible
  l.leftTop     = part(parts.leftTop);
  l.top         = part(parts.top);
  l.rightTop    = part(parts.rightTop);
  l.left        = part(parts.left);
  l.center      = part(parts.center);
  l.right       = part(parts.right);
  l.leftBottom  = part(parts.leftBottom);
  l.bottom      = part(parts.bottom);
  l.rightBottom = part(parts.rightBottom);
  l.tilingFlags = look.tiling;
  l.stretchMonolithImageToSize = look.stretch;
  l.topOuterBorderShift    = look.topShift;
  l.bottomOuterBorderShift = look.bottomShift;
  l.leftOuterBorderShift   = look.leftShift;
  l.rightOuterBorderShift  = look.rightShift;

  // How much of the edge is frame rather than content.
  const auto thickness = [](const std::optional<Piece>& a, const std::optional<Piece>& b, bool wide) {
    const std::optional<Piece>& p = a ? a : b;
    return p ? Scaled(wide ? p->w : p->h) : 0;
  };
  l.leftMonolithBorder   = thickness(parts.leftTop, parts.left, true);
  l.rightMonolithBorder  = thickness(parts.rightTop, parts.right, true);
  l.topMonolithBorder    = thickness(parts.leftTop, parts.top, false);
  l.bottomMonolithBorder = thickness(parts.leftBottom, parts.bottom, false);
  return l;
}

Theme::Layer Theme::composition(int x, int y, int corner)
{
  return this->layer(Expand(x, y, corner), {});
}

Theme::Layer Theme::monolith(const Piece& piece)
{
  Parts p;
  p.center = piece;
  return this->layer(p, {});
}

// style.lua: default_glow. Also default_shadow and default_dirt, by tint.
Theme::Layer Theme::defaultGlow(const agui::Color& tint)
{
  return this->layer(Expand(200, 128, 8), { .outer = true, .tint = tint });
}

Theme::Layer Theme::defaultInnerShadow()
{
  return this->layer(Expand(183, 128, 8), { .tint = HARD_SHADOW });
}

Theme::Layer Theme::topShadow()
{
  Parts p;
  p.top    = Piece{ 208, 128, 1, 8 };
  p.center = Piece{ 208, 136, 1, 1 };
  return this->layer(p, { .outer = true, .tint = SHADOW });
}

// style.lua: bottom_shadow.
Theme::Layer Theme::bottomShadow()
{
  Parts p;
  p.bottom = Piece{ 208, 137, 1, 8 };
  p.center = Piece{ 208, 136, 1, 1 };
  return this->layer(p, { .outer = true, .tint = SHADOW });
}

Theme::Layer Theme::roundedCornersGlow(const agui::Color& tint)
{
  return this->layer(Expand(240, 783, 16),
               { .outer = true, .tint = tint, .topShift = 4, .bottomShift = -4, .leftShift = 4, .rightShift = -4 });
}

// style.lua: rounded_button_glow.
Theme::Layer Theme::roundedButtonGlow(const agui::Color& tint)
{
  return this->layer(Expand(256, 191, 16),
               { .outer = true, .tint = tint, .topShift = 4, .bottomShift = -4, .leftShift = 4, .rightShift = -4 });
}

Theme::Layer Theme::tabGlow(const agui::Color& tint)
{
  Parts p;
  p.leftTop  = Piece{ 216, 0, 16, 16 };
  p.top      = Piece{ 208, 128, 1, 8 };
  p.rightTop = Piece{ 232, 0, 16, 16 };
  p.left     = Piece{ 200, 136, 8, 1 };
  p.right    = Piece{ 209, 136, 8, 1 };
  return this->layer(p, { .outer = true, .tint = tint, .topShift = 4, .leftShift = 4, .rightShift = -4 });
}

Theme::Layer Theme::radiobuttonGlow(const agui::Color& tint)
{
  return this->layer({ .center = Piece{ 123, 156, 34, 34 } }, { .tint = tint, .stretch = false });
}

// left_slider_glow (x = 481) and right_slider_glow (x = 537).
Theme::Layer Theme::slidingGlow(int x, const agui::Color& tint)
{
  return this->layer({ .center = Piece{ x, 96, 56, 40 } },
               { .outer = true, .tint = tint, .topShift = -4, .bottomShift = 4, .leftShift = -4, .rightShift = 4 });
}

Theme::Layer Theme::backButtonGlow(const agui::Color& tint)
{
  Parts p;
  p.left        = Piece{ 304, 424, 32, 80 };
  p.rightTop    = Piece{ 209, 128, 8, 8 };
  p.right       = Piece{ 209, 136, 8, 1 };
  p.rightBottom = Piece{ 209, 137, 8, 8 };
  p.center      = Piece{ 336, 424, 1, 80 };
  return this->layer(p, { .outer = true, .tint = tint, .leftShift = 12 });
}

Theme::Layer Theme::forwardButtonGlow(const agui::Color& tint)
{
  Parts p;
  p.right      = Piece{ 336, 424, 32, 80 };
  p.leftTop    = Piece{ 200, 128, 8, 8 };
  p.left       = Piece{ 200, 136, 8, 1 };
  p.leftBottom = Piece{ 200, 137, 8, 8 };
  p.center     = Piece{ 336, 424, 1, 80 };
  return this->layer(p, { .outer = true, .tint = tint, .rightShift = -12 });
}

// style.lua: arrow_back(tileset, index). The pointed left end
// comes from the arrow sheet, the square right end from the plain button.
Theme::Layer Theme::arrowBack(const ArrowTileset& tileset, int index)
{
  constexpr int ARROWS_X = 0, ARROW_W = 48, ARROW_H = 64;
  constexpr int COMPOSITION_Y = 17, CORNER = 8;
  const int arrowShift       = index * ARROW_W * 2;
  const int compositionShift = index * (CORNER * 2 + 1);
  const int rightX           = tileset.compositionX + CORNER + 1 + compositionShift;

  Parts p;
  p.left        = Piece{ ARROWS_X + arrowShift, tileset.arrowsY, ARROW_W / 2, ARROW_H };
  p.rightTop    = Piece{ rightX, COMPOSITION_Y, CORNER, CORNER };
  p.right       = Piece{ rightX, COMPOSITION_Y + CORNER, CORNER, 1 };
  p.rightBottom = Piece{ rightX, COMPOSITION_Y + CORNER + 1, CORNER, CORNER };
  p.center      = Piece{ ARROWS_X + ARROW_W / 2 + arrowShift, tileset.arrowsY, 1, ARROW_H };
  return this->layer(p, {});
}

// style.lua: arrow_forward(tileset, index). The mirror of the above -- the
// square end on the left, the point on the right.
Theme::Layer Theme::arrowForward(const ArrowTileset& tileset, int index)
{
  constexpr int ARROWS_X = 0, ARROW_W = 48, ARROW_H = 64;
  constexpr int COMPOSITION_Y = 17, CORNER = 8;
  const int arrowShift       = index * ARROW_W * 2;
  const int compositionShift = index * (CORNER * 2 + 1);
  const int leftX            = tileset.compositionX + compositionShift;
  const int pointX           = ARROWS_X + ARROW_W / 2 + arrowShift;

  Parts p;
  p.right      = Piece{ pointX, tileset.arrowsY, ARROW_W / 2, ARROW_H };
  p.leftTop    = Piece{ leftX, COMPOSITION_Y, CORNER, CORNER };
  p.left       = Piece{ leftX, COMPOSITION_Y + CORNER, CORNER, 1 };
  p.leftBottom = Piece{ leftX, COMPOSITION_Y + CORNER + 1, CORNER, CORNER };
  p.center     = Piece{ pointX, tileset.arrowsY, 1, ARROW_H };
  return this->layer(p, {});
}

// style.lua: draggable_space. Three ridged tiles that repeat across whatever
// width the strip is given.
Theme::Layer Theme::draggableRidges()
{
  Parts ridges;
  ridges.top    = Piece{ 192, 8, 8, 7 };
  ridges.center = Piece{ 200, 8, 8, 8 };
  ridges.bottom = Piece{ 208, 8, 8, 8 };
  return this->layer(ridges, { .tiling = Layer::TOP_TILING | Layer::CENTER_TILING_HORIZONTAL |
                                   Layer::BOTTOM_TILING });
}

std::unique_ptr<agui::Image> Theme::atlasImage(int x, int y, int w, int h) const
{
  return std::make_unique<agui_raylib::RaylibImage>(
      this->atlas, ::Rectangle{ float(x), float(y), float(w), float(h) }, SPRITE_SCALE);
}

// utility-sprites.lua: info (16 x 40) and reset (32 x 32), both scale = 0.5.
std::unique_ptr<agui::Image> Theme::infoIcon() const
{
  return std::make_unique<agui_raylib::RaylibImage>(this->info, ::Rectangle{ 0, 0, 16, 40 }, SPRITE_SCALE);
}

std::unique_ptr<agui::Image> Theme::resetIcon(bool enabled) const
{
  return std::make_unique<agui_raylib::RaylibImage>(enabled ? this->reset : this->resetWhite, ::Rectangle{ 0, 0, 32, 32 },
                                                    SPRITE_SCALE);
}

// utility-sprites.lua: search, white, 32 x 32 at no scale -- the frame
// action button stretches it over the 24 it is.
std::unique_ptr<agui::Image> Theme::searchIcon() const
{
  return std::make_unique<agui_raylib::RaylibImage>(this->search, ::Rectangle{ 0, 0, 32, 32 });
}

// resources/icon/goeditor-256.png, the exe's icon at its biggest: 128 x 128
// in the GUI, as big as Factorio's About shows its own.
std::unique_ptr<agui::Image> Theme::appIcon() const
{
  return std::make_unique<agui_raylib::RaylibImage>(this->appIconTexture, ::Rectangle{ 0, 0, 256, 256 }, SPRITE_SCALE);
}

// ------------------------------------------------------------------- theme

Theme::Theme(float scale)
    : dimLabel(&agui::Label::defaultStyle)
    , headingLabel(&agui::Label::defaultStyle)
    , captionLabel(&agui::Label::defaultStyle)
    , linkLabel(&agui::Label::defaultStyle)
    , versionLabel(&agui::Label::defaultStyle)
    , goodLabel(&agui::Label::defaultStyle)
    , badLabel(&agui::Label::defaultStyle)
    , menuButton(&agui::Button::defaultStyle)
    , continueButton(&this->menuButton)
    , backButton(&agui::Button::defaultStyle)
    , redBackButton(&this->backButton)
    , forwardButton(&this->backButton)
    , menuFrame(&agui::Frame::defaultStyle)
    , insideShallowFrame(&agui::Frame::defaultStyle)
    , insideShallowFrameWithPadding(&this->insideShallowFrame)
    , insideDeepFrame(&agui::Frame::defaultStyle)
    , subheaderFrame(&agui::Frame::defaultStyle)
    , borderedFrame(&agui::Frame::defaultStyle)
    , pointPlain(&this->pointBase)
    , pointDark(&agui::Label::defaultStyle)
    , pointLight(&this->pointDark)
    , pointDarkSmall(&this->pointDark)
    , pointLightSmall(&this->pointDark)
    , coordinate(&agui::Label::defaultStyle)
    , treeNode(&agui::Button::defaultStyle)
    , treeNodeCurrent(&this->treeNode)
    , treeNodeSelected(&this->treeNode)
    , treeNodeSelectedCurrent(&this->treeNode)
    , treeBranchButton(&agui::Button::defaultStyle)
    , treeNumberDark(&this->pointDark)
    , treeNumberLight(&this->pointLight)
    , toolButton(&agui::Button::defaultStyle)
    , greenToolButton(&this->toolButton)
    , redToolButton(&this->toolButton)
    , smallButton(&agui::Button::defaultStyle)
    , playerInputFlow(&agui::HorizontalFlow::defaultStyle)
    , frameSubheadingLabel(&agui::Label::defaultStyle)
    , sliderValueField(&agui::TextField::defaultStyle)
    , notchedSlider(&agui::Slider::defaultStyle)
    , frameActionButton(&agui::Button::defaultStyle)
    , frameActionIcon(&agui::ImageWidget::defaultStyle)
    , searchPopupFrame(&agui::Frame::defaultStyle)
    , searchPopupField(&agui::TextField::defaultStyle)
    , scrollPaneUnderSubheader(&agui::ScrollPane::defaultStyle)
    , shallowFrame(&agui::Frame::defaultStyle)
    , controlTable(&agui::Table::defaultStyle)
    , controlButton(&agui::Button::defaultStyle)
    , controlConflictButton(&this->controlButton)
    // style.lua's fonts: default-small, default, default-semibold, default-bold,
    // heading-2 and heading-1.
    , smallFont(FontAt(RasterPx(LineHeight(12), scale)), LineHeight(12), 0)
    , bodyFont(FontAt(RasterPx(LineHeight(14), scale)), LineHeight(14), 0)
    , semiboldFont(FontAt(RasterPx(LineHeight(14), scale), Weight::SemiBold), LineHeight(14), 0)
    , boldFont(FontAt(RasterPx(LineHeight(14), scale), Weight::Bold), LineHeight(14), 0)
    , headingFont(FontAt(RasterPx(LineHeight(15), scale), Weight::Bold), LineHeight(15), 0)
    , bigFont(FontAt(RasterPx(LineHeight(18), scale), Weight::Bold), LineHeight(18), 0)
{
  this->viewScale = scale;  // what the fonts above were made for
  ::Image atlasImg   = LoadImageFromMemory(".png", GUI_ATLAS_PNG, int(GUI_ATLAS_PNG_SIZE));
  ::Image derivedImg = ComposeDerived();
  this->atlas   = agui_raylib::MakeSharedTexture(atlasImg);
  this->derived = agui_raylib::MakeSharedTexture(derivedImg);
  UnloadImage(atlasImg);
  UnloadImage(derivedImg);
  // Everything is drawn at half size, where bilinear sampling averages the
  // 2x2 texels under each pixel.
  SetTextureFilter(*this->atlas, TEXTURE_FILTER_BILINEAR);
  SetTextureFilter(*this->derived, TEXTURE_FILTER_BILINEAR);

  const auto load = [](const unsigned char* png, std::size_t size) {
    ::Image image = LoadImageFromMemory(".png", png, int(size));
    std::shared_ptr<Texture2D> texture = agui_raylib::MakeSharedTexture(image);
    UnloadImage(image);
    SetTextureFilter(*texture, TEXTURE_FILTER_BILINEAR);
    return texture;
  };
  this->info       = load(ICON_INFO_PNG, ICON_INFO_PNG_SIZE);
  this->reset      = load(ICON_RESET_PNG, ICON_RESET_PNG_SIZE);
  this->resetWhite = load(ICON_RESET_WHITE_PNG, ICON_RESET_WHITE_PNG_SIZE);
  this->search     = load(ICON_SEARCH_PNG, ICON_SEARCH_PNG_SIZE);
  this->appIconTexture = load(APP_ICON_PNG, APP_ICON_PNG_SIZE);

  // --- root defaults: every property a widget might read is set here ---
  // For rich text's [font=...], by style.lua's names.
  for (auto [name, font] : { std::pair{ "default-small", &this->smallFont }, std::pair{ "default", &this->bodyFont },
                             std::pair{ "default-semibold", &this->semiboldFont }, std::pair{ "default-bold", &this->boldFont },
                             std::pair{ "heading-2", &this->headingFont }, std::pair{ "heading-1", &this->bigFont } })
      agui_raylib::RegisterFont(name, font);

  DefineAllRootStyles();
  this->themeBasics();
  this->themeToggles();
  this->themeSliders();
  this->themeBars();
  this->themeText();
  this->themeContainers();
  this->themeLists();
  this->themeCharts();
  this->themeMenus();
  this->themeBoard();
}

void Theme::themeBasics()
{
  agui::LabelStyle& label = agui::Label::defaultStyle;
  label.setFont(&this->bodyFont);
  label.setFontColor(PURE_WHITE);
  label.setGameControllerHoveredFontColor(Rgb(255, 173, 0));
  label.setDisabledFontColor(WHITE_HALF);
  label.setParentHoveredColor(PURE_BLACK);
  label.setRichTextSetting(agui::RichTextSetting::Disabled);
  label.setRichTextHighlightErrorColor(Rgb(255, 0, 0));
  label.setRichTextHighlightWarningColor(Rgb(255, 255, 0));
  label.setRichTextHighlightOkColor(Rgb(0, 255, 0));
  label.setSingleLine(true);

  // button: grey, orange with a glow when hovered, and a dirty edge.
  const Layer dirt = this->defaultGlow(DIRT);
  agui::ButtonStyle& b = agui::Button::defaultStyle;
  b.setFont(&this->semiboldFont);
  b.setDefaultFontColor(PURE_BLACK);
  b.setHoveredFontColor(PURE_BLACK);
  b.setClickedFontColor(PURE_BLACK);
  b.setDisabledFontColor(DISABLED_BUTTON);
  b.setSelectedFontColor(PURE_BLACK);
  b.setSelectedHoveredFontColor(PURE_BLACK);
  b.setSelectedClickedFontColor(PURE_BLACK);
  b.setStrikethroughColor(GREY);
  b.setPieProgressColor(PURE_WHITE);
  b.setClickedVerticalOffset(1);  // text goes down on click
  b.setDrawShadowUnderPicture(false);
  b.setDrawGrayscalePicture(false);
  b.setInvertColorsOfPictureWhenHoveredOrToggled(false);
  b.setInvertColorsOfPictureWhenDisabled(false);
  b.setIconHorizontalAlign(agui::HorizontalAlign::Center);
  const agui::ElementImageSet idle     = Set(this->composition(0, 17, 8), dirt);
  const agui::ElementImageSet hovered  = Set(this->composition(34, 17, 8), dirt, this->defaultGlow(GLOW));
  const agui::ElementImageSet clicked  = Set(this->composition(51, 17, 8), dirt);
  const agui::ElementImageSet disabled = Set(this->composition(17, 17, 8), dirt);
  const agui::ElementImageSet selected = Set(this->composition(225, 17, 8), dirt);
  const agui::ElementImageSet selectedHovered = Set(this->composition(369, 17, 8), dirt);
  const agui::ElementImageSet selectedClicked = Set(this->composition(352, 17, 8), dirt);
  b.setDefaultGraphicalSet(&idle);
  b.setHoveredGraphicalSet(&hovered);
  b.setClickedGraphicalSet(&clicked);
  b.setDisabledGraphicalSet(&disabled);
  b.setSelectedGraphicalSet(&selected);
  b.setSelectedHoveredGraphicalSet(&selectedHovered);
  b.setSelectedClickedGraphicalSet(&selectedClicked);
  b.setPaddings(0, 8, 0, 8);
  b.setMinimalWidth(108);
  b.setMinimalHeight(28);
  b.setHorizontalAlign(agui::HorizontalAlign::Center);
  b.setVerticalAlign(agui::VerticalAlign::Center);

  // default_container_spacing: one "module" of 4px.
  agui::VerticalFlow::defaultStyle.setVerticalSpacing(4);
  agui::HorizontalFlow::defaultStyle.setHorizontalSpacing(4);
  agui::Flow::defaultStyle.setHorizontalSpacing(4);
  agui::Flow::defaultStyle.setVerticalSpacing(4);
  agui::Flow::defaultStyle.setMaxOnRow(0);
  agui::EmptyWidget::defaultStyle.initGraphicalSet(&this->none);

  agui::ImageStyle& image = agui::ImageWidget::defaultStyle;
  image.setGraphicalSet(&this->none);
  image.setStretchImageToWidgetSize(false);
  image.setInvertColorsOfPictureWhenHoveredOrToggled(false);

  // line: style.lua's border_image_set().
  agui::BorderImageSet line;
  line.isSet          = true;
  line.lineWidth      = Scaled(8);
  line.verticalLine   = this->picture({ 0, 40, 8, 1 }, PURE_WHITE);
  line.horizontalLine = this->picture({ 8, 40, 1, 8 }, PURE_WHITE);
  line.topRightCorner    = this->picture({ 16, 40, 8, 8 }, PURE_WHITE);
  line.bottomRightCorner = this->picture({ 24, 40, 8, 8 }, PURE_WHITE);
  line.bottomLeftCorner  = this->picture({ 32, 40, 8, 8 }, PURE_WHITE);
  line.topLeftCorner     = this->picture({ 40, 40, 8, 8 }, PURE_WHITE);
  line.bottomT  = this->picture({ 48, 40, 8, 8 }, PURE_WHITE);
  line.leftT    = this->picture({ 56, 40, 8, 8 }, PURE_WHITE);
  line.topT     = this->picture({ 64, 40, 8, 8 }, PURE_WHITE);
  line.rightT   = this->picture({ 72, 40, 8, 8 }, PURE_WHITE);
  line.cross    = this->picture({ 80, 40, 8, 8 }, PURE_WHITE);
  line.topEnd    = this->picture({ 88, 40, 8, 8 }, PURE_WHITE);
  line.rightEnd  = this->picture({ 96, 40, 8, 8 }, PURE_WHITE);
  line.bottomEnd = this->picture({ 104, 40, 8, 8 }, PURE_WHITE);
  line.leftEnd   = this->picture({ 112, 40, 8, 8 }, PURE_WHITE);
  agui::Line::defaultStyle.setBorder(line);
  this->borderedFrame.setBorder(line);  // bordered_frame: border = border_image_set()
  // control_settings_bordered_table, from bordered_table: the same lines
  // round it and between its rows.
  this->controlTable.setBorder(line);
}

void Theme::themeToggles()
{
  const Layer dirt = this->defaultGlow(DIRT);
  const Layer glow = this->defaultGlow(GLOW);

  // checkbox: grey box, orange once checked.
  agui::CheckBoxStyle& check = agui::CheckBox::defaultStyle;
  check.setFont(&this->bodyFont);
  check.setFontColor(PURE_WHITE);
  check.setDisabledFontColor(WHITE_HALF);
  check.setTextPadding(8);
  check.setCheckmark(this->picture({ 112, 132, 28, 28 }, PURE_WHITE));
  check.setDisabledCheckmark(this->picture({ 456, 188, 28, 28 }, PURE_WHITE));
  check.setIntermediateMark(this->picture({ 456, 160, 28, 28 }, PURE_WHITE));
  const agui::ElementImageSet box            = Set(this->monolith({ 0, 132, 28, 28 }), dirt);
  const agui::ElementImageSet boxHovered     = Set(this->monolith({ 56, 132, 28, 28 }), {}, glow);
  const agui::ElementImageSet boxClicked     = Set(this->monolith({ 84, 132, 28, 28 }), {}, glow);
  const agui::ElementImageSet boxDisabled    = Set(this->monolith({ 28, 132, 28, 28 }), dirt);
  const agui::ElementImageSet boxChecked     = Set(this->monolith({ 56, 132, 28, 28 }), dirt);
  check.setDefaultGraphicalSet(&box);
  check.setHoveredGraphicalSet(&boxHovered);
  check.setClickedGraphicalSet(&boxClicked);
  check.setDisabledGraphicalSet(&boxDisabled);
  check.setSelectedGraphicalSet(&boxChecked);
  check.setSelectedHoveredGraphicalSet(&boxHovered);
  check.setSelectedClickedGraphicalSet(&boxClicked);
  check.setVerticalAlign(agui::VerticalAlign::Center);

  // radiobutton: the selected dot is part of the background sprite.
  const Layer radioDirt = this->radiobuttonGlow(DIRT);
  const Layer radioGlow = this->radiobuttonGlow(GLOW);
  agui::RadioButtonStyle& radio = agui::RadioButton::defaultStyle;
  radio.setFont(&this->bodyFont);
  radio.setFontColor(PURE_WHITE);
  radio.setDisabledFontColor(WHITE_HALF);
  radio.setTextPadding(8);
  const agui::ElementImageSet radioIdle     = Set(this->monolith({ 0, 160, 24, 24 }), radioDirt);
  const agui::ElementImageSet radioHovered  = Set(this->monolith({ 24, 160, 24, 24 }), radioGlow);
  const agui::ElementImageSet radioClicked  = Set(this->monolith({ 48, 160, 24, 24 }), radioDirt);
  const agui::ElementImageSet radioDisabled = Set(this->monolith({ 96, 160, 24, 24 }), radioDirt);
  const agui::ElementImageSet radioSelected = Set(this->monolith({ 72, 160, 24, 24 }), radioDirt);
  radio.setDefaultGraphicalSet(&radioIdle);
  radio.setHoveredGraphicalSet(&radioHovered);
  radio.setClickedGraphicalSet(&radioClicked);
  radio.setDisabledGraphicalSet(&radioDisabled);
  radio.setSelectedGraphicalSet(&radioSelected);
  radio.setSelectedHoveredGraphicalSet(&radioHovered);
  radio.setSelectedClickedGraphicalSet(&radioClicked);
  radio.setVerticalAlign(agui::VerticalAlign::Center);

  // switch
  agui::SwitchStyle& sw = agui::Switch::defaultStyle;
  sw.setBackgroundDefault(this->picture({ 0, 96, 64, 32 }, PURE_WHITE));
  sw.setBackgroundHover(this->picture({ 64, 96, 64, 32 }, PURE_WHITE));
  sw.setBackgroundDisabled(this->picture({ 0, 96, 64, 32 }, PURE_WHITE));
  sw.setLeftButtonPosition(2);
  sw.setMiddleButtonPosition(9);
  sw.setRightButtonPosition(16);
  sw.setMinimalWidth(32);
  sw.setMaximalWidth(32);
  sw.setMinimalHeight(16);
  sw.setMaximalHeight(16);
  agui::ButtonStyle* knob = Under(sw.initButtonStyle(), &agui::Button::defaultStyle);
  const agui::ElementImageSet knobIdle    = Set(this->monolith({ 128, 96, 28, 28 }));
  const agui::ElementImageSet knobHovered = Set(this->monolith({ 156, 96, 28, 28 }));
  const agui::ElementImageSet knobClicked = Set(this->monolith({ 184, 96, 28, 28 }));
  knob->setDefaultGraphicalSet(&knobIdle);
  knob->setHoveredGraphicalSet(&knobHovered);
  knob->setClickedGraphicalSet(&knobClicked);
  knob->setDisabledGraphicalSet(&knobIdle);
  knob->setSelectedGraphicalSet(&knobIdle);
  knob->setPaddings(0, 0, 0, 0);
  for (auto set : { &agui::Style::setMinimalWidth, &agui::Style::setMaximalWidth,
                    &agui::Style::setMinimalHeight, &agui::Style::setMaximalHeight })
      (knob->*set)(14);
  agui::LabelStyle* active = Under(sw.initActiveLabelStyle(), &agui::Label::defaultStyle);
  active->setFont(&this->boldFont);
  active->setFontColor(Rgb(241, 190, 100));
  agui::LabelStyle* inactive = Under(sw.initInactiveLabelStyle(), &agui::Label::defaultStyle);
  inactive->setHoveredFontColor(CAPTION);
}

void Theme::themeSliders()
{
  const Layer dirt   = this->defaultGlow(DIRT);
  const Layer shadow = this->defaultGlow(SHADOW);
  const Layer glow   = this->defaultGlow(GLOW);

  const agui::ElementImageSet fullBar         = Set(this->composition(73, 72, 8), dirt);
  const agui::ElementImageSet fullBarDisabled = Set(this->composition(90, 72, 8), dirt);
  Parts empty;
  empty.left   = Piece{ 56, 72, 8, 8 };
  empty.center = Piece{ 64, 72, 1, 8 };
  empty.right  = Piece{ 65, 72, 8, 8 };
  Parts emptyDisabled;
  emptyDisabled.left   = Piece{ 56, 80, 8, 8 };
  emptyDisabled.center = Piece{ 65, 80, 1, 8 };
  emptyDisabled.right  = Piece{ 65, 80, 8, 8 };
  const agui::ElementImageSet emptyBar         = Set(this->layer(empty, {}), dirt);
  const agui::ElementImageSet emptyBarDisabled = Set(this->layer(emptyDisabled, {}), dirt);

  for (agui::SliderStyle* s : { static_cast<agui::SliderStyle*>(&agui::Slider::defaultStyle),
                                static_cast<agui::SliderStyle*>(&agui::DoubleSlider::defaultStyle) }) {
    s->setFullBar(fullBar);
    s->setFullBarDisabled(fullBarDisabled);
    s->setEmptyBar(emptyBar);
    s->setEmptyBarDisabled(emptyBarDisabled);
    s->setDrawNotches(false);
    s->setNotch(this->none);
    s->setMinimalWidth(160);
    s->setMinimalHeight(12);
    s->setMaximalHeight(12);
    s->setVerticalAlign(agui::VerticalAlign::Center);
  }

  // slider_button, and the pointed left/right_slider_button of the double slider.
  struct Knob {
    agui::ButtonStyle* style;
    int                x;          // column of the four state sprites
    bool               pointed;
  };
  agui::SliderStyle& single = agui::Slider::defaultStyle;
  agui::SliderStyle& range  = agui::DoubleSlider::defaultStyle;
  for (Knob k : { Knob{ single.initButtonStyle(), 0, false }, Knob{ single.initHighButtonStyle(), 0, false },
                  Knob{ range.initButtonStyle(), 489, true }, Knob{ range.initHighButtonStyle(), 529, true } }) {
    Under(k.style, &agui::Button::defaultStyle);
    agui::ElementImageSet idle, hovered, clicked, disabled;
    if (k.pointed) {
      const int glowX = k.x == 489 ? 481 : 537;
      idle     = Set(this->monolith({ k.x, 0, 40, 24 }), this->slidingGlow(glowX, SHADOW));
      hovered  = Set(this->monolith({ k.x, 48, 40, 24 }), {}, this->slidingGlow(glowX, GLOW));
      clicked  = Set(this->monolith({ k.x, 72, 40, 24 }), this->slidingGlow(glowX, SHADOW));
      disabled = Set(this->monolith({ k.x, 24, 40, 24 }), this->slidingGlow(glowX, SHADOW));
    } else {
      idle     = Set(this->monolith({ 64, 48, 40, 24 }), shadow);
      hovered  = Set(this->monolith({ 144, 48, 40, 24 }), {}, glow);
      clicked  = Set(this->monolith({ 184, 48, 40, 24 }), shadow);
      disabled = Set(this->monolith({ 104, 48, 40, 24 }), shadow);
    }
    k.style->setDefaultGraphicalSet(&idle);
    k.style->setHoveredGraphicalSet(&hovered);
    k.style->setClickedGraphicalSet(&clicked);
    k.style->setSelectedGraphicalSet(&hovered);
    k.style->setDisabledGraphicalSet(&disabled);
    k.style->setPaddings(0, 0, 0, 0);
    k.style->setMinimalWidth(20);
    k.style->setMaximalWidth(20);
    k.style->setMinimalHeight(12);
    k.style->setMaximalHeight(12);
  }

  // notched_slider: a notch for every value it can take, with the bar under
  // them, and a knob that points down at the notch it is on.
  agui::ElementImageSet notch = Set(this->monolith({ 138, 200, 4, 16 }),
                                    this->layer({ .center = Piece{ 146, 192, 20, 32 } },
                                                { .outer = true, .tint = DIRT, .topShift = -4, .bottomShift = 4,
                                                  .leftShift = -4, .rightShift = 4 }));
  this->notchedSlider.setDrawNotches(true);
  this->notchedSlider.setNotch(notch);
  this->notchedSlider.setMinimalHeight(20);
  this->notchedSlider.setMaximalHeight(20);
  // notched_slider_glow
  const auto pointerGlow = [this](const agui::Color& tint) {
    return this->layer({ .center = Piece{ 96, 184, 40, 48 } },
                       { .outer = true, .tint = tint, .topShift = -2, .bottomShift = 4, .leftShift = -4, .rightShift = 4 });
  };
  agui::ButtonStyle* pointer = Under(this->notchedSlider.initButtonStyle(), &agui::Button::defaultStyle);
  const agui::ElementImageSet pointerIdle     = Set(this->monolith({ 0, 189, 24, 35 }), pointerGlow(SHADOW));
  const agui::ElementImageSet pointerHovered  = Set(this->monolith({ 48, 189, 24, 35 }), {}, pointerGlow(GLOW));
  const agui::ElementImageSet pointerClicked  = Set(this->monolith({ 72, 189, 24, 35 }), pointerGlow(SHADOW));
  const agui::ElementImageSet pointerDisabled = Set(this->monolith({ 24, 189, 24, 35 }), pointerGlow(SHADOW));
  pointer->setDefaultGraphicalSet(&pointerIdle);
  pointer->setHoveredGraphicalSet(&pointerHovered);
  pointer->setClickedGraphicalSet(&pointerClicked);
  pointer->setSelectedGraphicalSet(&pointerHovered);
  pointer->setDisabledGraphicalSet(&pointerDisabled);
  pointer->setPaddings(0, 0, 0, 0);
  pointer->setMinimalWidth(12);
  pointer->setMaximalWidth(12);
  pointer->setMinimalHeight(17);
  pointer->setMaximalHeight(17);
}

void Theme::themeBars()
{
  // progressbar / activity_bar: a white bar sprite, tinted green.
  this->barBackground = Set(this->composition(296, 48, 8), this->defaultGlow(DIRT));
  this->bar           = Set(this->composition(313, 48, 8));
  const agui::Color green = Rgb(0, 255, 0);

  agui::ProgressBarStyle& progress = agui::ProgressBar::defaultStyle;
  progress.setBar(&this->bar);
  progress.setBackground(&this->barBackground);
  progress.setBarWidth(8);  // thickness, not length
  progress.setColor(green);
  progress.setOtherColors({});
  progress.setFont(&this->bodyFont);
  progress.setFontColor(PURE_WHITE);
  progress.setFilledFontColor(PURE_BLACK);
  progress.setEmbedTextInBar(false);
  progress.setSideTextPadding(8);
  progress.setMinimalWidth(200);

  agui::ActivityBarStyle& activity = agui::ActivityBar::defaultStyle;
  activity.setBar(&this->bar);
  activity.setBackground(&this->barBackground);
  activity.setBarWidth(8);
  activity.setBarSizeRatio(0.07f);
  activity.setSpeed(0.01f);
  activity.setColor(green);
  activity.setMinimalWidth(200);
}

void Theme::themeText()
{
  agui::TextBoxStyle& text = agui::TextBox::defaultStyle;
  const Layer dirt = this->roundedCornersGlow(DIRT);  // textbox_dirt
  text.setFont(&this->bodyFont);
  text.setFontColor(PURE_BLACK);
  text.setDisabledFontColor(WHITE_HALF);
  text.setSelectionBackgroundColor(Rgb(241, 190, 100));
  text.setDefaultBackground(Set(this->composition(248, 0, 8), dirt));
  text.setActiveBackground(Set(this->composition(265, 0, 8), dirt));
  text.setGameControllerHoveredBackground(Set(this->composition(265, 0, 8), dirt));
  text.setDisabledBackground(Set(this->composition(282, 0, 8), dirt));
  text.setRichTextSettings(agui::RichTextSetting::Disabled);
  text.setRichTextHighlightErrorColor(Rgb(166, 10, 10));
  text.setRichTextHighlightWarningColor(Rgb(255, 90, 0));
  text.setRichTextHighlightOkColor(Rgb(63, 105, 0));
  text.setSelectedRichTextHighlightErrorColor(Rgb(166, 10, 10));
  text.setSelectedRichTextHighlightWarningColor(Rgb(182, 62, 4));
  text.setSelectedRichTextHighlightOkColor(Rgb(50, 80, 0));
  text.setPaddings(0, 2, 0, 3);
  text.setMinimalWidth(200);
  text.setMinimalHeight(28);  // TextField leaves its height entirely to the style
}

void Theme::themeContainers()
{
  // frame: the dark grey window, with a soft shadow round it.
  constexpr int HEADER_H = 24;  // draggable_space_header's height
  agui::FrameStyle& f = agui::Frame::defaultStyle;
  const agui::ElementImageSet window = Set(this->composition(0, 0, 8), this->defaultGlow(SHADOW));
  f.setGraphicalSet(&window);
  f.setUseHeaderFiller(true);
  f.setDragByTitle(true);  // a window moves by its title as well as its drag handle
  f.setBorder(agui::BorderImageSet{});
  f.setPaddings(4, 8, 8, 8);
  Under(f.initVerticalFlowStyle(), &agui::VerticalFlow::defaultStyle);
  Under(f.initHorizontalFlowStyle(), &agui::HorizontalFlow::defaultStyle);
  agui::HorizontalFlowStyle* header = Under(f.initHeaderFlowStyle(), &agui::HorizontalFlow::defaultStyle);
  header->setHorizontallyStretchable(true);
  // frame_header_flow's 4, and Factorio's title stands another module taller
  // than the drag handle, which is top-aligned above that: so a window's
  // content starts two modules below the handle.
  header->setBottomPadding(8);
  header->setHorizontalSpacing(8);
  header->setVerticalAlign(agui::VerticalAlign::Center);
  // draggable_space_header: the ridged strip right of the title.
  agui::ElementImageSet filler = Set(this->draggableRidges(), this->defaultGlow(DIRT_FILLER));
  agui::EmptyWidgetStyle* fillerStyle = Under(f.initHeaderFillerStyle(), &agui::EmptyWidget::defaultStyle);
  fillerStyle->initGraphicalSet(&filler);
  fillerStyle->setLeftMargin(4);
  fillerStyle->setHorizontallyStretchable(true);
  fillerStyle->setVerticallyStretchable(true);
  fillerStyle->setMinimalHeight(HEADER_H);
  fillerStyle->setMaximalHeight(HEADER_H);
  // frame_title. The header is as tall as the title line plus its paddings,
  // so these pad the title out to the drag handle's height. (style.lua's
  // top_margin = -3 / bottom_padding = 3 correct for Titillium's tall
  // ascender and would push Consolas off centre.)
  agui::LabelStyle* title = Under(f.initTitleStyle(), &agui::Label::defaultStyle);
  title->setFont(&this->bigFont);
  title->setFontColor(CAPTION);
  const int16_t titleSlack = int16_t(HEADER_H - this->bigFont.getLineHeight());
  title->setTopPadding(int16_t(titleSlack / 2));
  title->setBottomPadding(int16_t(titleSlack - titleSlack / 2));

  // tooltip_frame: a panel of its own, slightly see-through, close round
  // the text -- no padding above and below it, 4 at the sides.
  const agui::ElementImageSet tooltip =
      Set(this->layer(Expand(403, 0, 8), { .tint = agui::Color(1, 1, 1, 0.88f) }), this->defaultGlow(SHADOW));
  agui::ToolTip::defaultToolTipStyle.setGraphicalSet(&tooltip);
  agui::ToolTip::defaultToolTipStyle.setUseHeaderFiller(false);
  agui::ToolTip::defaultToolTipStyle.setPaddings(0, 4, 0, 4);
  Under(agui::ToolTip::defaultToolTipStyle.initVerticalFlowStyle(), &agui::VerticalFlow::defaultStyle)->setVerticalSpacing(2);
  // tooltip_label: wrapped at this width, not one long line.
  agui::ToolTip::defaultTitleStyle.setFont(&this->boldFont);  // tooltip_title_label
  agui::ToolTip::defaultTitleStyle.setMinimalWidth(50);
  agui::ToolTip::defaultTitleStyle.setMaximalWidth(356);
  agui::ToolTip::defaultLabelStyle.setMinimalWidth(50);
  agui::ToolTip::defaultLabelStyle.setMaximalWidth(356);
  agui::ToolTip::defaultLabelStyle.setSingleLine(false);
  // Keys in a tooltip are in their own font and colour (see ShortcutText).
  agui::ToolTip::defaultLabelStyle.setRichTextSetting(agui::RichTextSetting::ShowFontsAndColors);

  // inside_shallow_frame and inside_deep_frame: panels set into the window.
  // Their rim is drawn outside them, over the window's padding.
  for (auto [style, centerX] : { std::pair{ &this->insideShallowFrame, 76 }, std::pair{ &this->insideDeepFrame, 42 } }) {
    Parts panel = Expand(17, 0, 8);
    panel.center = Piece{ centerX, 8, 1, 1 };
    const agui::ElementImageSet set = Set(this->layer(panel, { .outer = true }), this->defaultInnerShadow());
    style->setGraphicalSet(&set);
    style->setPadding(0);
    style->setUseHeaderFiller(false);
    Under(style->initVerticalFlowStyle(), &agui::VerticalFlow::defaultStyle)->setVerticalSpacing(0);
  }
  this->insideShallowFrameWithPadding.setPadding(12);
  this->insideShallowFrameWithPadding.setHorizontallyStretchable(true);

  // subheader_frame: a darker strip across the top of an inside_shallow_frame,
  // holding a row of readouts or buttons over whatever the panel shows. It is
  // as wide as the panel and a fixed height, the bottom 4 of it its edge.
  Parts strip;
  strip.center = Piece{ 256, 25, 1, 1 };
  strip.bottom = Piece{ 256, 26, 1, 8 };
  const agui::ElementImageSet subheader = Set(this->layer(strip, {}), this->bottomShadow());
  this->subheaderFrame.setGraphicalSet(&subheader);
  this->subheaderFrame.setUseHeaderFiller(false);
  this->subheaderFrame.setPaddings(3, 4, 1, 4);
  this->subheaderFrame.setMinimalHeight(36);
  this->subheaderFrame.setMaximalHeight(36);
  this->subheaderFrame.setHorizontallyStretchable(true);
  this->subheaderFrame.setVerticallyStretchable(false);
  Under(this->subheaderFrame.initHorizontalFlowStyle(), &agui::HorizontalFlow::defaultStyle)
      ->setVerticalAlign(agui::VerticalAlign::Center);

  // Scroll bars: a dark trough with a grey thumb that goes orange on hover.
  const agui::ElementImageSet trough = Set(this->composition(0, 72, 8));
  const Layer shadow = this->defaultGlow(SHADOW);
  const Layer glow   = this->defaultGlow(GLOW);
  struct Thumb {
    agui::ScrollBarStyle* bar;
    bool                  vertical;
  };
  for (Thumb t : { Thumb{ &agui::VerticalPolicy::defaultStyle, true }, Thumb{ &agui::HorizontalPolicy::defaultStyle, false } }) {
    t.bar->setBackgroundGraphicalSet(&trough);
    const auto thumbLayer = [&](int state) {
      Parts p;
      if (t.vertical) {
        const int x = state * 20;
        p.top    = Piece{ x, 48, 20, 7 };
        p.center = Piece{ x, 55, 20, 8 };
        p.bottom = Piece{ x, 63, 20, 7 };
      } else {
        const int x = 224 + state * 24;
        p.left   = Piece{ x, 48, 8, 20 };
        p.center = Piece{ x + 8, 48, 8, 20 };
        p.right  = Piece{ x + 16, 48, 8, 20 };
      }
      return this->layer(p, { .tiling = t.vertical ? Layer::CENTER_TILING_VERTICAL : Layer::CENTER_TILING_HORIZONTAL });
    };
    const agui::ElementImageSet idle    = Set(thumbLayer(0), shadow);
    const agui::ElementImageSet hovered = Set(thumbLayer(1), {}, glow);
    const agui::ElementImageSet clicked = Set(thumbLayer(2), shadow);
    agui::ButtonStyle* thumb = Under(t.bar->initThumbButtonStyle(), &agui::Button::defaultStyle);
    thumb->setDefaultGraphicalSet(&idle);
    thumb->setHoveredGraphicalSet(&hovered);
    thumb->setClickedGraphicalSet(&clicked);
    thumb->setSelectedGraphicalSet(&hovered);
    thumb->setDisabledGraphicalSet(&idle);
    thumb->setPaddings(0, 0, 0, 0);
    thumb->setMinimalWidth(10);
    thumb->setMinimalHeight(10);
  }
  agui::VerticalPolicy::defaultStyle.setMinimalWidth(12);
  agui::VerticalPolicy::defaultStyle.setMaximalWidth(12);
  agui::VerticalPolicy::defaultStyle.setVerticallyStretchable(true);
  agui::HorizontalPolicy::defaultStyle.setMinimalHeight(12);
  agui::HorizontalPolicy::defaultStyle.setMaximalHeight(12);
  agui::HorizontalPolicy::defaultStyle.setHorizontallyStretchable(true);

  // scroll_pane: sunk into its parent (outer_frame_light).
  agui::ScrollPaneStyle& scroll = agui::ScrollPane::defaultStyle;
  Under(scroll.initVerticalFlowStyle(), &agui::VerticalFlow::defaultStyle);
  Under(scroll.initHorizontalScrollBarStyle(), &agui::HorizontalPolicy::defaultStyle);
  Under(scroll.initVerticalScrollBarStyle(), &agui::VerticalPolicy::defaultStyle);
  const agui::ElementImageSet sunk = Set(this->layer(Expand(17, 0, 8), { .outer = true }), this->defaultInnerShadow());
  scroll.initGraphicalSet(&sunk);
  scroll.initBackgroundGraphicalSet(&this->none);
  scroll.setExtraPaddingWhenActivated(4);
  scroll.setExtraTopMarginWhenActivated(0);
  scroll.setExtraBottomMarginWhenActivated(0);
  scroll.setExtraLeftMarginWhenActivated(0);
  scroll.setExtraRightMarginWhenActivated(0);
  scroll.setAlwaysDrawBorders(false);
  scroll.setScrollbarsGoOutside(false);
  scroll.setDontForceClippingRectForContents(false);
  scroll.setVerticallySquashable(true);
  scroll.setHorizontallySquashable(true);

  agui::TableStyle& table = agui::Table::defaultStyle;
  table.setHorizontalSpacing(4);
  table.setVerticalSpacing(4);
  table.setCellPadding(2);
  table.setApplyRowGraphicalSetPerColumn(false);
  table.setWideAsColumnCount(false);
  table.setColumnAlignments(agui::TableStyle::AreaAlignments({}));
  table.setColumnWidths(agui::TableStyle::Widths());
  table.setHoveredRowColor(TRANSPARENT);
  table.setSelectedRowColor(TRANSPARENT);
  table.setVerticalLineColor(TRANSPARENT);
  table.setHorizontalLineColor(TRANSPARENT);
  table.setBorder(agui::BorderImageSet{});
  for (agui::ButtonStyle* order : { table.initColumnOrderingAscendingButtonStyle(),
                                    table.initColumnOrderingDescendingButtonStyle(),
                                    table.initInactiveColumnOrderingAscendingButtonStyle(),
                                    table.initInactiveColumnOrderingDescendingButtonStyle() })
      Under(order, &agui::Button::defaultStyle);

  // tab: grey, orange when hovered, and the colour of the page under it
  // when selected, so the two join up.
  agui::TabStyle& tab = agui::Tab::defaultStyle;
  using TabSet = agui::TabStyle::GraphicalSetType;
  const Layer tabShadow = this->tabGlow(SHADOW);
  tab.setFont(&this->boldFont);
  tab.setBadgeFont(&this->smallFont);
  tab.setBadgeHorizontalSpacing(4);
  tab.setDefaultFontColor(PURE_BLACK);
  tab.setSelectedFontColor(CAPTION);
  tab.setDisabledFontColor(WHITE_HALF);
  tab.setDefaultBadgeFontColor(Rgb(142, 142, 142));
  tab.setSelectedBadgeFontColor(Rgb(64, 64, 64));
  tab.setDisabledBadgeFontColor(WHITE_HALF);
  tab.setDrawGrayscalePicture(false);
  tab.setOverrideGraphicsOnEdges(false);
  tab.setIncreaseHeightWhenSelected(true);
  const agui::ElementImageSet tabIdle     = Set(this->composition(102, 0, 8), tabShadow);
  const agui::ElementImageSet tabSelected = Set(this->composition(136, 0, 8), tabShadow);
  const agui::ElementImageSet tabHovered  = Set(this->composition(153, 0, 8), {}, this->tabGlow(GLOW));
  const agui::ElementImageSet tabClicked  = Set(this->composition(170, 0, 8), tabShadow);
  const agui::ElementImageSet tabDisabled = Set(this->composition(119, 0, 8), tabShadow);
  tab.setDefaultGraphicalSet(&tabIdle);
  tab.setHoveredGraphicalSet(&tabHovered);
  tab.setClickedGraphicalSet(&tabClicked);
  tab.setDisabledGraphicalSet(&tabDisabled);
  tab.setSelectedGraphicalSet(&tabSelected);
  tab.initGraphicalSet(TabSet::LeftEdgeSelected, tabSelected);
  tab.initGraphicalSet(TabSet::RightEdgeSelected, tabSelected);
  const agui::ElementImageSet badge        = Set(this->composition(176, 72, 8));
  const agui::ElementImageSet badgePressed = Set(this->composition(296, 71, 8));
  for (TabSet t : { TabSet::Default, TabSet::Hover, TabSet::Disabled })
    tab.initBadgeGraphicalSet(t, badge);
  for (TabSet t : { TabSet::Selected, TabSet::Press })
    tab.initBadgeGraphicalSet(t, badgePressed);
  tab.setPaddings(7, 8, 9, 8);
  tab.setMinimalWidth(84);
  tab.setHorizontalAlign(agui::HorizontalAlign::Center);
  tab.setVerticalAlign(agui::VerticalAlign::Center);

  // tabbed_pane, with tabbed_pane_with_extra_padding's page.
  agui::TabbedPaneStyle& pane = agui::TabbedPane::defaultStyle;
  pane.setVerticalSpacing(0);
  pane.setTopMargin(12);
  pane.setPadding(0);
  agui::FrameStyle* page = Under(pane.initContentFrame(), &agui::Frame::defaultStyle);
  Parts pageParts;
  pageParts.top    = Piece{ 76, 0, 1, 8 };
  pageParts.center = Piece{ 76, 8, 1, 1 };
  pageParts.bottom = Piece{ 76, 9, 1, 8 };
  const agui::ElementImageSet pageSet = Set(this->layer(pageParts, {}), this->topShadow());
  page->setGraphicalSet(&pageSet);
  page->setUseHeaderFiller(false);
  page->setPaddings(8, 12, 8, 12);
  agui::TableStyle* tabRow = Under(pane.initTabContainerStyle(), &agui::Table::defaultStyle);
  tabRow->setLeftPadding(12);
  tabRow->setRightPadding(12);
  tabRow->setHorizontalSpacing(0);
  tabRow->setVerticalSpacing(0);
}

void Theme::themeLists()
{
  // list_box_item: rows on the dark stripes of the list, orange on hover.
  agui::ListBoxStyle& list = agui::ListBox::defaultStyle;
  agui::ButtonStyle* item = Under(list.initItemStyle(), &agui::Button::defaultStyle);
  const agui::ElementImageSet itemIdle     = Set(this->composition(208, 17, 8));
  const agui::ElementImageSet itemHovered  = Set(this->composition(34, 17, 8));
  const agui::ElementImageSet itemClicked  = Set(this->composition(51, 17, 8));
  const agui::ElementImageSet itemDisabled = Set(this->composition(17, 17, 8));
  const agui::ElementImageSet itemSelected = Set(this->composition(225, 17, 8));
  item->setDefaultGraphicalSet(&itemIdle);
  item->setHoveredGraphicalSet(&itemHovered);
  item->setClickedGraphicalSet(&itemClicked);
  item->setDisabledGraphicalSet(&itemDisabled);
  item->setSelectedGraphicalSet(&itemSelected);
  item->setSelectedHoveredGraphicalSet(&itemHovered);
  item->setSelectedClickedGraphicalSet(&itemClicked);
  item->setDefaultFontColor(PURE_WHITE);
  item->setFont(&this->semiboldFont);  // default-listbox
  item->setHorizontalAlign(agui::HorizontalAlign::Left);
  item->setHorizontallyStretchable(true);
  item->setMinimalWidth(0);

  // list_box_scroll_pane: a deep inset, striped every 28px behind the rows
  // (deep_slot_background_tiling(nil, 28)).
  agui::ScrollPaneStyle* listScroll = Under(list.initScrollPaneStyle(), &agui::ScrollPane::defaultStyle);
  Parts deep = Expand(17, 0, 8);
  deep.center = Piece{ 42, 8, 1, 1 };
  const agui::ElementImageSet deepSet = Set(this->layer(deep, { .outer = true }), this->defaultInnerShadow());
  listScroll->initGraphicalSet(&deepSet);
  agui::ElementImageSet stripes = Set(this->composition(282, 17, 8));
  stripes.base.overallTilingVerticalSize      = 28 - 8;
  stripes.base.overallTilingVerticalSpacing   = 8;
  stripes.base.overallTilingVerticalPadding   = 4;
  stripes.base.overallTilingHorizontalPadding = 4;
  listScroll->initBackgroundGraphicalSet(&stripes);
  listScroll->setAlwaysDrawBorders(true);
  listScroll->setPadding(0);
  listScroll->setExtraPaddingWhenActivated(0);
  Under(listScroll->initVerticalFlowStyle(), &agui::VerticalFlow::defaultStyle)->setVerticalSpacing(0);
  list.setMinimalWidth(180);

  // dropdown: a grey button with a "v", opening a list with a shadow.
  agui::DropDownStyle& drop = agui::DropDown::defaultStyle;
  agui::ButtonStyle* dropButton = Under(drop.initButtonStyle(), &agui::Button::defaultStyle);
  dropButton->setPaddings(0, 0, 0, 0);
  dropButton->setHorizontalAlign(agui::HorizontalAlign::Left);
  dropButton->setFont(&this->bodyFont);
  agui::ListBoxStyle* dropList = Under(drop.initListBoxStyle(), &agui::ListBox::defaultStyle);
  dropList->setMaximalHeight(400);
  agui::ScrollPaneStyle* dropScroll = Under(dropList->initScrollPaneStyle(), listScroll);
  const agui::ElementImageSet shadowOnly = Set({}, this->defaultGlow(SHADOW));
  dropScroll->initGraphicalSet(&shadowOnly);
  dropScroll->initBackgroundGraphicalSet(&this->none);
  this->images.push_back(std::make_unique<agui_raylib::RaylibImage>(
      this->derived,
      ::Rectangle{ float(DROPDOWN_ARROW.x), float(DROPDOWN_ARROW.y), float(DROPDOWN_ARROW.w), float(DROPDOWN_ARROW.h) },
      SPRITE_SCALE));
  drop.setIcon(this->images.back().get());
  drop.setSelectorAndTitleSpacing(8);
  drop.setPaddings(-1, 4, 1, 8);
  drop.setMinimalWidth(116);
  drop.setMinimalHeight(28);
}

void Theme::themeCharts()
{
  agui::GraphStyle& graph = agui::Graph::defaultStyle;
  graph.setFont(&this->bodyFont);
  graph.setBackgroundColor(agui::Color(0.05f, 0.05f, 0.05f, 0.9f));
  graph.setLineColors({ Rgb(0, 109, 255), Rgb(255, 100, 0), Rgb(80, 178, 14), Rgb(204, 25, 40),
                        Rgb(212, 158, 27), Rgb(232, 0, 213), Rgb(0, 159, 173), Rgb(133, 69, 40) });
  graph.setGridLinesColor(agui::Color(0.15f, 0.15f, 0.15f));
  graph.setGuideLinesColor(agui::Color(0.9f, 0.9f, 0.9f));
  graph.setMinimalHorizontalLabelSpacing(40);  // style.lua: 25, for a narrower font
  graph.setMinimalVerticalLabelSpacing(22);
  graph.setHorizontalLabelsMargin(24);
  graph.setVerticalLabelsMargin(36);
  graph.setGraphTopMargin(12);
  graph.setGraphRightMargin(12);
  graph.setDataLineHighlightDistance(20);
  graph.setSelectionDotRadius(3);
  for (agui::LabelStyle* axis : { graph.initHorizontalLabelStyle(), graph.initVerticalLabelStyle() }) {
    Under(axis, &agui::Label::defaultStyle);
    axis->setFont(&this->smallFont);
    axis->setFontColor(Rgb(100, 100, 100));
  }
  graph.setNaturalWidth(550);
  graph.setMinimalHeight(200);
}

void Theme::themeMenus()
{
  this->dimLabel.setFontColor(GREY);  // grey_label

  this->headingLabel.setFont(&this->headingFont);  // heading_2_label
  this->headingLabel.setFontColor(CAPTION);

  this->captionLabel.setFont(&this->boldFont);  // caption_label
  this->captionLabel.setFontColor(CAPTION);

  // hyperlink_label: gui_color.blue, lighter under the mouse, underlined.
  this->linkLabel.setFontColor(Rgb(128, 206, 240));
  this->linkLabel.setHoveredFontColor(Rgb(154, 250, 255));
  this->linkLabel.setUnderlined(true);

  this->versionLabel.setFont(&this->bodyFont);  // main_menu_version_label
  this->versionLabel.setHoveredFontColor(ORANGE_TEXT);

  // The status line under the side panel.
  this->goodLabel.setFontColor(Rgb(160, 255, 180));
  this->badLabel.setFontColor(Rgb(255, 130, 120));

  // menu_button: the big buttons of the main menu.
  this->menuButton.setFont(&this->bigFont);
  for (auto set : { &agui::ButtonStyle::setDefaultFontColor, &agui::ButtonStyle::setHoveredFontColor,
                    &agui::ButtonStyle::setClickedFontColor })
      (this->menuButton.*set)(BOLD_BLACK);
  this->menuButton.setMinimalWidth(320);
  this->menuButton.setMaximalWidth(320);
  this->menuButton.setMinimalHeight(50);
  this->menuButton.setTopPadding(4);
  this->menuButton.setBottomPadding(4);

  // menu_button_continue: the same in green.
  const Layer dirt = this->defaultGlow(DIRT);
  const agui::ElementImageSet green         = Set(this->composition(68, 17, 8), dirt);
  const agui::ElementImageSet greenHovered  = Set(this->composition(102, 17, 8), dirt, this->defaultGlow(GREEN_GLOW));
  const agui::ElementImageSet greenClicked  = Set(this->composition(119, 17, 8), dirt);
  const agui::ElementImageSet greenDisabled = Set(this->composition(85, 25, 8), dirt);
  this->continueButton.setDefaultGraphicalSet(&green);
  this->continueButton.setHoveredGraphicalSet(&greenHovered);
  this->continueButton.setClickedGraphicalSet(&greenClicked);
  this->continueButton.setDisabledGraphicalSet(&greenDisabled);
  // tool_button_green: the same green on a square tool button.
  this->greenToolButton.setDefaultGraphicalSet(&green);
  this->greenToolButton.setHoveredGraphicalSet(&greenHovered);
  this->greenToolButton.setClickedGraphicalSet(&greenClicked);
  this->greenToolButton.setDisabledGraphicalSet(&greenDisabled);
  // tool_button_red, from red_button.
  const agui::ElementImageSet red         = Set(this->composition(136, 17, 8), dirt);
  const agui::ElementImageSet redHovered  = Set(this->composition(170, 17, 8), dirt, this->defaultGlow(RED_GLOW));
  const agui::ElementImageSet redClicked  = Set(this->composition(187, 17, 8), dirt);
  const agui::ElementImageSet redDisabled = Set(this->composition(153, 17, 8), dirt);
  this->redToolButton.setDefaultGraphicalSet(&red);
  this->redToolButton.setHoveredGraphicalSet(&redHovered);
  this->redToolButton.setClickedGraphicalSet(&redClicked);
  this->redToolButton.setDisabledGraphicalSet(&redDisabled);
  this->redToolButton.setPaddings(2, 2, 2, 2);  // size = 28
  for (auto set : { &agui::Style::setMinimalWidth, &agui::Style::setMaximalWidth,
                    &agui::Style::setMinimalHeight, &agui::Style::setMaximalHeight })
      (this->redToolButton.*set)(28);

  // back_button (via dialog_button): a grey button whose left end is an arrow.
  // Everything in a row of dialog buttons is this tall, the ridged strip between
  // them included.
  constexpr int16_t DIALOG_BUTTON_H = 32;
  this->backButton.setFont(&this->bigFont);
  for (auto set : { &agui::ButtonStyle::setDefaultFontColor, &agui::ButtonStyle::setHoveredFontColor,
                    &agui::ButtonStyle::setClickedFontColor })
      (this->backButton.*set)(BOLD_BLACK);
  this->backButton.setDisabledFontColor(GREY);
  this->backButton.setBottomPadding(2);
  this->backButton.setMinimalHeight(DIALOG_BUTTON_H);
  this->backButton.setMaximalHeight(DIALOG_BUTTON_H);
  this->backButton.setMinimalWidth(112);
  this->backButton.setHorizontalAlign(agui::HorizontalAlign::Left);
  this->backButton.setLeftPadding(16);  // clear of the arrow point
  const agui::ElementImageSet back         = Set(this->arrowBack(GREY_ARROWS, 0), this->backButtonGlow(DIRT));
  const agui::ElementImageSet backHovered  = Set(this->arrowBack(GREY_ARROWS, 2), {}, this->backButtonGlow(GLOW));
  const agui::ElementImageSet backClicked  = Set(this->arrowBack(GREY_ARROWS, 3));
  const agui::ElementImageSet backDisabled = Set(this->arrowBack(GREY_ARROWS, 1), {}, this->backButtonGlow(DIRT));
  this->backButton.setDefaultGraphicalSet(&back);
  this->backButton.setHoveredGraphicalSet(&backHovered);
  this->backButton.setClickedGraphicalSet(&backClicked);
  this->backButton.setDisabledGraphicalSet(&backDisabled);

  // The same arrow in red, for leaving a game rather than backing out of a page.
  const agui::ElementImageSet redBack     = Set(this->arrowBack(RED_ARROWS, 0), this->backButtonGlow(DIRT));
  const agui::ElementImageSet redBackHovered  = Set(this->arrowBack(RED_ARROWS, 2), {}, this->backButtonGlow(GLOW));
  const agui::ElementImageSet redBackClicked  = Set(this->arrowBack(RED_ARROWS, 3));
  const agui::ElementImageSet redBackDisabled = Set(this->arrowBack(RED_ARROWS, 1), {}, this->backButtonGlow(DIRT));
  this->redBackButton.setDefaultGraphicalSet(&redBack);
  this->redBackButton.setHoveredGraphicalSet(&redBackHovered);
  this->redBackButton.setClickedGraphicalSet(&redBackClicked);
  this->redBackButton.setDisabledGraphicalSet(&redBackDisabled);

  // arrow_forward(green_arrow_tileset): the point is on the right, so the text
  // is padded away from that end instead of the other one.
  const agui::ElementImageSet fwd         = Set(this->arrowForward(GREEN_ARROWS, 0), this->forwardButtonGlow(DIRT));
  const agui::ElementImageSet fwdHovered  = Set(this->arrowForward(GREEN_ARROWS, 2), {}, this->forwardButtonGlow(GREEN_GLOW));
  const agui::ElementImageSet fwdClicked  = Set(this->arrowForward(GREEN_ARROWS, 3));
  const agui::ElementImageSet fwdDisabled = Set(this->arrowForward(GREEN_ARROWS, 1), {}, this->forwardButtonGlow(DIRT));
  this->forwardButton.setDefaultGraphicalSet(&fwd);
  this->forwardButton.setHoveredGraphicalSet(&fwdHovered);
  this->forwardButton.setClickedGraphicalSet(&fwdClicked);
  this->forwardButton.setDisabledGraphicalSet(&fwdDisabled);
  this->forwardButton.setHorizontalAlign(agui::HorizontalAlign::Right);
  this->forwardButton.setLeftPadding(0);
  this->forwardButton.setRightPadding(16);  // clear of the arrow point

  // draggable_space: the strip between a row of dialog buttons. As tall as the
  // buttons beside it, and as wide as the gap it is given.
  agui::ElementImageSet space = Set(this->draggableRidges(), this->defaultGlow(DIRT_FILLER));
  this->draggableSpace.setParent(&agui::EmptyWidget::defaultStyle);
  this->draggableSpace.initGraphicalSet(&space);
  this->draggableSpace.setHorizontallyStretchable(true);
  this->draggableSpace.setMinimalHeight(DIALOG_BUTTON_H);
  this->draggableSpace.setMaximalHeight(DIALOG_BUTTON_H);

  // The main menu window has a title but, unlike other windows, no drag handle.
  this->menuFrame.setUseHeaderFiller(false);
}

// The Go board: wood, a grid of thin dark widgets over it, and a see-through
// button over each point. All the point styles share pointBase, which is
// where their size is set as the window changes.
void Theme::themeBoard()
{
  const agui::Color WOOD = Rgb(220, 179, 105);
  const agui::Color INK  = Rgb(44, 30, 12);

  // Flat colour off the derived sheet's solid block, tinted. Built by hand
  // because layer() cuts from the atlas, and the block is not in it.
  const auto solid = [this](const agui::Color& tint) {
    Layer l;
    l.type     = Layer::Type::Composition;
    l.drawType = Layer::DrawType::Inner;
    l.opacity  = 1.0f;
    l.center   = this->picture(this->derived, SOLID_INNER, tint);
    return Set(l);
  };

  agui::ElementImageSet wood = solid(WOOD);
  this->boardWood.setParent(&agui::EmptyWidget::defaultStyle);
  this->boardWood.initGraphicalSet(&wood);
  agui::ElementImageSet ink = solid(INK);
  this->gridLine.setParent(&agui::EmptyWidget::defaultStyle);
  this->gridLine.initGraphicalSet(&ink);

  // A point is a button with nothing of its own to draw: not the grey
  // button, not the orange glow under the cursor -- the stone that would be
  // played there is what shows the cursor.
  this->pointBase.setParent(&agui::Button::defaultStyle);
  this->pointBase.setPaddings(0, 0, 0, 0);
  this->pointBase.setClickedVerticalOffset(0);
  this->pointBase.setHorizontallyStretchable(false);
  this->pointBase.setVerticallyStretchable(false);
  this->pointBase.setHorizontallySquashable(false);
  this->pointBase.setVerticallySquashable(false);
  const auto allSets = { &agui::ButtonStyle::setDefaultGraphicalSet, &agui::ButtonStyle::setHoveredGraphicalSet,
                         &agui::ButtonStyle::setClickedGraphicalSet, &agui::ButtonStyle::setDisabledGraphicalSet,
                         &agui::ButtonStyle::setSelectedGraphicalSet, &agui::ButtonStyle::setSelectedHoveredGraphicalSet,
                         &agui::ButtonStyle::setSelectedClickedGraphicalSet };
  for (auto set : allSets) (this->pointBase.*set)(&this->none);

  // Labels on the points, dark on wood and white stones, light on black.
  const auto labelColor = [](agui::LabelStyle& label, const agui::Color& color) {
    label.setFontColor(color);
    label.setHoveredFontColor(color);
    label.setClickedFontColor(color);
    label.setDisabledFontColor(color);
    label.setParentHoveredColor(color);  // a label in a hovered button would otherwise go black
  };
  this->pointDark.setHorizontalAlign(agui::HorizontalAlign::Center);
  labelColor(this->pointDark, Rgb(16, 16, 16));
  labelColor(this->pointLight, Rgb(246, 246, 246));
  labelColor(this->pointDarkSmall, Rgb(16, 16, 16));
  labelColor(this->pointLightSmall, Rgb(246, 246, 246));

  this->coordinate.setFont(&this->smallFont);
  this->coordinate.setHorizontalAlign(agui::HorizontalAlign::Center);
  labelColor(this->coordinate, Rgb(84, 58, 24));

  // The game tree's nodes: see-through, lit orange under the cursor, and the
  // current one lit up whatever the cursor does.
  const agui::ElementImageSet lit     = Set(this->composition(34, 17, 8));
  const agui::ElementImageSet current = Set(this->composition(225, 17, 8));
  this->treeNode.setPaddings(0, 0, 0, 0);
  this->treeNode.setMinimalWidth(TREE_NODE_PX);
  this->treeNode.setMaximalWidth(TREE_NODE_PX);
  this->treeNode.setMinimalHeight(TREE_NODE_PX);
  this->treeNode.setMaximalHeight(TREE_NODE_PX);
  this->treeNode.setClickedVerticalOffset(0);
  for (auto set : allSets) (this->treeNode.*set)(&this->none);
  this->treeNode.setHoveredGraphicalSet(&lit);
  this->treeNode.setClickedGraphicalSet(&lit);
  for (auto set : allSets) (this->treeNodeCurrent.*set)(&current);
  // Selected: the pale selected button tinted blue, apart from the oranges of
  // the cursor and the current node.
  const agui::ElementImageSet selected        = Set(this->layer(Expand(352, 17, 8), { .tint = Rgb(120, 170, 255) }));
  const agui::ElementImageSet selectedLit     = Set(this->layer(Expand(352, 17, 8), { .tint = Rgb(160, 200, 255) }));
  const agui::ElementImageSet selectedCurrent = Set(this->layer(Expand(352, 17, 8), { .tint = Rgb(60, 110, 230) }));
  for (auto set : allSets) (this->treeNodeSelected.*set)(&selected);
  this->treeNodeSelected.setHoveredGraphicalSet(&selectedLit);
  this->treeNodeSelected.setClickedGraphicalSet(&selectedLit);
  for (auto set : allSets) (this->treeNodeSelectedCurrent.*set)(&selectedCurrent);
  // The branch's + and -: a button, as small as one goes, on the line.
  this->treeBranchButton.setPaddings(0, 0, 0, 0);
  this->treeBranchButton.setFont(&this->smallFont);
  this->treeBranchButton.setClickedVerticalOffset(0);

  // The numbers on the tree's stones: the board's colours, at one size that
  // fits three digits on a stone.
  for (agui::LabelStyle* number : { &this->treeNumberDark, &this->treeNumberLight }) {
    number->setFont(this->pointFont(LineHeight(11)));
    number->setHorizontalAlign(agui::HorizontalAlign::Center);
    number->setVerticalAlign(agui::VerticalAlign::Center);
  }

  // tool_button: square, sunk while its tool is the one in use.
  this->toolButton.setPaddings(0, 0, 0, 0);
  this->toolButton.setMinimalWidth(TOOL_PX);
  this->toolButton.setMaximalWidth(TOOL_PX);
  this->toolButton.setMinimalHeight(TOOL_PX);
  this->toolButton.setMaximalHeight(TOOL_PX);
  this->toolButton.setHorizontallyStretchable(false);

  this->smallButton.setPaddings(0, 8, 0, 8);
  this->smallButton.setMinimalWidth(36);
  this->smallButton.setMinimalHeight(28);
  this->smallButton.setMaximalHeight(28);
  this->smallButton.setHorizontallyStretchable(false);

  // bordered_frame: nothing but the ridged outline (set with the other lines,
  // in themeBasics) round a group of settings.
  this->borderedFrame.setGraphicalSet(&this->none);
  this->borderedFrame.setUseHeaderFiller(false);
  this->borderedFrame.setPaddings(4, 8, 8, 8);  // frame's
  this->borderedFrame.setHorizontallyStretchable(true);
  Under(this->borderedFrame.initVerticalFlowStyle(), &agui::VerticalFlow::defaultStyle)->setVerticalSpacing(4);

  // player_input_horizontal_flow: a setting's name, a pusher and its control.
  this->playerInputFlow.setHorizontalSpacing(8);
  this->playerInputFlow.setVerticalAlign(agui::VerticalAlign::Center);
  this->playerInputFlow.setMinimalHeight(28);
  this->playerInputFlow.setHorizontallyStretchable(true);

  this->frameSubheadingLabel.setTopPadding(4);  // frame_subheading_label

  // slider_value_textfield, as wide as other_settings_gui_textbox makes it.
  this->sliderValueField.setMinimalWidth(120);
  this->sliderValueField.setMaximalWidth(120);
  this->sliderValueField.setHorizontalAlign(agui::HorizontalAlign::Center);

  // frame_action_button, from frame_button: a 24 square button in a window's
  // title bar, dark until hovered, its white picture black while hovered
  // or down.
  const Layer frameShadow = this->layer(Expand(440, 24, 8), { .outer = true });
  const agui::ElementImageSet action         = Set(this->composition(0, 0, 8), frameShadow);
  const agui::ElementImageSet actionHovered  = Set(this->composition(34, 17, 8), frameShadow, this->defaultGlow(GLOW));
  const agui::ElementImageSet actionClicked  = Set(this->composition(51, 17, 8), frameShadow);
  const agui::ElementImageSet actionDisabled = Set(this->composition(17, 17, 8), frameShadow);
  const agui::ElementImageSet actionSelected = Set(this->composition(369, 17, 8), frameShadow);
  const agui::ElementImageSet actionSelectedHovered = Set(this->composition(352, 17, 8), frameShadow);
  this->frameActionButton.setDefaultGraphicalSet(&action);
  this->frameActionButton.setHoveredGraphicalSet(&actionHovered);
  this->frameActionButton.setClickedGraphicalSet(&actionClicked);
  this->frameActionButton.setDisabledGraphicalSet(&actionDisabled);
  this->frameActionButton.setSelectedGraphicalSet(&actionSelected);
  this->frameActionButton.setSelectedHoveredGraphicalSet(&actionSelectedHovered);
  this->frameActionButton.setSelectedClickedGraphicalSet(&actionSelected);
  this->frameActionButton.setPaddings(0, 0, 0, 0);
  for (auto set : { &agui::Style::setMinimalWidth, &agui::Style::setMaximalWidth,
                    &agui::Style::setMinimalHeight, &agui::Style::setMaximalHeight })
      (this->frameActionButton.*set)(24);
  this->frameActionIcon.setInvertColorsOfPictureWhenHoveredOrToggled(true);

  // search_popup_frame: the text field's strip, the colour of the title bar,
  // over the title just left of the button.
  const agui::ElementImageSet popup = Set(this->monolith({ 8, 8, 1, 1 }));
  this->searchPopupFrame.setGraphicalSet(&popup);
  this->searchPopupFrame.setUseHeaderFiller(false);
  this->searchPopupFrame.setPaddings(0, 4, 0, 4);
  this->searchPopupFrame.setRightMargin(4);
  Under(this->searchPopupFrame.initHorizontalFlowStyle(), &agui::HorizontalFlow::defaultStyle)
      ->setVerticalAlign(agui::VerticalAlign::Center);
  this->searchPopupField.setMinimalWidth(104);  // search_popup_textfield
  this->searchPopupField.setMaximalWidth(104);

  // What search looks at, as style.lua has it: the names of things --
  // labels, check boxes, radio buttons -- but not the controls beside them,
  // nor captions, which name a group rather than a setting.
  for (agui::Style* ignored : std::initializer_list<agui::Style*>{
           &agui::Button::defaultStyle, &agui::DropDown::defaultStyle, &agui::TextBox::defaultStyle,
           &agui::TextField::defaultStyle, &agui::Slider::defaultStyle, &agui::EmptyWidget::defaultStyle,
           &agui::ImageWidget::defaultStyle, &this->captionLabel })
      ignored->setIgnoredBySearch(true);
  this->subheaderFrame.setNeverHiddenBySearch(true);

  // scroll_pane_under_subheader: no frame of its own, the panel it is in is
  // the frame.
  this->scrollPaneUnderSubheader.initGraphicalSet(&this->none);
  this->scrollPaneUnderSubheader.setExtraPaddingWhenActivated(0);
  this->scrollPaneUnderSubheader.setPadding(4);

  // shallow_frame: a lighter panel, here a section of the Controls page.
  const agui::ElementImageSet shallow = Set(this->composition(68, 0, 8));
  this->shallowFrame.setGraphicalSet(&shallow);
  this->shallowFrame.setUseHeaderFiller(false);
  this->shallowFrame.setPadding(4);
  this->shallowFrame.setHorizontallyStretchable(true);
  Under(this->shallowFrame.initVerticalFlowStyle(), &agui::VerticalFlow::defaultStyle);
  Under(this->shallowFrame.initHorizontalFlowStyle(), &agui::HorizontalFlow::defaultStyle)
      ->setVerticalAlign(agui::VerticalAlign::Center);

  // control_settings_bordered_table (its lines are set in themeBasics): as
  // wide as the section, its edges over the section's padding.
  this->controlTable.setCellPadding(4);
  this->controlTable.setLeftCellPadding(8);
  this->controlTable.setHorizontalSpacing(0);
  this->controlTable.setVerticalSpacing(0);
  this->controlTable.setLeftMargin(-4);
  this->controlTable.setRightMargin(-4);
  this->controlTable.setBottomMargin(-4);
  this->controlTable.setTopMargin(4);
  this->controlTable.setHorizontallyStretchable(true);

  // control_settings_button, from rounded_button: lighter than a button, and
  // down (selected) while it waits for the keys.
  const Layer roundedDirt = this->roundedButtonGlow(DIRT);
  const agui::ElementImageSet rounded         = Set(this->composition(168, 200, 8), roundedDirt);
  const agui::ElementImageSet roundedHovered  = Set(this->composition(202, 200, 8), roundedDirt, this->roundedButtonGlow(GLOW));
  const agui::ElementImageSet roundedClicked  = Set(this->composition(219, 200, 8), roundedDirt);
  const agui::ElementImageSet roundedDisabled = Set(this->composition(185, 200, 8), roundedDirt);
  const agui::ElementImageSet roundedSelected = Set(this->composition(236, 200, 8), roundedDirt);
  this->controlButton.setDefaultGraphicalSet(&rounded);
  this->controlButton.setHoveredGraphicalSet(&roundedHovered);
  this->controlButton.setClickedGraphicalSet(&roundedClicked);
  this->controlButton.setDisabledGraphicalSet(&roundedDisabled);
  this->controlButton.setSelectedGraphicalSet(&roundedSelected);
  this->controlButton.setSelectedHoveredGraphicalSet(&roundedSelected);
  this->controlButton.setSelectedClickedGraphicalSet(&roundedSelected);
  this->controlButton.setHorizontalAlign(agui::HorizontalAlign::Left);
  this->controlButton.setMinimalWidth(224);  // 225 in style.lua; kept to whole modules
  this->controlButton.setMaximalWidth(224);
  this->controlButton.setHorizontallyStretchable(false);
  // Factorio's colour for keys it can't make sense of, here for keys that
  // are another control's too.
  const agui::Color conflict = Rgb(204, 0, 0);
  for (auto set : { &agui::ButtonStyle::setDefaultFontColor, &agui::ButtonStyle::setHoveredFontColor,
                    &agui::ButtonStyle::setClickedFontColor, &agui::ButtonStyle::setSelectedFontColor,
                    &agui::ButtonStyle::setSelectedHoveredFontColor, &agui::ButtonStyle::setSelectedClickedFontColor })
      (this->controlConflictButton.*set)(conflict);

  // The sheet over the editor while a page is up.
  agui::ElementImageSet dim = solid(DIM);
  this->dimmer.setParent(&agui::EmptyWidget::defaultStyle);  // for the base properties
  this->dimmer.initGraphicalSet(&dim);
}

void Theme::setPointSize(int px)
{
  if (px == this->pointPx) return;
  this->pointPx = px;

  this->pointBase.setMinimalWidth(px);
  this->pointBase.setMaximalWidth(px);
  this->pointBase.setMinimalHeight(px);
  this->pointBase.setMaximalHeight(px);

  // Up to two characters fill the point; three need to be smaller to fit
  // inside a stone. (The sizes are whole lines, which Titillium's digits fill
  // only about two thirds of.)
  const agui::Font* big   = this->pointFont(std::max(8, px * 7 / 10));
  const agui::Font* small = this->pointFont(std::max(8, px * 1 / 2));
  this->pointDark.setFont(big);
  this->pointLight.setFont(big);
  this->pointDarkSmall.setFont(small);
  this->pointLightSmall.setFont(small);

  this->coordinate.setFont(this->pointFont(std::clamp(px / 2, 13, 23)));
}

const agui::Font* Theme::pointFont(int px)
{
  std::unique_ptr<agui_raylib::RaylibFont>& font = this->pointFonts[px];
  if (!font) font = std::make_unique<agui_raylib::RaylibFont>(this->raster(px, Weight::Bold), px, FontSpacing(px));
  return font.get();
}
void Theme::setScale(float scale)
{
  if (scale == this->viewScale) return;
  this->viewScale = scale;

  const std::pair<agui_raylib::RaylibFont*, Weight> fonts[] = {
    { &this->smallFont, Weight::Regular }, { &this->bodyFont, Weight::Regular }, { &this->semiboldFont, Weight::SemiBold },
    { &this->boldFont, Weight::Bold },     { &this->headingFont, Weight::Bold }, { &this->bigFont, Weight::Bold },
  };
  for (const auto& [font, weight] : fonts) font->setRaster(this->raster(int(font->size()), weight));
  for (const auto& [px, font] : this->pointFonts) font->setRaster(this->raster(px, Weight::Bold));
}

const ::Font& Theme::raster(int px, Weight weight) const
{
  return FontAt(RasterPx(px, this->viewScale), weight);
}

}  // namespace ui
