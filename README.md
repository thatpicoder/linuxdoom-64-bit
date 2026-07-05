# DOOM — modernized `linuxdoom-1.10` port

This is id Software's official 1997 GPL Linux source release of DOOM
(`linuxdoom-1.10`), patched to build and run on modern 64-bit Linux
systems, with TrueColor rendering, a resizable window, and fullscreen
support added on top of the original X11 renderer.

The original 1997 release only worked on 32-bit machines with a legacy
8-bit PseudoColor display — neither of which exists on a typical Linux
desktop today. This repo fixes both.

## What was fixed / added

**Getting it to compile at all** (gcc has moved on a lot since 1997):
- `true`/`false` are reserved keywords in C23; the build now pins
  `-std=gnu17`.
- A handful of `#include <errnos.h>` typos and stray `extern int errno;`
  declarations that collide with glibc's real thread-local `errno`.
- Missing `libXext-devel` headers (handled by `setup.sh`).

**Real 64-bit bugs** — the original code assumed
`sizeof(pointer) == sizeof(int) == 4`, which was true in 1997 and false
on any modern 64-bit system:
- Texture loading smuggled a legacy on-disk field through a `void**`
  instead of the actual 4-byte format field, shifting every field read
  after it and crashing during startup.
- Several arrays of pointers were allocated with a hardcoded `* 4`
  instead of `sizeof(*ptr)`, silently under-allocating by half.
- Config defaults smuggled string pointers through an `int` field.
- A couple of pointer-alignment tricks cast a pointer to `int` and back,
  truncating the address on 64-bit.

**Made it actually usable on a modern desktop:**
- The renderer never called `XInstallColormap`, so on window managers
  that don't auto-install colormaps on focus, the palette was uploaded
  but never actually applied to the display.
- Added TrueColor/DirectColor rendering: the game's 256-color palette is
  converted to native RGB pixels on the fly, so it runs directly on a
  normal 24/32-bit desktop instead of requiring an 8-bit PseudoColor
  screen.
- Added dynamic window resizing (nearest-neighbour scaling of the fixed
  320x200 framebuffer to whatever size the window becomes).
- Added fullscreen support via the `_NET_WM_STATE_FULLSCREEN` EWMH
  hint, bound to **Alt+Enter** (F11 was left alone so vanilla DOOM's
  own F11 gamma-correction toggle, which has no menu equivalent, keeps
  working).
- The window now explicitly grabs keyboard focus on startup — without
  it, some window managers never focus a newly-mapped window, and the
  game just sits on the title/credits/demo attract-mode loop forever
  with no way to reach the menu.
- If no real key/mouse input arrives within 3 seconds of launch, the
  game synthesizes a single keypress itself — the same thing a real
  ENTER press does during the title/credits/demo loop — so it lands on
  the main menu automatically instead of cycling forever. Any real
  input before then cancels it.
- Sound effects now play through ALSA. id's own source defaults to
  piping audio through a separate `sndserver` helper process that isn't
  built by this repo, and its fallback (direct playback) only knows how
  to talk to the OSS `/dev/dsp` device, which doesn't exist on current
  Linux systems — both silently produced no sound.

## Requirements

Linux with X11 (Xorg or XWayland), a C compiler, and `libX11`/`libXext`
development headers. `setup.sh` installs all of this for you on
Fedora/RHEL (`dnf`), Debian/Ubuntu (`apt`), Arch (`pacman`), and
openSUSE (`zypper`).

## Quick start

```sh
./setup.sh   # installs build deps + fetches the shareware IWAD
./run.sh     # builds and launches DOOM
```

`setup.sh` only needs to be run once. After that, `./run.sh` rebuilds
(if anything changed) and starts the game.

If you own the full game, drop `doom.wad` or `doomu.wad` into `wad/`
and it will be picked up automatically instead of the shareware
episode (the engine prefers registered/retail/commercial IWADs over
the shareware one if more than one is present). The filename must be
exactly lowercase — `doom.wad`, not `DOOM.WAD` — since the engine looks
for that literal name and Linux filesystems, unlike the DOS/Windows
ones DOOM was designed for, are case-sensitive.

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
| F2 | Save game |
| F3 | Load game |
| F4 | Sound volume menu |
| F5 | Toggle detail level |
| F6 | Quicksave |
| F7 | End game |
| F8 | Toggle in-game messages |
| F9 | Quickload |
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
LICENSE.TXT          id Software's original license file, unmodified
README.TXT           id Software's original release notes, unmodified
ipx/, sersrc/, sndserv/   other pieces of id's original 1997 source drop
```

## License

GPLv2 — see [`LICENSE`](LICENSE). id Software released the original
`linuxdoom-1.10` source under GPLv2 only (no "or later" clause), so
this port, including all modifications described above, stays under
those same terms.

## Credits

- **id Software** — original DOOM and the 1997 Linux source release.
- The shareware IWAD is fetched from the [Internet Archive](https://archive.org/details/doom_20230531).
