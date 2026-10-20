# Performance

The simulation must run comfortably on mid-range Android phones (CLAUDE.md).
This page sets what "comfortably" means, says how it is checked, and keeps
the measurements.

## The budget

**One simulated millisecond may take at most 50 µs on a mid-range phone**:
5 % of one core at the 1 kHz step rate. A 60 fps frame (16.7 ms) runs about
17 steps, so the simulation takes under 1 ms of it; the rest of the frame,
and the other cores, belong to the front end (rendering, input, sound).

The budget is set on `BM_GameStep` (`bench/run/run_bench.cpp`): one step as
the game runs it, on the shipped tractor, sled and track. It covers the easy
layout with its clutch assist, the pull (engine, clutch, driveline, both
rear sides, the chassis in pitch and in the plane, the chain and the sled
with its sideways motion) and the run's lane and end checks. The same
budget, 50 µs a step, is set on `BM_SessionFrame`
(`bench/session/session_bench.cpp`): one 60 fps frame as the game will run
it, through `tp::session` (16 or 17 of those steps, the two snapshots drawn
between and their blend), so 833 µs. The limits live in
[`bench/speed_budget.toml`](../bench/speed_budget.toml).

## How it is checked

- **On CI's ARM runner**, the release jobs run the budgeted benchmarks
  (`BM_GameStep`, `BM_SessionFrame`, `BM_PullStep`, `BM_VehicleStep` and the
  most expensive step, `BM_VehicleStepBrakeHeld`, where a brake holds its
  side at rest) five times and compare the median CPU times with the `ci_arm64_ns` limits
  (`scripts/check_speed_budget.py`). The limits only mean something on that
  runner: elsewhere the check shows the numbers. Each limit is about
  twice what the runner measured when it was set: a change that makes a step
  much slower fails the build, the runner's own noise does not. A slowdown
  made on purpose raises the limit in the same PR, saying why.
- **No allocation in the step**: `tp_alloc_tests` replaces the global
  `operator new` with a counter and runs whole pulls, easy and pro, through
  the step functions, and a whole run through the session in 60 fps frames;
  any allocation fails it. It runs on every CI job and
  on the Android emulator.
- **On a phone**, `scripts/android_bench.sh build/android-arm64` runs the same
  benchmarks over adb and checks `BM_GameStep` and `BM_SessionFrame`
  against the phone budget. CI runs the script on the x86_64 emulator to
  keep it working; the emulator's numbers say nothing about a phone.

## Measurements

Median CPU time per call, release builds.

| Where | `BM_GameStep` | `BM_SessionFrame` | `BM_PullStep` | `BM_VehicleStep` | `BM_VehicleStepBrakeHeld` |
|---|---|---|---|---|---|
| x86-64 desktop (gcc 13) | 0.82 µs | 13.9 µs | 0.79 µs | 0.52 µs | 0.67 µs |
| CI ARM runner (`ubuntu-24.04-arm`, 2 GHz; gcc / clang) | 0.62 / 0.61 µs | | 0.58 / 0.57 µs | 0.36 / 0.38 µs | 0.51 / 0.54 µs |
| Android x86_64 emulator on CI (a server core, not a phone) | 0.65 µs | | 0.61 µs | 0.38 µs | 0.57 µs |
| A mid-range phone | not measured yet | | | | |

A frame is 16⅔ steps: `BM_SessionFrame` over 16.67 is the session's cost
per step, a few percent above `BM_GameStep` (the frame's two snapshots and
their blend).

The runner repeats within 0.1 % from run to run, so its limits
(1.25, 1.2, 0.8 and 1.1 µs) sit about twice above the slower compiler. Against
the phone budget the ARM runner leaves a factor of about 80. A
mid-range phone's cores are slower than a desktop's (the efficient cores
many times so), so expect a factor of 5 to 10 there; the budget is there to
catch regressions long before they matter.

`BM_GameStep` runs a pull with both brakes off. A brake that holds its side
at rest costs a little more (the difference between `BM_VehicleStepBrakeHeld`
and `BM_VehicleStep`), so the worst game step is about `BM_GameStep` plus that
difference: about 0.8 µs on the runner, still far inside the phone budget.
The phone budget stays on `BM_GameStep` alone; the brake-held step keeps its
own runner limit.

## Measuring on a phone

1. Turn on USB debugging on the phone (Developer options) and connect it;
   `adb devices` should list it.
2. Build for phones: `cmake --preset android-arm64 && cmake --build --preset android-arm64`
   (needs `ANDROID_NDK_HOME`, see the README).
3. Run `scripts/android_bench.sh build/android-arm64`. It prints the phone
   model, the benchmark table and the check against the budget.

The kernel may move the benchmark between fast and efficient cores; running
it a few times shows the spread. Record the result in the table above.
