#include "curve_editor_dialog.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <obs-module.h>

namespace {

constexpr std::array<const char *, 4> kLabels = {"Neutral", "Red", "Green", "Blue"};
constexpr const char *kBuildStamp = "Build: " __DATE__ " " __TIME__;

QString preset_directory_path()
{
  char *config_path = obs_module_config_path("presets");
  if (!config_path) {
    return {};
  }

  const QString path = QString::fromUtf8(config_path);
  bfree(config_path);
  return path;
}

QString sanitize_preset_name(QString name)
{
  name = name.trimmed();
  if (name.isEmpty()) {
    return {};
  }

  for (QChar &ch : name) {
    if (ch == '<' || ch == '>' || ch == ':' || ch == '"' || ch == '/' || ch == '\\' || ch == '|' || ch == '?' ||
        ch == '*') {
      ch = '_';
    }
  }

  return name;
}

QString saved_curve_file_path(const QString &name)
{
  const QString directory = preset_directory_path();
  if (directory.isEmpty()) {
    return {};
  }

  QDir dir(directory);
  if (!dir.exists() && !dir.mkpath(".")) {
    return {};
  }

  return dir.filePath(name + ".json");
}

QStringList saved_curve_names()
{
  const QString directory = preset_directory_path();
  if (directory.isEmpty()) {
    return {};
  }

  QDir dir(directory);
  if (!dir.exists()) {
    return {};
  }

  QStringList names;
  const QFileInfoList files = dir.entryInfoList({"*.json"}, QDir::Files, QDir::Name);
  for (const QFileInfo &file_info : files) {
    names.push_back(file_info.completeBaseName());
  }

  return names;
}

QColor apply_curves_to_color(const std::array<rgb_curves::PreparedCurve, 4> &curves, const QColor &input)
{
  const double r = input.redF();
  const double g = input.greenF();
  const double b = input.blueF();

  const double luminance = (0.2126 * r) + (0.7152 * g) + (0.0722 * b);
  const double mapped_luminance = rgb_curves::sample_prepared_curve(curves[0], luminance);
  const double gain = luminance > 1e-4 ? mapped_luminance / luminance : mapped_luminance;

  return QColor::fromRgbF(
    rgb_curves::sample_prepared_curve(curves[1], std::clamp(r * gain, 0.0, 1.0)),
    rgb_curves::sample_prepared_curve(curves[2], std::clamp(g * gain, 0.0, 1.0)),
    rgb_curves::sample_prepared_curve(curves[3], std::clamp(b * gain, 0.0, 1.0)));
}

} // namespace

