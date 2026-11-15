# programs.blxshell for Runix (runit, mdevd, elogind and seatd; no systemd).
# The options match the NixOS module in module.nix, so its docs apply here.
#
#   modules = [
#     blxshell.runixModules.default
#     {
#       programs.zwwm.enable = true;
#       programs.blxshell.enable = true;
#       programs.blxshell.greeter = { enable = true; syncUser = "alice"; };
#     }
#   ];
#
# blxshell starts from the compositor's own autostart (`exec-once = blxshell`
# for Hyprland, `exec-sh-on-startup` for zwwm). Runix has no per-session
# service manager to start it from, and the compositor has to be running first
# anyway. Users without their own zwwm or Hyprland configuration get one made
# for blxshell (runix/zwwm.zw and runix/hyprland.conf), copied once; see
# programs.blxshell.seedConfigs.
#
# The greeter runs as on NixOS: greetd runs a minimal zwwm, zwwm runs
# blxshell-greeter (one login per monitor) and quits as soon as it exits, and
# greetd starts the session that was picked. Here greetd is a runit service,
# and it replaces SDDM when runix.desktop is enabled.
self:
{ config, lib, pkgs, ... }:

let
  cfg = config.programs.blxshell;
  greeter = cfg.greeter;
  inherit (lib) mkEnableOption mkIf mkMerge mkOption optional optionalString types;

  system = pkgs.stdenv.hostPlatform.system;
  fonts = import ./fonts.nix { inherit pkgs lib; };

  # nixpkgs moved greetd from greetd.greetd to the top level.
  greetd = pkgs.greetd.greetd or pkgs.greetd;

  # The theme folder the greeter reads (see blxshell-greeter --export). It is
  # also the greeter user's home, where it remembers the last user and session.
  theme = "/var/lib/blxshell-greeter";
  uid = 176;
  zwwmBin = if greeter.zwwmPackage == null then "${config.programs.zwwm.package}/bin" else "${greeter.zwwmPackage}/bin";
  home = config.runix.users.${greeter.syncUser}.home or "/home/${greeter.syncUser}";
  synced = "/run/blxshell-greeter-synced";

  # Accounts that sign in: from uid 1000, the installer's first user, up to
  # the Nix build users at 30000, which have no real home.
  people = lib.filterAttrs (
    _: user: user.uid >= 1000 && user.uid < 30000 && user.home != "/var/empty"
  ) config.runix.users;
  seeds =
    optional config.programs.zwwm.enable { file = ./runix/zwwm.zw; target = "zwwm/config.zw"; }
    ++ optional config.programs.hyprland.enable { file = ./runix/hyprland.conf; target = "hypr/hyprland.conf"; };
  # Copies a config only where the user has none, so their own is never touched.
  seedScript = lib.concatStrings (
    lib.mapAttrsToList (
      name: user:
      lib.concatMapStrings (seed: ''
        if [ -d ${lib.escapeShellArg user.home} ] && [ ! -e ${lib.escapeShellArg "${user.home}/.config/${seed.target}"} ]; then
          install -D -m 0644 -o ${toString user.uid} -g ${toString user.gid} \
            ${seed.file} ${lib.escapeShellArg "${user.home}/.config/${seed.target}"}
          chown ${toString user.uid}:${toString user.gid} ${lib.escapeShellArg "${user.home}/.config"} \
            ${lib.escapeShellArg "${user.home}/.config/${builtins.dirOf seed.target}"}
        fi
      '') seeds
    ) people
  );

  # What greetd runs: zwwm, with the greeter's config, the installed sessions
  # and the cursor. runit services start with a bare environment, so the seat
  # backend and search path are set here.
  session = pkgs.writeShellScript "blxshell-greeter-session" ''
    export PATH=/run/current-system/sw/bin
    export LIBSEAT_BACKEND=seatd
    export XDG_SESSION_TYPE=wayland
    export XDG_CONFIG_HOME=${theme}/xdg
    export XDG_DATA_DIRS=/run/current-system/sw/share
    ${optionalString (greeter.cursor != null) ''
      export XCURSOR_PATH=${greeter.cursor.package}/share/icons
      export XCURSOR_THEME=${lib.escapeShellArg greeter.cursor.name}
      export XCURSOR_SIZE=${toString greeter.cursor.size}
    ''}
    exec ${zwwmBin}/zwwm --no-portal
  '';

  # zwwm for the greeter: nothing but the greeter, and quit after it.
  zwwmConfig = pkgs.writeText "blxshell-greeter-zwwm.zw" ''
    exec-sh-on-startup = ["${greeter.package}/bin/blxshell-greeter --layer; ${zwwmBin}/zwwmctl dispatch exit"]
  '';

  greetdConfig = (pkgs.formats.toml { }).generate "greetd.toml" {
    terminal.vt = greeter.vt;
    default_session = {
      user = "greeter";
      command = "${session}";
    };
  };

  # Same stack as Runix's /etc/pam.d/login; pam_elogind gives the greeter and
  # the sessions it starts their seat and XDG_RUNTIME_DIR.
  pam = pkgs.linux-pam;
  pamGreetd = pkgs.writeText "blxshell-pam-greetd" ''
    auth required ${pam}/lib/security/pam_unix.so
    account required ${pam}/lib/security/pam_unix.so
    password required ${pam}/lib/security/pam_unix.so
    session required ${pam}/lib/security/pam_env.so conffile=/etc/security/pam_env.conf readenv=0
    session required ${pam}/lib/security/pam_unix.so
    session optional ${pkgs.elogind}/lib/security/pam_elogind.so
  '';

  export = "${greeter.package}/bin/blxshell-greeter --export ${theme} --from ${home} --user ${greeter.syncUser}";
  watched = [
    "${home}/.config/blxshell/config.json"
    "${home}/.local/state/blxshell/matugen-colours.json"
    "${home}/.local/state/blxshell/wallpaper"
    "${home}/.face"
  ];

  # Exports once, then again whenever one of the watched files changes. It
  # watches their folders, since a file may not exist yet or be replaced by
  # a rename; folders that do not exist yet are retried every 30 seconds.
  syncScript = ''
    ${export} || echo "blxshell-greeter: theme export failed" >&2
    touch ${synced}
    while true; do
      folders=()
      for file in ${lib.escapeShellArgs watched}; do
        folder="''${file%/*}"
        [ -d "$folder" ] && folders+=("$folder")
      done
      if [ "''${#folders[@]}" -eq 0 ] ||
        ! inotifywait -qq -e close_write -e moved_to -e create -e delete \
          --include '(config\.json|matugen-colours\.json|wallpaper|\.face)$' "''${folders[@]}"; then
        sleep 30
        continue
      fi
      # A burst of saves (a new wallpaper and its scheme) is one export.
      sleep 3
      ${export} || echo "blxshell-greeter: theme export failed" >&2
    done
  '';
