#include "color_range_correction_editor_dialog.hpp"
#include "screen_color_picker.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <vector>

enum class RangeBandVisual {
  Hue,
  Saturation,
  Luma,
};

class RangeBandWidget final : public QWidget {
public:
  RangeBandWidget(RangeBandVisual visual, int minimum, int maximum, bool wrap, QWidget *parent = nullptr)
    : QWidget(parent), visual_(visual), minimum_(minimum), maximum_(maximum), wrap_(wrap),
      low_(minimum), high_(maximum), softness_(0)
  {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(46);
  }

  void set_values(int low, int high, int softness)
  {
    low_ = std::clamp(low, minimum_, maximum_);
    high_ = std::clamp(high, minimum_, maximum_);
    softness_ = std::clamp(softness, 0, max_softness());

    if (!wrap_ && low_ > high_) {
      std::swap(low_, high_);
    }

    update();
  }

  [[nodiscard]] int low_value() const { return low_; }
  [[nodiscard]] int high_value() const { return high_; }
  [[nodiscard]] int softness_value() const { return softness_; }

  void set_change_handler(std::function<void(int, int, int)> handler) { change_handler_ = std::move(handler); }

  [[nodiscard]] QSize sizeHint() const override { return {620, 48}; }

protected:
  void paintEvent(QPaintEvent *) override
  {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF track = track_rect();
    QPainterPath clip_path;
    clip_path.addRoundedRect(track, 4.0, 4.0);

    painter.save();
    painter.setClipPath(clip_path);
    painter.fillRect(track, QColor(132, 132, 132));

    const QRectF accent(track.left(), track.bottom() - 5.0, track.width(), 3.5);
    switch (visual_) {
    case RangeBandVisual::Hue:
      painter.fillRect(accent, hue_gradient(accent));
      break;
    case RangeBandVisual::Saturation:
      painter.fillRect(accent, QColor(214, 40, 40));
      break;
    case RangeBandVisual::Luma: {
      QLinearGradient gradient(accent.left(), 0.0, accent.right(), 0.0);
      gradient.setColorAt(0.0, QColor(74, 74, 74));
      gradient.setColorAt(1.0, QColor(238, 238, 238));
      painter.fillRect(accent, gradient);
      break;
    }
    }

    paint_selection(painter, track);
    painter.restore();

    painter.setPen(QPen(QColor(86, 86, 86), 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(track, 4.0, 4.0);

    draw_handles(painter, track);
  }

  void mousePressEvent(QMouseEvent *event) override
  {
    drag_handle_ = pick_handle(event->position());
    if (drag_handle_ != DragHandle::None) {
      update_from_position(event->position().x());
      event->accept();
      return;
    }

    QWidget::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent *event) override
  {
    if (drag_handle_ != DragHandle::None) {
      update_from_position(event->position().x());
      event->accept();
      return;
    }

    QWidget::mouseMoveEvent(event);
  }

  void mouseReleaseEvent(QMouseEvent *event) override
  {
    drag_handle_ = DragHandle::None;
    QWidget::mouseReleaseEvent(event);
  }

private:
  enum class DragHandle {
    None,
    LowSoft,
    LowHard,
    HighHard,
    HighSoft,
  };

  [[nodiscard]] QRectF track_rect() const
  {
    return QRectF(10.0, 15.0, std::max(1, width() - 20), 18.0);
  }

  [[nodiscard]] int span_length() const { return maximum_ - minimum_; }

  [[nodiscard]] int max_softness() const { return wrap_ ? span_length() / 2 : span_length(); }

  [[nodiscard]] double value_to_x(double value) const
  {
    const QRectF track = track_rect();
    const double span = std::max(1, span_length());
    const double normalized = (value - minimum_) / span;
    return track.left() + normalized * track.width();
  }

  [[nodiscard]] int x_to_value(double x) const
  {
    const QRectF track = track_rect();
    const double clamped = std::clamp(x, track.left(), track.right());
    const double ratio = (clamped - track.left()) / std::max(1.0, track.width());
    const double value = minimum_ + ratio * span_length();
    return static_cast<int>(std::round(value));
  }

  [[nodiscard]] int normalize_wrapped_value(int value) const
  {
    if (!wrap_) {
      return std::clamp(value, minimum_, maximum_);
    }

    if (value == maximum_) {
      return maximum_;
    }

    const int span = span_length();
    int normalized = value;
    while (normalized < minimum_) {
      normalized += span;
    }
    while (normalized > maximum_) {
      normalized -= span;
    }
    return normalized;
  }

  [[nodiscard]] int canonical_value(int value) const
  {
    const int span = span_length();
    if (span <= 0) {
      return minimum_;
    }

    int normalized = value - minimum_;
    normalized %= span;
    if (normalized < 0) {
      normalized += span;
    }
    return minimum_ + normalized;
  }

  [[nodiscard]] int circular_distance_forward(int from, int to) const
  {
    const int span = span_length();
    if (span <= 0) {
      return 0;
    }

    const int start = canonical_value(from) - minimum_;
    const int end = canonical_value(to) - minimum_;
    return (end - start + span) % span;
  }

  [[nodiscard]] std::vector<std::pair<double, double>> split_span(double start, double end) const
  {
    std::vector<std::pair<double, double>> segments;
    if (end <= start) {
      return segments;
    }

    if (!wrap_) {
      const double clamped_start = std::clamp(start, static_cast<double>(minimum_), static_cast<double>(maximum_));
      const double clamped_end = std::clamp(end, static_cast<double>(minimum_), static_cast<double>(maximum_));
      if (clamped_end > clamped_start) {
        segments.emplace_back(clamped_start, clamped_end);
      }
      return segments;
    }

    const double span = span_length();
    double normalized_start = start;
    double normalized_end = end;

    while (normalized_start < minimum_) {
      normalized_start += span;
      normalized_end += span;
    }
    while (normalized_start > maximum_) {
      normalized_start -= span;
      normalized_end -= span;
    }

    if (normalized_end <= maximum_) {
      segments.emplace_back(normalized_start, normalized_end);
      return segments;
    }

    segments.emplace_back(normalized_start, static_cast<double>(maximum_));
    segments.emplace_back(static_cast<double>(minimum_), normalized_end - span);
    return segments;
  }

  [[nodiscard]] QLinearGradient hue_gradient(const QRectF &rect) const
  {
    QLinearGradient gradient(rect.left(), 0.0, rect.right(), 0.0);
    gradient.setColorAt(0.00, QColor(255, 70, 70));
    gradient.setColorAt(0.16, QColor(255, 166, 0));
    gradient.setColorAt(0.33, QColor(210, 255, 0));
    gradient.setColorAt(0.50, QColor(0, 225, 140));
    gradient.setColorAt(0.66, QColor(0, 150, 255));
    gradient.setColorAt(0.82, QColor(151, 55, 255));
    gradient.setColorAt(1.00, QColor(255, 70, 140));
    return gradient;
  }

  void paint_selection(QPainter &painter, const QRectF &track) const
  {
    const QColor solid_fill(240, 240, 240, 96);
    const QColor soft_fill(240, 240, 240, 70);
    const QColor transparent(240, 240, 240, 0);

    auto fill_span = [&](double start, double end, const QBrush &brush) {
      for (const auto &[segment_start, segment_end] : split_span(start, end)) {
        const QRectF segment_rect(value_to_x(segment_start), track.top() + 1.0,
                                  std::max(1.0, value_to_x(segment_end) - value_to_x(segment_start)),
                                  track.height() - 2.0);
        painter.fillRect(segment_rect, brush);
      }
    };

    auto fill_soft_span = [&](double start, double end, bool fade_in) {
      for (const auto &[segment_start, segment_end] : split_span(start, end)) {
        const QRectF segment_rect(value_to_x(segment_start), track.top() + 1.0,
                                  std::max(1.0, value_to_x(segment_end) - value_to_x(segment_start)),
                                  track.height() - 2.0);
        QLinearGradient gradient(segment_rect.left(), 0.0, segment_rect.right(), 0.0);
        if (fade_in) {
          gradient.setColorAt(0.0, transparent);
          gradient.setColorAt(1.0, soft_fill);
        } else {
          gradient.setColorAt(0.0, soft_fill);
          gradient.setColorAt(1.0, transparent);
        }
        painter.fillRect(segment_rect, gradient);
      }
    };

    const double solid_start = static_cast<double>(low_);
    const double solid_end =
      static_cast<double>(high_) + ((wrap_ && low_ > high_) ? static_cast<double>(span_length()) : 0.0);

    fill_soft_span(solid_start - softness_, solid_start, true);
    fill_span(solid_start, solid_end, solid_fill);
    fill_soft_span(solid_end, solid_end + softness_, false);
  }

  void draw_triangle(QPainter &painter, double x, double apex_y, bool points_down, const QColor &color) const
  {
    const double width = 8.0;
    const double height = 7.0;

    QPolygonF triangle;
    if (points_down) {
      triangle << QPointF(x - width, apex_y - height) << QPointF(x + width, apex_y - height) << QPointF(x, apex_y);
    } else {
      triangle << QPointF(x - width, apex_y + height) << QPointF(x + width, apex_y + height) << QPointF(x, apex_y);
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawPolygon(triangle);
  }

  void draw_handles(QPainter &painter, const QRectF &track) const
  {
    const QColor soft_handle(106, 106, 106);
    const QColor hard_handle(74, 74, 74);

    const double low_soft_x = value_to_x(normalize_wrapped_value(low_ - softness_));
    const double low_hard_x = value_to_x(normalize_wrapped_value(low_));
    const double high_hard_x = value_to_x(normalize_wrapped_value(high_));
    const double high_soft_x = value_to_x(normalize_wrapped_value(high_ + softness_));

    draw_triangle(painter, low_soft_x, track.top() - 1.0, true, soft_handle);
    draw_triangle(painter, low_hard_x, track.bottom() + 1.0, false, hard_handle);
    draw_triangle(painter, high_hard_x, track.top() - 1.0, true, hard_handle);
    draw_triangle(painter, high_soft_x, track.bottom() + 1.0, false, soft_handle);
  }

  [[nodiscard]] DragHandle pick_handle(const QPointF &position) const
  {
    const QRectF track = track_rect();
    const std::array<std::pair<DragHandle, QPointF>, 4> handles = {{
      {DragHandle::LowSoft, QPointF(value_to_x(normalize_wrapped_value(low_ - softness_)), track.top() - 4.0)},
      {DragHandle::LowHard, QPointF(value_to_x(normalize_wrapped_value(low_)), track.bottom() + 4.0)},
      {DragHandle::HighHard, QPointF(value_to_x(normalize_wrapped_value(high_)), track.top() - 4.0)},
      {DragHandle::HighSoft, QPointF(value_to_x(normalize_wrapped_value(high_ + softness_)), track.bottom() + 4.0)},
    }};

    DragHandle best_handle = DragHandle::None;
    double best_distance = 14.0;

    for (const auto &[handle, point] : handles) {
      const double distance = std::hypot(position.x() - point.x(), position.y() - point.y());
      if (distance < best_distance) {
        best_distance = distance;
        best_handle = handle;
      }
    }

    return best_handle;
  }

  void update_from_position(double x)
  {
    const int value = x_to_value(x);
    int updated_low = low_;
    int updated_high = high_;
    int updated_softness = softness_;

    switch (drag_handle_) {
    case DragHandle::LowSoft:
      if (wrap_) {
        updated_softness = std::min(max_softness(), circular_distance_forward(value, low_));
      } else {
        updated_softness = std::clamp(low_ - value, 0, max_softness());
      }
      break;
    case DragHandle::LowHard:
      updated_low = wrap_ ? value : std::min(value, high_);
      if (!wrap_) {
        updated_softness = std::min(updated_softness, updated_low - minimum_);
      }
      break;
    case DragHandle::HighHard:
      updated_high = wrap_ ? value : std::max(value, low_);
      if (!wrap_) {
        updated_softness = std::min(updated_softness, maximum_ - updated_high);
      }
      break;
    case DragHandle::HighSoft:
      if (wrap_) {
        updated_softness = std::min(max_softness(), circular_distance_forward(high_, value));
      } else {
        updated_softness = std::clamp(value - high_, 0, max_softness());
      }
      break;
    case DragHandle::None:
      return;
    }

    if (!wrap_) {
      updated_softness = std::min(updated_softness, updated_low - minimum_);
      updated_softness = std::min(updated_softness, maximum_ - updated_high);
    }

    if (updated_low == low_ && updated_high == high_ && updated_softness == softness_) {
      return;
    }

    low_ = updated_low;
    high_ = updated_high;
    softness_ = updated_softness;
    update();

    if (change_handler_) {
      change_handler_(low_, high_, softness_);
    }
  }

  RangeBandVisual visual_;
  int minimum_ = 0;
  int maximum_ = 100;
  bool wrap_ = false;
  int low_ = 0;
  int high_ = 100;
  int softness_ = 0;
  DragHandle drag_handle_ = DragHandle::None;
  std::function<void(int, int, int)> change_handler_;
};

namespace {

constexpr const char *kBuildStamp = "Build: " __DATE__ " " __TIME__;

QFrame *make_separator(QWidget *parent = nullptr)
{
  auto *line = new QFrame(parent);
  line->setObjectName("sectionSeparator");
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Plain);
  line->setFixedHeight(1);
  return line;
}

QLabel *make_section_title(const QString &text, QWidget *parent = nullptr)
{
  auto *label = new QLabel(text, parent);
  label->setObjectName("sectionTitle");
  return label;
}

QLabel *make_row_label(const QString &text, QWidget *parent = nullptr)
{
  auto *label = new QLabel(text, parent);
  label->setObjectName("rowLabel");
  label->setMinimumWidth(64);
  return label;
}

QPushButton *make_swatch_button(const QColor &color, const QString &tooltip, QWidget *parent = nullptr)
{
  auto *button = new QPushButton(parent);
  button->setObjectName("swatchButton");
  button->setCursor(Qt::PointingHandCursor);
  button->setFixedSize(22, 22);
  button->setToolTip(tooltip);
  button->setStyleSheet(QString(
                          "QPushButton { background-color: %1; border: 2px solid #252525; border-radius: 11px; }"
                          "QPushButton:hover { border-color: #f1f1f1; }")
                          .arg(color.name()));
  return button;
}

QString format_slider_value(ColorRangeCorrectionEditorDialog::SliderFormat format, int value)
{
  switch (format) {
  case ColorRangeCorrectionEditorDialog::SliderFormat::SignedInteger:
    return value > 0 ? QString("+%1").arg(value) : QString::number(value);
  case ColorRangeCorrectionEditorDialog::SliderFormat::Percent:
  case ColorRangeCorrectionEditorDialog::SliderFormat::Integer:
  default:
    return QString::number(value);
  }
}

} // namespace

ColorRangeCorrectionEditorDialog::ColorRangeCorrectionEditorDialog(const ColorRangeCorrectionSettings &initial_settings,
                                                                   QWidget *parent)
  : QDialog(parent), settings_(initial_settings)
{
  setWindowTitle("Color Range Correction");
  resize(860, 660);

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(18, 16, 18, 16);
  root->setSpacing(14);

  auto *build_label = new QLabel(QString::fromUtf8(kBuildStamp), this);
  build_label->setObjectName("buildLabel");
  build_label->setAlignment(Qt::AlignRight);
  root->addWidget(build_label);

  auto *key_section = new QVBoxLayout();
  key_section->setSpacing(12);
  key_section->addWidget(make_section_title("Key", this));

  auto *set_color_row = new QHBoxLayout();
  set_color_row->setSpacing(10);
  set_color_row->addWidget(make_row_label("Set Color", this));

  if (colorforge::is_screen_color_picker_supported()) {
    screen_pick_button_ = new QPushButton("Pick Screen Color", this);
    screen_pick_button_->setObjectName("pickColorButton");
    screen_pick_button_->setToolTip("Pick a color from anywhere on the screen");
    connect(screen_pick_button_, &QPushButton::clicked, this, &ColorRangeCorrectionEditorDialog::begin_screen_color_pick);
    set_color_row->addWidget(screen_pick_button_);

    picked_color_chip_ = new QLabel(this);
    picked_color_chip_->setObjectName("pickedColorChip");
    picked_color_chip_->setFixedSize(18, 18);
    picked_color_chip_->setToolTip("Last picked color");
    set_color_row->addWidget(picked_color_chip_);
    update_picked_color_chip(QColor("#7f7f7f"));
  }

  const std::array<std::pair<QColor, int>, 7> swatches = {{
    {QColor("#981b1e"), 0},
    {QColor("#a4a700"), 58},
    {QColor("#0d9c0f"), 120},
    {QColor("#148f99"), 180},
    {QColor("#1e35ef"), 240},
    {QColor("#8a0aa0"), 300},
    {QColor("#c5c2cd"), -1},
  }};

  for (const auto &swatch : swatches) {
    const QColor color = swatch.first;
    const int center = swatch.second;
    auto *button = make_swatch_button(color, center >= 0 ? QString("Center hue near %1 degrees").arg(center)
                                                          : QString("Reset hue selection"),
                                      this);
    connect(button, &QPushButton::clicked, this, [this, center]() {
      apply_hue_preset(center >= 0 ? center : 0, center < 0);
    });
    set_color_row->addWidget(button);
  }

  set_color_row->addStretch(1);
  key_section->addLayout(set_color_row);

  auto *hue_row = new QHBoxLayout();
  hue_row->setSpacing(10);
  hue_row->addWidget(make_row_label("H", this));
  hue_band_ = new RangeBandWidget(RangeBandVisual::Hue, 0, 360, true, this);
  hue_row->addWidget(hue_band_, 1);
  key_section->addLayout(hue_row);

  auto *sat_row = new QHBoxLayout();
  sat_row->setSpacing(10);
  sat_row->addWidget(make_row_label("S", this));
  sat_band_ = new RangeBandWidget(RangeBandVisual::Saturation, 0, 100, false, this);
  sat_row->addWidget(sat_band_, 1);
  key_section->addLayout(sat_row);

  auto *luma_row = new QHBoxLayout();
  luma_row->setSpacing(10);
  luma_row->addWidget(make_row_label("L", this));
  luma_band_ = new RangeBandWidget(RangeBandVisual::Luma, 0, 100, false, this);
  luma_row->addWidget(luma_band_, 1);
  key_section->addLayout(luma_row);

  auto *preview_row = new QHBoxLayout();
  preview_row->setSpacing(10);
  invert_mask_ = new QCheckBox("Invert Mask", this);
  preview_row->addWidget(invert_mask_);

  preview_combo_ = new QComboBox(this);
  preview_combo_->addItem("Final", static_cast<int>(ColorRangeCorrectionPreviewMode::Final));
  preview_combo_->addItem("Mask", static_cast<int>(ColorRangeCorrectionPreviewMode::Mask));
  preview_combo_->addItem("Color / Gray", static_cast<int>(ColorRangeCorrectionPreviewMode::ColorGray));
  preview_combo_->setFixedWidth(170);
  preview_row->addWidget(preview_combo_);

  auto *reset_button = new QPushButton("Reset", this);
  connect(reset_button, &QPushButton::clicked, this, &ColorRangeCorrectionEditorDialog::reset_all);
  preview_row->addWidget(reset_button);
  preview_row->addStretch(1);
  key_section->addLayout(preview_row);

  root->addLayout(key_section);
  root->addWidget(make_separator(this));

  auto *refine_section = new QVBoxLayout();
  refine_section->setSpacing(12);
  refine_section->addWidget(make_section_title("Refine", this));
  blur_ = create_slider_control(0, 20, settings_.blur, SliderFormat::Integer, "blurSlider");

  auto *blur_row = new QHBoxLayout();
  blur_row->setSpacing(10);
  blur_row->addWidget(make_row_label("Blur", this));
  blur_row->addWidget(blur_.slider, 1);
  blur_row->addWidget(blur_.value_label);
  refine_section->addLayout(blur_row);

  root->addLayout(refine_section);
  root->addWidget(make_separator(this));

  auto *correction_section = new QVBoxLayout();
  correction_section->setSpacing(12);
  correction_section->addWidget(make_section_title("Correction", this));

  temperature_ = create_slider_control(-100, 100, settings_.temperature, SliderFormat::SignedInteger, "temperatureSlider");
  tint_ = create_slider_control(-100, 100, settings_.tint, SliderFormat::SignedInteger, "tintSlider");
  correction_saturation_ =
    create_slider_control(0, 200, settings_.correction_saturation, SliderFormat::Percent, "saturationSlider");
  correction_luma_ = create_slider_control(0, 200, settings_.correction_luma, SliderFormat::Percent, "lumaSlider");

  auto add_correction_row = [&](const QString &label_text, const SliderControl &control) {
    auto *row = new QHBoxLayout();
    row->setSpacing(10);
    row->addWidget(make_row_label(label_text, this));
    row->addWidget(control.slider, 1);
    row->addWidget(control.value_label);
    correction_section->addLayout(row);
  };

  add_correction_row("Temperature", temperature_);
  add_correction_row("Tint", tint_);
  add_correction_row("Saturation", correction_saturation_);
  add_correction_row("Luma", correction_luma_);

  root->addLayout(correction_section);
  root->addStretch(1);

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  root->addWidget(buttons);

  auto connect_slider = [this](SliderControl &control) {
    SliderControl *control_ptr = &control;
    connect(control.slider, &QSlider::valueChanged, this, [this, control_ptr](int value) {
      update_slider_value_label(*control_ptr, value);
      emit_settings_changed();
    });
  };

  connect_slider(blur_);
  connect_slider(temperature_);
  connect_slider(tint_);
  connect_slider(correction_saturation_);
  connect_slider(correction_luma_);

  hue_band_->set_change_handler([this](int, int, int) { emit_settings_changed(); });
  sat_band_->set_change_handler([this](int, int, int) { emit_settings_changed(); });
  luma_band_->set_change_handler([this](int, int, int) { emit_settings_changed(); });

  connect(preview_combo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { emit_settings_changed(); });
  connect(invert_mask_, &QCheckBox::toggled, this, [this](bool) { emit_settings_changed(); });

  apply_styles();
  refresh_controls_from_settings();
}

ColorRangeCorrectionSettings ColorRangeCorrectionEditorDialog::settings() const
{
  return settings_;
}

void ColorRangeCorrectionEditorDialog::reset_all()
{
  settings_ = {};
  refresh_controls_from_settings();
  emit settingsChanged(settings_);
}

ColorRangeCorrectionEditorDialog::SliderControl ColorRangeCorrectionEditorDialog::create_slider_control(
  int minimum, int maximum, int value, SliderFormat format, const char *object_name)
{
  SliderControl control;
  control.format = format;
  control.slider = new QSlider(Qt::Horizontal, this);
  control.slider->setRange(minimum, maximum);
  control.slider->setValue(value);
  if (object_name) {
    control.slider->setObjectName(QString::fromUtf8(object_name));
  }

  control.value_label = new QLabel(this);
  control.value_label->setObjectName("sliderValue");
  control.value_label->setMinimumWidth(42);
  control.value_label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  update_slider_value_label(control, value);
  return control;
}

void ColorRangeCorrectionEditorDialog::update_slider_value_label(const SliderControl &control, int value) const
{
  if (!control.value_label) {
    return;
  }

  control.value_label->setText(format_slider_value(control.format, value));
}

void ColorRangeCorrectionEditorDialog::update_picked_color_chip(const QColor &color)
{
  if (!picked_color_chip_) {
    return;
  }

  const QColor display_color = color.isValid() ? color : QColor("#7f7f7f");
  picked_color_chip_->setStyleSheet(
    QString("QLabel#pickedColorChip { background-color: %1; border: 1px solid #8a8a8a; border-radius: 9px; }")
      .arg(display_color.name()));
  picked_color_chip_->setToolTip(QString("Last picked color: %1").arg(display_color.name().toUpper()));
}

void ColorRangeCorrectionEditorDialog::refresh_controls_from_settings()
{
  syncing_ui_ = true;

  {
    const QSignalBlocker blocker(preview_combo_);
    preview_combo_->setCurrentIndex(std::clamp(settings_.preview_mode, 0, 2));
  }

  {
    const QSignalBlocker blocker(invert_mask_);
    invert_mask_->setChecked(settings_.invert_mask != 0);
  }

  hue_band_->set_values(settings_.hue_min, settings_.hue_max, settings_.hue_softness);
  sat_band_->set_values(settings_.sat_min, settings_.sat_max, settings_.sat_softness);
  luma_band_->set_values(settings_.luma_min, settings_.luma_max, settings_.luma_softness);

  auto apply_slider = [this](SliderControl &control, int value) {
    const QSignalBlocker blocker(control.slider);
    control.slider->setValue(value);
    update_slider_value_label(control, value);
  };

  apply_slider(blur_, settings_.blur);
  apply_slider(temperature_, settings_.temperature);
  apply_slider(tint_, settings_.tint);
  apply_slider(correction_saturation_, settings_.correction_saturation);
  apply_slider(correction_luma_, settings_.correction_luma);

  syncing_ui_ = false;
}

void ColorRangeCorrectionEditorDialog::apply_hue_preset(int center_degrees, bool full_range)
{
  if (full_range) {
    settings_.hue_min = 0;
    settings_.hue_max = 360;
    settings_.hue_softness = 12;
  } else {
    const auto wrap_hue = [](int value) {
      int wrapped = value % 360;
      if (wrapped < 0) {
        wrapped += 360;
      }
      return wrapped;
    };

    constexpr int kHalfWidth = 18;
    settings_.hue_min = wrap_hue(center_degrees - kHalfWidth);
    settings_.hue_max = wrap_hue(center_degrees + kHalfWidth);
    settings_.hue_softness = std::max(settings_.hue_softness, 12);
  }

  refresh_controls_from_settings();
  emit settingsChanged(settings_);
}

void ColorRangeCorrectionEditorDialog::begin_screen_color_pick()
{
  if (!colorforge::is_screen_color_picker_supported() || !screen_pick_button_) {
    return;
  }

  screen_pick_button_->setEnabled(false);

  QPointer<ColorRangeCorrectionEditorDialog> guard(this);
  colorforge::pick_screen_color(
    this,
    [guard](const QColor &color) {
      if (!guard) {
        return;
      }

      guard->apply_sampled_color(color);
      if (guard->screen_pick_button_) {
        guard->screen_pick_button_->setEnabled(true);
      }
      guard->raise();
      guard->activateWindow();
    },
    [guard]() {
      if (!guard) {
        return;
      }

      if (guard->screen_pick_button_) {
        guard->screen_pick_button_->setEnabled(true);
      }
      guard->raise();
      guard->activateWindow();
    });
}

void ColorRangeCorrectionEditorDialog::apply_sampled_color(const QColor &color)
{
  if (!color.isValid()) {
    return;
  }

  const QColor rgb_color = color.toRgb();
  update_picked_color_chip(rgb_color);

  const QColor hsv_color = rgb_color.toHsv();
  const int hue = hsv_color.hsvHue();
  const int saturation = static_cast<int>(std::round(hsv_color.hsvSaturationF() * 100.0));
  const int luma = static_cast<int>(
    std::round((0.2126 * rgb_color.redF() + 0.7152 * rgb_color.greenF() + 0.0722 * rgb_color.blueF()) * 100.0));

  const auto clamp_percent = [](int value) { return std::clamp(value, 0, 100); };
  const auto wrap_hue = [](int value) {
    int wrapped = value % 360;
    if (wrapped < 0) {
      wrapped += 360;
    }
    return wrapped;
  };

  if (hue < 0 || saturation < 3) {
    settings_.hue_min = 0;
    settings_.hue_max = 360;
    settings_.hue_softness = 12;
  } else {
    constexpr int kHueHalfWidth = 18;
    settings_.hue_min = wrap_hue(hue - kHueHalfWidth);
    settings_.hue_max = wrap_hue(hue + kHueHalfWidth);
    settings_.hue_softness = 12;
  }

  settings_.sat_min = clamp_percent(saturation - 22);
  settings_.sat_max = clamp_percent(saturation + 22);
  settings_.sat_softness = 10;
  settings_.luma_min = clamp_percent(luma - 24);
  settings_.luma_max = clamp_percent(luma + 24);
  settings_.luma_softness = 10;

  refresh_controls_from_settings();
  emit settingsChanged(settings_);
}

void ColorRangeCorrectionEditorDialog::emit_settings_changed()
{
  if (syncing_ui_) {
    return;
  }

  settings_.preview_mode = preview_combo_->currentData().toInt();
  settings_.invert_mask = invert_mask_->isChecked() ? 1 : 0;
  settings_.hue_min = hue_band_->low_value();
  settings_.hue_max = hue_band_->high_value();
  settings_.hue_softness = hue_band_->softness_value();
  settings_.sat_min = sat_band_->low_value();
  settings_.sat_max = sat_band_->high_value();
  settings_.sat_softness = sat_band_->softness_value();
  settings_.luma_min = luma_band_->low_value();
  settings_.luma_max = luma_band_->high_value();
  settings_.luma_softness = luma_band_->softness_value();
  settings_.blur = blur_.slider->value();
  settings_.temperature = temperature_.slider->value();
  settings_.tint = tint_.slider->value();
  settings_.correction_saturation = correction_saturation_.slider->value();
  settings_.correction_luma = correction_luma_.slider->value();

  emit settingsChanged(settings_);
}

void ColorRangeCorrectionEditorDialog::apply_styles()
{
  setStyleSheet(
    "QDialog { background-color: #1f1f1f; color: #e8e8e8; }"
    "QLabel#buildLabel { color: #8c8c8c; font-size: 11px; }"
    "QLabel#sectionTitle { color: #d9d9d9; font-size: 15px; font-weight: 600; }"
    "QLabel#rowLabel { color: #c9c9c9; font-size: 13px; }"
    "QLabel#sliderValue { color: #48a1ff; font-size: 13px; }"
    "QFrame#sectionSeparator { background-color: #343434; border: 0; }"
    "QPushButton { background-color: #2c2c2c; border: 1px solid #4a4a4a; border-radius: 6px; min-height: 28px; padding: 0 12px; }"
    "QPushButton:hover { background-color: #383838; }"
    "QPushButton#pickColorButton { padding: 0 10px; }"
    "QPushButton#swatchButton { padding: 0; min-height: 0; }"
    "QComboBox { background-color: #111111; border: 1px solid #4a4a4a; border-radius: 5px; min-height: 30px; padding: 0 10px; }"
    "QCheckBox { color: #d9d9d9; spacing: 8px; }"
    "QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid #858585; background-color: #111111; }"
    "QCheckBox::indicator:checked { background-color: #e8e8e8; }"
    "QSlider { min-height: 22px; }"
    "QSlider::groove:horizontal { border: 0; height: 3px; border-radius: 2px; background: #5c5c5c; }"
    "QSlider::handle:horizontal { background: #d8d8d8; width: 14px; height: 14px; margin: -6px 0; border-radius: 7px; }"
    "QSlider#blurSlider::groove:horizontal { background: #595959; }"
    "QSlider#temperatureSlider::groove:horizontal { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2a9cff, stop:1 #ff9b35); }"
    "QSlider#tintSlider::groove:horizontal { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #50d94a, stop:1 #f248ff); }"
    "QSlider#saturationSlider::groove:horizontal { background: #6d6d6d; }"
    "QSlider#lumaSlider::groove:horizontal { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #4a4a4a, stop:1 #d8d8d8); }");
}
