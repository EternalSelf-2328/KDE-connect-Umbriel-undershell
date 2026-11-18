{
  description = "Umbriel Connect: private phone integration for Umbriel, Noctalia and undershell";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  inputs.engine-source = {
    url = "git+https://invent.kde.org/network/kdeconnect-kde.git?rev=642a710abd9dfff4358b5d943ca61965fb34ec6e";
    flake = false;
  };
  outputs = { self, nixpkgs, engine-source }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
      package = import ./packaging/nixos/package.nix {
        inherit pkgs;
        src = self;
        engineSource = engine-source;
      };
    in {
      packages.${system} = { default = package; umbriel-connect = package; };
      checks.${system}.package = package;
      nixosModules.default = import ./packaging/nixos/module.nix { inherit self; };
    };
}
