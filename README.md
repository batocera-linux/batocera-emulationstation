EmulationStation
================

EmulationStation is a cross-platform graphical front-end for emulators with controller navigation.

Building
========

EmulationStation uses some C++11 code, which means you'll need to use at least g++-4.7 on Linux, or Visual Studio 2015 on Windows, to compile.

EmulationStation has a few dependencies. For building, you'll need CMake, SDL2, FreeImage, FreeType, cURL, RapidJSON, LibVLC, SDLMixer.  You also should probably install the `fonts-droid` package which contains fallback fonts for Chinese/Japanese/Korean characters, but ES will still work fine without it (this package is only used at run-time). Your distro may also offer a "system developer" group of packages that includes essential compilation tools, it is recommended to install that first.

**On Debian/Ubuntu:**
All of this can be easily installed with `apt-get`:
```bash
sudo apt-get install libsdl2-dev libsdl2-mixer-dev libfreeimage-dev libfreetype6-dev \
  libcurl4-openssl-dev rapidjson-dev libasound2-dev libgl1-mesa-dev build-essential \
  libboost-all-dev cmake fonts-droid-fallback libvlc-dev libvlccore-dev vlc-bin
```
**On Fedora:**
All of this can be easily installed with `dnf` (with rpmfusion activated) :
```bash
sudo dnf install SDL2-devel freeimage-devel freetype-devel curl-devel \
  alsa-lib-devel mesa-libGL-devel cmake \
  vlc-devel rapidjson-devel 
```
**On Solus:**
All of this can be easily installed with `eopkg`:
```bash
sudo eopkg it -c system.devel sdl2-devel freeimage-devel freetype2-devel curl-devel \
  rapidjson-devel alsa-lib-devel mesalib-devel vlc-devel sdl2-mixer-devel \
  libcec-devel
```

Note this Repository uses a git submodule - to checkout the source and all submodules, use

```bash
git clone --recursive https://github.com/batocera-linux/batocera-emulationstation.git
```

or 

```bash
git clone https://github.com/batocera-linux/batocera-emulationstation.git
cd batocera-emulationstation
git submodule update --init
```

To setup compilation, generate and build the Makefile with CMake:
```bash
cmake .
```

And then begin compilation:
```bash
make
```

**On the Raspberry Pi:**

