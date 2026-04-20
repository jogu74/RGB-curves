#pragma once

#include "color_range_correction_types.hpp"

#include <QColor>
#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class QWidget;
class RangeBandWidget;

class ColorRangeCorrectionEditorDialog final : public QDialog {
  Q_OBJECT

public:
  enum class SliderFormat {
    Integer,
    SignedInteger,
    Percent,
  };

  struct SliderControl {
    QSlider *slider = nullptr;
    QLabel *value_label = nullptr;
    SliderFormat format = SliderFormat::Integer;
  };

  explicit ColorRangeCorrectionEditorDialog(const ColorRangeCorrectionSettings &initial_settings, QWidget *parent = nullptr);

  [[nodiscard]] ColorRangeCorrectionSettings settings() const;

signals:
  void settingsChanged(const ColorRangeCorrectionSettings &settings);

private slots:
  void reset_all();

private:
  SliderControl create_slider_control(int minimum, int maximum, int value, SliderFormat format,
                                      const char *object_name = nullptr);
  void update_slider_value_label(const SliderControl &control, int value) const;
  void update_picked_color_chip(const QColor &color);
  void refresh_controls_from_settings();
  void apply_hue_preset(int center_degrees, bool full_range = false);
  void begin_screen_color_pick();
  void apply_sampled_color(const QColor &color);
  void emit_settings_changed();
  void apply_styles();

  ColorRangeCorrectionSettings settings_;
  bool syncing_ui_ = false;
  QComboBox *preview_combo_ = nullptr;
  QCheckBox *invert_mask_ = nullptr;
  QPushButton *screen_pick_button_ = nullptr;
  QLabel *picked_color_chip_ = nullptr;
  RangeBandWidget *hue_band_ = nullptr;
  RangeBandWidget *sat_band_ = nullptr;
  RangeBandWidget *luma_band_ = nullptr;
  SliderControl blur_;
  SliderControl temperature_;
  SliderControl tint_;
  SliderControl correction_saturation_;
  SliderControl correction_luma_;
};