CurveEditorDialog::CurveEditorDialog(const std::array<rgb_curves::CurvePoints, 4> &initial_curves,
                                     QWidget *parent)
  : QDialog(parent), curves_(initial_curves)
{
  setWindowTitle("RGB Curves");
  resize(620, 620);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(14, 14, 14, 14);
  layout->setSpacing(10);

  build_label_ = new QLabel(QString::fromUtf8(kBuildStamp), this);
  build_label_->setStyleSheet("QLabel { color: #9a9a9a; }");
  layout->addWidget(build_label_);

  auto *header = new QLabel(
    "Click to add a point, drag to move it, and double-click or right-click to remove it. "
    "Endpoint handles can move horizontally to create flat plateaus at the edges.");
  header->setWordWrap(true);
  layout->addWidget(header);

  auto *preset_row = new QHBoxLayout();
  preset_combo_ = new QComboBox(this);
  preset_combo_->setPlaceholderText("Saved curves");
  refresh_preset_combo();
  preset_row->addWidget(preset_combo_, 1);

  auto *load_button = new QPushButton("Load...", this);
  connect(load_button, &QPushButton::clicked, this, &CurveEditorDialog::load_preset);
  preset_row->addWidget(load_button);

  auto *save_button = new QPushButton("Save...", this);
  connect(save_button, &QPushButton::clicked, this, &CurveEditorDialog::save_preset);
  preset_row->addWidget(save_button);

  auto *delete_button = new QPushButton("Delete...", this);
  connect(delete_button, &QPushButton::clicked, this, &CurveEditorDialog::delete_preset);
  preset_row->addWidget(delete_button);

  auto *rename_button = new QPushButton("Rename...", this);
  connect(rename_button, &QPushButton::clicked, this, &CurveEditorDialog::rename_preset);
  preset_row->addWidget(rename_button);

  auto *export_button = new QPushButton("Export...", this);
  connect(export_button, &QPushButton::clicked, this, &CurveEditorDialog::export_preset);
  preset_row->addWidget(export_button);

  auto *import_button = new QPushButton("Import...", this);
  connect(import_button, &QPushButton::clicked, this, &CurveEditorDialog::import_preset);
  preset_row->addWidget(import_button);
  layout->addLayout(preset_row);

  auto *button_row = new QHBoxLayout();
  button_row->setSpacing(8);
  for (int i = 0; i < static_cast<int>(channel_buttons_.size()); ++i) {
    auto *button = new QPushButton(QString::fromUtf8(kLabels[static_cast<size_t>(i)]));
    button->setCheckable(true);
    button->setMinimumHeight(30);
    channel_buttons_[static_cast<size_t>(i)] = button;
    connect(button, &QPushButton::clicked, this, [this, i] { set_channel(i); });
    button_row->addWidget(button);
  }
  layout->addLayout(button_row);

  curve_widget_ = new CurveWidget(this);
  layout->addWidget(curve_widget_, 1);
  connect(curve_widget_, &CurveWidget::curveChanged, this, &CurveEditorDialog::handle_curve_changed);

  auto *preview_title = new QLabel("Preview");
  layout->addWidget(preview_title);

  preview_label_ = new QLabel(this);
  preview_label_->setFixedHeight(150);
  preview_label_->setMinimumWidth(420);
  preview_label_->setAlignment(Qt::AlignCenter);
  preview_label_->setStyleSheet("QLabel { background-color: #181818; border: 1px solid #444; }");
  layout->addWidget(preview_label_);

  auto *footer = new QHBoxLayout();
  auto *reset_button = new QPushButton("Reset Active Curve");
  connect(reset_button, &QPushButton::clicked, this, &CurveEditorDialog::reset_active_curve);
  footer->addWidget(reset_button);

  auto *reset_all_button = new QPushButton("Reset All Curves");
  connect(reset_all_button, &QPushButton::clicked, this, &CurveEditorDialog::reset_all_curves);
  footer->addWidget(reset_all_button);

  footer->addStretch(1);
  layout->addLayout(footer);

  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  layout->addWidget(buttons);

  histogram_timer_ = new QTimer(this);
  histogram_timer_->setInterval(350);
  connect(histogram_timer_, &QTimer::timeout, this, &CurveEditorDialog::refresh_histogram);

  invalidate_curve_cache();
  set_channel(0);
  update_preview();
}

void CurveEditorDialog::set_histogram_provider(std::function<std::array<float, 256>(int)> provider)
{
  histogram_provider_ = std::move(provider);
  if (histogram_provider_) {
    histogram_timer_->start();
    refresh_histogram();
  } else {
    histogram_timer_->stop();
  }
}

void CurveEditorDialog::set_preview_provider(std::function<QImage()> provider)
{
  preview_provider_ = std::move(provider);
  set_preview_source_image(preview_provider_ ? preview_provider_() : QImage());
  update_preview();
}

std::array<rgb_curves::CurvePoints, 4> CurveEditorDialog::curves() const
{
  return curves_;
}

void CurveEditorDialog::set_channel(int index)
{
  active_channel_ = std::clamp(index, 0, 3);
  curve_widget_->set_curve(curves_[static_cast<size_t>(active_channel_)]);
  refresh_channel_buttons();
  curve_widget_->update();
}

void CurveEditorDialog::handle_curve_changed(const rgb_curves::CurvePoints &curve)
{
  curves_[static_cast<size_t>(active_channel_)] = curve;
  prepared_curves_[static_cast<size_t>(active_channel_)] = rgb_curves::prepare_curve(curve);
  ++curves_revision_;
  update_preview();
  emit curvesChanged(curves_);
}

