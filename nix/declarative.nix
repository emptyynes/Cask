# Declarative Cask setup: `programs.cask.{user,runners,components,prefixes,xvfb}`.
#
# Imported by program.nix. Everything here works on the Nix/NixOS level; Cask's sources are not touched.
#
# Downloads are done by Nix (fetchzip: the hash is checked, the archive is unpacked into the Nix store).
# The `cask-sync` systemd service then makes Cask's folder (`programs.cask.dataDir`) match the
# configuration, as the user `programs.cask.user`:
#
#   * runners      -> data/runners/<name>           (symbolic link into the store)
#   * components   -> data/<dxvk|d7vk|vkd3d-proton>/<version>   (symbolic link into the store)
#   * prefixes     -> prefixes/<name>/ with cask.json: created, the runner set, components applied,
#                     winetricks verbs run
#
# What is already there is not redone: the service only creates and changes what differs from the
# configuration (see nix/cask-sync.sh for the exact rules).
{
  config,
  lib,
  pkgs,
  ...
}:

let
  cfg = config.programs.cask;

  # What a Cask folder knows as components (Components::types() in the sources).
  componentTypes = [
    "dxvk"
    "d7vk"
    "vkd3d-proton"
  ];

  # Verbs that Cask manages itself (Winetricks::isManagedByCask): use `components` for them.
  managedByCask = verb: lib.any (p: lib.hasPrefix p verb) [ "dxvk" "vkd3d" "d9vk" ];
  validVerb = verb: builtins.match "[A-Za-z0-9][A-Za-z0-9_.+=-]*" verb != null;
  validName = name: name != "" && !(lib.hasInfix "/" name) && !(lib.hasPrefix "." name);

  # Cask drops variables with other names, and the ones it sets itself (LaunchSettings::isValidEnvName,
  # reservedEnvNames), when it loads cask.json: they would silently not work.
  reservedEnvNames = [
    "WINEPREFIX"
    "WINEARCH"
    "PATH"
    "LANG"
    "LC_ALL"
    "DXVK_STATE_CACHE_PATH"
    "DXVK_SHADER_CACHE_PATH"
    "MESA_SHADER_CACHE_DIR"
    "VKD3D_SHADER_CACHE_PATH"
  ];
  validEnvName = name: builtins.match "[A-Za-z_][A-Za-z0-9_]*" name != null;
  # A DLL name without extension (`*` is allowed) and what Wine accepts as an override: a list of
  # native/builtin (n/b), or an empty string to disable the DLL.
  validOverrideName = name: builtins.match "[A-Za-z0-9_.*+-]+" name != null && !(lib.hasSuffix ".dll" name);
  validOverrideValue = value: builtins.match "|(native|builtin|n|b)(,(native|builtin|n|b))*" value != null;

  downloadOptions = {
    url = lib.mkOption {
      type = lib.types.str;
      example = "https://github.com/doitsujin/dxvk/releases/download/v2.7.1/dxvk-2.7.1.tar.gz";
      description = ''
        Where to download the archive from (zip, tar.gz, tar.xz, tar.zst, ...).
      '';
    };
    hash = lib.mkOption {
      type = lib.types.str;
      example = "sha256-AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
      description = ''
        Hash of the **unpacked** archive in SRI form (the one `fetchzip` wants, not the hash of the
        archive file). To find it, put `lib.fakeHash` here and rebuild: the error message shows
        the right value.
      '';
    };
    stripRoot = lib.mkOption {
      type = lib.types.bool;
      default = true;
      description = ''
        The archive contains one top-level folder (as release archives usually do) whose content is
        what is used. Set to `false` if the archive has its files at the top level.
      '';
    };
    extension = lib.mkOption {
      type = lib.types.nullOr lib.types.str;
      default = null;
      example = "tar.gz";
      description = ''
        File extension that tells how to unpack the archive, for URLs whose name does not end with it
        (e.g. `https://example.org/download?id=42`).
      '';
    };
  };

  runnerModule = lib.types.submodule {
    options = downloadOptions // {
      subdir = lib.mkOption {
        type = lib.types.nullOr lib.types.str;
        default = null;
        example = "files";
        description = ''
          Folder inside the archive that holds `bin/wine`. Cask expects `data/runners/<name>/bin/wine`;
          Proton builds keep it one level deeper (`files/bin/wine`), so `subdir = "files"`.
        '';
      };
    };
  };

  componentModule = lib.types.submodule { options = downloadOptions; };

  prefixModule = lib.types.submodule {
    options = {
      runner = lib.mkOption {
        type = lib.types.str;
        default = "system";
        example = "wine-ge-8-26";
        description = ''
          The Wine of the prefix: `"system"` (the {option}`programs.cask.wine` package) or the name of
          a runner from {option}`programs.cask.runners`. If it differs from the one recorded in the
          existing prefix, the prefix is switched to it the way Cask does (`wineboot -u`).
        '';
      };
      arch = lib.mkOption {
        type = lib.types.enum [
          "win64"
          "win32"
        ];
        default = "win64";
        description = ''
          Architecture of the prefix. It cannot be changed in an existing prefix: if it differs, the
          old prefix is renamed to `<name>.old-<arch>-<date>` (nothing is deleted) and a new one is
          created.
        '';
      };
      components = lib.genAttrs componentTypes (
        type:
        lib.mkOption {
          type = lib.types.nullOr lib.types.str;
          default = null;
          example = "${type}-1.0";
          description = ''
            Version of ${type} to apply to the prefix: a name from
            {option}`programs.cask.components.${type}`. When it changes, the old version is rolled
            back first. A component that is not mentioned is left as it is.
          '';
        }
      );
      env = lib.mkOption {
        type = lib.types.attrsOf lib.types.str;
        default = { };
        example = {
          DXVK_HUD = "fps";
          WINEDEBUG = "-all";
        };
        description = ''
          Environment variables of the programs that Cask starts in this prefix (the same list as
          "Environment variables" in Cask). Not allowed: the ones Cask sets itself (`WINEPREFIX`, `WINEARCH`,
          `PATH`, `LANG`, `LC_ALL`, `DXVK_STATE_CACHE_PATH`, `DXVK_SHADER_CACHE_PATH`,
          `MESA_SHADER_CACHE_DIR`, `VKD3D_SHADER_CACHE_PATH`). Variables you add in Cask yourself are kept;
          one that is removed from here is removed from the prefix.
        '';
      };
      dllOverrides = lib.mkOption {
        type = lib.types.attrsOf lib.types.str;
        default = { };
        example = {
          d3d11 = "native,builtin";
          winmm = "builtin";
          mscoree = "";
        };
        description = ''
          DLL overrides of the prefix, written to `HKCU\Software\Wine\DllOverrides`: the DLL name without
          extension (or `*`) and `native`, `builtin`, `native,builtin`, `builtin,native` (`n` and `b` work
          too), or `""` to disable the DLL. They are set after the components, so they win over the ones a
          component has set. One that is removed from here is removed from the registry; the others are
          not touched.
        '';
      };
      winetricks = lib.mkOption {
        type = lib.types.listOf lib.types.str;
        default = [ ];
        example = [
          "corefonts"
          "vcrun2022"
          "win10"
        ];
        description = ''
          Winetricks verbs (also settings such as `win10` or `sound=alsa`), run in this order with
          `winetricks -q`. A verb that winetricks has already recorded in the prefix's
          `winetricks.log` is not run again. winetricks downloads its files itself (into
          `<dataDir>/data/winetricks/cache`), the verified-by-hash downloads of Nix do not apply to
          them. DXVK, VKD3D-Proton and D9VK are applied through
          {option}`programs.cask.prefixes.<name>.components`, not through winetricks.
        '';
      };
    };
  };

  declared =
    cfg.runners != { }
    || lib.any (type: cfg.components.${type} != { }) componentTypes
    || cfg.prefixes != { };

  # ---- downloads ----------------------------------------------------------------------------

  fetch =
    name: d:
    pkgs.fetchzip {
      inherit (d) url hash stripRoot;
      name = lib.strings.sanitizeDerivationName name;
      extension = d.extension;
      nativeBuildInputs = [ pkgs.zstd ];
    };

  # The derivation names matter: cask-sync recognises the links it has made by `-cask-<kind>-` in the store path.
  runnerPaths = lib.mapAttrs (
    name: r:
    let
      src = fetch "cask-runner-${name}" r;
    in
    if r.subdir == null then "${src}" else "${src}/${r.subdir}"
  ) cfg.runners;

  componentPaths = lib.genAttrs componentTypes (
    type: lib.mapAttrs (version: c: "${fetch "cask-${type}-${version}" c}") cfg.components.${type}
  );

  manifest = pkgs.writeText "cask-sync-manifest.json" (
    builtins.toJSON {
      runners = runnerPaths;
      components = componentPaths;
      prefixes = lib.mapAttrs (_: p: {
        inherit (p) runner arch winetricks env dllOverrides;
        components = lib.filterAttrs (_: version: version != null) p.components;
      }) cfg.prefixes;
    }
  );

  # ---- the script and the service -------------------------------------------------------------

  tools =
    with pkgs;
    [
      bash
      coreutils
      findutils
      gnugrep
      gnused
      jq
      util-linux
      winetricks
      cabextract
      unzip
      p7zip
      curl
    ]
    ++ lib.optional cfg.xvfb xvfb-run
    ++ lib.optional (cfg.wine != null) cfg.wine
    ++ cfg.extraPackages;

  syncScript = pkgs.writeShellApplication {
    name = "cask-sync";
    runtimeInputs = tools;
    # No `errexit`: the script checks results itself so that one failed prefix does not stop the others.
    bashOptions = [
      "nounset"
      "pipefail"
    ];
    # SC2016: the jq filters are in single quotes on purpose ($t there is a jq variable, not a shell one).
    excludeShellChecks = [ "SC2016" ];
    text = builtins.readFile ./cask-sync.sh;
  };

  # Same default as the launcher of package.nix: a shell expression, evaluated by the service as the user.
  rootExpression = if cfg.dataDir != null then cfg.dataDir else "\${XDG_DATA_HOME:-$HOME/.local/share}/cask";

  runScript = pkgs.writeShellScript "cask-sync-run" ''
    export CASK_SYNC_ROOT="${rootExpression}"
    export CASK_SYNC_XVFB=${if cfg.xvfb then "1" else "0"}
    exec ${lib.getExe syncScript} ${manifest}
  '';

  # Downloaded Wine/Proton builds are generic Linux binaries: the setup has to run them in the same
  # FHS environment as `programs.cask.fhs` gives to Cask itself.
  fhsRun = pkgs.buildFHSEnv {
    name = "cask-sync-fhs";
    targetPkgs = _: [ ];
    multiArch = true; # the 32-bit loader and libraries, see passthru.fhs in package.nix
    multiPkgs = cfg.package.fhsMultiPkgs;
    profile = cfg.package.fhsProfile;
    runScript = "${runScript}";
  };
