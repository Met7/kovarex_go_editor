#include <app/App.hpp>

#include <app/Platform.hpp>
#include <game/Config.hpp>
#include <game/Sgf.hpp>
#include <ui/AguiRaylib.hpp>
#include <ui/EditorView.hpp>
#include <ui/Fonts.hpp>
#include <ui/Input.hpp>
#include <ui/Pages.hpp>

#include <raylib.h>
// raylib has no way to take back a click on the window's close button, so
// that one call goes to GLFW, which raylib is built on, directly.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <chrono>
#include <cstdio>

// The icon, embedded for the About page (see fastbuild/fbuild.bff).
namespace ui {
extern const unsigned char APP_ICON_PNG[];
extern const std::size_t   APP_ICON_PNG_SIZE;
}  // namespace ui

namespace {

// Today, as SGF writes dates.
std::string Today()
{
  const std::chrono::year_month_day today{ std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()) };
  char text[16];
  std::snprintf(text, sizeof(text), "%04d-%02u-%02u", int(today.year()), unsigned(today.month()), unsigned(today.day()));
  return text;
}

}  // namespace

App::Window::Window(const Settings& settings)
{
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | (settings.graphics.vsync ? FLAG_VSYNC_HINT : 0));
  InitWindow(settings.window.width, settings.window.height, cfg::APP_NAME);
  SetWindowMinSize(cfg::MIN_WINDOW_W, cfg::MIN_WINDOW_H);
  // Esc belongs to the editor; closing is the close button's job.
  SetExitKey(KEY_NULL);

  // A window too big for this screen -- the default on a full HD monitor,
  // whose taskbar and title bar leave less than its 1016 lines, or a size
  // saved on a bigger monitor -- opens maximized instead. That is as big as
  // it can be, and on full HD the interface still comes out at its 125%.
  if (settings.window.maximized || !platform::WindowFitsOnScreen(GetWindowHandle())) MaximizeWindow();

#ifndef _WIN32
  // Windows gives the window the exe's own icon (resources/goeditor.rc);
  // elsewhere it is given here, from the copy embedded for the About page.
  ::Image icon = LoadImageFromMemory(".png", ui::APP_ICON_PNG, int(ui::APP_ICON_PNG_SIZE));
  SetWindowIcon(icon);
  UnloadImage(icon);
#endif
}

App::Window::~Window()
{
  ui::UnloadFonts();
  agui_raylib::ReleaseCursors();
  CloseWindow();
}

App::App(const std::vector<std::filesystem::path>& files)
{
  ApplySettings(this->settings);
  // Whoever is met on a relay goes on the list of people met.
  this->sharing.setContacts(&this->settings.online.contacts);

  // The shortcuts are read before the Gui each frame; the keys they take
  // never reach it.
  this->gui.setKeyFilter([this](int key) { return this->shortcuts.claimed(key); });

  if (files.empty() || !this->open(files.front())) this->emptyBoard();
}

int App::run()
{
  while (!this->quitRequested) this->frame();
  this->settings.save();  // for the window size, if nothing else
  return 0;
}

void App::frame()
{
  const float dt = GetFrameTime();

  // Shortcuts first, before the Gui hands the same keys to whatever has the
  // focus. Whether a text box has the caret is last frame's answer, which is
  // the one the player was looking at.
  // While the Controls page waits for keys, they are for it, and nothing is
  // a shortcut; a click anywhere gives up waiting (and, on one of its
  // buttons, starts again).
  std::vector<Command> keys;
  ui::ControlsPage& controls = this->gui.pages().controls;
  if (controls.waiting()) {
    if (input::IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || input::IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) controls.stop();
    else if (const std::optional<ui::KeyCombo> pressed = this->shortcuts.capture()) controls.assign(*pressed);
  } else {
    keys = this->shortcuts.poll(this->settings.controls, this->gui.editor().typing(), this->gui.pages().isOpen());
  }

  // Settings next: a change made last frame -- a new interface scale, say --
  // is in place before the Gui lays itself out for this one.
  this->updateSettings();
  this->gui.update(dt);

  for (Command c : keys) this->handle(c);
  for (Command c : this->gui.editor().takeCommands()) this->handle(c);
  this->handlePages();
  this->handleDroppedFiles();
  this->handleClose();
  this->updateSharing();
  this->updateTitle();

  BeginDrawing();
  ClearBackground(cfg::BACKGROUND_COLOR);
  this->gui.draw();
  EndDrawing();
  input::NextFrame();
}

