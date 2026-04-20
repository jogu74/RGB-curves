#include "hue_curves_filter.hpp"

#include "hue_curve_editor_dialog.hpp"

#include <QApplication>
#include <QColor>
#include <QDialog>
#include <QWidget>

namespace {

constexpr const char *kFilterId = "colorforge_hue_curves_filter";
constexpr const char *kCurveDataSetting = "curve_data";
constexpr const char *kEditorButtonSetting = "open_editor";
constexpr uint32_t kLutWidth = 256;
constexpr uint32_t kLutHeight = 1;
constexpr uint32_t kPreviewCaptureWidth = 256;
constexpr uint32_t kPreviewCaptureHeight = 144;

QWidget *obs_parent_widget()
{
  for (QWidget *widget : QApplication::topLevelWidgets()) {
    if (widget && widget->isVisible()) {
      return widget;
    }
  }

  return nullptr;
}

} // namespace

HueCurvesFilter::HueCurvesFilter(obs_data_t *settings, obs_source_t *source) : source_(source)
{
  char *effect_path = obs_module_file("effects/hue-curves.effect");
  effect_path_ = effect_path ? effect_path : "";
  bfree(effect_path);
  curves_ = hue_curves::curves_from_json(obs_data_get_string(settings, kCurveDataSetting));
  lut_data_ = hue_curves::build_lut_rgba(curves_);
  load_effect();
  rebuild_lut();
}

HueCurvesFilter::~HueCurvesFilter()
{
  obs_enter_graphics();
  if (preview_stage_) {
    gs_stagesurface_destroy(preview_stage_);
    preview_stage_ = nullptr;
  }
  if (preview_texrender_) {
    gs_texrender_destroy(preview_texrender_);
    preview_texrender_ = nullptr;
  }
  if (lut_texture_) {
    gs_texture_destroy(lut_texture_);
    lut_texture_ = nullptr;
  }
  if (effect_) {
    gs_effect_destroy(effect_);
    effect_ = nullptr;
  }
  obs_leave_graphics();
}

void *HueCurvesFilter::Create(obs_data_t *settings, obs_source_t *source)
{
  return new HueCurvesFilter(settings, source);
}

void HueCurvesFilter::Destroy(void *data)
{
  delete static_cast<HueCurvesFilter *>(data);
}

void HueCurvesFilter::Update(void *data, obs_data_t *settings)
{
  auto *filter = static_cast<HueCurvesFilter *>(data);
  filter->curves_ = hue_curves::curves_from_json(obs_data_get_string(settings, kCurveDataSetting));
  filter->lut_data_ = hue_curves::build_lut_rgba(filter->curves_);
  filter->rebuild_lut();
}

void HueCurvesFilter::Render(void *data, gs_effect_t *effect)
{
  Q_UNUSED(effect);

  auto *filter = static_cast<HueCurvesFilter *>(data);
  if (!filter) {
    return;
  }

  if (!filter->effect_ || !filter->lut_texture_) {
    obs_source_skip_video_filter(filter->source_);
    return;
  }

  if (!obs_source_process_filter_begin(filter->source_, GS_RGBA, OBS_ALLOW_DIRECT_RENDERING)) {
    return;
  }

  gs_effect_set_texture(filter->lut_param_, filter->lut_texture_);
  obs_source_process_filter_end(filter->source_, filter->effect_, 0, 0);
}

obs_properties_t *HueCurvesFilter::Properties(void *data)
{
  auto *props = obs_properties_create();
  obs_properties_add_text(props, kCurveDataSetting, "Curve Data", OBS_TEXT_MULTILINE);
  obs_properties_add_button(props, kEditorButtonSetting, "Open Hue Curves Editor", &HueCurvesFilter::OpenEditorClicked);

  if (data) {
    obs_property_t *curve_property = obs_properties_get(props, kCurveDataSetting);
    if (curve_property) {
      obs_property_set_visible(curve_property, false);
    }
  }

  return props;
}

