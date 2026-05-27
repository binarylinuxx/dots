self: { config, lib, pkgs, ... }:

let
  cfg = config.programs.blxshell;
  package = cfg.package;
  pluginEnv = lib.concatStringsSep ":" (
    (map (plugin: "${plugin}/share/blxshell/plugins") cfg.plugins)
    ++ [ "${config.home.homeDirectory}/.local/share/blxshell/plugins" ]
  );
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

    home.packages = [ package ] ++ lib.optionals cfg.installDependencies ([
      self.packages.${pkgs.system}.blxshell-quickshell-git
    ] ++ (with pkgs; [
      fish
      ghostty
      starship
      brightnessctl
      grim
      python3
      python3Packages.pillow
      slurp
      uv
      wl-clipboard
      hyprland
      hyprsunset
      xdg-desktop-portal
      xdg-desktop-portal-gtk
      xdg-desktop-portal-hyprland
      kdePackages.xdg-desktop-portal-kde
      cava
      pavucontrol-qt
      pipewire
      playerctl
      wireplumber
      wf-recorder
      cmake
      ninja
      qt6.qtbase
      qt6.qtdeclarative
      qt6.qtshadertools
      wayland
      wayland-protocols
    ]));

    home.file.".local/blxshell" = {
      source = "${package}/share/blxshell";
      recursive = true;
    };

    home.sessionVariables = lib.mkIf (cfg.plugins != [ ]) {
      BLXSHELL_PLUGIN_DIRS = pluginEnv;
    };
  };
}
