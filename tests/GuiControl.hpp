// Playing the player, as Factorio's tests do it (tests/GuiControl there): the
// widget to use is looked up in the Gui -- by type, by a predicate, by its
// text -- and the mouse is moved onto it and clicked, or keys are pressed,
// through the editor's own input (ui/Input.hpp). So a click goes where a
// real one would: the shortcuts see the keys first, and a widget something
// else covers can't be clicked, which fails the check.
//
// Every action runs the frames it needs, through updateMe(), which is the
// scenario's: see TestScenarioGui.

#pragma once

#include <ui/Input.hpp>

#include <Agui/Gui.hpp>
#include <Agui/MouseButton.hpp>
#include <Agui/TopContainer.hpp>
#include <Agui/Widget.hpp>
#include <UnitTest.hpp>

#include <cstdint>
#include <functional>
#include <source_location>
#include <string>

class GuiControl
{
public:
  virtual ~GuiControl() = default;

  // Keys held with a key press.
  enum Modifiers : uint8_t
  {
    NoModifiers = 0,
    Ctrl        = 1,
    Shift       = 2,
    Alt         = 4,
  };

  void click(agui::Widget* widget, agui::MouseButton button = agui::MouseButton::LEFT, Modifiers modifiers = NoModifiers,
             std::source_location location = std::source_location::current());
  void doubleClick(agui::Widget* widget, std::source_location location = std::source_location::current());
  template <class T>
  void clickSelect(std::function<bool(T*)> widgetSelector = nullptr, agui::MouseButton button = agui::MouseButton::LEFT,
                   std::source_location location = std::source_location::current())
  {
    this->click(this->getWidget<T>(widgetSelector, location), button, NoModifiers, location);
  }
  // The visible widget with this text -- a button, mostly -- anywhere, or
  // only within `parent`, when the same text is on more than one.
  void clickSelectText(const std::string& text, std::source_location location = std::source_location::current());
  void clickSelectText(const std::string& text, agui::Widget& parent, std::source_location location = std::source_location::current());

  void hover(agui::Widget* widget, std::source_location location = std::source_location::current());
  void mouseDown(agui::Widget* widget, agui::MouseButton button = agui::MouseButton::LEFT,
                 std::source_location location = std::source_location::current());
  void buttonsUp();
  void moveTo(agui::Widget* widget, std::source_location location = std::source_location::current());
  void moveTo(agui::Widget* widget, agui::Point positionInWidget, std::source_location location = std::source_location::current());
  void moveToAbsolute(agui::Point absolutePosition);

  template <class T>
  T* getWidget(std::function<bool(T*)> widgetSelector = nullptr, std::source_location location = std::source_location::current())
  {
    T* result = this->findWidget<T>(widgetSelector);
    if (!result)
      CHECK_FAILED_SOURCELOCATION((std::string(typeid(T).name()) + " widget not found").c_str(), location);
    return result;
  }
  template <class T>
  T* getWidget(agui::Widget& parent, std::function<bool(T*)> widgetSelector = nullptr,
               std::source_location location = std::source_location::current())
  {
    T* result = parent.getWidget<T>(widgetSelector);
    if (!result)
      CHECK_FAILED_SOURCELOCATION((std::string(typeid(T).name()) + " widget not found").c_str(), location);
    return result;
  }
  template <class T>
  T* findWidget(std::function<bool(T*)> widgetSelector = nullptr)
  {
    return agui::Gui::instance->getTop()->getWidget<T>(widgetSelector);
  }
  template <class T = agui::Widget>
  T* findWidgetWithText(const std::string& text)
  {
    return this->findWidget<T>([&text](T* widget) { return widget->getText() == text; });
  }

  // raylib's KEY_*s.
  void keyDown(int key);
  void keyUp(int key);
  void press(int key, Modifiers modifiers = NoModifiers);
  void pressEnter() { this->press(KEY_ENTER); }
  void pressEscape() { this->press(KEY_ESCAPE); }
  void pressSelectAll() { this->press(KEY_A, Ctrl); }
  // Typed into whatever has the focus, as text.
  void write(const std::string& text);

  template <class T>
  static uint32_t countWidgets(agui::Widget& parent)
  {
    uint32_t count = 0;
    parent.getWidget<T>([&count](T*) { ++count; return false; });
    return count;
  }

  // Runs the editor `count` frames.
  virtual void updateMe(uint32_t count = 1) = 0;
  // Screen pixels per GUI unit, to put the mouse where a widget is.
  virtual float viewScale() = 0;

protected:
  input::SimulatedInput input;

private:
  void setModifiers(Modifiers modifiers, bool down);
  // Moves the mouse onto `widget` and checks it is what the mouse is over.
  void moveToSlot(agui::Widget* widget, agui::Point absolutePosition, std::source_location location);
};
