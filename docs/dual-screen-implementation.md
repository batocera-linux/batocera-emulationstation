# Native dual-screen support

## Why this exists

Dual-screen themes have worked around EmulationStation's single-window design by stretching one large window across both displays. On AYN Thor, this also required a hidden dummy output and a 4400×1080 window, with theme elements shifted into the portions visible on each physical screen.

Those workarounds can make the theme look correct while breaking other parts of the interface. Menus, popups and the left sidebar still position and size themselves against the oversized window, so they can appear in the wrong place, become too wide or fall outside the visible screen. Fixing this with more theme offsets makes the layout increasingly fragile.

This implementation moves dual-screen support into EmulationStation. Each display gets its own window and dimensions. Themes choose which screen contains their content, while the main interface uses the primary screen's dimensions. The aim is to remove the need for these layout hacks, rather than patch each affected UI element individually.

## What changes for users

**Dual-screen rendering is optional and disabled by default.** Updating EmulationStation alone does not enable it. Without the new display environment variables, existing display selection, themes and navigation keep their normal behavior.

When enabled, both screens follow the same system or game selection. Controllers still drive one interface; menus remain on the primary screen. A compatible theme is needed to provide content for the secondary screen. A compatible AYN Thor version of Canvas DS will be published in [marcbennasarp/canvas-ds, on the `thor-native-screens` branch](https://github.com/marcbennasarp/canvas-ds/tree/thor-native-screens).

The current implementation uses the GLES3 renderer and has been tested on AYN Thor. Other devices, including RG DS, need their own theme and display configuration and have not been tested.

## Enable on Thor

Use a build containing this implementation and set these variables in the launcher before starting EmulationStation:

```sh
export ES_PRIMARY_DISPLAY=DSI-2
export ES_SECONDARY_DISPLAY=DSI-1
exec emulationstation --log-path /var/log --no-splash --resolution 1920 1080
```

On Thor, `DSI-2` is the upper screen and `DSI-1` is the lower screen. Names can match either the full SDL display name or its connector suffix, such as `(DSI-1)`. Leave `ES_SECONDARY_DISPLAY` unset to keep one window; leave both variables unset to retain the existing display selection too.

Sway must enable both panels and place the windows on them. The tested layout uses:

```text
output DSI-2 enable transform 90 position 0 0
output DSI-1 enable transform 90 position 1920 0
output DSI-1 power on
for_window [title="^EmulationStation$"] move window to output DSI-2
for_window [title="^EmulationStation Secondary$"] floating enable, fullscreen disable, resize set 1240 1080, move absolute position 1920 0
no_focus [title="^EmulationStation Secondary$"]
```

Replace the old oversized-window configuration when enabling this mode. Disable any leftover dummy output and remove the stock Thor rules that reload Sway or turn off the lower panel when EmulationStation starts. Do not combine this setup with the old 4400×1080 launcher.

## Using the second screen in a theme

Existing views stay on the primary screen. Add `screen="secondary"` to a view to place its content on the other screen. For example, this shows the selected game's title on the lower screen:

```xml
<view name="basic, detailed, grid" screen="secondary">
  <text name="selected-title" extra="true">
    <text>{game:name}</text>
    <pos>0.08 0.8</pos>
    <size>0.84 0.1</size>
    <fontSize>0.05</fontSize>
  </text>
</view>
```

Positions and sizes are relative to that screen: `0.5 0.5` is its center. There is no need to account for the other panel's width or a hidden desktop area.

Secondary content follows the selected system or game through the existing theme bindings. A secondary `system` view can also define an `imagegrid` named `imagegrid`. That grid follows the primary system selection; the primary grid stops drawing, but still handles controller navigation.

Internally, the GLES3 renderer shares one graphics context between the two windows. It switches to the secondary screen's dimensions to draw its content, then restores the primary screen's state.

## Current status

The native Canvas DS preview was manually tested on AYN Thor running ROCKNIX 20260901. Checks covered both screen layouts, controller navigation, game lists, menus, and launching a game and returning. The contribution also builds on the newer upstream base, but that rebuilt version has not been retested on the device. These checks do not establish that every sidebar or popup issue is fixed.

This is still experimental. Secondary touch is ignored. Display disconnection/window-close handling, screensavers, per-screen expression variables and video/resource lifecycle need further work. True suspend was not tested because the tested ROCKNIX setup does not support it.
