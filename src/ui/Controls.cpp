#include <ui/Controls.hpp>

#include <raylib.h>

#include <algorithm>

namespace ui {

namespace {

struct KeyName {
  int         key;
  const char* name;
};

// Every key a control can be bound to, by the name it shows as -- and is
// written as in config.ini.
constexpr KeyName KEY_NAMES[] = {
  { KEY_A, "A" }, { KEY_B, "B" }, { KEY_C, "C" }, { KEY_D, "D" }, { KEY_E, "E" }, { KEY_F, "F" }, { KEY_G, "G" },
  { KEY_H, "H" }, { KEY_I, "I" }, { KEY_J, "J" }, { KEY_K, "K" }, { KEY_L, "L" }, { KEY_M, "M" }, { KEY_N, "N" },
  { KEY_O, "O" }, { KEY_P, "P" }, { KEY_Q, "Q" }, { KEY_R, "R" }, { KEY_S, "S" }, { KEY_T, "T" }, { KEY_U, "U" },
  { KEY_V, "V" }, { KEY_W, "W" }, { KEY_X, "X" }, { KEY_Y, "Y" }, { KEY_Z, "Z" },
  { KEY_ZERO, "0" }, { KEY_ONE, "1" }, { KEY_TWO, "2" }, { KEY_THREE, "3" }, { KEY_FOUR, "4" },
  { KEY_FIVE, "5" }, { KEY_SIX, "6" }, { KEY_SEVEN, "7" }, { KEY_EIGHT, "8" }, { KEY_NINE, "9" },
  { KEY_F1, "F1" }, { KEY_F2, "F2" }, { KEY_F3, "F3" }, { KEY_F4, "F4" }, { KEY_F5, "F5" }, { KEY_F6, "F6" },
  { KEY_F7, "F7" }, { KEY_F8, "F8" }, { KEY_F9, "F9" }, { KEY_F10, "F10" }, { KEY_F11, "F11" }, { KEY_F12, "F12" },
  { KEY_LEFT, "Left" }, { KEY_RIGHT, "Right" }, { KEY_UP, "Up" }, { KEY_DOWN, "Down" },
  { KEY_PAGE_UP, "Page Up" }, { KEY_PAGE_DOWN, "Page Down" }, { KEY_HOME, "Home" }, { KEY_END, "End" },
  { KEY_INSERT, "Insert" }, { KEY_DELETE, "Delete" }, { KEY_BACKSPACE, "Backspace" },
  { KEY_ENTER, "Enter" }, { KEY_TAB, "Tab" }, { KEY_SPACE, "Space" }, { KEY_ESCAPE, "Escape" },
  { KEY_PAUSE, "Pause" },
  { KEY_APOSTROPHE, "'" }, { KEY_COMMA, "," }, { KEY_MINUS, "-" }, { KEY_PERIOD, "." }, { KEY_SLASH, "/" },
  { KEY_SEMICOLON, ";" }, { KEY_EQUAL, "=" }, { KEY_LEFT_BRACKET, "[" }, { KEY_BACKSLASH, "\\" },
  { KEY_RIGHT_BRACKET, "]" }, { KEY_GRAVE, "`" },
  { KEY_KP_0, "Numpad 0" }, { KEY_KP_1, "Numpad 1" }, { KEY_KP_2, "Numpad 2" }, { KEY_KP_3, "Numpad 3" },
  { KEY_KP_4, "Numpad 4" }, { KEY_KP_5, "Numpad 5" }, { KEY_KP_6, "Numpad 6" }, { KEY_KP_7, "Numpad 7" },
  { KEY_KP_8, "Numpad 8" }, { KEY_KP_9, "Numpad 9" }, { KEY_KP_DECIMAL, "Numpad ." }, { KEY_KP_DIVIDE, "Numpad /" },
  { KEY_KP_MULTIPLY, "Numpad *" }, { KEY_KP_SUBTRACT, "Numpad -" }, { KEY_KP_ADD, "Numpad +" },
  { KEY_KP_ENTER, "Numpad Enter" },
};

// The modifiers, as they start the name of a combination.
constexpr std::string_view CTRL  = "Ctrl + ";
constexpr std::string_view SHIFT = "Shift + ";
constexpr std::string_view ALT   = "Alt + ";

constexpr KeyCombo Key(int key) { return { key }; }
constexpr KeyCombo CtrlKey(int key) { return { key, true }; }
constexpr KeyCombo CtrlShiftKey(int key) { return { key, true, true }; }
constexpr KeyCombo NONE{};

}  // namespace

std::string KeyText(const KeyCombo& combo)
{
  const auto name = std::find_if(std::begin(KEY_NAMES), std::end(KEY_NAMES), [&](const KeyName& k) { return k.key == combo.key; });
  if (name == std::end(KEY_NAMES)) return "Not set";
  std::string text;
  if (combo.ctrl) text += CTRL;
  if (combo.shift) text += SHIFT;
  if (combo.alt) text += ALT;
  return text + name->name;
}

KeyCombo ParseKey(std::string_view text)
{
  KeyCombo combo;
  // The modifiers come first, in this order, so the key's own name -- which
  // may have a + in it, as Numpad + does -- is whatever is left after them.
  for (auto [prefix, flag] : { std::pair{ CTRL, &KeyCombo::ctrl }, std::pair{ SHIFT, &KeyCombo::shift },
                               std::pair{ ALT, &KeyCombo::alt } }) {
    if (text.starts_with(prefix)) {
      combo.*flag = true;
      text.remove_prefix(prefix.size());
    }
  }
  const auto name = std::find_if(std::begin(KEY_NAMES), std::end(KEY_NAMES), [&](const KeyName& k) { return k.name == text; });
  if (name == std::end(KEY_NAMES)) return {};
  combo.key = name->key;
  return combo;
}

std::string ShortcutText(std::string_view keys)
{
  return "[font=default-semibold][color=128,206,240]" + std::string(keys) + "[/color][/font]";
}

bool IsBindable(int key)
{
  return std::any_of(std::begin(KEY_NAMES), std::end(KEY_NAMES), [key](const KeyName& k) { return k.key == key; });
}

const std::vector<Control>& AllControls()
{
  using S = ControlSection;
  constexpr const char* WHOLE_BRANCH = "This move and everything after it.";
  constexpr const char* MARK         = "A drag marks every point it crosses, Ctrl + drag takes the mark off. Shift + drag marks every stone in a box; Ctrl + Shift + drag clears a box.";
  constexpr const char* TERRITORY    = "A drag marks every point it crosses, Ctrl + drag takes the mark off. Shift + drag marks every empty point in a box; Ctrl + Shift + drag clears a box.";
  constexpr const char* LABEL        = "Ctrl + Shift + drag clears the labels in a box.";
  static const std::vector<Control> controls = {
    { "back",               Command::Back,              "Back one move",         "The mouse wheel does it too.", S::Moving, true,  Key(KEY_LEFT) },
    { "forward",            Command::Forward,           "Forward one move",      "The mouse wheel does it too.", S::Moving, true,  Key(KEY_RIGHT) },
    { "back-ten",           Command::BackMany,          "Back ten moves",        nullptr, S::Moving, true,  Key(KEY_PAGE_UP) },
    { "forward-ten",        Command::ForwardMany,       "Forward ten moves",     nullptr, S::Moving, true,  Key(KEY_PAGE_DOWN) },
    { "start",              Command::Start,             "To the start",          nullptr, S::Moving, false, Key(KEY_HOME) },
    { "end",                Command::End,               "To the end of the line", nullptr, S::Moving, false, Key(KEY_END) },
    { "previous-variation", Command::PreviousVariation, "Previous variation",    nullptr, S::Moving, true,  Key(KEY_UP) },
    { "next-variation",     Command::NextVariation,     "Next variation",        nullptr, S::Moving, true,  Key(KEY_DOWN) },

    { "pass",           Command::Pass,            "Pass",                  nullptr, S::Editing, false, CtrlKey(KEY_P) },
    { "undo",           Command::Undo,            "Undo",                  nullptr, S::Editing, true,  CtrlKey(KEY_Z) },
    { "redo",           Command::Redo,            "Redo",                  nullptr, S::Editing, true,  CtrlKey(KEY_Y), CtrlShiftKey(KEY_Z) },
    { "delete",         Command::DeleteBranch,    "Delete the move",       WHOLE_BRANCH, S::Editing, false, Key(KEY_DELETE) },
    { "main-line",      Command::PromoteMainLine, "Make it the main line", "The line to this move becomes the game's main line.", S::Editing, false, CtrlKey(KEY_M) },
    { "cut",            Command::Cut,             "Cut the move",          WHOLE_BRANCH, S::Editing, false, CtrlKey(KEY_X) },
    { "copy",           Command::Copy,            "Copy the move",         WHOLE_BRANCH, S::Editing, false, CtrlKey(KEY_C) },
    { "paste",          Command::Paste,           "Paste a variation",     "What was cut or copied, as a new variation from this move.", S::Editing, false, CtrlKey(KEY_V) },
    { "rotate-left",     Command::RotateLeft,     "Rotate the board 90 degrees left", "The whole game, every move and mark in it.", S::Editing, false, NONE },
    { "rotate-right",    Command::RotateRight,    "Rotate the board 90 degrees right", "The whole game, every move and mark in it.", S::Editing, false, NONE },
    { "flip-horizontal", Command::FlipHorizontal, "Flip horizontally", "The whole game, every move and mark in it.", S::Editing, false, NONE },
    { "flip-vertical",   Command::FlipVertical,   "Flip vertically", "The whole game, every move and mark in it.", S::Editing, false, NONE },

    { "tool-play",            ToolCommand(Tool::Play),           "Play moves",          nullptr, S::Tools, false, Key(KEY_Q) },
    { "tool-setup",           ToolCommand(Tool::SetupBlack),     "Black set-up stones", "Stones, not moves: black, or white with Shift. Ctrl + Shift + drag clears the set-up stones in a box.", S::Tools, false, Key(KEY_B) },
    { "tool-setup-white",     ToolCommand(Tool::SetupWhite),     "White set-up stones", "Stones, not moves: white, or black with Shift. Ctrl + Shift + drag clears the set-up stones in a box.", S::Tools, false, Key(KEY_W) },
    { "tool-erase",           ToolCommand(Tool::Erase),          "Clear set-up stones", nullptr, S::Tools, false, Key(KEY_E) },
    { "clear-setup",          Command::ClearSetup,               "Clear all set-up stones", nullptr, S::Tools, false, NONE },
    { "tool-territory-black", ToolCommand(Tool::TerritoryBlack), "Black's territory",   TERRITORY, S::Tools, false, NONE },
    { "tool-territory-white", ToolCommand(Tool::TerritoryWhite), "White's territory",   TERRITORY, S::Tools, false, NONE },
    { "tool-triangle",        ToolCommand(Tool::Triangle),       "Triangle",            MARK, S::Tools, false, Key(KEY_T) },
    { "tool-square",          ToolCommand(Tool::Square),         "Square",              MARK, S::Tools, false, Key(KEY_S) },
    { "tool-circle",          ToolCommand(Tool::Circle),         "Circle",              MARK, S::Tools, false, Key(KEY_C) },
    { "tool-cross",           ToolCommand(Tool::Cross),          "Cross",               MARK, S::Tools, false, Key(KEY_X) },
    { "tool-selected",        ToolCommand(Tool::Selected),       "Selected points",     MARK, S::Tools, false, NONE },
    { "tool-pen",             ToolCommand(Tool::Pen),            "Pen",                 "Draws on the board, to show something. The middle mouse button, or Alt with the left one, draws with any tool.", S::Tools, false, Key(KEY_P) },
    { "tool-letter",          ToolCommand(Tool::Letter),         "Letters",             LABEL, S::Tools, false, Key(KEY_L) },
    { "tool-number",          ToolCommand(Tool::Number),         "Numbers",             LABEL, S::Tools, false, Key(KEY_N) },
    { "tool-text",            ToolCommand(Tool::Text),           "Text",                LABEL, S::Tools, false, NONE },
    { "tool-arrow",           ToolCommand(Tool::Arrow),          "Arrow",               nullptr, S::Tools, false, Key(KEY_A) },
    { "tool-line",            ToolCommand(Tool::Line),           "Line",                nullptr, S::Tools, false, NONE },

    { "new-game",  Command::NewGame,  "New game",           nullptr, S::Files, false, CtrlKey(KEY_N) },
    { "open",      Command::Open,     "Open",               nullptr, S::Files, false, CtrlKey(KEY_O) },
    { "save",      Command::Save,     "Save",               nullptr, S::Files, false, CtrlKey(KEY_S) },
    { "save-as",   Command::SaveAs,   "Save as",            nullptr, S::Files, false, CtrlShiftKey(KEY_S) },
    { "game-info", Command::GameInfo, "Game information",   nullptr, S::Files, false, CtrlKey(KEY_I) },
    { "ai-sensei", Command::AiSensei, "Review on AI Sensei", "Opens AI Sensei's upload page in the browser, with the game on it.", S::Files, false, NONE },

    { "cancel",          Command::Cancel,         "Close or cancel",      "Closes a page, leaves a text box, lets go of the moves selected in the game tree, or drops a half-drawn arrow.", S::Interface, false, Key(KEY_ESCAPE) },
    { "settings",        Command::Settings,       "Settings",             nullptr, S::Interface, false, NONE },
    { "controls",        Command::Controls,       "Controls",             nullptr, S::Interface, false, NONE },
    { "host",            Command::Host,           "Host",                 "Starting a session, to share the game with other editors.", S::Interface, false, NONE },
    { "join",            Command::Join,           "Join",                 "Joining another editor's session.", S::Interface, false, NONE },
    { "about",           Command::About,          "About",                nullptr, S::Interface, false, NONE },
    { "focus-search",    Command::FocusSearch,    "Focus search",         "The search of the Settings or Controls page.", S::Interface, false, CtrlKey(KEY_F) },
    { "scale-up",        Command::ScaleUp,        "Bigger interface",     "One step up the UI scale, from the automatic one if that is in use.", S::Interface, false, CtrlKey(KEY_KP_ADD) },
    { "scale-down",      Command::ScaleDown,      "Smaller interface",    "One step down the UI scale, from the automatic one if that is in use.", S::Interface, false, CtrlKey(KEY_KP_SUBTRACT) },
    { "scale-automatic", Command::ScaleAutomatic, "Automatic UI scale",   "Back to the UI scale that follows the window's size.", S::Interface, false, CtrlKey(KEY_KP_0) },
  };
  return controls;
}

Bindings::Bindings()
{
  for (const Control& c : AllControls()) this->keys.push_back({ c.primary, c.alternative });
}

std::string Bindings::keysFor(Command command) const
{
  const std::vector<Control>& controls = AllControls();
  for (size_t i = 0; i < controls.size() && i < this->keys.size(); ++i) {
    if (controls[i].command != command) continue;
    const Pair& pair = this->keys[i];
    if (pair.primary.isSet()) return KeyText(pair.primary);
    if (pair.alternative.isSet()) return KeyText(pair.alternative);
  }
  return {};
}

}  // namespace ui
