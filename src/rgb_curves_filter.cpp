#include "rgb_curves_filter.hpp"

#include "curve_editor_dialog.hpp"

#include <QApplication>
#include <QDialog>
#include <QWidget>

namespace {

constexpr const char *kFilterId = "rgb_curves_filter";
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

RGBCurvesFilter::RGBCurvesFilter(obs_data_t *settings, obs_source_t *source) : source_(source)
{
  char *effect_path = obs_module_file("effects/rgb-curves.effect");
  effect_path_ = effect_path ? effect_path : "";
  bfree(effect_path);
  curves_ = rgb_curves::curves_from_json(obs_data_get_string(settings, kCurveDataSetting));
  lut_data_ = rgb_curves::build_lut_rgba(curves_);
  load_effect();
  rebuild_lut();
}

RGBCurvesFilter::~RGBCurvesFilter()
{
  obs_enter_graphics();
  if (histogram_stage_) {
    gs_stagesurface_destroy(histogram_stage_);
    histogram_stage_ = nullptr;
  }
  if (histogram_texrender_) {
    gs_texrender_destroy(histogram_texrender_);
    histogram_texrender_ = nullptr;
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

void *RGBCurvesFilter::Create(obs_data_t *settings, obs_source_t *source)
{
  return new RGBCurvesFilter(settings, source);
}

void RGBCurvesFilter::Destroy(void *data)
{
  delete static_cast<RGBCurvesFilter *>(data);
}

void RGBCurvesFilter::Update(void *data, obs_data_t *settings)
{
  auto *filter = static_cast<RGBCurvesFilter *>(data);
  filter->curves_ = rgb_curves::curves_from_json(obs_data_get_string(settings, kCurveDataSetting));
  filter->lut_data_ = rgb_curves::build_lut_rgba(filter->curves_);
  filter->rebuild_lut();
}

void RGBCurvesFilter::Render(void *data, gs_effect_t *effect)
{
  Q_UNUSED(effect);

  auto *filter = static_cast<RGBCurvesFilter *>(data);
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

obs_properties_t *RGBCurvesFilter::Properties(void *data)
{
  auto *props = obs_properties_create();
  obs_properties_add_text(props, kCurveDataSetting, "Curve Data", OBS_TEXT_MULTILINE);
  obs_properties_add_button(props, kEditorButtonSetting, "Open Curve Editor", &RGBCurvesFilter::OpenEditorClicked);

  if (data) {
    obs_property_t *curve_property = obs_properties_get(props, kCurveDataSetting);
    if (curve_property) {
      obs_property_set_visible(curve_property, false);
    }
  }

  return props;
}

void RGBCurvesFilter::Defaults(obs_data_t *settings)
{
  const std::string json = rgb_curves::qstring_to_utf8(rgb_curves::curves_to_json(rgb_curves::default_curves()));
  obs_data_set_default_string(settings, kCurveDataSetting, json.c_str());
}

const char *RGBCurvesFilter::Name(void *)
{
  return "RGB Curves";
}

std::array<float, 256> RGBCurvesFilter::histogram(size_t channel_index) const
{
  return histograms_[std::min(channel_index, histograms_.size() - 1)];
}

QImage RGBCurvesFilter::preview_image() const
{
  return preview_image_;
}

void RGBCurvesFilter::load_effect()
{
  char *errors = nullptr;

  obs_enter_graphics();
  effect_ = gs_effect_create_from_file(effect_path_.c_str(), &errors);
  if (effect_) {
    lut_param_ = gs_effect_get_param_by_name(effect_, "curve_lut");
  }
  obs_leave_graphics();

  if (errors) {
    blog(LOG_WARNING, "[rgb-curves] Failed to load effect: %s", errors);
    bfree(errors);
  }
}

void RGBCurvesFilter::rebuild_lut()
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

void RGBCurvesFilter::save_curves(obs_data_t *settings) const
{
  const std::string json = rgb_curves::qstring_to_utf8(rgb_curves::curves_to_json(curves_));
  obs_data_set_string(settings, kCurveDataSetting, json.c_str());
}

void RGBCurvesFilter::open_editor()
{
  const auto original_curves = curves_;
  CurveEditorDialog dialog(curves_, obs_parent_widget());
  dialog.set_histogram_provider([this](int channel_index) {
    update_histogram_from_target();
    return histogram(static_cast<size_t>(std::clamp(channel_index, 0, 3)));
  });
  dialog.set_preview_provider([this]() { return preview_image(); });
  QObject::connect(&dialog, &CurveEditorDialog::curvesChanged, [&dialog, this](const auto &curves) {
    Q_UNUSED(dialog);
    curves_ = curves;
    lut_data_ = rgb_curves::build_lut_rgba(curves_);
    rebuild_lut();
  });

  if (dialog.exec() != QDialog::Accepted) {
    curves_ = original_curves;
    lut_data_ = rgb_curves::build_lut_rgba(curves_);
    rebuild_lut();
    return;
  }

  curves_ = dialog.curves();
  lut_data_ = rgb_curves::build_lut_rgba(curves_);
  rebuild_lut();

  obs_data_t *settings = obs_source_get_settings(source_);
  save_curves(settings);
  obs_source_update(source_, settings);
  obs_data_release(settings);
}

bool RGBCurvesFilter::OpenEditorClicked(obs_properties_t *props, obs_property_t *property, void *data)
{
  Q_UNUSED(props);
  Q_UNUSED(property);

  auto *filter = static_cast<RGBCurvesFilter *>(data);
  if (filter) {
    filter->open_editor();
  }

  return false;
}

void RGBCurvesFilter::ensure_histogram_surfaces(uint32_t width, uint32_t height)
{
  width = std::max(width, 1u);
  height = std::max(height, 1u);

  obs_enter_graphics();
  if (!histogram_texrender_) {
    histogram_texrender_ = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
  }

  if (!histogram_stage_ || histogram_width_ != width || histogram_height_ != height) {
    if (histogram_stage_) {
      gs_stagesurface_destroy(histogram_stage_);
    }
    histogram_stage_ = gs_stagesurface_create(width, height, GS_RGBA);
    histogram_width_ = width;
    histogram_height_ = height;
  }
  obs_leave_graphics();
}

void RGBCurvesFilter::update_histogram_from_target()
{
  obs_source_t *target = obs_filter_get_target(source_);
  if (!target) {
    for (auto &histogram : histograms_) {
      histogram.fill(0.0f);
    }
    preview_image_ = QImage();
    return;
  }

  const uint32_t source_width = obs_source_get_width(target);
  const uint32_t source_height = obs_source_get_height(target);
  ensure_histogram_surfaces(kPreviewCaptureWidth, kPreviewCaptureHeight);
  if (!histogram_texrender_ || !histogram_stage_) {
    for (auto &histogram : histograms_) {
      histogram.fill(0.0f);
    }
    preview_image_ = QImage();
    return;
  }

  std::array<uint32_t, 256> luma_bins {};
  std::array<uint32_t, 256> red_bins {};
  std::array<uint32_t, 256> green_bins {};
  std::array<uint32_t, 256> blue_bins {};
  bool mapped = false;
  uint8_t *data = nullptr;
  uint32_t linesize = 0;

  obs_enter_graphics();
  gs_texrender_reset(histogram_texrender_);
  if (gs_texrender_begin(histogram_texrender_, histogram_width_, histogram_height_)) {
    struct vec4 clear_color;
    vec4_zero(&clear_color);
    gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
    gs_blend_state_push();
    gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
    gs_viewport_push();
    gs_projection_push();
    gs_matrix_push();
    gs_ortho(0.0f, static_cast<float>(histogram_width_), 0.0f, static_cast<float>(histogram_height_), -100.0f,
             100.0f);
    if (source_width > 0 && source_height > 0) {
      const float scale_x = static_cast<float>(histogram_width_) / static_cast<float>(source_width);
      const float scale_y = static_cast<float>(histogram_height_) / static_cast<float>(source_height);
      gs_matrix_scale3f(scale_x, scale_y, 1.0f);
    }
    obs_source_video_render(target);
    gs_matrix_pop();
    gs_projection_pop();
    gs_viewport_pop();
    gs_blend_state_pop();
    gs_texrender_end(histogram_texrender_);
    gs_stage_texture(histogram_stage_, gs_texrender_get_texture(histogram_texrender_));
    mapped = gs_stagesurface_map(histogram_stage_, &data, &linesize);
  }

  if (mapped) {
    QImage preview(static_cast<int>(histogram_width_), static_cast<int>(histogram_height_), QImage::Format_ARGB32);
    for (uint32_t y = 0; y < histogram_height_; ++y) {
      const uint8_t *row = data + (static_cast<size_t>(linesize) * y);
      QRgb *preview_row = reinterpret_cast<QRgb *>(preview.scanLine(static_cast<int>(y)));
      for (uint32_t x = 0; x < histogram_width_; ++x) {
        const uint8_t r = row[x * 4 + 0];
        const uint8_t g = row[x * 4 + 1];
        const uint8_t b = row[x * 4 + 2];
        preview_row[x] = qRgb(r, g, b);
        const double luminance = (0.2126 * r) + (0.7152 * g) + (0.0722 * b);
        const int index = std::clamp(static_cast<int>(std::lround(luminance)), 0, 255);
        luma_bins[static_cast<size_t>(index)] += 1;
        red_bins[static_cast<size_t>(r)] += 1;
        green_bins[static_cast<size_t>(g)] += 1;
        blue_bins[static_cast<size_t>(b)] += 1;
      }
    }
    preview_image_ = preview;
    gs_stagesurface_unmap(histogram_stage_);
  } else {
    preview_image_ = QImage();
  }
  obs_leave_graphics();

  const std::array<std::array<uint32_t, 256>, 4> bins_by_channel = {luma_bins, red_bins, green_bins, blue_bins};
  for (size_t channel = 0; channel < histograms_.size(); ++channel) {
    const auto max_it = std::max_element(bins_by_channel[channel].begin(), bins_by_channel[channel].end());
    const double max_value = *max_it > 0 ? static_cast<double>(*max_it) : 1.0;
    for (size_t i = 0; i < histograms_[channel].size(); ++i) {
      const double normalized = static_cast<double>(bins_by_channel[channel][i]) / max_value;
      histograms_[channel][i] = static_cast<float>(std::sqrt(normalized));
    }
  }
}

obs_source_info make_rgb_curves_filter_info()
{
  obs_source_info info {};
  info.id = kFilterId;
  info.type = OBS_SOURCE_TYPE_FILTER;
  info.output_flags = OBS_SOURCE_VIDEO;
  info.get_name = &RGBCurvesFilter::Name;
  info.create = &RGBCurvesFilter::Create;
  info.destroy = &RGBCurvesFilter::Destroy;
  info.update = &RGBCurvesFilter::Update;
  info.get_defaults = &RGBCurvesFilter::Defaults;
  info.get_properties = &RGBCurvesFilter::Properties;
  info.video_render = &RGBCurvesFilter::Render;
  return info;
}
