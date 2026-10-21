#include <TestScenarioGui.hpp>
#include <UnitTest.hpp>

#include <ui/EditorView.hpp>

SUITE(MarkBox)
{
  namespace fs = std::filesystem;

  // Stones at D16, E16 and D15 (black, white, black), the rest of the corner
  // empty: the editor at the last of them, with `tool`.
  struct Scenario
  {
    explicit Scenario(Tool tool)
      : scenario({ Written() })
    {
      ui::EditorView& editor = this->scenario.app().interface().editor();
      editor.run(Command::End);
      editor.run(ToolCommand(tool));
    }
    static fs::path Written()
    {
      const fs::path file = TestScenarioGui::folder().parent_path() / "mark-box.sgf";
      WriteFile(file, "(;GM[1]FF[4]SZ[19];B[dd];W[ed];B[de])");
      return file;
    }
    bool has(Mark mark, int x, int y) { return this->scenario.game().hasMark(Point{ x, y }, mark); }
    // From the corner A19 to F14: the three stones, and 33 empty points.
    void box(GuiControl::Modifiers modifiers) { this->scenario.drag(this->scenario.point(0, 0), this->scenario.point(5, 5), modifiers); }

    TestScenarioGui scenario;
  };

  TEST(ShiftDragMarksTheStonesInTheBox)
  {
    Scenario s(Tool::Triangle);
    s.box(GuiControl::Shift);
    CHECK(s.has(Mark::Triangle, 3, 3));
    CHECK(s.has(Mark::Triangle, 4, 3));
    CHECK(s.has(Mark::Triangle, 3, 4));
    CHECK(!s.has(Mark::Triangle, 0, 0));  // empty
    CHECK(!s.has(Mark::Triangle, 5, 5));
    CHECK(!s.has(Mark::Triangle, 6, 6));  // outside

    // All in one edit.
    CHECK(s.scenario.game().undo());
    CHECK(!s.has(Mark::Triangle, 3, 3));
    CHECK(!s.has(Mark::Triangle, 3, 4));
  }

  TEST(TerritoryFillsTheEmptyPoints)
  {
    Scenario s(Tool::TerritoryBlack);
    s.box(GuiControl::Shift);
    CHECK(s.has(Mark::TerritoryBlack, 0, 0));
    CHECK(s.has(Mark::TerritoryBlack, 5, 5));
    CHECK(!s.has(Mark::TerritoryBlack, 3, 3));  // a stone
    CHECK(!s.has(Mark::TerritoryBlack, 6, 6));  // outside
  }

  TEST(CtrlShiftDragTakesThemOffTheBox)
  {
    Scenario s(Tool::Square);
    s.scenario.clickPoint(3, 3);
    s.scenario.clickPoint(6, 6);
    s.box(GuiControl::Modifiers(GuiControl::Ctrl | GuiControl::Shift));
    CHECK(!s.has(Mark::Square, 3, 3));
    CHECK(s.has(Mark::Square, 6, 6));  // outside
  }

  TEST(CtrlDragOnlyTakesOff)
  {
    Scenario s(Tool::Circle);
    s.scenario.clickPoint(4, 3);
    // From an unmarked point across a marked one: a plain drag would put
    // circles on, from the first point; with Ctrl, it takes them off.
    s.scenario.drag(s.scenario.point(2, 3), s.scenario.point(5, 3), GuiControl::Ctrl);
    CHECK(!s.has(Mark::Circle, 2, 3));
    CHECK(!s.has(Mark::Circle, 4, 3));
    CHECK(!s.has(Mark::Circle, 5, 3));
    // A plain drag still paints whatever it crosses.
    s.scenario.drag(s.scenario.point(2, 3), s.scenario.point(5, 3));
    CHECK(s.has(Mark::Circle, 2, 3));
    CHECK(s.has(Mark::Circle, 5, 3));
  }

  TEST(CtrlShiftDragClearsTheLabelsInTheBox)
  {
    Scenario s(Tool::Letter);
    s.scenario.clickPoint(3, 3);
    s.scenario.clickPoint(1, 1);
    s.scenario.clickPoint(6, 6);
    CHECK_EQUAL(s.scenario.game().current().values("LB").size(), 3u);
    // Shift alone puts no labels on a box.
    s.box(GuiControl::Shift);
    CHECK_EQUAL(s.scenario.game().current().values("LB").size(), 3u);
    s.box(GuiControl::Modifiers(GuiControl::Ctrl | GuiControl::Shift));
    CHECK_EQUAL(s.scenario.game().current().values("LB").size(), 1u);  // G13's, outside
    CHECK(s.scenario.game().undo());
    CHECK_EQUAL(s.scenario.game().current().values("LB").size(), 3u);
  }

  TEST(CtrlShiftDragClearsTheSetUpStonesInTheBox)
  {
    Scenario s(Tool::SetupBlack);
    s.scenario.clickPoint(1, 1);
    s.scenario.clickPoint(2, 1);
    s.scenario.clickPoint(7, 7);
    s.box(GuiControl::Modifiers(GuiControl::Ctrl | GuiControl::Shift));
    CHECK(s.scenario.stone(1, 1) == Stone::None);
    CHECK(s.scenario.stone(2, 1) == Stone::None);
    CHECK(s.scenario.stone(7, 7) == Stone::Black);  // outside
    CHECK(s.scenario.stone(3, 3) == Stone::Black);  // a move's
    CHECK(s.scenario.stone(4, 3) == Stone::White);
    // One edit: one undo brings both back.
    CHECK(s.scenario.game().undo());
    CHECK(s.scenario.stone(1, 1) == Stone::Black);
    CHECK(s.scenario.stone(2, 1) == Stone::Black);
  }

  TEST(TheControlsPageListsTheBoxes)
  {
    TestScenarioGui scenario;
    scenario.clickSelectText("Controls");
    CHECK(scenario.shows("Marks/territory in an area"));
    CHECK(scenario.shows("Clear marks in an area"));
  }
}
