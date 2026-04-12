#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace rgb_curves {

struct CurvePoint {
  double x = 0.0;
  double y = 0.0;
};

using CurvePoints = std::vector<CurvePoint>;

struct PreparedCurve {
  CurvePoints points;
  std::vector<double> tangents;
};

enum class CurveChannel : int {
  Master = 0,
  Red = 1,
  Green = 2,
  Blue = 3,
};

inline CurvePoints default_curve()
{
  return {{0.0, 0.0}, {1.0, 1.0}};
}

inline std::array<CurvePoints, 4> default_curves()
{
  return {default_curve(), default_curve(), default_curve(), default_curve()};
}

inline double clamp01(double value)
{
  return std::clamp(value, 0.0, 1.0);
}

inline CurvePoints sanitize_curve(CurvePoints points)
{
  if (points.size() < 2) {
    return default_curve();
  }

  for (auto &point : points) {
    point.x = clamp01(point.x);
    point.y = clamp01(point.y);
  }

  std::sort(points.begin(), points.end(), [](const CurvePoint &lhs, const CurvePoint &rhs) {
    return lhs.x < rhs.x;
  });

  CurvePoints unique_points;
  unique_points.reserve(points.size());

  for (const auto &point : points) {
    if (!unique_points.empty() && std::abs(unique_points.back().x - point.x) < 1e-6) {
      unique_points.back().y = point.y;
      continue;
    }

    unique_points.push_back(point);
  }

  unique_points.front().y = clamp01(unique_points.front().y);
  unique_points.back().y = clamp01(unique_points.back().y);

  if (unique_points.size() < 2) {
    unique_points = default_curve();
  }

  return unique_points;
}

