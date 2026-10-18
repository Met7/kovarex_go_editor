// The Controls page: which keys do what, and changing them.
//
// Built the way Factorio's ControlSettingsGui is, less what a Go editor has
// no use for (collapsing the sections, the controller, the search): a deep
// panel under a subheader with the red reset button, and in it a section per
// group of controls, each a table of rows -- the control's name, then its
// primary and alternative keys on two buttons. Clicking a button waits for
// the keys to put on it ("Waiting"); right-clicking clears it ("Not set").
// Keys that another control has too show in red.
//
// Like the Settings page, it edits a draft, taken when it opens, that only
// Confirm keeps; the reset button and Back light up what they would change.
//
// The page can't read the keys itself: the App reads the keyboard ahead of
// the Gui, and while the page is waiting() hands it the keys pressed.

#pragma once

#include <app/Settings.hpp>
#include <ui/Resettable.hpp>

#include <Agui/GenericTargetable.hpp>
#include <Agui/Widget/Window.hpp>

#include <functional>
#include <string>
#include <vector>

namespace agui {
class Button;
class TextButton;
class MouseEvent;
class VerticalScrollPane;
class Widget;
}  // namespace agui

namespace ui {

class SearchBar;
class Theme;

class ControlsPage : public agui::GenericTargetable {
public:
  ControlsPage(Theme& theme, const Settings& live, std::function<void()> onConfirm, std::function<void()> onBack);

  agui::Window& root() { return this->window; }

  // The page is being opened: the draft starts as the keys are now.
  void open();

  // The keys as the page has them, for Confirm to keep.
  const Bindings& draft() const { return this->settings.controls; }

  // Whether a button is waiting for keys; assign() gives it them. stop()
  // gives up waiting, leaving the keys it had.
  bool waiting() const { return this->waitingFor != NONE; }
  void assign(const KeyCombo& keys);
  void stop();

  // The search in its title bar.
  SearchBar& searchBar() { return *this->search; }

  // How tall the page may be, in the Gui's units: the controls scroll
  // within what is left of it.
  void fit(int height);

private:
  // One of the buttons: whose keys it shows.
  struct Slot {
    agui::TextButton* button;
    size_t            control;  // in AllControls()
    bool              alternative;
  };
  static constexpr size_t NONE = size_t(-1);

  KeyCombo& keys(const Slot& slot);
  const KeyCombo& keys(const Slot& slot, const Settings& settings) const;

  // A section of `rows` with its caption; the controls of `section`, or
  // what the mouse does on the board or in the game tree.
  agui::Widget& section(const char* caption, ControlSection which);
  agui::Widget& mouseSection(bool tree);
  // A control's name, followed by an info icon when it has a tooltip.
  agui::Widget& name(const char* text, const char* tip);

  // Hides the rows that don't match what is searched for -- by name, or by
  // the keys on them, as Factorio's control settings search -- and the
  // sections left with none.
  void filter();

  void clicked(size_t slot, const agui::MouseEvent& event);
  // Brings every button in line with the draft: its keys, whether another
  // control has them too, and its tooltip.
  void refresh();

  const Settings& live;        // the App's, as they are in use
  Settings        settings;    // the draft: only its controls change
  Settings        openedWith;  // the draft as the page opened, for Back's highlight
  Theme&          theme;
  agui::Window    window;
  Resettable      resettable;

  agui::VerticalScrollPane* scroll = nullptr;
  int                       fittedTo = 0;

  // A section and its rows, for search.
  struct Row {
    agui::Widget*              row;
    std::string                name;
    std::vector<agui::Widget*> keys;  // the buttons, whose text is searched too
  };
  struct Found {
    agui::Widget*    section;
    std::vector<Row> lines;
  };
  std::vector<Found> found;
  SearchBar*         search = nullptr;
  std::string        searched;

  std::vector<Slot> slots;
  size_t            waitingFor = NONE;  // in slots
};

}  // namespace ui
