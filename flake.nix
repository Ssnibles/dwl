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