void HueCurvesFilter::Defaults(obs_data_t *settings)
{
  const std::string json = rgb_curves::qstring_to_utf8(hue_curves::curves_to_json(hue_curves::default_curves()));
  obs_data_set_default_string(settings, kCurveDataSetting, json.c_str());
}

const char *HueCurvesFilter::Name(void *)
{
  return "Hue Curves";
}

std::array<float, 256> HueCurvesFilter::histogram(int channel_index) const
{
  Q_UNUSED(channel_index);
  return hue_histogram_;
}

QImage HueCurvesFilter::preview_image() const
{
  return preview_image_;
}

void HueCurvesFilter::load_effect()
{
  char *errors = nullptr;

  obs_enter_graphics();
  effect_ = gs_effect_create_from_file(effect_path_.c_str(), &errors);
  if (effect_) {
    lut_param_ = gs_effect_get_param_by_name(effect_, "curve_lut");
  }
  obs_leave_graphics();

  if (errors) {
    blog(LOG_WARNING, "[obs-colorforge] Failed to load Hue Curves effect: %s", errors);
    bfree(errors);
  }
}

void HueCurvesFilter::rebuild_lut()
{
  obs_enter_graphics();

  if (!lut_texture_) {
    const uint8_t *data[] = {lut_data_.data()};
    lut_texture_ = gs_texture_create(kLutWidth, kLutHeight, GS_RGBA, 1, data, GS_DYNAMIC);
  } else {
    gs_texture_set_image(lut_texture_, lut_data_.data(), kLutWidth * 4, false);
  }

  obs_leave_graphics();
}

void HueCurvesFilter::save_curves(obs_data_t *settings) const
{
  const std::string json = rgb_curves::qstring_to_utf8(hue_curves::curves_to_json(curves_));
  obs_data_set_string(settings, kCurveDataSetting, json.c_str());
}

void HueCurvesFilter::open_editor()
{
  const auto original_curves = curves_;
  HueCurveEditorDialog dialog(curves_, obs_parent_widget());
  dialog.set_histogram_provider([this](int channel_index) {
    update_preview_from_target();
    return histogram(channel_index);
  });
  dialog.set_preview_provider([this]() { return preview_image(); });
  QObject::connect(&dialog, &HueCurveEditorDialog::curvesChanged, [this](const auto &curves) {
    curves_ = curves;
    lut_data_ = hue_curves::build_lut_rgba(curves_);
    rebuild_lut();
  });

  if (dialog.exec() != QDialog::Accepted) {
    curves_ = original_curves;
    lut_data_ = hue_curves::build_lut_rgba(curves_);
    rebuild_lut();
    return;
  }

  curves_ = dialog.curves();
  lut_data_ = hue_curves::build_lut_rgba(curves_);
  rebuild_lut();

  obs_data_t *settings = obs_source_get_settings(source_);
  save_curves(settings);
  obs_source_update(source_, settings);
  obs_data_release(settings);
}

bool HueCurvesFilter::OpenEditorClicked(obs_properties_t *props, obs_property_t *property, void *data)
{
  Q_UNUSED(props);
  Q_UNUSED(property);

  auto *filter = static_cast<HueCurvesFilter *>(data);
  if (filter) {
    filter->open_editor();
  }

  return false;
}

void HueCurvesFilter::ensure_preview_surfaces(uint32_t width, uint32_t height)
{
  width = std::max(width, 1u);
  height = std::max(height, 1u);

  obs_enter_graphics();
  if (!preview_texrender_) {
    preview_texrender_ = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
  }

  if (!preview_stage_ || preview_width_ != width || preview_height_ != height) {
    if (preview_stage_) {
      gs_stagesurface_destroy(preview_stage_);
    }
    preview_stage_ = gs_stagesurface_create(width, height, GS_RGBA);
    preview_width_ = width;
    preview_height_ = height;
  }
  obs_leave_graphics();
}

