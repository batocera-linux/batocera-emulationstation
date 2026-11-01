# Selection script events

EmulationStation runs scripts from `scripts/<event>/` in its configuration
directory. On Batocera, the user directory is
`/userdata/system/configs/emulationstation/scripts/`. Shell scripts must be
executable. Scripts placed directly in `scripts/` receive the event name as
their first argument, followed by the arguments below.

## `collection-selected`

This event reports the collection whose **game list is currently displayed**.
It lets external displays and companion tools distinguish collections that
contain games from the same systems, or even the same ROM.

| Argument | Value |
| --- | --- |
| 1 | The collection's internal name (`SystemData::getName()`). |
| 2, optional | The selected game's original system name. |
| 3, optional | The selected game's ROM path. |

For example, switching between two custom collections can produce:

```text
collection-selected "NelfePlay Multiplayer" "neogeo" "/userdata/roms/neogeo/mslug.zip"
collection-selected "NelfePlay World Scoring" "neogeo" "/userdata/roms/neogeo/mslug.zip"
```

These names are examples; scripts should match the actual collection names
configured in ES. The name is not a localized display label, the collection
bundle's name, or the selected game's original system.

The event is emitted once when the active collection changes, including when
both collections have the same game selected. It covers automatic and custom
collections, both standalone and inside the custom-collection bundle. Entering
a bundled collection reports that collection, not the bundle. It applies to
all game-list layouts, including the game carousel.

Selection is captured at the end of the UI update, after any deferred view
activation and folder cursor restoration. Populating or preloading a hidden
view does not report that hidden view as selected. Moving between games within
the same collection does not emit another `collection-selected` event; use
`game-selected` for those changes.

There are two cases without a selected game:

- In a collection with a folder, parent entry, placeholder or no item selected,
  only argument 1 is passed. No ROM is inferred from another selection.
- When leaving collection browsing for a regular system, the collection bundle
  root, or the main system carousel, the event is emitted **without arguments**.
  Consumers can clear their collection-specific display state. No initial
  empty event is emitted before a collection has been entered.

Hovering a collection in the main system carousel is still handled by
`system-selected`; `collection-selected` describes the active game-list context.

The new event is asynchronous by default, like `game-selected`. It contains its
own collection/game snapshot, so consumers do not need to combine two scripts'
results or depend on their completion order. Subsequent game changes are still
reported separately by `game-selected`.

### Logging example

Create `scripts/collection-selected/log.sh`:

```sh
#!/bin/sh
printf 'collection=<%s> system=<%s> rom=<%s>\n' \
    "${1-}" "${2-}" "${3-}" >> /tmp/es-collection-selection.log
```

On Batocera:

```sh
chmod +x /userdata/system/configs/emulationstation/scripts/collection-selected/log.sh
```

An empty `collection` value in this log means collection browsing was left.
For a script directly under `scripts/`, check `$1` for `collection-selected`
and read the collection, system and ROM from `$2`, `$3` and `$4` instead.

## Existing events

`game-selected` keeps its existing three arguments: original system name, ROM
path and game name. Returning to a cached game carousel requests the current
game selection again, even if the theme's storyboard already displays it.
This does not require rebuilding that storyboard.

`system-selected` keeps its existing behavior. Switching directly between
collection game lists does not add a `system-selected` event.

## Manual verification

Install the logging script above and a similar logger for `game-selected`.
Use two collections containing games from different systems and at least one
shared ROM. Enable a game-carousel theme with a storyboard to exercise the
cached-selection case.

| Action | Expected result |
| --- | --- |
| Enter a collection from the main carousel | One collection event with its name and the currently selected game's source system/ROM. |
| Switch directly to a second collection | One event with the destination collection and its remembered selection. |
| Switch between collections with the same ROM selected | The collection event still changes; its source system and ROM may remain identical. |
| Return to a previously visited carousel | The current game is requested through `game-selected`, including the cached storyboard path. |
| Scroll through games in one collection | Existing game events; no repeated collection event. |
| Enter a collection inside the bundle | The entered collection's name, not the bundle name. |
| Return from that collection to the bundle root | A collection event without arguments, even if a collection folder is highlighted. |
| Enter a visible empty/fully filtered collection | Its name only; no placeholder or old ROM in the payload. |
| Leave for a regular system or the main carousel | One collection event without arguments; no repeated clear events. |
| Return to the same collection after leaving it | A new collection event. |
| Preload/rebuild an inactive collection view | No event for the inactive collection. |
| Repeat with basic, detailed, video and grid layouts | The collection event follows the same contract. |

Also try collection names and ROM paths containing spaces, the direct
`scripts/` dispatch form, and the asynchronous Windows script path when testing
on Windows. Existing script scheduling and duplicate-command handling remain
in effect for `game-selected`.