// ---------------------------------------------------------------- commands

void App::handle(Command command)
{
  ui::Pages& pages = this->gui.pages();

  switch (command) {
  case Command::NewGame:
    pages.newGame.refresh();
    pages.open(ui::Pages::Page::NewGame);
    break;
  case Command::Open:
    this->showFiles(false);
    break;
  case Command::Save:
    this->saveOrAsk();
    break;
  case Command::SaveAs:
    this->showFiles(true);
    break;
  case Command::GameInfo:
    if (this->game) pages.gameInfo.load(this->game->root());
    pages.open(ui::Pages::Page::GameInfo);
    break;
  case Command::Settings:
    pages.settings.open();
    pages.settings.setAssociation(platform::IsSgfAssociated() ? ".sgf files open in this program."
                                                              : ".sgf files open in something else, or nothing.",
                                  platform::IsSgfAssociated());
    pages.open(ui::Pages::Page::Settings);
    break;
  case Command::AiSensei:
    this->sendToAiSensei();
    break;
  case Command::Host:
    pages.host.refresh();
    pages.open(ui::Pages::Page::Host);
    break;
  case Command::Join:
    pages.join.refresh();
    pages.open(ui::Pages::Page::Join);
    break;
  case Command::About:
    pages.open(ui::Pages::Page::About);
    break;
  case Command::Controls:
    pages.controls.open();
    pages.open(ui::Pages::Page::Controls);
    break;
  case Command::ScaleUp:
  case Command::ScaleDown:
  case Command::ScaleAutomatic:
    this->scale(command);
    break;
  case Command::FocusSearch:
    pages.focusSearch();
    break;
  case Command::Cancel:
    // Esc on a page first closes its search, if that is open, and then is
    // the page's Back button.
    if (pages.cancelSearch()) break;
    if (pages.isOpen()) {
      pages.close();
      this->after = After::None;
    } else {
      this->gui.editor().run(command);
    }
    break;
  default:
    this->gui.editor().run(command);
    break;
  }
}

void App::handlePages()
{
  ui::Pages& pages = this->gui.pages();

  switch (pages.takeAction()) {
  case ui::Pages::Action::StartGame:
    this->settings.save();  // so the next New game starts where this one did
    this->guard(After::NewGame);
    break;
  case ui::Pages::Action::ApplyGameInfo:
    if (this->game) this->game->setGameInfo(pages.gameInfo.values());
    break;
  case ui::Pages::Action::FileChosen:
    this->settings.folder = platform::ToUtf8(pages.files.folder());
    if (!this->saving) {
      this->guard(After::Open, pages.chosenFile());
    } else if (this->save(pages.chosenFile()) && this->after != After::None) {
      // Saved as part of answering "save the changes first?"
      this->proceed();
    }
    break;
  case ui::Pages::Action::Save:
    if (this->path.empty()) this->showFiles(true);  // then on to `after` once it's saved
    else if (this->save(this->path)) this->proceed();
    break;
  case ui::Pages::Action::Discard:
    this->proceed();
    break;
  case ui::Pages::Action::Associate: {
    std::string error;
    if (platform::AssociateSgfFiles(&error)) pages.settings.setAssociation("Done: double-click a .sgf file and it opens here.", true);
    else                                    pages.settings.setAssociation(error, false);
    break;
  }
  case ui::Pages::Action::SaveSettings:
    // Only now does anything the page changed take effect: updateSettings()
    // applies it from here on.
    this->settings.graphics = pages.settings.draft().graphics;
    this->settings.board    = pages.settings.draft().board;
    this->settings.save();
    break;
  case ui::Pages::Action::SaveControls:
    this->settings.controls = pages.controls.draft();
    this->settings.save();
    break;
  case ui::Pages::Action::Online: {
    // The name, addresses and codes are kept for next time, whatever was asked.
    OnlineSetup&       online = this->settings.online;
    const std::string& name   = online.name;
    const size_t       index  = pages.contact();
    using Request             = ui::OnlinePage::Request;
    switch (pages.onlineRequest()) {
    case Request::Connect:
      if (index < online.contacts.size()) {
        const Contact& with = online.contacts[index];
        this->sharing.join(with.relay, with.key, name, online.identity);
      }
      break;
    case Request::Forget:
      if (index < online.contacts.size()) online.contacts.erase(online.contacts.begin() + std::ptrdiff_t(index));
      break;
    case Request::OpenRoom:   this->sharing.join(online.relay, "", name, online.identity); break;
    case Request::JoinRoom:   this->sharing.join(online.relay, online.room, name, online.identity); break;
    case Request::JoinDirect: this->sharing.join(online.address, "", name, online.identity); break;
    case Request::Host: {
      std::string error;
      if (!this->sharing.host(online.port, name, online.identity, &error)) this->gui.editor().message(error, false);
      break;
    }
    case Request::Leave:
      this->sharing.leave();
      this->gui.editor().message("Left the session.");
      break;
    }
    this->settings.save();
    break;
  }
  case ui::Pages::Action::OpenProjectPage: {
    std::string error;
    if (!platform::OpenInBrowser(ui::PROJECT_URL, &error)) this->gui.editor().message(error, false);
    break;
  }
  case ui::Pages::Action::DiscardSettings:
    break;  // the page's draft is dropped, and the settings were never touched
  case ui::Pages::Action::Back:
    this->after = After::None;
    break;
  case ui::Pages::Action::None:
    break;
  }
}

