# The fonts blxshell draws with: Google Sans Flex, Rubik and Material Symbols,
# plus Readex Pro, Space Grotesk and Bitcount. blxshell-custom-fonts is all of them.
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

  # URLs name a commit, never a branch: a branch moves and breaks the hash.
  fetchFont = url: hash: pkgs.fetchurl { inherit url hash; };

  blxshell-font-bitcount = fontPackage {
    pname = "blxshell-font-bitcount";
    files = [
      { name = "BitcountSingle.ttf"; src = fetchFont "https://github.com/petrvanblokland/TYPETR-Bitcount/raw/89e7994f73b7f5ced80e7cf493d40be9e66ff82f/fonts/ttf/variable/BitcountSingle%5BCRSV%2CELSH%2CELXP%2Cslnt%2Cwght%5D.ttf" "sha256-IkdyOSKZTI5/xRuYS2Mzla6Fx3VcPL8N7Aygsd1J+rs="; }
      { name = "BitcountGridSingle.ttf"; src = fetchFont "https://github.com/petrvanblokland/TYPETR-Bitcount/raw/89e7994f73b7f5ced80e7cf493d40be9e66ff82f/fonts/ttf/variable/BitcountGridSingle%5BCRSV%2CELSH%2CELXP%2Cslnt%2Cwght%5D.ttf" "sha256-gmfZpGoTkzzaMNxbGqHB9aJCoK/13hhA3nwFbMOrSd0="; }
    ];
    meta.description = "Bitcount Single variable pixel fonts for blxshell";
  };

  blxshell-font-googlesans = fontPackage {
    pname = "blxshell-font-googlesans";
    files = [{ name = "GoogleSansFlex-Regular.ttf"; src = fetchFont "https://github.com/LineageOS/android_external_google-fonts_google-sans-flex/raw/d4acee8466031f21beac05367a390e2c080ac7aa/GoogleSansFlex-Regular.ttf" "sha256-JRCot6JL6x/oFj6aSYE8z+lrVFNES5RD1CZlyk+jIMk="; }];
    meta.description = "Google Sans Flex variable typeface for blxshell";
  };

  # From nixpkgs: a fixed version, served from its binary cache. Fetching
  # from the repository's master branch broke whenever Google updated it.
  blxshell-font-material-symbols = pkgs.material-symbols;

  blxshell-font-readex-pro = fontPackage {
    pname = "blxshell-font-readex-pro";
    files = [{ name = "ReadexPro.ttf"; src = fetchFont "https://github.com/ThomasJockin/readexpro/raw/563dfbb36ae45e52ec50829b016ce724ac2fca70/fonts/variable/Readexpro%5BHEXP%2Cwght%5D.ttf" "sha256-Jou6fh6POxTXmLP7DkDrqj/DkwjJrAAg4vr23xgcww4="; }];
    meta.description = "Readex Pro variable typeface for blxshell";
  };

  blxshell-font-rubik = pkgs.rubik;

  blxshell-font-space-grotesk = fontPackage {
    pname = "blxshell-font-space-grotesk";
    files = [{ name = "SpaceGrotesk.ttf"; src = fetchFont "https://github.com/floriankarsten/space-grotesk/raw/03507d024a01282884232081fc6011c09ff4e849/fonts/ttf/SpaceGrotesk%5Bwght%5D.ttf" "sha256-rK1t4fyTQ29cDx9BN3Ue8E8a6jBj5wNlNZcP/PvXn3I="; }];
    meta.description = "Space Grotesk variable typeface for blxshell";
  };
in rec {
  inherit blxshell-font-bitcount blxshell-font-googlesans blxshell-font-material-symbols blxshell-font-readex-pro blxshell-font-rubik blxshell-font-space-grotesk;

  blxshell-custom-fonts = pkgs.symlinkJoin {
    name = "blxshell-custom-fonts";
    paths = [ blxshell-font-bitcount blxshell-font-googlesans blxshell-font-material-symbols blxshell-font-readex-pro blxshell-font-rubik blxshell-font-space-grotesk ];
  };
}
