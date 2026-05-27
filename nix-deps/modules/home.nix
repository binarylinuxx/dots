self: { config, lib, pkgs, ... }:

let
  cfg = config.programs.blxshell;
  package = cfg.package;
  colGenPkg = self.packages.${pkgs.system}.blxshell-col-gen;
  homeDir = config.home.homeDirectory;
  userShellDir = "${homeDir}/.local/blxshell";
  pluginEnv = lib.concatStringsSep ":" (
    (map (plugin: "${plugin}/share/blxshell/plugins") cfg.plugins)
    ++ [ "${homeDir}/.local/share/blxshell/plugins" ]
  );

  # Files generated at runtime by col_gen or settings — must survive rebuilds
  userFiles = [
    "Colors.json"
    "config.json"
    "lockscreen/Colors.json"
    "menu/Colors.json"
  ];
in {
  options.programs.blxshell = {
    enable = lib.mkEnableOption "blxshell";

    package = lib.mkOption {
      type = lib.types.package;
      default = self.packages.${pkgs.system}.blxshell-bundle;
      description = "blxshell package to deploy.";
    };

    installDependencies = lib.mkOption {
      type = lib.types.bool;
      default = true;
      description = "Install blxshell runtime dependencies through Home Manager.";
    };

    plugins = lib.mkOption {
      type = lib.types.listOf lib.types.package;
      default = [ ];
      description = "blxshell plugin packages. Each package should expose share/blxshell/plugins.";
    };
  };

  config = lib.mkIf cfg.enable {
    fonts.fontconfig.enable = true;

    home.packages = [ package colGenPkg ] ++ lib.optionals cfg.installDependencies (with pkgs; [
      cmake
      ninja
      qt6.qtbase
      qt6.qtdeclarative
      qt6.qtshadertools
    ]);

    home.activation.installBlxshellRuntime = lib.hm.dag.entryAfter ["writeBoundary"] ''
      runtimeSrc="${package}/share/blxshell"
      runtimeDst="${userShellDir}"

      if [ ! -d "$runtimeDst" ]; then
        # First install — copy everything including default configs/colors
        mkdir -p "$runtimeDst"
        cp -r "$runtimeSrc/." "$runtimeDst/"
        chmod -R u+w "$runtimeDst"
        rm -rf "$runtimeDst/.obsidian" "$runtimeDst/.ruff_cache" "$runtimeDst/col_gen/.venv" "$runtimeDst/col_gen/__pycache__"
        find "$runtimeDst" -name '__pycache__' -type d -prune -exec rm -rf {} +
      else
        # Rebuild — preserve user-generated files, update everything else
        preserveDir=$(mktemp -d)
        for f in ${lib.concatStringsSep " " userFiles}; do
          srcFile="$runtimeDst/$f"
          if [ -f "$srcFile" ]; then
            mkdir -p "$(dirname "$preserveDir/$f")"
            cp "$srcFile" "$preserveDir/$f"
          fi
        done

        rm -rf "$runtimeDst"
        mkdir -p "$runtimeDst"
        cp -r "$runtimeSrc/." "$runtimeDst/"
        chmod -R u+w "$runtimeDst"
        rm -rf "$runtimeDst/.obsidian" "$runtimeDst/.ruff_cache" "$runtimeDst/col_gen/.venv" "$runtimeDst/col_gen/__pycache__"
        find "$runtimeDst" -name '__pycache__' -type d -prune -exec rm -rf {} +

        for f in ${lib.concatStringsSep " " userFiles}; do
          savedFile="$preserveDir/$f"
          if [ -f "$savedFile" ]; then
            cp "$savedFile" "$runtimeDst/$f"
          fi
        done

        rm -rf "$preserveDir"
      fi
    '';

    home.sessionVariables = {
      BLXSHELL_PATH = userShellDir;
    } // lib.optionalAttrs (cfg.plugins != [ ]) {
      BLXSHELL_PLUGIN_DIRS = pluginEnv;
    };
  };
}
