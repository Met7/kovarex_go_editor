#include <ui/NewGamePage.hpp>

#include <game/Game.hpp>
#include <ui/Form.hpp>
#include <ui/Theme.hpp>

#include <Agui/Widget/Button.hpp>
#include <Agui/Widget/DropDown.hpp>
#include <Agui/Widget/Frame.hpp>
#include <Agui/Widget/ImageWidget.hpp>
#include <Agui/Widget/Label.hpp>
#include <Agui/Widget/Slider.hpp>
#include <Agui/Widget/TextField.hpp>

#include <cmath>
#include <string>
#include <vector>

namespace ui {

namespace {

constexpr int SLIDER_W = 240;
constexpr int FIELD_W  = 240;
constexpr int VALUE_W  = 40;
// As wide as namedRow() makes a name.
constexpr int NAME_W = 150;
// The info mark after a caption with a tooltip: Factorio's size for it.
constexpr int INFO_W = 8;
constexpr int INFO_H = 20;

// The sizes people play on; anything else is Custom, from the sliders.
constexpr int SIZES[] = { 9, 13, 19 };
constexpr int CUSTOM  = int(std::size(SIZES));

agui::Slider& NumberSlider(int min, int max)
{
  agui::Slider& slider = make<agui::Slider>();
  slider.setMinMaxValues(min, max);
  slider.setValueStep(1);
  slider.style.setMinimalWidth(SLIDER_W);
  return slider;
}

agui::Label& ValueLabel()
{
  agui::Label& label = agui::label("");
  label.style.setMinimalWidth(VALUE_W);
  return label;
}

agui::TextField& Field()
{
  agui::TextField& field = make<agui::TextField>();
  field.style.setMinimalWidth(FIELD_W);
  field.style.setMaximalWidth(FIELD_W);
  return field;
}

int SliderValue(const agui::Slider& slider)
{
  return int(std::lround(slider.getValue()));
}

}  // namespace

NewGamePage::NewGamePage(Theme& theme, GameSetup& setup, std::function<void()> onStart, std::function<void()> onBack)
    : setup(setup)
    , window(agui::GuiDirection::Vertical, "New game")
{
  this->window.setDragTarget(&this->window);

  agui::VerticalFlow& content = column();
  content << agui::label("Board", &theme.headingLabel);

  this->size = &make<agui::DropDown>();
  for (int s : SIZES) this->size->addItem(std::to_string(s) + " x " + std::to_string(s));
  this->size->addItem(std::string("Custom"));
  this->size->onItemSelect(this, [this](int i) { this->applyPreset(i); });
  content << namedRow("Size", *this->size);

  this->width  = &NumberSlider(2, MAX_BOARD);
  this->height = &NumberSlider(2, MAX_BOARD);
  this->widthValue  = &ValueLabel();
  this->heightValue = &ValueLabel();
  this->width->onSliderMove(this, [this](double) { this->applySliders(); });
  this->height->onSliderMove(this, [this](double) { this->applySliders(); });
  content << (namedRow("Width", *this->width) << *this->widthValue);
  content << (namedRow("Height", *this->height) << *this->heightValue);

  content << agui::label("Game", &theme.headingLabel);

  // A notch for each: none, 1 -- no stones, only komi -- and 2 to 9 stones.
  this->handicap = &make<agui::Slider>(&theme.notchedSlider);
  this->handicap->setMinMaxValues(0, 9);
  this->handicap->setValueStep(1);
  this->handicap->setDiscreteSlider();
  this->handicap->style.setMinimalWidth(SLIDER_W);
  this->handicapValue = &ValueLabel();
  // As wide as the widest thing it can say, from the start, so the page
  // keeps its size as the slider moves.
  {
    std::vector<std::string> says = { "none", "1 (no stones)" };
    for (int i = 2; i <= 9; ++i) {
      says.push_back(std::to_string(i));
      says.push_back(std::to_string(i) + " (by hand)");
    }
    const std::string* widest = &says.front();
    for (const std::string& text : says) {
      if (agui::Label::getWidthToFit(&this->handicapValue->style, text) >
          agui::Label::getWidthToFit(&this->handicapValue->style, *widest)) {
        widest = &text;
      }
    }
    this->handicapValue->setSizeToFit(*widest);
  }
  this->handicap->onSliderMove(this, [this](double) {
    const int before     = this->setup.handicap;
    this->setup.handicap = SliderValue(*this->handicap);
    // Komi follows the handicap, unless it was set by hand to something else.
    if (before < 1 && this->setup.handicap >= 1 && (this->setup.komi == "6.5" || this->setup.komi == "7.5")) this->setup.komi = "0.5";
    if (before >= 1 && this->setup.handicap < 1 && this->setup.komi == "0.5") this->setup.komi = "6.5";
    this->refresh();
  });
  // The tooltip on the caption, with Factorio's info mark after it.
  const char* const handicapTip =
      "Black's stones set out before the game starts. White then plays first.\n1 is no stones: Black plays first, "
      "with komi only 0.5.\nBy hand: the board has no usual points for that many, so place the stones yourself.";
  agui::Label&       caption     = agui::label("Handicap");
  caption.setToolTip(handicapTip);
  agui::ImageWidget& info = make<agui::ImageWidget>(theme.infoIcon());
  info.style.setMinimalWidth(INFO_W);
  info.style.setMaximalWidth(INFO_W);
  info.style.setMinimalHeight(INFO_H);
  info.style.setMaximalHeight(INFO_H);
  info.setToolTip(handicapTip);
  agui::HorizontalFlow& name = row(4);
  name.style.setMinimalWidth(NAME_W);
  name << caption << info;
  content << (row() << name << *this->handicap << *this->handicapValue);

  this->komi = &Field();
  this->komi->onTextEdit(this, [this] { this->setup.komi = this->komi->getText(); });
  content << namedRow("Komi", *this->komi);

  this->black = &Field();
  this->black->onTextEdit(this, [this] { this->setup.black = this->black->getText(); });
  content << namedRow("Black", *this->black);

  this->white = &Field();
  this->white->onTextEdit(this, [this] { this->setup.white = this->white->getText(); });
  content << namedRow("White", *this->white);

  // Enter in a field is Start, as in any dialog.
  for (agui::TextField* field : { this->komi, this->black, this->white }) field->onConfirm(this, onStart);

  agui::Frame& panel = make<agui::Frame>(agui::GuiDirection::Vertical, &theme.insideShallowFrameWithPadding);
  panel << content;
  this->window << panel;

  // dialog_buttons_horizontal_flow
  agui::HorizontalFlow& footer = row(8);
  footer.style.setTopPadding(8);
  footer.style.setHorizontallyStretchable(true);
  footer << agui::button("Back", &this->window, std::move(onBack), &theme.backButton);
  footer << dragHandle(&theme.draggableSpace, &this->window);
  footer << footerButton("Start", &this->window, std::move(onStart), &theme.forwardButton, 160);
  this->window << footer;

  this->refresh();
}

void NewGamePage::applyPreset(int index)
{
  if (index >= 0 && index < CUSTOM) this->setup.width = this->setup.height = SIZES[index];
  this->refresh();
}

void NewGamePage::applySliders()
{
  this->setup.width  = SliderValue(*this->width);
  this->setup.height = SliderValue(*this->height);
  this->refresh();
}

void NewGamePage::refresh()
{
  // Set without dispatching, so this doesn't come round again.
  this->width->setValue(this->setup.width);
  this->height->setValue(this->setup.height);
  this->handicap->setValue(this->setup.handicap);
  this->widthValue->setText(std::to_string(this->setup.width));
  this->heightValue->setText(std::to_string(this->setup.height));

  const bool placed = this->setup.handicap < 2 ||
                      !HandicapPoints(this->setup.handicap, this->setup.width, this->setup.height).empty();
  this->handicapValue->setText(this->setup.handicap == 0   ? std::string("none")
                               : this->setup.handicap == 1 ? std::string("1 (no stones)")
                               : placed                    ? std::to_string(this->setup.handicap)
                                                           : std::to_string(this->setup.handicap) + " (by hand)");

  int preset = CUSTOM;
  for (int i = 0; i < CUSTOM; ++i) {
    if (this->setup.width == SIZES[i] && this->setup.height == SIZES[i]) preset = i;
  }
  this->size->setSelectedIndex(preset);

  if (this->komi->getText() != this->setup.komi) this->komi->setText(this->setup.komi);
  if (this->black->getText() != this->setup.black) this->black->setText(this->setup.black);
  if (this->white->getText() != this->setup.white) this->white->setText(this->setup.white);
}

}  // namespace ui