void HueCurvesFilter::update_preview_from_target()
{
  obs_source_t *target = obs_filter_get_target(source_);
  if (!target) {
    hue_histogram_.fill(0.0f);
    preview_image_ = QImage();
    return;
  }

  const uint32_t source_width = obs_source_get_width(target);
  const uint32_t source_height = obs_source_get_height(target);
  ensure_preview_surfaces(kPreviewCaptureWidth, kPreviewCaptureHeight);
  if (!preview_texrender_ || !preview_stage_) {
    hue_histogram_.fill(0.0f);
    preview_image_ = QImage();
    return;
  }

  std::array<uint32_t, 256> hue_bins {};
  bool mapped = false;
  uint8_t *data = nullptr;
  uint32_t linesize = 0;

  obs_enter_graphics();
  gs_texrender_reset(preview_texrender_);
  if (gs_texrender_begin(preview_texrender_, preview_width_, preview_height_)) {
    struct vec4 clear_color;
    vec4_zero(&clear_color);
    gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
    gs_blend_state_push();
    gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
    gs_viewport_push();
    gs_projection_push();
    gs_matrix_push();
    gs_ortho(0.0f, static_cast<float>(preview_width_), 0.0f, static_cast<float>(preview_height_), -100.0f, 100.0f);
    if (source_width > 0 && source_height > 0) {
      const float scale_x = static_cast<float>(preview_width_) / static_cast<float>(source_width);
      const float scale_y = static_cast<float>(preview_height_) / static_cast<float>(source_height);
      gs_matrix_scale3f(scale_x, scale_y, 1.0f);
    }
    obs_source_video_render(target);
    gs_matrix_pop();
    gs_projection_pop();
    gs_viewport_pop();
    gs_blend_state_pop();
    gs_texrender_end(preview_texrender_);
    gs_stage_texture(preview_stage_, gs_texrender_get_texture(preview_texrender_));
    mapped = gs_stagesurface_map(preview_stage_, &data, &linesize);
  }

  if (mapped) {
    QImage preview(static_cast<int>(preview_width_), static_cast<int>(preview_height_), QImage::Format_ARGB32);
    for (uint32_t y = 0; y < preview_height_; ++y) {
      const uint8_t *row = data + (static_cast<size_t>(linesize) * y);
      QRgb *preview_row = reinterpret_cast<QRgb *>(preview.scanLine(static_cast<int>(y)));
      for (uint32_t x = 0; x < preview_width_; ++x) {
        const uint8_t r = row[x * 4 + 0];
        const uint8_t g = row[x * 4 + 1];
        const uint8_t b = row[x * 4 + 2];
        preview_row[x] = qRgb(r, g, b);

        const QColor color = QColor::fromRgb(r, g, b);
        const qreal saturation = color.hsvSaturationF();
        const qreal hue = color.hsvHueF();
        if (saturation > 0.02 && hue >= 0.0) {
          const int index = std::clamp(static_cast<int>(std::lround(hue * 255.0)), 0, 255);
          hue_bins[static_cast<size_t>(index)] += 1;
        }
      }
    }
    preview_image_ = preview;
    gs_stagesurface_unmap(preview_stage_);
  } else {
    preview_image_ = QImage();
  }
  obs_leave_graphics();

  const auto max_it = std::max_element(hue_bins.begin(), hue_bins.end());
  const double max_value = *max_it > 0 ? static_cast<double>(*max_it) : 1.0;
  for (size_t i = 0; i < hue_histogram_.size(); ++i) {
    const double normalized = static_cast<double>(hue_bins[i]) / max_value;
    hue_histogram_[i] = static_cast<float>(std::sqrt(normalized));
  }
}

obs_source_info make_hue_curves_filter_info()
{
  obs_source_info info {};
  info.id = kFilterId;
  info.type = OBS_SOURCE_TYPE_FILTER;
  info.output_flags = OBS_SOURCE_VIDEO;
  info.get_name = &HueCurvesFilter::Name;
  info.create = &HueCurvesFilter::Create;
  info.destroy = &HueCurvesFilter::Destroy;
  info.update = &HueCurvesFilter::Update;
  info.get_defaults = &HueCurvesFilter::Defaults;
  info.get_properties = &HueCurvesFilter::Properties;
  info.video_render = &HueCurvesFilter::Render;
  return info;
}
