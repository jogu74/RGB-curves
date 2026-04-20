#include "color_range_correction_editor_dialog.hpp"
#include "color_range_correction_filter.hpp"

#include <QApplication>
#include <QDialog>
#include <QWidget>

#include <algorithm>

namespace {

constexpr const char *kFilterId = "colorforge_color_range_correction_filter";
constexpr const char *kEditorButtonSetting = "open_editor";

constexpr const char *kPreviewModeSetting = "preview_mode";
constexpr const char *kHueMinSetting = "hue_min";
constexpr const char *kHueMaxSetting = "hue_max";
constexpr const char *kHueSoftnessSetting = "hue_softness";
constexpr const char *kSatMinSetting = "sat_min";
constexpr const char *kSatMaxSetting = "sat_max";
constexpr const char *kSatSoftnessSetting = "sat_softness";
constexpr const char *kLumaMinSetting = "luma_min";
constexpr const char *kLumaMaxSetting = "luma_max";
constexpr const char *kLumaSoftnessSetting = "luma_softness";
constexpr const char *kBlurSetting = "blur";
constexpr const char *kInvertMaskSetting = "invert_mask";
constexpr const char *kTemperatureSetting = "temperature";
constexpr const char *kTintSetting = "tint";
constexpr const char *kSaturationSetting = "correction_saturation";
constexpr const char *kLumaSetting = "correction_luma";

QWidget *obs_parent_widget()
{
  for (QWidget *widget : QApplication::topLevelWidgets()) {
    if (widget && widget->isVisible()) {
      return widget;
    }
  }

  return nullptr;
}

float normalize_percent(int value)
{
  return std::clamp(static_cast<float>(value) / 100.0f, 0.0f, 1.0f);
}

float normalize_signed_percent(int value)
{
  return std::clamp(static_cast<float>(value) / 100.0f, -1.0f, 1.0f);
}

float normalize_hue(int value)
{
  return std::clamp(static_cast<float>(value) / 360.0f, 0.0f, 1.0f);
}

} // namespace

ColorRangeCorrectionFilter::ColorRangeCorrectionFilter(obs_data_t *settings, obs_source_t *source) : source_(source)
{
  char *effect_path = obs_module_file("effects/color-range-correction.effect");
  effect_path_ = effect_path ? effect_path : "";
  bfree(effect_path);

  update_settings(settings);
  load_effect();
}

ColorRangeCorrectionFilter::~ColorRangeCorrectionFilter()
{
  if (editor_dialog_) {
    editor_dialog_->close();
    editor_dialog_.clear();
  }

  obs_enter_graphics();
  if (effect_) {
    gs_effect_destroy(effect_);
    effect_ = nullptr;
  }
  obs_leave_graphics();
}

void *ColorRangeCorrectionFilter::Create(obs_data_t *settings, obs_source_t *source)
{
  return new ColorRangeCorrectionFilter(settings, source);
}

void ColorRangeCorrectionFilter::Destroy(void *data)
{
  delete static_cast<ColorRangeCorrectionFilter *>(data);
}

void ColorRangeCorrectionFilter::Update(void *data, obs_data_t *settings)
{
  auto *filter = static_cast<ColorRangeCorrectionFilter *>(data);
  if (filter) {
    filter->update_settings(settings);
  }
}

