#include "rgb_curves_filter.hpp"

#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("rgb-curves", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
  return "Interactive RGB curves filter with a point-based editor.";
}

bool obs_module_load(void)
{
  static obs_source_info info = make_rgb_curves_filter_info();
  obs_register_source(&info);
  blog(LOG_INFO, "[rgb-curves] plugin loaded");
  return true;
}
