# FEC-RTTY GUI scaling and layout smoke test

> Dated development/test record. Statements below apply to the recorded version and test host, not every reader's setup.
> Device IDs are examples; generated artifacts named here are not public downloads.
> Use the [current testing guide](../docs/TESTING.md) and [documentation index](../PROJECT_INDEX.md) for current instructions.

Test date: 2026-10-04 on the Windows bench machine.

## Display environment

- Adapter: Intel(R) UHD Graphics 630
- Desktop: 3840 x 2160
- Reported refresh value: 29 Hz
- Executable: packaged `gui\\fectty-gui.exe`
- Qt runtime: project-local Qt 6.8.3 deployment

## Qt scale-factor launch matrix

Each case was started as a normal visible Windows process and checked after
startup. The process stayed alive and responsive, created a non-zero main
window handle, and reported the expected title `FEC-RTTY - M0NXD`.

| `QT_SCALE_FACTOR` | Result |
| ---: | --- |
| 1.00 | Pass |
| 1.25 | Pass |
| 1.50 | Pass |
| 2.00 | Pass |

## Responsive resize smoke

With `QT_SCALE_FACTOR=1.50`, the visible window was moved through 1920x1080,
2560x1440, and 3840x2160 requests. Each `SetWindowPos` call succeeded, the
process remained responsive, and the window retained the expected title. The
desktop constrained the resulting window dimensions in this session, so this
test is recorded as a startup/responsiveness gate rather than a claim about a
pixel-perfect full-screen layout at every resolution.

## Gate result

The v0.38 GUI scaling gate is **pass** for functional startup, DPI scaling,
and responsive resizing on the available 4K Windows desktop. A future visual
polish pass may still tune spacing or typography, but no launch or layout
failure was observed in this matrix.
