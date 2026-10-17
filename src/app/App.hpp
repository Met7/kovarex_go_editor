// The application: the window, the game being edited and the file it came
// from, and the frame loop. Each frame the shortcuts are read first, then the
// Gui runs, then whatever either asked for is done.
//
// The App is what deals with files: opening, saving, starting a new game, and
// asking first when any of those -- or closing the window -- would throw
// away unsaved changes.

#pragma once

#include <app/Settings.hpp>
#include <app/Sharing.hpp>
#include <game/Game.hpp>
#include <ui/Commands.hpp>
#include <ui/GuiLayer.hpp>
#include <ui/Shortcuts.hpp>

#include <filesystem>
#include <future>
#include <memory>
#include <string>
#include <vector>

class App {
public:
  // Opens the first of `files` (a file double-clicked in Explorer arrives on
  // the command line), or starts with an empty board.
  explicit App(const std::vector<std::filesystem::path>& files);

  // Runs until the window closes.
  int run();

  // One frame of run(): the tests run the editor a frame at a time, and look
  // at what it holds in between.
  void frame();
  Game*                        currentGame() const { return this->game.get(); }
  const std::filesystem::path& file() const { return this->path; }
  ui::GuiLayer&                interface() { return this->gui; }
  Settings&                    config() { return this->settings; }
  bool                         quitting() const { return this->quitRequested; }

private:
  // Opens the window on construction and closes it on destruction. Declared
  // before every member that needs the GL context, so it brackets them.
  struct Window {
    explicit Window(const Settings& settings);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
  };

  // What to do once the unsaved-changes question has been answered.
  enum class After { None, NewGame, Open, Quit };

  void handle(Command command);
  void handlePages();
  void handleDroppedFiles();
  void handleClose();
  void updateSettings();
  // The interface scale's controls: a step up or down, or back to automatic.
  void scale(Command command);
  void updateTitle();
  // Sends this frame's edits to the others sharing the game, and applies
  // theirs; the lines drawn on the board both ways.
  void updateSharing();

  // Asks about unsaved changes, if there are any, and then does `then`.
  void guard(After then, const std::filesystem::path& file = {});
  void proceed();

  // A game as the New game page is set up; at startup, an empty 19x19
  // board, whatever that page was last set to -- no handicap stones, other
  // size or players' names that nobody asked for this time.
  void newGame();
  void emptyBoard();
  void start(GameSetup setup);
  bool open(const std::filesystem::path& file);
  bool save(const std::filesystem::path& file);
  void saveOrAsk();
  void showFiles(bool saving);
  // The game to AI Sensei for review: its upload page, opened in the web
  // browser with the game already on it.
  void sendToAiSensei();

  // The file's name, or "Untitled" before it has one.
  std::string name() const;
  std::filesystem::path folder() const;

  // Read before the window opens, which is sized from it.
  Settings settings = Settings::load();
  Settings applied  = this->settings;  // what the window was last set to
  int      shownScale = 0;             // the interface scale the GUI is drawn at

  // The stones painted and the wood decoded while the window opens, which
  // takes the graphics driver a good third of a second: begun before it.
  std::future<ui::GoSprites::Pixels> pictures = std::async(std::launch::async, &ui::GoSprites::prepare);

  Window       window{ this->settings };
  ui::GuiLayer gui{ this->settings, this->pictures };
  ui::Shortcuts shortcuts;

  // The game shared with other editors, when it is.
  Sharing sharing;
  bool    wasJoined = false;  // in a session last frame

  std::unique_ptr<Game>  game;
  std::filesystem::path  path;  // empty until the game is saved or was opened

  After                 after = After::None;
  std::filesystem::path afterFile;
  bool                  saving = false;  // the file browser is up for Save as, not Open

  std::string shownTitle;
  bool        quitRequested = false;
};
