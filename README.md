# Cask: Nix packaging

This branch contains only the Nix files: `flake.nix`, `nix/package.nix` (derivation), `nix/program.nix`
(NixOS module `programs.cask`) and, for the declarative setup of runners, components and prefixes,
`nix/declarative.nix` with `nix/cask-sync.sh`. The program itself lives in the `main` branch and is fetched from there
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

**Release or any commit.** By default the flake follows the sources it is locked to. After every release the
Build workflow on `main` points this branch at the new tag, so the default is the latest release. To choose
another version, override the input in your flake:

```nix
inputs.cask = {
  url = "github:ndiruhniu/cask/nixos";
  inputs.cask-src.url = "github:ndiruhniu/cask/v1.2.3";   # a tag, or a commit hash, or "main"
  inputs.nixpkgs.follows = "nixpkgs";                      # optional: use your own nixpkgs
};
```

"About" shows the release number when the sources are exactly the release commit, otherwise the short
commit hash.

**Without flakes:** copy `nix/package.nix`, `nix/program.nix`, `nix/declarative.nix` and `nix/cask-sync.sh` into one folder and add `imports = [ ./program.nix ];`. The sources are fetched from `main` automatically
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
| `user` | The user whose Cask folder the options below set up (required when any of them is used). |
| `runners.<name>` | Wine builds to download: `url`, `hash`, and `subdir` / `stripRoot` / `extension` if needed. |
| `components.<dxvk,d7vk,vkd3d-proton>.<version>` | DXVK, D7VK, VKD3D-Proton versions to download: `url`, `hash`. |
| `prefixes.<name>` | Prefixes to have: `runner`, `arch`, `components.<type> = "<version>"`, `winetricks = [ verbs ]`, `env = { VAR = "value"; }`, `dllOverrides = { dll = "native,builtin"; }`. |
| `xvfb` | Run the setup on a virtual X server (default true). |

Development: inside a checkout of `main`, `nix develop github:ndiruhniu/cask/nixos` gives a shell with the build tools and `CASK_ROOT=$PWD/.cask-dev`;
`nix build` / `nix run` build and start the package.

Note: keep "Integrate with the desktop" in Cask's settings **off** on NixOS; the package ships its own
desktop entry and the module sets the MIME defaults.

## Declarative runners, components and prefixes

```nix
programs.cask = {
  enable = true;
  user = "alice";
  fhs = true;                              # downloaded Wine builds need it
  dataDir = "$HOME/Games/Cask";            # prefixes go to <dataDir>/prefixes

  runners."wine-ge-8-26" = {
    url = "https://github.com/GloriousEggroll/wine-ge-custom/releases/download/GE-Proton8-26/wine-lutris-GE-Proton8-26-x86_64.tar.xz";
    hash = "sha256-...";
  };
  components.dxvk."dxvk-2.7.1" = {
    url = "https://github.com/doitsujin/dxvk/releases/download/v2.7.1/dxvk-2.7.1.tar.gz";
    hash = "sha256-...";
  };
  prefixes.games = {
    runner = "wine-ge-8-26";               # or "system" (programs.cask.wine)
    arch = "win64";
    components.dxvk = "dxvk-2.7.1";
    winetricks = [ "corefonts" "vcrun2022" "win10" ];
    env.DXVK_HUD = "fps";                  # environment variables of the programs Cask starts
    dllOverrides.winmm = "builtin";        # Wine DLL overrides ("" disables the DLL)
  };
};
```

**Downloads** are done by Nix (`fetchzip`): the archive is checked against `hash`, unpacked into the store and linked
from `<dataDir>/data/runners/<name>` or `<dataDir>/data/<type>/<version>`. `hash` is the hash of the *unpacked*
archive: put `lib.fakeHash` there first, and the build error shows the right value. Proton builds keep `bin/wine` in
`files/`, so they need `subdir = "files"`.

**The setup** is the `cask-sync` systemd service. It runs as `programs.cask.user` on every `nixos-rebuild switch`
or `test` (also when the configuration did not change) and at boot; the rebuild waits for it, the output is in
`journalctl -u cask-sync` (`systemctl start cask-sync` runs it again by hand). It only does what differs from the
configuration:

- A runner or component that is linked already is not touched. A folder or link that Nix did not create is never
  replaced (a warning is printed); links of runners and components that are no longer declared are removed.
- A prefix that exists and is whole is not rebuilt. Whole means: a usable `cask.json`, non-empty registry files
  (`system.reg`, `user.reg`, `userdef.reg`) of the right architecture, `kernel32.dll` and `ntdll.dll` in `system32`,
  and `syswow64` for `win64`. What is installed in the prefix (games, saves, your own registry changes) is not
  checked. **A prefix that is not whole, or a folder with no usable `cask.json`, is skipped** with a warning in the
  journal (this is not an error of the run): nothing in it is changed, and the runners, components and the other
  prefixes are handled as usual. To have it created again, move it away or delete it. A missing prefix is created
  the way Cask does it (`wineboot --init`, `cask.json`).
  If `runner` differs from the one in `cask.json`, the prefix is switched like Cask does (`wineboot -u`). `arch` cannot
  be changed in place: the old prefix is renamed to `<name>.old-<arch>-<date>` (nothing is deleted) and a new one is
  created.
- A winetricks verb that is in the prefix's `winetricks.log` is not run again; the verbs run in the given order, and
  a failed one stops the rest of that prefix (the next run retries it). winetricks downloads its own files into
  `<dataDir>/data/winetricks/cache`; they are checked by winetricks, not by Nix.
- `env` goes to the environment variables of the prefix in `cask.json`, `dllOverrides` to the registry (after the
  components, so they win over the overrides of a component). Only what Nix put there is ever changed or removed;
  what you set in Cask yourself stays. The list of what Nix manages is `cask-nix-state.json` in the prefix.
- A component version is applied the way Cask applies it (DLLs copied with `*.cask-bak` backups, `native,builtin`
  overrides in the registry, the files recorded in `cask.json`); a changed version rolls the old one back first.
  Components that the configuration does not mention are left as they are, as are prefixes that are not listed.

Not covered: a `paths/data` or `paths/prefixes` that you set in Cask's own settings (the service uses the
`<dataDir>/data` and `<dataDir>/prefixes` defaults).
