{ pkgs, lib }:

let
  scriptPackage = { pname, url, hash, runtimeInputs ? [ ] }:
    pkgs.stdenvNoCC.mkDerivation {
      inherit pname;
      version = "0-unstable";
      dontUnpack = true;
      nativeBuildInputs = [ pkgs.makeWrapper ];
      src = pkgs.fetchurl { inherit url hash; };

      installPhase = ''
        runHook preInstall
        install -Dm755 "$src" "$out/bin/${pname}"
        wrapProgram "$out/bin/${pname}" --prefix PATH : ${lib.makeBinPath runtimeInputs}
        runHook postInstall
      '';

      meta.mainProgram = pname;
    };

  randwall = scriptPackage {
    pname = "randwall";
    runtimeInputs = with pkgs; [ coreutils findutils libnotify ];
    url = "https://codeberg.org/blx/blx-shell/raw/branch/main/scripts/randwall";
    hash = "sha256-BnkDa8Yr5RYyw9dUvCjYPkTJ/CR6X5WZA8a92UfoUk8=";
  };

  wlshot = scriptPackage {
    pname = "wlshot";
    runtimeInputs = with pkgs; [ grim slurp wl-clipboard ];
    url = "https://raw.githubusercontent.com/binarylinuxx/wlshot/refs/heads/main/wlshot";
    hash = "sha256-tfMtlKxi3KLuESwQfZky5Q5zZUq/Pz+ZDmyxLJUj1iA=";
  };
in {
  inherit randwall wlshot;

  blxshell-scripts = pkgs.symlinkJoin {
    name = "blxshell-scripts";
    paths = [ randwall wlshot ];
  };
}
