# programs.blxshell.greeter: blxshell-greeter as the login screen. greetd runs
# a minimal zwwm, zwwm runs the greeter (one login per monitor) and quits as
# soon as it exits, and greetd starts the session that was picked.
#
#   programs.blxshell.greeter = {
#     enable = true;
#     syncUser = "alice";   # wear alice's colours, wallpaper and picture
#     cursor = { package = pkgs.bibata-cursors; name = "Bibata-Modern-Classic"; size = 24; };
#     fonts = [ … ];        # Rubik, Google Sans Flex, Material Symbols Rounded
#   };
self:
{ config, lib, pkgs, ... }:

let
  cfg = config.programs.blxshell.greeter;
  inherit (lib) mkEnableOption mkIf mkOption optional optionalString types;

  # The theme folder the greeter reads (see blxshell-greeter --export).
  theme = "/var/lib/blxshell-greeter";
  sessions = "${config.services.displayManager.sessionData.desktops}/share";
  zwwmBin = if cfg.zwwmPackage == null then "/run/current-system/sw/bin" else "${cfg.zwwmPackage}/bin";
  home = config.users.users.${cfg.syncUser}.home;

  # What greetd runs: zwwm, with the greeter's config, the installed sessions
  # and the cursor.
  session = pkgs.writeShellScript "blxshell-greeter-session" ''
    export XDG_CONFIG_HOME=${theme}/xdg
    export XDG_DATA_DIRS=${sessions}
    ${optionalString (cfg.cursor != null) ''
      export XCURSOR_PATH=${cfg.cursor.package}/share/icons
      export XCURSOR_THEME=${lib.escapeShellArg cfg.cursor.name}
      export XCURSOR_SIZE=${toString cfg.cursor.size}
    ''}
    exec ${zwwmBin}/zwwm --no-portal
  '';

  export = "${cfg.package}/bin/blxshell-greeter --export ${theme} --from ${home} --user ${cfg.syncUser}";
in {
  options.programs.blxshell.greeter = {
    enable = mkEnableOption "blxshell-greeter, the blxshell login screen, under greetd";

    package = mkOption {
      type = types.package;
      default = self.packages.${pkgs.stdenv.hostPlatform.system}.default;
      defaultText = lib.literalExpression "blxshell.packages.\${system}.default";
      description = "The blxshell package providing blxshell-greeter.";
    };

    syncUser = mkOption {
      type = types.nullOr types.str;
      default = null;
      example = "alice";
      description = ''
        Whose look the login screen wears. Their config, colour scheme,
        wallpaper and ~/.face are copied to ${theme} at boot and whenever they
        change. Null leaves the folder to `blxshell-greeter --export`.
      '';
    };

    cursor = mkOption {
      type = types.nullOr (types.submodule {
        options = {
          package = mkOption {
            type = types.package;
            example = lib.literalExpression "pkgs.bibata-cursors";
            description = "Package providing the cursor theme in share/icons.";
          };
          name = mkOption {
            type = types.str;
            example = "Bibata-Modern-Classic";
            description = "The cursor theme's name.";
          };
          size = mkOption {
            type = types.int;
            default = 24;
            description = "Cursor size.";
          };
        };
      });
      default = null;
      description = "The pointer on the login screen, like home-manager's home.pointerCursor.";
    };

    fonts = mkOption {
      type = types.listOf types.package;
      default = [ ];
      description = "Fonts for the greeter: Rubik, Google Sans Flex and Material Symbols Rounded.";
    };

    zwwmPackage = mkOption {
      type = types.nullOr types.package;
      default = null;
      description = "zwwm to run the greeter in; null takes the system's (programs.zwwm).";
    };
  };

  config = mkIf cfg.enable {
    services.greetd = {
      enable = true;
      settings.default_session = {
        user = "greeter";
        command = "${session}";
      };
    };

    # zwwm for the greeter: nothing but the greeter, and quit after it.
    environment.etc."blxshell-greeter/zwwm/config.zw".text = ''
      exec-sh-on-startup = ["${cfg.package}/bin/blxshell-greeter --layer; ${zwwmBin}/zwwmctl dispatch exit"]
    '';

    systemd.tmpfiles.rules = [
      "d ${theme} 0755 root root -"
      "d ${theme}/xdg 0755 greeter greeter -"
      "d ${theme}/xdg/zwwm 0755 greeter greeter -"
      "L+ ${theme}/xdg/zwwm/config.zw - - - - /etc/blxshell-greeter/zwwm/config.zw"
      # The last user and session, remembered by the greeter.
      "d ${theme}/.local/state/blxshell-greeter 0755 greeter greeter -"
    ];

    # Keeps the login screen in step with syncUser's look.
    systemd.services.blxshell-greeter-sync = mkIf (cfg.syncUser != null) {
      description = "Copy ${cfg.syncUser}'s blxshell theme to the login screen";
      wantedBy = [ "multi-user.target" ];
      before = [ "display-manager.service" ];
      after = [ "local-fs.target" "systemd-tmpfiles-setup.service" ];
      unitConfig.RequiresMountsFor = [ home ];
      serviceConfig = {
        Type = "oneshot";
        ExecStart = export;
      };
    };
    systemd.paths.blxshell-greeter-sync = mkIf (cfg.syncUser != null) {
      description = "Watch ${cfg.syncUser}'s blxshell theme for the login screen";
      wantedBy = [ "multi-user.target" ];
      pathConfig = {
        PathChanged = [
          "${home}/.config/blxshell/config.json"
          "${home}/.local/state/blxshell/matugen-colours.json"
          "${home}/.local/state/blxshell/wallpaper"
          "${home}/.face"
        ];
        # A burst of saves (a new wallpaper and its scheme) is one export.
        TriggerLimitIntervalSec = "10s";
        TriggerLimitBurst = 5;
      };
    };

    fonts.packages = cfg.fonts;
    # --export by hand, and the cursor theme for anything else that wants it.
    environment.systemPackages = [ cfg.package ] ++ optional (cfg.cursor != null) cfg.cursor.package;

    assertions = [{
      assertion = cfg.zwwmPackage != null || (config.programs.zwwm.enable or false);
      message = "programs.blxshell.greeter runs inside zwwm: enable programs.zwwm or set zwwmPackage.";
    }];
  };
}
