{ self, nixpkgs }:

let
  systems = [ "x86_64-linux" "aarch64-linux" ];
  forAllSystems = nixpkgs.lib.genAttrs systems;
in {
  packages = forAllSystems (system:
    let
      pkgs = nixpkgs.legacyPackages.${system};
      lib = pkgs.lib;
      fonts = import ./packages/fonts.nix { inherit pkgs lib; };
      scripts = import ./packages/scripts.nix { inherit pkgs lib; };

      pythonColGenEnv = pkgs.python314.withPackages (ps: [
        ps.jinja2 ps.materialyoucolor ps.numpy ps.opencv-python ps.pillow
      ]);

      blxshell-runtime = pkgs.stdenvNoCC.mkDerivation {
        pname = "blxshell-runtime";
        version = "0-unstable";
        src = self;

        installPhase = ''
          runHook preInstall
          mkdir -p "$out/share/blxshell"
          cp -R .config/quickshell/. "$out/share/blxshell/"
          rm -rf \
            "$out/share/blxshell/.obsidian" \
            "$out/share/blxshell/.ruff_cache" \
            "$out/share/blxshell/col_gen/.venv" \
            "$out/share/blxshell/col_gen/__pycache__"
          find "$out/share/blxshell" -name '__pycache__' -type d -prune -exec rm -rf {} +
          find "$out/share/blxshell" \( -name '*_grim.png' -o -name 'shot-*.png' \) -delete
          rm -rf "$out/share/blxshell/qml/PluginManager"

          printf '%s\n' '#!/bin/bash' 'exec blxshell-col-gen "$@"' \
            > "$out/share/blxshell/col_gen/generate"
          printf '%s\n' '#!/bin/bash' \
            "exec ${pythonColGenEnv}/bin/python3 $out/share/blxshell/col_gen/analyze.py \"\$@\"" \
            > "$out/share/blxshell/col_gen/analyze"

          runHook postInstall
        '';
      };

      blxshell-quickshell-git = pkgs.stdenv.mkDerivation {
        pname = "blxshell-quickshell-git";
        version = "0.1.0-git-4b4fca3";

        src = pkgs.fetchFromGitHub {
          owner = "quickshell-mirror";
          repo = "quickshell";
          rev = "4b4fca3224ab977dc515ac0bb78d00b3dfa71e00";
          hash = "sha256-zTslhsxLqUlRTML506iougTGzyR38Fzhzn7t4KDEuuE=";
        };

        nativeBuildInputs = with pkgs; [ cmake ninja pkg-config qt6.wrapQtAppsHook ];
        buildInputs = with pkgs; [
          cli11 jemalloc libdrm libxcb mesa pam pipewire polkit
          qt6.qtbase qt6.qtdeclarative qt6.qtsvg qt6.qtshadertools qt6.qtwayland
          spirv-tools sysprof wayland wayland-protocols
        ];

        cmakeFlags = [
          "-DCMAKE_BUILD_TYPE=RelWithDebInfo"
          "-DDISTRIBUTOR=Nix flake (blxshell)"
          "-DDISTRIBUTOR_DEBUGINFO_AVAILABLE=NO"
          "-DINSTALL_QML_PREFIX=lib/qt6/qml"
          "-DCRASH_HANDLER=OFF"
        ];

        meta = {
          description = "Quickshell pinned commit build for blxshell";
          homepage = "https://git.outfoxxed.me/quickshell/quickshell";
          license = lib.licenses.lgpl3Only;
          mainProgram = "quickshell";
          platforms = lib.platforms.linux;
        };
      };

      blxshell-col-gen = pkgs.writeShellScriptBin "blxshell-col-gen" ''
        exec ${pythonColGenEnv}/bin/python3 "${blxshell-runtime}/share/blxshell/col_gen/main.py" "$@"
      '';

      blxshell-plugin-manager = pkgs.stdenv.mkDerivation {
        pname = "blxshell-plugin-manager";
        version = "0-unstable";
        src = ../.config/quickshell/plugin_manager;

        nativeBuildInputs = with pkgs; [ cmake ninja pkg-config qt6.wrapQtAppsHook ];
        buildInputs = with pkgs; [ qt6.qtbase qt6.qtdeclarative ];

        cmakeFlags = [
          "-DCMAKE_BUILD_TYPE=Release"
          "-DQS_CONFIG_DIR=${placeholder "out"}/share/blxshell"
        ];

        installPhase = ''
          runHook preInstall
          test -d "$out/share/blxshell/qml/PluginManager"
          runHook postInstall
        '';
      };

      blxshell-greeter-launcher = pkgs.stdenvNoCC.mkDerivation {
        pname = "blxshell-greeter-launcher";
        version = "0-unstable";
        src = ../.config/quickshell/greeter-wrapper;
        nativeBuildInputs = [ pkgs.makeWrapper ];

        installPhase = ''
          runHook preInstall
          install -Dm755 build/greeter-launcher "$out/libexec/greeter-launcher"
          makeWrapper "$out/libexec/greeter-launcher" "$out/bin/greeter-launcher" \
            --set BLXSHELL_PATH ${blxshell-runtime}/share/blxshell \
            --prefix PATH : ${lib.makeBinPath [ blxshell-quickshell-git pkgs.coreutils ]}
          runHook postInstall
        '';
      };

      blxshell = pkgs.writeShellApplication {
        name = "blxshell";
        runtimeInputs = [
          blxshell-quickshell-git
          blxshell-col-gen
          pkgs.playerctl
          pkgs.procps
          pkgs.pulseaudio
        ];

        text = ''
          QML_IMPORT_PATH="${blxshell-plugin-manager}/share/blxshell/qml"
          export QML_IMPORT_PATH
          LD_LIBRARY_PATH="${blxshell-plugin-manager}/share/blxshell/qml/PluginManager''${LD_LIBRARY_PATH:+:''$LD_LIBRARY_PATH}"
          export LD_LIBRARY_PATH
          SHELL_QML="${blxshell-runtime}/share/blxshell/shell.qml"

          usage() {
            cat <<'USAGE'
        Usage: blxshell <command> [args]

        Commands:
          start           Launch quickshell with blxshell config
          log             Attach to running instance log
          reload          Trigger a soft reload of the running instance
          restart         Restart quickshell
          theme <image>   Generate and apply a color theme from wallpaper
          lock            Lock screen
          powermenu       Toggle the powermenu
          help            Show this message
        USAGE
          }

          case "''${1:-start}" in
            start|launch)
              shift 2>/dev/null || true
              exec qs -p "$SHELL_QML" "$@"
              ;;
            log)
              exec qs -p "$SHELL_QML" log
              ;;
            reload)
              exec qs -p "$SHELL_QML" ipc call shell reload
              ;;
            restart)
              pkill -x qs 2>/dev/null || true
              sleep 0.3
              exec qs -p "$SHELL_QML"
              ;;
            theme)
              if [[ $# -lt 2 ]]; then
                echo "Usage: blxshell theme <image_path>" >&2
                exit 1
              fi
              shift
              exec blxshell-col-gen image "$@"
              ;;
            lock)
              exec qs -p "$SHELL_QML" ipc call lockscreen lock
              ;;
            powermenu)
              exec qs -p "$SHELL_QML" ipc call -- powermenu toggle
              ;;
            help|--help|-h)
              usage
              ;;
            *)
              exec qs -p "$SHELL_QML" "$@"
              ;;
          esac
        '';

        meta.mainProgram = "blxshell";
      };

      blxshell-bundle = pkgs.symlinkJoin {
        name = "blxshell-bundle";
        paths = [
          blxshell
          blxshell-col-gen
          blxshell-plugin-manager
          blxshell-runtime
          fonts.blxshell-custom-fonts
          scripts.blxshell-scripts
        ];
        meta.mainProgram = "blxshell";
      };
    in rec {
      inherit
        blxshell
        blxshell-bundle
        blxshell-col-gen
        blxshell-greeter-launcher
        blxshell-plugin-manager
        blxshell-quickshell-git
        blxshell-runtime;
      inherit (fonts)
        blxshell-custom-fonts
        blxshell-font-bitcount
        blxshell-font-googlesans
        blxshell-font-material-symbols
        blxshell-font-readex-pro
        blxshell-font-rubik
        blxshell-font-space-grotesk
        blxshell-fonts;
      inherit (scripts)
        blxshell-scripts
        randwall
        wlshot;

      blxshell-shell = pkgs.symlinkJoin {
        name = "blxshell-shell";
        paths = with pkgs; [ uv python3 python3Packages.pillow fonts.blxshell-custom-fonts ];
      };
      quickshell = blxshell-quickshell-git;
      default = blxshell;
    });

  overlays.default = final: prev:
    let packages = self.packages.${prev.system};
    in {
      inherit (packages)
        blxshell-bundle
        blxshell-col-gen
        blxshell-custom-fonts
        blxshell-font-bitcount
        blxshell-font-googlesans
        blxshell-font-material-symbols
        blxshell-font-readex-pro
        blxshell-font-rubik
        blxshell-font-space-grotesk
        blxshell-fonts
        blxshell-greeter-launcher
        blxshell-plugin-manager
        blxshell-quickshell-git
        blxshell-runtime
        blxshell-scripts
        blxshell-shell
        quickshell
        randwall
        wlshot;
      blxshell = packages.default;
    };

  homeManagerModules.default = import ./modules/home.nix self;
  nixosModules.default = import ./modules/nixos.nix self;

  devShells = forAllSystems (system:
    let pkgs = nixpkgs.legacyPackages.${system};
    in {
      default = pkgs.mkShell {
        packages = with pkgs; [ cmake ninja python3 python3Packages.pillow qt6.qtbase qt6.qtdeclarative qt6.qtshadertools uv ];
        QML_IMPORT_PATH = "$PWD/.config/quickshell/qml";
      };
    });
}
