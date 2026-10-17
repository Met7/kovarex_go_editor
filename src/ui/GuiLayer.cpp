#include <ui/GuiLayer.hpp>

#include <app/Settings.hpp>
#include <ui/EditorView.hpp>
#include <ui/GoSprites.hpp>
#include <ui/Input.hpp>
#include <ui/Pages.hpp>
#include <ui/Theme.hpp>

#include <Agui/Gui.hpp>
#include <Agui/TopContainer.hpp>
#include <Agui/Widget/ToolTip.hpp>
#include <rlgl.h>

namespace ui {

GuiLayer::GuiLayer(Settings& settings, std::future<GoSprites::Pixels>& pictures)
{
  // The scale the GUI will be drawn at, which the Theme rasterises its fonts
  // for from the start rather than at 100% and then again.
  const int percent = EffectiveInterfaceScale(settings.graphics, GetScreenWidth(), GetScreenHeight());

  // Theme loads fonts through Agui, so the loader has to be in place first.
  agui::Font::setFontLoader(&this->fontLoader);
  this->theme   = std::make_unique<Theme>(float(percent) / 100.0f);
  this->sprites = std::make_unique<GoSprites>(pictures.get());

  this->gui = std::make_unique<agui::Gui>();
  this->gui->setGraphics(&this->graphics);
  this->gui->setInput(&this->input);
  this->gui->setCursorProvider(&this->cursor);
  this->gui->resizeToDisplay();

  // Order is depth: the editor at the back, the pages (and the sheet they dim
  // the editor with) on top.
  this->editorView = std::make_unique<EditorView>(*this->gui, *this->theme, *this->sprites);
  this->pageStack  = std::make_unique<Pages>(*this->gui, *this->theme, settings);

  this->setScale(percent);
  this->setTooltipDelay(settings.graphics.tooltipDelay);
}

void GuiLayer::setTooltipDelay(int milliseconds)
{
  // Agui's own "never" is -1 seconds.
  this->gui->setGuiTooltipHoverInterval(milliseconds < 0 ? -1.0 : double(milliseconds) / 1000.0);
  this->gui->resetGuiTooltipHoverTime();
}

GuiLayer::~GuiLayer()
{
  this->pageStack.reset();
  this->editorView.reset();
  this->gui.reset();
  this->sprites.reset();
  this->theme.reset();
}

void GuiLayer::setScale(int percent)
{
  const float scale = float(percent) / 100.0f;
  this->graphics.setViewScale(scale);
  this->input.setViewScale(scale);
  this->theme->setScale(scale);
  this->editorView->setScale(percent);
}

void GuiLayer::setKeyFilter(std::function<bool(int key)> filter)
{
  this->input.setKeyFilter(std::move(filter));
}

void GuiLayer::update(float dt)
{
  // Compared rather than asking IsWindowResized(): a resize made mid-frame, like
  // toggling fullscreen from the settings page, is cleared by the next
  // EndDrawing() before this ever sees it. In GUI units, so a change of scale
  // is a resize too. Not while minimized: Windows reports that as 0x0, and
  // laying out for it would recentre everything the player had dragged
  // somewhere.
  const agui::Dimension display = this->graphics.getDisplaySize();
  if (!IsWindowMinimized() && (display.width != this->screenWidth || display.height != this->screenHeight)) {
    this->screenWidth  = display.width;
    this->screenHeight = display.height;
    this->gui->resizeToDisplay();
  }
  // Shift shows tooltips at once, whatever the delay -- even when it is
  // "never". Agui takes back the ones it showed when Shift is let go of.
  const bool shift = input::IsKeyDown(KEY_LEFT_SHIFT) || input::IsKeyDown(KEY_RIGHT_SHIFT);
  this->gui->setInstantTooltip(shift);
  this->editorView->setShift(shift);
  this->gui->logic(true);

  // A tooltip is a window like any other, and would take the mouse when it
  // moved onto one -- dropping the tooltip, which then comes back, over and
  // over. The mouse goes through them to what is under them.
  for (agui::Widget* child : this->gui->getTop()->getChildren()) {
    if (dynamic_cast<agui::ToolTip*>(child) && !child->isIgnoredByInteraction()) child->setIgnoredByInteraction(true);
  }

  // The editor is still there behind a page, dimmed, but nothing on it can be
  // clicked.
  this->editorView->setInteractive(!this->pageStack->isOpen());
  this->editorView->update(dt, this->screenWidth, this->screenHeight);
  this->pageStack->layout(this->screenWidth, this->screenHeight);
}

void GuiLayer::draw()
{
  // Agui draws in GUI units; the matrix turns them into screen pixels.
  rlPushMatrix();
  rlScalef(this->graphics.getViewScale(), this->graphics.getViewScale(), 1.0f);
  this->graphics._beginPaint();
  this->gui->render();
  this->graphics._endPaint();
  rlPopMatrix();
}

}  // namespace ui
