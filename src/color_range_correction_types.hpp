#pragma once

struct ColorRangeCorrectionSettings {
  int preview_mode = 0;
  int hue_min = 0;
  int hue_max = 360;
  int hue_softness = 12;
  int sat_min = 0;
  int sat_max = 100;
  int sat_softness = 10;
  int luma_min = 0;
  int luma_max = 100;
  int luma_softness = 10;
  int blur = 0;
  int invert_mask = 0;
  int temperature = 0;
  int tint = 0;
  int correction_saturation = 100;
  int correction_luma = 100;
};

enum class ColorRangeCorrectionPreviewMode {
  Final = 0,
  Mask = 1,
  ColorGray = 2,
};
