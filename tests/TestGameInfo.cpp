#include <TestScenarioGui.hpp>
#include <UnitTest.hpp>

#include <game/Sgf.hpp>

SUITE(GameInfo)
{
  using Page = ui::Pages::Page;

  agui::Window& openGameInfo(TestScenarioGui& scenario)
  {
    scenario.clickSelectText("Game info");
    CHECK(scenario.pageOpen(Page::GameInfo));
    return scenario.pages().gameInfo.root();
  }

  TEST(Apply)
  {
    TestScenarioGui scenario;
    agui::Window& page = openGameInfo(scenario);
    scenario.fillField(page, "Event", "Test cup");
    scenario.fillField(page, "Result", "B+R");
    scenario.clickSelectText("Apply", page);

    CHECK(!scenario.pages().isOpen());
    CHECK_EQUAL(scenario.game().root().get("EV"), "Test cup");
    CHECK_EQUAL(scenario.game().root().get("RE"), "B+R");
    CHECK(scenario.game().modified());
  }

  TEST(EnterApplies)
  {
    TestScenarioGui scenario;
    agui::Window& page = openGameInfo(scenario);
    scenario.fillField(page, "Place", "Prague");
    scenario.pressEnter();

    CHECK(!scenario.pages().isOpen());
    CHECK_EQUAL(scenario.game().root().get("PC"), "Prague");
  }

  TEST(BackChangesNothing)
  {
    TestScenarioGui scenario;
    agui::Window& page = openGameInfo(scenario);
    scenario.fillField(page, "Event", "Never");
    scenario.clickSelectText("Back", page);

    CHECK(!scenario.pages().isOpen());
    CHECK_EQUAL(scenario.game().root().get("EV"), "");
    CHECK(!scenario.game().modified());
  }

  TEST(ShowsWhatTheGameSays)
  {
    TestScenarioGui scenario;
    agui::Window& page = openGameInfo(scenario);
    scenario.fillField(page, "Black", "Takemiya");
    scenario.pressEnter();

    openGameInfo(scenario);
    CHECK_EQUAL(scenario.field(page, "Black")->getText(), "Takemiya");
  }

  TEST(UndoTakesItBack)
  {
    TestScenarioGui scenario;
    agui::Window& page = openGameInfo(scenario);
    scenario.fillField(page, "Event", "Oops");
    scenario.pressEnter();
    scenario.press(KEY_Z, GuiControl::Ctrl);
    CHECK_EQUAL(scenario.game().root().get("EV"), "");
  }
}
