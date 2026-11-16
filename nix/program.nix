# NixOS module for Cask: `programs.cask`.
#
# Everything here works on the Nix/NixOS level (package arguments, PATH, MIME defaults, system
# options); Cask's own sources and its settings format are not involved.
#
# Use it through the flake:    imports = [ inputs.cask.nixosModules.default ];
# or copy `package.nix` and `program.nix` next to your configuration and import ./program.nix.
{
  config,
  lib,
  pkgs,
  ...
}:

let
  cfg = config.programs.cask;

  # The package with the options below applied (they are plain arguments of package.nix).
  configured = cfg.package.override (
    {
      extraPackages = cfg.extraPackages;
    }
    // lib.optionalAttrs (cfg.dataDir != null) { dataDir = cfg.dataDir; }
    // lib.optionalAttrs (cfg.wine != null) { wine = cfg.wine; }
  );

  finalPackage = if cfg.fhs then configured.fhs else configured;
in
{
  options.programs.cask = {
    enable = lib.mkEnableOption "Cask, a Wine prefix manager";

    package = lib.mkOption {
      type = lib.types.package;
      default = pkgs.callPackage ./package.nix { };
      defaultText = lib.literalExpression "pkgs.callPackage ./package.nix { }";
      description = ''
        The Cask package. It must accept the arguments of `package.nix`
        (`dataDir`, `wine`, `extraPackages`) and provide the `fhs` attribute.
      '';
    };

    dataDir = lib.mkOption {
      type = lib.types.nullOr lib.types.str;
      default = null;
      example = "$HOME/Games/Cask";
      description = ''
        Directory where Cask keeps everything: its settings, Wine builds, DXVK/VKD3D-Proton packages,
        winetricks cache and the prefixes (Cask's `CASK_ROOT`). It is a shell expression evaluated
        when Cask starts, so `$HOME` works. The default is `$XDG_DATA_HOME/cask`
        (`~/.local/share/cask`). Setting the `CASK_ROOT` environment variable overrides it.
      '';
    };

    wine = lib.mkOption {
      type = lib.types.nullOr lib.types.package;
      default = null;
      example = lib.literalExpression "pkgs.wineWowPackages.staging";
      description = ''
        A Wine package to offer in Cask as "System Wine". It is put in front of Cask's `PATH`, so Wine
        builds from nixpkgs work without any manual setup. Wine builds that you download into Cask's
        `data/runners` need {option}`programs.cask.fhs` instead.
      '';
    };

    extraPackages = lib.mkOption {
      type = lib.types.listOf lib.types.package;
      default = [ ];
      example = lib.literalExpression "[ pkgs.zenity ]";
      description = ''
        Additional programs available to Cask and to everything it starts. Winetricks, cabextract,
        unzip, p7zip, curl, GameMode and MangoHud are always included.
      '';
    };

    fhs = lib.mkOption {
      type = lib.types.bool;
      default = false;
      description = ''
        Run Cask inside an FHS environment (like `steam-run`). Enable this to use Wine/Proton builds
        that you download and place into Cask's `data/runners`: generic Linux binaries do not run
        on NixOS otherwise. Not needed when {option}`programs.cask.wine` is used.
      '';
    };

    gamemode = lib.mkOption {
      type = lib.types.bool;
      default = false;
      description = ''
        Enable the GameMode daemon ({option}`programs.gamemode.enable`). Cask's "GameMode" option
        starts programs through `gamemoderun`, which needs the daemon.
      '';
    };

    graphics32 = lib.mkOption {
      type = lib.types.bool;
      default = true;
      description = ''
        Enable 32-bit graphics driver support ({option}`hardware.graphics.enable32Bit`, NixOS 24.11 or
        newer). Needed by 32-bit Windows programs under Wine/DXVK.
      '';
    };

    defaultForWindowsFiles = lib.mkOption {
      type = lib.types.bool;
      default = true;
      description = ''
        Make Cask the default application for `.exe`, `.msi`, `.bat` and `.lnk` files
        ({option}`xdg.mime.defaultApplications`), so a double click in the file manager opens its prefix
        picker. This replaces Cask's own "Integrate with the desktop" setting, which should stay off on
        NixOS.
      '';
    };
  };

  config = lib.mkIf cfg.enable {
    environment.systemPackages = [ finalPackage ];

    xdg.mime.defaultApplications = lib.mkIf cfg.defaultForWindowsFiles (
      lib.genAttrs cfg.package.mimeTypes (_: lib.mkDefault "cask.desktop")
    );

    programs.gamemode.enable = lib.mkIf cfg.gamemode true;

    hardware.graphics = lib.mkIf cfg.graphics32 {
      enable = lib.mkDefault true;
      enable32Bit = lib.mkDefault true;
    };
  };
}
