#include <ui/TreeView.hpp>

#include <ui/Form.hpp>
#include <ui/Input.hpp>
#include <ui/Theme.hpp>

#include <Agui/Widget/ImageWidget.hpp>
#include <Agui/Widget/Label.hpp>

#include <algorithm>
#include <cstdlib>
#include <string>

namespace ui {

namespace {

// Round the whole tree, so the first node isn't against the pane's edge.
constexpr int PAD = 4;
// A node with its move number on it: big enough for three digits.
constexpr int NUMBERED_NODE_PX = 32;
// The + and - on a branch leaving the selection: small enough to sit on the
// line between two nodes.
constexpr int BRANCH_BUTTON_PX = 14;

void Pin(agui::Widget& widget, agui::Style& style, int w, int h)
{
  style.setMinimalWidth(w);
  style.setMaximalWidth(w);
  style.setMinimalHeight(h);
  style.setMaximalHeight(h);
  widget.setSize(w, h, agui::SetSizeInfo());
}

// Puts a node's stone, and its number, in the middle of its button. Children
// sit inside the button's padding, which includes the border of whatever it
// is drawn with -- and the current node's highlight has one where the others
// have none -- so this has to be done again whenever the button's style
// changes.
void CentreIcon(agui::Button& button)
{
  for (uint32_t i = 0; i < button.getChildCount(); ++i) {
    agui::Widget& child = *button.getChildAt(i);
    child.setLocation((button.getWidth() - child.getWidth()) / 2 - button.getLeftPadding(),
                      (button.getHeight() - child.getHeight()) / 2 - button.getTopPadding());
  }
}

}  // namespace

TreeView::TreeView(Theme& theme, const GoSprites& sprites)
    : theme(theme)
    , sprites(sprites)
{
  this->pane.setHScrollPolicy(agui::ScrollPolicy::Auto);
  this->pane.setVScrollPolicy(agui::ScrollPolicy::Auto);
  this->canvas = &make<agui::EmptyWidget>();
  this->pane.add(this->canvas);
}

void TreeView::show(Game* newGame)
{
  this->game      = newGame;
  this->shownTree = 0;
  this->refresh();
}

void TreeView::setSize(int width, int height)
{
  if (width == this->pane.getWidth() && height == this->pane.getHeight()) return;
  Pin(this->pane, this->pane.style, width, height);
}

void TreeView::setNumbers(bool on)
{
  if (on == this->numbers) return;
  this->numbers   = on;
  this->shownTree = 0;  // laid out again, at the other size
  this->refresh();
}

int TreeView::nodePx() const
{
  return this->numbers ? NUMBERED_NODE_PX : Theme::TREE_NODE_PX;
}

int TreeView::centre(int cell) const
{
  return PAD + cell * this->pitch() + this->pitch() / 2;
}

void TreeView::refresh()
{
  if (!this->game) {
    if (this->shownTree != 0) {
      this->canvas->clear();
      this->buttons.clear();
      this->lit       = nullptr;
      this->litNode   = nullptr;
      this->shownTree = 0;
    }
    return;
  }

  // The selection changes which nodes are blue and where the + and - go:
  // drawn again, like a change to the tree.
  if (this->shownTree != this->game->treeRevision() || this->shownSelection != this->game->selectionRevision()) {
    this->rebuild();
    this->shownTree      = this->game->treeRevision();
    this->shownSelection = this->game->selectionRevision();
    this->shownRevision  = 0;
  }

  if (this->shownRevision != this->game->revision()) {
    this->shownRevision = this->game->revision();
    const auto    it  = this->buttons.find(&this->game->current());
    agui::Button* now = it == this->buttons.end() ? nullptr : it->second;
    if (now != this->lit) {
      if (this->lit) {
        this->lit->style.setParent(this->nodeStyle(*this->litNode, false));
        CentreIcon(*this->lit);
      }
      if (now) {
        now->style.setParent(this->nodeStyle(this->game->current(), true));
        CentreIcon(*now);
      }
      this->lit     = now;
      this->litNode = now ? &this->game->current() : nullptr;
    }
    if (now) this->pane.scrollToMakeWidgetVisible(now, ScrollMode::InView);
    // A comment written or wiped here changes the node's mark, not the tree.
    const sgf::Node& current = this->game->current();
    if (current.get("C").empty() == this->commented.contains(&current)) {
      this->shownTree = 0;
      this->refresh();
      return;
    }
  }
}

agui::ButtonStyle* TreeView::nodeStyle(const sgf::Node& node, bool current) const
{
  if (this->game->isSelected(node)) return current ? &this->theme.treeNodeSelectedCurrent : &this->theme.treeNodeSelected;
  return current ? &this->theme.treeNodeCurrent : &this->theme.treeNode;
}

void TreeView::clicked(sgf::Node& node)
{
  // Shift: the moves from the current one to this, in place of what was
  // selected; Ctrl: those from the selection to this, added to it. Neither
  // goes anywhere. A plain click lets go of the selection and goes there.
  if (input::IsKeyDown(KEY_LEFT_SHIFT) || input::IsKeyDown(KEY_RIGHT_SHIFT)) {
    this->game->selectRange(node);
  } else if (input::IsKeyDown(KEY_LEFT_CONTROL) || input::IsKeyDown(KEY_RIGHT_CONTROL)) {
    this->game->extendSelection(node);
  } else {
    if (!this->game->selection().empty()) this->game->clearSelection();
    this->game->goTo(node);
  }
}

void TreeView::placeLine(sgf::Node* start, int column, int minRow)
{
  std::vector<sgf::Node*> line;
  for (sgf::Node* n = start;; n = &n->child(0)) {
    line.push_back(n);
    if (n->childCount() == 0) break;
  }

  // The row has to be free from the parent's column -- the branch's line
  // drops down that one -- to the end of this line.
  const int from = start->parent() ? column - 1 : column;
  const int to   = column + int(line.size()) - 1;
  if (int(this->nextFree.size()) <= to) this->nextFree.resize(size_t(to) + 1, 0);
  int row = minRow;
  for (int k = from; k <= to; ++k) row = std::max(row, this->nextFree[size_t(k)]);
  for (int k = from; k <= to; ++k) this->nextFree[size_t(k)] = row + 1;

  for (size_t i = 0; i < line.size(); ++i) this->placed.push_back({ line[i], column + int(i), row });

  // The variations, from the far end of the line back. A branch nearer the
  // end is placed first and takes the rows nearest the line; one from
  // further back drops past it and runs underneath, so no two lines cross.
  for (size_t i = line.size(); i-- > 0;) {
    for (size_t k = 1; k < line[i]->childCount(); ++k) this->placeLine(&line[i]->child(k), column + int(i) + 1, row + 1);
  }
}

void TreeView::rebuild()
{
  this->canvas->clear();
  this->buttons.clear();
  this->commented.clear();
  this->lit     = nullptr;
  this->litNode = nullptr;
  this->placed.clear();
  this->nextFree.clear();

  this->placeLine(&this->game->root(), 0, 0);

  std::unordered_map<const sgf::Node*, std::pair<int, int>> where;
  int columns = 1, rows = 1;
  for (const Placed& p : this->placed) {
    where[p.node] = { p.column, p.row };
    columns = std::max(columns, p.column + 1);
    rows    = std::max(rows, p.row + 1);
  }

  const int PITCH = this->pitch();
  const int NODE  = this->nodePx();
  const int STONE = this->stonePx();

  // Each move's number, counted as the board counts them: MN starts again
  // from its value. A line is placed after the move it branches from, so the
  // parent's number is always there.
  std::unordered_map<const sgf::Node*, int> numberOf;
  for (const Placed& p : this->placed) {
    int number = p.node->parent() ? numberOf[p.node->parent()] : 0;
    if (Game::MoveColor(*p.node) != Stone::None) {
      const std::string& mn = p.node->get("MN");
      number                = mn.empty() ? number + 1 : std::atoi(mn.c_str());
    }
    numberOf[p.node] = number;
  }

  // The lines first, so the nodes sit on top of them.
  const auto picture = [this](Sprite sprite, int x, int y, int w, int h) {
    agui::ImageWidget& image = make<agui::ImageWidget>(this->sprites.image(sprite), true);
    image.setIgnoredByInteraction(true);
    Pin(image, image.style, w, h);
    image.setLocation(x, y);
    *this->canvas << image;
  };
  for (const Placed& p : this->placed) {
    // On along the line to the next move.
    if (p.node->childCount() > 0) picture(Sprite::TreeHorizontal, this->centre(p.column), PAD + p.row * PITCH, PITCH, PITCH);

    // Down from the move a variation branches off, then across to it.
    if (p.node->parent() && p.node->indexInParent() > 0) {
      const auto [parentColumn, parentRow] = where[p.node->parent()];
      if (p.row - 1 > parentRow) {
        picture(Sprite::TreeVertical, PAD + parentColumn * PITCH, this->centre(parentRow), PITCH, (p.row - 1 - parentRow) * PITCH);
      }
      picture(Sprite::TreeDiagonal, this->centre(parentColumn), this->centre(p.row - 1), PITCH, PITCH);
    }
  }

  for (const Placed& p : this->placed) {
    agui::Button& button = make<agui::Button>(this->nodeStyle(*p.node, false));
    button.setFocusable(false);
    Pin(button, button.style, NODE, NODE);

    const Stone color = Game::MoveColor(*p.node);
    agui::ImageWidget& icon = make<agui::ImageWidget>(
        this->sprites.image(color == Stone::Black   ? Sprite::BlackStone
                            : color == Stone::White ? WhiteShell(unsigned(p.column) * 73856093u ^ unsigned(p.row) * 19349663u)
                                                    : Sprite::TreeSetup));
    icon.scaleToKeepTheRatio = true;
    icon.setIgnoredByInteraction(true);
    const bool pass = this->game->isPass(*p.node);
    if (pass) icon.opacity = 0.4;  // a pass: a move, but no stone
    button << icon;
    Pin(icon, icon.style, STONE, STONE);

    // A comment: a triangle, as CGoban has it -- in place of the number, or,
    // on the bare stones, smaller so it fits on one.
    const bool hasComment = !p.node->get("C").empty();
    if (hasComment) {
      const bool         onBlack = color == Stone::Black && !pass;
      agui::ImageWidget& mark    = make<agui::ImageWidget>(
          this->sprites.image(onBlack ? Sprite::TriangleLight : Sprite::TriangleDark));
      mark.scaleToKeepTheRatio = true;
      mark.setIgnoredByInteraction(true);
      // Bigger where it stands for a number; no bigger than a set-up node's square.
      const int size = this->numbers && color != Stone::None ? STONE * 5 / 4 : STONE;
      Pin(mark, mark.style, size, size);
      button << mark;
      this->commented.insert(p.node);
    } else if (this->numbers && color != Stone::None) {
      // A pass has no stone to be dark or light on: the number goes on the
      // background, like the dark one.
      const bool light = color == Stone::Black && !pass;
      agui::Label& number = agui::label(std::to_string(numberOf[p.node]), light ? &this->theme.treeNumberLight
                                                                                 : &this->theme.treeNumberDark);
      number.setIgnoredByInteraction(true);
      Pin(number, number.style, STONE, STONE);
      button << number;
    }
    CentreIcon(button);

    // A comment shows when the node is hovered, so commented moves can be found.
    if (p.node->has("C")) button.setToolTip(p.node->get("C").substr(0, 300));

    sgf::Node* node = p.node;
    button.onClick(this, [this, node] { this->clicked(*node); });
    const int offset = (PITCH - NODE) / 2;
    button.setLocation(PAD + p.column * PITCH + offset, PAD + p.row * PITCH + offset);
    *this->canvas << button;
    this->buttons[p.node] = &button;
  }

  // Where a branch leaves a selected move, on the line to it: a + to take the
  // branch in, all of it; or, where more than one branch from there is in, a -
  // on each to leave it out again. A line just going on is no branch.
  for (const Placed& p : this->placed) {
    const sgf::Node* from = p.node->parent();
    if (!from || from->childCount() < 2 || !this->game->isSelected(*from)) continue;
    const bool in = this->game->isSelected(*p.node);
    int inFrom = 0;
    for (size_t k = 0; k < from->childCount(); ++k) inFrom += this->game->isSelected(from->child(k)) ? 1 : 0;
    if (in && inFrom < 2) continue;

    const auto [fromColumn, fromRow] = where[from];
    // The middle of the line: across to the main line's next move, or the
    // diagonal down to a variation.
    const int x = this->centre(fromColumn) + PITCH / 2;
    const int y = p.node->indexInParent() == 0 ? this->centre(fromRow) : this->centre(p.row - 1) + PITCH / 2;
    agui::Button& toggle = agui::button(in ? "\xE2\x88\x92" : "+", nullptr, nullptr, &this->theme.treeBranchButton);
    toggle.setFocusable(false);
    toggle.setToolTip(in ? "Leave this branch out of the selection" : "Take this branch into the selection, all of it");
    Pin(toggle, toggle.style, BRANCH_BUTTON_PX, BRANCH_BUTTON_PX);
    toggle.setLocation(x - BRANCH_BUTTON_PX / 2, y - BRANCH_BUTTON_PX / 2);
    const sgf::Node* branch = p.node;
    toggle.onClick(this, [this, branch, in] { this->game->selectBranch(*branch, !in); });
    *this->canvas << toggle;
  }

  Pin(*this->canvas, this->canvas->style, columns * PITCH + 2 * PAD, rows * PITCH + 2 * PAD);
}

}  // namespace ui
