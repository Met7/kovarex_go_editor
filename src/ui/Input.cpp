#include <ui/Input.hpp>

namespace input {

namespace {

SimulatedInput* simulated = nullptr;

}  // namespace

void SimulatedInput::pressKey(int key)
{
  this->keys[size_t(key)] = true;
  this->pressed.push_back(key);
}

void SimulatedInput::nextFrame()
{
  this->keysBefore    = this->keys;
  this->buttonsBefore = this->buttons;
  this->pressed.clear();
  this->characters.clear();
  this->wheel = 0;
}

int SimulatedInput::takeKey()
{
  if (this->pressed.empty()) return 0;
  const int key = this->pressed.front();
  this->pressed.pop_front();
  return key;
}

int SimulatedInput::takeCharacter()
{
  if (this->characters.empty()) return 0;
  const int character = this->characters.front();
  this->characters.pop_front();
  return character;
}

void Simulate(SimulatedInput* input)
{
  simulated = input;
}

void NextFrame()
{
  if (simulated) simulated->nextFrame();
}

bool IsKeyDown(int key)
{
  return simulated ? simulated->keyDown(key) : ::IsKeyDown(key);
}

bool IsKeyPressed(int key)
{
  return simulated ? simulated->keyDown(key) && !simulated->keyWasDown(key) : ::IsKeyPressed(key);
}

bool IsKeyPressedRepeat(int key)
{
  return simulated ? false : ::IsKeyPressedRepeat(key);
}

bool IsKeyReleased(int key)
{
  return simulated ? !simulated->keyDown(key) && simulated->keyWasDown(key) : ::IsKeyReleased(key);
}

int GetKeyPressed()
{
  return simulated ? simulated->takeKey() : ::GetKeyPressed();
}

int GetCharPressed()
{
  return simulated ? simulated->takeCharacter() : ::GetCharPressed();
}

Vector2 GetMousePosition()
{
  return simulated ? simulated->mousePosition() : ::GetMousePosition();
}

bool IsMouseButtonPressed(int button)
{
  return simulated ? simulated->buttonDown(button) && !simulated->buttonWasDown(button) : ::IsMouseButtonPressed(button);
}

bool IsMouseButtonReleased(int button)
{
  return simulated ? !simulated->buttonDown(button) && simulated->buttonWasDown(button) : ::IsMouseButtonReleased(button);
}

float GetMouseWheelMove()
{
  return simulated ? simulated->wheelMove() : ::GetMouseWheelMove();
}

}  // namespace input
