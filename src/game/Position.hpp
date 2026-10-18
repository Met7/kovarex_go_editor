// The rules of Go, as far as an editor needs them: a board of stones, what
// a move captures, and how a record's moves and set-up stones add up to the
// position at any point of it. No window, no input, no drawing.
//
// The rules are the lenient ones SGF itself allows: a record may contain a
// suicide (the group comes off the board) or a move onto an occupied point
// (the stone replaces what was there). The editor refuses to *play* those --
// see Game -- but a file that has them still opens, and an edit further back
// can leave a later move on a stone, which Game lists for putting right.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sgf {
class Node;
}

enum class Stone : uint8_t { None, Black, White };

constexpr Stone Opponent(Stone s)
{
  return s == Stone::Black ? Stone::White : s == Stone::White ? Stone::Black : Stone::None;
}

struct Point {
  int x = -1;
  int y = -1;

  bool valid() const { return this->x >= 0 && this->y >= 0; }
  bool operator==(const Point&) const = default;
};

// SGF's coordinates: a letter per axis, a-z then A-Z, so boards up to 52.
inline constexpr int MAX_BOARD = 52;

std::string          ToSgf(Point p);
std::optional<Point> FromSgf(std::string_view text);  // nullopt unless it is two coordinate letters

// A move's value: a point, or a pass -- B[] in FF[4], and B[tt] on boards up
// to 19, which older programs write.
bool IsPass(std::string_view value, int width, int height);

// A point list, with FF[4]'s compressed rectangles ("aa:cc") spelled out.
std::vector<Point> PointList(const std::vector<std::string>& values);

// Where the move goes on screen and in the move list: "Q16", letters left to
// right skipping I, numbers from the bottom. Boards wider than 25 run out of
// letters, and get numbers across as well.
std::string DisplayName(Point p, int width, int height);
std::string ColumnName(int x, int width);
std::string RowName(int y, int height);

// Star points, for drawing the board.
std::vector<Point> StarPoints(int width, int height);

// Where handicap stones go, for `count` stones. Only the standard placements
// on boards with room for them; empty otherwise.
std::vector<Point> HandicapPoints(int count, int width, int height);

class Position {
public:
  Position(int width = 19, int height = 19);

  int width() const { return this->w; }
  int height() const { return this->h; }

  bool  inside(Point p) const { return p.x >= 0 && p.y >= 0 && p.x < this->w && p.y < this->h; }
  Stone at(Point p) const { return this->stones[this->index(p)]; }

  // The node that put the stone at `p` there -- a move, or the set-up that
  // added it -- and that move's number (0 for set-up). Null where there is no
  // stone. This is what lets a stone on the board be traced to where it came
  // from, to move it or to number it.
  const sgf::Node* origin(Point p) const { return this->origins[this->index(p)]; }
  int              moveNumberAt(Point p) const { return this->numbers[this->index(p)]; }

  // Stones each side has taken off the board.
  int captures(Stone by) const { return by == Stone::Black ? this->blackCaptures : this->whiteCaptures; }

  // A set-up stone, or None to clear the point. Captures nothing.
  void set(Point p, Stone s, const sgf::Node* origin = nullptr);

  // Plays `color` at `p`: opponent groups left without a liberty come off,
  // then the played group itself if it has none. Returns the stones taken.
  // Assumes `p` is on the board; an occupied point is overwritten.
  int play(Point p, Stone color, const sgf::Node* origin = nullptr, int moveNumber = 0);

  // Would the move take nothing and leave its own group without a liberty?
  bool isSuicide(Point p, Stone color) const;


  // The point a ko forbids retaking right now, if the last move was a single
  // stone capturing a single stone. Invalid when there is none.
  Point koPoint() const { return this->ko; }

private:
  size_t index(Point p) const { return size_t(p.y) * size_t(this->w) + size_t(p.x); }

  // Fills `group` with the stones connected to `p` and returns whether any of
  // them touches an empty point.
  bool groupHasLiberty(Point p, std::vector<Point>& group) const;
  int  removeGroup(const std::vector<Point>& group);

  int w;
  int h;
  std::vector<Stone>            stones;
  std::vector<const sgf::Node*> origins;
  std::vector<int>              numbers;
  int   blackCaptures = 0;
  int   whiteCaptures = 0;
  Point ko;
};
