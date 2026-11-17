# Cask: Nix packaging

This branch contains only the Nix files: `flake.nix`, `nix/package.nix` (derivation) and `nix/program.nix`
(NixOS module `programs.cask`). The program itself lives in the `main` branch and is fetched from there
(flake input `cask-src`; run `nix flake update cask-src` to move to a newer `main`).

The Nix store is read-only, so the package installs a small launcher that sets `CASK_ROOT`
(default `~/.local/share/cask`) and starts the Qt-wrapped binary. Runners, DXVK etc. go into
`<dataDir>/data/...` as usual.

**With flakes:**

```nix
{
  inputs.cask.url = "github:ndiruhniu/cask/nixos";
  outputs = { nixpkgs, cask, ... }: {
    nixosConfigurations.host = nixpkgs.lib.nixosSystem {
      modules = [ cask.nixosModules.default { programs.cask.enable = true; } ];
    };
  };
}
```

**Without flakes:** copy `nix/package.nix` and `nix/program.nix` and add `imports = [ ./program.nix ];`. The sources are fetched from `main` automatically
(override with `caskSrc` if you want your own copy).

| Option | Meaning |
|---|---|
| `enable` | Install Cask. |
| `package` | The package (default: `pkgs.callPackage ./package.nix { }`). |
| `dataDir` | Shell expression for the Cask root (default `${XDG_DATA_HOME:-$HOME/.local/share}/cask`). |
| `wine` | Optional Nix Wine package put into `PATH` of Cask. |
| `extraPackages` | Extra tools for `PATH`. |
| `fhs` | Run Cask in an FHS environment, needed for downloaded Wine/Proton builds. |
| `gamemode` | Enable `programs.gamemode`. |
| `graphics32` | Enable 32-bit graphics support (default true). |
| `defaultForWindowsFiles` | Make Cask the default for `.exe`, `.msi`, `.bat`, `.lnk`. |

Development: inside a checkout of `main`, `nix develop github:ndiruhniu/cask/nixos` gives a shell with the build tools and `CASK_ROOT=$PWD/.cask-dev`;
`nix build` / `nix run` build and start the package.

Note: keep "Integrate with the desktop" in Cask's settings **off** on NixOS; the package ships its own
desktop entry and the module sets the MIME defaults.
