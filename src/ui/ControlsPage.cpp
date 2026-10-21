#include <ui/ControlsPage.hpp>

#include <ui/Form.hpp>
#include <ui/SearchBar.hpp>
#include <ui/Theme.hpp>

#include <Agui/Gui.hpp>
#include <Agui/LowercaseString.hpp>
#include <Agui/MouseEvent.hpp>
#include <Agui/StringMatcher.hpp>
#include <Agui/Widget/Button.hpp>
#include <Agui/Widget/Frame.hpp>
#include <Agui/Widget/HorizontalFlow.hpp>
#include <Agui/Widget/ImageWidget.hpp>
#include <Agui/Widget/Label.hpp>
#include <Agui/Widget/Table.hpp>
#include <Agui/Widget/TextButton.hpp>
#include <Agui/Widget/VerticalScrollPane.hpp>

#include <algorithm>
#include <map>
#include <span>
#include <string>

namespace ui {

namespace {

// The info icon after a name that has a tooltip: as tall as a line of text.
constexpr int INFO_W = 8;
constexpr int INFO_H = 20;

// What the rest of the page takes of its height: the title bar, the
// subheader, the row of buttons along the bottom, and the paddings between.
constexpr int CHROME_H = 160;
// Less than this and the page runs off the screen rather than shrink.
constexpr int MIN_SCROLL_H = 160;

// locale: gui-control-settings.waiting and gui.instruction-to-clear-generic.
constexpr const char* WAITING      = "Waiting";
// Right-click in the style of keys, as __CONTROL_RIGHT_CLICK__ is.
const std::string HOW_TO_CLEAR = ShortcutText("Right-click") + " to clear.";

// What the mouse does on the board: not controls anyone can change, but
// listed with them, on buttons that can't be pressed, so that the page shows
// everything there is to do.
struct MouseAction {
  const char* name;
  const char* tip;
  const char* primary;
  const char* alternative;
};
constexpr MouseAction MOUSE[] = {
  { "Play a move", "Or step into it, if the record already has it.", "Click", nullptr },
  { "Insert a move", "After the current one; the rest of the game follows on after it.", "Ctrl + click", nullptr },
  { "Move a stone", "Changes the move that played it, however long ago that was.", "Drag a stone", nullptr },
  { "Go to the move that played a stone", nullptr, "Right-click a stone", "Shift + click a stone" },
  { "Draw on the board", "To show something; in a shared game, the others see it too. The Pen tool draws with a plain drag.",
    "Alt + drag", "Middle-drag" },
  { "Back and forward through the game", nullptr, "Mouse wheel", nullptr },
  { "Clear a point", "With a mark tool: takes off whatever mark is there.", "Right-click", nullptr },
  { "Marks/territory in an area", "With a mark tool, every stone in the box gets the mark; with a territory tool, every empty point.",
    "Shift + drag", nullptr },
  { "Clear marks in an area", "With a mark or territory tool: that mark off every point in the box. With a label tool, every label; "
    "with a set-up tool, every set-up stone.", "Ctrl + Shift + drag", nullptr },
  { "Show tooltips at once", "Whatever the tooltip delay in Settings says.", "Hold Shift", nullptr },
};
// And in the game tree: picking out moves for the Variation group's edits,
// and moving them.
constexpr MouseAction TREE[] = {
  { "Select moves", "From the current move to the one clicked, in place of what was selected before. Delete, Cut and Copy then act on just those.",
    "Shift + click a move", nullptr },
  { "Add to the selection", "The moves from the selection to the one clicked, additionally to what was selected before.", "Ctrl + click a move", nullptr },
  { "Move a subtree", "Move a whole variation or selection.", "Drag a stone", nullptr },
};

}  // namespace

ControlsPage::ControlsPage(Theme& theme, const Settings& live, std::function<void()> onConfirm,
                           std::function<void()> onBack)
    : live(live)
    , settings(live)
    , openedWith(live)
    , theme(theme)
    , window(agui::GuiDirection::Vertical, "Controls")
    , resettable(theme,
                 [this] {
                   this->stop();
                   this->settings.controls = Bindings();
                   this->refresh();
                 })
{
  this->window.setDragTarget(&this->window);

  this->scroll = &make<agui::VerticalScrollPane>(&theme.scrollPaneUnderSubheader);
  *this->scroll << this->section("Moving through the game", ControlSection::Moving);
  *this->scroll << this->section("Editing", ControlSection::Editing);
  *this->scroll << this->section("Tools", ControlSection::Tools);
  *this->scroll << this->section("Files", ControlSection::Files);
  *this->scroll << this->section("Interface", ControlSection::Interface);
  *this->scroll << this->mouseSection(false);
  *this->scroll << this->mouseSection(true);

  agui::HorizontalFlow& strip = row();
  strip.style.setHorizontallyStretchable(true);
  // The subheader, and the reset button at its right end.
  strip << agui::pusher << this->resettable.resetButton();
  agui::Frame& subheader = make<agui::Frame>(agui::GuiDirection::Horizontal, &theme.subheaderFrame);
  subheader << strip;

  agui::Frame& panel = make<agui::Frame>(agui::GuiDirection::Vertical, &theme.insideDeepFrame);
  panel << subheader << *this->scroll;
  this->window << panel;

  this->search = &make<SearchBar>(theme, this->window, [this](const std::string& text) {
    this->searched = text;
    this->filter();
  });
  this->search->keepSizeOf(panel);

  // Back throws the changes away, and while the mouse is on it lights up
  // what it would throw away; Confirm keeps them.
  agui::Button& back = agui::button("Back", &this->window,
                                    [this, onBack = std::move(onBack)] {
                                      this->stop();
                                      onBack();
                                    },
                                    &theme.backButton);
  this->resettable.lightWhileHovered(back, [this]() -> const Settings& { return this->openedWith; });
  agui::HorizontalFlow& footer = row(8);
  footer.style.setTopPadding(8);
  footer.style.setHorizontallyStretchable(true);
  footer << back;
  footer << dragHandle(&theme.draggableSpace, &this->window);
  footer << footerButton("Confirm", &this->window,
                         [this, onConfirm = std::move(onConfirm)] {
                           this->stop();
                           onConfirm();
                         },
                         &theme.forwardButton, 200);
  this->window << footer;

  this->refresh();
}

agui::Widget& ControlsPage::section(const char* caption, ControlSection which)
{
  // shallow_frame, its caption over a control_settings_bordered_table.
  agui::Frame& frame = make<agui::Frame>(agui::GuiDirection::Vertical, &this->theme.shallowFrame);
  this->found.push_back({ &frame, {} });
  agui::HorizontalFlow& header = make<agui::HorizontalFlow>(&this->theme.playerInputFlow);
  header.style.setLeftPadding(8);
  header << agui::label(caption, &this->theme.captionLabel) << agui::pusher;
  frame << header;

  agui::Table& table = make<agui::Table>(1u, &this->theme.controlTable);
  const std::vector<Control>& controls = AllControls();
  for (size_t i = 0; i < controls.size(); ++i) {
    if (controls[i].section != which) continue;
    agui::HorizontalFlow& line = make<agui::HorizontalFlow>(&this->theme.playerInputFlow);
    line << this->name(controls[i].name, controls[i].tip) << agui::pusher;
    this->found.back().lines.push_back({ &line, controls[i].name, {} });
    for (bool alternative : { false, true }) {
      agui::TextButton& button = make<agui::TextButton>(&this->theme.controlButton);
      button.setFocusable(false);
      button.setToggleButton(true);
      button.mouseButtonFilter = agui::MouseButton::LEFT_AND_RIGHT;
      const size_t slot = this->slots.size();
      button.onClick(this, [this, slot](const agui::MouseEvent& event) { this->clicked(slot, event); });
      this->slots.push_back({ &button, i, alternative });
      const Slot& s = this->slots.back();
      this->resettable.track(button, [this, s](const Settings& other) { return this->keys(s) != this->keys(s, other); });
      this->found.back().lines.back().keys.push_back(&button);
      line << button;
    }
    table << line;
  }
  frame << table;
  return frame;
}

agui::Widget& ControlsPage::mouseSection(bool tree)
{
  agui::Frame& frame = make<agui::Frame>(agui::GuiDirection::Vertical, &this->theme.shallowFrame);
  this->found.push_back({ &frame, {} });
  agui::HorizontalFlow& header = make<agui::HorizontalFlow>(&this->theme.playerInputFlow);
  header.style.setLeftPadding(8);
  header << agui::label(tree ? "The mouse in the game tree" : "The mouse on the board", &this->theme.captionLabel) << agui::pusher;
  frame << header;

  agui::Table& table = make<agui::Table>(1u, &this->theme.controlTable);
  for (const MouseAction& action : tree ? std::span<const MouseAction>(TREE) : std::span<const MouseAction>(MOUSE)) {
    agui::HorizontalFlow& line = make<agui::HorizontalFlow>(&this->theme.playerInputFlow);
    line << this->name(action.name, action.tip) << agui::pusher;
    this->found.back().lines.push_back({ &line, action.name, {} });
    for (const char* text : { action.primary, action.alternative }) {
      agui::Button& button = agui::button(std::string(text ? text : ""), &this->window, nullptr, &this->theme.controlButton);
      button.setFocusable(false);
      button.setEnabled(false);
      this->found.back().lines.back().keys.push_back(&button);
      line << button;
    }
    table << line;
  }
  frame << table;
  return frame;
}

agui::Widget& ControlsPage::name(const char* text, const char* tip)
{
  agui::Label& label = agui::label(text);
  if (!tip) return label;
  // Factorio's labelWithToolTipWithInfoIcon: either of them shows the tooltip.
  label.setToolTip(tip);
  agui::ImageWidget& icon = make<agui::ImageWidget>(this->theme.infoIcon());
  icon.style.setMinimalWidth(INFO_W);
  icon.style.setMaximalWidth(INFO_W);
  icon.style.setMinimalHeight(INFO_H);
  icon.style.setMaximalHeight(INFO_H);
  icon.setToolTip(tip);
  agui::HorizontalFlow& flow = row(4);
  flow << label << icon;
  return flow;
}

KeyCombo& ControlsPage::keys(const Slot& slot)
{
  Bindings::Pair& pair = this->settings.controls.keys[slot.control];
  return slot.alternative ? pair.alternative : pair.primary;
}

const KeyCombo& ControlsPage::keys(const Slot& slot, const Settings& other) const
{
  const Bindings::Pair& pair = other.controls.keys[slot.control];
  return slot.alternative ? pair.alternative : pair.primary;
}

void ControlsPage::clicked(size_t slot, const agui::MouseEvent& event)
{
  // Clicking the button that waits, or another, starts again from nothing.
  this->stop();
  if (event.getButton() == agui::MouseButton::RIGHT) {
    this->keys(this->slots[slot]) = {};
  } else {
    this->waitingFor = slot;
    // The keys pressed now are for the button, not a text field.
    if (agui::Gui::instance) agui::Gui::instance->clearFocus();
  }
  this->refresh();
}

void ControlsPage::assign(const KeyCombo& keys)
{
  if (!this->waiting()) return;
  this->keys(this->slots[this->waitingFor]) = keys;
  this->stop();
}

void ControlsPage::stop()
{
  if (!this->waiting()) return;
  this->waitingFor = NONE;
  this->refresh();
}

void ControlsPage::open()
{
  this->search->clearAndHide();
  this->waitingFor = NONE;
  this->settings   = this->live;
  this->openedWith = this->live;
  this->refresh();
}

void ControlsPage::filter()
{
  const agui::LowercaseString wanted(this->searched);
  const auto matches = [&wanted](std::string_view text) {
    return agui::StringMatcher::matchesSearchPattern(text, wanted) == agui::StringMatcherResult::Match;
  };
  for (const Found& section : this->found) {
    bool any = false;
    for (const Row& line : section.lines) {
      const bool shown = matches(line.name) ||
                         std::any_of(line.keys.begin(), line.keys.end(), [&](agui::Widget* key) { return matches(key->getText()); });
      if (line.row->isVisible() != shown) line.row->setVisible(shown);
      any |= shown;
    }
    if (section.section->isVisible() != any) section.section->setVisible(any);
  }
}

void ControlsPage::fit(int height)
{
  if (height == this->fittedTo) return;
  this->fittedTo = height;
  this->scroll->style.setMaximalHeight(std::max(MIN_SCROLL_H, height - CHROME_H));
}

void ControlsPage::refresh()
{
  // Which controls each set of keys is on, to show the ones on more than one.
  std::map<std::pair<int, int>, std::vector<size_t>> users;
  const auto id = [](const KeyCombo& k) { return std::pair{ k.key, int(k.ctrl) | int(k.shift) << 1 | int(k.alt) << 2 }; };
  for (const Slot& slot : this->slots) {
    if (this->keys(slot).isSet()) users[id(this->keys(slot))].push_back(slot.control);
  }

  const std::vector<Control>& controls = AllControls();
  for (size_t i = 0; i < this->slots.size(); ++i) {
    const Slot&       slot   = this->slots[i];
    const KeyCombo&   keys   = this->keys(slot);
    agui::TextButton& button = *slot.button;
    const bool        waits  = i == this->waitingFor;

    std::string tip = KeyText(keys);
    bool conflict = false;
    if (keys.isSet()) {
      for (size_t other : users[id(keys)]) {
        if (other == slot.control) continue;
        tip += std::string(conflict ? ", " : "\nAlso: ") + controls[other].name;
        conflict = true;
      }
    }
    button.setText(waits ? WAITING : KeyText(keys));
    button.setToolTip(tip + "\n" + HOW_TO_CLEAR);
    button.setToggleState(waits);
    const agui::ButtonStyle* style = conflict ? &this->theme.controlConflictButton : &this->theme.controlButton;
    if (button.style.getParent() != style) button.style.setParent(style);
  }
  this->resettable.changed();
  // The keys on the buttons may have changed what matches.
  if (!this->searched.empty()) this->filter();
}

}  // namespace ui
