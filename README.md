# Omonitor

Omonitor is a small, keyboard-friendly display arranger for Omarchy and Hyprland.
It follows the active Omarchy accent, language, and window rounding.

![Omonitor arranging two displays](docs/screenshot.png)

## Features

- Drag displays into their physical arrangement.
- Snap dragged displays to the nearest edge and remove gaps automatically.
- Detect connected displays automatically and identify each physical screen.
- Pick resolution, refresh rate, scale, and orientation.
- Preserve manual settings outside Omonitor's managed block in `monitors.lua`.
- Validate the new Hyprland configuration before accepting it.
- Automatically restore the previous configuration after 15 seconds unless confirmed.
- Follow Omarchy's theme and language live (English and Italian included).

## Build

```sh
./bin/build
./bin/test
```

## Install on Omarchy

```sh
./bin/install
```

The package installs the `omonitor` binary and its launcher entry. Open it with
`Super + Space`, then search for **Omonitor**.

## Terminal UI

Run the same display arranger inside a terminal:

```sh
omonitor tui
```

The TUI can detect, configure, move, compact, apply, confirm, and revert the
display layout without opening a desktop window.

## Keyboard shortcuts

| Key | Action |
| --- | --- |
| `Space` | Apply the display configuration |
| `A` | Remove gaps |
| `I` | Identify the physical displays |
| `R` | Detect displays again |
| `?` | Show or hide shortcut help |
| `Q` | Quit |

## Configuration safety

Omonitor writes only the block delimited by `-- omonitor:start` and
`-- omonitor:end` in `~/.config/hypr/monitors.lua`. Before each write it creates
a timestamped `.bak` copy. If `hyprctl reload` or `hyprctl configerrors` fails,
the previous file is restored immediately.

## License

MIT
