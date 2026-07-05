# DOOM — modernized `linuxdoom-1.10` port

id Software's 1997 GPL Linux source release of DOOM, patched to build
and run on modern 64-bit Linux, with TrueColor rendering, a resizable
window, and fullscreen support added on top of the original X11
renderer.

## What was fixed / added

- Fixed several 64-bit bugs (pointer/int size assumptions from 1997)
  that caused crashes and under-allocated buffers, plus C23/modern-gcc
  build breakage.
- Added TrueColor/DirectColor rendering — no 8-bit PseudoColor display
  required.
- Added dynamic window resizing and fullscreen (**Alt+Enter**, via
  EWMH).
- Fixed keyboard focus grab on startup (previously the game could get
  stuck on the title/demo loop forever on some window managers).
- Auto-advances past the title/demo loop after 3 seconds if no input
  arrives, same as a real ENTER press.
- Replaced the broken/unbuilt sound path with direct ALSA playback.

## Requirements

Linux with X11 (Xorg or XWayland), a C compiler, and `libX11`/`libXext`
dev headers. `setup.sh` installs all of this on Fedora/RHEL, Debian/
Ubuntu, Arch, and openSUSE.

## Quick start

```sh
./setup.sh   # installs build deps + fetches the shareware IWAD
./run.sh     # builds and launches DOOM
```

If you own the full game, drop `doom.wad` or `doomu.wad` (lowercase)
into `wad/` and it's picked up automatically instead of the shareware
episode.

## Controls

| Key | Action |
|---|---|
| Arrow keys | Move / turn |
| Ctrl | Fire |
| Space | Use / open doors, flip switches |
| Shift | Run |
| Alt | Strafe (combine with arrow keys) |
| 1-7 | Select weapon |
| Tab | Toggle automap |
| Esc | Main menu |
| Enter | Confirm menu selection |
| \- / = | Shrink / grow status bar view |
| F1 | Help screen |
| F2 / F3 | Save / load game |
| F4 | Sound volume menu |
| F5 | Toggle detail level |
| F6 / F9 | Quicksave / quickload |
| F7 | End game |
| F8 | Toggle in-game messages |
| F10 | Quit DOOM |
| F11 | Cycle gamma correction |
| Pause | Pause game |
| Alt+Enter | Toggle fullscreen (this port's addition) |
| drag window edge | Resize the window (this port's addition) |

## Command-line options

Pass these through `run.sh`, e.g. `./run.sh -fullscreen`:

| Flag | Effect |
|---|---|
| `-fullscreen` | Start in fullscreen |
| `-2`, `-3`, `-4` | Start at 2x/3x/4x the base 320x200 window size |
| `-geom [+-]X[+-]Y` | Start position |
| `-grabmouse` | Grab and confine the pointer |
| `-disp <display>` | Use a specific X display |

## Project layout

```
setup.sh            installs dependencies, fetches doom1.wad
run.sh               builds and runs the game
TESTING.md           manual testing checklist
linuxdoom-1.10/      the (modified) id Software source
wad/                 IWAD files live here (fetched/added, not tracked in git)
LICENSE              GPLv2, matching id Software's original release

```

## Star History

<a href="https://www.star-history.com/?repos=thatpicoder%2Flinuxdoom-64-bit&type=date&legend=top-left">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/chart?repos=thatpicoder/linuxdoom-64-bit&type=date&theme=dark&legend=top-left" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/chart?repos=thatpicoder/linuxdoom-64-bit&type=date&legend=top-left" />
   <img alt="Star History Chart" src="https://api.star-history.com/chart?repos=thatpicoder/linuxdoom-64-bit&type=date&legend=top-left" />
 </picture>
</a>

## License

GPLv2 — see [`LICENSE`](LICENSE). id Software released the original
`linuxdoom-1.10` source under GPLv2 only, so this port stays under
those same terms.

## Credits

- **id Software** — original DOOM and the 1997 Linux source release.
- The shareware IWAD is fetched from the [Internet Archive](https://archive.org/details/doom_20230531).
