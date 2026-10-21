// The editor's screen: a bar of file buttons across the top, the board on the
// left, and down the right a panel with the players, the tools, the buttons
// for moving about the record, the comment, and the game tree.
//
// Every button here comes down to a Command (see Commands.hpp), the same as
// the keyboard shortcuts, and they are handed to the App: whatever works on
// the record the App gives back to run(); files and pages it deals with
// itself.

#pragma once

#include <game/Game.hpp>
#include <ui/BoardView.hpp>
#include <ui/Commands.hpp>
#include <ui/Controls.hpp>
#include <ui/TreeView.hpp>

#include <Agui/GenericTargetable.hpp>
#include <Agui/Widget/Frame.hpp>

#include <array>
#include <string>
#include <vector>

namespace agui {
class Button;
class DropDown;
class Gui;
class ImageWidget;
class Label;
class TextBox;
class TextField;
}  // namespace agui

namespace ui {

class GoSprites;
enum class Sprite;
class Theme;

class EditorView : public agui::GenericTargetable {
public:
  // Adds itself to `gui`. Do this before the pages, so they draw on top.
  EditorView(agui::Gui& gui, Theme& theme, const GoSprites& sprites);
  ~EditorView();
  EditorView(const EditorView&) = delete;
  EditorView& operator=(const EditorView&) = delete;

  // The game to show and edit. Not owned; null for none.
  void show(Game* game);
  // What the top bar says the file is.
  void setTitle(const std::string& title);
  void setBoardOptions(const BoardView::Options& options);
  // The buttons' tooltips name the keys that do the same.
  void setBindings(const Bindings& bindings);
  // Move numbers on the game tree's stones.
  void setTreeNumbers(bool on);
  // The Navigate, Moves and Variation groups, or not: their keys do the same.
  void setNavigationButtons(bool shown);
  // The game tree, and the Variation group -- the selection's, while moves
  // are selected in the tree.
  TreeView&     gameTree() { return this->tree; }
  agui::Widget& variationButtons() { return *this->variationGroup; }
  // The row for a node's name and how good its position and move are: shown
  // when `on`, and anyway for a game that has any of that.
  void setEvaluation(bool on);
  // Drawing on the board (see BoardView): what was drawn here, what others
  // drew, and the colour lines drawn here come out in.
  std::vector<net::StrokePart> takeStrokes() { return this->board.takeStrokes(); }
  void addStroke(uint32_t author, uint8_t colour, const net::StrokePart& part) { this->board.addStroke(author, colour, part); }
  void setOwnColour(uint8_t colour) { this->board.setOwnColour(colour); }
  void setShift(bool held) { this->board.setShift(held); }

  // Whether this editor shares its game, as the panel's first box shows it:
  // offline, connecting, hosting or joined, and who else is there.
  struct Presence {
    enum class Mode { Offline, Connecting, Hosting, Joined };
    Mode                          mode = Mode::Offline;
    std::vector<net::Participant> people;
    uint32_t                      self = 0;
    bool operator==(const Presence&) const = default;
  };
  void showPresence(const Presence& presence);

  // The interface scale, in percent, which the board lines its grid up with.
  void setScale(int percent) { this->board.setScale(percent); }

  // Does what `command` asks of the record: moving about it, changing it,
  // or picking a tool. Commands about files and pages are the App's.
  void run(Command command);

  // What the buttons asked for since the last call, in order.
  std::vector<Command> takeCommands();

  // A line along the bottom of the side panel, for a few seconds.
  void report(const Outcome& outcome);
  void message(const std::string& text, bool good = true);

  // Once a frame, after Gui::logic(): brings everything in line with the
  // record, lays the screen out for its size, and runs the status line's
  // clock.
  void update(float dt, int screenWidth, int screenHeight);

  // While a page is up the editor still shows, but takes no clicks.
  void setInteractive(bool value);

