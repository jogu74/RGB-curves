#include "screen_color_picker.hpp"

#import <AppKit/AppKit.h>

#include <memory>
#include <utility>

namespace colorforge {

bool is_screen_color_picker_supported()
{
  if (@available(macOS 10.15, *)) {
    return true;
  }

  return false;
}

void pick_screen_color(QWidget *parent, ScreenColorPickedCallback on_picked, ScreenColorCanceledCallback on_canceled)
{
  (void)parent;

  if (!is_screen_color_picker_supported()) {
    if (on_canceled) {
      on_canceled();
    }
    return;
  }

  auto *picked_callback = new ScreenColorPickedCallback(std::move(on_picked));
  auto *canceled_callback = new ScreenColorCanceledCallback(std::move(on_canceled));

  __block NSColorSampler *sampler = [[NSColorSampler alloc] init];
  [sampler showSamplerWithSelectionHandler:^(NSColor *_Nullable selectedColor) {
    std::unique_ptr<ScreenColorPickedCallback> picked_guard(picked_callback);
    std::unique_ptr<ScreenColorCanceledCallback> canceled_guard(canceled_callback);

    if (selectedColor != nil) {
      NSColor *srgb_color = [selectedColor colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
      if (srgb_color == nil) {
        srgb_color = selectedColor;
      }

      QColor color;
      color.setRgbF([srgb_color redComponent], [srgb_color greenComponent], [srgb_color blueComponent], 1.0);

      if (*picked_guard) {
        (*picked_guard)(color);
      }
    } else if (*canceled_guard) {
      (*canceled_guard)();
    }

    [sampler release];
    sampler = nil;
  }];
}

} // namespace colorforge