Complete Raspberry Pi build instructions at [emulationstation.org](http://emulationstation.org/gettingstarted.html#install_rpi_standalone). You'll still have to run the instructions for Debian/Ubuntu as mentioned above first.

If the Pi uses the legacy/Broadcom driver, install the `libraspberry-dev` package before running `cmake` to configure the build.

On the Pi 4 specifically, since the legacy GL drivers are not supported anymore, you must use the following command in place of the `cmake` command:
```bash
cmake -DUSE_MESA_GLES=On .
```

**On Windows:**

[FreeImage](http://downloads.sourceforge.net/freeimage/FreeImage3154Win32.zip)

[FreeType2](http://download.savannah.gnu.org/releases/freetype/freetype-2.4.9.tar.bz2) (you'll need to compile)

[SDL2](http://www.libsdl.org/release/SDL2-devel-2.0.8-VC.zip)

[cURL](http://curl.haxx.se/download.html) (you'll need to compile or get the pre-compiled DLL version)

[RapisJSON](https://github.com/tencent/rapidjson) (you'll need the `include/rapidsjon` added to the include path)

[SDL Mixer](https://www.libsdl.org/projects/SDL_mixer/) 

[LibVlc](http://download.videolan.org/pub/videolan/vlc/) (x86 sdk files are present in .7z files)

[CMake](http://www.cmake.org/cmake/resources/software.html) (this is used for generating the Visual Studio project)

```
set ES_LIB_DIR=c:\src\lib

mkdir c:\src\batocera-emulationstation\build
cd c:\src\batocera-emulationstation\build /D

cmake -g "Visual Studio 14 2015 x86" .. -DEIGEN3_INCLUDE_DIR=%ES_LIB_DIR%\eigen -DRAPIDJSON_INCLUDE_DIRS=%ES_LIB_DIR%\rapidjson\include -DFREETYPE_INCLUDE_DIRS=%ES_LIB_DIR%\freetype-2.7\include -DFREETYPE_LIBRARY=%ES_LIB_DIR%\freetype-2.7\objs\vc2010\Win32\freetype27.lib -DFreeImage_INCLUDE_DIR=%ES_LIB_DIR%\FreeImage\Source -DFreeImage_LIBRARY=%ES_LIB_DIR%\FreeImage\Dist\x32\FreeImage.lib -DSDL2_INCLUDE_DIR=%ES_LIB_DIR%\SDL2-2.0.9\include -DSDL2_LIBRARY=%ES_LIB_DIR%\SDL2-2.0.9\build\Release\SDL2.lib;%ES_LIB_DIR%\SDL2-2.0.9\build\Release\SDL2main.lib;Imm32.lib;version.lib -DBOOST_ROOT=%ES_LIB_DIR%\boost_1_61_0 -DBoost_LIBRARY_DIR=%ES_LIB_DIR%\boost_1_61_0\lib32-msvc-14.0 -DCURL_INCLUDE_DIR=%ES_LIB_DIR%\curl-7.50.3\include -DCURL_LIBRARY=%ES_LIB_DIR%\curl-7.50.3\builds\libcurl-vc14-x86-release-dll-ipv6-sspi-winssl\lib\libcurl.lib -DVLC_INCLUDE_DIR=%ES_LIB_DIR%\libvlc-2.2.2\include -DVLC_LIBRARIES=%ES_LIB_DIR%\libvlc-2.2.2\lib\msvc\libvlc.lib;%ES_LIB_DIR%\libvlc-2.2.2\lib\msvc\libvlccore.lib -DVLC_VERSION=1.0.0 -DSDLMIXER_INCLUDE_DIR=%ES_LIB_DIR%\SDL2_mixer-2.0.4\include -DSDLMIXER_LIBRARY=%ES_LIB_DIR%\SDL2_mixer-2.0.4\lib\x86\SDL2_mixer.lib
```

Launching outside of Batocera
=============================

To launch Batocera EmulationStation, it is recommended to build it with [batocera.linux](https://github.com/batocera-linux/batocera.linux) instead. However, you can run a barebones version of ES (perhaps you want to tweak some menus without waiting for the entirety of Batocera to compile) if you provide the appropriate folder structure and files for it. **If you launch Batocera EmulationStation this way outside of Batocera, expect a lot of things to not actually be functional (eg. emulator launching, the background music player, the webserver, etc.), such things can only work with an environment set up identically to Batocera.** Before attempting this, it is recommended to have built batocera.linux at least once to have the appropriate configuration files for Batocera Emulationstation. They can also conveniently be taken from an existing Batocera install.

Create the appropriate directories for ES's configuration files and resources, then copy them in from Batocera (instructions assuming you've already built batocera.linux):

```bash
sudo mkdir /etc/emulationstation
sudo chmod a+w /etc/emulationstation
cp -r ~/batocera.linux/output/x86_64/target/usr/share/emulationstation/ /etc/
```

Then create the `/userdata` directory in your root, give it the correct permissions, and copy in the data from Batocera's `datainit` folder:

```bash
sudo mkdir /userdata
sudo chmod a+w /userdata
cp -r ~/batocera.linux/output/x86_64/target/usr/share/batocera/datainit/* /userdata
```

Then run `./emulationstation.sh` to start up EmulationStation. This will give you enough to navigate menus and change settings, however since we aren't truly inside of Batocera you'll find that none of the emulators launch and none of the secondary functions (such as the background music player) work.

If you want to see the full Batocera EmulationStation experience, it is recommended to build [batocera.linux](https://github.com/batocera-linux/batocera.linux) instead.

Configuring
===========

**~/.emulationstation/es_systems.cfg:**
When first run, an example systems configuration file will be created at `~/.emulationstation/es_systems.cfg`.  `~` is `$HOME` on Linux, and `%HOMEPATH%` on Windows.  This example has some comments explaining how to write the configuration file. See the "Writing an es_systems.cfg" section for more information.

**Keep in mind you'll have to set up your emulator separately from EmulationStation if not launching from within Batocera!**

**~/.emulationstation/es_input.cfg:**
When you first start EmulationStation, you will be prompted to configure an input device. The process is thus:

1. Hold a button on the device you want to configure.  This includes the keyboard.

2. Press the buttons as they appear in the list.  Some inputs can be skipped by holding any button down for a few seconds.

3. You can review your mappings by pressing up and down, making any changes by pressing South (B on SNES).

4. Choose "SAVE" to save this device and close the input configuration screen.

The new configuration will be added to the `/etc/emulationstation/es_input.cfg` file.

**Both new and old devices can be (re)configured at any time by pressing the Start button and choosing "MAP CONTROLLER".** From here, you may unplug the device you used to open the menu and plug in a new one, if necessary. New devices will be appended to the existing input configuration file, so your old devices will remain configured.

**If your controller stops working, you can delete the `~/.emulationstation/es_input.cfg` file to make the input configuration screen re-appear on next run.**

You can use `--help` or `-h` to view a list of command-line options. Briefly outlined here:
```
--resolution [width] [height]   try and force a particular resolution
--gamelist-only                 skip automatic game search, only read from gamelist.xml
--ignore-gamelist               ignore the gamelist (useful for troubleshooting)
--draw-framerate                display the framerate
--no-exit                       don't show the exit option in the menu
--no-splash                     don't show the splash screen
--debug                         more logging, show console on Windows
--scrape                        scrape using command line interface
--windowed                      not fullscreen, should be used with --resolution
--fullscreen-borderless			fullscreen, non exclusive.
--vsync [1/on or 0/off]         turn vsync on or off (default is on)
--max-vram [size]               Max VRAM to use in Mb before swapping. 0 for unlimited
--force-kid             		Force the UI mode to be Kid
--force-kiosk           		Force the UI mode to be Kiosk
--force-disable-filters         Force the UI to ignore applied filters in gamelist
--home							Force the .emulationstation folder (windows)
--help, -h                      summon a sentient, angry tuba
```

As long as ES hasn't frozen, you can always press F4 to close the application.

Writing an es_systems.cfg
=========================

Complete configuration instructions at [emulationstation.org](http://emulationstation.org/gettingstarted.html#config).

The `es_systems.cfg` file contains the system configuration data for EmulationStation, written in XML.  This tells EmulationStation what systems you have, what platform they correspond to (for scraping), and where the games are located.

ES will check the following location for its `es_systems.cfg` file:
* `/etc/emulationstation/es_systems.cfg`

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

*You can use ES's [scraping](http://en.wikipedia.org/wiki/Web_scraping) tools to avoid creating a `gamelist.xml` by hand.*  There are two ways to run the scraper:

* **If you want to scrape multiple games:** press start to open the menu and choose the "SCRAPER" option.  Adjust your settings and press "SCRAPE NOW".
* **If you just want to scrape one game:** find the game on the game list in ES and press select.  Choose "EDIT THIS GAME'S METADATA" and then press the "SCRAPE" button at the bottom of the metadata editor.

You can also edit metadata within ES by using the metadata editor - just find the game you wish to edit on the gamelist, press Select, and choose "EDIT THIS GAME'S METADATA."

A command-line version of the scraper is also provided - just run emulationstation with `--scrape` *(currently broken)*.

The switch `--ignore-gamelist` can be used to ignore the gamelist and force ES to use the non-detailed view.

If you're writing a tool to generate or parse gamelist.xml files, you should check out [GAMELISTS.md](GAMELISTS.md) for more detailed documentation.


Themes
======

By default, EmulationStation will use ES-Carbon. Additional themes can be installed into `/etc/emulationstation/themes`.

If you want to know more about making your own themes (or editing existing ones), read [THEMES.md](THEMES.md) or check out the [wiki page](https://wiki.batocera.org/write_themes_for_emulationstation).

