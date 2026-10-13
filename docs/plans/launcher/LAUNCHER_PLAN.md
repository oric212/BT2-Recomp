# Launcher and settings

Requested by the user on 2026-10-13 as the final stage before play-testing: a launcher in the style of the PS2 system menu (the "browser"), usable with a controller, that holds the port's settings. This replaces the earlier rule that no launcher exists until the game is playable.

## What the user asked for

A launcher screen, operated with a controller, with these settings:

1. **Saves:** loading saves.
2. **Controls:** control configuration.
3. **Integer scaling.**
4. **HD textures:** a checkbox. The pack is local, in `work/HD_TEXTURES/`.

## What exists today

| Piece | State |
| --- | --- |
| Window | `runtime/host/viewer_window.h`: a Win32 window that shows each frame, with integer or stretched scaling (`BT2_VIEWER_SCALE`). No input. |
| Sound | Live output on the audio device, which holds the run at the game's speed (`--sound`). |
| Controller | The native controller service answers the game from a pad script (`runtime/services/devices/pad_script.h`). No host input. |
| Memory card | The native card service reports an empty slot. No card image. |
| Settings | Environment variables read by the probe. No settings file. |
| HD textures | Nothing. The GPU renderer draws from GS memory as the game wrote it. |
| Runner | The probe (`bt2-entry-probe`), started through `run_startup_probe.py`. |

## Design

**One program.** The launcher is the first screen of the runner's window, not a second executable. Choosing "Start" runs the game in the same window; the settings apply to that run. The probe's command-line and environment options keep working for automated runs, which never show the launcher.

**Style.** The look of the PS2 system menu is the model: a dark background with slowly drifting translucent shapes, a short vertical list with one highlighted entry, a line at the bottom that names the buttons. Everything is drawn by our own code with a system font. No Sony artwork, logos, sounds or fonts are used or imitated; the screen carries this project's name.

**Drawing.** The launcher draws into the same kind of frame the viewer already presents (`DisplayFrame`), in software. That keeps it independent of the game's renderer and lets a test or a tool save any launcher screen as an image.

**Structure** (new code in `runtime/host/`, one purpose per file):

| Part | Responsibility |
| --- | --- |
| Host input | Keyboard and XInput state, read once per frame. Used by the launcher and, through the mapping, by the game's controller service. |
| Settings | A typed settings record, loaded from and saved to a file next to the program's local data (ignored by git). Unknown or invalid entries are refused by name, not silently dropped. |
| Menu model | Screens, entries, the highlighted entry, what a press does. No drawing and no Win32 in it, so it is unit-tested with scripted presses. |
| Menu renderer | Draws the menu model into a frame. |
| Launcher loop | Window, input, model, renderer; ends with "start the game" or "quit". |

**Screens and settings:**

| Screen | Contents |
| --- | --- |
| Main | Start game, Saves, Controls, Display, Quit. |
| Saves | The memory card image in slot 1: which file is used, the saves on it (title and size, read from the card's directory), create a new formatted card, pick another card file. Reading a card file made by PCSX2 is the intended way to bring existing saves in, because such a file is a plain card image. |
| Controls | Each PS2 button with its keyboard key and its pad button. Selecting an entry waits for the next press and assigns it. Reset to defaults. |
| Display | Integer scaling on or off (off stretches to the window). HD textures on or off. |

**HD textures.** The local pack holds 1,244 PNG files for SLUS-21441 in the layout PCSX2's texture replacement uses: each file is named by a hash of the texture's data, a hash of its palette and a format word. Using it needs two things the port does not have:

1. **The same names.** For each texture the game draws with, compute the name PCSX2 would give it. PCSX2's replacement code is the behavioural reference (which bytes are hashed, with which hash, what the format word holds); it is adapted, not copied, and confirmed by finding the pack's names among the textures of a real frame.
2. **A way to draw with a larger texture.** The GPU renderer samples GS memory directly. Whether the pinned paraLLEl-GS can substitute a texture, and at what cost, is not known and is the first thing to find out.

Until both work, the launcher shows the checkbox as unavailable instead of accepting a setting that does nothing. The pack is never committed; its source thread (noted in `work/HD_TEXTURES_FOR_LATER.txt`) is credited in the README when the feature ships.

## Order of work

1. **Host input and controller support:** keyboard and XInput into the native controller service, with a default mapping. This is also implementation plan step "Controller support", and it is what the launcher's own navigation needs.
2. **Launcher shell:** settings file, menu model, renderer, loop; Main and Display screens; integer scaling applied to the run. From here the game is started from the launcher with a controller.
3. **Controls screen:** remapping, saved in the settings.
4. **Saves:** a memory card image behind the native card service ([memory card plan](../memory-card/MEMORY_CARD_PLAN.md)), then the Saves screen. This is implementation plan step "Saves".
5. **HD textures:** the investigation above, then the feature, then the checkbox.

Play-testing by the user can begin after step 2 for anything that does not save, and fully after step 4. HD textures do not hold it back.

## Verification

- **Unit tests:** the menu model driven by scripted presses (navigation, wrap-around, entering and leaving screens, assigning a button, toggles); settings written and read back; invalid settings refused; the mapping from host input to the PS2 pad state.
- **Images:** each launcher screen saved to `work/frames/` for the user's inspection.
- **Game side:** the controller service's replies under live input have the same form as under the pad script, which is compared with the original; a card image created by the launcher is accepted by the game (its "create a save file" path), and a save written by the port is read back.
- **The user:** operates the launcher with a controller and confirms the settings take effect.

## Open questions

- **"Saves load":** read here as choosing and inspecting the memory card image the game uses. If it means something else (for example starting the game directly from a chosen save), the Saves screen changes.
- **HD textures:** whether the GPU renderer can substitute textures at all (step 5 starts with this).
- **Where the settings and card files live:** proposed `user/` in the repository root, ignored by git.
