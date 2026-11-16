{
  description = "Cask: a Wine prefix manager written in C++/Qt";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs =
    { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});
    in
    {
      # nix build, nix run
      packages = forAllSystems (pkgs: rec {
        cask = pkgs.callPackage ./nix/package.nix { };
        cask-fhs = cask.fhs;
        default = cask;
      });

      apps = forAllSystems (pkgs: {
        default = {
          type = "app";
          program = nixpkgs.lib.getExe self.packages.${pkgs.stdenv.hostPlatform.system}.cask;
        };
      });

      # nix develop: everything to build, debug and run Cask from a checkout.
      #   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build && build/Cask/cask
      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          inputsFrom = [ self.packages.${pkgs.stdenv.hostPlatform.system}.cask ];
          packages = with pkgs; [
            gdb
            valgrind
            clang-tools
            python3
            winetricks
            cabextract
            unzip
          ];
          shellHook = ''
            # Keep a development run away from your real Cask folder.
            export CASK_ROOT="$PWD/.cask-dev"
            echo "Cask dev shell. Build: cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build"
            echo "Run: build/Cask/cask   (data in \$CASK_ROOT = $CASK_ROOT)"
          '';
        };
      });

      # For other flakes: `inputs.cask.overlays.default` gives `pkgs.cask`.
      overlays.default = final: _prev: {
        cask = final.callPackage ./nix/package.nix { };
      };

      # For NixOS configurations: `imports = [ inputs.cask.nixosModules.default ];`
      nixosModules.default =
        { lib, pkgs, ... }:
        {
          imports = [ ./nix/program.nix ];
          programs.cask.package = lib.mkDefault self.packages.${pkgs.stdenv.hostPlatform.system}.cask;
        };
      nixosModules.cask = self.nixosModules.default;
    };
}