inline QString curves_to_json(const std::array<CurvePoints, 4> &curves)
{
  QJsonObject root;
  const std::array<const char *, 4> names = {"master", "red", "green", "blue"};

  for (size_t index = 0; index < curves.size(); ++index) {
    QJsonArray curve_array;
    for (const auto &point : curves[index]) {
      QJsonArray point_array;
      point_array.append(point.x);
      point_array.append(point.y);
      curve_array.append(point_array);
    }
    root.insert(names[index], curve_array);
  }

  return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

inline std::array<CurvePoints, 4> curves_from_json(const char *json)
{
  auto curves = default_curves();

  if (!json || !*json) {
    return curves;
  }

  const auto document = QJsonDocument::fromJson(QByteArray(json));
  if (!document.isObject()) {
    return curves;
  }

  const QJsonObject root = document.object();
  const std::array<const char *, 4> names = {"master", "red", "green", "blue"};

  for (size_t index = 0; index < curves.size(); ++index) {
    const auto value = root.value(names[index]);
    if (!value.isArray()) {
      continue;
    }

    CurvePoints points;
    const auto point_array = value.toArray();
    points.reserve(static_cast<size_t>(point_array.size()));

    for (const auto &entry : point_array) {
      if (!entry.isArray()) {
        continue;
      }

      const auto tuple = entry.toArray();
      if (tuple.size() != 2 || !tuple.at(0).isDouble() || !tuple.at(1).isDouble()) {
        continue;
      }

      points.push_back({tuple.at(0).toDouble(), tuple.at(1).toDouble()});
    }

    curves[index] = sanitize_curve(std::move(points));
  }

  return curves;
}

inline std::vector<double> monotone_tangents(const CurvePoints &points)
{
  const size_t count = points.size();
  std::vector<double> tangents(count, 0.0);

  if (count < 2) {
    return tangents;
  }

  std::vector<double> dx(count - 1, 0.0);
  std::vector<double> slopes(count - 1, 0.0);

  for (size_t i = 0; i + 1 < count; ++i) {
    dx[i] = std::max(points[i + 1].x - points[i].x, 1e-6);
    slopes[i] = (points[i + 1].y - points[i].y) / dx[i];
  }

  tangents[0] = slopes[0];
  tangents[count - 1] = slopes[count - 2];

  for (size_t i = 1; i + 1 < count; ++i) {
    if (slopes[i - 1] * slopes[i] <= 0.0) {
      tangents[i] = 0.0;
      continue;
    }

    const double w1 = 2.0 * dx[i] + dx[i - 1];
    const double w2 = dx[i] + 2.0 * dx[i - 1];
    tangents[i] = (w1 + w2) / ((w1 / slopes[i - 1]) + (w2 / slopes[i]));
  }

  return tangents;
}

inline PreparedCurve prepare_curve(const CurvePoints &input_points)
{
  PreparedCurve prepared;
  prepared.points = sanitize_curve(input_points);
  prepared.tangents = monotone_tangents(prepared.points);
  return prepared;
}

inline double sample_prepared_curve(const PreparedCurve &prepared, double x)
{
  const auto &points = prepared.points;
  if (points.size() < 2) {
    return clamp01(x);
  }

  x = clamp01(x);

  if (x <= points.front().x) {
    return clamp01(points.front().y);
  }

  if (x >= points.back().x) {
    return clamp01(points.back().y);
  }

  size_t segment = 0;
  while (segment + 1 < points.size() && points[segment + 1].x < x) {
    ++segment;
  }

  const auto &p0 = points[segment];
  const auto &p1 = points[segment + 1];
  const double span = std::max(p1.x - p0.x, 1e-6);
  const double t = (x - p0.x) / span;
  const double t2 = t * t;
  const double t3 = t2 * t;

  const double h00 = (2.0 * t3) - (3.0 * t2) + 1.0;
  const double h10 = t3 - (2.0 * t2) + t;
  const double h01 = (-2.0 * t3) + (3.0 * t2);
  const double h11 = t3 - t2;

  const double value = (h00 * p0.y) + (h10 * span * prepared.tangents[segment]) + (h01 * p1.y) +
                       (h11 * span * prepared.tangents[segment + 1]);

  return clamp01(value);
}

inline double sample_curve(const CurvePoints &input_points, double x)
{
  return sample_prepared_curve(prepare_curve(input_points), x);
}

inline std::array<uint8_t, 1024> build_lut_rgba(const std::array<CurvePoints, 4> &curves)
{
  std::array<PreparedCurve, 4> prepared = {
    prepare_curve(curves[0]), prepare_curve(curves[1]), prepare_curve(curves[2]), prepare_curve(curves[3])};
  std::array<uint8_t, 1024> lut {};

  for (int i = 0; i < 256; ++i) {
    const double x = static_cast<double>(i) / 255.0;
    lut[static_cast<size_t>(i) * 4 + 0] =
      static_cast<uint8_t>(std::lround(sample_prepared_curve(prepared[1], x) * 255.0));
    lut[static_cast<size_t>(i) * 4 + 1] =
      static_cast<uint8_t>(std::lround(sample_prepared_curve(prepared[2], x) * 255.0));
    lut[static_cast<size_t>(i) * 4 + 2] =
      static_cast<uint8_t>(std::lround(sample_prepared_curve(prepared[3], x) * 255.0));
    lut[static_cast<size_t>(i) * 4 + 3] =
      static_cast<uint8_t>(std::lround(sample_prepared_curve(prepared[0], x) * 255.0));
  }

  return lut;
}

inline std::array<uint8_t, 1024> build_lut_rgba(const std::array<PreparedCurve, 4> &prepared)
{
  std::array<uint8_t, 1024> lut {};

  for (int i = 0; i < 256; ++i) {
    const double x = static_cast<double>(i) / 255.0;
    lut[static_cast<size_t>(i) * 4 + 0] =
      static_cast<uint8_t>(std::lround(sample_prepared_curve(prepared[1], x) * 255.0));
    lut[static_cast<size_t>(i) * 4 + 1] =
      static_cast<uint8_t>(std::lround(sample_prepared_curve(prepared[2], x) * 255.0));
    lut[static_cast<size_t>(i) * 4 + 2] =
      static_cast<uint8_t>(std::lround(sample_prepared_curve(prepared[3], x) * 255.0));
    lut[static_cast<size_t>(i) * 4 + 3] =
      static_cast<uint8_t>(std::lround(sample_prepared_curve(prepared[0], x) * 255.0));
  }

  return lut;
}

inline std::string qstring_to_utf8(const QString &value)
{
  const QByteArray bytes = value.toUtf8();
  return {bytes.constData(), static_cast<size_t>(bytes.size())};
}

} // namespace rgb_curves