void ColorRangeCorrectionFilter::Render(void *data, gs_effect_t *effect)
{
  (void)effect;

  auto *filter = static_cast<ColorRangeCorrectionFilter *>(data);
  if (!filter) {
    return;
  }

  if (!filter->effect_) {
    obs_source_skip_video_filter(filter->source_);
    return;
  }

  if (!obs_source_process_filter_begin(filter->source_, GS_RGBA, OBS_ALLOW_DIRECT_RENDERING)) {
    return;
  }

  obs_source_t *target = obs_filter_get_target(filter->source_);
  const uint32_t target_width = target ? std::max(obs_source_get_width(target), 1u) : 1u;
  const uint32_t target_height = target ? std::max(obs_source_get_height(target), 1u) : 1u;
  struct vec2 texel_size;
  texel_size.x = 1.0f / static_cast<float>(target_width);
  texel_size.y = 1.0f / static_cast<float>(target_height);

  gs_effect_set_vec2(filter->texel_size_param_, &texel_size);
  gs_effect_set_int(filter->preview_mode_param_, filter->settings_.preview_mode);
  gs_effect_set_float(filter->hue_min_param_, normalize_hue(filter->settings_.hue_min));
  gs_effect_set_float(filter->hue_max_param_, normalize_hue(filter->settings_.hue_max));
  gs_effect_set_float(filter->hue_softness_param_, normalize_hue(filter->settings_.hue_softness));
  gs_effect_set_float(
    filter->hue_full_range_param_, (filter->settings_.hue_min == 0 && filter->settings_.hue_max == 360) ? 1.0f : 0.0f);
  gs_effect_set_float(filter->sat_min_param_, normalize_percent(filter->settings_.sat_min));
  gs_effect_set_float(filter->sat_max_param_, normalize_percent(filter->settings_.sat_max));
  gs_effect_set_float(filter->sat_softness_param_, normalize_percent(filter->settings_.sat_softness));
  gs_effect_set_float(filter->luma_min_param_, normalize_percent(filter->settings_.luma_min));
  gs_effect_set_float(filter->luma_max_param_, normalize_percent(filter->settings_.luma_max));
  gs_effect_set_float(filter->luma_softness_param_, normalize_percent(filter->settings_.luma_softness));
  gs_effect_set_float(filter->blur_param_, static_cast<float>(std::clamp(filter->settings_.blur, 0, 20)));
  gs_effect_set_int(filter->invert_mask_param_, filter->settings_.invert_mask != 0 ? 1 : 0);
  gs_effect_set_float(filter->temperature_param_, normalize_signed_percent(filter->settings_.temperature));
  gs_effect_set_float(filter->tint_param_, normalize_signed_percent(filter->settings_.tint));
  gs_effect_set_float(
    filter->saturation_param_, std::clamp(static_cast<float>(filter->settings_.correction_saturation) / 100.0f, 0.0f, 2.0f));
  gs_effect_set_float(
    filter->luma_param_, std::clamp(static_cast<float>(filter->settings_.correction_luma) / 100.0f, 0.0f, 2.0f));
  obs_source_process_filter_end(filter->source_, filter->effect_, 0, 0);
}

obs_properties_t *ColorRangeCorrectionFilter::Properties(void *data)
{
  (void)data;

  auto *props = obs_properties_create();
  obs_properties_add_button(
    props, kEditorButtonSetting, "Open Color Range Correction Editor", &ColorRangeCorrectionFilter::OpenEditorClicked);
  return props;
}

void ColorRangeCorrectionFilter::Defaults(obs_data_t *settings)
{
  ColorRangeCorrectionSettings defaults;
  obs_data_set_default_int(settings, kPreviewModeSetting, defaults.preview_mode);
  obs_data_set_default_int(settings, kHueMinSetting, defaults.hue_min);
  obs_data_set_default_int(settings, kHueMaxSetting, defaults.hue_max);
  obs_data_set_default_int(settings, kHueSoftnessSetting, defaults.hue_softness);
  obs_data_set_default_int(settings, kSatMinSetting, defaults.sat_min);
  obs_data_set_default_int(settings, kSatMaxSetting, defaults.sat_max);
  obs_data_set_default_int(settings, kSatSoftnessSetting, defaults.sat_softness);
  obs_data_set_default_int(settings, kLumaMinSetting, defaults.luma_min);
  obs_data_set_default_int(settings, kLumaMaxSetting, defaults.luma_max);
  obs_data_set_default_int(settings, kLumaSoftnessSetting, defaults.luma_softness);
  obs_data_set_default_int(settings, kBlurSetting, defaults.blur);
  obs_data_set_default_bool(settings, kInvertMaskSetting, defaults.invert_mask != 0);
  obs_data_set_default_int(settings, kTemperatureSetting, defaults.temperature);
  obs_data_set_default_int(settings, kTintSetting, defaults.tint);
  obs_data_set_default_int(settings, kSaturationSetting, defaults.correction_saturation);
  obs_data_set_default_int(settings, kLumaSetting, defaults.correction_luma);
}

const char *ColorRangeCorrectionFilter::Name(void *)
{
  return "Color Range Correction";
}

