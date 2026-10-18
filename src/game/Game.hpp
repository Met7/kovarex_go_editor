// A game record being edited: the SGF tree, where in it the editor is, and
// every change the editor can make to it. Still no window and no drawing --
// the board and the side panel read this and call into it.
//
// What the properties mean lives here. A move is B[] or W[]; stones set up are
// AB/AW/AE; marks are TR, SQ, CR, MA, SL, TB, TW, DD, labels LB, arrows and
// lines AR and LN; the comment is C. Everything else in a node is carried
// along untouched.
//
// Every edit is one undo step, and an edit that would leave the record
// broken is refused, with the reason, rather than made.
//
// Two edits go beyond what a recorder normally offers, because a game copied
// down by hand is rarely copied down right first time:
//
//   moveStone()   A stone on the board can be picked up and put somewhere
//                 else, and the move that played it is changed -- however many
//                 moves ago that was. Everything after it is played out again
//                 on top, so captures come out as they now would.
//
//   insertMove()  A move is put in after the current one, with everything that
//                 followed hanging on after it, rather than starting a new
//                 variation. Two of these put back an exchange that was left
//                 out.
//
// Both check the whole of the record after the change, variations included,
// and are refused if any later move would land on a stone.

#pragma once

#include <game/Position.hpp>
#include <game/Sgf.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

// The marks a point can carry, and the SGF property each is kept in.
enum class Mark { Triangle, Square, Circle, Cross, Selected, TerritoryBlack, TerritoryWhite, Dim };
const char* MarkProperty(Mark mark);

// What an edit came to. `message` says why it was refused, or -- for the
// edits that do something the player might not expect -- what was done.
struct Outcome {
  bool        ok = true;
  std::string message;

  static Outcome Fail(std::string why) { return { false, std::move(why) }; }
  static Outcome Done(std::string what = {}) { return { true, std::move(what) }; }
};

// Ways the whole board can be turned or mirrored.
enum class BoardTransform { RotateLeft, RotateRight, FlipHorizontal, FlipVertical };

// A new game, as the New game page sets it up.
struct GameSetup {
  int         width    = 19;
  int         height   = 19;
  int         handicap = 0;
  std::string komi     = "6.5";
  std::string black;
  std::string white;
  std::string date;  // DT, as YYYY-MM-DD
};

class Game {
public:
  explicit Game(const GameSetup& setup = {});
  // Takes over a file that has been read; edits its first game and keeps the
  // others to write back.
  explicit Game(sgf::Collection file);

  Game(const Game&) = delete;
  Game& operator=(const Game&) = delete;

  const sgf::Collection& file() const { return this->tree; }
  sgf::Node&             root() const { return *this->tree.games.front(); }
  sgf::Node&             current() const { return *this->cursor; }

  int width() const { return this->w; }
  int height() const { return this->h; }

  // The board at the current node, and the number of the move there (0 before
  // the first). Worked out again after anything changes.
  const Position& position() const;
  int             moveNumber() const;

  // Whose move it is at the current node.
  Stone toPlay() const;

  // The move a node makes: its colour, None if it makes none, and its point,
  // invalid for a pass.
  static Stone MoveColor(const sgf::Node& node);
  Point        movePoint(const sgf::Node& node) const;
  bool         isPass(const sgf::Node& node) const;

  // The nodes from the root down to `node`, inclusive.
  static std::vector<const sgf::Node*> Path(const sgf::Node& node);

  // --- navigation: none of these changes the record ---

  bool forward();
  bool back();
  bool toStart();
  bool toEnd();
  // Steps `count` moves along, stopping at either end.
  bool step(int count);
  // To the next or previous variation at this point: the current node's
  // sibling, so the board shows the alternative to the move just played.
  bool nextVariation();
  bool previousVariation();
  void goTo(sgf::Node& node);
  // Where the current node is, as the child indices from the root -- how
  // editors sharing a game tell each other where they are.
  std::vector<int> path() const { return this->cursorPath(); }
  // To the node at `path`, or as far along it as the record goes.
  void goToPath(const std::vector<int>& path);
  // To the node that put the stone at `p` on the board: the move that played
  // it, or the set-up that added it. False if there is no stone there.
  bool goToStone(Point p);

