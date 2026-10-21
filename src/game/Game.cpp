#include <game/Game.hpp>

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <iterator>
#include <tuple>

namespace {

constexpr size_t MAX_UNDO = 500;

constexpr const char* SHAPES[]    = { "TR", "SQ", "CR", "MA", "SL" };
constexpr const char* POSITIONS[] = { "GB", "GW", "DM", "UC" };
constexpr const char* MOVES[]     = { "TE", "BM", "IT", "DO" };

const char* ColorName(Stone s)
{
  return s == Stone::Black ? "Black" : "White";
}

const char* MoveId(Stone s)
{
  return s == Stone::Black ? "B" : "W";
}

// SZ[19], or SZ[19:13] for a rectangular board.
std::pair<int, int> BoardSize(const sgf::Node& root)
{
  const std::string& sz = root.get("SZ");
  if (sz.empty()) return { 19, 19 };
  const auto number = [](std::string_view text, int fallback) {
    int v = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), v);
    return error == std::errc() ? std::clamp(v, 1, MAX_BOARD) : fallback;
  };
  const size_t colon = sz.find(':');
  if (colon == std::string::npos) {
    const int n = number(sz, 19);
    return { n, n };
  }
  return { number(std::string_view(sz).substr(0, colon), 19), number(std::string_view(sz).substr(colon + 1), 19) };
}

// A node's set-up, then its move, onto `position`. `number` is the move
// number so far, and comes back as this node's. False if the move landed on
// a stone -- which a record may say, but the editor will not make.
bool Apply(Position& position, const sgf::Node& node, int& number, int width, int height)
{
  for (Point p : PointList(node.values("AE"))) position.set(p, Stone::None);
  for (Point p : PointList(node.values("AB"))) position.set(p, Stone::Black, &node);
  for (Point p : PointList(node.values("AW"))) position.set(p, Stone::White, &node);

  const Stone color = Game::MoveColor(node);
  if (color == Stone::None) return true;

  // MN renumbers from this move on, as problem collections do.
  if (const std::string& mn = node.get("MN"); !mn.empty()) number = std::atoi(mn.c_str());
  else                                                     ++number;

  const std::string& value = node.get(MoveId(color));
  if (IsPass(value, width, height)) return true;
  const std::optional<Point> p = FromSgf(value);
  if (!p || !position.inside(*p)) return true;

  const bool occupied = position.at(*p) != Stone::None;
  position.play(*p, color, &node, number);
  return !occupied;
}

// The nodes from `a` to `b`, both included: up from `a` to where their lines
// part, and down to `b`.
std::vector<const sgf::Node*> Between(const sgf::Node& a, const sgf::Node& b)
{
  const std::vector<const sgf::Node*> up   = Game::Path(a);
  const std::vector<const sgf::Node*> down = Game::Path(b);
  size_t common = 0;  // both paths start at the root
  while (common < up.size() && common < down.size() && up[common] == down[common]) ++common;
  std::vector<const sgf::Node*> nodes(up.begin() + ptrdiff_t(common) - 1, up.end());
  nodes.insert(nodes.end(), down.begin() + ptrdiff_t(common), down.end());
  return nodes;
}

// A copy of `node` with only those of the nodes under it that are in `keep`.
std::unique_ptr<sgf::Node> CloneSelected(const sgf::Node& node, const std::unordered_set<const sgf::Node*>& keep)
{
  auto copy = std::make_unique<sgf::Node>();
  for (const sgf::Property& p : node.properties()) copy->setValues(p.id, p.values);
  for (size_t k = 0; k < node.childCount(); ++k) {
    if (keep.contains(&node.child(k))) copy->addChild(CloneSelected(node.child(k), keep));
  }
  return copy;
}

// The number of the move `node` makes, or of the last one before it if it
// makes none: counted as Apply() counts them.
int MoveNumberAt(const sgf::Node& node)
{
  int number = 0;
  for (const sgf::Node* n : Game::Path(node)) {
    if (Game::MoveColor(*n) == Stone::None) continue;
    const std::string& mn = n->get("MN");
    number = mn.empty() ? number + 1 : std::atoi(mn.c_str());
  }
  return number;
}

}  // namespace

const char* MarkProperty(Mark mark)
{
  switch (mark) {
  case Mark::Triangle:       return "TR";
  case Mark::Square:         return "SQ";
  case Mark::Circle:         return "CR";
  case Mark::Cross:          return "MA";
  case Mark::Selected:       return "SL";
  case Mark::TerritoryBlack: return "TB";
  case Mark::TerritoryWhite: return "TW";
  case Mark::Dim:            return "DD";
  }
  return "";
}

// ---------------------------------------------------------------- setup

Game::Game(const GameSetup& setup)
{
  auto root = std::make_unique<sgf::Node>();
  this->w = std::clamp(setup.width, 1, MAX_BOARD);
  this->h = std::clamp(setup.height, 1, MAX_BOARD);

  root->set("FF", "4");
  root->set("GM", "1");
  root->set("CA", "UTF-8");
  root->set("AP", "GoEditor:1.0");
  root->set("SZ", this->w == this->h ? std::to_string(this->w) : std::to_string(this->w) + ":" + std::to_string(this->h));
  if (!setup.komi.empty()) root->set("KM", setup.komi);
  if (!setup.black.empty()) root->set("PB", setup.black);
  if (!setup.white.empty()) root->set("PW", setup.white);
  if (!setup.date.empty()) root->set("DT", setup.date);

  if (setup.handicap >= 2) {
    root->set("HA", std::to_string(setup.handicap));
    std::vector<std::string> stones;
    for (Point p : HandicapPoints(setup.handicap, this->w, this->h)) stones.push_back(ToSgf(p));
    root->setValues("AB", std::move(stones));
  }

  this->cursor = root.get();
  this->tree.games.push_back(std::move(root));
}

Game::Game(sgf::Collection file)
    : tree(std::move(file))
{
  if (this->tree.games.empty()) this->tree.games.push_back(std::make_unique<sgf::Node>());
  const auto [width, height] = BoardSize(this->root());
  this->w      = width;
  this->h      = height;
  this->cursor = &this->root();
}

void Game::stampFormat()
{
  for (auto& game : this->tree.games) {
    game->set("CA", "UTF-8");
    if (std::atoi(game->get("FF").c_str()) < 4) game->set("FF", "4");
    if (!game->has("GM")) game->set("GM", "1");
  }
}

// ---------------------------------------------------------------- reading

Stone Game::MoveColor(const sgf::Node& node)
{
  if (node.has("B")) return Stone::Black;
  if (node.has("W")) return Stone::White;
  return Stone::None;
}

