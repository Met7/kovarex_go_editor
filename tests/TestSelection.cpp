#include <TestScenarioGui.hpp>
#include <UnitTest.hpp>

#include <game/Sgf.hpp>
#include <ui/EditorView.hpp>

#include <algorithm>

SUITE(Selection)
{
  namespace fs = std::filesystem;

  // A record of moves, opened as the editor opens a file.
  Game Record(const std::string& text)
  {
    std::optional<sgf::Collection> parsed = sgf::Parse(text);
    if (!parsed)
      throw std::runtime_error("Not a record: " + text);
    return Game(std::move(*parsed));
  }

  // The tree under `node` as SGF without the brackets round it: a node's
  // properties, then its one child, or each of its children in brackets.
  std::string Shape(const sgf::Node& node)
  {
    std::string out = ";";
    for (const sgf::Property& p : node.properties())
    {
      out += p.id;
      for (const std::string& v : p.values)
        out += "[" + v + "]";
    }
    if (node.childCount() == 1)
      return out + Shape(node.child(0));
    for (size_t k = 0; k < node.childCount(); ++k)
      out += "(" + Shape(node.child(k)) + ")";
    return out;
  }

  // The moves of the game: the tree under its root.
  std::string Moves(const Game& game)
  {
    std::string out;
    for (size_t k = 0; k < game.root().childCount(); ++k)
      out += game.root().childCount() == 1 ? Shape(game.root().child(0)) : "(" + Shape(game.root().child(k)) + ")";
    return out;
  }

  // The node at `path`: child indices from the root.
  sgf::Node& At(const Game& game, const std::vector<int>& path)
  {
    sgf::Node* node = &game.root();
    for (int k : path)
      node = &node->child(size_t(k));
    return *node;
  }

  // The selected moves, each as its move ("B[dd]"), in the order of the record.
  std::string Selected(const Game& game)
  {
    std::string out;
    std::vector<const sgf::Node*> todo{ &game.root() };
    while (!todo.empty())
    {
      const sgf::Node* n = todo.back();
      todo.pop_back();
      if (game.isSelected(*n))
        out += n->has("B") ? "B[" + n->get("B") + "]" : n->has("W") ? "W[" + n->get("W") + "]" : "root";
      for (size_t k = n->childCount(); k-- > 0;)
        todo.push_back(&n->child(k));
    }
    return out;
  }

  // Five moves in a row, and the same with a variation at move 3:
  //   B[dd] W[pp] B[dp] W[pd] B[jj]
  //   B[dd] W[pp] (B[dp] W[pd]) (B[qd] W[qq])
  const char* const LINE   = "(;SZ[19];B[dd];W[pp];B[dp];W[pd];B[jj])";
  const char* const BRANCH = "(;SZ[19];B[dd];W[pp](;B[dp];W[pd])(;B[qd];W[qq]))";

  // ------------------------------------------------------------ selecting

  TEST(ShiftSelectsFromTheCurrentMoveEitherWay)
  {
    Game game = Record(LINE);
    game.goToPath({ 0 });
    game.selectRange(At(game, { 0, 0, 0 }));
    CHECK_EQUAL(Selected(game), "B[dd]W[pp]B[dp]");
    CHECK(&game.current() == &At(game, { 0 }));  // the editor stays where it was

    game.goToPath({ 0, 0, 0, 0 });
    game.selectRange(At(game, { 0, 0 }));
    CHECK_EQUAL(Selected(game), "W[pp]B[dp]W[pd]");
  }

  TEST(ShiftAcrossLinesGoesThroughWhereTheyPart)
  {
    Game game = Record(BRANCH);
    game.goToPath({ 0, 0, 0, 0 });  // W[pd]
    game.selectRange(At(game, { 0, 0, 1, 0 }));  // W[qq]
    CHECK_EQUAL(Selected(game), "W[pp]B[dp]W[pd]B[qd]W[qq]");
  }

  TEST(ShiftAgainStartsOverCtrlAdds)
  {
    Game game = Record(LINE);
    game.goToPath({ 0 });
    game.selectRange(At(game, { 0, 0, 0 }));
    game.selectRange(At(game, { 0, 0 }));
    CHECK_EQUAL(Selected(game), "B[dd]W[pp]");

    // From the nearest selected move, so the selection stays in one piece.
    game.extendSelection(At(game, { 0, 0, 0, 0 }));
    CHECK_EQUAL(Selected(game), "B[dd]W[pp]B[dp]W[pd]");
  }

  TEST(CtrlWithNothingSelectedIsShift)
  {
    Game game = Record(LINE);
    game.goToPath({ 0, 0 });
    game.extendSelection(At(game, { 0, 0, 0 }));
    CHECK_EQUAL(Selected(game), "W[pp]B[dp]");
  }

  TEST(ABranchInAndOut)
  {
    Game game = Record(BRANCH);
    game.goToPath({ 0 });
    game.selectRange(At(game, { 0, 0, 0 }));
    CHECK_EQUAL(Selected(game), "B[dd]W[pp]B[dp]");

    game.selectBranch(At(game, { 0, 0, 1 }), true);
    CHECK_EQUAL(Selected(game), "B[dd]W[pp]B[dp]B[qd]W[qq]");
    game.selectBranch(At(game, { 0, 0, 0 }), false);
    CHECK_EQUAL(Selected(game), "B[dd]W[pp]B[qd]W[qq]");
  }

  TEST(AnEditLetsGoOfTheSelection)
  {
    Game game = Record(LINE);
    game.goToPath({ 0 });
    game.selectRange(At(game, { 0, 0 }));
    CHECK(game.play(Point{ 2, 2 }).ok);  // a new variation off B[dd]
    CHECK(game.selection().empty());
  }

  // ------------------------------------------------------------ copying

  TEST(CopyTakesOnlyTheSelectedMoves)
  {
    Game game = Record(BRANCH);
    game.goToPath({ 0, 0 });
    game.selectRange(At(game, { 0, 0, 0 }));  // W[pp] B[dp]: not W[pd], not the variation
    std::optional<sgf::Collection> copied = sgf::Parse(game.copySelection());
    CHECK(copied.has_value());
    if (copied)
      CHECK_EQUAL(Shape(*copied->games.front()), ";W[pp];B[dp]");

    // With the variation taken in, it comes along as a variation.
    game.selectBranch(At(game, { 0, 0, 1 }), true);
    copied = sgf::Parse(game.copySelection());
    if (copied)
      CHECK_EQUAL(Shape(*copied->games.front()), ";W[pp](;B[dp])(;B[qd];W[qq])");
  }

  // ------------------------------------------------------------ deleting

  TEST(DeletingASegmentJoinsWhatCameBeforeAndAfter)
  {
    Game game = Record(LINE);
    game.goToPath({ 0, 0 });
    game.selectRange(At(game, { 0, 0, 0 }));  // W[pp] B[dp]
    const Outcome outcome = game.deleteSelection();
    CHECK(outcome.ok);
    CHECK_EQUAL(outcome.message, "Deleted the 2 selected moves; the moves after them follow on from where they started.");
    CHECK_EQUAL(Moves(game), ";B[dd];W[pd];B[jj]");
    CHECK(&game.current() == &At(game, { 0 }));  // the move was deleted: the editor goes to the one before
    CHECK(game.selection().empty());

    CHECK(game.undo());
    CHECK_EQUAL(Moves(game), ";B[dd];W[pp];B[dp];W[pd];B[jj]");
  }

  TEST(DeletingToTheEndLeavesNothingAfter)
  {
    Game game = Record(LINE);
    game.goToPath({ 0, 0, 0 });
    game.selectRange(At(game, { 0, 0, 0, 0, 0 }));
    CHECK(game.deleteSelection().ok);
    CHECK_EQUAL(Moves(game), ";B[dd];W[pp]");
  }

  TEST(ABranchLeavingTheSelectionGoesWithIt)
  {
    // W[pp] and B[dp] selected: the variation off W[pp] has nothing selected
    // and goes; the main line goes on after B[dp], and W[pd] joins B[dd].
    Game game = Record(BRANCH);
    game.goToPath({ 0, 0 });
    game.selectRange(At(game, { 0, 0, 0 }));
    CHECK(game.deleteSelection().ok);
    CHECK_EQUAL(Moves(game), ";B[dd];W[pd]");
  }

  TEST(DownIntoAVariationTheMainLineGoes)
  {
    // W[pp] and B[qd], down the variation: the rest of the variation joins
    // B[dd], and the main line after W[pp], none of it selected, goes.
    Game game = Record(BRANCH);
    game.goToPath({ 0, 0 });
    game.selectRange(At(game, { 0, 0, 1 }));
    CHECK_EQUAL(Selected(game), "W[pp]B[qd]");
    CHECK(game.deleteSelection().ok);
    CHECK_EQUAL(Moves(game), ";B[dd];W[qq]");
  }

  TEST(OfTwoLinesCutShortTheMainLineGoesOn)
  {
    // W[pp], B[dp] and B[qd]: both lines stop part way. The main line's W[pd]
    // joins B[dd]; the variation's W[qq] goes.
    Game game = Record(BRANCH);
    game.goToPath({ 0, 0 });
    game.selectRange(At(game, { 0, 0, 0 }));
    game.extendSelection(At(game, { 0, 0, 1 }));
    CHECK_EQUAL(Selected(game), "W[pp]B[dp]B[qd]");
    CHECK(game.deleteSelection().ok);
    CHECK_EQUAL(Moves(game), ";B[dd];W[pd]");
  }

  TEST(EndingWhereLinesPartTakesThemAll)
  {
    // W[pp] alone: where the selection stops, two lines go on -- branches
    // with nothing selected, which go with it.
    Game game = Record(BRANCH);
    game.goToPath({ 0, 0 });
    game.selectRange(At(game, { 0, 0 }));
    CHECK(game.deleteSelection().ok);
    CHECK_EQUAL(Moves(game), ";B[dd]");
  }

  TEST(TheEditorStaysOnAMoveThatStays)
  {
    Game game = Record(LINE);
    game.goToPath({ 0, 0 });
    game.selectRange(At(game, { 0, 0, 0 }));  // W[pp] B[dp]
    game.goToPath({ 0, 0, 0, 0, 0 });          // on to B[jj], which isn't selected
    CHECK_EQUAL(Selected(game), "W[pp]B[dp]");
    CHECK(game.deleteSelection().ok);
    CHECK_EQUAL(game.current().get("B"), "jj");
  }

  TEST(WhatCantBeDeleted)
  {
    Game game = Record(LINE);
    Outcome outcome = game.deleteSelection();
    CHECK(!outcome.ok);
    CHECK_EQUAL(outcome.message, "Nothing is selected: Shift or Ctrl and click the game tree to select moves.");

    game.selectRange(At(game, { 0 }));  // from the start
    outcome = game.deleteSelection();
    CHECK(!outcome.ok);
    CHECK_EQUAL(outcome.message, "The start of the game can't be deleted.");
  }

  TEST(TheMovesAfterMayLandOnAStone)
  {
    // White takes B[aa] with W[ab], and Black plays B[aa] again. Without W[ab]
    // the corner stone is still there: deleted all the same, and B[aa] is red.
    Game game = Record("(;SZ[19];B[aa];W[ba];B[pp];W[ab];B[aa])");
    game.goToPath({ 0, 0, 0, 0 });
    game.selectRange(game.current());
    const Outcome outcome = game.deleteSelection();
    CHECK(outcome.ok);
    CHECK_EQUAL(outcome.message, "Deleted the selected move; the moves after them follow on from where they started. "
                                 "Now move 4 (Black A19) lands on a stone: it's red in the game tree.");
    CHECK_EQUAL(Moves(game), ";B[aa];W[ba];B[pp];B[aa]");
    CHECK(game.isStoneOnStone(At(game, { 0, 0, 0, 0 })));
  }

  // ------------------------------------------------------------ in the editor

  TEST(ClickingTheTree)
  {
    const fs::path file = TestScenarioGui::folder().parent_path() / "selection.sgf";
    WriteFile(file, std::string("(;GM[1]FF[4]") + (BRANCH + 2));
    TestScenarioGui scenario({ file });
    ui::EditorView& editor = scenario.app().interface().editor();
    const auto node = [&scenario, &editor](const std::vector<int>& path) {
      return editor.gameTree().nodeButton(At(scenario.game(), path));
    };

    scenario.click(node({ 0 }));  // a plain click goes there
    CHECK_EQUAL(scenario.game().moveNumber(), 1);
    scenario.click(node({ 0, 0, 0 }), agui::MouseButton::LEFT, GuiControl::Shift);
    CHECK_EQUAL(Selected(scenario.game()), "B[dd]W[pp]B[dp]");
    CHECK_EQUAL(scenario.game().moveNumber(), 1);
    scenario.click(node({ 0, 0, 0, 0 }), agui::MouseButton::LEFT, GuiControl::Ctrl);
    CHECK_EQUAL(Selected(scenario.game()), "B[dd]W[pp]B[dp]W[pd]");

    // The variation off W[pp] has a + on the way to it: all of it comes in.
    scenario.click(scenario.getWidget<agui::Widget>(editor.gameTree().widget(),
                                                    [](agui::Widget* widget) { return widget->getText() == "+"; }));
    CHECK_EQUAL(Selected(scenario.game()), "B[dd]W[pp]B[dp]W[pd]B[qd]W[qq]");

    // The Variation group is the selection's now: its Delete deletes every
    // selected move, which is all of them.
    scenario.updateMe();
    CHECK(scenario.shows("Selection"));
    scenario.clickSelectText("Delete", editor.variationButtons());
    CHECK(scenario.shows("Deleted the 6 selected moves."));
    CHECK_EQUAL(Moves(scenario.game()), "");

    // A plain click lets go of the selection.
    scenario.clickSelectText("Undo");
    CHECK_EQUAL(Moves(scenario.game()), ";B[dd];W[pp](;B[dp];W[pd])(;B[qd];W[qq])");
    scenario.click(node({ 0, 0 }), agui::MouseButton::LEFT, GuiControl::Shift);
    CHECK(!scenario.game().selection().empty());
    scenario.click(node({ 0, 0 }));
    CHECK(scenario.game().selection().empty());
    scenario.updateMe();
    CHECK(scenario.shows("Variation"));
    CHECK_EQUAL(scenario.game().moveNumber(), 2);
    // So does Cancel, which stands in for Main line while moves are selected.
    scenario.click(node({ 0, 0, 0 }), agui::MouseButton::LEFT, GuiControl::Shift);
    CHECK(!scenario.game().selection().empty());
    scenario.updateMe();
    CHECK(!scenario.shows("Main line"));
    scenario.clickSelectText("Cancel", editor.variationButtons());
    CHECK(scenario.game().selection().empty());
    scenario.updateMe();
    CHECK(scenario.shows("Main line"));
    CHECK(!scenario.shows("Cancel"));
    CHECK_EQUAL(scenario.game().moveNumber(), 2);
  }

  TEST(TheControlsPageListsTheClicks)
  {
    TestScenarioGui scenario;
    scenario.clickSelectText("Controls");
    CHECK(scenario.shows("The mouse in the game tree"));
    CHECK(scenario.shows("Shift + click a move"));
    CHECK(scenario.shows("Ctrl + click a move"));
  }
}
