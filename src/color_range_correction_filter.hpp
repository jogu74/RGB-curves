#pragma once

#include "color_range_correction_types.hpp"

#include <QPointer>

#include <graphics/effect.h>
#include <graphics/vec2.h>
#include <obs-module.h>

#include <string>

class ColorRangeCorrectionFilter final {
public:
  explicit ColorRangeCorrectionFilter(obs_data_t *settings, obs_source_t *source);
  ~ColorRangeCorrectionFilter();

  static void *Create(obs_data_t *settings, obs_source_t *source);
  static void Destroy(void *data);
  static void Update(void *data, obs_data_t *settings);
  static void Render(void *data, gs_effect_t *effect);
  static obs_properties_t *Properties(void *data);
  static void Defaults(obs_data_t *settings);
  static const char *Name(void *);

private:
  static bool OpenEditorClicked(obs_properties_t *props, obs_property_t *property, void *data);
  void load_effect();
  void update_settings(obs_data_t *settings);
  void save_settings(obs_data_t *settings) const;
  void open_editor();

  obs_source_t *source_ = nullptr;
  gs_effect_t *effect_ = nullptr;
  gs_eparam_t *texel_size_param_ = nullptr;
  gs_eparam_t *preview_mode_param_ = nullptr;
  gs_eparam_t *hue_min_param_ = nullptr;
  gs_eparam_t *hue_max_param_ = nullptr;
  gs_eparam_t *hue_softness_param_ = nullptr;
  gs_eparam_t *hue_full_range_param_ = nullptr;
  gs_eparam_t *sat_min_param_ = nullptr;
  gs_eparam_t *sat_max_param_ = nullptr;
  gs_eparam_t *sat_softness_param_ = nullptr;
  gs_eparam_t *luma_min_param_ = nullptr;
  gs_eparam_t *luma_max_param_ = nullptr;
  gs_eparam_t *luma_softness_param_ = nullptr;
  gs_eparam_t *invert_mask_param_ = nullptr;
  gs_eparam_t *temperature_param_ = nullptr;
  gs_eparam_t *tint_param_ = nullptr;
  gs_eparam_t *saturation_param_ = nullptr;
  gs_eparam_t *luma_param_ = nullptr;
  gs_eparam_t *blur_param_ = nullptr;

  ColorRangeCorrectionSettings settings_ {};
  ColorRangeCorrectionSettings dialog_original_settings_ {};
  QPointer<class ColorRangeCorrectionEditorDialog> editor_dialog_;
  std::string effect_path_;
};

obs_source_info make_color_range_correction_filter_info();
