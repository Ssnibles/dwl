# Ssnibles's DWL (dwl - dwm for Wayland)

Personal fork of [dwl](https://codeberg.org/dwl/dwl), customised and packaged as a Nix Flake.

---

## Features & Customisations

- **Keybindings**: Custom Vim-style navigation (`H/J/K/L`), window movement (`Shift+H/J/K/L`), and resizing (`Ctrl+H/J/K/L`).
- **Tagging**: Configured with 10 workspace tags (`1-9` and `0` for Tag 10).
- **Trackpad Integration**: Tap-to-click, natural scrolling, and 2-finger scroll enabled.
- **System Binds**: Hardware volume, brightness, media keys, and grim/slurp screenshot shortcuts.
- **Nix Flake Support**: Native `flake.nix` included for easy inclusion into NixOS / Home Manager configurations.

---

## 🛠️ Usage with Nix Flakes

### 1. Adding to your `flake.nix`

Add this repository as a flake input in your NixOS configuration:

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
    # Use inputs.dwl.packages.${pkgs.system}.default in your module
  };
}
```

---

## 🔄 Local Development & Override Workflow

When making local C changes inside `~/dwl`, you **do not** need to commit and push to GitHub every time to test your changes in NixOS!

### Testing Local Changes Directly in NixOS:

Run `nixos-rebuild` (or your build tool) with `--override-input dwl path:~/dwl`:

```bash
# Test local ~/dwl changes in your system build without pushing to GitHub:
sudo nixos-rebuild switch --flake ~/NixConfig#desktop --override-input dwl path:~/dwl

```

### Local Flake Build:

```bash
cd ~/dwl
nix build

```

---

## 📤 Pushing to GitHub

To synchronise your local repo with GitHub:

```bash
cd ~/dwl
git remote set-url origin git@github.com:Ssnibles/dwl.git
git push -u origin main

```

---

## 📚 Learning & Architecture Resources

If you are interested in Wayland compositors, windowing systems, and hacking on `dwl` / `wlroots`:

### Key Architectural Concepts
- **Window Geometry vs. Surface Buffers**: Wayland applications can render buffers larger than their visible window (e.g., client-side drop shadows or margins). The visible bounds are defined via `xdg_surface.set_window_geometry`. Subsurface tree clipping (`wlr_scene_subsurface_tree_set_clip`) ensures compositor borders remain crisp, uniform, and unaffected by client buffer overflow.
- **Scene Graph Trees (`wlr_scene` / SceneFX)**: `dwl` organizes visual elements into hierarchical scene trees (`LyrTile`, `LyrFloat`, `LyrFS`, etc.). Surface trees handle window content, while `wlr_scene_rect` nodes render background borders with corner rounding.
- **Tiling Mathematics**: Window layout is calculated deterministically in `tile()` by partitioning monitor coordinate boxes (`wlr_box`).

### Recommended Learning Resources
- **[The Wayland Book](https://wayland-book.com/)** by Drew DeVault – The definitive guide to the Wayland protocol, IPC architecture, objects, and event loops.
- **[Wayland Explorer (wayland.app)](https://wayland.app)** – Interactive reference for standard Wayland protocols (`xdg-shell`, `layer-shell`, `fractional-scale-v1`, etc.).
- **[wlroots & tinywl](https://gitlab.freedesktop.org/wlroots/wlroots/-/tree/master/tinywl)** – Minimal ~1,000 line C compositor demonstration for understanding compositor bootstrapping.
- **[dwl Upstream Repository](https://codeberg.org/dwl/dwl)** – Official source code, issue tracker, and community patches for `dwl`.

---

## 📄 Licence

Licenced under the GPL-3.0 Licence (inherited from upstream dwl).