void CurveEditorDialog::reset_active_curve()
{
  curves_[static_cast<size_t>(active_channel_)] = rgb_curves::default_curve();
  curve_widget_->set_curve(curves_[static_cast<size_t>(active_channel_)]);
  prepared_curves_[static_cast<size_t>(active_channel_)] =
    rgb_curves::prepare_curve(curves_[static_cast<size_t>(active_channel_)]);
  ++curves_revision_;
  update_preview();
  emit curvesChanged(curves_);
}

void CurveEditorDialog::reset_all_curves()
{
  curves_ = rgb_curves::default_curves();
  invalidate_curve_cache();
  curve_widget_->set_curve(curves_[static_cast<size_t>(active_channel_)]);
  update_preview();
  emit curvesChanged(curves_);
}

void CurveEditorDialog::save_preset()
{
  const QString entered_name =
    QInputDialog::getText(this, "Save Curve", "Saved curve name:", QLineEdit::Normal, "My Curve");
  const QString preset_name = sanitize_preset_name(entered_name);
  if (preset_name.isEmpty()) {
    return;
  }

  const QString path = saved_curve_file_path(preset_name);
  if (path.isEmpty()) {
    QMessageBox::warning(this, "Save Failed", "Could not access the saved curves folder.");
    return;
  }

  if (QFile::exists(path)) {
    const auto overwrite = QMessageBox::question(
      this, "Overwrite Saved Curve", QString("Saved curve \"%1\" already exists. Overwrite it?").arg(preset_name),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (overwrite != QMessageBox::Yes) {
      return;
    }
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    QMessageBox::warning(this, "Save Failed", "Could not write the saved curve file.");
    return;
  }

  file.write(rgb_curves::curves_to_json(curves_).toUtf8());
  refresh_preset_combo();

  const int combo_index = preset_combo_ ? preset_combo_->findData(preset_name) : -1;
  if (preset_combo_ && combo_index >= 0) {
    preset_combo_->setCurrentIndex(combo_index);
  }
}

void CurveEditorDialog::load_preset()
{
  if (!preset_combo_ || preset_combo_->count() == 0) {
    QMessageBox::information(this, "No Saved Curves", "There are no saved curves yet.");
    return;
  }

  const QString preset_name = preset_combo_->currentData().toString();
  if (preset_name.isEmpty()) {
    QMessageBox::information(this, "No Saved Curves", "Choose a saved curve from the list first.");
    return;
  }

  if (!load_saved_preset_by_name(preset_name)) {
    QMessageBox::warning(this, "Load Failed", "Could not open the saved curve.");
  }
}

void CurveEditorDialog::delete_preset()
{
  if (!preset_combo_ || preset_combo_->count() == 0) {
    QMessageBox::information(this, "No Saved Curves", "There are no saved curves to delete.");
    return;
  }

  const QString preset_name = preset_combo_->currentData().toString();
  if (preset_name.isEmpty()) {
    QMessageBox::information(this, "No Saved Curves", "Choose a saved curve from the list first.");
    return;
  }

  const auto confirmation = QMessageBox::question(
    this, "Delete Saved Curve", QString("Delete saved curve \"%1\"?").arg(preset_name),
    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (confirmation != QMessageBox::Yes) {
    return;
  }

  const QString path = saved_curve_file_path(preset_name);
  if (path.isEmpty() || !QFile::exists(path)) {
    QMessageBox::warning(this, "Delete Failed", "Could not find the saved curve file.");
    return;
  }

  if (!QFile::remove(path)) {
    QMessageBox::warning(this, "Delete Failed", "Could not delete the saved curve file.");
    return;
  }

  refresh_preset_combo();
  if (preset_combo_ && preset_combo_->count() > 0) {
    preset_combo_->setCurrentIndex(0);
  }
}

void CurveEditorDialog::rename_preset()
{
  if (!preset_combo_ || preset_combo_->count() == 0) {
    QMessageBox::information(this, "No Saved Curves", "There are no saved curves to rename.");
    return;
  }

  const QString current_name = preset_combo_->currentData().toString();
  if (current_name.isEmpty()) {
    QMessageBox::information(this, "No Saved Curves", "Choose a saved curve from the list first.");
    return;
  }

  const QString entered_name =
    QInputDialog::getText(this, "Rename Saved Curve", "New name:", QLineEdit::Normal, current_name);
  const QString new_name = sanitize_preset_name(entered_name);
  if (new_name.isEmpty() || new_name == current_name) {
    return;
  }

  const QString old_path = saved_curve_file_path(current_name);
  const QString new_path = saved_curve_file_path(new_name);
  if (old_path.isEmpty() || new_path.isEmpty()) {
    QMessageBox::warning(this, "Rename Failed", "Could not access the saved curves folder.");
    return;
  }

  if (QFile::exists(new_path)) {
    const auto overwrite = QMessageBox::question(
      this, "Overwrite Saved Curve", QString("Saved curve \"%1\" already exists. Overwrite it?").arg(new_name),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (overwrite != QMessageBox::Yes) {
      return;
    }
    if (!QFile::remove(new_path)) {
      QMessageBox::warning(this, "Rename Failed", "Could not replace the existing saved curve.");
      return;
    }
  }

  if (!QFile::rename(old_path, new_path)) {
    QMessageBox::warning(this, "Rename Failed", "Could not rename the saved curve.");
    return;
  }

  refresh_preset_combo();
  const int combo_index = preset_combo_ ? preset_combo_->findData(new_name) : -1;
  if (preset_combo_ && combo_index >= 0) {
    preset_combo_->setCurrentIndex(combo_index);
  }
}

void CurveEditorDialog::export_preset()
{
  const QString path =
    QFileDialog::getSaveFileName(this, "Export Curve", "rgb-curves.json", "RGB Curves (*.json)");
  if (path.isEmpty()) {
    return;
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    QMessageBox::warning(this, "Export Failed", "Could not write the export file.");
    return;
  }

  file.write(rgb_curves::curves_to_json(curves_).toUtf8());
}

void CurveEditorDialog::import_preset()
{
  const QString path = QFileDialog::getOpenFileName(this, "Import Curve", QString(), "RGB Curves (*.json)");
  if (path.isEmpty()) {
    return;
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    QMessageBox::warning(this, "Import Failed", "Could not open the import file.");
    return;
  }

  apply_curves(rgb_curves::curves_from_json(file.readAll().constData()));
}

void CurveEditorDialog::refresh_histogram()
{
  bool preview_changed = false;

  if (histogram_provider_) {
    curve_widget_->set_histogram(histogram_provider_(active_channel_));
  }

  if (preview_provider_) {
    const QImage image = preview_provider_();
    const qint64 key = image.isNull() ? 0 : image.cacheKey();
    if (key != preview_source_cache_key_) {
      set_preview_source_image(image);
      preview_changed = true;
    }
  }

  if (preview_changed) {
    update_preview();
  }
}

void CurveEditorDialog::refresh_preset_combo()
{
  if (!preset_combo_) {
    return;
  }

  const QVariant current_data = preset_combo_->currentData();
  const QString current_text = preset_combo_->currentText();

  preset_combo_->clear();
  const QStringList names = saved_curve_names();
  for (const QString &name : names) {
    preset_combo_->addItem(name, name);
  }

  int restore_index = -1;
  if (current_data.isValid()) {
    restore_index = preset_combo_->findData(current_data);
  }
  if (restore_index < 0 && !current_text.isEmpty()) {
    restore_index = preset_combo_->findText(current_text);
  }
  if (restore_index >= 0) {
    preset_combo_->setCurrentIndex(restore_index);
  } else if (preset_combo_->count() > 0) {
    preset_combo_->setCurrentIndex(0);
  }
}

bool CurveEditorDialog::load_saved_preset_by_name(const QString &preset_name)
{
  const QString path = saved_curve_file_path(preset_name);
  if (path.isEmpty()) {
    return false;
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }

  apply_curves(rgb_curves::curves_from_json(file.readAll().constData()));
  const int combo_index = preset_combo_ ? preset_combo_->findData(preset_name) : -1;
  if (preset_combo_ && combo_index >= 0) {
    preset_combo_->setCurrentIndex(combo_index);
  }
  return true;
}

void CurveEditorDialog::refresh_channel_buttons()
{
  for (int i = 0; i < static_cast<int>(channel_buttons_.size()); ++i) {
    auto *button = channel_buttons_[static_cast<size_t>(i)];
    button->setChecked(i == active_channel_);
    const QColor color = color_for_channel(i);
    button->setStyleSheet(QString(
      "QPushButton { background-color: rgb(%1,%2,%3); color: white; border: 1px solid #3a3a3a; padding: 6px 12px; }"
      "QPushButton:checked { border: 2px solid #f2f2f2; }")
                            .arg(color.red())
                            .arg(color.green())
                            .arg(color.blue()));
  }
}

QColor CurveEditorDialog::color_for_channel(int index) const
{
  switch (index) {
  case 1:
    return QColor(163, 30, 44);
  case 2:
    return QColor(34, 150, 62);
  case 3:
    return QColor(34, 74, 189);
  case 0:
  default:
    return QColor(118, 118, 118);
  }
}

void CurveEditorDialog::update_preview()
{
  if (!preview_label_) {
    return;
  }

  if (rendered_preview_revision_ == curves_revision_ && rendered_preview_source_key_ == preview_source_cache_key_) {
    return;
  }

  const QSize canvas_size = preview_label_->size().isValid() ? preview_label_->size() : QSize(440, 150);
  QImage image = preview_source_image_;
  if (image.isNull()) {
    image = QImage(canvas_size, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(24, 24, 24));
    QPainter painter(&image);
    painter.setPen(QColor(160, 160, 160));
    painter.drawText(image.rect(), Qt::AlignCenter, "Live video preview unavailable");
    painter.end();
  } else {
    if (image.format() != QImage::Format_ARGB32 && image.format() != QImage::Format_RGB32) {
      image = image.convertToFormat(QImage::Format_ARGB32);
    }
    for (int y = 0; y < image.height(); ++y) {
      QRgb *row = reinterpret_cast<QRgb *>(image.scanLine(y));
      for (int x = 0; x < image.width(); ++x) {
        row[x] = apply_curves_to_color(prepared_curves_, QColor::fromRgb(row[x])).rgb();
      }
    }
  }

  QImage canvas(canvas_size, QImage::Format_ARGB32_Premultiplied);
  canvas.fill(QColor(24, 24, 24));

  QPainter painter(&canvas);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
  const QSize fitted = image.size().scaled(canvas_size, Qt::KeepAspectRatio);
  const QRect target_rect((canvas_size.width() - fitted.width()) / 2, (canvas_size.height() - fitted.height()) / 2,
                          fitted.width(), fitted.height());
  painter.drawImage(target_rect, image);
  painter.end();

  QPixmap pixmap = QPixmap::fromImage(canvas);
  pixmap.setDevicePixelRatio(1.0);
  preview_label_->setPixmap(pixmap);
  rendered_preview_revision_ = curves_revision_;
  rendered_preview_source_key_ = preview_source_cache_key_;
}

void CurveEditorDialog::apply_curves(const std::array<rgb_curves::CurvePoints, 4> &curves)
{
  curves_ = curves;
  invalidate_curve_cache();
  curve_widget_->set_curve(curves_[static_cast<size_t>(active_channel_)]);
  update_preview();
  emit curvesChanged(curves_);
}

void CurveEditorDialog::invalidate_curve_cache()
{
  for (size_t i = 0; i < curves_.size(); ++i) {
    prepared_curves_[i] = rgb_curves::prepare_curve(curves_[i]);
  }
  ++curves_revision_;
}

void CurveEditorDialog::set_preview_source_image(const QImage &image)
{
  preview_source_image_ = image;
  preview_source_cache_key_ = image.isNull() ? 0 : image.cacheKey();
}
