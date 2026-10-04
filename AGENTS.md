# BT2 project working instructions

Use this repository as the project root. Before continuing development, read docs/PROG.md and docs/IMPLEMENTATION_PLAN.md. Consult docs/REFERENCE_ARCHITECTURE.md and its linked sources before architecture or unfamiliar PS2 subsystem work.

## Permanent meaning of "continue"

A plain "continue", including spelling/case variants, requests an aggressive pass through multiple execution blockers. Apply this automatically across sessions; never require the user to repeat the instruction. Resolving one function or blocker is not a stopping point.

Follow: generate/link -> run -> inspect -> gather evidence -> fix -> rerun -> repeat. Gather available evidence from local captures/tooling and reference repositories, then continue whenever a safe evidence-based change is possible. Adapt reference implementations; do not copy them 1:1. Keep unknown function names neutral.

Stop only when:

- A genuinely new major subsystem requires a separate implementation plan. If an actionable plan already exists, follow it and continue.
- Required information or access cannot be obtained in the current session.
- The next change would be speculative after consulting available evidence.

When stopping, identify the actual condition and evidence. Do not stop merely because one fix succeeded or a convenient number of functions was completed.

Build/test meaningful changes. At the end of each pass, update concise docs/PROG.md and detailed docs/progress/ history, review the diff/staged files, commit and push to the existing remote.

Never download or commit proprietary game/BIOS content, retail executables, disc images, extracted assets or proprietary-derived generated code. Keep local content, captures, prepared/generated code and build output ignored. README.md stays at the root; project documentation stays in docs/.
