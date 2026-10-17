#include <ui/Shortcuts.hpp>

#include <ui/Input.hpp>

#include <raylib.h>

#include <algorithm>

namespace ui {

namespace {

// The keys pressed since last frame, from raylib's queue of them. A key
// pressed and let go again between two frames is in it, though
// input::IsKeyPressed() never saw it down -- a quick tap, or a key sent by a
// program -- and so is the modifier held along with it.
std::vector<int> PressedSinceLastFrame()
{
  std::vector<int> keys;
  for (int key = input::GetKeyPressed(); key != 0; key = input::GetKeyPressed()) keys.push_back(key);
  return keys;
}

// `key` with the modifiers down with it: down now, or pressed since last frame.
KeyCombo Held(int key, const std::vector<int>& pressed)
{
  const auto down = [&pressed](int a, int b) {
    return input::IsKeyDown(a) || input::IsKeyDown(b) || std::find(pressed.begin(), pressed.end(), a) != pressed.end() ||
           std::find(pressed.begin(), pressed.end(), b) != pressed.end();
  };
  return { key, down(KEY_LEFT_CONTROL, KEY_RIGHT_CONTROL), down(KEY_LEFT_SHIFT, KEY_RIGHT_SHIFT),
           down(KEY_LEFT_ALT, KEY_RIGHT_ALT) };
}
// Whether a text box with the caret in it leaves `combo` to the shortcuts.
bool TextBoxLetsGo(const KeyCombo& combo)
{
  if (combo.key == KEY_ESCAPE || (combo.key >= KEY_F1 && combo.key <= KEY_F12)) return true;
  if (combo.alt) return true;
  if (!combo.ctrl) return false;
  // Select all, and the text's own cut, copy and paste.
  return combo.key != KEY_A && combo.key != KEY_C && combo.key != KEY_V && combo.key != KEY_X && combo.key != KEY_INSERT;
}

}  // namespace

std::vector<Command> Shortcuts::poll(const Bindings& bindings, bool typing, bool dialog)
{
  this->taken.clear();
  const std::vector<int> queued = PressedSinceLastFrame();
  std::vector<Command> commands;

  const std::vector<Control>& controls = AllControls();
  for (size_t i = 0; i < controls.size() && i < bindings.keys.size(); ++i) {
    const Control& control = controls[i];
    for (const KeyCombo& combo : { bindings.keys[i].primary, bindings.keys[i].alternative }) {
      if (!combo.isSet() || Held(combo.key, queued) != combo) continue;
      // Alt+F4 is Windows', whatever is bound to it.
      if (combo.alt && combo.key == KEY_F4) continue;

      const bool pressed = input::IsKeyPressed(combo.key) || std::find(queued.begin(), queued.end(), combo.key) != queued.end() ||
                           (control.repeats && input::IsKeyPressedRepeat(combo.key));
      if (!pressed) continue;
      // A key two controls share goes to the first.
      if (this->claimed(combo.key)) continue;

      if (typing && !TextBoxLetsGo(combo)) continue;
      const bool onPages = control.command == Command::Cancel || control.command == Command::FocusSearch ||
                           control.command == Command::ScaleUp || control.command == Command::ScaleDown ||
                           control.command == Command::ScaleAutomatic;
      if (dialog && !onPages) continue;

      commands.push_back(control.command);
      this->taken.push_back(combo.key);
      break;
    }
  }
  // Esc stays the Gui's as well: a drop-down that is open closes on it.
  std::erase(this->taken, KEY_ESCAPE);
  return commands;
}

std::optional<KeyCombo> Shortcuts::capture()
{
  this->taken.clear();
  // All of the queue is taken, so keys pressed while waiting don't turn up later.
  const std::vector<int> queued = PressedSinceLastFrame();
  const auto key = std::find_if(queued.begin(), queued.end(), IsBindable);
  if (key == queued.end()) return std::nullopt;
  this->taken.push_back(*key);
  return Held(*key, queued);
}

bool Shortcuts::claimed(int key) const
{
  return std::find(this->taken.begin(), this->taken.end(), key) != this->taken.end();
}

}  // namespace ui