Point Game::movePoint(const sgf::Node& node) const
{
  const Stone color = MoveColor(node);
  if (color == Stone::None) return {};
  const std::string& value = node.get(MoveId(color));
  if (IsPass(value, this->w, this->h)) return {};
  const std::optional<Point> p = FromSgf(value);
  if (!p || p->x >= this->w || p->y >= this->h) return {};
  return *p;
}

bool Game::isPass(const sgf::Node& node) const
{
  const Stone color = MoveColor(node);
  return color != Stone::None && IsPass(node.get(MoveId(color)), this->w, this->h);
}

std::vector<const sgf::Node*> Game::Path(const sgf::Node& node)
{
  std::vector<const sgf::Node*> path;
  for (const sgf::Node* n = &node; n; n = n->parent()) path.push_back(n);
  std::reverse(path.begin(), path.end());
  return path;
}

const Position& Game::position() const
{
  if (this->cachedRevision != this->changes) {
    Position position(this->w, this->h);
    int number = 0;
    for (const sgf::Node* node : Path(*this->cursor)) Apply(position, *node, number, this->w, this->h);
    this->cached         = std::move(position);
    this->cachedNumber   = number;
    this->cachedRevision = this->changes;
  }
  return this->cached;
}

int Game::moveNumber() const
{
  this->position();
  return this->cachedNumber;
}

Stone Game::toPlay() const
{
  for (const sgf::Node* n = this->cursor; n; n = n->parent()) {
    if (const std::string& pl = n->get("PL"); !pl.empty()) return (pl[0] == 'W' || pl[0] == 'w') ? Stone::White : Stone::Black;
    const Stone moved = MoveColor(*n);
    if (moved != Stone::None) return Opponent(moved);
  }
  // Nothing played yet: Black starts, unless Black has handicap stones.
  return std::atoi(this->root().get("HA").c_str()) >= 2 ? Stone::White : Stone::Black;
}

Position Game::positionBefore(const sgf::Node& node, int* number) const
{
  Position position(this->w, this->h);
  int moves = 0;
  for (const sgf::Node* n : Path(node)) {
    if (n == &node) break;
    Apply(position, *n, moves, this->w, this->h);
  }
  if (number) *number = moves;
  return position;
}

std::vector<const sgf::Node*> Game::findStonesOnStones() const
{
  // Straight down each line in place; a copy of the board only where the
  // record branches, for the variations to start from.
  struct Branch {
    const sgf::Node* node;
    Position         position;
    int              number;
  };
  std::vector<const sgf::Node*> found;
  std::vector<Branch> todo{ { &this->root(), Position(this->w, this->h), 0 } };
  while (!todo.empty()) {
    Branch branch = std::move(todo.back());
    todo.pop_back();

    const sgf::Node* n = branch.node;
    while (true) {
      if (!Apply(branch.position, *n, branch.number, this->w, this->h)) found.push_back(n);
      if (n->childCount() == 0) break;
      for (size_t k = n->childCount(); k-- > 1;) todo.push_back({ &n->child(k), branch.position, branch.number });
      n = &n->child(0);
    }
  }
  return found;
}

const std::unordered_set<const sgf::Node*>& Game::stonesOnStones() const
{
  if (this->onStonesRevision != this->treeChanges) {
    const std::vector<const sgf::Node*> found = this->findStonesOnStones();
    this->onStones         = { found.begin(), found.end() };
    this->onStonesRevision = this->treeChanges;
  }
  return this->onStones;
}

std::optional<Game::StoneOnStone> Game::stoneOnStone(const sgf::Node& node) const
{
  if (!this->isStoneOnStone(node)) return std::nullopt;

  // The board as the move finds it: after the node's own set-up, if it has
  // any (FF[3] let a node have both).
  Position  board = this->positionBefore(node);
  sgf::Node setupOnly;
  for (const char* id : { "AE", "AB", "AW" }) setupOnly.setValues(id, node.values(id));
  int ignored = 0;
  Apply(board, setupOnly, ignored, this->w, this->h);

  StoneOnStone s;
  s.number      = MoveNumberAt(node);
  s.at          = this->movePoint(node);
  s.color       = MoveColor(node);
  s.under       = board.at(s.at);
  s.underNode   = board.origin(s.at);
  s.underIsMove = s.underNode && MoveColor(*s.underNode) != Stone::None;
  s.underNumber = s.underIsMove ? board.moveNumberAt(s.at) : s.underNode ? MoveNumberAt(*s.underNode) : 0;

  // Down the stack: the stone under it, and what that one lands on in turn.
  if (s.underNode) {
    s.stack.push_back(s.underNode);
    if (const std::optional<StoneOnStone> below = this->stoneOnStone(*s.underNode)) {
      s.stack.insert(s.stack.end(), below->stack.begin(), below->stack.end());
    }
  }
  return s;
}

std::string Game::stoneOnStoneReason(const sgf::Node& node) const
{
  const std::optional<StoneOnStone> s = this->stoneOnStone(node);
  if (!s) return {};

  const std::string point = DisplayName(s->at, this->w, this->h);
  std::string under;
  if (s->underIsMove) {
    under = "move " + std::to_string(s->underNumber) + " (" + ColorName(s->under) + " " + point + ")";
  } else {
    under = std::string("a ") + (s->under == Stone::Black ? "black" : "white") + " set-up stone (set up " +
            (s->underNumber == 0 ? std::string("at the start") : "after move " + std::to_string(s->underNumber)) + ")";
  }
  return "Move " + std::to_string(s->number) + " (" + ColorName(s->color) + " " + point + ") is played on top of " +
         under + ", which is still on the board.";
}

std::string Game::newStonesOnStones(const std::unordered_set<const sgf::Node*>& before) const
{
  // In the order they are met, so "the first" is the first along the record.
  std::vector<const sgf::Node*> fresh;
  for (const sgf::Node* n : this->findStonesOnStones()) {
    if (!before.contains(n)) fresh.push_back(n);
  }
  if (fresh.empty()) return {};

  const sgf::Node& first = *fresh.front();
  const std::string move = "move " + std::to_string(MoveNumberAt(first)) + " (" + ColorName(MoveColor(first)) + " " +
                           DisplayName(this->movePoint(first), this->w, this->h) + ")";
  if (fresh.size() == 1) return "Now " + move + " lands on a stone: it's red in the game tree.";
  return "Now " + std::to_string(fresh.size()) + " moves land on a stone, the first " + move +
         ": they're red in the game tree.";
}

