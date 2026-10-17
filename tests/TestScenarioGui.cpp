#include <TestScenarioGui.hpp>

#include <app/Settings.hpp>

#include <Agui/Widget/Label.hpp>

#include <fstream>
#include <sstream>

TestScenarioGui::TestScenarioGui(const std::vector<std::filesystem::path>& files)
{
  std::filesystem::remove_all(folder());
  std::filesystem::create_directories(folder());

  // What the App reads when it starts. The window fits on any screen the tests
  // run on, the interface is at 100% whatever its size, nothing waits for the
  // monitor, and no tooltip comes up over what is clicked next.
  Settings settings;
  settings.graphics.vsync          = false;
  settings.graphics.automaticScale = false;
  settings.graphics.interfaceScale = 100;
  settings.graphics.tooltipDelay   = Settings::Graphics::TOOLTIPS_NEVER;
  settings.window.width            = 1280;
  settings.window.height           = 800;
  settings.folder                  = folder().string();
  settings.save();

  input::Simulate(&this->input);
  this->application = std::make_unique<App>(files);
  // The first frames lay everything out.
  this->updateMe(2);
}

TestScenarioGui::~TestScenarioGui()
{
  this->application.reset();
  input::Simulate(nullptr);
}

void TestScenarioGui::updateMe(uint32_t count)
{
  for (uint32_t i = 0; i < count; ++i)
    this->application->frame();
}

float TestScenarioGui::viewScale()
{
  return this->app().interface().viewScale();
}

Game& TestScenarioGui::game()
{
  Game* game = this->app().currentGame();
  if (!game)
    throw std::runtime_error("There is no game");
  return *game;
}

ui::PointWidget* TestScenarioGui::point(int x, int y, std::source_location location)
{
  // The board builds its points row by row, in one parent: the Gui gives
  // them back in that order.
  std::vector<ui::PointWidget*> points;
  this->findWidget<ui::PointWidget>([&points](ui::PointWidget* point) { points.push_back(point); return false; });
  const Game& game = this->game();
  if (x < 0 || y < 0 || x >= game.width() || y >= game.height() || points.size() != size_t(game.width() * game.height()))
  {
    CHECK_FAILED_SOURCELOCATION(ssprintf("No point %d, %d on a %dx%d board of %zu points", x, y, game.width(), game.height(),
                                         points.size()).c_str(), location);
    return nullptr;
  }
  return points[size_t(y) * size_t(game.width()) + size_t(x)];
}

void TestScenarioGui::clickPoint(int x, int y, Modifiers modifiers, std::source_location location)
{
  this->click(this->point(x, y, location), agui::MouseButton::LEFT, modifiers, location);
}

agui::TextField* TestScenarioGui::field(agui::Widget& parent, const std::string& name, std::source_location location)
{
  agui::Label* label = this->getWidget<agui::Label>(parent, [&name](agui::Label* label) { return label->getText() == name; }, location);
  if (!label || !label->getParent())
    return nullptr;
  return this->getWidget<agui::TextField>(*label->getParent(), nullptr, location);
}

void TestScenarioGui::fillField(agui::Widget& parent, const std::string& name, const std::string& text, std::source_location location)
{
  agui::TextField* field = this->field(parent, name, location);
  if (!field)
    return;
  this->click(field, agui::MouseButton::LEFT, NoModifiers, location);
  this->pressSelectAll();
  this->press(KEY_BACKSPACE);
  this->write(text);
}

std::filesystem::path TestScenarioGui::folder()
{
  return std::filesystem::temp_directory_path() / "kovarex_go_editor_tests" / "files";
}

std::string ReadFile(const std::filesystem::path& file)
{
  std::ifstream in(file, std::ios::binary);
  std::stringstream text;
  text << in.rdbuf();
  return text.str();
}

void WriteFile(const std::filesystem::path& file, const std::string& text)
{
  std::ofstream out(file, std::ios::binary);
  out << text;
}
