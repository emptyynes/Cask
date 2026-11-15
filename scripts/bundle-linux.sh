#!/usr/bin/env bash
# Делает из build/Cask портативную папку: кладёт Qt и плагины в lib/ и plugins/,
# чтобы Cask запускался без установленного Qt.
# Использование: scripts/bundle-linux.sh build/Cask
set -euo pipefail

DIR="$(cd "$1" && pwd)"
BIN="$DIR/cask"
LIB="$DIR/lib"
PLUG="$DIR/plugins"
mkdir -p "$LIB" "$PLUG/platforms" "$PLUG/xcbglintegrations"

QT_PLUGINS="$(find /usr/lib -type d -path '*/qt6/plugins' | head -n1)"
[ -n "$QT_PLUGINS" ] || { echo "Qt plugins dir not found" >&2; exit 1; }

# Платформенные плагины
for p in libqxcb.so libqoffscreen.so libqwayland-generic.so; do
  [ -f "$QT_PLUGINS/platforms/$p" ] && cp -L "$QT_PLUGINS/platforms/$p" "$PLUG/platforms/"
done
cp -L "$QT_PLUGINS"/xcbglintegrations/*.so "$PLUG/xcbglintegrations/" 2>/dev/null || true

# Какие библиотеки забираем с собой (остальное, например glibc/libGL, берётся из системы)
want() {
  case "$(basename "$1")" in
    libQt6*|libicu*|libdouble-conversion*|libpcre2-16*|libmd4c*|libb2*) return 0 ;;
    libxcb-cursor*|libxcb-icccm*|libxcb-image*|libxcb-keysyms*|libxcb-render-util*|libxcb-shape*|libxcb-xkb*|libxkbcommon-x11*) return 0 ;;
    *) return 1 ;;
  esac
}

collect() {
  ldd "$1" 2>/dev/null | awk '/=> \//{print $3}' | while read -r so; do
    if want "$so" && [ ! -f "$LIB/$(basename "$so")" ]; then
      cp -L "$so" "$LIB/"
    fi
  done
}

# Несколько проходов, чтобы подтянуть зависимости зависимостей
for _ in 1 2 3; do
  collect "$BIN"
  for f in "$PLUG"/platforms/*.so "$PLUG"/xcbglintegrations/*.so "$LIB"/*.so*; do
    [ -f "$f" ] && collect "$f"
  done
done

# RPATH: бинарник ищет в ./lib, библиотеки рядом друг с другом, плагины в ../../lib
patchelf --set-rpath '$ORIGIN/lib' "$BIN"
for f in "$LIB"/*.so*; do patchelf --set-rpath '$ORIGIN' "$f"; done
for f in "$PLUG"/platforms/*.so "$PLUG"/xcbglintegrations/*.so; do
  [ -f "$f" ] && patchelf --set-rpath '$ORIGIN/../../lib' "$f"
done

# Qt ищет плагины рядом с бинарником
printf '[Paths]\nPlugins = plugins\n' > "$DIR/qt.conf"

if ldd "$BIN" | grep -q 'not found'; then
  echo "Unresolved libraries:" >&2
  ldd "$BIN" | grep 'not found' >&2
  exit 1
fi
echo "Bundled $(ls "$LIB" | wc -l) libraries into $LIB"