std::string Game::moveProblem(const Position& position, Point p, Stone color) const
{
  if (!position.inside(p)) return "That point is off the board.";
  if (position.at(p) != Stone::None) return "There is already a stone there.";
  if (p == position.koPoint()) return "Ko: that stone can't be taken back straight away.";
  if (position.isSuicide(p, color)) return "That move would be suicide.";
  return {};
}

// ---------------------------------------------------------------- navigation

sgf::Node* Game::nextNode() const
{
  if (this->cursor->childCount() == 0) return nullptr;
  if (const auto it = this->lastVisited.find(this->cursor); it != this->lastVisited.end()) {
    for (size_t k = 0; k < this->cursor->childCount(); ++k) {
      if (&this->cursor->child(k) == it->second) return &this->cursor->child(k);
    }
  }
  return &this->cursor->child(0);
}

bool Game::forward()
{
  sgf::Node* next = this->nextNode();
  if (!next) return false;
  this->cursor = next;
  this->touched(false);
  return true;
}

bool Game::back()
{
  sgf::Node* parent = this->cursor->parent();
  if (!parent) return false;
  this->lastVisited[parent] = this->cursor;
  this->cursor = parent;
  this->touched(false);
  return true;
}

bool Game::toStart()
{
  bool moved = false;
  while (this->back()) moved = true;
  return moved;
}

bool Game::toEnd()
{
  bool moved = false;
  while (this->forward()) moved = true;
  return moved;
}

bool Game::step(int count)
{
  bool moved = false;
  for (int i = 0; i < count && this->forward(); ++i) moved = true;
  for (int i = 0; i > count && this->back(); --i) moved = true;
  return moved;
}

bool Game::nextVariation()
{
  sgf::Node* parent = this->cursor->parent();
  if (!parent) return false;
  const size_t next = size_t(this->cursor->indexInParent()) + 1;
  if (next >= parent->childCount()) return false;
  this->goTo(parent->child(next));
  return true;
}

bool Game::previousVariation()
{
  sgf::Node* parent = this->cursor->parent();
  if (!parent) return false;
  const int index = this->cursor->indexInParent();
  if (index <= 0) return false;
  this->goTo(parent->child(size_t(index - 1)));
  return true;
}

void Game::goTo(sgf::Node& node)
{
  this->cursor = &node;
  this->remember(node);
  this->touched(false);
}

void Game::goToPath(const std::vector<int>& path)
{
  this->goTo(*this->nodeAt(path));
}

bool Game::goToStone(Point p)
{
  const Position& now = this->position();
  if (!now.inside(p) || !now.origin(p)) return false;
  // The position only hands out read-only pointers; the node is the record's own.
  this->goTo(const_cast<sgf::Node&>(*now.origin(p)));
  return true;
}

// So that stepping forward from anywhere above `node` comes back down the
// line it is on, rather than the main line.
void Game::remember(const sgf::Node& node)
{
  for (const sgf::Node* n = &node; n->parent(); n = n->parent()) this->lastVisited[n->parent()] = n;
}

// ---------------------------------------------------------------- undo

std::vector<int> Game::cursorPath() const
{
  std::vector<int> path;
  for (const sgf::Node* n = this->cursor; n->parent(); n = n->parent()) path.push_back(n->indexInParent());
  std::reverse(path.begin(), path.end());
  return path;
}

sgf::Node* Game::nodeAt(const std::vector<int>& path) const
{
  sgf::Node* node = &this->root();
  for (int index : path) {
    if (index < 0 || size_t(index) >= node->childCount()) break;
    node = &node->child(size_t(index));
  }
  return node;
}

void Game::beginEdit(bool structural)
{
  this->undoStack.push_back({ this->tree.clone(), this->cursorPath(), this->version });
  if (this->undoStack.size() > MAX_UNDO) this->undoStack.erase(this->undoStack.begin());
  this->redoStack.clear();
  this->version  = ++this->versions;
  this->textNode = nullptr;
  this->touched(structural);
}

void Game::restore(Snapshot& snapshot)
{
  this->tree    = std::move(snapshot.tree);
  this->cursor  = this->nodeAt(snapshot.path);
  // Turning the board can have swapped a rectangular board's sides.
  std::tie(this->w, this->h) = BoardSize(this->root());
  this->version = snapshot.version;
  // The old nodes are gone, and the map would only point at their ghosts.
  this->lastVisited.clear();
  this->remember(*this->cursor);
  this->textNode = nullptr;
  this->touched(true);
}

void Game::touched(bool structural)
{
  ++this->changes;
  if (!structural) return;
  ++this->treeChanges;
  // The nodes may be gone, or moved: what was selected can't be told any more.
  if (!this->selected.empty()) this->clearSelection();
}

bool Game::undo()
{
  if (this->undoStack.empty()) return false;
  this->redoStack.push_back({ this->tree.clone(), this->cursorPath(), this->version });
  this->restore(this->undoStack.back());
  this->undoStack.pop_back();
  return true;
}

bool Game::redo()
{
  if (this->redoStack.empty()) return false;
  this->undoStack.push_back({ this->tree.clone(), this->cursorPath(), this->version });
  this->restore(this->redoStack.back());
  this->redoStack.pop_back();
  return true;
}

// ---------------------------------------------------------------- moves

sgf::Node* Game::newChild(sgf::Node& parent, Stone color, Point p, size_t index)
{
  auto node = std::make_unique<sgf::Node>();
  node->set(MoveId(color), p.valid() ? ToSgf(p) : std::string());
  return &parent.addChild(std::move(node), index);
}

Outcome Game::play(Point p)
{
  // Clicking the move that comes next just steps into it, as does any move
  // already recorded here -- a variation is only made when it is new.
  for (size_t k = 0; k < this->cursor->childCount(); ++k) {
    sgf::Node& child = this->cursor->child(k);
    if (MoveColor(child) != Stone::None && this->movePoint(child) == p) {
      this->goTo(child);
      return Outcome::Done();
    }
  }

  const Stone color = this->toPlay();
  if (std::string problem = this->moveProblem(this->position(), p, color); !problem.empty()) return Outcome::Fail(problem);

  this->beginEdit();
  sgf::Node* node = this->newChild(*this->cursor, color, p);
  this->goTo(*node);
  return Outcome::Done(node->parent()->childCount() > 1 ? "New variation." : "");
}

Outcome Game::pass()
{
  for (size_t k = 0; k < this->cursor->childCount(); ++k) {
    sgf::Node& child = this->cursor->child(k);
    if (this->isPass(child)) {
      this->goTo(child);
      return Outcome::Done();
    }
  }
  this->beginEdit();
  this->goTo(*this->newChild(*this->cursor, this->toPlay(), {}));
  return Outcome::Done();
}

