#include "curve_widget.hpp"

#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

#include <array>

namespace {

constexpr int kHandleRadius = 6;
constexpr int kMargin = 26;

QColor curve_color()
{
  return QColor(232, 232, 232);
}

} // namespace

CurveWidget::CurveWidget(QWidget *parent) : QWidget(parent)
{
  setMinimumSize(420, 320);
  setMouseTracking(true);
}

void CurveWidget::set_curve(const rgb_curves::CurvePoints &curve)
{
  curve_ = rgb_curves::sanitize_curve(curve);
  active_index_ = -1;
  update();
}

rgb_curves::CurvePoints CurveWidget::curve() const
{
  return curve_;
}

void CurveWidget::reset_curve()
{
  set_curve(rgb_curves::default_curve());
  emit_curve_changed();
}

void CurveWidget::set_histogram(const std::array<float, 256> &histogram)
{
  histogram_ = histogram;
  update();
}

QRectF CurveWidget::plot_rect() const
{
  return QRectF(kMargin, kMargin, width() - (2.0 * kMargin), height() - (2.0 * kMargin));
}

QPointF CurveWidget::point_to_canvas(const rgb_curves::CurvePoint &point) const
{
  const QRectF rect = plot_rect();
  return {
    rect.left() + (point.x * rect.width()),
    rect.bottom() - (point.y * rect.height()),
  };
}

rgb_curves::CurvePoint CurveWidget::canvas_to_point(const QPointF &canvas_point) const
{
  const QRectF rect = plot_rect();
  const double x = (canvas_point.x() - rect.left()) / rect.width();
  const double y = (rect.bottom() - canvas_point.y()) / rect.height();
  return {rgb_curves::clamp01(x), rgb_curves::clamp01(y)};
}

int CurveWidget::hit_test_handle(const QPointF &position) const
{
  for (size_t index = 0; index < curve_.size(); ++index) {
    const QPointF handle_pos = point_to_canvas(curve_[index]);
    if (QLineF(position, handle_pos).length() <= (kHandleRadius + 3)) {
      return static_cast<int>(index);
    }
  }

  return -1;
}

void CurveWidget::paintEvent(QPaintEvent *event)
{
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.fillRect(rect(), QColor(28, 28, 28));

  const QRectF rect = plot_rect();
  painter.setPen(QPen(QColor(110, 110, 110), 1.0));
  painter.drawRect(rect);

  painter.setPen(QPen(QColor(90, 90, 90), 1.0, Qt::DashLine));
  for (int i = 1; i < 4; ++i) {
    const double x = rect.left() + (rect.width() * i / 4.0);
    const double y = rect.top() + (rect.height() * i / 4.0);
    painter.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    painter.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
  }

  painter.setPen(QPen(QColor(62, 62, 62), 1.0));
  painter.drawLine(rect.bottomLeft(), rect.topRight());

  if (!curve_.empty()) {
    const QPointF first = point_to_canvas(curve_.front());
    const QPointF last = point_to_canvas(curve_.back());
    painter.setPen(QPen(QColor(140, 140, 140, 160), 1.0, Qt::DotLine));
    painter.drawLine(QPointF(rect.left(), first.y()), first);
    painter.drawLine(last, QPointF(rect.right(), last.y()));
  }

  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(170, 170, 170, 70));
  const double bin_width = rect.width() / 256.0;
  for (int i = 0; i < 256; ++i) {
    const double value = std::clamp(static_cast<double>(histogram_[static_cast<size_t>(i)]), 0.0, 1.0);
    if (value <= 0.0) {
      continue;
    }

    const double height = value * rect.height();
    painter.drawRect(
      QRectF(rect.left() + (bin_width * i), rect.bottom() - height, std::max(bin_width, 1.0), height));
  }

  QPainterPath curve_path;
  bool first = true;
  for (int i = 0; i <= 255; ++i) {
    const double x = static_cast<double>(i) / 255.0;
    const double y = rgb_curves::sample_curve(curve_, x);
    const QPointF p = point_to_canvas({x, y});
    if (first) {
      curve_path.moveTo(p);
      first = false;
    } else {
      curve_path.lineTo(p);
    }
  }

  painter.setPen(QPen(curve_color(), 2.2));
  painter.drawPath(curve_path);

  for (size_t index = 0; index < curve_.size(); ++index) {
    const QPointF p = point_to_canvas(curve_[index]);
    const bool highlighted = static_cast<int>(index) == active_index_ || static_cast<int>(index) == hover_index_;
    painter.setBrush(highlighted ? QColor(255, 216, 96) : QColor(240, 240, 240));
    painter.setPen(QPen(QColor(18, 18, 18), 1.0));
    painter.drawEllipse(p, kHandleRadius, kHandleRadius);
  }
}

void CurveWidget::mousePressEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton) {
    active_index_ = hit_test_handle(event->position());
    if (active_index_ < 0) {
      add_point_at(event->position());
      active_index_ = hit_test_handle(event->position());
    }
    update();
    return;
  }

  if (event->button() == Qt::RightButton) {
    remove_point_at(hit_test_handle(event->position()));
  }
}

void CurveWidget::mouseMoveEvent(QMouseEvent *event)
{
  hover_index_ = hit_test_handle(event->position());
  if (active_index_ >= 0) {
    update_dragged_point(event->position());
  }
  update();
}

void CurveWidget::mouseReleaseEvent(QMouseEvent *event)
{
  Q_UNUSED(event);
  active_index_ = -1;
  update();
}

void CurveWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
  if (event->button() == Qt::LeftButton) {
    remove_point_at(hit_test_handle(event->position()));
  }
}

void CurveWidget::leaveEvent(QEvent *event)
{
  Q_UNUSED(event);
  hover_index_ = -1;
  update();
}

void CurveWidget::add_point_at(const QPointF &position)
{
  if (!plot_rect().contains(position)) {
    return;
  }

  curve_.push_back(canvas_to_point(position));
  curve_ = rgb_curves::sanitize_curve(curve_);
  emit_curve_changed();
}

void CurveWidget::remove_point_at(int index)
{
  if (index < 0 || index >= static_cast<int>(curve_.size()) || curve_.size() <= 2) {
    return;
  }

  curve_.erase(curve_.begin() + index);
  curve_ = rgb_curves::sanitize_curve(curve_);
  emit_curve_changed();
  update();
}

void CurveWidget::update_dragged_point(const QPointF &position)
{
  if (active_index_ < 0 || active_index_ >= static_cast<int>(curve_.size())) {
    return;
  }

  auto point = canvas_to_point(position);
  if (active_index_ == 0) {
    const double max_x = curve_[static_cast<size_t>(active_index_ + 1)].x - 0.001;
    point.x = std::clamp(point.x, 0.0, max_x);
  } else if (active_index_ == static_cast<int>(curve_.size()) - 1) {
    const double min_x = curve_[static_cast<size_t>(active_index_ - 1)].x + 0.001;
    point.x = std::clamp(point.x, min_x, 1.0);
  } else {
    const double min_x = curve_[static_cast<size_t>(active_index_ - 1)].x + 0.001;
    const double max_x = curve_[static_cast<size_t>(active_index_ + 1)].x - 0.001;
    point.x = std::clamp(point.x, min_x, max_x);
  }

  curve_[static_cast<size_t>(active_index_)] = point;
  curve_ = rgb_curves::sanitize_curve(curve_);
  emit_curve_changed();
}

void CurveWidget::emit_curve_changed()
{
  emit curveChanged(curve_);
  update();
}
