// Agui ships with empty default styles -- every widget type reads its fonts,
// colours and 9-slice backgrounds from a root `defaultStyle` that the host
// program is expected to fill. Theme does that for every widget in the
// library and keeps the fonts and images those styles point at alive.
//
// The look is Factorio's: resources/gui.png is Factorio's gui-new.png (compiled
// into the exe, see fastbuild/fbuild.bff), and the regions, paddings and colours
// come from its style prototype (resources/style.lua, next to it).
// Styles are named after their style.lua counterparts, so that file is the
// place to look up what a number means or to port another style.

#pragma once

#include <ui/AguiRaylib.hpp>
#include <ui/Fonts.hpp>

#include <Agui/ElementImageSet.hpp>
#include <Agui/Widget/ButtonStyle.hpp>
#include <Agui/Widget/EmptyWidgetStyle.hpp>
#include <Agui/Widget/FrameStyle.hpp>
#include <Agui/Widget/HorizontalFlowStyle.hpp>
#include <Agui/Widget/ImageStyle.hpp>
#include <Agui/Widget/LabelStyle.hpp>
#include <Agui/Widget/ScrollPaneStyle.hpp>
#include <Agui/Widget/SliderStyle.hpp>
#include <Agui/Widget/TableStyle.hpp>
#include <Agui/Widget/TextBoxStyle.hpp>

#include <map>
#include <memory>
#include <vector>

namespace ui {

class Theme {
public:
  // Needs the window. `scale` is the interface scale the GUI will be drawn
  // at, which the fonts are rasterised for (see setScale()).
  explicit Theme(float scale);
  Theme(const Theme&) = delete;
  Theme& operator=(const Theme&) = delete;

  // Named styles, parented to the defaults.
  agui::LabelStyle  dimLabel;            // grey_label
  agui::LabelStyle  headingLabel;        // heading_2_label
  agui::LabelStyle  captionLabel;        // caption_label: the bold heading inside a bordered_frame
  agui::LabelStyle  linkLabel;           // hyperlink_label: a web address, blue and underlined
  agui::LabelStyle  versionLabel;        // main_menu_version_label
  agui::LabelStyle  goodLabel;           // green: something worked
  agui::LabelStyle  badLabel;            // red: something was refused
  agui::ButtonStyle menuButton;          // menu_button
  agui::ButtonStyle continueButton;      // menu_button_continue: the green one
  agui::ButtonStyle backButton;          // back_button: grey, arrow pointing left
  agui::ButtonStyle redBackButton;       // the same in red_arrow_tileset: leaving, not going back
  agui::ButtonStyle forwardButton;       // green, arrow pointing right: the button that gets on with it
  agui::FrameStyle  menuFrame;           // frame, without the drag handle
  agui::FrameStyle  insideShallowFrame;  // inside_shallow_frame
  agui::FrameStyle  insideShallowFrameWithPadding;  // inside_shallow_frame_with_padding
  agui::FrameStyle  insideDeepFrame;     // inside_deep_frame
  agui::FrameStyle  subheaderFrame;      // subheader_frame: the lighter strip across the top of a deep frame
  agui::FrameStyle  borderedFrame;       // bordered_frame: a ridged outline round a group of settings

  // The Go board. The board is a wooden panel with the grid laid on it as
  // thin widgets, and a button over each point; what is on a point -- a
  // stone, a mark, a label -- is pictures and text the button carries, not
  // part of its style.
  agui::EmptyWidgetStyle boardWood;    // the board itself
  agui::EmptyWidgetStyle gridLine;     // one line of the grid
  agui::ButtonStyle      pointPlain;   // a point: see-through, the lines and stones show
  agui::LabelStyle       pointDark;    // a label or move number on wood or on a white stone
  agui::LabelStyle       pointLight;   // ...and on a black stone
  agui::LabelStyle       pointDarkSmall;   // the same for three digits, which need a smaller font to fit
  agui::LabelStyle       pointLightSmall;
  agui::LabelStyle       coordinate;   // the letters and numbers round the edge