Outcome Game::insertMove(Point p)
{
  if (this->cursor->childCount() == 0) return this->play(p);

  const Stone color = this->toPlay();
  const Position before = this->position();
  const int numberBefore = this->moveNumber();
  if (std::string problem = this->moveProblem(before, p, color); !problem.empty()) return Outcome::Fail(problem);

  const std::unordered_set<const sgf::Node*> onStonesBefore = this->stonesOnStones();
  this->beginEdit();
  // The new move takes over everything that came after this node -- every
  // variation, not just the main line, since they all follow on from here.
  auto inserted = std::make_unique<sgf::Node>();
  inserted->set(MoveId(color), ToSgf(p));
  while (this->cursor->childCount() > 0) inserted->addChild(this->cursor->detachChild(0));
  sgf::Node& node = this->cursor->addChild(std::move(inserted));

  this->goTo(node);
  std::string what = "Inserted " + std::string(ColorName(color)) + " " + DisplayName(p, this->w, this->h) +
                     " as move " + std::to_string(numberBefore + 1) + "; the moves after it follow on.";
  if (std::string news = this->newStonesOnStones(onStonesBefore); !news.empty()) what += " " + news;
  return Outcome::Done(what);
}

Outcome Game::moveStone(Point from, Point to)
{
  if (from == to) return Outcome::Done();
  const Position& now = this->position();
  if (!now.inside(from) || !now.inside(to)) return Outcome::Fail("That point is off the board.");

  const sgf::Node* origin = now.origin(from);
  if (!origin) return Outcome::Fail("There is no stone there to move.");
  // The record's own node: the position only hands out read-only pointers.
  sgf::Node& node = const_cast<sgf::Node&>(*origin);

  const Position before = this->positionBefore(node);
  const std::unordered_set<const sgf::Node*> onStonesBefore = this->stonesOnStones();

  const Stone color  = MoveColor(node);
  const bool  isMove = color != Stone::None && this->movePoint(node) == from;
  const std::string target = DisplayName(to, this->w, this->h);
  std::string what;

  if (isMove) {
    const int number = now.moveNumberAt(from);
    if (before.at(to) != Stone::None) {
      return Outcome::Fail("When move " + std::to_string(number) + " was played, " + target + " already had a stone on it.");
    }
    if (before.isSuicide(to, color)) {
      return Outcome::Fail("Move " + std::to_string(number) + " at " + target + " would have been suicide.");
    }
    this->beginEdit();
    node.set(MoveId(color), ToSgf(to));
    what = "Move " + std::to_string(number) + " is now at " + target + ".";
  } else {
    // A set-up stone: it moves within the node that set it up.
    const Stone stone = now.at(from);
    Position afterSetup = before;
    int ignored = 0;
    sgf::Node setupOnly;
    for (const char* id : { "AE", "AB", "AW" }) setupOnly.setValues(id, node.values(id));
    Apply(afterSetup, setupOnly, ignored, this->w, this->h);
    if (afterSetup.at(to) != Stone::None) return Outcome::Fail(target + " already has a stone on it.");

    this->beginEdit();
    const char* id = stone == Stone::Black ? "AB" : "AW";
    this->expandList(node, id);
    this->expandList(node, "AE");
    node.removeValue(id, ToSgf(from));
    node.removeValue("AE", ToSgf(to));
    node.addValue(id, ToSgf(to));
    what = "The set-up stone is now at " + target + ".";
  }

  this->touched(true);
  if (std::string news = this->newStonesOnStones(onStonesBefore); !news.empty()) what += " " + news;
  return Outcome::Done(what);
}

Outcome Game::deleteBranch()
{
  sgf::Node* parent = this->cursor->parent();
  if (!parent) return Outcome::Fail("The start of the game can't be deleted.");

  this->beginEdit();
  const int index = this->cursor->indexInParent();
  this->cursor = parent;
  parent->detachChild(size_t(index));
  this->lastVisited.erase(parent);
  this->touched(true);
  return Outcome::Done();
}

Outcome Game::promoteToMainLine()
{
  bool already = true;
  for (const sgf::Node* n = this->cursor; n->parent(); n = n->parent()) {
    if (n->indexInParent() != 0) already = false;
  }
  if (already) return Outcome::Done("This is already the main line.");

  this->beginEdit();
  for (sgf::Node* n = this->cursor; n->parent();) {
    sgf::Node* parent = n->parent();
    const int  index  = n->indexInParent();
    if (index > 0) parent->addChild(parent->detachChild(size_t(index)), 0);
    n = parent;
  }
  this->touched(true);
  return Outcome::Done("This line is now the main line.");
}

// ---------------------------------------------------------------- the selection

void Game::selectRange(const sgf::Node& node)
{
  this->selected.clear();
  for (const sgf::Node* n : Between(*this->cursor, node)) this->selected.insert(n);
  ++this->selectionChanges;
}

void Game::extendSelection(const sgf::Node& node)
{
  if (this->selected.empty()) return this->selectRange(node);
  // From the selected move nearest to it, so the selection stays in one piece.
  std::vector<const sgf::Node*> shortest;
  for (const sgf::Node* from : this->selected) {
    std::vector<const sgf::Node*> way = Between(*from, node);
    if (shortest.empty() || way.size() < shortest.size()) shortest = std::move(way);
  }
  this->selected.insert(shortest.begin(), shortest.end());
  ++this->selectionChanges;
}

void Game::selectBranch(const sgf::Node& start, bool on)
{
  std::vector<const sgf::Node*> todo{ &start };
  while (!todo.empty()) {
    const sgf::Node* n = todo.back();
    todo.pop_back();
    if (on) this->selected.insert(n);
    else    this->selected.erase(n);
    for (size_t k = 0; k < n->childCount(); ++k) todo.push_back(&n->child(k));
  }
  ++this->selectionChanges;
}

void Game::clearSelection()
{
  this->selected.clear();
  ++this->selectionChanges;
}

const sgf::Node* Game::selectionTop() const
{
  for (const sgf::Node* n : this->selected) {
    if (!n->parent() || !this->selected.contains(n->parent())) return n;
  }
  return nullptr;
}

std::string Game::copySelection() const
{
  const sgf::Node* top = this->selectionTop();
  if (!top) return {};
  sgf::Collection copy;
  copy.games.push_back(CloneSelected(*top, this->selected));
  return sgf::Write(copy);
}

