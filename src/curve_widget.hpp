#pragma once

#include "curve_types.hpp"

#include <QPointF>
#include <QRectF>
#include <QWidget>

#include <array>

class CurveWidget final : public QWidget {
  Q_OBJECT

public:
  enum class ColorMode {
    Solid,
    HueGradient,
  };

  enum class CurveBehavior {
    Linear,
    WrappedHue,
  };

  explicit CurveWidget(QWidget *parent = nullptr);

  void set_curve(const rgb_curves::CurvePoints &curve);
  [[nodiscard]] rgb_curves::CurvePoints curve() const;
  void reset_curve();
  void set_histogram(const std::array<float, 256> &histogram);
  void set_color_mode(ColorMode mode);
  void set_curve_behavior(CurveBehavior behavior);

signals:
  void curveChanged(const rgb_curves::CurvePoints &curve);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void leaveEvent(QEvent *event) override;

private:
  QRectF plot_rect() const;
  QPointF point_to_canvas(const rgb_curves::CurvePoint &point) const;
  rgb_curves::CurvePoint canvas_to_point(const QPointF &canvas_point) const;
  int hit_test_handle(const QPointF &position) const;
  void add_point_at(const QPointF &position);
  void remove_point_at(int index);
  void update_dragged_point(const QPointF &position);
  void emit_curve_changed();

  rgb_curves::CurvePoints curve_ = rgb_curves::default_curve();
  std::array<float, 256> histogram_ {};
  ColorMode color_mode_ = ColorMode::Solid;
  CurveBehavior curve_behavior_ = CurveBehavior::Linear;
  int active_index_ = -1;
  int hover_index_ = -1;
};
