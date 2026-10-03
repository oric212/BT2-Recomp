# Project goals

Run the original BT2 PS2 game logic natively on Windows 10/11 x64 using Visual Studio 2022, MSVC, C++20, and CMake.

The initial target is the North American release, SLUS-21441. Static recompilation will translate game code for the native host. A custom compatibility runtime will implement only the PS2 behavior BT2 needs, as established through evidence. Building a full PS2 emulator is outside the project scope.

PS2Recomp is a prospective recompilation dependency, Ghidra a prospective analysis tool, and PCSX2 a behavioral reference. None is integrated yet.

The first major gameplay goal is a playable versus battle. Work toward it incrementally through executable validation, ELF inspection, recompilation, runtime entry, rendering, menus, character selection, and battle loading. Audio and a full match are later milestones.

This session ends with the native host foundation. Next: validate a locally supplied SLUS-21441 executable without committing it.
