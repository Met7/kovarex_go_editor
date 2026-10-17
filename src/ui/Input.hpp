// The keyboard and the mouse, as everything in the editor reads them. Normally
// they are raylib's: these are its IsKeyDown(), GetCharPressed() and the rest.
// A test plays the player instead, by handing in a SimulatedInput (see
// tests/GuiControl.hpp): from then on the keys and buttons it holds down are
// the only ones there are, the shortcuts and the Gui alike see them, and the
// real keyboard and mouse are ignored.

#pragma once

#include <raylib.h>

#include <array>
#include <deque>

namespace input {

// What a test says the player is doing. Frame to frame, as raylib keeps it: a
// key is pressed in the first frame it is down, released in the first it is
// up again. nextFrame() moves on to the next frame -- the App's frame does it
// at its end, as EndDrawing() does for raylib.
class SimulatedInput {
public:
  void setKeyDown(int key, bool down) { this->keys[size_t(key)] = down; }
  // The key, as pressed since last frame -- the order keys go in, which raylib
  // also keeps -- and down until setKeyDown(key, false).
  void pressKey(int key);
  // Text typed, character by character: what GetCharPressed() gives.
  void typeCharacter(int codepoint) { this->characters.push_back(codepoint); }

  void setMousePosition(Vector2 position) { this->mouse = position; }
  void setMouseButtonDown(int button, bool down) { this->buttons[size_t(button)] = down; }
  void setWheel(float move) { this->wheel = move; }

  void nextFrame();

  bool    keyDown(int key) const { return this->keys[size_t(key)]; }
  bool    keyWasDown(int key) const { return this->keysBefore[size_t(key)]; }
  bool    buttonDown(int button) const { return this->buttons[size_t(button)]; }
  bool    buttonWasDown(int button) const { return this->buttonsBefore[size_t(button)]; }
  Vector2 mousePosition() const { return this->mouse; }
  float   wheelMove() const { return this->wheel; }
  int     takeKey();
  int     takeCharacter();

private:
  static constexpr size_t KEY_COUNT    = 512;  // raylib's MAX_KEYBOARD_KEYS
  static constexpr size_t BUTTON_COUNT = 8;    // and MAX_MOUSE_BUTTONS

  std::array<bool, KEY_COUNT>    keys{}, keysBefore{};
  std::array<bool, BUTTON_COUNT> buttons{}, buttonsBefore{};
  std::deque<int>                pressed, characters;
  Vector2                        mouse{ 0, 0 };
  float                          wheel = 0;
};

// Takes over from raylib's input until it is given nullptr again.
void Simulate(SimulatedInput* simulated);
// The frame is over: see SimulatedInput::nextFrame(). Does nothing without one.
void NextFrame();

bool    IsKeyDown(int key);
bool    IsKeyPressed(int key);
bool    IsKeyPressedRepeat(int key);
bool    IsKeyReleased(int key);
int     GetKeyPressed();
int     GetCharPressed();
Vector2 GetMousePosition();
bool    IsMouseButtonPressed(int button);
bool    IsMouseButtonReleased(int button);
float   GetMouseWheelMove();

}  // namespace input