void ColorRangeCorrectionFilter::load_effect()
{
  char *errors = nullptr;

  obs_enter_graphics();
  effect_ = gs_effect_create_from_file(effect_path_.c_str(), &errors);
  if (effect_) {
    texel_size_param_ = gs_effect_get_param_by_name(effect_, "texel_size");
    preview_mode_param_ = gs_effect_get_param_by_name(effect_, "preview_mode");
    hue_min_param_ = gs_effect_get_param_by_name(effect_, "hue_min");
    hue_max_param_ = gs_effect_get_param_by_name(effect_, "hue_max");
    hue_softness_param_ = gs_effect_get_param_by_name(effect_, "hue_softness");
    hue_full_range_param_ = gs_effect_get_param_by_name(effect_, "hue_full_range");
    sat_min_param_ = gs_effect_get_param_by_name(effect_, "sat_min");
    sat_max_param_ = gs_effect_get_param_by_name(effect_, "sat_max");
    sat_softness_param_ = gs_effect_get_param_by_name(effect_, "sat_softness");
    luma_min_param_ = gs_effect_get_param_by_name(effect_, "luma_min");
    luma_max_param_ = gs_effect_get_param_by_name(effect_, "luma_max");
    luma_softness_param_ = gs_effect_get_param_by_name(effect_, "luma_softness");
    invert_mask_param_ = gs_effect_get_param_by_name(effect_, "invert_mask");
    temperature_param_ = gs_effect_get_param_by_name(effect_, "temperature");
    tint_param_ = gs_effect_get_param_by_name(effect_, "tint");
    saturation_param_ = gs_effect_get_param_by_name(effect_, "correction_saturation");
    luma_param_ = gs_effect_get_param_by_name(effect_, "correction_luma");
    blur_param_ = gs_effect_get_param_by_name(effect_, "blur_amount");
  }
  obs_leave_graphics();

  if (errors) {
    blog(LOG_WARNING, "[obs-colorforge] Failed to load Color Range Correction effect: %s", errors);
    bfree(errors);
  }
}

void ColorRangeCorrectionFilter::update_settings(obs_data_t *settings)
{
  settings_.preview_mode = std::clamp(static_cast<int>(obs_data_get_int(settings, kPreviewModeSetting)), 0, 2);
  settings_.hue_min = std::clamp(static_cast<int>(obs_data_get_int(settings, kHueMinSetting)), 0, 360);
  settings_.hue_max = std::clamp(static_cast<int>(obs_data_get_int(settings, kHueMaxSetting)), 0, 360);
  settings_.hue_softness = std::clamp(static_cast<int>(obs_data_get_int(settings, kHueSoftnessSetting)), 0, 180);
  settings_.sat_min = std::clamp(static_cast<int>(obs_data_get_int(settings, kSatMinSetting)), 0, 100);
  settings_.sat_max = std::clamp(static_cast<int>(obs_data_get_int(settings, kSatMaxSetting)), 0, 100);
  settings_.sat_softness = std::clamp(static_cast<int>(obs_data_get_int(settings, kSatSoftnessSetting)), 0, 100);
  settings_.luma_min = std::clamp(static_cast<int>(obs_data_get_int(settings, kLumaMinSetting)), 0, 100);
  settings_.luma_max = std::clamp(static_cast<int>(obs_data_get_int(settings, kLumaMaxSetting)), 0, 100);
  settings_.luma_softness = std::clamp(static_cast<int>(obs_data_get_int(settings, kLumaSoftnessSetting)), 0, 100);
  settings_.blur = std::clamp(static_cast<int>(obs_data_get_int(settings, kBlurSetting)), 0, 20);
  settings_.invert_mask = obs_data_get_bool(settings, kInvertMaskSetting) ? 1 : 0;
  settings_.temperature = std::clamp(static_cast<int>(obs_data_get_int(settings, kTemperatureSetting)), -100, 100);
  settings_.tint = std::clamp(static_cast<int>(obs_data_get_int(settings, kTintSetting)), -100, 100);
  settings_.correction_saturation =
    std::clamp(static_cast<int>(obs_data_get_int(settings, kSaturationSetting)), 0, 200);
  settings_.correction_luma = std::clamp(static_cast<int>(obs_data_get_int(settings, kLumaSetting)), 0, 200);

  if (settings_.sat_min > settings_.sat_max) {
    std::swap(settings_.sat_min, settings_.sat_max);
  }
  if (settings_.luma_min > settings_.luma_max) {
    std::swap(settings_.luma_min, settings_.luma_max);
  }
}

