{ pkgs, lib }:

let
  fontPackage = { pname, files, meta ? { } }:
    pkgs.stdenvNoCC.mkDerivation {
      inherit pname;
      version = "1.0";
      dontUnpack = true;

      installPhase = ''
        runHook preInstall
        mkdir -p "$out/share/fonts/truetype/blxshell"
      '' + lib.concatMapStringsSep "\n" (file: ''
        install -Dm644 ${file.src} "$out/share/fonts/truetype/blxshell/${file.name}"
      '') files + ''
        runHook postInstall
      '';

      meta = {
        platforms = lib.platforms.all;
      } // meta;
    };

  fetchFont = url: hash: pkgs.fetchurl { inherit url hash; };

  blxshell-fonts = pkgs.stdenvNoCC.mkDerivation {
    pname = "blxshell-fonts";
    version = "0-unstable";
    src = ../../.config/quickshell/fonts;

    installPhase = ''
      runHook preInstall
      install -Dm644 *.ttf -t "$out/share/fonts/truetype/blxshell"
      runHook postInstall
    '';
  };

  blxshell-font-bitcount = fontPackage {
    pname = "blxshell-font-bitcount";
    files = [
      { name = "BitcountSingle.ttf"; src = fetchFont "https://github.com/petrvanblokland/TYPETR-Bitcount/raw/main/fonts/ttf/variable/BitcountSingle%5BCRSV%2CELSH%2CELXP%2Cslnt%2Cwght%5D.ttf" "sha256-IkdyOSKZTI5/xRuYS2Mzla6Fx3VcPL8N7Aygsd1J+rs="; }
      { name = "BitcountGridSingle.ttf"; src = fetchFont "https://github.com/petrvanblokland/TYPETR-Bitcount/raw/main/fonts/ttf/variable/BitcountGridSingle%5BCRSV%2CELSH%2CELXP%2Cslnt%2Cwght%5D.ttf" "sha256-gmfZpGoTkzzaMNxbGqHB9aJCoK/13hhA3nwFbMOrSd0="; }
    ];
    meta.description = "Bitcount Single variable pixel fonts for blxshell";
  };

  blxshell-font-googlesans = fontPackage {
    pname = "blxshell-font-googlesans";
    files = [{ name = "GoogleSansFlex-Regular.ttf"; src = fetchFont "https://github.com/LineageOS/android_external_google-fonts_google-sans-flex/raw/lineage-23.0/GoogleSansFlex-Regular.ttf" "sha256-JRCot6JL6x/oFj6aSYE8z+lrVFNES5RD1CZlyk+jIMk="; }];
    meta.description = "Google Sans Flex variable typeface for blxshell";
  };

  blxshell-font-material-symbols = fontPackage {
    pname = "blxshell-font-material-symbols";
    files = [
      { name = "MaterialSymbolsOutlined.ttf"; src = fetchFont "https://github.com/google/material-design-icons/raw/master/variablefont/MaterialSymbolsOutlined%5BFILL%2CGRAD%2Copsz%2Cwght%5D.ttf" "sha256-b4PAXYghIvz1F8RKiJDlKstqyKPgCOWM0RKVLZ31ya8="; }
      { name = "MaterialSymbolsRounded.ttf"; src = fetchFont "https://github.com/google/material-design-icons/raw/master/variablefont/MaterialSymbolsRounded%5BFILL%2CGRAD%2Copsz%2Cwght%5D.ttf" "sha256-L8mYt9pEAhFK/5SHiaBQ4e5WSa27CYvtWQ0V/j0OP/0="; }
      { name = "MaterialSymbolsSharp.ttf"; src = fetchFont "https://github.com/google/material-design-icons/raw/master/variablefont/MaterialSymbolsSharp%5BFILL%2CGRAD%2Copsz%2Cwght%5D.ttf" "sha256-xVilKBXX+cHyOLy84RP1Z4D9SRd4XSgPx4l3yP58lrQ="; }
    ];
    meta.description = "Google Material Symbols variable icon fonts for blxshell";
  };

  blxshell-font-readex-pro = fontPackage {
    pname = "blxshell-font-readex-pro";
    files = [{ name = "ReadexPro.ttf"; src = fetchFont "https://github.com/ThomasJockin/readexpro/raw/master/fonts/variable/Readexpro%5BHEXP%2Cwght%5D.ttf" "sha256-Jou6fh6POxTXmLP7DkDrqj/DkwjJrAAg4vr23xgcww4="; }];
    meta.description = "Readex Pro variable typeface for blxshell";
  };

  blxshell-font-rubik = fontPackage {
    pname = "blxshell-font-rubik";
    files = [
      { name = "Rubik.ttf"; src = fetchFont "https://github.com/googlefonts/rubik/raw/main/fonts/variable/Rubik%5Bwght%5D.ttf" "sha256-Gzp0N7oq+A5GXnc+1gxQNtG6as5JLYkEbbzxj7MeTog="; }
      { name = "Rubik-Italic.ttf"; src = fetchFont "https://github.com/googlefonts/rubik/raw/main/fonts/variable/Rubik-Italic%5Bwght%5D.ttf" "sha256-CMbEAYpa2otRdAe0aJfkbPbrsQaFP70+ia3bUdO1nGI="; }
    ];
    meta.description = "Rubik variable typeface for blxshell";
  };

  blxshell-font-space-grotesk = fontPackage {
    pname = "blxshell-font-space-grotesk";
    files = [{ name = "SpaceGrotesk.ttf"; src = fetchFont "https://github.com/floriankarsten/space-grotesk/raw/master/fonts/ttf/SpaceGrotesk%5Bwght%5D.ttf" "sha256-rK1t4fyTQ29cDx9BN3Ue8E8a6jBj5wNlNZcP/PvXn3I="; }];
    meta.description = "Space Grotesk variable typeface for blxshell";
  };
in rec {
  inherit blxshell-font-bitcount blxshell-font-googlesans blxshell-font-material-symbols blxshell-font-readex-pro blxshell-font-rubik blxshell-font-space-grotesk blxshell-fonts;

  blxshell-custom-fonts = pkgs.symlinkJoin {
    name = "blxshell-custom-fonts";
    paths = [ blxshell-font-bitcount blxshell-font-googlesans blxshell-font-material-symbols blxshell-font-readex-pro blxshell-font-rubik blxshell-font-space-grotesk blxshell-fonts ];
  };
}