sgf::Node* Game::selectionRest(const sgf::Node& top) const
{
  // Where the selection stops part way along a line -- a selected move whose
  // one next move isn't -- the moves after it stay. Of several such lines,
  // the first: down the main line before the variations.
  std::vector<const sgf::Node*> todo{ &top };
  while (!todo.empty()) {
    const sgf::Node* n = todo.back();
    todo.pop_back();
    if (n->childCount() == 1 && !this->isSelected(n->child(0))) return const_cast<sgf::Node*>(&n->child(0));
    for (size_t k = n->childCount(); k-- > 0;) {
      if (this->isSelected(n->child(k))) todo.push_back(&n->child(k));
    }
  }
  return nullptr;
}

std::vector<std::vector<int>> Game::selectionPaths() const
{
  std::vector<std::vector<int>> paths;
  for (const sgf::Node* n : this->selected) {
    std::vector<int> path;
    for (; n->parent(); n = n->parent()) path.push_back(n->indexInParent());
    std::reverse(path.begin(), path.end());
    paths.push_back(std::move(path));
  }
  return paths;
}

void Game::reselect(const std::vector<std::vector<int>>& paths)
{
  this->selected.clear();
  for (const std::vector<int>& path : paths) this->selected.insert(this->nodeAt(path));
  ++this->selectionChanges;
}

Outcome Game::deleteSelection()
{
  const sgf::Node* top = this->selectionTop();
  if (!top) return Outcome::Fail("Nothing is selected: Shift or Ctrl and click the game tree to select moves.");
  sgf::Node* parent = top->parent();
  if (!parent) return Outcome::Fail("The start of the game can't be deleted.");

  sgf::Node* rest = this->selectionRest(*top);

  // The editor stays where it is if that stays, and goes to where the
  // selection started if not.
  const auto under = [](const sgf::Node* n, const sgf::Node* ancestor) {
    for (; n; n = n->parent()) {
      if (n == ancestor) return true;
    }
    return false;
  };
  const bool cursorGoes = under(this->cursor, top) && !(rest && under(this->cursor, rest));
  const size_t count    = this->selected.size();
  // What stays may then land on a stone: kept all the same, and said.
  const std::unordered_set<const sgf::Node*> onStonesBefore = this->stonesOnStones();

  this->beginEdit();
  const size_t index = size_t(top->indexInParent());
  std::unique_ptr<sgf::Node> removed = parent->detachChild(index);
  if (rest) parent->addChild(rest->parent()->detachChild(size_t(rest->indexInParent())), index);
  if (cursorGoes) this->cursor = parent;
  this->lastVisited.clear();
  this->remember(*this->cursor);
  this->touched(true);

  const std::string what = count == 1 ? "Deleted the selected move" : "Deleted the " + std::to_string(count) + " selected moves";
  const std::string news = this->newStonesOnStones(onStonesBefore);
  return Outcome::Done(what + (rest ? "; the moves after them follow on from where they started." : ".") + (news.empty() ? "" : " " + news));
}

Outcome Game::moveSelection(sgf::Node& target)
{
  const sgf::Node* top = this->selectionTop();
  if (!top) return Outcome::Fail("Nothing is selected: Shift or Ctrl and click the game tree to select moves.");
  if (!top->parent()) return Outcome::Fail("The start of the game can't be moved.");
  if (this->isSelected(target)) return Outcome::Fail("Drop them on a move that is not in the selection.");
  if (top->parent() == &target) return Outcome::Fail("They already follow on from that move.");
  // Below the selection, only the moves that deleting it keeps stay to drop on.
  const auto under = [](const sgf::Node* n, const sgf::Node* ancestor) {
    for (; n; n = n->parent()) {
      if (n == ancestor) return true;
    }
    return false;
  };
  const sgf::Node* rest = this->selectionRest(*top);
  if (under(&target, top) && !(rest && under(&target, rest))) return Outcome::Fail("That move goes with them: drop them somewhere else.");

  // Moves that land on a stone where they go are moved all the same, and said.
  const std::unordered_set<const sgf::Node*> onStonesBefore = this->stonesOnStones();
  const std::string                          moved          = this->copySelection();
  const size_t                               count          = this->selected.size();
  const Outcome                              taken          = this->deleteSelection();
  if (!taken.ok) return taken;
  std::vector<const sgf::Node*> there;  // to tell whether they merged into one of these
  for (size_t k = 0; k < target.childCount(); ++k) there.push_back(&target.child(k));
  this->cursor      = &target;
  const Outcome put = this->pasteBranch(moved);
  if (!put.ok) {
    // Nothing a copy makes is refused, but if it were: the cut goes back too.
    this->undo();
    this->redoStack.pop_back();
    return put;
  }
  // One edit to undo: the tree as it was before the cut, not between the two.
  this->undoStack.pop_back();

  const bool        merged = std::find(there.begin(), there.end(), this->cursor) != there.end();
  const std::string what   = count == 1 ? "Moved the move" : "Moved the " + std::to_string(count) + " moves";
  const std::string news   = this->newStonesOnStones(onStonesBefore);
  return Outcome::Done(what +
                       (!merged ? "." : count == 1 ? "; it merged into the same move already there." : "; they merged into the moves already there.") +
                       (news.empty() ? "" : " " + news));
}

Outcome Game::moveBranch(const sgf::Node& start, sgf::Node& target)
{
  const std::vector<std::vector<int>> paths = this->selectionPaths();
  this->selected.clear();
  this->selectBranch(start, true);
  const Outcome outcome = this->moveSelection(target);
  if (!outcome.ok) this->reselect(paths);
  return outcome;
}

// ---------------------------------------------------------------- turning the board

