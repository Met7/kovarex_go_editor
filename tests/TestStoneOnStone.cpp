#include <TestScenarioGui.hpp>
#include <UnitTest.hpp>

#include <game/Sgf.hpp>

SUITE(StoneOnStone)
{
  namespace fs = std::filesystem;

  // Columns and rows as SGF counts them, from the top left: D16 is {3, 3}.
  constexpr Point D16{ 3, 3 };
  constexpr Point E15{ 4, 4 };
  constexpr Point K10{ 9, 9 };
  constexpr Point Q4{ 15, 15 };
  constexpr Point D4{ 3, 15 };
  constexpr Point C17{ 2, 2 };

  // A record in tests/sgf, which are there to be opened by hand too.
  fs::path SampleFile(const std::string& name)
  {
    return fs::path(__FILE__).parent_path() / "sgf" / name;
  }

  Game Record(const std::string& text)
  {
    std::optional<sgf::Collection> parsed = sgf::Parse(text);
    if (!parsed)
      throw std::runtime_error("Not a record: " + text);
    return Game(std::move(*parsed));
  }

  // tests/sgf/stone-on-stone.sgf: move 4 (White D16) on Black's move 1,
  // move 6 (White K10) on the white set-up stone, and in the variation move 6
  // (White Q16) on Black's move 5.
  Game StoneOnStoneFile() { return Record(ReadFile(SampleFile("stone-on-stone.sgf"))); }

  const std::vector<int> MOVE_4           = { 0, 0, 0, 0 };
  const std::vector<int> MOVE_5           = { 0, 0, 0, 0, 0 };
  const std::vector<int> MOVE_6           = { 0, 0, 0, 0, 0, 0 };
  const std::vector<int> MOVE_6_VARIATION = { 0, 0, 0, 0, 0, 1 };

  // ------------------------------------------------------------ the file

  TEST(AFileWithStonesOnStonesOpens)
  {
    Game game = StoneOnStoneFile();
    CHECK_EQUAL(game.stonesOnStones().size(), 3u);
    for (const std::vector<int>& path : { MOVE_4, MOVE_6, MOVE_6_VARIATION })
    {
      game.goToPath(path);
      CHECK(game.isStoneOnStone(game.current()));
    }
    for (const std::vector<int>& path : { std::vector<int>{}, std::vector<int>{ 0 }, MOVE_5 })
    {
      game.goToPath(path);
      CHECK(!game.isStoneOnStone(game.current()));
    }
  }

  TEST(TheStoneOnTopReplacesTheOneUnder)
  {
    Game game = StoneOnStoneFile();
    game.goToPath({ 0, 0, 0 });
    CHECK(game.position().at(D16) == Stone::Black);
    game.goToPath(MOVE_4);
    CHECK(game.position().at(D16) == Stone::White);
    CHECK_EQUAL(game.position().moveNumberAt(D16), 4);
    CHECK_EQUAL(game.moveNumber(), 4);
  }

  TEST(TheReasonSaysWhatTheMoveLandsOn)
  {
    Game game = StoneOnStoneFile();
    game.goToPath(MOVE_4);
    CHECK_EQUAL(game.stoneOnStoneReason(game.current()),
                "Move 4 (White D16) is played on top of move 1 (Black D16), which is still on the board.");
    game.goToPath(MOVE_6);
    CHECK_EQUAL(game.stoneOnStoneReason(game.current()),
                "Move 6 (White K10) is played on top of a white set-up stone (set up at the start), which is still on "
                "the board.");
    game.goToPath(MOVE_6_VARIATION);
    CHECK_EQUAL(game.stoneOnStoneReason(game.current()),
                "Move 6 (White Q16) is played on top of move 5 (Black Q16), which is still on the board.");
    game.goToPath(MOVE_5);
    CHECK_EQUAL(game.stoneOnStoneReason(game.current()), "");
  }

  TEST(TheStoneUnderIsTheNodeThatPutItThere)
  {
    Game game = StoneOnStoneFile();

    // Move 4 lands on move 1.
    game.goToPath(MOVE_4);
    std::optional<Game::StoneOnStone> s = game.stoneOnStone(game.current());
    CHECK(s.has_value());
    if (!s)
      return;
    CHECK_EQUAL(s->number, 4);
    CHECK(s->at == D16);
    CHECK(s->color == Stone::White);
    CHECK(s->under == Stone::Black);
    CHECK(s->underIsMove);
    CHECK_EQUAL(s->underNumber, 1);
    CHECK(s->underNode == &game.root().child(0));

    // Move 6 lands on the root's set-up stone.
    game.goToPath(MOVE_6);
    s = game.stoneOnStone(game.current());
    CHECK(s.has_value());
    if (!s)
      return;
    CHECK(!s->underIsMove);
    CHECK_EQUAL(s->underNumber, 0);
    CHECK(s->underNode == &game.root());
    CHECK(s->stack == std::vector<const sgf::Node*>{ &game.root() });  // a set-up stone is the bottom

    game.goToPath(MOVE_5);
    CHECK(!game.stoneOnStone(game.current()).has_value());
  }

  TEST(ASetUpStoneMidGameSaysWhenItWasSetUp)
  {
    // Black's move 3 lands on the white stone set up after move 1.
    Game game = Record("(;FF[4]GM[1]SZ[19];B[dd];AW[qq];W[pp];B[qq])");
    game.goToPath({ 0, 0, 0, 0 });
    CHECK_EQUAL(game.stoneOnStoneReason(game.current()),
                "Move 3 (Black R3) is played on top of a white set-up stone (set up after move 1), which is still on "
                "the board.");
    const std::optional<Game::StoneOnStone> s = game.stoneOnStone(game.current());
    CHECK(s && s->underNode == &game.root().child(0).child(0));
  }

  TEST(SavingKeepsTheMovesAsTheyWere)
  {
    Game game     = StoneOnStoneFile();
    Game reopened = Record(sgf::Write(game.file()));
    CHECK_EQUAL(reopened.stonesOnStones().size(), 3u);
  }

  // ------------------------------------------------------------ putting it right

  TEST(DraggingTheStoneMovesTheLaterMove)
  {
    Game game = StoneOnStoneFile();

    // The stone at D16 on the board is move 4's, the one on top.
    game.goToPath(MOVE_4);
    const Outcome outcome = game.moveStone(D16, E15);
    CHECK(outcome.ok);
    CHECK_EQUAL(outcome.message, "Move 4 is now at E15.");
    CHECK(!game.isStoneOnStone(game.current()));
    CHECK_EQUAL(game.stonesOnStones().size(), 2u);
    CHECK_EQUAL(game.current().get("W"), "ee");
    // Black's move 1 is back in view under it.
    CHECK(game.position().at(D16) == Stone::Black);
    CHECK(game.position().at(E15) == Stone::White);

    CHECK(game.undo());
    CHECK_EQUAL(game.stonesOnStones().size(), 3u);
  }

  // tests/sgf/stacked-stones.sgf: three moves on D16, 1 (Black), 4 (White)
  // and 7 (Black). Move 7 lands on move 4, which itself lands on move 1.
  TEST(AStoneOnAStoneOnAStone)
  {
    Game game = Record(ReadFile(SampleFile("stacked-stones.sgf")));
    CHECK_EQUAL(game.stonesOnStones().size(), 2u);

    game.goToPath({ 0, 0, 0, 0, 0, 0, 0 });
    const std::optional<Game::StoneOnStone> s = game.stoneOnStone(game.current());
    CHECK(s.has_value());
    if (!s)
      return;
    CHECK_EQUAL(s->underNumber, 4);
    CHECK(s->underNode && game.isStoneOnStone(*s->underNode));
    // The whole stack under it, nearest first: move 4, then move 1. All of
    // it is orange in the game tree.
    const sgf::Node* move1 = &game.root().child(0);
    const sgf::Node* move4 = &move1->child(0).child(0).child(0);
    CHECK((s->stack == std::vector<const sgf::Node*>{ move4, move1 }));
    CHECK_EQUAL(game.stoneOnStoneReason(game.current()),
                "Move 7 (Black D16) is played on top of move 4 (White D16), which is still on the board.");

    // Move 4's stack is move 1 alone.
    game.goToPath({ 0, 0, 0, 0 });
    const std::optional<Game::StoneOnStone> s4 = game.stoneOnStone(game.current());
    CHECK(s4 && s4->stack == std::vector<const sgf::Node*>{ move1 });
  }

  // The same three moves: a drag of the stone on D16 moves the selected move
  // if it is one of them, and the latest of them played so far if it isn't.
  TEST(DraggingAStackMovesTheSelectedOrTheLatestMove)
  {
    const std::string RECORD = ReadFile(SampleFile("stacked-stones.sgf"));
    struct Case
    {
      int         selected;  // the move selected
      const char* moved;     // the move that ends up at E15
    };
    const Case cases[] = {
      { 1, "1" },  // move 1 selected
      { 3, "1" },  // move 3: only move 1 is on D16 yet
      { 4, "4" },  // move 4 selected
      { 5, "4" },  // move 5: move 4 is the latest
      { 7, "7" },  // move 7 selected
      { 8, "7" },  // move 8: move 7 is the latest
    };
    for (const Case& c : cases)
    {
      Game game = Record(RECORD);
      game.goToPath(std::vector<int>(size_t(c.selected), 0));
      CHECK(game.moveStone(D16, E15).ok);

      std::string atE15;
      const sgf::Node* n = &game.root();
      for (int number = 1; number <= 8; ++number)
      {
        n = &n->child(0);
        if (n->get(Game::MoveColor(*n) == Stone::Black ? "B" : "W") == "ee")
          atE15 += std::to_string(number);
      }
      CHECK_EQUAL(atE15, c.moved);
    }
  }

  TEST(PlayingOntoAStoneIsStillRefused)
  {
    Game game;
    CHECK(game.play(D16).ok);
    const Outcome outcome = game.play(D16);
    CHECK(!outcome.ok);
    CHECK_EQUAL(outcome.message, "There is already a stone there.");
    CHECK(game.stonesOnStones().empty());
  }

  // ------------------------------------------------------------ edits that make one

  TEST(PastedMovesMayLandOnAStone)
  {
    Game game;
    CHECK(game.play(D16).ok);  // 1
    CHECK(game.play(Q4).ok);   // 2

    const Outcome outcome = game.pasteBranch("(;B[qd];W[dd])");
    CHECK(outcome.ok);
    CHECK_EQUAL(outcome.message,
                "Pasted as a new variation. Now move 4 (White D16) lands on a stone: it's red in the game tree.");
    CHECK_EQUAL(game.stonesOnStones().size(), 1u);
    CHECK_EQUAL(game.current().get("B"), "qd");
    CHECK(game.isStoneOnStone(game.current().child(0)));
  }

  TEST(PastedMovesThatFitSayNothingMore)
  {
    Game game;
    CHECK(game.play(D16).ok);
    const Outcome outcome = game.pasteBranch("(;W[pp];B[pd])");
    CHECK(outcome.ok);
    CHECK_EQUAL(outcome.message, "");
    CHECK(game.stonesOnStones().empty());
  }

  TEST(InsertingAMoveMayPutALaterOneOnAStone)
  {
    Game game;
    CHECK(game.play(C17).ok);  // 1
    CHECK(game.play(Q4).ok);   // 2
    CHECK(game.play(D4).ok);   // 3
    CHECK(game.play(D16).ok);  // 4, White
    game.goToPath({ 0, 0 });   // back to move 2

    // Black D16 as move 3: White's D16 after it, now move 5, lands on it.
    const Outcome outcome = game.insertMove(D16);
    CHECK(outcome.ok);
    CHECK_EQUAL(outcome.message, "Inserted Black D16 as move 3; the moves after it follow on. "
                                 "Now move 5 (White D16) lands on a stone: it's red in the game tree.");
    CHECK_EQUAL(game.stonesOnStones().size(), 1u);
  }

  TEST(MovingAnEarlierStoneMayPutALaterOneOnAStone)
  {
    Game game;
    CHECK(game.play(D16).ok);  // 1
    CHECK(game.play(Q4).ok);   // 2
    CHECK(game.play(D4).ok);   // 3
    CHECK(game.play(E15).ok);  // 4, White

    // Move 1 dragged to E15, where White plays move 4.
    game.goToPath({ 0 });
    const Outcome outcome = game.moveStone(D16, E15);
    CHECK(outcome.ok);
    CHECK_EQUAL(outcome.message, "Move 1 is now at E15. Now move 4 (White E15) lands on a stone: it's red in the game tree.");
    CHECK_EQUAL(game.stonesOnStones().size(), 1u);

    // And dragging it back puts it right.
    CHECK(game.moveStone(E15, D16).ok);
    CHECK(game.stonesOnStones().empty());
  }

  TEST(SetUpStonesMayBeInALaterMovesWay)
  {
    Game game;
    CHECK(game.play(K10).ok);  // 1
    game.toStart();

    const Outcome outcome = game.setupStone(K10, Stone::White);
    CHECK(outcome.ok);
    CHECK_EQUAL(outcome.message, "Now move 1 (Black K10) lands on a stone: it's red in the game tree.");
    game.forward();
    CHECK(game.isStoneOnStone(game.current()));
    CHECK_EQUAL(game.stoneOnStoneReason(game.current()),
                "Move 1 (Black K10) is played on top of a white set-up stone (set up at the start), which is still on "
                "the board.");
  }

  TEST(OnlyTheNewOnesAreSaid)
  {
    Game game = StoneOnStoneFile();

    // Pasting moves that fit says nothing of the three already in the file.
    game.goToPath(MOVE_5);
    const Outcome outcome = game.pasteBranch("(;W[qq])");
    CHECK(outcome.ok);
    CHECK_EQUAL(outcome.message, "");
    CHECK_EQUAL(game.stonesOnStones().size(), 3u);
  }

  // ------------------------------------------------------------ in the editor

  TEST(TheStatusLineSaysWhyAndADragPutsItRight)
  {
    TestScenarioGui scenario({ SampleFile("stone-on-stone.sgf") });
    for (int k = 0; k < 4; ++k)
      scenario.press(KEY_RIGHT);
    CHECK_EQUAL(scenario.game().moveNumber(), 4);
    CHECK(scenario.shows("Move 4 (White D16) is played on top of move 1 (Black D16), which is still on the board."));

    // The white stone dragged off D16, to E15: move 4 goes with it.
    scenario.mouseDown(scenario.point(3, 3));
    scenario.moveTo(scenario.point(4, 4));
    scenario.buttonsUp();
    CHECK(scenario.shows("Move 4 is now at E15."));
    CHECK(scenario.stone(3, 3) == Stone::Black);
    CHECK(scenario.stone(4, 4) == Stone::White);
    CHECK(!scenario.game().isStoneOnStone(scenario.game().current()));
  }
}
