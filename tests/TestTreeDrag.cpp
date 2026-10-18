#include <TestScenarioGui.hpp>
#include <UnitTest.hpp>

#include <game/Sgf.hpp>
#include <ui/EditorView.hpp>

SUITE(TreeDrag)
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

  // Five moves in a row, and the same with a variation at move 3:
  //   B[dd] W[pp] B[dp] W[pd] B[jj]
  //   B[dd] W[pp] (B[dp] W[pd]) (B[qd] W[qq])
  const char* const LINE   = "(;SZ[19];B[dd];W[pp];B[dp];W[pd];B[jj])";
  const char* const BRANCH = "(;SZ[19];B[dd];W[pp](;B[dp];W[pd])(;B[qd];W[qq]))";

  TEST(ABranchMovesWithEverythingAfterIt)
  {
    Game game = Record(BRANCH);
    const Outcome outcome = game.moveBranch(At(game, { 0, 0, 1 }), At(game, { 0 }));
    CHECK(outcome.ok);
    CHECK_EQUAL(outcome.message, "Moved the 2 moves.");
    CHECK_EQUAL(Moves(game), ";B[dd](;W[pp];B[dp];W[pd])(;B[qd];W[qq])");
    // The editor goes to the first of them.
    CHECK_EQUAL(&game.current(), &At(game, { 0, 1 }));

    // One undo puts it all back.
    CHECK(game.undo());
    CHECK_EQUAL(Moves(game), ";B[dd];W[pp](;B[dp];W[pd])(;B[qd];W[qq])");
  }

  TEST(TheSelectionMovesAndTheMovesAfterItCloseUp)
  {
    Game game = Record(LINE);
    game.goTo(At(game, { 0, 0 }));
    game.selectRange(At(game, { 0, 0, 0 }));  // W[pp] B[dp]
    const Outcome outcome = game.moveSelection(At(game, { 0, 0, 0, 0, 0 }));
    CHECK(outcome.ok);
    CHECK_EQUAL(outcome.message, "Moved the 2 moves.");
    CHECK_EQUAL(Moves(game), ";B[dd];W[pd];B[jj];W[pp];B[dp]");
    CHECK(game.selection().empty());

    CHECK(game.undo());
    CHECK_EQUAL(Moves(game), ";B[dd];W[pp];B[dp];W[pd];B[jj]");
  }

  TEST(DroppedOnAVariationThatIsAlreadyThere)
  {
    Game game = Record("(;SZ[19](;B[dd];W[pp])(;B[pp])(;B[qq];B[dd];W[pp]))");
    // B[dd] W[pp] after B[qq], dropped on the start: it is the first variation.
    const Outcome outcome = game.moveBranch(At(game, { 2, 0 }), game.root());
    CHECK(outcome.ok);
    CHECK_EQUAL(outcome.message, "Moved the 2 moves; they merged into the moves already there.");
    CHECK_EQUAL(Moves(game), "(;B[dd];W[pp])(;B[pp])(;B[qq])");
  }

  TEST(WhereTheyCantGo)
  {
    Game game = Record(BRANCH);
    // B[dp] W[pd] selected.
    game.goTo(At(game, { 0, 0, 0 }));
    game.selectRange(At(game, { 0, 0, 0, 0 }));

    // Onto one of them.
    Outcome outcome = game.moveSelection(At(game, { 0, 0, 0, 0 }));
    CHECK(!outcome.ok);
    CHECK_EQUAL(outcome.message, "Drop them on a move that is not in the selection.");
    // Onto everything after it, which goes along.
    outcome = game.moveBranch(At(game, { 0 }), At(game, { 0, 0, 1 }));
    CHECK(!outcome.ok);
    CHECK_EQUAL(outcome.message, "Drop them on a move that is not in the selection.");
    // Onto where they already are.
    outcome = game.moveBranch(At(game, { 0, 0, 1 }), At(game, { 0, 0 }));
    CHECK(!outcome.ok);
    CHECK_EQUAL(outcome.message, "They already follow on from that move.");
    // Onto a variation that deleting them takes away.
    outcome = game.moveSelection(At(game, { 0, 0, 1 }));
    CHECK(outcome.ok);  // B[dp] W[pd] onto B[qd]: that one stays
    CHECK(game.undo());
    game.goTo(At(game, { 0 }));
    game.selectRange(At(game, { 0, 0 }));  // B[dd] W[pp]: both variations go with them
    const auto before = game.selection();
    outcome = game.moveSelection(At(game, { 0, 0, 1 }));
    CHECK(!outcome.ok);
    CHECK_EQUAL(outcome.message, "That move goes with them: drop them somewhere else.");
    CHECK(game.selection() == before);

    // The start of the game.
    outcome = game.moveBranch(game.root(), At(game, { 0 }));
    CHECK(!outcome.ok);
    CHECK_EQUAL(outcome.message, "The start of the game can't be moved.");
    CHECK(game.selection() == before);
    CHECK_EQUAL(Moves(game), ";B[dd];W[pp](;B[dp];W[pd])(;B[qd];W[qq])");
  }

  TEST(DroppedWhereAMoveLandsOnAStone)
  {
    // W[dd] after B[pp], dropped after B[dd]: it lands on that stone, and is
    // kept all the same, in red.
    Game game = Record("(;SZ[19](;B[dd])(;B[pp];W[dd]))");
    const Outcome outcome = game.moveBranch(At(game, { 1, 0 }), At(game, { 0 }));
    CHECK(outcome.ok);
    CHECK_EQUAL(outcome.message, "Moved the move. Now move 2 (White D16) lands on a stone: it's red in the game tree.");
    CHECK_EQUAL(Moves(game), "(;B[dd];W[dd])(;B[pp])");
    CHECK(game.isStoneOnStone(At(game, { 0, 0 })));
    CHECK(game.undo());
    CHECK_EQUAL(Moves(game), "(;B[dd])(;B[pp];W[dd])");
  }

  TEST(DraggingInTheTree)
  {
    const fs::path file = TestScenarioGui::folder().parent_path() / "tree-drag.sgf";
    WriteFile(file, std::string("(;GM[1]FF[4]") + (BRANCH + 2));
    TestScenarioGui scenario({ file });
    ui::EditorView& editor = scenario.app().interface().editor();
    const auto node = [&scenario, &editor](const std::vector<int>& path) {
      return editor.gameTree().nodeButton(At(scenario.game(), path));
    };

    // B[qd]'s variation, dragged onto B[dd]. Nothing moves until it is let go.
    scenario.mouseDown(node({ 0, 0, 1 }));
    scenario.moveTo(node({ 0 }));
    CHECK_EQUAL(Moves(scenario.game()), ";B[dd];W[pp](;B[dp];W[pd])(;B[qd];W[qq])");
    scenario.buttonsUp();
    CHECK_EQUAL(Moves(scenario.game()), ";B[dd](;W[pp];B[dp];W[pd])(;B[qd];W[qq])");
    CHECK(scenario.shows("Moved the 2 moves."));
    CHECK_EQUAL(scenario.game().moveNumber(), 2);

    // A selected move takes the selection along: W[pp] B[dp], onto B[qd], as
    // a variation of the move after it. W[pd] closes up behind B[dd].
    scenario.click(node({ 0, 0 }));
    scenario.click(node({ 0, 0, 0 }), agui::MouseButton::LEFT, GuiControl::Shift);
    scenario.mouseDown(node({ 0, 0, 0 }));
    scenario.moveTo(node({ 0, 1 }));
    scenario.buttonsUp();
    CHECK_EQUAL(Moves(scenario.game()), ";B[dd](;W[pd])(;B[qd](;W[qq])(;W[pp];B[dp]))");

    // Let go where it started, it is no click: the editor stays.
    const int number = scenario.game().moveNumber();
    scenario.mouseDown(node({ 0, 0 }));
    scenario.moveTo(node({ 0, 0 }), { 0, 0 });
    scenario.moveTo(node({ 0, 0 }));
    scenario.buttonsUp();
    CHECK_EQUAL(scenario.game().moveNumber(), number);
  }

  TEST(TheControlsPageListsTheDrag)
  {
    TestScenarioGui scenario;
    scenario.clickSelectText("Controls");
    CHECK(scenario.shows("Move a subtree"));
    CHECK(scenario.shows("Drag a stone"));
  }
}