Outcome Game::transform(BoardTransform how)
{
  const int  cols  = this->w;
  const int  rows  = this->h;
  const bool turns = how == BoardTransform::RotateLeft || how == BoardTransform::RotateRight;

  const auto map = [how, cols, rows](Point p) -> Point {
    // Anything off the board -- a sloppy file's -- is left where it is.
    if (p.x < 0 || p.y < 0 || p.x >= cols || p.y >= rows) return p;
    switch (how) {
    case BoardTransform::RotateLeft:     return { p.y, cols - 1 - p.x };
    case BoardTransform::RotateRight:    return { rows - 1 - p.y, p.x };
    case BoardTransform::FlipHorizontal: return { cols - 1 - p.x, p.y };
    case BoardTransform::FlipVertical:   return { p.x, rows - 1 - p.y };
    }
    return p;
  };
  // A point, as SGF writes it; anything that isn't one stays as it is.
  const auto point = [&map](std::string_view text) {
    const std::optional<Point> p = FromSgf(text);
    return p ? ToSgf(map(*p)) : std::string(text);
  };
  // FF[4]'s compressed rectangle, "aa:cc": its corners turned are still two
  // opposite corners, but maybe not the top-left and bottom-right.
  const auto rectangle = [&map](std::string_view text) {
    const size_t               colon = text.find(':');
    const std::optional<Point> a     = FromSgf(text.substr(0, colon));
    const std::optional<Point> b     = FromSgf(text.substr(colon + 1));
    if (!a || !b) return std::string(text);
    const Point ma = map(*a), mb = map(*b);
    return ToSgf({ std::min(ma.x, mb.x), std::min(ma.y, mb.y) }) + ":" +
           ToSgf({ std::max(ma.x, mb.x), std::max(ma.y, mb.y) });
  };

  this->beginEdit();
  std::vector<sgf::Node*> pending{ &this->root() };
  while (!pending.empty()) {
    sgf::Node& node = *pending.back();
    pending.pop_back();
    for (size_t i = 0; i < node.childCount(); ++i) pending.push_back(&node.child(i));

    std::vector<std::string> ids;
    for (const sgf::Property& property : node.properties()) ids.push_back(property.id);
    for (const std::string& id : ids) {
      std::vector<std::string> values = node.values(id);
      const bool move  = id == "B" || id == "W";
      const bool list  = id == "AB" || id == "AW" || id == "AE" || id == "TR" || id == "SQ" || id == "CR" ||
                         id == "MA" || id == "SL" || id == "TB" || id == "TW" || id == "DD" || id == "VW";
      const bool label = id == "LB";
      const bool line  = id == "AR" || id == "LN";
      if (!move && !list && !label && !line) continue;
      for (std::string& value : values) {
        const size_t colon = value.find(':');
        if (move) {
          if (!IsPass(value, cols, rows)) value = point(value);
        } else if (list) {
          value = colon == std::string::npos ? point(value) : rectangle(value);
        } else if (colon != std::string::npos) {
          // LB[point:text], AR[from:to] and LN[from:to].
          value = point(std::string_view(value).substr(0, colon)) + ":" +
                  (label ? value.substr(colon + 1) : point(std::string_view(value).substr(colon + 1)));
        }
      }
      node.setValues(id, std::move(values));
    }
  }

  if (turns && cols != rows) {
    this->w = rows;
    this->h = cols;
    this->root().set("SZ", std::to_string(this->w) + ":" + std::to_string(this->h));
  }

  switch (how) {
  case BoardTransform::RotateLeft:     return Outcome::Done("Rotated the board 90 degrees left.");
  case BoardTransform::RotateRight:    return Outcome::Done("Rotated the board 90 degrees right.");
  case BoardTransform::FlipHorizontal: return Outcome::Done("Flipped the board horizontally.");
  case BoardTransform::FlipVertical:   return Outcome::Done("Flipped the board vertically.");
  }
  return Outcome::Done();
}

// ---------------------------------------------------------------- sharing

void Game::replace(sgf::Collection file, const std::vector<int>& path)
{
  if (file.games.empty()) return;
  this->beginEdit();
  this->tree = std::move(file);
  std::tie(this->w, this->h) = BoardSize(this->root());
  // The old nodes are gone, and the map would only point at their ghosts.
  this->lastVisited.clear();
  this->cursor = this->nodeAt(path);
  this->remember(*this->cursor);
}

// ---------------------------------------------------------------- the clipboard

std::string Game::copyBranch() const
{
  sgf::Collection branch;
  branch.games.push_back(this->cursor->clone());
  return sgf::Write(branch);
}

Outcome Game::pasteBranch(std::string_view text)
{
  std::optional<sgf::Collection> pasted = sgf::Parse(text);
  if (!pasted || pasted->games.empty()) return Outcome::Fail("There is no game record on the clipboard to paste.");
  std::unique_ptr<sgf::Node> top = std::move(pasted->games.front());

  // A whole game starts with its root. The board has to be this one's size,
  // and what the root says about the game -- players, komi, the format --
  // stays behind; its set-up, if any, comes along.
  if (const std::string& size = top->get("SZ"); !size.empty()) {
    const size_t colon = size.find(':');
    const int    sizeW = std::atoi(size.c_str());
    const int    sizeH = colon == std::string::npos ? sizeW : std::atoi(size.c_str() + colon + 1);
    if (sizeW != this->w || sizeH != this->h) {
      return Outcome::Fail("That record is for a " + std::to_string(sizeW) + "x" + std::to_string(sizeH) + " board, not this one.");
    }
  }
  static constexpr const char* ABOUT_THE_GAME[] = {
    "AP", "CA", "FF", "GM", "ST", "SZ",                                            // root
    "AN", "BR", "BT", "CP", "DT", "EV", "GC", "GN", "HA", "KM", "ON", "OT", "PB",  // game info
    "PC", "PW", "RE", "RO", "RU", "SO", "TM", "US", "WR", "WT",
  };
  for (const char* id : ABOUT_THE_GAME) top->remove(id);

  // A root with nothing left in it is only where the moves hang: each line
  // under it becomes a variation of its own.
  std::vector<std::unique_ptr<sgf::Node>> branches;
  if (top->properties().empty()) {
    while (top->childCount() > 0) branches.push_back(top->detachChild(0));
  } else {
    branches.push_back(std::move(top));
  }
  if (branches.empty()) return Outcome::Fail("There are no moves on the clipboard to paste.");

  const std::unordered_set<const sgf::Node*> onStonesBefore = this->stonesOnStones();
  this->beginEdit();
  // They go where they belong: into the variations already here that they
  // follow, as a move is never recorded twice under one parent. Moves that
  // land on a stone are pasted all the same, as a file that has them opens:
  // put right on the board rather than refused.
  bool merges = false;
  for (const std::unique_ptr<sgf::Node>& branch : branches) {
    for (size_t k = 0; k < this->cursor->childCount(); ++k) merges = merges || this->sameNode(this->cursor->child(k), *branch);
  }
  const size_t count = branches.size();
  sgf::Node*   first = &this->mergeChild(*this->cursor, std::move(branches.front()));
  for (size_t i = 1; i < count; ++i) this->mergeChild(*this->cursor, std::move(branches[i]));

  this->cursor = first;
  this->remember(*this->cursor);
  this->touched(true);
  const std::string news  = this->newStonesOnStones(onStonesBefore);
  const std::string after = news.empty() ? "" : " " + news;
  if (count > 1) return Outcome::Done("Pasted " + std::to_string(count) + " variations; this is the first." + after);
  if (merges) return Outcome::Done("Merged into the variation that was already here." + after);
  return Outcome::Done(news.empty() ? std::string() : "Pasted as a new variation. " + news);
}