void App::handleDroppedFiles()
{
  if (!IsFileDropped()) return;
  FilePathList files = LoadDroppedFiles();
  // GLFW hands the names over in UTF-8.
  const std::filesystem::path first = files.count > 0 ? platform::FromUtf8(files.paths[0]) : std::filesystem::path();
  UnloadDroppedFiles(files);
  if (!first.empty()) this->guard(After::Open, first);
}

void App::handleClose()
{
  if (!WindowShouldClose()) return;
  // Not yet: the answer to "save first?" decides. Cleared in GLFW, raylib
  // reads it again at the end of the frame.
  glfwSetWindowShouldClose(glfwGetCurrentContext(), GLFW_FALSE);
  this->guard(After::Quit);
}

// ---------------------------------------------------------------- documents

void App::guard(After then, const std::filesystem::path& file)
{
  // Something else is taking over the screen: whatever the settings page
  // changed and didn't confirm goes, as if Back had been pressed.
  if (this->gui.pages().current() == ui::Pages::Page::Settings) this->gui.pages().close();
  this->after     = then;
  this->afterFile = file;
  if (this->game && this->game->modified()) {
    this->gui.pages().confirm.setFile(this->name());
    this->gui.pages().open(ui::Pages::Page::Confirm);
    return;
  }
  this->proceed();
}

void App::proceed()
{
  const After what = this->after;
  this->after = After::None;
  switch (what) {
  case After::NewGame: this->newGame(); break;
  case After::Open:    this->open(this->afterFile); break;
  case After::Quit:    this->quitRequested = true; break;
  case After::None:    break;
  }
}

void App::newGame()
{
  this->start(this->settings.newGame);
}

void App::emptyBoard()
{
  this->start(GameSetup{});
}

void App::start(GameSetup setup)
{
  setup.date = Today();
  this->game = std::make_unique<Game>(setup);
  this->path.clear();
  this->gui.editor().show(this->game.get());
}

bool App::open(const std::filesystem::path& file)
{
  ui::EditorView& editor = this->gui.editor();
  const std::string fileName = platform::ToUtf8(file.filename());

  std::string error;
  const std::optional<std::string> text = platform::ReadText(file, &error);
  if (!text) {
    editor.message(error, false);
    return false;
  }
  std::optional<sgf::Collection> parsed = sgf::Parse(*text, &error);
  if (!parsed) {
    editor.message("Couldn't read " + fileName + ": " + error + ".", false);
    return false;
  }
  const std::string gm = parsed->games.front()->get("GM");
  if (!gm.empty() && gm != "1") {
    editor.message(fileName + " is a record of some other game than Go (GM[" + gm + "]).", false);
    return false;
  }

  const size_t games = parsed->games.size();
  this->game = std::make_unique<Game>(std::move(*parsed));
  this->path = file;
  this->settings.folder = platform::ToUtf8(file.parent_path());
  editor.show(this->game.get());
  if (games > 1) editor.message(fileName + " holds " + std::to_string(games) + " games: this is the first. Saving keeps them all.");
  return true;
}

bool App::save(const std::filesystem::path& file)
{
  if (!this->game) return false;
  this->game->stampFormat();
  std::string error;
  if (!platform::WriteText(file, sgf::Write(this->game->file()), &error)) {
    this->gui.editor().message(error, false);
    return false;
  }
  this->path = file;
  this->game->markSaved();
  this->gui.editor().message("Saved " + platform::ToUtf8(file.filename()) + ".");
  return true;
}

