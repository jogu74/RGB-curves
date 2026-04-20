#pragma once

#include "curve_types.hpp"
#include "curve_widget.hpp"

#include <QColor>
#include <QDialog>
#include <QImage>

class QComboBox;
class QLabel;
class QPushButton;
class QTimer;

#include <array>
#include <cstdint>
#include <functional>
#include <limits>

class HueCurveEditorDialog final : public QDialog {
  Q_OBJECT

public:
  explicit HueCurveEditorDialog(const std::array<hue_curves::CurvePoints, 3> &initial_curves,
                                QWidget *parent = nullptr);
  void set_histogram_provider(std::function<std::array<float, 256>(int)> provider);
  void set_preview_provider(std::function<QImage()> provider);

  [[nodiscard]] std::array<hue_curves::CurvePoints, 3> curves() const;

signals:
  void curvesChanged(const std::array<hue_curves::CurvePoints, 3> &curves);

private slots:
  void set_channel(int index);
  void handle_curve_changed(const rgb_curves::CurvePoints &curve);
  void reset_active_curve();
  void reset_all_curves();
  void save_preset();
  void load_preset();
  void delete_preset();
  void rename_preset();
  void export_preset();
  void import_preset();
  void refresh_histogram();

private:
  void refresh_preset_combo();
  bool load_saved_preset_by_name(const QString &preset_name);
  void refresh_channel_buttons();
  QColor color_for_channel(int index) const;
  void invalidate_curve_cache();
  void set_preview_source_image(const QImage &image);
  void update_preview();
  void apply_curves(const std::array<hue_curves::CurvePoints, 3> &curves);

  std::array<hue_curves::CurvePoints, 3> curves_;
  std::array<hue_curves::PreparedCurve, 3> prepared_curves_ {
    hue_curves::prepare_curve(hue_curves::default_curve()),
    hue_curves::prepare_curve(hue_curves::default_curve()),
    hue_curves::prepare_curve(hue_curves::default_curve()),
  };
  CurveWidget *curve_widget_ = nullptr;
  QLabel *preview_label_ = nullptr;
  QLabel *build_label_ = nullptr;
  QComboBox *preset_combo_ = nullptr;
  QTimer *histogram_timer_ = nullptr;
  std::function<std::array<float, 256>(int)> histogram_provider_;
  std::function<QImage()> preview_provider_;
  QImage preview_source_image_;
  qint64 preview_source_cache_key_ = 0;
  uint64_t curves_revision_ = 0;
  uint64_t rendered_preview_revision_ = std::numeric_limits<uint64_t>::max();
  qint64 rendered_preview_source_key_ = std::numeric_limits<qint64>::min();
  std::array<QPushButton *, 3> channel_buttons_ {};
  int active_channel_ = 0;
};