  // The game tree: a button per node, the current one lit up. A move played
  // on a stone already there is red, lit or not; while it is the current
  // node, the move or set-up whose stone it lands on is deep orange.
  agui::ButtonStyle treeNode;
  agui::ButtonStyle treeNodeCurrent;
  // Selected moves: blue, darker on the current one.
  agui::ButtonStyle treeNodeSelected;
  agui::ButtonStyle treeNodeSelectedCurrent;
  // The small + and - where a branch leaves the selection: takes it in, or
  // leaves it out again.
  agui::ButtonStyle treeBranchButton;
  agui::ButtonStyle treeNodeOnStone;
  agui::ButtonStyle treeNodeOnStoneCurrent;
  agui::ButtonStyle treeNodeUnder;
  // A move's number on its stone in the tree: dark on white, light on black.
  agui::LabelStyle  treeNumberDark;
  agui::LabelStyle  treeNumberLight;

  // A square button in the tool bar, down while its tool is the one in use.
  agui::ButtonStyle toolButton;
  // The same in green, and in red like Factorio's reset-to-defaults button.
  agui::ButtonStyle greenToolButton;
  agui::ButtonStyle redToolButton;
  // The smaller buttons of the tool bar's other rows.
  agui::ButtonStyle smallButton;

  // draggable_space: the ridged strip that fills the gap in a row of dialog
  // buttons, and drags the window it is given as a drag target.
  agui::EmptyWidgetStyle draggableSpace;

  // The settings page, as Factorio's settings windows lay out: a row of a
  // setting's name and its control, the name over a slider, and the text
  // field beside the slider that shows its value and takes a typed one.
  agui::HorizontalFlowStyle playerInputFlow;       // player_input_horizontal_flow
  agui::LabelStyle          frameSubheadingLabel;  // frame_subheading_label
  agui::TextBoxStyle        sliderValueField;      // slider_value_textfield, other_settings_gui_textbox
  agui::SliderStyle         notchedSlider;         // notched_slider: a notch per value, a pointed knob

  // The search in a window's title bar: its button, the icon on it, and the
  // text field that opens beside it.
  agui::ButtonStyle  frameActionButton;  // frame_action_button
  agui::ImageStyle   frameActionIcon;    // its picture: white, black while hovered or down
  agui::FrameStyle   searchPopupFrame;   // search_popup_frame
  agui::TextBoxStyle searchPopupField;   // search_popup_textfield

  // The Controls page, as Factorio's control settings: a section per group
  // of controls, each a table of rows with the keys on two buttons.
  agui::ScrollPaneStyle scrollPaneUnderSubheader;  // scroll_pane_under_subheader, from naked_scroll_pane
  agui::FrameStyle  shallowFrame;          // shallow_frame
  agui::TableStyle  controlTable;          // control_settings_bordered_table
  agui::ButtonStyle controlButton;         // control_settings_button, from rounded_button
  agui::ButtonStyle controlConflictButton; // the same in red: the keys do something else too

  // The sheet that darkens the game behind the main menu.
  agui::EmptyWidgetStyle dimmer;

  // Resizes the point styles to `pointPx`, and picks label fonts to match.
  // Cheap to call with the size it already has.
  void setPointSize(int pointPx);

  // Sizes in whole modules of 4px, as all of Factorio's are, so every
  // standard interface scale keeps the same proportions.
  // How big the tree's nodes are, and so how far apart.
  static constexpr int TREE_NODE_PX = 24;
  // A tool button in the side panel: square.
  static constexpr int TOOL_PX = 32;

  // The interface scale the GUI is drawn at. Sizes stay as they are -- the
  // whole GUI is scaled when drawn -- but every font is rasterised again for
  // the screen pixels its text will cover, so it stays sharp.
  void setScale(float scale);

  // Factorio's utility sprites, at their size in the GUI: the info mark
  // that follows a name with a tooltip (8 x 20, as tall as a line of text,
  // the mark in its middle), and the reset button's arrow (16 x 16), dark,
  // or white for a disabled button.
  std::unique_ptr<agui::Image> infoIcon() const;
  std::unique_ptr<agui::Image> resetIcon(bool enabled) const;
  // The search button's magnifying glass, white.
  std::unique_ptr<agui::Image> searchIcon() const;
  // The editor's own icon, big, for the About page.
  std::unique_ptr<agui::Image> appIcon() const;

  // A plain (not 9-sliced) picture cut from the atlas, for ImageWidget.
  // Coordinates are atlas pixels; it shows at half that size, like the rest.
  std::unique_ptr<agui::Image> atlasImage(int x, int y, int w, int h) const;