void App::saveOrAsk()
{
  if (this->path.empty()) this->showFiles(true);
  else                    this->save(this->path);
}

void App::sendToAiSensei()
{
  if (!this->game) return;
  // What saving writes, CA[UTF-8] included, since that is how it's sent.
  this->game->stampFormat();
  const std::string sgf = sgf::Write(this->game->file());

  // ai-sensei.com/upload?sgf=... takes the record as pasted text: the upload
  // dialog opens with it, or after logging in if the browser isn't yet.
  // Percent-encoded byte by byte, so the UTF-8 of a name stays intact.
  std::string url = "https://ai-sensei.com/upload?sgf=";
  static const char HEX[] = "0123456789ABCDEF";
  for (const unsigned char c : sgf) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      url += char(c);
    } else {
      url += '%';
      url += HEX[c >> 4];
      url += HEX[c & 15];
    }
  }

  // Windows hands the URL to the browser on its command line, which holds
  // 32767 characters in all.
  constexpr size_t MAX_URL = 30000;
  if (url.size() > MAX_URL) {
    this->gui.editor().message("This game is too long to send this way: save it and upload the file on ai-sensei.com.",
                               false);
    return;
  }
  std::string error;
  if (platform::OpenInBrowser(url, &error)) this->gui.editor().message("Opened AI Sensei's upload page in the browser.");
  else                                      this->gui.editor().message(error, false);
}

void App::showFiles(bool forSaving)
{
  this->saving = forSaving;
  const std::string suggested = forSaving ? (this->path.empty() ? std::string("game.sgf") : platform::ToUtf8(this->path.filename()))
                                          : std::string();
  this->gui.pages().files.start(forSaving ? ui::FilePage::Mode::Save : ui::FilePage::Mode::Open, this->folder(), suggested);
  this->gui.pages().open(ui::Pages::Page::Files);
}

std::string App::name() const
{
  return this->path.empty() ? std::string("Untitled") : platform::ToUtf8(this->path.filename());
}

std::filesystem::path App::folder() const
{
  if (!this->path.empty()) return this->path.parent_path();
  if (!this->settings.folder.empty()) return platform::FromUtf8(this->settings.folder);
  return platform::DocumentsFolder();
}

// ---------------------------------------------------------------- the window

void App::updateSharing()
{
  ui::EditorView& editor = this->gui.editor();
  // Drawn here: to the others. (Without a session the lines are only here.)
  for (const net::StrokePart& part : editor.takeStrokes()) this->sharing.sendStroke(part);
  if (this->game) this->sharing.update(*this->game);
  // In a session now -- hosting, or welcomed into one: the Host or Join page
  // has done its job, and goes.
  const bool            joined = this->sharing.joined();
  const ui::Pages::Page page  = this->gui.pages().current();
  const bool            online = page == ui::Pages::Page::Host || page == ui::Pages::Page::Join;
  if (joined && !this->wasJoined && online) this->gui.pages().close();
  this->wasJoined = joined;
  for (const net::Session::Event& e : this->sharing.takeStrokes()) {
    editor.addStroke(e.author, this->sharing.colourOf(e.author), e.stroke);
  }
  editor.setOwnColour(this->sharing.ownColour());
  for (const std::string& message : this->sharing.takeMessages()) editor.message(message);
  if (this->sharing.contactsChanged()) this->settings.save();

  ui::EditorView::Presence presence;
  if (const net::Session* s = this->sharing.current()) {
    using Mode      = ui::EditorView::Presence::Mode;
    presence.mode   = s->isHost() || this->sharing.started() ? Mode::Hosting : s->isJoined() ? Mode::Joined : Mode::Connecting;
    presence.people = s->participants();
    presence.self   = s->self();
  }
  editor.showPresence(presence);

  // The Online page, while it is up, shows the session as it is.
  if (online) {
    ui::OnlinePage::Status status;
    if (const net::Session* s = this->sharing.current()) {
      status.active = true;
      status.people = s->participants();
      status.self   = s->self();
      if (s->isHost()) {
        std::string where;
        for (const std::string& address : this->sharing.addresses()) {
          if (!where.empty()) where += " or ";
          const std::string host = address.find(':') != std::string::npos ? "[" + address + "]" : address;
          where += s->port() == net::DEFAULT_PORT ? host : host + ":" + std::to_string(s->port());
        }
        status.what = "Hosting. Others join at " + (where.empty() ? std::string("this computer's address") : where) + ".";
      } else if (const Contact* with = this->sharing.pairRoom()) {
        status.what = "In the room you share with " + with->name + ", on the relay.";
      } else if (s->isJoined()) {
        if (s->room().empty()) {
          status.what = "In the session.";
        } else {
          status.what = "On the relay. Someone new joins with the invite code; the people you met can connect to you "
                        "without it.";
          status.code = s->room();
        }
      } else {
        status.what = "Connecting to " + this->sharing.address() + "...";
      }
    }
    (page == ui::Pages::Page::Host ? this->gui.pages().host : this->gui.pages().join).show(status);
  }
}

