# dwl - dwm for Wayland

Personal fork of [dwl](https://codeberg.org/dwl/dwl), customized and packaged as a Nix Flake.

---

## Overview

This repository contains a customized build of `dwl` (dwm for Wayland), extending the minimalist Wayland compositor with SceneFX visual enhancements, advanced layout algorithms, spatial window management, and native Nix Flake packaging.

---

## Features

- **Custom Layout Engine**:
  - **Dwindle (`[\\]`)**: Fibonacci recursive box splitting layout.
  - **Tile (`[]=`)**: Classic master and stack tiling layout.
  - **Monocle (`[M]`)**: Fullscreen single-window focus.
  - **Columns (`|||`)**: Equal vertical column layout.
  - **Floating (`><>`)**: Manual free-floating window positioning.

- **Vim Spatial Navigation & Control**:
  - Intuitive directional focus with `Mod + H/J/K/L` or Arrow keys.
  - Directional stack shifting with `Mod + Shift + H/J/K/L`.
  - Master ratio (`mfact`) and client height ratio (`cfact`) dynamic resizing.

- **Scratchpad Overlay Workspace**:
  - Persistent overlay scratchpad workspace with distinct background opacity (`Mod + ~` or `Mod + P`).
  - Seamless toggle and client assignment across displays.

- **Overview Mode**:
  - Interactive grid overview of active windows with keyboard jump labels (`Mod + O`).

- **Tokyo Night Aesthetic & SceneFX Integration**:
  - Curated Tokyo Night color palette (`0x1a1b26`).
  - SceneFX powered rounded corners (`8px`) and configurable window gaps (`8px`).
  - Distinct snap border and backdrop indicators.

- **Integrated Hardware & Utility Shortcuts**:
  - Native WirePlumber (`wpctl`) volume and microphone controls.
  - `brightnessctl` display backlight adjustment.
  - `playerctl` media playback control.
  - Screenshot capture via `grim` and `slurp` with built-in clipboard copying and OCR processing (`tesseract`).

- **Nix Flake Package & System Sync**:
  - Complete `flake.nix` supporting `x86_64-linux` and `aarch64-linux`.
  - Built-in `dwl-apply` app for zero-friction local NixOS system updates.

---

## Keybindings

The default modifier key is `MODKEY` (`Super` / `Logo`).

### Application Launchers

| Keybinding      | Action                  | Command / Function        |
| :-------------- | :---------------------- | :------------------------ |
| `Mod + Return`  | Launch Terminal         | `foot`                    |
| `Mod + Space`   | Launch Vicinae Launcher | `vicinae toggle`          |
| `Mod + D`       | Toggle Command Center   | Quickshell Command Center |
| `Mod + Alt + L` | Lock Screen             | Quickshell Lockscreen     |
| `Mod + E`       | Launch File Manager     | `foot -e yazi`            |

### Window Management & Layouts

| Keybinding            | Action                                   |
| :-------------------- | :--------------------------------------- | --- | --- | --- |
| `Mod + Q`             | Close focused window                     |
| `Mod + F`             | Toggle fullscreen state                  |
| `Mod + V`             | Toggle floating state for focused window |
| `Mod + Shift + R`     | Quit compositor                          |
| `Mod + O`             | Toggle Overview mode                     |
| `Mod + R`             | Set layout to Dwindle (`[\\]`)           |
| `Mod + T`             | Set layout to Tile (`[]=`)               |
| `Mod + Shift + Space` | Set layout to Floating (`><>`)           |
| `Mod + M`             | Set layout to Monocle (`[M]`)            |
| `Mod + C`             | Set layout to Columns (`                 |     |     | `)  |

### Focus & Spatial Navigation

| Keybinding              | Action                     |
| :---------------------- | :------------------------- |
| `Mod + H / Left`        | Focus window to the left   |
| `Mod + L / Right`       | Focus window to the right  |
| `Mod + K / Up`          | Focus window above         |
| `Mod + J / Down`        | Focus window below         |
| `Mod + Alt + H / Left`  | Focus monitor to the left  |
| `Mod + Alt + L / Right` | Focus monitor to the right |
| `Mod + Alt + K / Up`    | Focus monitor above        |
| `Mod + Alt + J / Down`  | Focus monitor below        |

### Stack & Resizing Controls

| Keybinding                           | Action                                        |
| :----------------------------------- | :-------------------------------------------- |
| `Mod + Shift + H / J / K / L`        | Move window position within stack             |
| `Mod + Ctrl + H`                     | Decrease master area factor (`mfact -0.05`)   |
| `Mod + Ctrl + L`                     | Increase master area factor (`mfact +0.05`)   |
| `Mod + Ctrl + J`                     | Decrease client height factor (`cfact -0.25`) |
| `Mod + Ctrl + K`                     | Increase client height factor (`cfact +0.25`) |
| `Mod + Ctrl + -`                     | Reset client height factor (`cfact`)          |
| `Mod + Ctrl + =`                     | Reset master area factor (`mfact`)            |
| `Mod + Shift + Return`               | Promote focused window to master area         |
| `Mod + Shift + =`                    | Increase number of windows in master area     |
| `Mod + Shift + -`                    | Decrease number of windows in master area     |
| `Mod + Shift + Ctrl + H / J / K / L` | Move window to monitor in target direction    |

### Workspace & Scratchpad

| Keybinding                             | Action                                     |
| :------------------------------------- | :----------------------------------------- |
| `Mod + 1..9`                           | Switch to workspace 1..9                   |
| `Mod + Shift + 1..9`                   | Move focused window to workspace 1..9      |
| `Mod + ~` or `Mod + P`                 | Toggle scratchpad overlay view             |
| `Mod + Shift + ~` or `Mod + Shift + P` | Move focused window into/out of scratchpad |

### System & Media Controls

| Keybinding                   | Action                              | Command / Utility            |
| :--------------------------- | :---------------------------------- | :--------------------------- |
| `Print` or `Mod + Shift + S` | Crop Screenshot to Clipboard & File | `grim` + `slurp` + `wl-copy` |
| `Mod + Ctrl + S`             | OCR Area Screenshot to Clipboard    | `tesseract` + `wl-copy`      |
| `XF86AudioRaiseVolume`       | Volume Up (+5%)                     | `wpctl`                      |
| `XF86AudioLowerVolume`       | Volume Down (-5%)                   | `wpctl`                      |
| `XF86AudioMute`              | Toggle Audio Mute                   | `wpctl`                      |
| `XF86AudioMicMute`           | Toggle Microphone Mute              | `wpctl`                      |
| `XF86AudioPlay`              | Media Play / Pause                  | `playerctl`                  |
| `XF86AudioNext`              | Media Next Track                    | `playerctl`                  |
| `XF86AudioPrev`              | Media Previous Track                | `playerctl`                  |
| `XF86AudioStop`              | Media Stop                          | `playerctl`                  |
| `XF86MonBrightnessUp`        | Display Brightness Up (+5%)         | `brightnessctl`              |
| `XF86MonBrightnessDown`      | Display Brightness Down (-5%)       | `brightnessctl`              |

---

## Usage with Nix Flakes

### 1. Adding to `flake.nix`

Add this repository as an input in your NixOS configuration:

```nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    dwl = {
      url = "github:Ssnibles/dwl";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, dwl, ... }: {
    # Reference inputs.dwl.packages.${pkgs.system}.default in your system or home-manager configuration
  };
}
```

### 2. Local Development & Live Override Workflow

When modifying C source code or layout rules locally in `~/dwl`, you do not need to commit and push changes to test them in NixOS.

#### Apply Local Changes to NixOS System:

You can use the built-in Nix flake application or Makefile shortcut:

```bash
# Via Makefile:
make apply

# Or via Nix directly:
nix run .#apply
```

Alternatively, invoke `nixos-rebuild` with `--override-input`:

```bash
sudo nixos-rebuild switch --flake ~/NixConfig#desktop --override-input dwl path:~/dwl
```

#### Build Local Flake Package:

```bash
cd ~/dwl
nix build
```

---

## Traditional Build & Installation

### Build Dependencies

Building `dwl` requires standard C compiler tools (`make`, `pkg-config`, `gcc` or `clang`) along with development packages for:

- `wayland` & `wayland-protocols`
- `wlroots` (0.19.x)
- `scenefx` (0.4.x)
- `libinput`
- `libxkbcommon`
- `pixman`
- `xcbutilwm`

### Compilation and Installation

```bash
# Build binary
make

# Install system-wide
sudo make install
```

---

## Remote Synchronisation

To update the remote repository:

```bash
cd ~/dwl
git remote set-url origin git@github.com:Ssnibles/dwl.git
git push -u origin main
```

---

## Architecture & Technical Concepts

- **Window Geometry vs. Surface Buffers**: Wayland clients can attach surface buffers larger than their visible window frame (for instance, client-side drop shadows or margins). The compositor extracts visible window boundaries using `xdg_surface.set_window_geometry`. Subsurface tree clipping (`wlr_scene_subsurface_tree_set_clip`) ensures borders remain crisp and unaffected by client buffer margins.
- **Scene Graph Architecture (`wlr_scene` / SceneFX)**: `dwl` structures renderable elements in hierarchical scene trees (`LyrTile`, `LyrFloat`, `LyrFS`). Window surfaces are attached to surface nodes, while SceneFX `wlr_scene_rect` nodes render backgrounds with configurable corner rounding and border highlights.
- **Deterministic Tiling Mathematics**: Layout algorithms calculate geometry iteratively across monitor coordinate bounds (`wlr_box`).

---

## Learning Resources

- **[The Wayland Book](https://wayland-book.com/)** - Comprehensive guide to the Wayland protocol architecture and client/compositor IPC.
- **[Wayland Explorer](https://wayland.app)** - Interactive documentation for core and staging Wayland protocols.
- **[wlroots & tinywl](https://gitlab.freedesktop.org/wlroots/wlroots/-/tree/master/tinywl)** - Minimal Wayland compositor baseline implementation.
- **[dwl Upstream Repository](https://codeberg.org/dwl/dwl)** - Official dwl codebase and upstream community patches.

---

## License

Distributed under the GPL-3.0 License (inherited from upstream dwl).
