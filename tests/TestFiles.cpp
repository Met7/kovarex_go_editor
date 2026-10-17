#include <TestScenarioGui.hpp>
#include <UnitTest.hpp>

#include <game/Sgf.hpp>

SUITE(Files)
{
  using Page = ui::Pages::Page;
  namespace fs = std::filesystem;

  // On the file page, which is up: types the name, and confirms with the
  // button or with Enter.
  void chooseFile(TestScenarioGui& scenario, const std::string& name, bool enter)
  {
    CHECK(scenario.pageOpen(Page::Files));
    agui::Window& page = scenario.pages().files.root();
    scenario.fillField(page, "File name", name);
    if (enter)
      scenario.pressEnter();
    else
      scenario.click(scenario.getWidget<agui::Widget>(page, [](agui::Widget* widget) {
        return widget->getText() == "Save" || widget->getText() == "Open";
      }));
  }

  TEST(SaveAs)
  {
    for (bool enter : { false, true })
    {
      TestScenarioGui scenario;
      scenario.clickPoint(3, 3);
      scenario.clickSelectText("Save as");
      CHECK(scenario.pageOpen(Page::Files));
      CHECK_EQUAL(scenario.pages().files.root().title.getText(), "Save as");
      chooseFile(scenario, "first.sgf", enter);

      const fs::path file = TestScenarioGui::folder() / "first.sgf";
      CHECK(!scenario.pages().isOpen());
      CHECK(fs::exists(file));
      CHECK(ReadFile(file).find(";B[dd]") != std::string::npos);
      CHECK_EQUAL(scenario.app().file(), file);
      CHECK(!scenario.game().modified());
      CHECK(scenario.shows("Saved first.sgf."));
    }
  }

  TEST(SaveUntitledAsksForAName)
  {
    TestScenarioGui scenario;
    scenario.clickPoint(3, 3);
    scenario.clickSelectText("Save");
    CHECK(scenario.pageOpen(Page::Files));
    agui::TextField* name = scenario.field(scenario.pages().files.root(), "File name");
    CHECK_EQUAL(name->getText(), "game.sgf");
    // The field doesn't have the caret when the page opens: Enter needs it.
    scenario.click(name);
    scenario.pressEnter();

    CHECK(fs::exists(TestScenarioGui::folder() / "game.sgf"));
  }

  TEST(SaveAgainWritesTheSameFile)
  {
    TestScenarioGui scenario;
    scenario.clickPoint(3, 3);
    scenario.clickSelectText("Save as");
    chooseFile(scenario, "game.sgf", true);

    scenario.clickPoint(15, 15);
    CHECK(scenario.game().modified());
    scenario.clickSelectText("Save");
    CHECK(!scenario.pages().isOpen());
    CHECK(!scenario.game().modified());
    const std::string text = ReadFile(TestScenarioGui::folder() / "game.sgf");
    CHECK(text.find(";B[dd]") != std::string::npos);
    CHECK(text.find(";W[pp]") != std::string::npos);

    scenario.clickPoint(15, 3);
    scenario.press(KEY_S, GuiControl::Ctrl);
    CHECK(!scenario.game().modified());
    CHECK(ReadFile(TestScenarioGui::folder() / "game.sgf").find(";B[pd]") != std::string::npos);
  }

  TEST(GameInfoIsSaved)
  {
    TestScenarioGui scenario;
    scenario.clickSelectText("New");
    scenario.fillField(scenario.pages().newGame.root(), "Black", "Go Seigen");
    scenario.pressEnter();
    scenario.clickSelectText("Save as");
    chooseFile(scenario, "info.sgf", true);
    CHECK(ReadFile(TestScenarioGui::folder() / "info.sgf").find("PB[Go Seigen]") != std::string::npos);
  }

  TEST(OpenByName)
  {
    TestScenarioGui scenario;
    WriteFile(TestScenarioGui::folder() / "old.sgf", "(;GM[1]FF[4]SZ[13]PB[Black One]PW[White Two];B[cc];W[kk])");
    scenario.clickSelectText("Open");
    CHECK_EQUAL(scenario.pages().files.root().title.getText(), "Open");
    chooseFile(scenario, "old.sgf", false);

    CHECK(!scenario.pages().isOpen());
    CHECK_EQUAL(scenario.app().file(), TestScenarioGui::folder() / "old.sgf");
    CHECK_EQUAL(scenario.game().width(), 13);
    CHECK_EQUAL(scenario.game().root().get("PB"), "Black One");
    CHECK(!scenario.game().modified());
  }

  TEST(OpenByDoubleClick)
  {
    TestScenarioGui scenario;
    WriteFile(TestScenarioGui::folder() / "listed.sgf", "(;GM[1]FF[4]SZ[19];B[dd])");
    scenario.press(KEY_O, GuiControl::Ctrl);
    CHECK(scenario.pageOpen(Page::Files));
    scenario.doubleClick(scenario.getWidget<agui::Widget>(scenario.pages().files.root(),
                                                          [](agui::Widget* widget) { return widget->getText() == "listed.sgf"; }));

    CHECK(!scenario.pages().isOpen());
    CHECK_EQUAL(scenario.app().file(), TestScenarioGui::folder() / "listed.sgf");
  }

  TEST(OpenFromTheCommandLine)
  {
    const fs::path file = TestScenarioGui::folder().parent_path() / "given.sgf";
    WriteFile(file, "(;GM[1]FF[4]SZ[9]PB[Given];B[ee])");
    TestScenarioGui scenario({ file });

    CHECK_EQUAL(scenario.app().file(), file);
    CHECK_EQUAL(scenario.game().width(), 9);
    CHECK_EQUAL(scenario.game().root().get("PB"), "Given");
  }

  TEST(BrokenFileSaysWhy)
  {
    TestScenarioGui scenario;
    WriteFile(TestScenarioGui::folder() / "broken.sgf", "this is not a game");
    const Game* before = &scenario.game();
    scenario.clickSelectText("Open");
    chooseFile(scenario, "broken.sgf", true);

    CHECK_EQUAL(&scenario.game(), before);
    CHECK(scenario.app().file().empty());
    CHECK(scenario.findWidget<agui::Widget>([](agui::Widget* widget) {
      return widget->getText().starts_with("Couldn't read broken.sgf");
    }));
  }

  TEST(SaveThenOpenRoundTrip)
  {
    {
      TestScenarioGui scenario;
      scenario.clickPoint(3, 3);
      scenario.clickPoint(15, 15);
      scenario.clickPoint(2, 16);
      scenario.clickSelectText("Save as");
      chooseFile(scenario, "round.sgf", true);
    }
    // folder() is emptied by the next scenario, so the file is kept aside.
    const fs::path kept = TestScenarioGui::folder().parent_path() / "round.sgf";
    fs::copy_file(TestScenarioGui::folder() / "round.sgf", kept, fs::copy_options::overwrite_existing);

    TestScenarioGui scenario({ kept });
    CHECK_EQUAL(scenario.game().moveNumber(), 0);
    scenario.clickSelectText(">|");
    CHECK_EQUAL(scenario.game().moveNumber(), 3);
    CHECK(scenario.stone(3, 3) == Stone::Black);
    CHECK(scenario.stone(15, 15) == Stone::White);
    CHECK(scenario.stone(2, 16) == Stone::Black);
  }
}
