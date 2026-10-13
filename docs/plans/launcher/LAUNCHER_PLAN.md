# Launcher, settings and in-game overlay

Requested by the user on 2026-10-13 as the final stage before play-testing: a launcher in the style of the PS2 system menu (the "browser"), usable with a controller, that holds the port's settings, and an in-game overlay for what should change on the fly. This replaces the earlier rule that no launcher exists until the game is playable.

## What the user asked for (2026-10-13)

**Launcher**, operated with a controller:

1. **Saves:** load a memory card file that holds a save of the game. Card files are listed from `user/memcards/`.
2. **Controls:** control configuration.
3. **Integer scaling.**
4. **HD textures:** a checkbox. The pack is local, in `work/HD_TEXTURES/`.
5. **Further useful settings.** Chosen by the user: fullscreen and window size, volumes, a frame rate display. The disc image path is added because the program needs it.

**When the launcher appears:** it has its own executable. The game's executable opens the launcher by itself when no configuration file exists (first start); otherwise it starts the game directly.

**Overlay**, opened in the game with Start + Select:

- sound settings and whatever else should change on the fly;
- quitting the game;
- fast forward, with the buttons that trigger it configurable in the overlay.

**Not wanted:** save states. They were considered and dropped: the game runs as native code, so a state would have to capture native call stacks as well as memory. Fast forward is the only such feature.

**Files:** settings and card files live in `user/` in the repository root, ignored by git.

## What exists today

| Piece | State |
| --- | --- |
| Window | `runtime/host/viewer_window.h`: a Win32 window that shows each frame, with integer or stretched scaling (`BT2_VIEWER_SCALE`). No input, no fullscreen. |
| Sound | Live output on the audio device, which holds the run at the game's speed (`--sound`). Fixed levels. |
| Controller | The native controller service answers the game from a pad script (`runtime/services/devices/pad_script.h`). No host input. |
| Memory card | The native card service reports an empty slot. No card image. |
| Settings | Environment variables read by the probe. No settings file. |
| HD textures | Nothing. The GPU renderer draws from GS memory as the game wrote it. |
| Runner | The probe (`bt2-entry-probe`), started through `run_startup_probe.py` with the disc image and probe arguments. |

## Design

### Programs

Two executables built from the same code:

| Program | Behaviour |
| --- | --- |
| Game | Reads `user/settings`. If the file is missing, it shows the launcher first. Otherwise it starts the game at once with the stored settings. |
| Launcher | Always shows the launcher. "Start" runs the game in the same window. |

The probe stays the tool for automated runs (the gate, the discovery loop, pad scripts); it never shows the launcher or the overlay and does not read `user/`. The game program is the first player-facing executable; it uses the same runtime and the same host code as the probe's viewer mode.

### Style

The look of the PS2 system menu is the model: a dark background with slowly drifting translucent shapes, a short vertical list with one highlighted entry, a line at the bottom that names the buttons. Everything is drawn by our own code with a system font. No Sony artwork, logos, sounds or fonts are used or imitated; the screen carries this project's name. The overlay uses the same drawing over the dimmed game picture.

### Drawing

The launcher and the overlay draw in software into the same kind of frame the viewer presents (`DisplayFrame`). That keeps them independent of the game's renderer and lets a test or a tool save any screen as an image.

### Structure

New code in `runtime/host/`, one purpose per file:

| Part | Responsibility |
| --- | --- |
| Host input | Keyboard and XInput state, read once per frame. Used by the menus and, through the mapping, by the game's controller service. |
| Settings | A typed settings record, loaded from and saved to `user/`. Unknown or invalid entries are refused by name, not silently dropped. |
| Menu model | Screens, entries, the highlighted entry, what a press does. No drawing and no Win32 in it, so it is unit-tested with scripted presses. Shared by the launcher and the overlay. |
| Menu renderer | Draws a menu model into a frame. |
| Launcher loop | Window, input, model, renderer; ends with "start the game" or "quit". |
| Overlay | Watches for its button combination during the game, pauses the game while open, applies changes at once. |

### Launcher screens