  // Whether a text box of the editor's has the caret.
  bool typing() const;

private:
  agui::Widget& buildTopBar();
  agui::Widget& buildPlayers();
  agui::Widget& buildTools();
  agui::Widget& buildNavigation();
  agui::Widget& buildNode();

  agui::Button& commandButton(const char* text, Command command, const char* tip, int width = 0);
  // A tool-bar button with a picture on it that gives `command`.
  agui::Button& iconButton(Sprite sprite, Command command, const char* tip);
  // The button that picks `which` tool, as TOOLS describes it.
  agui::Button& toolButton(Tool which);
  // The Stones group's Clear all, which is no tool.
  agui::Button& clearAllButton();
  // A bordered frame round a group of buttons, as the settings page has, with
  // the group's name at its top.
  agui::Frame& group(const char* caption);
  void setTool(Tool tool);
  // The tooltips of `tipped`, with the keys tippedWith has for them.
  void applyTips();

  void refresh();
  void placeIcons();
  void layout(int screenWidth, int screenHeight);

  agui::Gui&       gui;
  Theme&           theme;
  const GoSprites& sprites;

  BoardView board;
  TreeView  tree;

  // The only widgets the view owns outright; everything under them is held
  // by the tree and goes with it.
  agui::Frame topBar;
  agui::Frame side;

  agui::Label*     title = nullptr;
  agui::Label*     blackName = nullptr;
  agui::Label*     whiteName = nullptr;
  agui::Label*     blackCaptures = nullptr;
  agui::Label*     whiteCaptures = nullptr;
  agui::Label*     presenceLabel = nullptr;
  agui::Widget*    presenceInfo = nullptr;  // the (i) with who is there, when not offline
  Presence         shownPresence;
  bool             presenceShown = false;
  agui::Label*     status = nullptr;
  agui::TextField* labelText = nullptr;
  agui::TextField* nodeName = nullptr;
  agui::DropDown*  positionNote = nullptr;
  agui::DropDown*  moveNote = nullptr;
  agui::TextBox*   comment = nullptr;
  agui::Widget*    upper = nullptr;  // everything in the side panel above the comment
  agui::Widget*    navigation = nullptr;  // the Navigate, Moves and Variation groups
  agui::Widget*    variationGroup = nullptr;
  agui::Label*     variationCaption = nullptr;  // "Selection" while moves are selected
  agui::Button*    mainLineButton = nullptr;
  agui::Button*    cancelSelectionButton = nullptr;  // in Main line's place while moves are selected
  bool             navigationShown = true;
  agui::Widget*    evaluationRow = nullptr;  // the node's name, and the position's and move's notes
  bool             evaluationOn  = false;
  const Game*      evaluatedGame = nullptr;  // the game last looked through for them,
  bool             gameEvaluated = false;    // and whether it has any
  void             showEvaluation();

  std::array<agui::Button*, size_t(Tool::Count)> toolButtons{};

  // Every button that does what a control does, with its tooltip before
  // the keys are added to it: they go on the end of its first line.
  struct Tipped {
    agui::Button* button;
    Command       command;
    std::string   tip;
  };
  std::vector<Tipped> tipped;
  Bindings            tippedWith;

  // A picture on a tool button, and where it goes, from the button's outer
  // top-left corner.
  struct IconSpot {
    agui::Button* button;
    agui::Widget* icon;
    int           x, y;
  };
  std::vector<IconSpot> icons;

  Game* game = nullptr;
  Tool  tool = Tool::Play;

  std::vector<Command> pending;

  // What the panel last showed, so it only changes what has changed.
  uint64_t         shownRevision = 0;
  const sgf::Node* commentNode   = nullptr;
  // The node last arrived at: arriving on a move played on a stone puts its
  // reason up at once, over whatever the status line had.
  const sgf::Node* arrivedAt     = nullptr;

  // The status line says why the current move lands on a stone, for as long
  // as it is the current move: no time runs out on that.
  void explainCurrent();

  float statusTime    = 0.0f;   // seconds the status line has left
  bool  showingReason = false;  // it is explainCurrent()'s
};

}  // namespace ui
