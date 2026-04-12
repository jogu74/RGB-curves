#pragma once

#include "curve_types.hpp"

#include <QImage>

#include <graphics/effect.h>
#include <graphics/graphics.h>
#include <obs-module.h>

#include <array>
#include <string>

class RGBCurvesFilter final {
public:
  explicit RGBCurvesFilter(obs_data_t *settings, obs_source_t *source);
  ~RGBCurvesFilter();

  static void *Create(obs_data_t *settings, obs_source_t *source);
  static void Destroy(void *data);
  static void Update(void *data, obs_data_t *settings);
  static void Render(void *data, gs_effect_t *effect);
  static obs_properties_t *Properties(void *data);
  static void Defaults(obs_data_t *settings);
  static const char *Name(void *);
  [[nodiscard]] std::array<float, 256> histogram(size_t channel_index) const;
  [[nodiscard]] QImage preview_image() const;

private:
  void load_effect();
  void rebuild_lut();
  void save_curves(obs_data_t *settings) const;
  void open_editor();
  void ensure_histogram_surfaces(uint32_t width, uint32_t height);
  void update_histogram_from_target();

  static bool OpenEditorClicked(obs_properties_t *props, obs_property_t *property, void *data);

  obs_source_t *source_ = nullptr;
  gs_effect_t *effect_ = nullptr;
  gs_eparam_t *lut_param_ = nullptr;
  gs_texture_t *lut_texture_ = nullptr;
  gs_texrender_t *histogram_texrender_ = nullptr;
  gs_stagesurf_t *histogram_stage_ = nullptr;
  uint32_t histogram_width_ = 0;
  uint32_t histogram_height_ = 0;

  std::array<rgb_curves::CurvePoints, 4> curves_ = rgb_curves::default_curves();
  std::array<uint8_t, 1024> lut_data_ {};
  std::array<std::array<float, 256>, 4> histograms_ {};
  QImage preview_image_;
  std::string effect_path_;
};

obs_source_info make_rgb_curves_filter_info();
