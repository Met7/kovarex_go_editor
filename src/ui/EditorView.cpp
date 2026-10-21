#include <ui/EditorView.hpp>

#include <ui/Form.hpp>
#include <ui/GoSprites.hpp>
#include <ui/Theme.hpp>

#include <Agui/Font.hpp>
#include <Agui/Gui.hpp>
#include <Agui/SystemClipboard.hpp>
#include <Agui/Widget/Button.hpp>
#include <Agui/Widget/DropDown.hpp>
#include <Agui/Widget/HorizontalFlow.hpp>
#include <Agui/Widget/ImageWidget.hpp>
#include <Agui/Widget/Label.hpp>
#include <Agui/Widget/TextBox.hpp>
#include <Agui/Widget/TextField.hpp>
#include <Agui/Widget/VerticalFlow.hpp>

#include <algorithm>
#include <cstdlib>

namespace ui {

namespace {

// The tool buttons' size: Theme::toolButton's.
constexpr int TOOL_PX = Theme::TOOL_PX;
// The side panel's least width: nine tool buttons across, and the players' names.
// It gets whatever the board leaves, which is usually more.
constexpr int SIDE_W = 432;
// How long a message stays on the status line.
constexpr float STATUS_SECONDS = 6.0f;
// Below this, the comment and the tree stop shrinking and the panel runs off
// the bottom of the window instead.
constexpr int MIN_COMMENT_H = 60;
constexpr int MIN_TREE_H    = 60;
// Comments are seldom long: more than this goes to the tree.
constexpr int MAX_COMMENT_H = 120;
// The info mark after a name with a tooltip: Factorio's size for it.
constexpr int INFO_W = 8;
constexpr int INFO_H = 20;
// The Stones group's buttons with words on them.
constexpr int CLEAR_W     = 48;
constexpr int CLEAR_ALL_W = 68;

struct ToolLook {
  Tool                  tool;
  std::optional<Sprite> icon;
  const char*           text;
  const char*           tip;
};

// The tools, and how their buttons look.
constexpr ToolLook TOOLS[] = {
  { Tool::Play,           std::nullopt,           "",    "Play moves\nCtrl+click inserts a move after this one; drag a stone to move it, however long ago it was played." },
  { Tool::SetupBlack,     Sprite::BlackStone,     "",    "Set up black stones\nShift+click sets up a white one. A click on a stone of that colour takes it off. Ctrl+Shift+drag clears the set-up stones in a box." },
  { Tool::SetupWhite,     Sprite::WhiteStone,     "",    "Set up white stones\nShift+click sets up a black one. A click on a stone of that colour takes it off. Ctrl+Shift+drag clears the set-up stones in a box." },
  { Tool::Erase,          std::nullopt,           "Clear", "Clear set-up stones" },
  { Tool::Triangle,       Sprite::TriangleDark,   "",    "Triangle\nShift+drag marks every stone in a box; with Ctrl, drags take marks off." },
  { Tool::Square,         Sprite::SquareDark,     "",    "Square\nShift+drag marks every stone in a box; with Ctrl, drags take marks off." },
  { Tool::Circle,         Sprite::CircleDark,     "",    "Circle\nShift+drag marks every stone in a box; with Ctrl, drags take marks off." },
  { Tool::Cross,          Sprite::CrossDark,      "",    "Cross\nShift+drag marks every stone in a box; with Ctrl, drags take marks off." },
  { Tool::Selected,       Sprite::Selected,       "",    "Selected points\nShift+drag marks every stone in a box; with Ctrl, drags take marks off." },
  { Tool::Letter,         std::nullopt,           "A",   "Letters, A, B, C...\nCtrl+Shift+drag clears the labels in a box." },
  { Tool::Number,         std::nullopt,           "1",   "Numbers, 1, 2, 3...\nCtrl+Shift+drag clears the labels in a box." },
  { Tool::Text,           std::nullopt,           "Ab",  "A label of your own: type it in the box on the right\nCtrl+Shift+drag clears the labels in a box." },
  { Tool::Arrow,          std::nullopt,           "->",  "Arrow: click where it starts, then where it points" },
  { Tool::Line,           std::nullopt,           "--",  "Line: click one end, then the other" },
  { Tool::TerritoryBlack, Sprite::TerritoryBlack, "",    "Black's territory\nShift+drag marks every empty point in a box; with Ctrl, drags take marks off." },
  { Tool::TerritoryWhite, Sprite::TerritoryWhite, "",    "White's territory\nShift+drag marks every empty point in a box; with Ctrl, drags take marks off." },
  { Tool::Pen,            Sprite::Pen,            "",    "Draw on the board, to show something -- in a shared game, the others see it too. The lines fade away. The middle mouse button, or Alt with the left one, draws whatever the tool." },
};

// The drop-downs' items, and the SGF property each sets.
constexpr const char* POSITION_NOTES[][2] = {
  { "Position: -", "" }, { "Good for Black", "GB" }, { "Good for White", "GW" }, { "Even", "DM" }, { "Unclear", "UC" },
};
constexpr const char* MOVE_NOTES[][2] = {
  { "Move: -", "" }, { "Good move", "TE" }, { "Bad move", "BM" }, { "Interesting", "IT" }, { "Doubtful", "DO" },
};

void Pin(agui::Widget& widget, agui::Style& style, int w, int h)
{
  style.setMinimalWidth(w);
  style.setMaximalWidth(w);
  style.setMinimalHeight(h);
  style.setMaximalHeight(h);
  widget.setSize(w, h, agui::SetSizeInfo());
}

void PinWidth(agui::Style& style, int w)
{
  style.setMinimalWidth(w);
  style.setMaximalWidth(w);
}

agui::ImageWidget& Icon(const GoSprites& sprites, Sprite sprite, int side)
{
  agui::ImageWidget& icon = make<agui::ImageWidget>(sprites.image(sprite));
  icon.scaleToKeepTheRatio = true;
  icon.setIgnoredByInteraction(true);
  Pin(icon, icon.style, side, side);
  return icon;
}

// "Honinbo Shusaku 4d", from a name and a rank either of which may be missing.
std::string Player(const sgf::Node& root, const char* name, const char* rank, const char* fallback)
{
  std::string text = root.get(name).empty() ? fallback : root.get(name);
  if (!root.get(rank).empty()) text += "  " + root.get(rank);
  return text;
}

}  // namespace

// ---------------------------------------------------------------- building

EditorView::EditorView(agui::Gui& gui, Theme& theme, const GoSprites& sprites)
    : gui(gui)
    , theme(theme)
    , sprites(sprites)
    , board(theme, sprites)
    , tree(theme, sprites)
    , topBar(agui::GuiDirection::Horizontal)
    , side(agui::GuiDirection::Vertical)
{
  this->topBar << this->buildTopBar();

  agui::VerticalFlow& upperFlow = column(8);
  upperFlow << this->buildPlayers();
  upperFlow << this->buildTools();
  upperFlow << this->buildNavigation();
  upperFlow << this->buildNode();
  this->upper = &upperFlow;
  this->side << upperFlow;

  this->comment = &make<agui::TextBox>();
  this->comment->setWordWrap(true);
  this->comment->setHScrollPolicy(agui::ScrollPolicy::Never);
  this->comment->setVScrollPolicy(agui::ScrollPolicy::Auto);
  // Its tooltip names the key that lets go of it: see applyTips().
  this->comment->onTextEdit(this, [this] {
    if (this->game) this->game->setNodeText("C", this->comment->getText());
  });
  this->side << *this->comment;

  this->side << this->tree.widget();
  this->tree.onMoved([this](const Outcome& outcome) { this->report(outcome); });

  // Long enough to say why an edit was refused, so it wraps.
  this->status = &agui::label("", &theme.dimLabel, agui::SingleLine::False);
  this->side << *this->status;

  this->board.onOutcome = [this](const Outcome& outcome) { this->report(outcome); };
  this->board.onWheel   = [this](int steps) { this->pending.push_back(steps < 0 ? Command::Back : Command::Forward); };

  gui.add(&this->board.widget());
  gui.add(&this->topBar);
  gui.add(&this->side);
  this->setTool(Tool::Play);
  this->applyTips();
}

EditorView::~EditorView()
{
  this->gui.remove(&this->side);
  this->gui.remove(&this->topBar);
  this->gui.remove(&this->board.widget());
}

agui::Button& EditorView::commandButton(const char* text, Command command, const char* tip, int width)
{
  agui::Button& button = agui::button(std::string(text), &this->side, [this, command] { this->pending.push_back(command); },
                                      &this->theme.smallButton);
  button.setFocusable(false);
  if (tip) this->tipped.push_back({ &button, command, tip });
  if (width > 0) button.style.setMinimalWidth(width);
  return button;
}

agui::Widget& EditorView::buildTopBar()
{
  agui::HorizontalFlow& bar = row(4);
  bar.style.setHorizontallyStretchable(true);
  bar << this->commandButton("New", Command::NewGame, "A new game", 60);
  bar << this->commandButton("Open", Command::Open, "Open a file", 60);
  bar << this->commandButton("Save", Command::Save, "Save", 60);
  bar << this->commandButton("Save as", Command::SaveAs, "Save under another name", 80);
  bar << this->commandButton("Game info", Command::GameInfo, "Players, result, date and the rest", 100);
  bar << this->commandButton("AI Sensei", Command::AiSensei,
                             "Have AI Sensei review this game: its upload page opens in the browser with the game on it", 100);

  this->title = &agui::label("", &this->theme.headingLabel);
  this->title->style.setLeftPadding(16);
  bar << *this->title;
  bar << agui::pusher;
  bar << this->commandButton("Host", Command::Host, "Share this game with other editors: all of you edit it, and see the same position", 70);
  bar << this->commandButton("Join", Command::Join, "Join another editor's game: all of you edit it, and see the same position", 70);
  bar << this->commandButton("Controls", Command::Controls, nullptr, 90);
  bar << this->commandButton("Settings", Command::Settings, nullptr, 90);
  bar << this->commandButton("About", Command::About, nullptr, 70);
  return bar;
}

agui::Widget& EditorView::buildPlayers()
{
  agui::Frame& panel = make<agui::Frame>(agui::GuiDirection::Vertical, &this->theme.insideShallowFrameWithPadding);
  agui::VerticalFlow& rows = column(4);

  const auto playerRow = [this](Sprite stone, agui::Label*& name, agui::Label*& captures) {
    agui::HorizontalFlow& r = row(8);
    r.style.setHorizontallyStretchable(true);
    name     = &agui::label("", &this->theme.headingLabel);
    captures = &agui::label("", &this->theme.dimLabel);
    captures->setToolTip("Stones this player has captured");
    r << Icon(this->sprites, stone, 20) << *name << agui::pusher << *captures;
    return &r;
  };
  rows << *playerRow(Sprite::BlackStone, this->blackName, this->blackCaptures);
  rows << *playerRow(Sprite::WhiteStone, this->whiteName, this->whiteCaptures);

  // Whether the game is shared, and with how many; the (i) after it tells
  // who, in their colours.
  agui::HorizontalFlow& presence = row(4);
  this->presenceLabel = &agui::label("Offline", &this->theme.dimLabel);
  agui::ImageWidget& info = make<agui::ImageWidget>(this->theme.infoIcon());
  info.style.setMinimalWidth(INFO_W);
  info.style.setMaximalWidth(INFO_W);
  info.style.setMinimalHeight(INFO_H);
  info.style.setMaximalHeight(INFO_H);
  info.setVisible(false);
  this->presenceInfo = &info;
  presence << *this->presenceLabel << info;
  rows << presence;

  panel << rows;
  return panel;
}

agui::Button& EditorView::toolButton(Tool which)
{
  const ToolLook& look = *std::find_if(std::begin(TOOLS), std::end(TOOLS), [which](const ToolLook& t) { return t.tool == which; });
  agui::Button& button = look.text[0] ? agui::button(std::string(look.text), &this->side, nullptr, &this->theme.toolButton)
                                      : make<agui::Button>(&this->theme.toolButton);
  button.setFocusable(false);
  button.setToggleButton(true);
  this->tipped.push_back({ &button, ToolCommand(which), look.tip });
  button.onClick(this, [this, which] { this->pending.push_back(ToolCommand(which)); });

  // Pictures sit in the middle of the button. A button does not lay out
  // its children, so they are placed by hand -- see placeIcons().
  constexpr int ICON = 24;
  const int     at   = (TOOL_PX - ICON) / 2;
  if (look.icon) {
    agui::ImageWidget& icon = Icon(this->sprites, *look.icon, ICON);
    button << icon;
    this->icons.push_back({ &button, &icon, at, at });
  } else if (which == Tool::Play) {
    // Both colours, one after the other, the pair centred together.
    constexpr int STONE = 16, STEP = 8;
    const int     from  = (TOOL_PX - STONE - STEP) / 2;
    agui::ImageWidget& black = Icon(this->sprites, Sprite::BlackStone, STONE);
    agui::ImageWidget& white = Icon(this->sprites, Sprite::WhiteStone, STONE);
    button << black << white;
    this->icons.push_back({ &button, &black, from, from });
    this->icons.push_back({ &button, &white, from + STEP, from + STEP });
  }
  // A word is wider than a picture.
  if (look.text[0] && look.text[1] && look.text[2]) Pin(button, button.style, CLEAR_W, TOOL_PX);
  this->toolButtons[size_t(which)] = &button;
  return button;
}

agui::Button& EditorView::clearAllButton()
{
  // A command, but as tall as the tools beside it.
  agui::Button& button = agui::button(std::string("Clear all"), &this->side, [this] { this->pending.push_back(Command::ClearSetup); },
                                      &this->theme.toolButton);
  button.setFocusable(false);
  this->tipped.push_back({ &button, Command::ClearSetup, "Clear all set-up stones in this position." });
  Pin(button, button.style, CLEAR_ALL_W, TOOL_PX);
  return button;
}

agui::Button& EditorView::iconButton(Sprite sprite, Command command, const char* tip)
{
  agui::Button& button = make<agui::Button>(&this->theme.toolButton);
  button.setFocusable(false);
  button.onClick(this, [this, command] { this->pending.push_back(command); });
  this->tipped.push_back({ &button, command, tip });
  // In the middle of the button, placed by placeIcons().
  constexpr int ICON = 24;
  const int     at   = (TOOL_PX - ICON) / 2;
  agui::ImageWidget& icon = Icon(this->sprites, sprite, ICON);
  button << icon;
  this->icons.push_back({ &button, &icon, at, at });
  return button;
}

agui::Frame& EditorView::group(const char* caption)
{
  agui::Frame& frame = make<agui::Frame>(agui::GuiDirection::Vertical, &this->theme.borderedFrame);
  frame << agui::label(caption, &this->theme.captionLabel);
  return frame;
}

// The tools, in groups: stones, territory, and the marks and labels.
agui::Widget& EditorView::buildTools()
{
  agui::VerticalFlow& rows = column(4);

  agui::HorizontalFlow& stones = row(4);
  for (Tool t : { Tool::Play, Tool::SetupBlack, Tool::SetupWhite, Tool::Erase }) stones << this->toolButton(t);
  stones << this->clearAllButton();
  agui::HorizontalFlow& territory = row(4);
  for (Tool t : { Tool::TerritoryBlack, Tool::TerritoryWhite }) territory << this->toolButton(t);
  agui::HorizontalFlow& top = row(4);
  top.style.setHorizontallyStretchable(true);
  top << (this->group("Stones") << stones) << (this->group("Territory") << territory);
  rows << top;

  agui::HorizontalFlow& shapes = row(4);
  for (Tool t : { Tool::Triangle, Tool::Square, Tool::Circle, Tool::Cross, Tool::Selected }) {
    shapes << this->toolButton(t);
  }
  agui::HorizontalFlow& labels = row(4);
  for (Tool t : { Tool::Letter, Tool::Number, Tool::Text }) labels << this->toolButton(t);
  // The Text tool's text, right after its button.
  this->labelText = &make<agui::TextField>();
  this->labelText->style.setMinimalWidth(60);
  this->labelText->style.setMaximalWidth(60);
  this->labelText->setToolTip("What the Text tool writes on the board");
  this->labelText->onTextEdit(this, [this] { this->setTool(Tool::Text); });
  labels << *this->labelText;
  for (Tool t : { Tool::Arrow, Tool::Line, Tool::Pen }) labels << this->toolButton(t);
  // Turning and mirroring the board, beside the marks: two rows of two, so
  // the panel grows no taller for them.
  agui::HorizontalFlow& turns = row(4);
  turns << this->iconButton(Sprite::RotateLeft, Command::RotateLeft, "Rotate the board 90 degrees left")
        << this->iconButton(Sprite::RotateRight, Command::RotateRight, "Rotate the board 90 degrees right");
  agui::HorizontalFlow& flips = row(4);
  flips << this->iconButton(Sprite::FlipHorizontal, Command::FlipHorizontal, "Flip horizontally")
        << this->iconButton(Sprite::FlipVertical, Command::FlipVertical, "Flip vertically");
  agui::HorizontalFlow& marksAndBoard = row(4);
  marksAndBoard.style.setHorizontallyStretchable(true);
  marksAndBoard << (this->group("Marks") << shapes << labels) << (this->group("Board") << turns << flips);
  rows << marksAndBoard;
  return rows;
}

// Going through the record, and changing it.
agui::Widget& EditorView::buildNavigation()
{
  agui::VerticalFlow& rows = column(4);

  agui::HorizontalFlow& moves = row(4);
  // Ten moves at a time and the variations are keys only (see Controls).
  moves << this->commandButton("|<", Command::Start, "To the start");
  moves << this->commandButton("<", Command::Back, "Back one move\nThe mouse wheel does it too.", 44);
  moves << this->commandButton(">", Command::Forward, "Forward one move\nThe mouse wheel does it too.", 44);
  moves << this->commandButton(">|", Command::End, "To the end of this line");
  agui::HorizontalFlow& edits = row(4);
  edits << this->commandButton("Pass", Command::Pass, "Pass");
  edits << this->commandButton("Undo", Command::Undo, "Undo");
  edits << this->commandButton("Redo", Command::Redo, "Redo");
  agui::HorizontalFlow& top = row(4);
  top.style.setHorizontallyStretchable(true);
  top << (this->group("Navigate") << moves) << (this->group("Moves") << edits);
  rows << top;

  agui::HorizontalFlow& variation = row(4);
  variation << this->commandButton("Delete", Command::DeleteBranch, "Delete this move and everything after it");
  variation << this->commandButton("Cut", Command::Cut, "Cut this move and everything after it");
  variation << this->commandButton("Copy", Command::Copy, "Copy this move and everything after it");
  variation << this->commandButton("Paste", Command::Paste, "Paste what was cut or copied as a new variation here");
  // Last, as one of the two is shown at a time.
  this->mainLineButton = &this->commandButton("Main line", Command::PromoteMainLine, "Make this line the main line");
  this->cancelSelectionButton = &this->commandButton("Cancel", Command::Cancel, "Let go of the selected moves");
  this->cancelSelectionButton->setVisible(false);
  variation << *this->mainLineButton << *this->cancelSelectionButton;
  // Its edits are the selection's while moves are selected in the game tree:
  // it says so, and has Cancel for Main line then (refresh).
  this->variationCaption = &agui::label("Variation", &this->theme.captionLabel);
  this->variationGroup   = &(make<agui::Frame>(agui::GuiDirection::Vertical, &this->theme.borderedFrame)
                           << *this->variationCaption << variation);
  rows << *this->variationGroup;

  // All of it goes without the navigation buttons (setNavigationButtons).
  this->navigation = &rows;
  return rows;
}

agui::Widget& EditorView::buildNode()
{
  agui::HorizontalFlow& r = row(4);

  this->nodeName = &make<agui::TextField>();
  PinWidth(this->nodeName->style, 104);
  this->nodeName->setToolTip("A name for this move or position (N)");
  this->nodeName->onTextEdit(this, [this] {
    if (this->game) this->game->setNodeText("N", this->nodeName->getText());
  });

  this->positionNote = &make<agui::DropDown>();
  for (const auto& note : POSITION_NOTES) this->positionNote->addItem(std::string(note[0]));
  PinWidth(this->positionNote->style, 160);
  this->positionNote->onItemSelect(this, [this](int i) {
    if (this->game && i >= 0) this->game->setPositionAnnotation(POSITION_NOTES[i][1]);
  });

  this->moveNote = &make<agui::DropDown>();
  for (const auto& note : MOVE_NOTES) this->moveNote->addItem(std::string(note[0]));
  PinWidth(this->moveNote->style, 140);
  this->moveNote->onItemSelect(this, [this](int i) {
    if (this->game && i >= 0) this->game->setMoveAnnotation(MOVE_NOTES[i][1]);
  });

  r << *this->nodeName << *this->positionNote << *this->moveNote;
  this->evaluationRow = &r;
  r.setVisible(false);
  return r;
}

// ---------------------------------------------------------------- running

void EditorView::show(Game* newGame)
{
  this->game          = newGame;
  this->shownRevision = 0;
  this->commentNode   = nullptr;
  this->arrivedAt     = nullptr;
  this->evaluatedGame = nullptr;  // looked through afresh
  this->board.show(newGame);
  this->tree.show(newGame);
  this->refresh();
  this->showEvaluation();
}

void EditorView::setTitle(const std::string& text)
{
  if (this->title->getText() != text) this->title->setText(text);
}

void EditorView::setBoardOptions(const BoardView::Options& options)
{
  this->board.setOptions(options);
}

void EditorView::setBindings(const Bindings& bindings)
{
  if (bindings == this->tippedWith) return;
  this->tippedWith = bindings;
  this->applyTips();
}

void EditorView::applyTips()
{
  for (const Tipped& t : this->tipped) {
    const std::string keys = this->tippedWith.keysFor(t.command);
    if (keys.empty()) {
      t.button->setToolTip(t.tip);
      continue;
    }
    const size_t end = std::min(t.tip.find('\n'), t.tip.size());
    t.button->setToolTip(t.tip.substr(0, end) + " (" + ShortcutText(keys) + ")" + t.tip.substr(end));
  }
  // The comment box: what lets go of it, so the keys are shortcuts again.
  const std::string cancel = this->tippedWith.keysFor(Command::Cancel);
  this->comment->setToolTip("The comment on this move. " +
                            (cancel.empty() ? std::string("A click on the board") : ShortcutText(cancel) + ", or a click on the board,") +
                            " and the keys are shortcuts again.");
}

void EditorView::setTreeNumbers(bool on)
{
  this->tree.setNumbers(on);
}

void EditorView::setNavigationButtons(bool shown)
{
  if (shown == this->navigationShown) return;
  this->navigationShown = shown;
  this->navigation->setVisible(shown);
}

void EditorView::setEvaluation(bool on)
{
  if (on == this->evaluationOn) return;
  this->evaluationOn = on;
  this->showEvaluation();
}

namespace {

// Whether any node of the game has a name or says how good its position or
// move is.
bool Evaluated(const sgf::Node& root)
{
  constexpr std::string_view IDS[] = { "N", "GB", "GW", "DM", "UC", "TE", "BM", "IT", "DO" };
  std::vector<const sgf::Node*> left{ &root };
  while (!left.empty()) {
    const sgf::Node& node = *left.back();
    left.pop_back();
    for (const std::string_view id : IDS) {
      if (node.has(id)) return true;
    }
    for (size_t i = 0; i < node.childCount(); ++i) left.push_back(&node.child(i));
  }
  return false;
}

}  // namespace

void EditorView::showEvaluation()
{
  // Looked for once a game, and after it changes until found: once there,
  // the row stays for that game, not to come and go as it is edited.
  if (this->game && (this->game != this->evaluatedGame || !this->gameEvaluated)) {
    this->evaluatedGame = this->game;
    this->gameEvaluated = Evaluated(this->game->root());
  }
  this->evaluationRow->setVisible(this->evaluationOn || this->gameEvaluated);
}

void EditorView::showPresence(const Presence& presence)
{
  if (this->presenceShown && presence == this->shownPresence) return;
  this->presenceShown = true;
  this->shownPresence = presence;

  using Mode = Presence::Mode;
  const std::string others = " (" + std::to_string(presence.people.empty() ? 0 : presence.people.size() - 1) + " connected)";
  switch (presence.mode) {
  case Mode::Offline:    this->presenceLabel->setText(std::string("Offline")); break;
  case Mode::Connecting: this->presenceLabel->setText(std::string("Connecting...")); break;
  case Mode::Hosting:    this->presenceLabel->setText("Hosting" + others); break;
  case Mode::Joined:     this->presenceLabel->setText("Joined" + others); break;
  }
  this->presenceLabel->style.setParent(presence.mode == Mode::Offline ? &this->theme.dimLabel : &agui::Label::defaultStyle);

  // Who is there, each name in the colour they draw in.
  std::string who;
  for (const net::Participant& p : presence.people) {
    const agui::Color c = ParticipantColour(p.colour);
    if (!who.empty()) who += "\n";
    who += "[color=" + std::to_string(int(c.getR() * 255)) + "," + std::to_string(int(c.getG() * 255)) + "," +
           std::to_string(int(c.getB() * 255)) + "]" + p.name + "[/color]" + (p.id == presence.self ? " (you)" : "");
  }
  this->presenceInfo->setVisible(!who.empty());
  this->presenceLabel->setToolTip(who);
  this->presenceInfo->setToolTip(who);
}

std::vector<Command> EditorView::takeCommands()
{
  std::vector<Command> commands;
  commands.swap(this->pending);
  return commands;
}

void EditorView::setTool(Tool newTool)
{
  this->tool = newTool;
  for (size_t i = 0; i < this->toolButtons.size(); ++i) {
    agui::Button* button = this->toolButtons[i];
    if (button && button->isToggledInternal() != (Tool(i) == newTool)) button->setToggleState(Tool(i) == newTool);
  }
  this->board.setTool(newTool, this->labelText ? this->labelText->getText() : std::string());
}

void EditorView::run(Command command)
{
  if (IsToolCommand(command)) {
    this->setTool(CommandTool(command));
    return;
  }
  if (command == Command::Cancel) {
    // Esc lets go of a text box first, then of the moves selected in the
    // game tree, then of a half-drawn arrow, then goes back to playing moves.
    if (this->typing()) this->gui.clearFocus();
    else if (this->game && !this->game->selection().empty()) this->game->clearSelection();
    else {
      this->board.cancel();
      this->setTool(Tool::Play);
    }
    return;
  }
  if (!this->game) return;

  switch (command) {
  case Command::Back:              this->game->back(); break;
  case Command::Forward:           this->game->forward(); break;
  case Command::BackMany:          this->game->step(-10); break;
  case Command::ForwardMany:       this->game->step(10); break;
  case Command::Start:             this->game->toStart(); break;
  case Command::End:               this->game->toEnd(); break;
  case Command::PreviousVariation:
    if (!this->game->previousVariation()) this->message("This is the first variation here.", false);
    break;
  case Command::NextVariation:
    if (!this->game->nextVariation()) this->message("This is the last variation here.", false);
    break;
  case Command::Pass:            this->report(this->game->pass()); break;
  case Command::DeleteBranch:
    this->report(this->game->selection().empty() ? this->game->deleteBranch() : this->game->deleteSelection());
    break;
  case Command::PromoteMainLine: this->report(this->game->promoteToMainLine()); break;
  case Command::RotateLeft:      this->report(this->game->transform(BoardTransform::RotateLeft)); break;
  case Command::RotateRight:     this->report(this->game->transform(BoardTransform::RotateRight)); break;
  case Command::FlipHorizontal:  this->report(this->game->transform(BoardTransform::FlipHorizontal)); break;
  case Command::FlipVertical:    this->report(this->game->transform(BoardTransform::FlipVertical)); break;
  case Command::ClearSetup: {
    const Position& position = this->game->position();
    this->report(this->game->clearSetupStones({ 0, 0 }, { position.width() - 1, position.height() - 1 }));
    break;
  }
  case Command::Undo:
    if (!this->game->undo()) this->message("Nothing to undo.", false);
    break;
  case Command::Redo:
    if (!this->game->redo()) this->message("Nothing to redo.", false);
    break;
  case Command::Copy:
    if (!this->game->selection().empty()) {
      agui::SystemClipboard::copy(this->game->copySelection());
      this->message("Copied the selected moves.");
      break;
    }
    agui::SystemClipboard::copy(this->game->copyBranch());
    this->message(this->game->current().parent() ? "Copied this move and everything after it." : "Copied the whole game.");
    break;
  case Command::Cut:
    if (!this->game->selection().empty()) {
      // Nothing is cut that can't be deleted.
      const std::string copied = this->game->copySelection();
      const Outcome     outcome = this->game->deleteSelection();
      if (!outcome.ok) {
        this->report(outcome);
        break;
      }
      agui::SystemClipboard::copy(copied);
      this->message("Cut the selected moves.");
      break;
    }
    // Nothing is cut that can't be deleted: the start of the game can't.
    if (!this->game->current().parent()) {
      this->message("The start of the game can't be cut, only copied: that copies the whole game.", false);
      break;
    }
    agui::SystemClipboard::copy(this->game->copyBranch());
    this->report(this->game->deleteBranch());
    this->message("Cut this move and everything after it.");
    break;
  case Command::Paste: {
    const Outcome outcome = this->game->pasteBranch(agui::SystemClipboard::paste(false));
    if (outcome.ok && outcome.message.empty()) this->message("Pasted as a new variation.");
    else                                       this->report(outcome);
    break;
  }
  default:
    break;
  }
}

void EditorView::report(const Outcome& outcome)
{
  if (!outcome.ok) this->message(outcome.message, false);
  else if (!outcome.message.empty()) this->message(outcome.message, true);
}

void EditorView::message(const std::string& text, bool good)
{
  this->status->style.setParent(good ? &this->theme.goodLabel : &this->theme.badLabel);
  this->status->setText(text);
  this->statusTime    = STATUS_SECONDS;
  this->showingReason = false;
}

void EditorView::explainCurrent()
{
  if (this->statusTime > 0.0f) return;  // another message is up: the reason waits for it to go
  const std::string reason = this->game ? this->game->stoneOnStoneReason(this->game->current()) : std::string();
  if (!reason.empty()) {
    this->status->style.setParent(&this->theme.badLabel);
    if (this->status->getText() != reason) this->status->setText(reason);
    this->showingReason = true;
  } else if (this->showingReason) {
    this->status->setText(std::string());
    this->showingReason = false;
  }
}

bool EditorView::typing() const
{
  const agui::Widget* focused = this->gui.getFocusedWidget();
  return focused && focused->isTextBox();
}

void EditorView::setInteractive(bool value)
{
  this->board.setInteractive(value);
  this->topBar.setIgnoredByInteraction(!value);
  this->side.setIgnoredByInteraction(!value);
}

void EditorView::update(float dt, int screenWidth, int screenHeight)
{
  if (this->statusTime > 0.0f) {
    this->statusTime -= dt;
    if (this->statusTime <= 0.0f) {
      this->status->setText(std::string());
      this->explainCurrent();  // back to why the current move lands on a stone, if it does
    }
  }

  // The Text tool writes whatever the box says now.
  if (this->tool == Tool::Text) this->board.setTool(Tool::Text, this->labelText->getText());

  this->refresh();
  this->placeIcons();
  this->board.refresh();
  this->tree.refresh();
  this->layout(screenWidth, screenHeight);
}

void EditorView::placeIcons()
{
  // Children sit inside the button's padding, and that includes the border
  // of whatever it is drawn with at the moment -- which a toggled button
  // draws differently. So the offset from the button's edge is kept, and the
  // padding taken off whatever it is now.
  for (const IconSpot& spot : this->icons) {
    const int x = spot.x - spot.button->getLeftPadding();
    const int y = spot.y - spot.button->getTopPadding();
    if (spot.icon->getLocation().x != x || spot.icon->getLocation().y != y) spot.icon->setLocation(x, y);
  }
}

void EditorView::refresh()
{
  if (this->game) {
    const std::string caption = this->game->selection().empty() ? "Variation" : "Selection";
    if (this->variationCaption->getText() != caption) {
      this->variationCaption->setText(caption);
      this->mainLineButton->setVisible(caption == "Variation");
      this->cancelSelectionButton->setVisible(caption == "Selection");
    }
  }
  if (!this->game || this->shownRevision == this->game->revision()) return;
  this->shownRevision = this->game->revision();
  this->showEvaluation();

  const sgf::Node& root     = this->game->root();
  const sgf::Node& node     = this->game->current();
  const Position&  position = this->game->position();

  // A move played on a stone already there: what it landed on, and how to
  // put it right. The tree has it in red.
  if (this->arrivedAt != &node) {
    this->arrivedAt = &node;
    if (this->game->isStoneOnStone(node)) this->statusTime = 0.0f;
  }
  this->explainCurrent();

  this->blackName->setText(Player(root, "PB", "BR", "Black"));
  this->whiteName->setText(Player(root, "PW", "WR", "White"));
  this->blackCaptures->setText("captures " + std::to_string(position.captures(Stone::Black)));
  this->whiteCaptures->setText("captures " + std::to_string(position.captures(Stone::White)));

  // The comment: only when another node's comes up, or it changed some
  // other way -- an undo -- while the box wasn't being typed in, or the
  // caret would jump about under the typing.
  const std::string& text = node.get("C");
  if (this->commentNode != &node || (!this->comment->isFocused() && this->comment->getText() != text)) {
    this->comment->setText(text);
    this->commentNode = &node;
  }
  if (!this->nodeName->isFocused() && this->nodeName->getText() != node.get("N")) this->nodeName->setText(node.get("N"));

  const std::string positionId = this->game->positionAnnotation();
  const std::string moveId     = this->game->moveAnnotation();
  for (int i = 0; i < int(std::size(POSITION_NOTES)); ++i) {
    if (positionId == POSITION_NOTES[i][1] && this->positionNote->getSelectedIndex() != i) this->positionNote->setSelectedIndex(i);
  }
  for (int i = 0; i < int(std::size(MOVE_NOTES)); ++i) {
    if (moveId == MOVE_NOTES[i][1] && this->moveNote->getSelectedIndex() != i) this->moveNote->setSelectedIndex(i);
  }
}

void EditorView::layout(int screenWidth, int screenHeight)
{
  // The bar across the top; under it the board, as tall as the window
  // leaves, and the panel down the right in whatever width is left -- at
  // least SIDE_W, which the board gives way to in a narrow window.
  this->topBar.setLocation(0, 0);
  if (this->topBar.style.getMinimalWidth() != screenWidth) {
    this->topBar.style.setMinimalWidth(screenWidth);
    this->topBar.style.setMaximalWidth(screenWidth);
  }
  const int top    = this->topBar.getHeight();
  const int sideH  = std::max(0, screenHeight - top);
  const int boardW = this->board.layout(0, top, screenWidth - std::min(SIDE_W, screenWidth / 2), sideH);
  const int sideW  = std::max(0, screenWidth - boardW);
  this->side.setLocation(screenWidth - sideW, top);
  if (this->side.style.getMinimalWidth() != sideW || this->side.style.getMinimalHeight() != sideH) {
    Pin(this->side, this->side.style, sideW, sideH);
  }

  // The comment and the tree share what the rest of the panel leaves,
  // measured off the last layout rather than guessed at. The status line
  // has room for two lines kept for it, said or not, so a message coming and
  // going doesn't move the comment or the tree; a longer one takes what it
  // needs more off the tree's bottom only.
  const agui::Font* statusFont = this->status->style.getFont();
  const int statusH = 2 * (statusFont ? statusFont->getLineHeight() : 0) + this->status->getVerticalPaddings();
  const int statusOver = std::max(0, this->status->getHeight() - statusH);
  const int inner  = sideW - this->side.getHorizontalPaddings();
  const int chrome = this->side.getVerticalPaddings() + this->upper->getHeight() + statusH + 3 * 4 + 8;
  const int room   = std::max(MIN_COMMENT_H + MIN_TREE_H, sideH - chrome);
  const int commentH = std::clamp(room / 4, MIN_COMMENT_H, MAX_COMMENT_H);
  const int treeH    = std::max(MIN_TREE_H, room - commentH - statusOver);
  if (this->comment->getWidth() != inner || this->comment->getHeight() != commentH) {
    Pin(*this->comment, this->comment->style, inner, commentH);
  }
  this->tree.setSize(inner, treeH);
  PinWidth(this->status->style, inner);

}

}  // namespace ui