  // Where forward() would go: the child last visited from here, else the main
  // line. Null at the end of a line.
  sgf::Node* nextNode() const;

  // --- edits ---

  // A move at `p` by whoever is to play. If the current node already goes on
  // with that move, that is just stepped into; otherwise it starts a new
  // variation (or continues the line, at the end of one).
  Outcome play(Point p);
  Outcome pass();
  // The move put in after the current one rather than beside what follows.
  Outcome insertMove(Point p);
  // Moves the stone at `from` to `to` by changing the node that put it
  // there -- the move, or the set-up that added it.
  Outcome moveStone(Point from, Point to);
  // The current node and everything after it; the editor steps back one.
  Outcome deleteBranch();
  // Makes the line to the current node everyone's first child, so it is the
  // main line.
  Outcome promoteToMainLine();

  // The whole game turned a quarter or mirrored: every move, set-up stone,
  // mark, label, arrow and line in every variation. A rectangular board
  // turned has its sides swapped. For a game recorded from the other side,
  // or to put the corner the game starts in where the player expects it.
  Outcome transform(BoardTransform how);

  // The whole record replaced with `file`, and the editor at `path` in it:
  // someone else's edit, in a shared game, arriving. One undo step, like any
  // other edit.
  void replace(sgf::Collection file, const std::vector<int>& path);

  // --- the clipboard: branches as SGF text, so they go to and from other
  // programs as well ---

  // The current node and everything after it; at the start of the game, the
  // whole game. Cut is this and deleteBranch().
  std::string copyBranch() const;
  // An SGF record hung on the current node as a new variation, and the
  // editor moved onto it. A whole game -- from a file, or another program --
  // keeps only what isn't about the game: its moves, and any set-up in its
  // root. Refused if the board size differs or a move lands on a stone.
  // Moves already recorded aren't added twice: where the record follows a
  // variation that is already here, it is merged into it, and only what is
  // new branches off.
  Outcome pasteBranch(std::string_view text);

  // Set-up stones. `color` None clears the point; a colour that is already
  // there takes it away again. Set-up never shares a node with a move, so
  // after a move this starts a new node for it.
  Outcome setupStone(Point p, Stone color);

  // A mark on the current node: on if it was not there, off if it was. A point
  // carries one shape (or label) at a time.
  void toggleMark(Point p, Mark mark);
  bool hasMark(Point p, Mark mark) const;
  // Puts the mark on, or takes it off -- nothing if it already is so. With
  // `sameEdit`, part of the last edit, for one undo: a drag across points.
  void setMark(Point p, Mark mark, bool on, bool sameEdit);
  // A label, LB[p:text]. The same text again takes it off.
  void toggleLabel(Point p, const std::string& text);
  // The label the letter and number tools would put down next.
  std::string nextLetter() const;
  std::string nextNumber() const;
  // An arrow (AR) or a line (LN) between two points. Again takes it off.
  void toggleLine(Point from, Point to, bool arrow);

  // A text property of the current node -- the comment, C, or its name, N.
  // Empty removes it. Typing runs together into one undo step.
  void setNodeText(std::string_view id, std::string text);

  // The node's judgement of the position (GB, GW, DM, UC) and of the move
  // (TE, BM, IT, DO); "" for none. Each set is exclusive.
  void        setPositionAnnotation(std::string_view id);
  void        setMoveAnnotation(std::string_view id);
  std::string positionAnnotation() const;
  std::string moveAnnotation() const;

  // Game information in the root: players, result, komi and the rest.
  // Several at once, as one undo step; an empty value removes the property.
  void setGameInfo(const std::vector<std::pair<std::string, std::string>>& values);

