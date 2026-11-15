#!/bin/sh
# OpenRA - Red Alert, installed by emulationstation-re.
#
# The Arch package provides:
#   openra-ra   Red Alert
#   openra-cnc  Tiberian Dawn
#   openra-d2k  Dune 2000
# This port launches the first one it finds (Red Alert when the full package
# is installed). Copy this file and change the binary to launch another mod.
#
# Without the native package, the Flathub app is used when installed.

set -u

for game in openra-ra openra-cnc openra-d2k; do
    if command -v "$game" >/dev/null 2>&1; then
        exec "$game" "$@"
    fi
done

if command -v flatpak >/dev/null 2>&1 && flatpak info net.openra.OpenRA >/dev/null 2>&1; then
    exec flatpak run net.openra.OpenRA "$@"
fi

message="OpenRA is not installed.
Install it with your package manager (Arch: openra), or from Flathub:
flatpak install flathub net.openra.OpenRA"

if command -v zenity >/dev/null 2>&1; then
    zenity --error --title="OpenRA" --text="$message"
else
    printf '%s\n' "$message" >&2
fi

exit 1