bool Game::sameNode(const sgf::Node& a, const sgf::Node& b) const
{
  const Stone color = MoveColor(a);
  if (color != MoveColor(b)) return false;
  if (color == Stone::None) {
    const std::vector<sgf::Property>& pa = a.properties();
    const std::vector<sgf::Property>& pb = b.properties();
    return pa.size() == pb.size() && std::equal(pa.begin(), pa.end(), pb.begin(), [](const sgf::Property& x, const sgf::Property& y) {
             return x.id == y.id && x.values == y.values;
           });
  }
  return this->isPass(a) == this->isPass(b) && this->movePoint(a) == this->movePoint(b);
}

sgf::Node& Game::mergeChild(sgf::Node& parent, std::unique_ptr<sgf::Node> node)
{
  sgf::Node* same = nullptr;
  for (size_t k = 0; k < parent.childCount() && !same; ++k) {
    if (this->sameNode(parent.child(k), *node)) same = &parent.child(k);
  }
  if (!same) return parent.addChild(std::move(node));

  for (const sgf::Property& property : node->properties()) {
    if (!same->has(property.id)) {
      same->setValues(property.id, property.values);
    } else if (property.id == "C") {
      const std::string& mine   = same->get("C");
      const std::string& theirs = property.values.empty() ? std::string() : property.values.front();
      if (!theirs.empty() && mine.find(theirs) == std::string::npos) same->set("C", mine + "\n\n" + theirs);
    }
  }
  while (node->childCount() > 0) this->mergeChild(*same, node->detachChild(0));
  return *same;
}

// ---------------------------------------------------------------- set-up and marks

void Game::expandList(sgf::Node& node, std::string_view id) const
{
  const std::vector<std::string>& values = node.values(id);
  if (std::none_of(values.begin(), values.end(), [](const std::string& v) { return v.find(':') != std::string::npos; })) return;
  std::vector<std::string> spelled;
  for (Point p : PointList(values)) spelled.push_back(ToSgf(p));
  node.setValues(id, std::move(spelled));
}

sgf::Node& Game::setupNode()
{
  if (MoveColor(*this->cursor) == Stone::None) return *this->cursor;
  // FF[4] keeps set-up and moves in separate nodes.
  sgf::Node* node = &this->cursor->addChild(std::make_unique<sgf::Node>());
  this->cursor = node;
  this->remember(*node);
  return *node;
}

Outcome Game::setupStone(Point p, Stone color)
{
  const Position now = this->position();  // a copy: the edit changes the position
  if (!now.inside(p)) return Outcome::Fail("That point is off the board.");
  const Stone there = now.at(p);
  if (color == Stone::None && there == Stone::None) return Outcome::Done();

  // The position the edit makes. A stone of the colour already there comes
  // off; any other goes down as a move there would, so a group it leaves
  // without a liberty is taken -- a position can't have one.
  const bool clearing = color == Stone::None || there == color;
  Position   after    = now;
  if (clearing) after.set(p, Stone::None);
  else          after.play(p, color);
  // A stone that would itself be taken changes nothing: no edit to undo.
  bool differs = false;
  for (int y = 0; y < now.height() && !differs; ++y) {
    for (int x = 0; x < now.width() && !differs; ++x) differs = after.at({ x, y }) != now.at({ x, y });
  }
  if (!differs) return Outcome::Fail("That stone would have no liberty: it would be taken at once.");

  // On a set-up node with moves after it, a stone added can be in a later
  // move's way.
  const std::unordered_set<const sgf::Node*> onStonesBefore = this->stonesOnStones();
  this->beginEdit();
  this->writeSetup(now, after);
  this->touched(true);
  return Outcome::Done(this->newStonesOnStones(onStonesBefore));
}

Outcome Game::clearSetupStones(Point a, Point b)
{
  // A set-up stone is one whose node is no move: the start's, or a set-up's.
  const Position now   = this->position();  // a copy: the edit changes the position
  Position       after = now;
  int            count = 0;
  for (int y = std::max(0, std::min(a.y, b.y)); y <= std::min(now.height() - 1, std::max(a.y, b.y)); ++y) {
    for (int x = std::max(0, std::min(a.x, b.x)); x <= std::min(now.width() - 1, std::max(a.x, b.x)); ++x) {
      const Point q{ x, y };
      if (now.at(q) == Stone::None || !now.origin(q) || MoveColor(*now.origin(q)) != Stone::None) continue;
      after.set(q, Stone::None);
      ++count;
    }
  }
  if (count == 0) return Outcome::Fail("There are no set-up stones to clear.");

  const std::unordered_set<const sgf::Node*> onStonesBefore = this->stonesOnStones();
  this->beginEdit();
  this->writeSetup(now, after);
  this->touched(true);
  return Outcome::Done(this->newStonesOnStones(onStonesBefore));
}

void Game::writeSetup(const Position& now, const Position& after)
{
  sgf::Node& node = this->setupNode();
  for (const char* id : { "AB", "AW", "AE" }) this->expandList(node, id);

  // Every point that differs, written into the set-up node.
  for (int y = 0; y < now.height(); ++y) {
    for (int x = 0; x < now.width(); ++x) {
      const Point q{ x, y };
      const Stone s = after.at(q);
      if (s == now.at(q)) continue;
      const std::string v = ToSgf(q);
      node.removeValue("AB", v);
      node.removeValue("AW", v);
      node.removeValue("AE", v);
      if (s != Stone::None) {
        node.addValue(s == Stone::Black ? "AB" : "AW", v);
      } else if (now.origin(q) != &node) {
        // Taking out of this node's own list was enough for a stone it set
        // up; one that was there before this node needs clearing.
        node.addValue("AE", v);
      }
    }
  }
}

void Game::toggleMark(Point p, Mark mark)
{
  this->setMark(p, mark, !this->hasMark(p, mark), false);
}

bool Game::hasMark(Point p, Mark mark) const
{
  if (!this->position().inside(p)) return false;
  const std::vector<Point> marked = PointList(this->cursor->values(MarkProperty(mark)));
  return std::find(marked.begin(), marked.end(), p) != marked.end();
}

