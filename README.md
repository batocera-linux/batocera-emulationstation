EmulationStation-RE
===================

[![Arch Linux x86_64](https://github.com/JyuHo/emulationstation-re/actions/workflows/build-arch.yml/badge.svg?branch=master)](https://github.com/JyuHo/emulationstation-re/actions/workflows/build-arch.yml)
[![Debian amd64](https://github.com/JyuHo/emulationstation-re/actions/workflows/build-debian-amd64.yml/badge.svg?branch=master)](https://github.com/JyuHo/emulationstation-re/actions/workflows/build-debian-amd64.yml)
[![Debian arm64](https://github.com/JyuHo/emulationstation-re/actions/workflows/build-debian-arm64.yml/badge.svg?branch=master)](https://github.com/JyuHo/emulationstation-re/actions/workflows/build-debian-arm64.yml)
[![Code quality](https://github.com/JyuHo/emulationstation-re/actions/workflows/quality.yml/badge.svg?branch=master)](https://github.com/JyuHo/emulationstation-re/actions/workflows/quality.yml)
[![CodeQL](https://github.com/JyuHo/emulationstation-re/actions/workflows/codeql.yml/badge.svg?branch=master)](https://github.com/JyuHo/emulationstation-re/actions/workflows/codeql.yml)
[![Latest release](https://img.shields.io/github/v/release/JyuHo/emulationstation-re)](https://github.com/JyuHo/emulationstation-re/releases/latest)

EmulationStation-RE is a graphical front-end for emulators with controller navigation, for
Linux only. It started from the Batocera fork of EmulationStation and was reworked:

* **C++23 and SDL3** (SDL 3.2+ and SDL_image 3.2+), with built-in audio decoding (MP3, Ogg Vorbis, FLAC, WAV) instead of SDL_mixer.
* **One OpenGL renderer**, built for desktop OpenGL 3.3 core (x86_64) or for OpenGL ES 3.x (Raspberry Pi 4 and later).
* **XDG folders**: settings in `~/.config`, data in `~/.local/share`, cache, logs and runtime files in their own XDG folders (see [Where files live](#where-files-live)).
* **es-launcher**: native C++ generators that configure each emulator (controllers, video, per-game options) and build the command that starts it, in place of Batocera's Python `batocera-launch` (see [Game launcher](#game-launcher)).
* **Emulator and theme catalog**: the **Emulator catalog** menu and **Updates & downloads > Themes** install emulators, libretro cores and themes listed by [es-re-catalog](https://github.com/JyuHo/es-re-catalog).
* **The Bezel Project**: **Updates & downloads > The Bezel Project** downloads, system by system, the bezels of the games you have straight from [The Bezel Project](https://github.com/thebezelproject)'s GitHub repositories (nothing of it is shipped with ES), into the `thebezelproject` decoration set, as Batocera does. It uses Batocera's lists of bezels; copies of them in `~/.config/emulationstation/thebezelproject/` (`tbp_systems.txt`, `tbp_<system>.txt`) take precedence.
* **Updates from GitHub releases**: Arch Linux and Debian packages are published for each `vX.Y.Z` tag, and ES installs them from **Updates & downloads > Check for updates** (see [Installing and running](#installing-and-running)).
* **Web server** on port 1234, for local tools and remote control. It only listens on `127.0.0.1` unless **Enable public web API access** is turned on, then it answers on `http://<host>:1234`. Its pages show a dashboard (systems, game counts, the running game), a game browser that starts games, and a manager for the ROMs, BIOS, saves and configuration files.

Supported platforms:

* x86_64 (Arch Linux / CachyOS), OpenGL 3.3 core
* Raspberry Pi 4 and later (Armbian / Debian trixie, arm64), OpenGL ES 3.x

Installing and running
======================

Each `vX.Y.Z` tag publishes a [release](https://github.com/JyuHo/emulationstation-re/releases) with
an Arch Linux package (x86_64, OpenGL) and Debian trixie packages (amd64 with OpenGL, arm64 with
OpenGL ES for the Raspberry Pi 4 and later), plus their `SHA256SUMS`. ES's update menu only sees the latest *full* release: GitHub leaves
pre-releases out of `releases/latest`.

```bash
sudo pacman -U emulationstation-re-<version>-1-x86_64.pkg.tar.zst   # Arch Linux / CachyOS
sudo apt install ./emulationstation-re_<version>_arm64.deb          # Debian / Armbian trixie
```

When ES was installed from one of these packages, **Updates & downloads > Check for updates**
compares it with the latest release (its `latest.json`) and installs the new package:
`es-re-update` (in `<libexecdir>/emulationstation-re`) downloads it, checks it against the
release's `SHA256SUMS` and installs it with pacman or apt, run as root by `pkexec`. Its polkit
policy lets the active local session do this without a password; it needs `curl` and `pkexec`
(package `polkit` on Arch, `pkexec` on Debian).

ES also looks at `latest.json` by itself, 10 seconds after it starts and then once a day: when a
newer version is out, a small notification shows in the top right corner, and nothing shows
otherwise, network errors included. **Check for updates at startup**, in the same menu, turns
this off.

The release packages get their [API keys](#api-keys) from the repository's GitHub Actions
secrets (see [Keys in the release packages](#keys-in-the-release-packages)). A service whose key
is not set there (ScreenScraper, TheGamesDB, HfsDB, RetroAchievements) is not available in them.

Emulator catalog
----------------

The **Emulator catalog** menu installs standalone emulators (AppImages, archives or Flathub apps)
and libretro cores from the catalog published by [es-re-catalog](https://github.com/JyuHo/es-re-catalog):
it downloads its `catalog.json` and checks every file against the sha256 listed there. Set
`global.catalog.url` to use another catalog. The format is documented in the es-re-catalog README.

When a standalone emulator is installed, ES adds it to the system entries it supports and writes
an `es_systems_<system>.cfg` file when a system needs one (the existing path, extensions and
RetroArch cores stay available). The files are read at the next start: restart EmulationStation
to see the systems. Removing the emulator removes it from these files; a file created only for
the emulator is deleted.

Ports
-----

The package installs an `es_systems_ports.cfg` extra: shell scripts ending in `.sh` in
`~/ROMs/ports`, started with `bash`. Being an extra, it is merged even when the user has an
`es_systems_custom.cfg` base. The package also ships a default OpenRA port; on first start ES
copies the scripts of `/usr/share/emulationstation/ports` to `~/ROMs/ports` when that folder does
not exist yet.

The Arch package depends on `openra`. On Debian, install the Flathub app
(`flatpak install flathub net.openra.OpenRA`) or build OpenRA; the port script uses either the
native `openra-ra`/`openra-cnc`/`openra-d2k` binaries or the Flatpak. It launches Red Alert by
default; copy `OpenRA.sh` and change the binary to launch Tiberian Dawn or Dune 2000.

Where files live
----------------

EmulationStation follows the [XDG Base Directory specification](https://specifications.freedesktop.org/basedir-spec/latest/).
Each `XDG_*` variable falls back to the default shown in brackets.

| What | Folder |
| --- | --- |
| Settings, input, `es_systems.cfg`, `es_features.cfg`, scripts, theme settings, `padtokey` | `$XDG_CONFIG_HOME/emulationstation` (`~/.config`) |
| Themes, music, gamelists, collections, resources overrides | `$XDG_DATA_HOME/emulationstation` (`~/.local/share`) |
| ROMs of the shipped system list | `~/ROMs/<system>` (the `~/ROMs` folder is created on first start) |
| RetroArch shader presets (`shaders/`), video filters (`videofilters/`), decoration sets (`decorations/<set>/`, such as The Bezel Project's overlays) | `$XDG_DATA_HOME/emulationstation` |
| Downloads, video thumbnails, temporary files | `$XDG_CACHE_HOME/emulationstation` (`~/.cache`) |
| Logs (`es_log.txt`), last used input | `$XDG_STATE_HOME/emulationstation` (`~/.local/state`) |
| Flag files (`restart.please`...), `game.xml`, `es_input_players.cfg`, runtime scripts | `$XDG_RUNTIME_DIR/emulationstation` (`/tmp/emulationstation-<uid>`) |
| System-wide `es_systems.cfg` and `es_features.cfg` | `$XDG_CONFIG_DIRS/emulationstation` (`/etc/xdg`) and `$XDG_DATA_DIRS/emulationstation` (`/usr/local/share:/usr/share`), then the install prefix |
| System-wide resources, themes and music | `$XDG_DATA_DIRS/emulationstation` (`/usr/local/share:/usr/share`), then the install prefix |

`--home <folder>` builds the user folders from that folder instead (for example
`<folder>/.config/emulationstation`) and ignores the `XDG_*_HOME` variables.

The old `~/.emulationstation` folder is no longer read. To keep an existing setup,
move the settings and `es_*.cfg` files to `~/.config/emulationstation`, and the
`themes`, `music`, `gamelists` and `collections` folders to `~/.local/share/emulationstation`.

Developing
==========

Dependencies
------------

The code is built as C++23, so you need GCC 13+ or Clang 18+ and CMake 3.25+. You also need SDL 3.2+, SDL_image 3.2+, FreeType, cURL, RapidJSON, LibVLC, ALSA and gettext. FriBidi, libudev and libcec are optional. The `fonts-droid-fallback` package (or equivalent) provides fallback fonts for Chinese/Japanese/Korean characters at run time.

**On Arch Linux / CachyOS:**
```bash
sudo pacman -S --needed base-devel cmake ninja sdl3 sdl3_image freetype2 curl \
  rapidjson vlc alsa-lib gettext fribidi libcec zlib
```

**On Armbian trixie / Debian:**
```bash
sudo apt-get install build-essential cmake ninja-build libsdl3-dev \
  libsdl3-image-dev libfreetype-dev libcurl4-openssl-dev rapidjson-dev libvlc-dev \
  libasound2-dev libgles-dev libgl-dev libudev-dev libfribidi-dev libcec-dev zlib1g-dev gettext \
  fonts-droid-fallback
```

Building
--------

This repository uses a git submodule:

```bash
git clone --recursive https://github.com/JyuHo/emulationstation-re.git
```

or, in an existing clone, `git submodule update --init`.

Then configure and build:

```bash
# x86_64, desktop OpenGL 3.3 core
cmake -S . -B build -G Ninja -DGL=ON -DCMAKE_BUILD_TYPE=Release
# Raspberry Pi 4 and later, OpenGL ES 3.x
cmake -S . -B build -G Ninja -DGLES3=ON -DCMAKE_BUILD_TYPE=Release

cmake --build build
```

Use `-DCMAKE_BUILD_TYPE=Debug` for a debug build (the default is `Release`).

Install `ccache` (`ccache` package on Arch and Debian) to make rebuilds much faster: CMake
uses it on its own when it finds it. A rebuild after switching branches or pulling then only
compiles the files that changed (13 s instead of about 4 minutes on 4 cores).

Everything the build produces stays in the build folder, nothing is written to the
source tree: `build/emulationstation`, `build/locale/<lang>/LC_MESSAGES/*.mo` and a
`build/resources` link to the source resources. Translations are only compiled when
gettext (`msgfmt`) is installed. To refresh the `.pot` template and the `.po` files
after changing strings in the code, run `cmake --build build --target i18n-update`.

Emulator options (`es_features.cfg`)
------------------------------------

The options of the per system and per game settings come from `features/es_features.cfg`,
installed in `<prefix>/share/emulationstation` and linked next to `build/emulationstation`.
It is built from Batocera's emulator definitions and only keeps the emulators and options
es-launcher handles. After adding an option to a generator, rebuild it from a batocera.linux
checkout (`pip install ruamel.yaml`):

```bash
git clone --depth 1 https://github.com/batocera-linux/batocera.linux.git /tmp/batocera
python3 features/build_es_features.py /tmp/batocera
```

This also refreshes the translations of the options (`locale/features/<lang>.po`). Options
of an emulator Batocera does not have go in `features/<emulator>.emulator.yml` (Eden).
An `es_features.cfg` in `~/.config/emulationstation` replaces the installed one, and
`es_features_*.cfg` files add to it.

Running from the build folder
-----------------------------

There is no need to install: run the binary straight from the build folder, it finds its
resources through the `build/resources` link.

```bash
./build/emulationstation --windowed --resolution 1280 720 --debug
```

`--home <folder>` keeps a test setup apart from your own (for example `--home /tmp/es-test`
uses `/tmp/es-test/.config/emulationstation` and so on). `emulationstation.sh` runs the binary
in a loop, so that **Restart** in the Quit menu starts it again.

`cmake --install build` puts the binary in `<prefix>/bin` and the resources in
`<prefix>/share/emulationstation`.

CMake options
-------------

| Option | Default | What it does |
| --- | --- | --- |
| `GL` | `OFF` | Build the desktop OpenGL 3.3 core renderer |
| `GLES3` | `OFF` | Build the OpenGL ES 3.x renderer |
| `ES_TESTS` | `OFF` | Build the es-app unit tests (catalog, updates, system services) |
| `ES_LAUNCHER_TESTS` | `OFF` (`ON` when es-launcher is built on its own) | Build the es-launcher tests |
| `ES_VERSION` | from `git describe` | Version shown in ES and given to the packages |
| `CEC` | `ON` | HDMI-CEC support, when libcec is found |
| `ENABLE_PULSE` | `OFF` | PulseAudio volume control instead of ALSA |
| `ENABLE_TTS` | `OFF` | Text to speech with espeak |
| `ENABLE_FILEMANAGER` | `OFF` | F1 opens a file manager |
| `DISABLE_KODI` | `OFF` | Remove Kodi from the menu |
| `USE_SYSTEM_PUGIXML` | `OFF` | Use the system pugixml instead of the bundled one |
| `ES_CCACHE` | `ON` | Compile through ccache when it is installed |
| `ES_PCH` | `ON` | Precompile the headers that most files of es-core and es-app include |

`GL` and `GLES3` are mutually exclusive. When neither is given, ARM builds pick `GLES3` and
the others pick `GL`.

API keys
--------

Some web services need a developer key, which is compiled into the binary. Each key is read
from the CMake variable of that name, or else from the environment variable of that name when
CMake configures the build. When it is empty or not set, the matching scraper or menu is left
out of the build.

| Variable | Service | Value |
| --- | --- | --- |
| `SCREENSCRAPER_DEV_LOGIN` | ScreenScraper scraper | URL parameters added to every API call: `devid=<id>&devpassword=<password>` |
| `SCREENSCRAPER_SOFTNAME` | ScreenScraper | Software name sent with each call, followed by the version (default `Emulationstation`) |
| `GAMESDB_APIKEY` | TheGamesDB scraper | The API key |
| `HFS_DEV_LOGIN` | HfsDB scraper (db.hfsplay.fr) | `<user>:<password>` |
| `CHEEVOS_DEV_LOGIN` | RetroAchievements menu | URL parameters added to every API call: `z=<user>&y=<web API key>`; `z` is also sent as the user agent |

```bash
cmake -S . -B build -G Ninja -DGL=ON \
  -DSCREENSCRAPER_DEV_LOGIN="devid=<id>&devpassword=<password>" \
  -DSCREENSCRAPER_SOFTNAME="EmulationStation-RE" \
  -DGAMESDB_APIKEY="<key>" \
  -DHFS_DEV_LOGIN="<user>:<password>" \
  -DCHEEVOS_DEV_LOGIN="z=<user>&y=<web API key>"
```

or, with environment variables:

```bash
export GAMESDB_APIKEY="<key>" CHEEVOS_DEV_LOGIN="z=<user>&y=<web API key>"
cmake -S . -B build -G Ninja -DGL=ON
```

Quote the values: they hold `&`. They are written to a generated `ApiKeys.h`, so `#`, quotes
and backslashes are supported. Keys given with `-D` are stored in `build/CMakeCache.txt` (remove
one with `cmake -U <VARIABLE> build`), and every key can be read back from the binary, so do not
publish a build or a cache made with your own keys.

IGDB and ArcadeDB need no build-time key. The IGDB client ID and secret are entered in the
scraper settings (**Client ID** and **Client secret**, saved as `IGDBClientID` and `IGDBSecret`).

The scraper account settings also accept user keys:

- **ScreenScraper**: your account username/password, plus the developer ID and password
  (they replace the compiled `SCREENSCRAPER_DEV_LOGIN`);
- **HFSDB**: username and password (they replace the compiled `HFS_DEV_LOGIN`);
- **TheGamesDB**: API key (it replaces the compiled `GAMESDB_APIKEY`);
- **RetroAchievements**: a web API key (with the username, it replaces the compiled
  `CHEEVOS_DEV_LOGIN`).

A key entered by the user takes precedence over the compiled one. The keys are saved in
`es_settings.cfg`, chmod `0600` (the optional `config` file used by `SystemConf` is chmod
`0600` too). The compiled keys remain in the binary as a fallback.

Tests
-----

```bash
cmake -S . -B build -G Ninja -DGL=ON -DES_TESTS=ON -DES_LAUNCHER_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

es-launcher can also be built and tested on its own, without the rest of ES:

```bash
cmake -S es-launcher -B build-launcher -G Ninja
cmake --build build-launcher
ctest --test-dir build-launcher --output-on-failure
```

Game launcher
-------------

`es-launcher/` holds the game launcher: it configures each emulator (controllers, video,
per-game options) and builds the command that starts it, as Batocera's `batocera-launch` does.
It has native generators for RetroArch, Azahar, Cemu, Dolphin, DuckStation, Eden, PCSX2,
PPSSPP, RPCS3, xemu and Xenia; an emulator without a generator is started with the command of
its `es_systems.cfg` entry. es-launcher is licensed under the LGPL 3.0 or later, unlike the rest
of the code (MIT), and is built as a shared library, `libes-launcher.so`. See
[es-launcher/README.md](es-launcher/README.md).

Packages
--------

The version comes from the tag (`git describe` for other builds, or `-DES_VERSION=<version>`).
To make the packages yourself: `ES_VERSION=<version> makepkg -s` in `packaging/arch`, or configure
with `-DCMAKE_INSTALL_PREFIX=/usr`, build, and run `cpack` in the build folder for the `.deb`.
Three workflows build and check the packages for every pull request and every push to `master`,
each with its own badge at the top of this page: [Arch Linux x86_64](.github/workflows/build-arch.yml),
[Debian amd64](.github/workflows/build-debian-amd64.yml) and [Debian arm64](.github/workflows/build-debian-arm64.yml).
When a `vX.Y.Z` tag is pushed, the [release workflow](.github/workflows/release.yml) builds the three
again, with the API keys, and publishes them.

Code quality
------------

The [Code quality workflow](.github/workflows/quality.yml) runs clang-tidy on the C++ lines each
pull request adds or changes, with the checks listed in [.clang-tidy](.clang-tidy). Only those
lines are checked, not the code ES-RE inherited, and the warnings appear on the lines of the pull
request. For now it warns without failing. To check your changes before pushing, configure with
`-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` and run `.github/scripts/clang-tidy-changed.py master build`.
A line can be excused with `// NOLINT(check-name)` and a reason.

The [CodeQL workflow](.github/workflows/codeql.yml) looks for security issues in the whole code
(and in the workflows) on every push to `master` and once a week. Its alerts are in the
repository's **Security** tab.

Configuring
===========

**~/.config/emulationstation/es_systems.cfg:**
The package installs a default system list in `/usr/share/emulationstation/es_systems.cfg`, so EmulationStation has a working configuration out of the box. Copy it to `~/.config/emulationstation/es_systems.cfg` to customize it; the user copy takes precedence. See the "Writing an es_systems.cfg" section for more information.

On first start, EmulationStation creates its user folders (`~/.config/emulationstation`, `~/.local/share/emulationstation/...`, `~/.cache/emulationstation`, `~/.local/state/emulationstation`) and the default ROM folder `~/ROMs`. Put each system's games in `~/ROMs/<system>`: a system is shown only when its folder contains at least one game with a matching extension.

**Keep in mind you'll have to set up your emulators separately from EmulationStation!**

**~/.config/emulationstation/es_input.cfg:**
When you first start EmulationStation, you will be prompted to configure an input device. The process is thus:

1. Hold a button on the device you want to configure.  This includes the keyboard.

2. Press the buttons as they appear in the list.  Some inputs can be skipped by holding any button down for a few seconds.

3. You can review your mappings by pressing up and down, making any changes by pressing South (B on SNES).

4. Choose "SAVE" to save this device and close the input configuration screen.

The new configuration will be added to the `~/.config/emulationstation/es_input.cfg` file.

**Both new and old devices can be (re)configured at any time by pressing the Start button and choosing "MAP CONTROLLER".** From here, you may unplug the device you used to open the menu and plug in a new one, if necessary. New devices will be appended to the existing input configuration file, so your old devices will remain configured.

**If your controller stops working, you can delete the `~/.config/emulationstation/es_input.cfg` file to make the input configuration screen re-appear on next run.**

You can use `--help` or `-h` to view a list of command-line options. Briefly outlined here:
```
--resolution [width] [height]   try and force a particular resolution
--monitor [index]               open the window on this monitor
--gamelist-only                 skip automatic game search, only read from gamelist.xml
--ignore-gamelist               ignore the gamelist (useful for troubleshooting)
--draw-framerate                display the framerate
--no-exit                       don't show the exit option in the menu
--no-splash                     don't show the splash screen
--debug                         more logging
--windowed                      not fullscreen, should be used with --resolution
--fullscreen-borderless         fullscreen, non exclusive
--vsync [1/on or 0/off]         turn vsync on or off (default is on)
--max-vram [size]               Max VRAM to use in Mb before swapping. 0 for unlimited
--anti-aliasing [0, 2 or 4]     set MSAA anti-aliasing to disabled, 2x or 4x
--force-kid                     Force the UI mode to be Kid
--force-kiosk                   Force the UI mode to be Kiosk
--force-disable-filters         Force the UI to ignore applied filters in gamelist
--home [path]                   build the user folders from this folder
--help, -h                      summon a sentient, angry tuba
```

As long as ES hasn't frozen, you can always press F4 to close the application.

Writing an es_systems.cfg
=========================

Complete configuration instructions at [emulationstation.org](http://emulationstation.org/gettingstarted.html#config).

The `es_systems.cfg` file contains the system configuration data for EmulationStation, written in XML.  This tells EmulationStation what systems you have, what platform they correspond to (for scraping), and where the games are located.

A default file is installed in `/usr/share/emulationstation/es_systems.cfg`; copy it to `~/.config/emulationstation/es_systems.cfg` before editing it. It uses RetroArch through the integrated launcher and expects the games in `~/ROMs/<system>`.

ES uses the first `es_systems.cfg` it finds in these folders:
* `~/.config/emulationstation/` (`es_systems_custom.cfg` is read first if it exists)
* the folder of the executable
* `$XDG_CONFIG_DIRS/emulationstation/` (`/etc/xdg/emulationstation/`)
* `$XDG_DATA_DIRS/emulationstation/` (`/usr/local/share/emulationstation/`, `/usr/share/emulationstation/`)

Extra `es_systems_*.cfg` files found in the same folders are merged in.

Because `es_systems_custom.cfg` is read first, creating it hides the installed default. Copy the default into it, or keep the default as an `es_systems_*.cfg` extra file, if you want both.

The order EmulationStation displays systems reflects the order you define them in.

**NOTE:** A system *must* have at least one game present in its "path" directory, or ES will ignore it! If no valid systems are found, ES will report an error and quit!

See [SYSTEMS.md](SYSTEMS.md) for some live examples in EmulationStation.

The following "tags" are replaced by ES in launch commands:

`%ROM%`		- Replaced with absolute path to the selected ROM, with most Bash special characters escaped with a backslash.

`%BASENAME%`	- Replaced with the "base" name of the path to the selected ROM. For example, a path of "/foo/bar.rom", this tag would be "bar". This tag is useful for setting up AdvanceMAME.

`%ROM_RAW%`	- Replaced with the unescaped, absolute path to the selected ROM.  If your emulator is picky about paths, you might want to use this instead of %ROM%, but enclosed in quotes.

`%HOME%`	- Replaced with the home folder.

`%SYSTEM%`	- Replaced with the current selected system name.

`%EMULATOR%`	- Replaced with the current selected emulator.

`%CORE%`	- Replaced with the current selected core.

gamelist.xml
============

The `gamelist.xml` file for a system defines metadata for games, such as a name, image (like a screenshot or box art), description, release date, and rating.

If at least one game in a system has an image specified, ES will use the detailed view for that system (which displays metadata alongside the game list).

*You can use ES's [scraping](http://en.wikipedia.org/wiki/Web_scraping) tools (ScreenScraper, TheGamesDB, HfsDB, IGDB, ArcadeDB; see [API keys](#api-keys) for the ones that need a key at build time) to avoid creating a `gamelist.xml` by hand.*  There are two ways to run the scraper:

* **If you want to scrape multiple games:** press start to open the menu and choose the "SCRAPER" option.  Adjust your settings and press "SCRAPE NOW".
* **If you just want to scrape one game:** find the game on the game list in ES and press select.  Choose "EDIT THIS GAME'S METADATA" and then press the "SCRAPE" button at the bottom of the metadata editor.

You can also edit metadata within ES by using the metadata editor - just find the game you wish to edit on the gamelist, press Select, and choose "EDIT THIS GAME'S METADATA."

The switch `--ignore-gamelist` can be used to ignore the gamelist and force ES to use the non-detailed view.

If you're writing a tool to generate or parse gamelist.xml files, you should check out [GAMELISTS.md](GAMELISTS.md) for more detailed documentation.


Themes
======

Themes can be installed from **Updates & downloads > Themes**, which lists the themes of the
[es-re-catalog](https://github.com/JyuHo/es-re-catalog) repository (its `themes.json`; set
`global.themes.url` to use another list). They can also be copied by hand into
`~/.local/share/emulationstation/themes`, or system-wide into `/usr/share/emulationstation/themes`.

If you want to know more about making your own themes (or editing existing ones), read [THEMES.md](THEMES.md) or check out the [wiki page](https://wiki.batocera.org/write_themes_for_emulationstation).

