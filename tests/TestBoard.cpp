#include <TestScenarioGui.hpp>
#include <UnitTest.hpp>

SUITE(Board)
{
  TEST(ClickPlaysMovesInTurn)
  {
    TestScenarioGui scenario;
    scenario.clickPoint(3, 3);
    scenario.clickPoint(15, 15);

    CHECK(scenario.stone(3, 3) == Stone::Black);
    CHECK(scenario.stone(15, 15) == Stone::White);
    CHECK(scenario.stone(15, 3) == Stone::None);
    CHECK_EQUAL(scenario.game().moveNumber(), 2);
    CHECK(scenario.game().modified());
  }

  TEST(Capture)
  {
    TestScenarioGui scenario;
    scenario.clickPoint(1, 0);  // black
    scenario.clickPoint(0, 0);  // white, in the corner
    scenario.clickPoint(0, 1);  // black takes it

    CHECK(scenario.stone(0, 0) == Stone::None);
    CHECK(scenario.stone(1, 0) == Stone::Black);
    CHECK(scenario.stone(0, 1) == Stone::Black);
  }

  TEST(UndoAndRedo)
  {
    TestScenarioGui scenario;
    scenario.clickPoint(3, 3);
    scenario.clickPoint(15, 15);

    scenario.clickSelectText("Undo");
    CHECK(scenario.stone(15, 15) == Stone::None);
    CHECK(scenario.stone(3, 3) == Stone::Black);

    scenario.press(KEY_Z, GuiControl::Ctrl);
    CHECK(scenario.stone(3, 3) == Stone::None);
    CHECK(!scenario.game().modified());

    scenario.clickSelectText("Redo");
    CHECK(scenario.stone(3, 3) == Stone::Black);
  }

  TEST(StepThroughTheGame)
  {
    TestScenarioGui scenario;
    scenario.clickPoint(3, 3);
    scenario.clickPoint(15, 15);
    scenario.clickPoint(15, 3);
    CHECK_EQUAL(scenario.game().moveNumber(), 3);

    scenario.press(KEY_LEFT);
    CHECK_EQUAL(scenario.game().moveNumber(), 2);
    CHECK(scenario.stone(15, 3) == Stone::None);

    scenario.clickSelectText("|<");
    CHECK_EQUAL(scenario.game().moveNumber(), 0);
    CHECK(scenario.stone(3, 3) == Stone::None);

    scenario.clickSelectText(">");
    CHECK_EQUAL(scenario.game().moveNumber(), 1);

    scenario.clickSelectText(">|");
    CHECK_EQUAL(scenario.game().moveNumber(), 3);
    CHECK(scenario.stone(15, 3) == Stone::Black);
  }

  TEST(Pass)
  {
    TestScenarioGui scenario;
    scenario.clickSelectText("Pass");
    scenario.clickPoint(3, 3);
    CHECK_EQUAL(scenario.game().moveNumber(), 2);
    CHECK(scenario.stone(3, 3) == Stone::White);
  }

  TEST(BoardTakesNoClicksUnderAPage)
  {
    TestScenarioGui scenario;
    scenario.clickSelectText("Game info");
    scenario.moveTo(scenario.point(9, 9));
    // The page's dark sheet is over it.
    CHECK(agui::Gui::instance->getWidgetUnderMouse() != scenario.point(9, 9));
    scenario.pressEscape();
    scenario.clickPoint(9, 9);
    CHECK(scenario.stone(9, 9) == Stone::Black);
  }
}
