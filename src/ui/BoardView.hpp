// The Go board on screen. It is a widget tree like everything else here: a
// wooden panel, the grid laid on it as thin widgets, and over each point a
// button carrying pictures -- the stone, a mark -- and a label. So the points
// take clicks through the Gui, rather than by working out which point the
// mouse is over, and they theme and scale like the rest.
//
// It is also where a click means something. What it means depends on the
// tool (see Commands.hpp); with the Play tool:
//
//   click          plays a move, or steps into it if the record already has it
//   Ctrl+click     inserts the move after this one, the rest of the game
//                  following on after it, instead of starting a variation
//   drag a stone   moves it, and changes the move that played it -- however
//                  long ago that was
//   right-click or
//   Shift+click    on a stone, goes to the move that played it
//
// and the mouse wheel steps through the game.

#pragma once

#include <game/Game.hpp>
#include <net/Protocol.hpp>
#include <ui/Commands.hpp>
#include <ui/GoSprites.hpp>

#include <Agui/GenericTargetable.hpp>
#include <Agui/MouseEvent.hpp>
#include <Agui/Widget/Button.hpp>
#include <Agui/Widget/EmptyWidget.hpp>
#include <Agui/Widget/ImageWidget.hpp>
#include <Agui/Widget/Label.hpp>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace ui {

class Theme;

// One point of the board: a see-through button with a stone, a mark and a
// label on it, in that order, so the label reads over the stone.
class PointWidget : public agui::Button {
  using super = agui::Button;

public:
  explicit PointWidget(const agui::ButtonStyle* style);

  // Each only does anything when asked for something other than what it
  // shows already -- which is what makes refreshing every point after every
  // move cheap enough to do.
  void setStone(std::optional<Sprite> sprite, double opacity, const GoSprites& sprites);
  void setMark(std::optional<Sprite> sprite, double opacity, const GoSprites& sprites);
  void setLabel(const std::string& text, const agui::LabelStyle* style);
  // The piece of the board's wood under the point, and whether to show it:
  // a label on an empty point sits on bare wood, so the lines don't cross it
  // out.
  void setWood(std::unique_ptr<agui::Image> image);
  void setBare(bool bare);

protected:
  // The pictures are children, and a button does not lay its children out,
  // so they are kept over the point by hand.
  void onSizeChanged(agui::Dimension originalSize) override;

private:
  void place();

  agui::ImageWidget patch;  // the wood, over the lines
  // Under the stone, bigger than the point and shifted down and to the
  // right, so the shadow falls on the wood round it. The points are drawn
  // row by row, so the stones to the right and below sit over it.
  agui::ImageWidget shadow;
  agui::ImageWidget stone;
  agui::ImageWidget mark;
  agui::Label       label;

  std::optional<Sprite>   shownStone;
  std::optional<Sprite>   shownMark;
  double                  stoneOpacity = 1.0;
  double                  markOpacity  = 1.0;
  const agui::LabelStyle* labelStyle   = nullptr;
};

// SGF's arrows and lines, which run from point to point across the board and
// so can't be a picture on either. A see-through layer over the points that
// draws them with the Gui's own line drawing.
class LineLayer : public agui::Widget {
public:
  struct Line {
    agui::Point from, to;
    bool        arrow;
    bool        preview = false;  // the one being drawn: see-through
    bool        box     = false;  // a side of a Shift-drag's box: the game tree's selection blue
  };
  LineLayer();
  void setLines(std::vector<Line> lines, float thickness);

protected:
  void paintComponent(const agui::PaintEvent& paintEvent, const agui::Point& absolutePosition) override;

private:
  std::vector<Line> lines;
  float             thickness = 2.0f;
};

// The colour of participant colour `index` -- theirs on everyone's screen:
// the lines they draw, and their name in the session's list.
agui::Color ParticipantColour(uint8_t index);

// Lines drawn freehand over the board -- to show something, not part of the
// game -- each in its drawer's colour, fading away a while after it is done.
class StrokeLayer : public agui::Widget {
public:
  struct Stroke {
    std::vector<agui::Point> points;
    agui::Color              colour;
    float                    opacity = 1.0f;
  };
  StrokeLayer();
  void setStrokes(std::vector<Stroke> strokes, float thickness);

protected:
  void paintComponent(const agui::PaintEvent& paintEvent, const agui::Point& absolutePosition) override;

private:
  std::vector<Stroke> strokes;
  float               thickness = 3.0f;
};

class BoardView : public agui::GenericTargetable {
public:
  struct Options {
    bool coordinates = true;
    bool moveNumbers = false;  // on every stone, not just the last
    bool nextMoves   = true;   // where the variations from here go, as faint stones

    bool operator==(const Options&) const = default;
  };

  BoardView(Theme& theme, const GoSprites& sprites);
  ~BoardView();
  BoardView(const BoardView&) = delete;
  BoardView& operator=(const BoardView&) = delete;

  // The panel, for whoever lays the screen out to add to the Gui.
  agui::EmptyWidget& widget() { return this->board; }

  // The game to show and edit, or null for none. Not owned.
  void show(Game* game);
  void setOptions(const Options& options);
  // The tool clicks use; `text` is what the Text tool writes.
  void setTool(Tool tool, const std::string& text);
  // Drops a half-drawn arrow or line.
  void cancel();

  // Brings the points in line with the game. Cheap when nothing changed, so
  // it is called every frame.
  void refresh();

  // Fits the board, as big as it will go, into this rectangle of the screen,
  // at its left and filling its height: the grid as big as it will go, and
  // the wood round it taking up the rest -- there is no black above or below
  // it unless the width is what limits it. Returns the board's width.
  int layout(int x, int y, int width, int height);

