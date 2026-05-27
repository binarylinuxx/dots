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
          cli11
          jemalloc
          libdrm
          libxcb
          mesa
          pam
          pipewire
          polkit
          qt6.qtbase
          qt6.qtdeclarative
          qt6.qtsvg
          qt6.qtshadertools
          qt6.qtwayland
          spirv-tools
          sysprof
          wayland
          wayland-protocols
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

      blxshell-unwrapped = pkgs.writeShellApplication {
        name = "blxshell-unwrapped";
        runtimeInputs = [ blxshell-quickshell-git pkgs.uv pkgs.procps ];
        text = ''
          usage() {
            printf '%s\n' \
              "Usage: blxshell <command> [args]" \
              "" \
              "Commands:" \
              "  start           Launch quickshell with this config" \
              "  log             Attach to running instance log" \
              "  reload          Trigger a soft reload of the running instance" \
              "  restart         Restart quickshell with this config" \
              "  theme <image>   Generate and apply a color theme from a wallpaper image" \
              "  lock            Lock screen" \
              "  powermenu       Toggle the powermenu" \
              "  help            Show this message" \
              "" \
              "Environment:" \
              "  BLXSHELL_PATH   Override shell root"
          }

          shell_qml="$BLXSHELL_PATH/shell.qml"

          case "''${1:-start}" in
            start) exec qs -p "$shell_qml" ;;
            log) exec qs -p "$shell_qml" log ;;
            reload) exec qs -p "$shell_qml" ipc call shell reload ;;
            restart)
              pkill -x qs 2>/dev/null || true
              sleep 0.3
              exec qs -p "$shell_qml"
              ;;
            theme)
              if [[ -z "''${2:-}" ]]; then
                echo "Usage: blxshell theme <image_path>" >&2
                exit 1
              fi
              exec uv run --project "$BLXSHELL_PATH/col_gen" "$BLXSHELL_PATH/col_gen/main.py" "$2"
              ;;
            lock) exec qs -p "$shell_qml" ipc call lockscreen lock ;;
            powermenu) exec qs -p "$shell_qml" ipc call -- powermenu toggle ;;
            help|--help|-h) usage ;;
            *) exec qs -p "$shell_qml" "$@" ;;
          esac
        '';
      };

      blxshell-cli = pkgs.stdenvNoCC.mkDerivation {
        pname = "blxshell-cli";
        version = "0-unstable";
        dontUnpack = true;
        nativeBuildInputs = [ pkgs.makeWrapper ];

        installPhase = ''
          runHook preInstall
          makeWrapper ${blxshell-unwrapped}/bin/blxshell-unwrapped "$out/bin/blxshell" \
            --set BLXSHELL_PATH ${blxshell-runtime}/share/blxshell \
            --set-default BLXSHELL_PLUGIN_DIRS "\$HOME/.local/share/blxshell/plugins" \
            --set QML_IMPORT_PATH ${blxshell-runtime}/share/blxshell/qml \
            --set LD_LIBRARY_PATH ${blxshell-runtime}/share/blxshell/qml/PluginManager \
            --prefix PATH : ${lib.makeBinPath [ blxshell-quickshell-git pkgs.uv pkgs.procps ]}
          runHook postInstall
        '';

        meta.mainProgram = "blxshell";
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

      blxshell-bundle = pkgs.symlinkJoin {
        name = "blxshell-bundle";
        paths = [ blxshell-cli blxshell-plugin-manager blxshell-runtime fonts.blxshell-custom-fonts scripts.blxshell-scripts ];
        meta.mainProgram = "blxshell";
      };
    in rec {
      inherit blxshell-bundle blxshell-cli blxshell-greeter-launcher blxshell-plugin-manager blxshell-quickshell-git blxshell-runtime blxshell-unwrapped;
      inherit (fonts) blxshell-custom-fonts blxshell-font-bitcount blxshell-font-googlesans blxshell-font-material-symbols blxshell-font-readex-pro blxshell-font-rubik blxshell-font-space-grotesk blxshell-fonts;
      inherit (scripts) blxshell-scripts randwall wlshot;

      blxshell-shell = pkgs.symlinkJoin { name = "blxshell-shell"; paths = with pkgs; [ uv python3 python3Packages.pillow fonts.blxshell-custom-fonts ]; };
      quickshell = blxshell-quickshell-git;
      blxshell = pkgs.symlinkJoin {
        name = "blxshell";
        paths = [ blxshell-bundle blxshell-quickshell-git ];
        meta.mainProgram = "blxshell";
      };
      default = blxshell;
    });

  overlays.default = final: prev:
    let packages = self.packages.${prev.system};
    in {
      inherit (packages) blxshell-bundle blxshell-cli blxshell-custom-fonts blxshell-font-bitcount blxshell-font-googlesans blxshell-font-material-symbols blxshell-font-readex-pro blxshell-font-rubik blxshell-font-space-grotesk blxshell-fonts blxshell-greeter-launcher blxshell-plugin-manager blxshell-quickshell-git blxshell-runtime blxshell-scripts blxshell-shell blxshell-unwrapped quickshell randwall wlshot;
      blxshell = packages.default;
    };

  homeManagerModules.default = import ./modules/home.nix self;
  nixosModules.default = import ./modules/nixos.nix self;

  devShells = forAllSystems (system:
    let pkgs = nixpkgs.legacyPackages.${system};
    in {
      default = pkgs.mkShell {
        packages = with pkgs; [ cmake ninja python3 python3Packages.pillow qt6.qtbase qt6.qtdeclarative qt6.qtshadertools uv ];
        BLXSHELL_PATH = "$PWD/.config/quickshell";
        QML_IMPORT_PATH = "$PWD/.config/quickshell/qml";
      };
    });
}