void Game::setMark(Point p, Mark mark, bool on, bool sameEdit)
{
  if (!this->position().inside(p) || this->hasMark(p, mark) == on) return;
  if (!sameEdit || this->undoStack.empty()) this->beginEdit(false);
  sgf::Node&        node = *this->cursor;
  const char*       id   = MarkProperty(mark);
  const std::string v    = ToSgf(p);

  this->expandList(node, id);
  if (!on) {
    node.removeValue(id, v);
    this->touched(false);
    return;
  }

  const bool shape = std::any_of(std::begin(SHAPES), std::end(SHAPES), [id](const char* s) { return std::string_view(s) == id; });
  if (shape) {
    // One shape to a point, and no label under it.
    for (const char* other : SHAPES) {
      this->expandList(node, other);
      node.removeValue(other, v);
    }
    std::vector<std::string> labels = node.values("LB");
    std::erase_if(labels, [&v](const std::string& l) { return l.rfind(v + ":", 0) == 0; });
    node.setValues("LB", std::move(labels));
  }
  if (mark == Mark::TerritoryBlack || mark == Mark::TerritoryWhite) {
    const char* other = mark == Mark::TerritoryBlack ? "TW" : "TB";
    this->expandList(node, other);
    node.removeValue(other, v);
  }
  node.addValue(id, v);
  this->touched(false);
}

void Game::toggleLabel(Point p, const std::string& text)
{
  if (!this->position().inside(p) || text.empty()) return;
  this->beginEdit(false);
  sgf::Node&        node   = *this->cursor;
  const std::string prefix = ToSgf(p) + ":";

  std::vector<std::string> labels = node.values("LB");
  const bool same = std::find(labels.begin(), labels.end(), prefix + text) != labels.end();
  std::erase_if(labels, [&prefix](const std::string& l) { return l.rfind(prefix, 0) == 0; });
  if (!same) {
    labels.push_back(prefix + text);
    for (const char* shape : SHAPES) {
      this->expandList(node, shape);
      node.removeValue(shape, ToSgf(p));
    }
  }
  node.setValues("LB", std::move(labels));
  this->touched(false);
}

void Game::clearLabels(Point a, Point b)
{
  const auto inBox = [a, b](const std::string& label) {
    const std::optional<Point> p = label.size() >= 2 ? FromSgf(label.substr(0, 2)) : std::nullopt;
    return p && p->x >= std::min(a.x, b.x) && p->x <= std::max(a.x, b.x) && p->y >= std::min(a.y, b.y) && p->y <= std::max(a.y, b.y);
  };
  std::vector<std::string> labels = this->cursor->values("LB");
  if (std::none_of(labels.begin(), labels.end(), inBox)) return;
  this->beginEdit(false);
  std::erase_if(labels, inBox);
  if (labels.empty()) this->cursor->remove("LB");
  else                this->cursor->setValues("LB", std::move(labels));
  this->touched(false);
}

std::string Game::nextLetter() const
{
  std::vector<std::string> used;
  for (const std::string& l : this->cursor->values("LB")) {
    if (l.size() > 3) used.push_back(l.substr(3));
  }
  for (const char* range : { "ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz" }) {
    for (const char* c = range; *c; ++c) {
      const std::string letter(1, *c);
      if (std::find(used.begin(), used.end(), letter) == used.end()) return letter;
    }
  }
  return "?";
}

std::string Game::nextNumber() const
{
  std::vector<std::string> used;
  for (const std::string& l : this->cursor->values("LB")) {
    if (l.size() > 3) used.push_back(l.substr(3));
  }
  for (int n = 1;; ++n) {
    const std::string number = std::to_string(n);
    if (std::find(used.begin(), used.end(), number) == used.end()) return number;
  }
}

void Game::toggleLine(Point from, Point to, bool arrow)
{
  if (from == to || !this->position().inside(from) || !this->position().inside(to)) return;
  this->beginEdit(false);
  sgf::Node&        node     = *this->cursor;
  const char*       id       = arrow ? "AR" : "LN";
  const char*       other    = arrow ? "LN" : "AR";
  const std::string forward  = ToSgf(from) + ":" + ToSgf(to);
  const std::string backward = ToSgf(to) + ":" + ToSgf(from);

  // A line has no direction, so either way round is the same one.
  if (node.removeValue(id, forward) || (!arrow && node.removeValue(id, backward))) {
    this->touched(false);
    return;
  }
  // SGF allows a pair of points one arrow or one line, not both.
  node.removeValue(other, forward);
  node.removeValue(other, backward);
  node.addValue(id, forward);
  this->touched(false);
}

// ---------------------------------------------------------------- text and annotations

void Game::setNodeText(std::string_view id, std::string text)
{
  sgf::Node& node = *this->cursor;
  if (text.empty() ? !node.has(id) : node.get(id) == text) return;

  // Every keystroke calls this; only the first of a burst is an undo step.
  const bool burst = this->textNode == this->cursor && this->textId == id && !this->undoStack.empty();
  if (burst) {
    this->redoStack.clear();
    this->version = ++this->versions;
  } else {
    this->beginEdit(false);
    this->textNode = this->cursor;
    this->textId   = std::string(id);
  }

  if (text.empty()) node.remove(id);
  else              node.set(id, std::move(text));
  this->touched(false);
}

void Game::setPositionAnnotation(std::string_view id)
{
  if (this->positionAnnotation() == id) return;
  this->beginEdit(false);
  for (const char* a : POSITIONS) this->cursor->remove(a);
  if (!id.empty()) this->cursor->set(id, "1");
  this->touched(false);
}

void Game::setMoveAnnotation(std::string_view id)
{
  if (this->moveAnnotation() == id) return;
  this->beginEdit(false);
  for (const char* a : MOVES) this->cursor->remove(a);
  // TE and BM carry how strongly they are meant; IT and DO carry nothing.
  if (id == "TE" || id == "BM") this->cursor->set(id, "1");
  else if (!id.empty())         this->cursor->set(id, "");
  this->touched(false);
}

std::string Game::positionAnnotation() const
{
  for (const char* a : POSITIONS) {
    if (this->cursor->has(a)) return a;
  }
  return {};
}

std::string Game::moveAnnotation() const
{
  for (const char* a : MOVES) {
    if (this->cursor->has(a)) return a;
  }
  return {};
}

void Game::setGameInfo(const std::vector<std::pair<std::string, std::string>>& values)
{
  const sgf::Node& root = this->root();
  const bool differs = std::any_of(values.begin(), values.end(), [&root](const auto& entry) {
    const auto& [id, value] = entry;
    return value.empty() ? root.has(id) : root.get(id) != value;
  });
  if (!differs) return;

  this->beginEdit(false);
  for (const auto& [id, value] : values) {
    if (value.empty()) this->root().remove(id);
    else               this->root().set(id, value);
  }
  this->touched(false);
}
