#include <TestScenarioGui.hpp>
#include <UnitTest.hpp>

#include <ui/EditorView.hpp>

SUITE(SetupTools)
{
  TEST(EachColourHasItsTool)
  {
    TestScenarioGui scenario;
    ui::EditorView& editor = scenario.app().interface().editor();

    // Each puts down its own colour, and the other with Shift.
    editor.run(ToolCommand(Tool::SetupBlack));
    scenario.clickPoint(3, 3);
    scenario.clickPoint(4, 3, GuiControl::Shift);
    editor.run(ToolCommand(Tool::SetupWhite));
    scenario.clickPoint(5, 3);
    scenario.clickPoint(6, 3, GuiControl::Shift);
    CHECK(scenario.stone(3, 3) == Stone::Black);
    CHECK(scenario.stone(4, 3) == Stone::White);
    CHECK(scenario.stone(5, 3) == Stone::White);
    CHECK(scenario.stone(6, 3) == Stone::Black);
    CHECK_EQUAL(scenario.game().moveNumber(), 0);

    // A click on a stone of the tool's colour takes it off.
    scenario.clickPoint(5, 3);
    CHECK(scenario.stone(5, 3) == Stone::None);

    // Clear takes off the one clicked.
    scenario.clickSelectText("Clear");
    scenario.clickPoint(3, 3);
    CHECK(scenario.stone(3, 3) == Stone::None);
    CHECK(scenario.stone(4, 3) == Stone::White);
  }

  TEST(ClearAllTakesOffEverySetUpStone)
  {
    TestScenarioGui scenario;
    ui::EditorView& editor = scenario.app().interface().editor();
    scenario.clickPoint(10, 10);  // a move: it stays
    editor.run(ToolCommand(Tool::SetupBlack));
    scenario.clickPoint(3, 3);
    scenario.clickPoint(15, 15, GuiControl::Shift);

    scenario.clickSelectText("Clear all");
    CHECK(scenario.stone(3, 3) == Stone::None);
    CHECK(scenario.stone(15, 15) == Stone::None);
    CHECK(scenario.stone(10, 10) == Stone::Black);

    // One edit: one undo brings both back.
    CHECK(scenario.game().undo());
    CHECK(scenario.stone(3, 3) == Stone::Black);
    CHECK(scenario.stone(15, 15) == Stone::White);

    // Nothing to clear, and it says so.
    scenario.clickPoint(3, 3);
    scenario.clickPoint(15, 15, GuiControl::Shift);
    scenario.clickSelectText("Clear all");
    CHECK(scenario.shows("There are no set-up stones to clear."));
  }

  TEST(TheControlsPageListsThem)
  {
    TestScenarioGui scenario;
    scenario.clickSelectText("Controls");
    CHECK(scenario.shows("White set-up stones"));
    CHECK(scenario.shows("Clear all set-up stones"));
  }
}