| Screen | Contents |
| --- | --- |
| Main | Start game, Saves, Controls, Display, Sound, Game disc, Quit. |
| Saves | The card files found in `user/memcards/` and which one is in slot 1. For the chosen card, the saves on it (title and size, read from the card's directory). Create a new formatted card. Copying a card file made by PCSX2 into the folder brings existing saves in. |
| Controls | Each PS2 button with its keyboard key and its pad button. Selecting an entry waits for the next press and assigns it. The fast forward buttons and the overlay combination. Reset to defaults. |
| Display | Integer scaling on or off (off stretches to the window). Fullscreen or windowed. Window size. Frame rate display on or off. HD textures on or off. |
| Sound | Master, music and effects volume. |
| Game disc | The disc image the game is read from. Images found in `rom/` and `user/` are listed; the program checks that the chosen one is the supported release before it is accepted. |

### Overlay

Opened and closed with Start + Select (configurable), or a keyboard key. While it is open the game does not advance and the sound is silent.

| Entry | Effect |
| --- | --- |
| Resume | Closes the overlay. |
| Volumes | Master, music, effects; heard at once. |
| Display | Integer scaling, fullscreen, frame rate display; applied at once. |
| Fast forward | Which buttons trigger it, and whether it is held or toggled. |
| Quit game | Ends the program after a confirmation. |

Changes made in the overlay are written to the settings file, so the launcher shows them next time.

**The combination and the game.** Start and Select also mean something to the game. The press that completes the combination is not passed on, but the first of the two buttons has already reached the game by then (Start pauses a fight). This is accepted for now; if it disturbs, the combination is changed in Controls or a short hold is required.

### Fast forward

Normally the audio device holds the run at the game's speed. Fast forward lets the run go as fast as the machine allows: fields are produced without waiting for the device, the picture is shown at the display's pace, and the sound is muted while it lasts. It only helps where the run is faster than the game (menus, movies, loading); fights currently run below the game's speed.

### Volumes

The mixer already has separate sources for streams (music, movies, voices in streams) and for the effects sampler. A volume per source and a master volume scale them before the mix is clamped. Full volume is today's level, so the measured stream level and the user's confirmed mix stay the reference.

### HD textures

The local pack holds 1,244 PNG files for SLUS-21441 in the layout PCSX2's texture replacement uses: each file is named by a hash of the texture's data, a hash of its palette and a format word. Using it needs two things the port does not have:

1. **The same names.** For each texture the game draws with, compute the name PCSX2 would give it. PCSX2's replacement code is the behavioural reference (which bytes are hashed, with which hash, what the format word holds); it is adapted, not copied, and confirmed by finding the pack's names among the textures of a real frame.
2. **A way to draw with a larger texture.** The GPU renderer samples GS memory directly. Whether the pinned paraLLEl-GS can substitute a texture, and at what cost, is not known and is the first thing to find out.

Until both work, the launcher shows the checkbox as unavailable instead of accepting a setting that does nothing. The pack is never committed; its source thread (noted in `work/HD_TEXTURES_FOR_LATER.txt`) is credited in the README when the feature ships.

## Order of work

1. **Host input and controller support:** keyboard and XInput into the native controller service, with a default mapping. This is also the implementation plan's step "Controller support", and the menus need it for their own navigation.
2. **Launcher shell:** settings file, menu model, renderer, loop; the game and launcher programs; Main, Display (scaling, fullscreen, window size, frame rate display) and Game disc screens. From here the game is started from the launcher with a controller.
3. **Overlay:** the combination, pause, Resume and Quit; volumes (with the mixer's volume controls and the launcher's Sound screen); fast forward.
4. **Controls screen:** remapping of the game buttons, the fast forward buttons and the overlay combination.
5. **Saves:** a memory card image behind the native card service ([memory card plan](../memory-card/MEMORY_CARD_PLAN.md)), then the Saves screen. This is the implementation plan's step "Saves".
6. **HD textures:** the investigation above, then the feature, then the checkbox.

Play-testing by the user can begin after step 2 for anything that does not save, and fully after step 5. HD textures do not hold it back.

## Verification

- **Unit tests:** the menu model driven by scripted presses (navigation, wrap-around, entering and leaving screens, assigning a button, toggles, sliders); settings written and read back; invalid settings refused; the mapping from host input to the PS2 pad state; the overlay combination (what reaches the game); the mixer's volumes.
- **Images:** each launcher and overlay screen saved to `work/frames/` for the user's inspection.
- **Game side:** the controller service's replies under live input have the same form as under the pad script, which is compared with the original; a card image created by the launcher is accepted by the game (its "create a save file" path), a save written by the port is read back, and a card file from PCSX2 with a save of the game is loaded by the game.
- **The gate** is unaffected: probe runs do not read `user/` and show no launcher or overlay.
- **The user:** operates the launcher and the overlay with a controller and confirms the settings take effect.

## Open questions

- **HD textures:** whether the GPU renderer can substitute textures at all (step 6 starts with this).
- **Card file formats:** the target is the card file PCSX2 writes (to my knowledge a raw image of the card that includes each page's spare bytes; to be confirmed on a real file when the Saves step starts). Folder-based cards and other layouts are refused by name unless the user needs one.
- **Gamepads other than XInput** (for example a DualShock on Windows without a wrapper) are not planned unless the user needs one.
