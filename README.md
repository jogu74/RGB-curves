# OBS RGB Curves

Ett OBS-filter som exponerar fyra punktbaserade kurvor:

- `Neutral`: påverkar luminans/ljusnivåer gemensamt.
- `Red`, `Green`, `Blue`: justerar respektive färgkanal separat.

I stället för sliders öppnas en egen editor där du:

- klickar i grafen för att skapa punkter
- drar punkter för att forma kurvan
- dubbelklickar eller högerklickar för att ta bort en punkt

## Arkitektur

- `src/rgb_curves_filter.*`: OBS-filter, inställningar och LUT-generering.
- `src/curve_editor_dialog.*`: enkel Qt-dialog för kanalval.
- `src/curve_widget.*`: interaktiv canvas för kurvredigering.
- `data/effects/rgb-curves.effect`: shader som applicerar neutral kurva + RGB-kurvor.

## Bygga

Projektet förutsätter att du har:

- OBS Studio SDK/libobs headers
- Qt 6 Widgets
- CMake 3.22+

Sätt miljövariabler innan konfigurering om CMake inte hittar libobs automatiskt:

```powershell
$env:OBS_INCLUDE_DIR="C:\path\to\obs-studio\libobs"
$env:OBS_LIB_DIR="C:\path\to\obs-studio\build\rundir\RelWithDebInfo\obs-plugins\64bit"
cmake -S . -B build
cmake --build build --config RelWithDebInfo
```

Efter build kopieras shaderfilen till pluginens `data/obs-rgb-curves/effects/`-katalog.

## Nästa steg

- koppla in lokaliseringssträngar
- lägga till presets/import/export
- verifiera mot exakt OBS-SDK på maskinen och justera CMake-länkning vid behov