void ColorRangeCorrectionFilter::save_settings(obs_data_t *settings) const
{
  obs_data_set_int(settings, kPreviewModeSetting, settings_.preview_mode);
  obs_data_set_int(settings, kHueMinSetting, settings_.hue_min);
  obs_data_set_int(settings, kHueMaxSetting, settings_.hue_max);
  obs_data_set_int(settings, kHueSoftnessSetting, settings_.hue_softness);
  obs_data_set_int(settings, kSatMinSetting, settings_.sat_min);
  obs_data_set_int(settings, kSatMaxSetting, settings_.sat_max);
  obs_data_set_int(settings, kSatSoftnessSetting, settings_.sat_softness);
  obs_data_set_int(settings, kLumaMinSetting, settings_.luma_min);
  obs_data_set_int(settings, kLumaMaxSetting, settings_.luma_max);
  obs_data_set_int(settings, kLumaSoftnessSetting, settings_.luma_softness);
  obs_data_set_int(settings, kBlurSetting, settings_.blur);
  obs_data_set_bool(settings, kInvertMaskSetting, settings_.invert_mask != 0);
  obs_data_set_int(settings, kTemperatureSetting, settings_.temperature);
  obs_data_set_int(settings, kTintSetting, settings_.tint);
  obs_data_set_int(settings, kSaturationSetting, settings_.correction_saturation);
  obs_data_set_int(settings, kLumaSetting, settings_.correction_luma);
}

void ColorRangeCorrectionFilter::open_editor()
{
  if (editor_dialog_) {
    editor_dialog_->show();
    editor_dialog_->raise();
    editor_dialog_->activateWindow();
    return;
  }

  dialog_original_settings_ = settings_;
  editor_dialog_ = new ColorRangeCorrectionEditorDialog(settings_, obs_parent_widget());
  editor_dialog_->setAttribute(Qt::WA_DeleteOnClose, true);
  editor_dialog_->setModal(false);

  QObject::connect(editor_dialog_, &ColorRangeCorrectionEditorDialog::settingsChanged, [this](const auto &settings) {
    settings_ = settings;
  });

  QObject::connect(editor_dialog_, &QDialog::accepted, [this]() {
    if (!editor_dialog_) {
      return;
    }

    settings_ = editor_dialog_->settings();
    obs_data_t *source_settings = obs_source_get_settings(source_);
    save_settings(source_settings);
    obs_source_update(source_, source_settings);
    obs_data_release(source_settings);
  });

  QObject::connect(editor_dialog_, &QDialog::rejected, [this]() { settings_ = dialog_original_settings_; });

  QObject::connect(editor_dialog_, &QObject::destroyed, [this]() { editor_dialog_ = nullptr; });

  editor_dialog_->show();
  editor_dialog_->raise();
  editor_dialog_->activateWindow();
}

bool ColorRangeCorrectionFilter::OpenEditorClicked(obs_properties_t *props, obs_property_t *property, void *data)
{
  (void)props;
  (void)property;

  auto *filter = static_cast<ColorRangeCorrectionFilter *>(data);
  if (filter) {
    filter->open_editor();
  }

  return false;
}

obs_source_info make_color_range_correction_filter_info()
{
  obs_source_info info {};
  info.id = kFilterId;
  info.type = OBS_SOURCE_TYPE_FILTER;
  info.output_flags = OBS_SOURCE_VIDEO;
  info.get_name = &ColorRangeCorrectionFilter::Name;
  info.create = &ColorRangeCorrectionFilter::Create;
  info.destroy = &ColorRangeCorrectionFilter::Destroy;
  info.update = &ColorRangeCorrectionFilter::Update;
  info.get_defaults = &ColorRangeCorrectionFilter::Defaults;
  info.get_properties = &ColorRangeCorrectionFilter::Properties;
  info.video_render = &ColorRangeCorrectionFilter::Render;
  return info;
}
