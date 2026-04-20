#include "color_range_correction_filter.hpp"
#include "hue_curves_filter.hpp"
#include "rgb_curves_filter.hpp"

#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-colorforge", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
  return "ColorForge for OBS Studio. A multi-filter color toolkit with RGB Curves, Hue Curves and Color Range Correction.";
}

bool obs_module_load(void)
{
  static obs_source_info color_range_correction_info = make_color_range_correction_filter_info();
  static obs_source_info rgb_curves_info = make_rgb_curves_filter_info();
  static obs_source_info hue_curves_info = make_hue_curves_filter_info();
  obs_register_source(&color_range_correction_info);
  obs_register_source(&rgb_curves_info);
  obs_register_source(&hue_curves_info);
  blog(LOG_INFO, "[obs-colorforge] plugin loaded");
  return true;
}
