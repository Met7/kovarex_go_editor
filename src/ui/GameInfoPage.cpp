#include <ui/GameInfoPage.hpp>

#include <game/Sgf.hpp>
#include <ui/Form.hpp>
#include <ui/Theme.hpp>

#include <Agui/Widget/Button.hpp>
#include <Agui/Widget/Frame.hpp>
#include <Agui/Widget/TextBox.hpp>
#include <Agui/Widget/TextField.hpp>

namespace ui {

namespace {

constexpr int NAME_W  = 112;
constexpr int FIELD_W = 212;

struct Field {
  const char* id;
  const char* name;
  const char* tip;
};

// Two columns of them: the game on the left, where it came from on the right.
constexpr Field LEFT[] = {
  { "PB", "Black", nullptr },
  { "BR", "Black's rank", "Like 5d, 2k or 9p" },
  { "BT", "Black's team", nullptr },
  { "PW", "White", nullptr },
  { "WR", "White's rank", nullptr },
  { "WT", "White's team", nullptr },
  { "RE", "Result", "Like B+R, W+3.5, B+T, 0 for a draw, Void, or ?" },
  { "KM", "Komi", nullptr },
  { "HA", "Handicap", "The number of handicap stones. The stones themselves are set up on the board." },
  { "RU", "Rules", "Japanese, Chinese, AGA, GOE, NZ..." },
  { "TM", "Time (seconds)", "Main time per player, in seconds" },
};
constexpr Field RIGHT[] = {
  { "OT", "Overtime", "Like 5x30 byo-yomi" },
  { "DT", "Date", "YYYY-MM-DD" },
  { "EV", "Event", nullptr },
  { "RO", "Round", nullptr },
  { "PC", "Place", nullptr },
  { "GN", "Game name", nullptr },
  { "ON", "Opening", nullptr },
  { "SO", "Source", nullptr },
  { "AN", "Annotator", nullptr },
  { "US", "Entered by", nullptr },
  { "CP", "Copyright", nullptr },
};

}  // namespace

GameInfoPage::GameInfoPage(Theme& theme, std::function<void()> onApply, std::function<void()> onBack)
    : window(agui::GuiDirection::Vertical, "Game info")
{
  this->window.setDragTarget(&this->window);

  agui::HorizontalFlow& columns = row(24);
  columns.style.setVerticalAlign(agui::VerticalAlign::Top);
  for (const auto* side : { &LEFT, &RIGHT }) {
    agui::VerticalFlow& fieldColumn = column(4);
    for (const Field& f : *side) {
      agui::TextField& field = make<agui::TextField>();
      field.style.setMinimalWidth(FIELD_W);
      field.style.setMaximalWidth(FIELD_W);
      if (f.tip) field.setToolTip(f.tip);
      // Enter in a field is Apply, as in any dialog.
      field.onConfirm(this, onApply);
      fieldColumn << namedRow(f.name, field, NAME_W);
      this->fields.emplace_back(f.id, &field);
    }
    columns << fieldColumn;
  }

  agui::VerticalFlow& content = column();
  content << columns;
  content << agui::label("Game comment", &theme.headingLabel);
  this->gameComment = &make<agui::TextBox>();
  this->gameComment->setWordWrap(true);
  this->gameComment->setHScrollPolicy(agui::ScrollPolicy::Never);
  this->gameComment->setVScrollPolicy(agui::ScrollPolicy::Auto);
  this->gameComment->style.setMinimalWidth(2 * (NAME_W + FIELD_W) + 36);
  this->gameComment->style.setMinimalHeight(80);
  this->gameComment->style.setMaximalHeight(80);
  content << *this->gameComment;

  agui::Frame& panel = make<agui::Frame>(agui::GuiDirection::Vertical, &theme.insideShallowFrameWithPadding);
  panel << content;
  this->window << panel;

  agui::HorizontalFlow& footer = row(8);
  footer.style.setTopPadding(8);
  footer.style.setHorizontallyStretchable(true);
  footer << agui::button("Back", &this->window, std::move(onBack), &theme.backButton);
  footer << dragHandle(&theme.draggableSpace, &this->window);
  footer << footerButton("Apply", &this->window, std::move(onApply), &theme.forwardButton, 160);
  this->window << footer;
}

void GameInfoPage::load(const sgf::Node& root)
{
  for (auto& [id, field] : this->fields) field->setText(root.get(id));
  this->gameComment->setText(root.get("GC"));
}

std::vector<std::pair<std::string, std::string>> GameInfoPage::values() const
{
  std::vector<std::pair<std::string, std::string>> result;
  for (const auto& [id, field] : this->fields) result.emplace_back(id, field->getText());
  result.emplace_back("GC", this->gameComment->getText());
  return result;
}

}  // namespace ui
