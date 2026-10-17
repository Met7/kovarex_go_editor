// The editor, running, for a test to play: as Factorio's TestScenarioGui is
// the game with a player in it. Each one starts from nothing -- settings of
// its own, written fresh, and an empty folder for files (folder()) -- and
// opens the window, so what a test does can't leak into the next one.
//
//   TestScenarioGui scenario;
//   scenario.clickSelectText("New");
//   scenario.clickSelectText("Start", scenario.pages().newGame.root());
//   scenario.clickPoint(3, 3);
//   CHECK(scenario.stone(3, 3) == Stone::Black);

#pragma once

#include <GuiControl.hpp>

#include <app/App.hpp>
#include <game/Game.hpp>
#include <ui/BoardView.hpp>
#include <ui/Pages.hpp>

#include <Agui/Widget/TextField.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class TestScenarioGui : public GuiControl
{
public:
  // Opens `files` as if given on the command line, like a .sgf double-clicked.
  explicit TestScenarioGui(const std::vector<std::filesystem::path>& files = {});
  ~TestScenarioGui() override;

  void  updateMe(uint32_t count = 1) override;
  float viewScale() override;

  App&        app() { return *this->application; }
  Game&       game();
  ui::Pages&  pages() { return this->app().interface().pages(); }
  bool        pageOpen(ui::Pages::Page page) { return this->pages().current() == page; }

  // The board's point at (x, y), (0, 0) the top left one; and a click on it.
  ui::PointWidget* point(int x, int y, std::source_location location = std::source_location::current());
  void             clickPoint(int x, int y, Modifiers modifiers = NoModifiers,
                              std::source_location location = std::source_location::current());
  Stone            stone(int x, int y) { return this->game().position().at(Point{ x, y }); }

  // The text field in `parent` on the row named `name`, as Form.hpp's
  // namedRow() lays them out: "Black" on the New game page, say.
  agui::TextField* field(agui::Widget& parent, const std::string& name,
                         std::source_location location = std::source_location::current());
  // Clicks into it, takes out what is there and types `text`.
  void fillField(agui::Widget& parent, const std::string& name, const std::string& text,
                 std::source_location location = std::source_location::current());

  // Where the scenario keeps its files, emptied when it starts.
  static std::filesystem::path folder();
  // Whether the status line, or anything else on screen, says `text`.
  bool shows(const std::string& text) { return this->findWidgetWithText(text) != nullptr; }

private:
  std::unique_ptr<App> application;
};

// A file as the editor wrote it.
std::string ReadFile(const std::filesystem::path& file);
void        WriteFile(const std::filesystem::path& file, const std::string& text);
