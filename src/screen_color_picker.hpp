#pragma once

#include <QColor>

#include <functional>

class QWidget;

namespace colorforge {

using ScreenColorPickedCallback = std::function<void(const QColor &)>;
using ScreenColorCanceledCallback = std::function<void()>;

bool is_screen_color_picker_supported();
void pick_screen_color(QWidget *parent, ScreenColorPickedCallback on_picked,
                       ScreenColorCanceledCallback on_canceled = {});

} // namespace colorforge
