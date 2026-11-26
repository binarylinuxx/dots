{
  description = "blxshell: a Wayland desktop shell built on libvw";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    vw = {
      url = "github:binarylinuxx/vw";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, vw }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in {
      packages = forAllSystems (system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          fonts = import ./nix/fonts.nix { inherit pkgs; inherit (pkgs) lib; };
        in fonts // {
          default = vw.lib.mkShell {
            inherit system;
            pname = "blxshell";
            # Facet's interpreter (vw::facet) runs plugins' views.
            facet = true;
            # .blx packages are zstd tarballs (src/packages.cpp).
            shellDeps = [ pkgs.zstd ];
            # Only what the build reads, so editing the dotfiles in .config
            # doesn't rebuild the shell.
            src = nixpkgs.lib.fileset.toSource {
              root = ./.;
              fileset = nixpkgs.lib.fileset.unions [ ./CMakeLists.txt ./src ./shaders ./include ./packages ./schemas ./share ./wallpapers/runix-m3.png ./icons/runix-logo.svg ./translations ];
            };
          };
        });

      # programs.blxshell: the shell, its fonts and app theming (nix/shell.nix);
      # programs.blxshell.greeter: the login screen (nix/greeter.nix).
      nixosModules.default = {
        imports = [ (import ./nix/shell.nix self) (import ./nix/greeter.nix self) ];
      };

      # programs.blxshell for Runix (nix/runix-module.nix): fonts, and the
      # greeter as a runit service.
      runixModules.default = import ./nix/runix-module.nix self;

      apps = forAllSystems (system: {
        default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/blxshell";
        };
      });

      # Builds vw from ../vw (VW_SOURCE_DIR) so both can be edited together.
      devShells = forAllSystems (system:
        let pkgs = nixpkgs.legacyPackages.${system};
        in {
          default = (pkgs.mkShell.override { stdenv = pkgs.clangStdenv; }) {
            inputsFrom = [ vw.devShells.${system}.default ];
            VW_SOURCE_DIR = "/home/blx/vw";
            # inputsFrom carries packages, not environment variables.
            ZWAYLAND_ROOT = "${vw.inputs.zwwm}";
            packages = with pkgs; [ cmake ninja zstd ];
          };
        });
    };
}