void App::updateTitle()
{
  const bool        changed = this->game && this->game->modified();
  const std::string title   = this->name() + (changed ? " *" : "");
  if (title == this->shownTitle) return;
  this->shownTitle = title;
  this->gui.editor().setTitle(title);
  SetWindowTitle((title + " - " + cfg::APP_NAME).c_str());
}

void App::scale(Command command)
{
  // A step up or down, from anywhere -- from the automatic scale, which it
  // leaves for a manual one, as Factorio's do -- or back to automatic. Kept
  // and applied at once, unless the settings page is up: there it is one
  // more change to the page's draft, which only Confirm keeps.
  const bool onPage = this->gui.pages().current() == ui::Pages::Page::Settings;
  Settings::Graphics& graphics = onPage ? this->gui.pages().settings.draft().graphics : this->settings.graphics;
  if (command == Command::ScaleAutomatic) {
    if (graphics.automaticScale) return;
    graphics.automaticScale = true;
  } else {
    const int step = command == Command::ScaleUp ? Settings::Graphics::SCALE_STEP : -Settings::Graphics::SCALE_STEP;
    const int from = graphics.automaticScale ? AutomaticInterfaceScale(GetScreenWidth(), GetScreenHeight())
                                             : graphics.interfaceScale;
    graphics.interfaceScale = ClampedInterfaceScale(from + step);
    graphics.automaticScale = false;
  }
  if (onPage) this->gui.pages().settings.refresh();
  else        this->settings.save();
}

void App::updateSettings()
{
  this->gui.pages().settings.setAutomaticScale(AutomaticInterfaceScale(GetScreenWidth(), GetScreenHeight()));

  const Settings::Graphics& graphics = this->settings.graphics;

  // The scale drawn at: on automatic, it follows the window as it is resized.
  if (const int scale = EffectiveInterfaceScale(graphics, GetScreenWidth(), GetScreenHeight()); scale != this->shownScale) {
    this->gui.setScale(scale);
    this->shownScale = scale;
  }

  this->gui.editor().setBoardOptions({ this->settings.board.coordinates, this->settings.board.moveNumbers,
                                       this->settings.board.nextMoves });
  this->gui.editor().setTreeNumbers(this->settings.board.treeNumbers);
  this->gui.editor().setNavigationButtons(this->settings.board.navigationButtons);
  this->gui.editor().setEvaluation(this->settings.board.evaluationButtons);
  this->gui.editor().setBindings(this->settings.controls);
  this->gui.pages().setSearchShortcut(this->settings.controls.keysFor(Command::FocusSearch));

  // Bring the window and the Gui in line with the settings, when Confirm on
  // the settings page (or the scale's shortcut) has changed them.
  if (this->settings.graphics != this->applied.graphics || this->settings.board != this->applied.board) {
    if (this->settings.graphics.tooltipDelay != this->applied.graphics.tooltipDelay) {
      this->gui.setTooltipDelay(this->settings.graphics.tooltipDelay);
    }
    ApplySettings(this->settings);
    this->applied = this->settings;
  }

  // Remember the window as the player left it. Not while windowed fullscreen,
  // which would record the monitor's size, nor maximized, which would record
  // the maximized size in place of the one to restore to, nor minimized, which
  // Windows reports as 0x0.
  if (!IsWindowedFullscreen() && !IsWindowMinimized()) {
    this->settings.window.maximized = IsWindowMaximized();
    if (!this->settings.window.maximized) {
      this->settings.window.width  = GetScreenWidth();
      this->settings.window.height = GetScreenHeight();
    }
  }
}
