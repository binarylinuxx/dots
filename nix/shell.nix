# programs.blxshell: the shell, its fonts, and what Settings → Theme → Apps
# needs for GTK and Qt apps (qt5ct/qt6ct, adw-gtk3).
self:
{ config, lib, pkgs, ... }:

let
  cfg = config.programs.blxshell;
  inherit (lib) mkDefault mkEnableOption mkIf mkMerge mkOption types;
  system = pkgs.stdenv.hostPlatform.system;
in {
  options.programs.blxshell = {
    enable = mkEnableOption "blxshell";
    package = mkOption {
      type = types.package;
      default = self.packages.${system}.default;
      defaultText = lib.literalExpression "blxshell.packages.\${system}.default";
    };
    fonts = mkOption { type = types.bool; default = true; };
    appTheming = mkOption { type = types.bool; default = true; };
  };

  config = mkIf cfg.enable (mkMerge [
    {
      environment.systemPackages = [ cfg.package ];
      fonts.packages = mkIf cfg.fonts [ self.packages.${system}.blxshell-custom-fonts ];
    }
    (mkIf cfg.appTheming {
      qt = { enable = mkDefault true; platformTheme = mkDefault "qt5ct"; };
      environment.systemPackages = [ pkgs.adw-gtk3 ];
    })
  ]);
}
