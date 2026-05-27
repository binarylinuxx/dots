self: { config, lib, pkgs, ... }:

let
  cfg = config.services.blxshellGreeter;
in {
  options.services.blxshellGreeter = {
    enable = lib.mkEnableOption "blxshell greeter";

    package = lib.mkOption {
      type = lib.types.package;
      default = self.packages.${pkgs.system}.blxshell-greeter-launcher;
      description = "blxshell greeter launcher package.";
    };

    user = lib.mkOption {
      type = lib.types.str;
      default = "greeter";
      description = "User that runs the greeter session.";
    };

    cacheDir = lib.mkOption {
      type = lib.types.path;
      default = "/var/cache/blxshell-greeter";
      description = "Writable cache/home directory used by the greeter.";
    };

    debug = lib.mkOption {
      type = lib.types.bool;
      default = false;
      description = "Enable verbose greeter-launcher logs.";
    };
  };

  config = lib.mkIf cfg.enable {
    services.greetd = {
      enable = true;
      settings.default_session = {
        user = cfg.user;
        command = lib.concatStringsSep " " ([
          "${cfg.package}/bin/greeter-launcher"
          "--cache-dir"
          (toString cfg.cacheDir)
        ] ++ lib.optional cfg.debug "--debug");
      };
    };

    users.users.${cfg.user} = {
      isSystemUser = true;
      group = cfg.user;
      home = toString cfg.cacheDir;
      createHome = false;
    };

    users.groups.${cfg.user} = { };

    systemd.tmpfiles.rules = [
      "d ${toString cfg.cacheDir} 0750 ${cfg.user} ${cfg.user} - -"
    ];

    environment.systemPackages = [ cfg.package ];
    programs.hyprland.enable = lib.mkDefault true;
    security.polkit.enable = lib.mkDefault true;
  };
}
