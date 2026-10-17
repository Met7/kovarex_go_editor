#include <TestScenarioGui.hpp>
#include <UnitTest.hpp>

#include <game/Sgf.hpp>

#include <Agui/Widget/DropDown.hpp>

SUITE(NewGame)
{
  using Page = ui::Pages::Page;

  // The New game page, opened from the top bar.
  agui::Window& openNewGame(TestScenarioGui& scenario)
  {
    scenario.clickSelectText("New");
    CHECK(scenario.pageOpen(Page::NewGame));
    return scenario.pages().newGame.root();
  }

  TEST(StartButton)
  {
    TestScenarioGui scenario;
    agui::Window& page = openNewGame(scenario);
    scenario.fillField(page, "Black", "Lee Sedol");
    scenario.fillField(page, "White", "AlphaGo");
    scenario.clickSelectText("Start", page);

    CHECK(!scenario.pages().isOpen());
    CHECK_EQUAL(scenario.game().root().get("PB"), "Lee Sedol");
    CHECK_EQUAL(scenario.game().root().get("PW"), "AlphaGo");
    CHECK_EQUAL(scenario.game().width(), 19);
    CHECK_EQUAL(scenario.game().moveNumber(), 0);
  }

  TEST(EnterInFieldStarts)
  {
    for (const char* field : { "Komi", "Black", "White" })
    {
      TestScenarioGui scenario;
      agui::Window& page = openNewGame(scenario);
      scenario.fillField(page, "Black", "Cho Chikun");
      scenario.click(scenario.field(page, field));
      scenario.pressEnter();

      CHECK(!scenario.pages().isOpen());
      CHECK_EQUAL(scenario.game().root().get("PB"), "Cho Chikun");
    }
  }

  TEST(KomiIsKept)
  {
    TestScenarioGui scenario;
    agui::Window& page = openNewGame(scenario);
    scenario.fillField(page, "Komi", "5.5");
    scenario.pressEnter();
    CHECK_EQUAL(scenario.game().root().get("KM"), "5.5");
  }

  TEST(BackStartsNothing)
  {
    TestScenarioGui scenario;
    const Game* before = &scenario.game();
    agui::Window& page = openNewGame(scenario);
    scenario.fillField(page, "Black", "Nobody");
    scenario.clickSelectText("Back", page);

    CHECK(!scenario.pages().isOpen());
    CHECK_EQUAL(&scenario.game(), before);
    CHECK_EQUAL(scenario.game().root().get("PB"), "");
  }

  TEST(EscapeClosesThePage)
  {
    TestScenarioGui scenario;
    const Game* before = &scenario.game();
    openNewGame(scenario);
    scenario.pressEscape();

    CHECK(!scenario.pages().isOpen());
    CHECK_EQUAL(&scenario.game(), before);
  }

  TEST(ShortcutOpensThePage)
  {
    TestScenarioGui scenario;
    scenario.press(KEY_N, GuiControl::Ctrl);
    CHECK(scenario.pageOpen(Page::NewGame));
  }

  TEST(SmallBoard)
  {
    TestScenarioGui scenario;
    agui::Window& page = openNewGame(scenario);
    scenario.click(scenario.getWidget<agui::DropDown>(page));
    scenario.clickSelectText("9 x 9");
    scenario.clickSelectText("Start", page);

    CHECK_EQUAL(scenario.game().width(), 9);
    CHECK_EQUAL(scenario.game().height(), 9);
    scenario.clickPoint(8, 8);
    CHECK(scenario.stone(8, 8) == Stone::Black);
  }

  TEST(NextGameStartsAsTheLastOne)
  {
    TestScenarioGui scenario;
    agui::Window& page = openNewGame(scenario);
    scenario.fillField(page, "Black", "Honinbo Shusaku");
    scenario.pressEnter();

    openNewGame(scenario);
    CHECK_EQUAL(scenario.field(page, "Black")->getText(), "Honinbo Shusaku");
  }
}
