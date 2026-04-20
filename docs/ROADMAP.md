# ColorForge Roadmap

`ColorForge` is the umbrella plugin for OBS color tools.

Current filter:

- `RGB Curves`
- `Hue Curves`
- `Color Range Correction`

Shared foundation goals:

- common plugin identity, packaging and installation
- reusable curve widgets and curve math
- reusable preview / histogram capture utilities
- a consistent preset and UI language across filters

Implementation order:

1. Rename the plugin from `RGB Curves` to `ColorForge` while keeping the existing filter stable.
2. Split shared infrastructure out of the current RGB Curves implementation as new filters need it.
3. Refine `Hue Curves` with better UI affordances, hue gradient guidance and tuning.
4. Expand `Color Range Correction` from keying into full secondary correction and refine controls.
