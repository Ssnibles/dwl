# dwl - dwm for Wayland

A fork of [dwl](https://codeberg.org/dwl/dwl) packaged as a Nix Flake. This is mainly just an experiment and trying to learn wlroots and wayland.

---

## Features

- **Layouts**: Dwindle (`[\\]`), Tile (`[]=`), Monocle (`[M]`), Columns (`|||`), and Floating (`><>`).
- **Navigation & Resizing**: Focus/move windows via `Mod + H/J/K/L` or Arrow keys; adjust master (`mfact`) and client (`cfact`) ratios.
- **Scratchpad**: Toggle a persistent floating window overlay (`Mod + ~` or `Mod + P`).
- **Overview**: Window grid view with quick-jump keys (`Mod + O`).
- **Visuals**: Tokyo Night colours (`0x1a1b26`), rounded corners (`8px`), and configurable gaps (`8px`) via SceneFX.
- **Shortcuts**: Audio/mic (`wpctl`), brightness (`brightnessctl`), media (`playerctl`), screenshots and OCR (`grim`, `slurp`, `tesseract`).
- **Nix Support**: Builds on `x86_64-linux` and `aarch64-linux` with a `dwl-apply` script for local updates.

---

## Keybindings

Default modifier: `MODKEY` (`Super` / `Logo`).

### Launchers

| Keybinding      | Action         | Command                   |
| :-------------- | :------------- | :------------------------ |
| `Mod + Return`  | Terminal       | `foot`                    |
| `Mod + Space`   | App Launcher   | `vicinae toggle`          |
| `Mod + D`       | Command Centre | Quickshell Command Centre |
| `Mod + Alt + L` | Lock Screen    | Quickshell Lockscreen     |
| `Mod + E`       | File Manager   | `foot -e yazi`            |

### Window Management & Layouts

| Keybinding            | Action                         |
| :-------------------- | :----------------------------- |
| `Mod + Q`             | Close window                   |
| `Mod + F`             | Toggle fullscreen              |
| `Mod + V`             | Toggle floating                |
| `Mod + Shift + R`     | Quit dwl                       |
| `Mod + O`             | Toggle Overview                |
| `Mod + R`             | Set layout: Dwindle (`[\\]`)   |
| `Mod + T`             | Set layout: Tile (`[]=`)       |
| `Mod + Shift + Space` | Set layout: Floating (`><>`)   |
| `Mod + M`             | Set layout: Monocle (`[M]`)    |
| `Mod + C`             | Set layout: Columns (`\|\|\|`) |

### Focus & Navigation

| Keybinding              | Action              |
| :---------------------- | :------------------ |
| `Mod + H / Left`        | Focus left          |
| `Mod + L / Right`       | Focus right         |
| `Mod + K / Up`          | Focus up            |
| `Mod + J / Down`        | Focus down          |
| `Mod + Alt + H / Left`  | Focus monitor left  |
| `Mod + Alt + L / Right` | Focus monitor right |
| `Mod + Alt + K / Up`    | Focus monitor up    |
| `Mod + Alt + J / Down`  | Focus monitor down  |

### Sizing & Window Placement

| Keybinding                           | Action                                     |
| :----------------------------------- | :----------------------------------------- |
| `Mod + Shift + H / J / K / L`        | Move window in stack                       |
| `Mod + Ctrl + H`                     | Shrink master area (`mfact -0.05`)         |
| `Mod + Ctrl + L`                     | Grow master area (`mfact +0.05`)           |
| `Mod + Ctrl + J`                     | Shrink window height (`cfact -0.25`)       |
| `Mod + Ctrl + K`                     | Grow window height (`cfact +0.25`)         |
| `Mod + Ctrl + -`                     | Reset window heights (`cfact`)             |
| `Mod + Ctrl + =`                     | Reset master area size (`mfact`)           |
| `Mod + Shift + Return`               | Move window to master area                 |
| `Mod + Shift + =`                    | Add window to master area                  |
| `Mod + Shift + -`                    | Remove window from master area             |
| `Mod + Shift + Ctrl + H / J / K / L` | Move window to monitor in target direction |

### Workspaces & Scratchpad

| Keybinding                             | Action                             |
| :------------------------------------- | :--------------------------------- |
| `Mod + 1..9`                           | Go to workspace 1..9               |
| `Mod + Shift + 1..9`                   | Move window to workspace 1..9      |
| `Mod + ~` or `Mod + P`                 | Toggle scratchpad                  |
| `Mod + Shift + ~` or `Mod + Shift + P` | Move window into/out of scratchpad |

### Media & Hardware

| Keybinding                   | Action                           | Command                      |
| :--------------------------- | :------------------------------- | :--------------------------- |
| `Print` or `Mod + Shift + S` | Copy and save screenshot snippet | `grim` + `slurp` + `wl-copy` |
| `Mod + Ctrl + S`             | Copy text from screenshot (OCR)  | `tesseract` + `wl-copy`      |
| `XF86AudioRaiseVolume`       | Volume +5%                       | `wpctl`                      |
| `XF86AudioLowerVolume`       | Volume -5%                       | `wpctl`                      |
| `XF86AudioMute`              | Mute/unmute audio                | `wpctl`                      |
| `XF86AudioMicMute`           | Mute/unmute mic                  | `wpctl`                      |
| `XF86AudioPlay`              | Play / Pause                     | `playerctl`                  |
| `XF86AudioNext`              | Next track                       | `playerctl`                  |
| `XF86AudioPrev`              | Previous track                   | `playerctl`                  |
| `XF86AudioStop`              | Stop media                       | `playerctl`                  |
| `XF86MonBrightnessUp`        | Brightness +5%                   | `brightnessctl`              |
| `XF86MonBrightnessDown`      | Brightness -5%                   | `brightnessctl`              |

---

## Nix Flake Setup

### 1. Add to `flake.nix`

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
    # Use inputs.dwl.packages.${pkgs.system}.default
  };
}

```

### 2. Local Testing & Rebuilding

Test local changes in `~/dwl` without committing:

```bash
# Build and apply to NixOS:
make apply
# Or:
nix run .#apply
# Or via nixos-rebuild:
sudo nixos-rebuild switch --flake ~/NixConfig#desktop --override-input dwl path:~/dwl

# Build package only:
nix build

```

---

## Manual Build & Installation

### Dependencies

Requires standard build tools (`make`, `pkg-config`, `gcc` or `clang`) and header files for:

`wayland`, `wayland-protocols`, `wlroots` (0.19.x), `scenefx` (0.4.x), `libinput`, `libxkbcommon`, `pixman`, `xcbutilwm`.

### Build & Install

```bash
make
sudo make install

```

---

## References

- [The Wayland Book](https://wayland-book.com/)
- [Wayland Explorer](https://wayland.app)
- [wlroots repository](https://gitlab.freedesktop.org/wlroots/wlroots)
- [dwl repository](https://codeberg.org/dwl/dwl)

---

## Licence

GPL-3.0 Licence.
