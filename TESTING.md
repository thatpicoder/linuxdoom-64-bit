# Manual testing checklist

There's no automated test suite for this port — it's a game renderer/
input/audio backend, best verified by hand. Run through this after any
change to `i_video.c`, `i_sound.c`, the `Makefile`, or `setup.sh`/`run.sh`.

- X means something worked. If there's nothing in a box, it didn't work. This is being ran on an Ubuntu Desktop 22.04 LTS Live VM.

## Build

- [X] `./setup.sh` on a machine that's never had the deps installed —
      confirm it installs everything and fetches `wad/doom1.wad`
      (check the printed MD5 matches `f0cefca49926d00903cf57551d901abe`)
- [X] `rm -rf linuxdoom-1.10/linux` then `./run.sh` — confirm a fully
      clean build succeeds (catches missing `mkdir -p linux`-type bugs)
- [X] `./run.sh` a second time with no changes — confirm `make` reports
      nothing to do and it launches straight away

## Video

- [X] Title screen shows correct colors (red/brown background, green
      marine, gold logo) — garbled/inverted colors means the
      TrueColor palette conversion broke
- [X] Drag a window edge — image scales to fill the new size, no crash,
      no leftover black/garbage borders
- [X] Alt+Enter — window goes fullscreen at the monitor's native
      resolution; Alt+Enter again returns to windowed
- [it resizes the window, but the game itself isn't reszied. it sits at the top-left corner, and the rest is black. TO BE FIXED] `-fullscreen` flag starts the game already fullscreen
- [X] `-2`, `-3`, `-4` flags start at that initial window multiple
- [i ran ./run.sh -geom +100+50, but it printed Error: bad -geom parameter. TO BE FIXED"] `-geom +100+50` positions the window as requested

## Input / focus

- [X] Launch the game and, without clicking anything, press a key
      immediately — it should respond (confirms keyboard focus is
      grabbed on startup, not left to the window manager)
- [it did not auto-advance. TO BE FIXED] Launch and touch nothing for 3+ seconds — it auto-advances off
      the title/credits/demo loop onto the main menu
- [X] Launch and press a real key within 3 seconds — menu opens
      immediately from that keypress, and the game does *not* also
      auto-advance later

## Controls

- [X] Arrow keys move/turn, Alt-strafe, Shift-run
- [ctrl fires, space does not open doors. TO BE FIXED] Ctrl fires, Space uses/opens doors
- [X] 1-7 switch weapons
- [X] Tab toggles the automap
- [X] F1 help, F2 save, F3 load, F4 volume menu, F5 detail toggle,
      F6 quicksave, F7 end game, F8 message toggle, F9 quickload,
      F10 quit, F11 cycles gamma (screen brightness visibly changes)
- [X] Save a game, quit, relaunch, load it back — same spot/state

## Audio

- [X] Startup log shows `I_InitSound:  configured audio device` with
      **no** `Could not open/configure ALSA audio device` line
- [X] Menu navigation makes a blip sound
- [X] Firing a weapon, opening a door, and monster sounds are audible
      in-game
- [X] Sound volume menu (F4) actually changes volume
- [X] No background music is expected — id's own source never
      implemented it (`I_PlaySong` etc. are dummies), so silence
      during gameplay is normal, not a regression
- [X] `pactl list short sink-inputs` (or equivalent) shows an active
      stream from `linuxxdoom` while playing, as a sanity check that
      audio is actually reaching the system mixer

## WAD handling

- [X] With only `doom1.wad` present, it loads the shareware episode
- [ i dropped the full version of doom "DOOM.WAD" and it loaded doom1.wad. TO BE FIXED] Drop a `doom.wad`/`doomu.wad` into `wad/` alongside it — the
      registered/retail WAD takes priority automatically

## Process hygiene

- [X] Quit normally (F10 → confirm) — process exits, no zombie left
      behind (`ps aux | grep linuxxdoom`)
- [X] Kill and relaunch a few times in a row — no stale shared-memory
      segments accumulate (`ipcs -m`), no "user appears to be running
      DOOM" warning loop
