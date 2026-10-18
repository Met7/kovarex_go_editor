// The game tree: every node of the record as a small button, moves left to
// right, the main line along the top and each variation branching down off
// the move it varies from. Click a node to go there; Shift or Ctrl and click
// to select moves, for the Variation group's edits to act on instead.
//
// Built from widgets like the rest -- a button per node, and the lines
// between them as stretched pictures -- on a canvas in a scroll pane. It is
// laid out again only when nodes come or go or the selection changes;
// stepping through the game just moves the highlight.

#pragma once

#include <game/Game.hpp>
#include <ui/GoSprites.hpp>

#include <Agui/GenericTargetable.hpp>
#include <Agui/Widget/Button.hpp>
#include <Agui/Widget/EmptyWidget.hpp>
#include <Agui/Widget/ScrollPane.hpp>

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ui {

class Theme;

class TreeView : public agui::GenericTargetable {
public:
  TreeView(Theme& theme, const GoSprites& sprites);
  TreeView(const TreeView&) = delete;
  TreeView& operator=(const TreeView&) = delete;

  agui::ScrollPane& widget() { return this->pane; }

  // The game whose tree to show, or null. Not owned.
  void show(Game* game);

  // Lays the tree out again if the record's shape changed, and keeps the
  // current node lit and in view. Cheap when nothing changed.
  void refresh();

  // The pane's size on screen.
  void setSize(int width, int height);

  // Whether each move's stone carries its number, as in CGoban: white on a
  // black stone, black on a white one. The nodes are bigger then, for three
  // digits to fit.
  void setNumbers(bool on);

  // The button standing for `node`, or null if it has none (yet).
  agui::Button* nodeButton(const sgf::Node& node) const
  {
    const auto it = this->buttons.find(&node);
    return it == this->buttons.end() ? nullptr : it->second;
  }

private:
  struct Placed {
    sgf::Node* node;
    int        column;
    int        row;
  };

  void rebuild();

  // A node's button, from one node to the next, and the stone in it.
  int nodePx() const;
  int pitch() const { return this->nodePx() + 8; }
  int stonePx() const { return this->nodePx() - 8; }
  // The middle of a grid cell of the tree.
  int centre(int cell) const;
  // Places `start` and the first-child line that follows it on the first
  // row free all the way along, then the variations off that line.
  void placeLine(sgf::Node* start, int column, int minRow);
  // How a node's button is drawn: blue if it is selected; otherwise red if
  // its move lands on a stone, lit if it is the current node, orange if it
  // put the stone the current move lands on.
  agui::ButtonStyle* nodeStyle(const sgf::Node& node) const;
  // The node's button drawn as nodeStyle() says, if it has one.
  void restyle(const sgf::Node* node);
  // A node clicked: Shift or Ctrl selects up to it, a plain click goes there.
  void clicked(sgf::Node& node);

  Theme&           theme;
  const GoSprites& sprites;

  agui::ScrollPane   pane;
  agui::EmptyWidget* canvas = nullptr;

  Game* game = nullptr;

  std::vector<Placed>                           placed;
  std::vector<int>                              nextFree;  // the first free row, per column
  std::unordered_map<const sgf::Node*, agui::Button*> buttons;
  // The nodes drawn with a comment's mark: drawn again when that changes.
  std::unordered_set<const sgf::Node*> commented;
  const sgf::Node*                              litNode   = nullptr;  // the current node, drawn lit
  const sgf::Node*                              underNode = nullptr;  // drawn deep orange

  uint64_t shownTree      = 0;
  uint64_t shownRevision  = 0;
  uint64_t shownSelection = 0;
  bool     numbers       = true;
};

}  // namespace ui
