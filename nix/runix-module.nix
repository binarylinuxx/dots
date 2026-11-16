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
# anyway. With zwwm, the "blxshell" session runs zwwm on Runix's own setup
# for the shell (runix/zwwm.zw and its shaders, from the store), apart from
# the user's ~/.config/zwwm. Hyprland users without a configuration get
# runix/hyprland.conf, copied once; see programs.blxshell.seedConfigs.
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
  # The seeded configs name the session cursor.
  withCursor = name: file: pkgs.replaceVars file {
    cursorName = cfg.cursor.name;
    cursorSize = toString cfg.cursor.size;
  };
  # zwwm gets no seed: the blxshell session below brings its own setup, and
  # the plain zwwm session is left to the user's ~/.config/zwwm.
  seeds =
    optional config.programs.hyprland.enable { file = withCursor "hyprland.conf" ./runix/hyprland.conf; target = "hypr/hyprland.conf"; };

  # The blxshell session: zwwm with Runix's setup for the shell (rounded
  # borders, shadows, animations, the cursor and blxshell itself), read from
  # the store through ZWWM_CONFIG. It follows every rebuild and never touches
  # ~/.config/zwwm, which the plain zwwm session keeps using.
  sessionZwwm = if greeter.zwwmPackage != null then greeter.zwwmPackage else config.programs.zwwm.package;
  sessionConfig = pkgs.runCommand "blxshell-zwwm-config" { } ''
    mkdir -p $out/shaders
    cp ${withCursor "config.zw" ./runix/zwwm.zw} $out/config.zw
    cp ${./runix/zwwm-shaders/glass.frag} $out/shaders/glass.frag
    cp ${./runix/zwwm-shaders/border.frag} $out/shaders/border.frag
    cp ${./runix/zwwm-shaders/shadow.frag} $out/shaders/shadow.frag
    # The shader interface of the zwwm that runs them, when it ships one.
    if [ -e ${sessionZwwm}/share/zwwm/shaders/zwwm.glsl ]; then
      cp ${sessionZwwm}/share/zwwm/shaders/zwwm.glsl $out/shaders/zwwm.glsl
    else
      cp ${./runix/zwwm-shaders/zwwm.glsl} $out/shaders/zwwm.glsl
    fi
  '';
  sessionScript = pkgs.writeShellScript "blxshell-session" ''
    export ZWWM_CONFIG=${sessionConfig}/config.zw
    export XCURSOR_PATH=$HOME/.icons:$HOME/.local/share/icons:/run/current-system/sw/share/icons
    export XCURSOR_THEME=${lib.escapeShellArg cfg.cursor.name}
    export XCURSOR_SIZE=${toString cfg.cursor.size}
    exec ${sessionZwwm}/bin/zwwm-session "$@"
  '';
  sessionPackage = pkgs.runCommand "blxshell-session" { passthru.providedSessions = [ "blxshell" ]; } ''
    mkdir -p $out/share/wayland-sessions
    cat > $out/share/wayland-sessions/blxshell.desktop <<EOF
    [Desktop Entry]
    Name=blxshell
    Comment=The Runix desktop: blxshell on zwwm
    Exec=${sessionScript}
    Type=Application
    DesktopNames=zwwm
    EOF
    sed -i 's/^    //' $out/share/wayland-sessions/blxshell.desktop
  '';
  cursorType = types.submodule {
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
  };
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
    # Runix's sessions first: each starts through runix-session (a session
    # bus and audio); the first file of a name wins.
    export XDG_DATA_DIRS=/run/current-system/sessions/share:/run/current-system/sw/share
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

    cursor = mkOption {
      type = cursorType;
      default = { package = pkgs.bibata-cursors; name = "Bibata-Modern-Ice"; size = 24; };
      defaultText = lib.literalExpression ''{ package = pkgs.bibata-cursors; name = "Bibata-Modern-Ice"; size = 24; }'';
      description = ''
        The pointer: installed, worn in the blxshell session and the seeded
        Hyprland config, and on the login screen unless greeter.cursor says
        otherwise.
      '';
    };

    seedConfigs = mkOption {
      type = types.bool;
      default = true;
      description = ''
        Give every user without a Hyprland configuration one set up for
        blxshell: it starts the shell and binds its launcher, lock screen,
        power menu, wallpapers and settings. It is copied once, at
        installation or the next boot, and never overwritten after that.
        zwwm needs none: the blxshell session brings its own.
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
        type = types.nullOr cursorType;
        # Without a theme zwwm has no cursor to draw.
        default = cfg.cursor;
        defaultText = lib.literalExpression "config.programs.blxshell.cursor";
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
      runix.packages = [ cfg.package pkgs.matugen cfg.cursor.package ];
      fonts.packages = cfg.fonts;
    })

    (mkIf (cfg.enable && config.programs.zwwm.enable) {
      # SDDM lists sessionPackages; the greeter reads the system profile.
      runix.wayland.sessionPackages = [ sessionPackage ];
      runix.packages = [ sessionPackage ];
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

      # One login screen: the greeter replaces SDDM, which is then not built.
      runix.desktop.sddm.enable = false;

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

      };
    })
  ];
}
