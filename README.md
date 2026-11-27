# Meshtastic Device UI

Shared LVGL user interface library for Meshtastic radios and standalone display clients.

[Meshtastic website](https://meshtastic.org/) · [Documentation](https://meshtastic.org/docs/) · [Firmware repository](https://github.com/meshtastic/firmware)

## Overview

device-ui is a C++20 UI library used by Meshtastic firmware and standalone clients. It provides reusable display, view, input, and navigation components for supported TFT devices, with Linux/Portduino support for host builds and applications.

The library uses LVGL 9 and LovyanGFX. Its views cover common Meshtastic screens such as messages, nodes, channels, telemetry, maps, and device settings. It also includes localization, persistent storage, and serial and TCP client interfaces.

<div align="center">
  <img src="docs/T-Deck.jpg" alt="Meshtastic running on a T-Deck" width="220">
  <img src="docs/Tracker_L2.png" alt="Meshtastic running on a Tracker L2" width="220">
  <img src="docs/Pi400-TFT.jpg" alt="Meshtastic running on a Raspberry Pi with a TFT" width="220">
</div>

## Platforms and devices

The library is designed for Arduino and FreeRTOS targets, including ESP32, nRF52, and RP2040 boards, as well as Linux/Portduino hosts.

Examples of integrated devices include the LILYGO T-Deck and T-Deck Plus, Seeed SenseCAP Indicator and Wio Tracker L2, Heltec V4 TFT, Elecrow ThinkNode M9 and CrowPanel Advance S3, unPhone, and PICOmputer S3. Standalone clients include devices such as the LilyGo T-HMI, WT32-SC01, and Replicator.

The exact device and panel support depends on the consuming firmware target and its display-driver configuration. See [Adding a TFT device](AGENTS.md#adding-a-tft-device) for the integration pattern.

## Build and test

A native CMake build is available for host-side development and tests. It uses C++20 and fetches several dependencies during configuration, so the first configure needs network access.

```sh
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

When this repository is configured as the top-level CMake project, doctests are enabled by default. These tests run on the host and do not replace building or testing the relevant hardware target.

## Firmware integration

This repository provides the UI library; firmware build environments and board definitions live in the consuming firmware repository. Follow the [firmware build instructions](https://meshtastic.org/docs/development/firmware/build/) and select the appropriate device target there.

Keep panel, touch, and pin configuration with the firmware target. The `VIEW_*` build definition selects a UI layout independently from the physical display resolution. For embedded targets, define `DISPLAY_SET_RESOLUTION` when the LVGL display should use the resolution reported by the display driver. Portduino reads panel settings from the host's YAML configuration.

The native CMake build selects the `ui_320x240` generated view. CMake uses configure-time source globs, so rerun `cmake -S . -B build` after adding or removing source files.

## Linux input configuration

Linux clients can select libinput devices and configure keyboard navigation, cursor behavior, and the main navigation panel's scrollbar through `DisplayDriverConfig`:

```cpp
DisplayDriverConfig displayConfig(
    DisplayDriverConfig::device_t::FB, width, height);

displayConfig
    .input(DisplayDriverConfig::input_config_t{
        .keyboardDevice = keyboardPath,
        .pointerDevice = pointerPath,
        .keyboardNavigation = true,
        .cursorMode = DisplayDriverConfig::cursor_mode_t::AUTO,
    })
    .navigation(DisplayDriverConfig::navigation_config_t{
        .scrollbarMode = DisplayDriverConfig::scrollbar_mode_t::AUTO,
    });
```

- `keyboardNavigation` defaults to `true` and uses context-aware arrow-key navigation. Set it to `false` to forward raw LVGL keyboard events.
- `cursorMode` accepts `AUTO`, `ALWAYS`, or `NEVER`. Absolute touch devices do not show a cursor; `NEVER` disables it.
- `scrollbarMode` accepts `AUTO`, `ON`, or `OFF`. Keyboard focus can still scroll the panel when the scrollbar is hidden.

These options apply to the configured Linux input devices. An X11 window driver's own input and cursor behavior is unchanged.

## Architecture

The library separates public interfaces, implementations, platform support, and generated views:

- `include/` — public interfaces and headers
- `source/` — UI, graphics, input, communication, storage, and utility implementations
- `src/` — Meshtastic protobuf and constants integration
- `portduino/` — Linux/Portduino support
- `drivers/` — device-specific support
- `generated/` — generated LVGL and EEZ Studio view sources
- `tests/` — native test cases

[View the class diagram](docs/class-diagram.png).

Do not edit files under `generated/` by hand. Update the corresponding UI authoring source and regenerate the affected view. See [AGENTS.md](AGENTS.md) for repository build and device-integration details.

## License

See [LICENSE](LICENSE) for the license terms.
