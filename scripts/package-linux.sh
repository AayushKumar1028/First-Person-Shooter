#!/usr/bin/env bash
#
# Builds a portable Linux copy of the game and packs it into a tarball.
#
#   ./scripts/package-linux.sh
#
# The result is dist/fps-shooter-linux-x86_64.tar.gz, which unpacks to a folder
# holding a single self-contained executable:
#
#   fps-shooter/
#     fps-shooter          <- run this
#     lib/                 <- bundled SDL2, found automatically via $ORIGIN
#     assets/              <- drop-in texture overrides live here
#     fps-shooter.desktop  <- install into the application menu if you like
#     README.txt
#
# Nothing needs to be installed on the target machine, and no environment
# variables have to be set: the executable carries its own rpath.

set -euo pipefail

cd "$(dirname "$0")/.."
ROOT="$PWD"

NAME="fps-shooter"
BUNDLE="$ROOT/dist/$NAME"
TARBALL="$ROOT/dist/$NAME-linux-x86_64.tar.gz"
BUILD="$ROOT/build-portable"

say() { printf '\033[1m==>\033[0m %s\n' "$*"; }

# --- 1. configure a portable build -------------------------------------------
say "Configuring ($BUILD)"
cmake -S "$ROOT" -B "$BUILD" \
  -DCMAKE_BUILD_TYPE=Release \
  -DFPS_PORTABLE=ON > /dev/null

say "Compiling"
cmake --build "$BUILD" -j "$(nproc 2>/dev/null || echo 2)"

# --- 2. lay out the bundle ---------------------------------------------------
rm -rf "$BUNDLE"
mkdir -p "$BUNDLE/lib" "$BUNDLE/assets"

say "Assembling bundle"
cp "$BUILD/fps_shooter" "$BUNDLE/$NAME"
strip --strip-unneeded "$BUNDLE/$NAME" 2>/dev/null || true

# Ship the SDL2 shared object next to the binary. zlib and the base C library
# are part of every mainstream distribution and are deliberately not bundled.
# ldconfig -p prints "soname (libc6,x86-64) => /path/to/lib", so the first field
# is the exact soname we are after.
copy_lib() {
  local soname="$1"
  local found
  found="$(ldconfig -p 2>/dev/null | awk -v want="$soname" '$1 == want { print $NF; exit }')"
  if [ -n "$found" ] && [ -f "$found" ]; then
    cp -L "$found" "$BUNDLE/lib/"
    return 0
  fi
  return 1
}

if copy_lib "libSDL2-2.0.so.0"; then
  say "Bundled libSDL2-2.0.so.0"
else
  say "WARNING: libSDL2-2.0.so.0 not found on this machine; the bundle will"
  say "         require the target system to provide SDL2."
fi

cp -R "$ROOT/assets/." "$BUNDLE/assets/" 2>/dev/null || true
# Generated art dumps shouldn't ship in the bundle.
rm -f "$BUNDLE"/assets/textures/*.png "$BUNDLE"/assets/textures/*.ppm 2>/dev/null || true

# --- 3. desktop entry + readme ----------------------------------------------
cat > "$BUNDLE/$NAME.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=FPS Shooter
Comment=Doom-style raycasting first-person shooter
Exec=fps-shooter
Terminal=false
Categories=Game;ActionGame;
DESKTOP

cat > "$BUNDLE/README.txt" <<'README'
FPS Shooter - portable Linux build
==================================

Run it:

    ./fps-shooter

Starting from the command line is worthwhile once, because --help lists the
useful options:

    ./fps-shooter --help
    ./fps-shooter --fullscreen
    ./fps-shooter --render 320x200     # chunkier pixels, faster on old CPUs
    ./fps-shooter --arena              # skip the campaign

The lib/ folder holds the SDL2 this build was compiled against; the executable
finds it automatically (via an $ORIGIN rpath), so do not move the binary out of
this folder on its own.

Custom textures: drop PNG/BMP/PPM files named after the art slots into
assets/textures/ and restart. Run

    ./fps-shooter --dump-textures

first to write out every built-in texture as an editable PNG to paint over.
README


# --- 4. tarball --------------------------------------------------------------
say "Creating $TARBALL"
tar -czf "$TARBALL" -C "$ROOT/dist" "$NAME"

# --- 5. prove it works -------------------------------------------------------
say "Verifying the packaged executable"
# --mem exercises asset generation, decoding and the reporting path without
# needing a display, so it is a real smoke test of the shipped binary.
if ! "$BUNDLE/$NAME" --mem --quiet | grep -q '=== MEMORY ==='; then
  echo "verification failed: the packaged binary did not run" >&2
  exit 1
fi
if ! "$BUNDLE/$NAME" --selftest --quiet | grep -q 'selftest PASSED'; then
  echo "verification failed: the packaged binary's self-test did not pass" >&2
  exit 1
fi
if ldd "$BUNDLE/$NAME" | grep -q 'not found'; then
  echo "verification failed: unresolved shared libraries" >&2
  ldd "$BUNDLE/$NAME" >&2
  exit 1
fi
# The whole point of the bundle is that it does not pick up the host's SDL2, so
# check it is really resolving the copy in lib/.
if ! ldd "$BUNDLE/$NAME" | grep -q "$BUNDLE/lib/libSDL2"; then
  echo "verification failed: the binary is not using the bundled SDL2" >&2
  ldd "$BUNDLE/$NAME" >&2
  exit 1
fi

say "Done"
printf '\n  %s\n  size: %s\n\n' "$TARBALL" "$(du -h "$TARBALL" | cut -f1)"
printf 'On another machine:\n  tar -xzf %s && ./%s/%s\n\n' \
  "$(basename "$TARBALL")" "$NAME" "$NAME"
