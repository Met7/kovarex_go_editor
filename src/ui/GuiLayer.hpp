// The Agui side of the screen: the raylib backend, the theme, the Gui itself,
// and everything living in it -- the editor, and the pages over it. Owns them
// in the order they have to be built and torn down: widgets reference the
// theme's fonts and textures, so the theme outlives them, and a widget added to
// the Gui first draws under one added later, which is what keeps the pages on
// top of the editor.

#pragma once

#include <ui/AguiRaylib.hpp>
#include <ui/GoSprites.hpp>

#include <functional>
#include <future>
#include <memory>

struct Settings;

namespace agui {
class Gui;
}

namespace ui {

class EditorView;
class Pages;
class Theme;

class GuiLayer {
public:
  // Needs the window. The pages edit `settings` in place. `pictures` is
  // GoSprites::prepare(), under way on another thread: it is waited for only
  // once everything that doesn't need it is done.
  GuiLayer(Settings& settings, std::future<GoSprites::Pixels>& pictures);
  ~GuiLayer();
  GuiLayer(const GuiLayer&) = delete;
  GuiLayer& operator=(const GuiLayer&) = delete;

  // Once per frame, before anything asks the editor or the pages what
  // happened.
  void update(float dt);

  // The interface scale, in percent: everything is laid out on a display that
  // much smaller than the window and drawn that much bigger. Takes effect as a
  // resize on the next update().
  void setScale(int percent);
  // Screen pixels per GUI unit: the scale, as a factor.
  float viewScale() const { return this->graphics.getViewScale(); }

  // How long the mouse rests on something before its tooltip shows, in
  // milliseconds; negative for never. Shift shows them at once regardless.
  void setTooltipDelay(int milliseconds);

  // Keys this says yes to never reach a widget: see ui/Shortcuts.hpp.
  void setKeyFilter(std::function<bool(int key)> filter);

  // Everything: the editor, the sheet dimming it while a page is up, and the
  // page.
  void draw();

  EditorView& editor() { return *this->editorView; }
  Pages&      pages() { return *this->pageStack; }

private:
  agui_raylib::RaylibFontLoader     fontLoader;
  agui_raylib::RaylibGraphics       graphics;
  agui_raylib::RaylibInput          input;
  agui_raylib::RaylibCursorProvider cursor;

  std::unique_ptr<Theme>     theme;
  std::unique_ptr<GoSprites> sprites;
  std::unique_ptr<agui::Gui> gui;

  // Added to the Gui in this order, so each draws over the one before it.
  std::unique_ptr<EditorView> editorView;
  std::unique_ptr<Pages>      pageStack;

  int screenWidth  = 0;  // what the Gui was last sized to, in GUI units
  int screenHeight = 0;
};

}  // namespace ui