  // --- undo ---

  bool canUndo() const { return !this->undoStack.empty(); }
  bool canRedo() const { return !this->redoStack.empty(); }
  bool undo();
  bool redo();

  // Changed since it was last saved or opened.
  bool modified() const { return this->version != this->savedVersion; }
  void markSaved() { this->savedVersion = this->version; }

  // Just before writing: the file is always written as FF[4] in UTF-8,
  // whatever it was read as, and its header has to say so. Not an edit.
  void stampFormat();

  // Bumped by anything that changes what the board shows -- a move, a step,
  // an undo -- so a view knows when to look again.
  uint64_t revision() const { return this->changes; }
  // Bumped only when nodes come and go or change their moves, which is when
  // the game tree has to be laid out again.
  uint64_t treeRevision() const { return this->treeChanges; }

private:
  struct Snapshot {
    sgf::Collection  tree;
    std::vector<int> path;  // child indices from the root to the cursor
    uint64_t         version;
  };

  std::vector<int> cursorPath() const;
  sgf::Node*       nodeAt(const std::vector<int>& path) const;

  // Before every edit: saves what to undo to, and forgets what could be
  // redone. `structural` edits relayout the tree view.
  void beginEdit(bool structural = true);
  // Puts back the state beginEdit() saved, for an edit that turned out to
  // break the record.
  void rollback();
  void restore(Snapshot& snapshot);
  void touched(bool structural);

  // The node a set-up or mark edit goes in.
  sgf::Node& setupNode();

  // The position just before `node` is applied, and the move number there.
  Position positionBefore(const sgf::Node& node, int* number = nullptr) const;

  // Plays out everything under `node` (inclusive) from `before`, and returns
  // the first move that lands on a stone, or null if none does. `number` is
  // that move's number.
  const sgf::Node* firstConflict(const sgf::Node& node, const Position& before, int numberBefore,
                                 int* number) const;

  // Whether `a` and `b` are the same move -- or, neither being a move, say
  // exactly the same -- so one of them is all a parent needs.
  bool sameNode(const sgf::Node& a, const sgf::Node& b) const;
  // Hangs `node` under `parent`, or, when `parent` already has the same one,
  // merges it into that: the properties it lacks, the comment if it says
  // something else, and the children, the same way. Returns where it went.
  sgf::Node& mergeChild(sgf::Node& parent, std::unique_ptr<sgf::Node> node);

  // Why a move by `color` at `p` could not be played on `position`, or "".
  std::string moveProblem(const Position& position, Point p, Stone color) const;

  // The node's AB/AW/AE/marks spelled out point by point, so single points can
  // be taken out of a rectangle.
  void expandList(sgf::Node& node, std::string_view id) const;

  sgf::Node* newChild(sgf::Node& parent, Stone color, Point p, size_t index = size_t(-1));

  void remember(const sgf::Node& node);

  sgf::Collection tree;
  sgf::Node*      cursor = nullptr;
  int             w      = 19;
  int             h      = 19;

  // Which child forward() goes to from each node: the one last come back from.
  std::unordered_map<const sgf::Node*, const sgf::Node*> lastVisited;

  std::vector<Snapshot> undoStack;
  std::vector<Snapshot> redoStack;

  // Which state of the record this is, for modified(): every edit makes a new
  // one, and undo goes back to an old one.
  uint64_t version      = 0;
  uint64_t savedVersion = 0;
  uint64_t versions     = 0;

  uint64_t changes     = 1;
  uint64_t treeChanges = 1;

  // The text property last typed into, so a burst of typing is one undo step.
  const sgf::Node* textNode = nullptr;
  std::string      textId;

  // position() and moveNumber() are worked out again only when revision()
  // has moved on since.
  mutable Position cached{ 19, 19 };
  mutable int      cachedNumber   = 0;
  mutable uint64_t cachedRevision = 0;
};
