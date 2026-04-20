#include "screen_color_picker.hpp"

namespace colorforge {

bool is_screen_color_picker_supported()
{
  return false;
}

void pick_screen_color(QWidget *parent, ScreenColorPickedCallback on_picked, ScreenColorCanceledCallback on_canceled)
{
  (void)parent;
  (void)on_picked;

  if (on_canceled) {
    on_canceled();
  }
}

} // namespace colorforge
