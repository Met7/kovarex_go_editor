#include <TestScenarioGui.hpp>
#include <UnitTest.hpp>

SUITE(UnsavedChanges)
{
  using Page = ui::Pages::Page;
  namespace fs = std::filesystem;

  // A move played, then New game's Start: which asks first.
  void startOverAModifiedGame(TestScenarioGui& scenario)
  {
    scenario.clickPoint(3, 3);
    scenario.clickSelectText("New");
    scenario.clickSelectText("Start", scenario.pages().newGame.root());
    CHECK(scenario.pageOpen(Page::Confirm));
  }

  TEST(NoQuestionWithoutChanges)
  {
    TestScenarioGui scenario;
    const Game* before = &scenario.game();
    scenario.clickSelectText("New");
    scenario.clickSelectText("Start", scenario.pages().newGame.root());
    CHECK(!scenario.pages().isOpen());
    CHECK(&scenario.game() != before);
  }

  TEST(Cancel)
  {
    TestScenarioGui scenario;
    startOverAModifiedGame(scenario);
    scenario.clickSelectText("Cancel", scenario.pages().confirm.root());

    CHECK(!scenario.pages().isOpen());
    CHECK(scenario.stone(3, 3) == Stone::Black);
    CHECK(scenario.game().modified());
  }

  TEST(EscapeCancels)
  {
    TestScenarioGui scenario;
    startOverAModifiedGame(scenario);
    scenario.pressEscape();

    CHECK(!scenario.pages().isOpen());
    CHECK(scenario.stone(3, 3) == Stone::Black);
  }

  TEST(DontSave)
  {
    TestScenarioGui scenario;
    startOverAModifiedGame(scenario);
    scenario.clickSelectText("Don't save", scenario.pages().confirm.root());

    CHECK(!scenario.pages().isOpen());
    CHECK(scenario.stone(3, 3) == Stone::None);
    CHECK(!scenario.game().modified());
    CHECK(fs::is_empty(TestScenarioGui::folder()));
  }

  TEST(SaveUntitledAsksForANameAndThenGoesOn)
  {
    TestScenarioGui scenario;
    startOverAModifiedGame(scenario);
    scenario.clickSelectText("Save", scenario.pages().confirm.root());
    CHECK(scenario.pageOpen(Page::Files));
    scenario.fillField(scenario.pages().files.root(), "File name", "kept.sgf");
    scenario.pressEnter();

    CHECK(!scenario.pages().isOpen());
    CHECK(ReadFile(TestScenarioGui::folder() / "kept.sgf").find(";B[dd]") != std::string::npos);
    // And the new game started.
    CHECK(scenario.stone(3, 3) == Stone::None);
    CHECK(scenario.app().file().empty());
  }

  TEST(SaveNamedSavesAndGoesOn)
  {
    TestScenarioGui scenario;
    scenario.clickSelectText("Save as");
    scenario.fillField(scenario.pages().files.root(), "File name", "named.sgf");
    scenario.pressEnter();

    startOverAModifiedGame(scenario);
    scenario.clickSelectText("Save", scenario.pages().confirm.root());
    CHECK(!scenario.pages().isOpen());
    CHECK(ReadFile(TestScenarioGui::folder() / "named.sgf").find(";B[dd]") != std::string::npos);
    CHECK(scenario.stone(3, 3) == Stone::None);
  }

  TEST(OpeningAsksToo)
  {
    TestScenarioGui scenario;
    WriteFile(TestScenarioGui::folder() / "other.sgf", "(;GM[1]FF[4]SZ[19]PB[Other])");
    scenario.clickPoint(3, 3);
    scenario.clickSelectText("Open");
    scenario.fillField(scenario.pages().files.root(), "File name", "other.sgf");
    scenario.pressEnter();
    CHECK(scenario.pageOpen(Page::Confirm));
    scenario.clickSelectText("Don't save", scenario.pages().confirm.root());

    CHECK_EQUAL(scenario.game().root().get("PB"), "Other");
  }
}
