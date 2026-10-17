#include <GuiControl.hpp>

#include <Agui/Rectangle.hpp>

#include <cmath>

namespace
{
  // Whether `widget` is `target`, or inside it: a button's label or picture
  // can be what is under the mouse, and the click still goes to the button.
  bool isWithin(const agui::Widget* widget, const agui::Widget* target)
  {
    for (; widget; widget = widget->getParent())
      if (widget == target)
        return true;
    return false;
  }

  std::string describe(const agui::Widget* widget)
  {
    if (!widget)
      return "nothing";
    std::string result = typeid(*widget).name();
    if (!widget->getText().empty())
      result += " \"" + widget->getText() + "\"";
    return result;
  }
}

void GuiControl::click(agui::Widget* widget, agui::MouseButton button, Modifiers modifiers, std::source_location location)
{
  CHECK_SOURCELOCATION(widget, location);
  this->moveToSlot(widget, widget->getAbsoluteRectangle().getCenter(), location);
  this->setModifiers(modifiers, true);
  this->mouseDown(widget, button, location);
  this->buttonsUp();
  this->setModifiers(modifiers, false);
  this->updateMe();
}

void GuiControl::doubleClick(agui::Widget* widget, std::source_location location)
{
  this->click(widget, agui::MouseButton::LEFT, NoModifiers, location);
  this->click(widget, agui::MouseButton::LEFT, NoModifiers, location);
}

void GuiControl::clickSelectText(const std::string& text, std::source_location location)
{
  this->click(this->getWidget<agui::Widget>([&text](agui::Widget* widget) { return widget->getText() == text; }, location),
              agui::MouseButton::LEFT, NoModifiers, location);
}

void GuiControl::clickSelectText(const std::string& text, agui::Widget& parent, std::source_location location)
{
  this->click(this->getWidget<agui::Widget>(parent, [&text](agui::Widget* widget) { return widget->getText() == text; }, location),
              agui::MouseButton::LEFT, NoModifiers, location);
}

void GuiControl::hover(agui::Widget* widget, std::source_location location)
{
  CHECK_SOURCELOCATION(widget, location);
  this->moveToSlot(widget, widget->getAbsoluteRectangle().getCenter(), location);
  agui::Gui::instance->forceHover();
  this->updateMe();
}

void GuiControl::mouseDown(agui::Widget* widget, agui::MouseButton button, std::source_location location)
{
  CHECK_SOURCELOCATION(widget, location);
  this->moveToSlot(widget, widget->getAbsoluteRectangle().getCenter(), location);
  switch (button)
  {
    case agui::MouseButton::LEFT: this->input.setMouseButtonDown(MOUSE_BUTTON_LEFT, true); break;
    case agui::MouseButton::RIGHT: this->input.setMouseButtonDown(MOUSE_BUTTON_RIGHT, true); break;
    case agui::MouseButton::MIDDLE: this->input.setMouseButtonDown(MOUSE_BUTTON_MIDDLE, true); break;
    default: CHECK_FAILED_SOURCELOCATION("Only the left, right and middle buttons can be pressed", location);
  }
  this->updateMe();
}

void GuiControl::buttonsUp()
{
  for (int button : { MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT, MOUSE_BUTTON_MIDDLE })
    this->input.setMouseButtonDown(button, false);
  this->updateMe();
}

void GuiControl::moveTo(agui::Widget* widget, std::source_location location)
{
  CHECK_SOURCELOCATION(widget, location);
  this->moveToAbsolute(widget->getAbsoluteRectangle().getCenter());
}

void GuiControl::moveTo(agui::Widget* widget, agui::Point positionInWidget, std::source_location location)
{
  CHECK_SOURCELOCATION(widget, location);
  this->moveToAbsolute(widget->getAbsoluteRectangle().getLeftTop() + positionInWidget);
}

void GuiControl::moveToAbsolute(agui::Point absolutePosition)
{
  // In the middle of the screen pixels the GUI unit covers, so it rounds back
  // to the same unit whatever the scale.
  const float scale = this->viewScale();
  this->input.setMousePosition({ (float(absolutePosition.x) + 0.5f) * scale, (float(absolutePosition.y) + 0.5f) * scale });
  this->updateMe();
}

void GuiControl::moveToSlot(agui::Widget* widget, agui::Point absolutePosition, std::source_location location)
{
  this->moveToAbsolute(absolutePosition);
  agui::Widget* underMouse = agui::Gui::instance->getWidgetUnderMouse();
  if (!isWithin(underMouse, widget))
    CHECK_FAILED_SOURCELOCATION(("Expected under mouse " + describe(widget) + " but got " + describe(underMouse)).c_str(), location);
}

void GuiControl::setModifiers(Modifiers modifiers, bool down)
{
  if (modifiers & Ctrl)
    this->input.setKeyDown(KEY_LEFT_CONTROL, down);
  if (modifiers & Shift)
    this->input.setKeyDown(KEY_LEFT_SHIFT, down);
  if (modifiers & Alt)
    this->input.setKeyDown(KEY_LEFT_ALT, down);
}

void GuiControl::keyDown(int key)
{
  this->input.pressKey(key);
  this->updateMe();
}

void GuiControl::keyUp(int key)
{
  this->input.setKeyDown(key, false);
  this->updateMe();
}

void GuiControl::press(int key, Modifiers modifiers)
{
  this->setModifiers(modifiers, true);
  this->keyDown(key);
  this->keyUp(key);
  this->setModifiers(modifiers, false);
  this->updateMe();
}

void GuiControl::write(const std::string& text)
{
  // ASCII is all the tests type; anything else would be decoded from UTF-8 here.
  for (char character : text)
    this->input.typeCharacter(static_cast<unsigned char>(character));
  this->updateMe();
}
