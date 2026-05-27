{
  description = "blxshell dotfiles, runtime packages, and Home Manager module";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    import ./nix-deps { inherit self nixpkgs; };
}
