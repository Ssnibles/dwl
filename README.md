# Ssnibles's DWL (dwl - dwm for Wayland)

Personal fork of [dwl](https://codeberg.org/dwl/dwl), customized and packaged as a Nix Flake.

---

## 🚀 Features & Customizations

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

To sync your local repo to GitHub:

```bash
cd ~/dwl
git remote set-url origin git@github.com:Ssnibles/dwl.git
git push -u origin main
```

---

## 📄 License
Licensed under the GPL-3.0 License (inherited from upstream dwl).
