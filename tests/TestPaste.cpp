#include <TestScenarioGui.hpp>
#include <UnitTest.hpp>

#include <game/Sgf.hpp>

SUITE(Paste)
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

  TEST(TheSameMovesAgainAreAlreadyThere)
  {
    Game game = Record("(;SZ[19];B[dd];W[pp];B[dp])");
    game.goToPath({ 0 });
    const std::string copied = game.copyBranch();
    game.toStart();

    const Outcome outcome = game.pasteBranch(copied);
    CHECK(outcome.ok);
    CHECK_EQUAL(outcome.message, "Those moves are already here.");
    CHECK_EQUAL(Moves(game), ";B[dd];W[pp];B[dp]");
    CHECK(!game.modified());
    CHECK(!game.canUndo());
    CHECK(&game.current() == &game.root().child(0));
  }

  TEST(MergesAndBranchesOffWhereTheyDiffer)
  {
    Game game = Record("(;SZ[19];B[dd];W[pp];B[dp])");
    const Outcome outcome = game.pasteBranch("(;B[dd];W[pp];B[qd];W[qq])");
    CHECK(outcome.ok);
    CHECK_EQUAL(outcome.message, "Merged with the moves already here, adding only what differs.");
    CHECK_EQUAL(Moves(game), ";B[dd];W[pp](;B[dp])(;B[qd];W[qq])");
    // The editor is where the pasted moves start: on the B[dd] already there.
    CHECK(&game.current() == &game.root().child(0));

    CHECK(game.undo());
    CHECK_EQUAL(Moves(game), ";B[dd];W[pp];B[dp]");
  }

  TEST(AWholeSubtreeMergesAndBranchesOffInSeveralPlaces)
  {
    Game game = Record("(;SZ[19];B[dd](;W[pp];B[dp])(;W[qq]))");
    const Outcome outcome = game.pasteBranch("(;B[dd](;W[pp];B[cc])(;W[qq];B[dp])(;W[ee]))");
    CHECK(outcome.ok);
    CHECK_EQUAL(outcome.message, "Merged with the moves already here, adding only what differs.");
    CHECK_EQUAL(Moves(game), ";B[dd](;W[pp](;B[dp])(;B[cc]))(;W[qq];B[dp])(;W[ee])");
  }

  TEST(AnyDifferenceIsADifference)
  {
    // A comment, a mark, a name: each makes a move a new variation, where it is.
    const std::pair<const char*, const char*> cases[] = {
      { "(;B[dd]C[A comment];W[pp])", "(;B[dd];W[pp])(;B[dd]C[A comment];W[pp])" },
      { "(;B[dd];W[pp]TR[aa])", ";B[dd](;W[pp])(;W[pp]TR[aa])" },
      { "(;B[dd];W[pp]N[Joseki])", ";B[dd](;W[pp])(;W[pp]N[Joseki])" },
      { "(;W[dd])", "(;B[dd];W[pp])(;W[dd])" },
    };
    for (const auto& [pasted, expected] : cases)
    {
      Game game = Record("(;SZ[19];B[dd];W[pp])");
      CHECK(game.pasteBranch(pasted).ok);
      CHECK_EQUAL(Moves(game), expected);
    }
  }

  TEST(TheOrderPropertiesAreWrittenInDoesNot)
  {
    Game game = Record("(;SZ[19];B[dd]C[Hello]TR[aa][bb];W[pp])");
    const Outcome outcome = game.pasteBranch("(;TR[bb][aa]C[Hello]B[dd];W[pp])");
    CHECK_EQUAL(outcome.message, "Those moves are already here.");
    CHECK(!game.modified());
  }

  TEST(AWholeGameMergesIntoTheStart)
  {
    // Copied at the start, a game takes its game information along; pasted at
    // the start of another, that stays behind and the moves merge.
    Game game = Record("(;SZ[19]PB[Black];B[dd];W[pp])");
    const Outcome outcome = game.pasteBranch("(;FF[4]GM[1]SZ[19]PB[Someone]KM[6.5];B[dd];W[pp];B[dp])");
    CHECK(outcome.ok);
    CHECK_EQUAL(outcome.message, "Merged with the moves already here, adding only what differs.");
    CHECK_EQUAL(Moves(game), ";B[dd];W[pp];B[dp]");
    CHECK_EQUAL(game.root().get("PB"), "Black");
  }

  TEST(DifferentMovesStillMakeAPlainVariation)
  {
    Game game = Record("(;SZ[19];B[dd];W[pp])");
    const Outcome outcome = game.pasteBranch("(;B[pd];W[dp])");
    CHECK(outcome.ok);
    CHECK_EQUAL(outcome.message, "");
    CHECK_EQUAL(Moves(game), "(;B[dd];W[pp])(;B[pd];W[dp])");
  }

  TEST(CopyAndPasteThroughTheButtons)
  {
    // Copy and Paste go through the system clipboard: this puts a game on it.
    const fs::path file = TestScenarioGui::folder().parent_path() / "paste.sgf";
    WriteFile(file, "(;GM[1]FF[4]SZ[19];B[dd];W[pp];B[dp])");
    TestScenarioGui scenario({ file });

    scenario.press(KEY_RIGHT);  // on move 1
    scenario.clickSelectText("Copy");
    scenario.press(KEY_LEFT);   // back at the start
    scenario.clickSelectText("Paste");
    CHECK(scenario.shows("Those moves are already here."));
    CHECK_EQUAL(Moves(scenario.game()), ";B[dd];W[pp];B[dp]");
    CHECK(!scenario.game().modified());
    CHECK_EQUAL(scenario.game().moveNumber(), 1);

    // Moves 2 and 3 deleted, the same paste puts them back after move 1.
    scenario.press(KEY_RIGHT);  // on move 2
    scenario.clickSelectText("Delete");
    CHECK_EQUAL(Moves(scenario.game()), ";B[dd]");
    scenario.press(KEY_LEFT);   // back at the start
    scenario.clickSelectText("Paste");
    CHECK(scenario.shows("Merged with the moves already here, adding only what differs."));
    CHECK_EQUAL(Moves(scenario.game()), ";B[dd];W[pp];B[dp]");
    CHECK_EQUAL(scenario.game().moveNumber(), 1);
  }
}
