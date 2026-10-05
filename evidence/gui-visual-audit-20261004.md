# FEC-RTTY GUI visual and contrast audit

> Dated development/test record. Statements below apply to the recorded version and test host, not every reader's setup.
> Device IDs are examples; generated artifacts named here are not public downloads.
> Use the [current testing guide](../docs/TESTING.md) and [documentation index](../PROJECT_INDEX.md) for current instructions.

Test date: 2026-10-04. The audit covers the packaged Qt Widgets theme and the
rebuilt source stylesheet.

## Palette checks

The GUI now sets an explicit dark palette for active, inactive, and disabled
controls. It also explicitly styles fields, buttons, tabs, dock titles, the
status bar, menus, tooltips, selections, and scrollbars so these controls do
not inherit a light or platform-inconsistent fallback.

| UI pair | Foreground | Background | Contrast |
| --- | --- | --- | ---: |
| Main text | `#e7edf5` | `#111923` | 15.02:1 |
| Input text | `#e7edf5` | `#172331` | 13.50:1 |
| Standard button text | `#e7edf5` | `#26384b` | 10.19:1 |
| Primary button text | `#f4fbff` | `#246d60` | 5.85:1 |
| Primary hover text | `#f4fbff` | `#2f705f` | 5.57:1 |
| Disabled control text | `#aab6c4` | `#1b2735` | 7.35:1 |
| Help/event text | `#9aaabd` | `#111923` | 7.46:1 |
| Status pill caption | `#8ca0b5` | `#1d2c3b` | 5.29:1 |
| Status value | `#7ed7c3` | `#1d2c3b` | 8.40:1 |
| Selected tab text | `#7ed7c3` | `#1d2c3b` | 8.40:1 |
| Tooltip text | `#ffffff` | `#26384b` | 12.00:1 |

The previous primary-button combination was `#e7edf5` on `#2b8a78`, only
3.56:1. It was replaced with the darker, more legible `#246d60` treatment.

## Neatness and consistency checks

- Status pills have a consistent minimum width and aligned caption/value stack.
- Long activity messages wrap instead of being clipped at narrow widths.
- Group boxes, tabs, dock separators, focus rings, disabled controls, and
  scrollbars use the same restrained blue/teal palette.
- Selected controls use both a visible background and text change; status
  meaning remains written in words and is not conveyed by color alone.
- The rebuilt GUI starts normally and retains the existing 4K/high-DPI launch
  matrix and VB-Audio test behavior.