in
{
  options.programs.blxshell = {
    enable = mkEnableOption "blxshell, the desktop shell; start it from your compositor's autostart";

    package = mkOption {
      type = types.package;
      default = self.packages.${system}.default;
      defaultText = lib.literalExpression "blxshell.packages.\${system}.default";
      description = "The blxshell package.";
    };

    seedConfigs = mkOption {
      type = types.bool;
      default = true;
      description = ''
        Give every user without a zwwm or Hyprland configuration one set up
        for blxshell: it starts the shell and binds its launcher, lock screen,
        power menu, wallpapers and settings. It is copied once, at
        installation or the next boot, and never overwritten after that.
      '';
    };

    fonts = mkOption {
      type = types.listOf types.package;
      default = [ fonts.blxshell-custom-fonts ];
      defaultText = lib.literalExpression "[ blxshell-custom-fonts ]";
      description = "Fonts blxshell draws with: Google Sans Flex, Rubik, Material Symbols, Readex Pro, Space Grotesk and Bitcount.";
    };

    greeter = {
      enable = mkEnableOption "blxshell-greeter, the blxshell login screen, under greetd";

      package = mkOption {
        type = types.package;
        default = self.packages.${system}.default;
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

      vt = mkOption {
        type = types.ints.positive;
        default = 7;
        description = "Virtual terminal the login screen runs on. The default keeps clear of the getty on tty1.";
      };
    };
  };

  config = mkMerge [
    (mkIf cfg.enable {
      # matugen builds the Material colour scheme from the wallpaper.
      runix.packages = [ cfg.package pkgs.matugen ];
      fonts.packages = cfg.fonts;
    })

    (mkIf (cfg.enable && cfg.seedConfigs) {
      # Both seeded configs open ghostty with Super+Return.
      runix.packages = [ pkgs.ghostty ];
      runix.preparationScripts = [ seedScript ];
    })

    (mkIf greeter.enable {
      assertions = [
        {
          assertion = greeter.zwwmPackage != null || config.programs.zwwm.enable;
          message = "programs.blxshell.greeter runs inside zwwm: enable programs.zwwm or set zwwmPackage.";
        }
        {
          assertion = greeter.syncUser == null || config.runix.users ? ${greeter.syncUser};
          message = "programs.blxshell.greeter.syncUser names a user that runix.users does not define.";
        }
      ];

      # Seats, sessions and the system bus, as every Runix Wayland session has.
      runix.wayland.enable = lib.mkDefault true;

      runix.groups.greeter.gid = uid;
      runix.users.greeter = {
        inherit uid;
        gid = uid;
        home = theme;
        shell = "/bin/false";
        extraGroups = [ "video" "input" "render" ];
      };

      fonts.packages = greeter.fonts;
      # --export by hand, and the cursor theme for anything else that wants it.
      runix.packages = [ greeter.package greetd ] ++ optional (greeter.cursor != null) greeter.cursor.package;

      runix.preparationScripts = [
        ''
          mkdir -p /etc/greetd /etc/pam.d
          ln -sfn ${greetdConfig} /etc/greetd/config.toml
          ln -sfn ${pamGreetd} /etc/pam.d/greetd
          install -d -m 0755 ${theme} ${theme}/.local ${theme}/.local/state
          install -d -m 0755 -o ${toString uid} -g ${toString uid} \
            ${theme}/xdg ${theme}/xdg/zwwm ${theme}/.local/state/blxshell-greeter
          ln -sfn ${zwwmConfig} ${theme}/xdg/zwwm/config.zw
        ''
      ];

      runix.services = {
        greetd = {
          command = "${greetd}/bin/greetd --config /etc/greetd/config.toml";
          after = [ "dbus" "elogind" "seatd" "mdevd-coldplug" ]
            ++ optional (greeter.syncUser != null) "blxshell-greeter-sync";
        };

        # Keeps the login screen in step with syncUser's look.
        blxshell-greeter-sync = mkIf (greeter.syncUser != null) {
          script = syncScript;
          path = [ pkgs.coreutils pkgs.inotify-tools ];
          check = "test -e ${synced}";
        };

        # One login screen: the greeter takes over from Runix's SDDM.
        sddm = mkIf config.runix.desktop.enable { enable = lib.mkForce false; };
      };
    })
  ];
}
