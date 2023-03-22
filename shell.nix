# This is a nix-shell for use with the nix package manager.
# If you have nix installed, you may simply run `nix-shell`
# in this repo, and have all dependencies ready in the new shell.

{ pkgs ? import <nixpkgs> {} }:
pkgs.mkShell {
  buildInputs = with pkgs;
    [
      cmake
      python3
      python39Packages.orderedmultidict

      # for UHDM_USE_HOST_{CAPNP,GTEST}
      capnproto
      gtest

      # Ease development
      ccache
      ninja
      clang-tools
    ];
  shellHook =
  ''
    export CMAKE_CXX_COMPILER_LAUNCHER=ccache

    # Use host version by default.
    export ADDITIONAL_CMAKE_OPTIONS="-DUHDM_USE_HOST_GTEST=On"
    export ADDITIONAL_CMAKE_OPTIONS="$ADDITIONAL_CMAKE_OPTIONS -DUHDM_USE_HOST_CAPNP=On"
  '';
}
