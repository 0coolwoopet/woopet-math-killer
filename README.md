# Woopet Math Fucker — Geode project (Android-oriented)

Adds a **Woopet Math Fucker** button to Geometry Dash's Options screen. The popup includes:
- A switch for best-effort runtime math hooks.
- A sine/cosine swap switch.
- A custom positive pi value.

## What the hooks do

At mod load, the project attempts to hook runtime symbols `sin`, `cos`, `sinf`, `cosf`, `acos`, `acosf`, `atan2`, and `atan2f` through Geode. The sine/cosine switch swaps calls that pass through the hooked symbols. The custom pi value is used for common runtime expressions that produce pi, including `acos(-1)` and `atan2(±0, negative x)`.

## Important limits

This is **best-effort, not a guaranteed total-game math replacement**. Some calls may be inlined, compiler-folded, use other symbols such as `sincos`, use engine-specific math, or use hard-coded pi constants; those will not be changed by these hooks. Hooking low-level math functions can affect unrelated game and mod calculations and may cause instability. Test on a copy/test install and disable the global hook toggle if the game behaves unexpectedly. The hooks are installed at mod load and remain installed but become pass-through when disabled.

The code has not been compiled or tested against your exact Geometry Dash/Geode Android build. API/ABI or linker differences may require adjustments.

## Build

Build with the Geode SDK for the **same Android architecture and Geometry Dash version** you use. Follow the official Geode docs: https://docs.geode-sdk.org/ .