in
{
  options.programs.cask = {
    user = lib.mkOption {
      type = lib.types.nullOr lib.types.str;
      default = null;
      example = "alice";
      description = ''
        The user whose Cask folder ({option}`programs.cask.dataDir`) is set up by
        {option}`programs.cask.runners`, {option}`programs.cask.components` and
        {option}`programs.cask.prefixes`. The `cask-sync` service runs as this user on
        `nixos-rebuild switch`/`test` (every time, not only when the configuration changed) and at boot, so a
        long winetricks verb makes the rebuild wait for it. Its output is in `journalctl -u cask-sync`.
      '';
    };

    runners = lib.mkOption {
      type = lib.types.attrsOf runnerModule;
      default = { };
      example = lib.literalExpression ''
        {
          "wine-ge-8-26" = {
            url = "https://github.com/GloriousEggroll/wine-ge-custom/releases/download/GE-Proton8-26/wine-lutris-GE-Proton8-26-x86_64.tar.xz";
            hash = lib.fakeHash;
          };
        }
      '';
      description = ''
        Wine builds to download, by name. Each appears in Cask as a runner of that name
        (`<dataDir>/data/runners/<name>`, a link to the Nix store). Generic Wine/Proton builds run only
        with {option}`programs.cask.fhs`. A link that Nix did not make is never replaced, and the
        links of runners that are removed from the configuration are removed.
      '';
    };

    components = lib.genAttrs componentTypes (
      type:
      lib.mkOption {
        type = lib.types.attrsOf componentModule;
        default = { };
        example = lib.literalExpression ''
          {
            "${type}-1.0" = {
              url = "https://example.org/${type}-1.0.tar.gz";
              hash = lib.fakeHash;
            };
          }
        '';
        description = ''
          ${type} versions to download, by name. Each appears in Cask under that name
          (`<dataDir>/data/${type}/<name>`, a link to the Nix store); the archive must have the
          `x64` and `x32` (or `x86`) folders with the DLLs. Apply one to a prefix with
          {option}`programs.cask.prefixes.<name>.components.${type}`.
        '';
      }
    );

    prefixes = lib.mkOption {
      type = lib.types.attrsOf prefixModule;
      default = { };
      example = lib.literalExpression ''
        {
          games = {
            runner = "wine-ge-8-26";
            components.dxvk = "dxvk-2.7.1";
            winetricks = [ "corefonts" "vcrun2022" "win10" ];
          };
        }
      '';
      description = ''
        Prefixes to have, by name: `<dataDir>/prefixes/<name>`. A prefix that already exists and
        matches is not touched; only what differs from the configuration is changed. Prefixes
        that are not listed here are left alone.
      '';
    };

    xvfb = lib.mkOption {
      type = lib.types.bool;
      default = true;
      description = ''
        Run the `cask-sync` service on a virtual X server (Xvfb): the service has no display, and
        some winetricks verbs and Wine itself open windows.
      '';
    };
  };

  config = lib.mkIf (cfg.enable && declared) {
    assertions =
      [
        {
          assertion = cfg.user != null;
          message = "programs.cask.user must be set when programs.cask.runners, components or prefixes are used.";
        }
        {
          assertion = cfg.user == null || config.users.users ? ${cfg.user};
          message = "programs.cask.user = \"${toString cfg.user}\" is not a user of this system (users.users).";
        }
      ]
      ++ lib.mapAttrsToList (name: _: {
        assertion = validName name;
        message = "programs.cask.runners.\"${name}\": the name must not be empty, contain '/' or start with '.'.";
      }) cfg.runners
      ++ lib.concatMap (
        type:
        lib.mapAttrsToList (version: _: {
          assertion = validName version;
          message = "programs.cask.components.${type}.\"${version}\": the name must not be empty, contain '/' or start with '.'.";
        }) cfg.components.${type}
      ) componentTypes
      ++ lib.concatLists (
        lib.mapAttrsToList (
          name: p:
          [
            {
              assertion = validName name;
              message = "programs.cask.prefixes.\"${name}\": the name must not be empty, contain '/' or start with '.'.";
            }
            {
              assertion = p.runner == "system" -> cfg.wine != null;
              message = "programs.cask.prefixes.\"${name}\".runner = \"system\" needs programs.cask.wine to be set.";
            }
            {
              assertion = p.runner == "system" || cfg.runners ? ${p.runner};
              message = "programs.cask.prefixes.\"${name}\".runner = \"${p.runner}\" is not \"system\" and is not declared in programs.cask.runners.";
            }
          ]
          ++ lib.concatMap (verb: [
            {
              assertion = validVerb verb;
              message = "programs.cask.prefixes.\"${name}\".winetricks: \"${verb}\" is not a winetricks verb"
                + " (letters, digits and _ . + = -, as in `winetricks list-all`).";
            }
            {
              assertion = !(managedByCask verb);
              message = "programs.cask.prefixes.\"${name}\".winetricks: \"${verb}\" is managed by Cask itself;"
                + " apply DXVK, VKD3D-Proton and D9VK with programs.cask.prefixes.<name>.components.";
            }
          ]) p.winetricks
          ++ lib.concatMap (var: [
            {
              assertion = validEnvName var;
              message = "programs.cask.prefixes.\"${name}\".env: \"${var}\" is not a valid variable name (Cask accepts [A-Za-z_][A-Za-z0-9_]*).";
            }
            {
              assertion = !(lib.elem var reservedEnvNames);
              message = "programs.cask.prefixes.\"${name}\".env: \"${var}\" is set by Cask itself and would be ignored.";
            }
          ]) (lib.attrNames p.env)
          ++ lib.concatMap (dll: [
            {
              assertion = validOverrideName dll;
              message = "programs.cask.prefixes.\"${name}\".dllOverrides: \"${dll}\" is not a DLL name (write it without the .dll extension).";
            }
            {
              assertion = validOverrideValue p.dllOverrides.${dll};
              message = "programs.cask.prefixes.\"${name}\".dllOverrides.${dll} = \"${p.dllOverrides.${dll}}\": use native, builtin, native,builtin, builtin,native (n/b) or \"\" to disable.";
            }
          ]) (lib.attrNames p.dllOverrides)
          ++ lib.concatMap (
            type:
            lib.optional (p.components.${type} != null) {
              assertion = cfg.components.${type} ? ${p.components.${type}};
              message = "programs.cask.prefixes.\"${name}\".components.${type} = \"${p.components.${type}}\" is not declared in programs.cask.components.${type}.";
            }
          ) componentTypes
        ) cfg.prefixes
      );

    warnings = lib.optional (cfg.runners != { } && !cfg.fhs) ''
      programs.cask.runners are generic Linux Wine/Proton builds, which do not run on NixOS outside an FHS
      environment: set programs.cask.fhs = true (the cask-sync service and Cask then run in one).
    '';

    systemd.services.cask-sync = {
      description = "Declarative Cask setup (runners, components, prefixes) for ${toString cfg.user}";
      wantedBy = [ "multi-user.target" ];
      wants = [ "network-online.target" ];
      after = [
        "network-online.target"
        "local-fs.target"
      ];
      # No RemainAfterExit: the service is inactive after its run, and `switch-to-configuration` starts the active
      # targets (and with them such services) on every switch, so the prefixes are checked after every rebuild.
      # restartTriggers: also restart it if it is running while the configuration changes.
      restartTriggers = [
        manifest
        runScript
      ];
      environment.HOME = config.users.users.${cfg.user}.home;
      serviceConfig = {
        Type = "oneshot";
        User = cfg.user;
        TimeoutStartSec = "infinity";
        ExecStart = if cfg.fhs then "${fhsRun}/bin/cask-sync-fhs" else "${runScript}";
      };
    };
  };
}
