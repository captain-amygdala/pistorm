{
  outputs = { self, nixpkgs }:
  let
    hostPkgs = import nixpkgs { system = "x86_64-linux"; };
    overlay = self: super: {
      pistorm = self.callPackage ./pistorm.nix {};
      e2fsprogs = null;
      libraspberrypi = super.libraspberrypi.overrideAttrs (old: { patches = [ ./userland.patch ]; });
      pistorm-tar = hostPkgs.callPackage (nixpkgs + "/nixos/lib/make-system-tarball.nix") {
        contents = [];
        storeContents = [
          {
            object = self.pistorm;
            symlink = "/pistorm";
          }
        ];
      };
    };
  in {
    packages.aarch64-linux =
    let
      pkgs = import nixpkgs { system = "aarch64-linux"; overlays = [ overlay ]; };
    in {
      inherit (pkgs) pistorm;
    };
    packages.armv7l-linux =
    let
      pkgs = import nixpkgs { system = "armv7l-linux"; overlays = [ overlay ]; };
    in {
      inherit (pkgs) pistorm pistorm-tar;
    };
    hydraJobs.armv7l-linux = {
      inherit (self.packages.armv7l-linux) pistorm pistorm-tar;
    };
  };
}