  // An atlas region, in atlas pixels.
  struct Piece {
    int x, y, w, h;
  };
  // The pieces of a composition, as style.lua lists them. Any may be absent.
  struct Parts;
  // How a layer is drawn: style.lua's draw_type, tint, *_outer_border_shift
  // and tiling flags.
  struct Look;

private:
  using Layer = agui::ElementImageSet::Layer;

  agui_raylib::RaylibImage* picture(const Piece& piece, const agui::Color& tint);
  agui_raylib::RaylibImage* picture(const std::shared_ptr<Texture2D>& sheet, const Piece& piece,
                                    const agui::Color& tint);
  Layer layer(const Parts& parts, const Look& look);
  // `base` expanded from position + corner_size, the most common layer.
  Layer composition(int x, int y, int corner);
  Layer monolith(const Piece& piece);

  // style.lua's shared shadow and glow layers.
  Layer defaultGlow(const agui::Color& tint);
  Layer defaultInnerShadow();
  Layer topShadow();
  Layer bottomShadow();
  Layer roundedCornersGlow(const agui::Color& tint);
  Layer roundedButtonGlow(const agui::Color& tint);
  Layer tabGlow(const agui::Color& tint);
  Layer radiobuttonGlow(const agui::Color& tint);
  Layer slidingGlow(int x, const agui::Color& tint);  // left/right_slider_glow
  Layer backButtonGlow(const agui::Color& tint);
  Layer forwardButtonGlow(const agui::Color& tint);

  // style.lua's arrow tilesets: where the arrows are in the atlas, and which
  // button composition they sit against.
  struct ArrowTileset {
    int arrowsY;
    int compositionX;
  };
  static constexpr ArrowTileset GREY_ARROWS{ 232, 0 };
  static constexpr ArrowTileset GREEN_ARROWS{ 296, 68 };
  static constexpr ArrowTileset RED_ARROWS{ 360, 136 };

  // arrow_back / arrow_forward(tileset, index) without their glows. `index` is
  // style.lua's state order: 0 default, 1 disabled, 2 hovered, 3 clicked.
  Layer arrowBack(const ArrowTileset& tileset, int index);
  Layer arrowForward(const ArrowTileset& tileset, int index);

  // The ridges of style.lua's draggable_space, shared by a window's header
  // filler and the one in a row of dialog buttons.
  Layer draggableRidges();

  void themeBasics();
  void themeToggles();
  void themeSliders();
  void themeBars();
  void themeText();
  void themeContainers();
  void themeLists();
  void themeCharts();
  void themeMenus();
  void themeBoard();

  // A bold font `px` tall, rasterised on first ask and kept. The points go
  // through this rather than holding a font, because their size follows the
  // window.
  const agui::Font* pointFont(int px);

  std::shared_ptr<Texture2D> atlas;
  std::shared_ptr<Texture2D> info, reset, resetWhite, search;  // the utility sprites
  std::shared_ptr<Texture2D> appIconTexture;  // the exe's icon, for About
  // Pieces the atlas doesn't have (the drop-down arrow), drawn at load time.
  std::shared_ptr<Texture2D> derived;
  std::vector<std::unique_ptr<agui_raylib::RaylibImage>> images;

  agui_raylib::RaylibFont smallFont;    // default-small
  agui_raylib::RaylibFont bodyFont;     // default
  agui_raylib::RaylibFont semiboldFont; // default-semibold
  agui_raylib::RaylibFont boldFont;     // default-bold
  agui_raylib::RaylibFont headingFont;  // heading-2
  agui_raylib::RaylibFont bigFont;      // heading-1 / default-dialog-button

  // The point styles' shared parent: the one place their size is set.
  agui::ButtonStyle pointBase;
  std::map<int, std::unique_ptr<agui_raylib::RaylibFont>> pointFonts;
  int pointPx = 0;

  float viewScale = 1.0f;
  // A weight of the face, rasterised for text `px` tall at viewScale.
  const ::Font& raster(int px, Weight weight) const;

  // Sets that styles hold by pointer rather than copying.
  agui::ElementImageSet none;
  agui::ElementImageSet barBackground, bar;
};

}  // namespace ui