  // The interface scale, in percent. At one like 125% a GUI unit is not a
  // whole number of pixels, and a one-unit line would come out one pixel
  // wide or two depending on where it fell. So the board keeps every line on
  // a GUI coordinate that is a whole pixel on screen, and they all come out
  // alike.
  void setScale(int percent);

  // While a page is up, the board shows but takes no clicks.
  void setInteractive(bool value);

  // Drawing on the board: with the middle mouse button whatever the tool, or
  // the left one with the Pen tool. Lines are in board coordinates -- a
  // point's centre at whole numbers, (0, 0) the top left one -- so they land
  // in the same place on every screen.
  // What was drawn here since the last call, for the others in a session.
  std::vector<net::StrokePart> takeStrokes();
  // A line someone else draws, `part` at a time, in their colour.
  void addStroke(uint32_t author, uint8_t colour, const net::StrokePart& part);
  // The colour lines drawn here are in.
  void setOwnColour(uint8_t colour) { this->ownColour = colour; }
  // Whether Shift is held: the set-up stones tool then puts down white.
  void setShift(bool held)
  {
    if (held != this->shiftHeld) this->dirty = true;
    this->shiftHeld = held;
  }

  // What a click came to, for the status line; and the mouse wheel, as moves
  // to step (negative is back).
  std::function<void(const Outcome&)> onOutcome;
  std::function<void(int)>            onWheel;

private:
  void build();
  void place();

  void pressed(Point p, const agui::MouseEvent& event);
  void dragged(Point p, const agui::MouseEvent& event);
  void released(Point p, const agui::MouseEvent& event);
  void click(Point p, bool ctrl, bool shift);
  // Whether a line is being drawn on the board just now.
  bool drawing() const { return this->drawingWith != agui::MouseButton::NONE; }
  void rightClick(Point p);
  // The tool's mark put on `p`, or taken off, as `paintOn` says, in the same
  // edit as the rest of the drag; and the box let go of, done.
  void paintMark(Point p);
  void applyBox();
  // The point under the mouse, from an event on the point it was pressed on:
  // everything after the press goes to that point, wherever the mouse is.
  Point pointAt(Point pressedOn, const agui::MouseEvent& event) const;

  void report(const Outcome& outcome);

  PointWidget& point(Point p) { return *this->points[size_t(p.y) * size_t(this->columns) + size_t(p.x)]; }

  Theme&           theme;
  const GoSprites& sprites;

  // The only widget the view owns outright; everything under it is held by
  // the tree and goes with it.
  agui::EmptyWidget board;

  agui::ImageWidget*              wood = nullptr;  // the grain, under everything
  std::vector<PointWidget*>       points;  // row by row
  std::vector<agui::EmptyWidget*> lines;   // columns, then rows
  std::vector<agui::ImageWidget*> stars;
  std::vector<Point>              starPoints;
  std::vector<agui::Label*>       columnLabels;  // top then bottom, per column
  std::vector<agui::Label*>       rowLabels;     // left then right, per row
  LineLayer*                      lineLayer = nullptr;

  Game*   game = nullptr;
  Options options;
  Tool    tool = Tool::Play;
  std::string text;

  int columns = 0, rows = 0;
  int pitch = 0, margin = 0;  // px per point, and round the grid
  int labelMargin = 0;        // what the margin would be without filling: the coordinates keep to that
  int snap  = 1;              // GUI units to a whole screen pixel: see setScale()

  // How far a line `width` wide starts before the middle of its points:
  // half its width, but on the pixel grid.
  int offGrid(int width) const { return width / 2 / this->snap * this->snap; }

  Point hover;       // the point under the mouse
  Point pressPoint;  // where the left button went down
  Point dragTarget;  // where a dragged stone would go
  bool  dragging = false;
  Point lineStart;   // the first click of an arrow or a line
  // A mark tool dragged across the board: putting the mark on (or taking it
  // off, by the first point, or with Ctrl), and the last point done.
  bool  painting = false;
  bool  paintOn  = false;
  Point paintedOn;
  bool  painted  = false;  // whether the drag changed anything yet: its first change starts the edit
  // A mark tool dragged with Shift: a box from one corner to the other, whose
  // stones -- or, for territory, empty points -- get the mark when it is let
  // go; with Ctrl too, every point in it loses it. With Ctrl and Shift, the
  // label tools take the labels off the box, and the set-up one its set-up
  // stones; neither puts anything on a box.
  bool  boxing = false;
  bool  boxOn  = true;
  Point boxFrom;
  Point boxTo;

  uint64_t shownRevision = 0;
  bool     dirty         = true;

  // Drawing.
  struct Drawn {
    uint32_t                          author = 0;  // 0 for this editor
    uint32_t                          id     = 0;
    uint8_t                           colour = 0;
    std::vector<std::pair<float, float>> points;
    bool                              finished = false;
    double                            finishedAt = 0;  // seconds, on the clock below
  };
  // Where the mouse is, in board coordinates, from an event on point `on`.
  std::pair<float, float> boardAt(Point on, const agui::MouseEvent& event) const;
  void                    drawTo(std::pair<float, float> at);
  void                    updateStrokes();
  static double           Now();

  StrokeLayer*                  strokeLayer = nullptr;
  std::vector<Drawn>            sketches;
  std::vector<net::StrokePart>  outgoing;
  agui::MouseButton             drawingWith = agui::MouseButton::NONE;
  Point                         drawingOn;  // the point the drawing button went down on
  uint32_t                      nextStroke  = 1;
  uint8_t                       ownColour   = 0;
  bool                          shiftHeld   = false;
};

}  // namespace ui
