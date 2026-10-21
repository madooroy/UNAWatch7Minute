# Third-party notices

- **UNA Watch SDK** (MIT, (c) 2026 UNA Watch Ltd) - https://github.com/UNAWatch/una-sdk.
  The app builds against it; `Software/Apps/LVGL-GUI/simulator/main.cpp`,
  `ConfigurationSimulator.hpp`, `Software/Libs/libs.cmake` and the CMake
  structure are adapted from the SDK's tutorials.
- **Poppins** font (SIL Open Font License 1.1, (c) Indian Type Foundry) -
  `Software/Apps/LVGL-GUI/assets/ttf/`, as shipped with the UNA SDK. The
  generated `assets/fonts/*.c` files are derived from it.
- **LVGL** (MIT) - used through the SDK's `ThirdParty/lvgl` submodule.
