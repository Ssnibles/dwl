{
  description = "Custom dwl Wayland compositor build for Ssnibles";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      supportedSystems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    in {
      packages = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in {
          default = pkgs.stdenv.mkDerivation {
            pname = "dwl";
            version = "0.8-dev";
            src = ./.;

            nativeBuildInputs = with pkgs; [
              pkg-config
              wayland-scanner
            ];

            buildInputs = with pkgs; [
              wayland
              wlroots_0_19
              scenefx
              libGL
              wayland-protocols
              libinput
              libxkbcommon
              pixman
              xcbutilwm
            ];

            makeFlags = [
              "PREFIX=$(out)"
              "DATADIR=$(out)/share"
              "MANDIR=$(out)/share/man"
            ];

            meta = {
              mainProgram = "dwl";
            };
          };
        }
      );

      apps = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          applyScript = pkgs.writeShellApplication {
            name = "dwl-apply";
            runtimeInputs = with pkgs; [ git coreutils gawk ];
            text = ''
              set -euo pipefail
              NIXCONFIG_DIR="/home/josh/NixConfig"
              DWL_DIR="$(pwd)"
              HOST="''${NIXOS_HOST:-$(hostname)}"

              echo "==> Staging NixOS config changes in $NIXCONFIG_DIR..."
              git -C "$NIXCONFIG_DIR" add -A

              echo "==> Rebuilding NixOS host '$HOST' with local dwl override..."
              sudo nixos-rebuild switch --flake "$NIXCONFIG_DIR#$HOST" --override-input dwl "$DWL_DIR"

              GEN=$(nixos-rebuild list-generations 2>/dev/null | awk 'NR==2 {print $1}' || echo "unknown")
              echo ""
              echo "✔ NixOS rebuild completed successfully! (Generation $GEN)"
              echo "--> DWL changes applied! Exit your current session and log back in to use the new build."
            '';
          };
        in {
          default = {
            type = "app";
            program = "${applyScript}/bin/dwl-apply";
          };
          apply = {
            type = "app";
            program = "${applyScript}/bin/dwl-apply";
          };
        }
      );

      devShells = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
        in {
          default = pkgs.mkShell {
            inputsFrom = [ self.packages.${system}.default ];
          };
        }
      );
    };
}


