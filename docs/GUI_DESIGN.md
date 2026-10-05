# FEC-RTTY desktop GUI design

## Status and reading this document

This file combines implemented UI direction with future product requirements.
The [complete project guide](PROJECT_GUIDE.md) describes the current v0.45.1
implementation. The project began as a 24-hour digital-mode experiment, not
as a completed production radio application. Current operation is direct,
linkless plaintext; CAT/PTT is disabled.

Implemented: layout-based Qt Widgets shell, dockable setup/diagnostics,
Audio/Radio/Modem pages, real-audio text TX/RX, output volume, waterfall,
precise linked/split audio tuning, settings, and bench reports. A full GUI
logbook/macros/identity workflow, the named service-class split below, and
live radio keying remain future work. The mode list has one fixed entry.

## Product goal

FEC-RTTY needs a clean, calm Windows desktop interface that remains usable from a normal laptop display through a 4K monitor, high-DPI scaling, and window resizing. The interface must expose the modem, audio, radio-control, and logging choices without forcing users to understand the internal transport classes.

The GUI is an operator console, not a second modem implementation. All modulation, receive state, diagnostics, and radio-control actions remain in `fectty_core` and are exposed through small service interfaces.

## Technology direction

- Use Qt 6 for the first production desktop UI.
- Keep the current Qt Widgets target as the initial integration path, but replace fixed geometry with layouts, size policies, model/view controls, and DPI-aware metrics.
- Do not position controls with hard-coded screen coordinates or assume a 96-DPI display.
- Prefer semantic spacing, platform fonts, scalable icons, and layouts that survive 125%, 150%, 200%, and 4K display scaling.
- Keep the GUI optional in the dependency-free bench build. `fectty-live`, simulator, and tests must continue to build without Qt.

## Main window information architecture

The default window should make the current receive state and text exchange obvious.
The active GUI is linkless; the waterfall is above the conversation rather
than hidden in diagnostics:

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│ FEC-RTTY  [RX state] [audio level] [frequency] [CAT/PTT status]  [Settings] │
├───────────────┬──────────────────────────────────────────────┬───────────────┤
│ Session setup │ Live waterfall + RX/TX audio offsets         │ Diagnostics   │
│               │                                              │               │
│ Radio         │ Conversation / received text                 │ State machine │
│ Audio         │                                              │ Frame counters│
│ Modem         │                                              │ Signal meters │
│               │ Transmit editor              [Send]          │ Drop counters │
├───────────────┴──────────────────────────────────────────────┴───────────────┤
│ status message / activity / last event                                        │
└──────────────────────────────────────────────────────────────────────────────┘
```

The three main regions should collapse gracefully. At narrower widths, the setup and diagnostics regions become dockable or tabbed panels rather than forcing the conversation view below an unusable width. The conversation view remains the primary surface.

## Design requirements (not all implemented)

These are product requirements. Use [GUI operation](GUI_PACKAGE.md) for the
controls that actually work in 0.45.1. RF tuning/PTT, a GUI logbook, selectable
sample rate and the full identity/export workflow are not current features.

### Radio control

Provide a backend selector with explicit safe defaults:

- NullRig / bench mode — default for development and virtual-cable tests.
- Hamlib / `rigctld` — host, port, and connection test.
- OmniRig — Windows installation/profile selection and connection test.
- Future CAT/PTT providers must register through the same interface.

Show connection state, current frequency, mode, PTT state, and the reason when a backend is unavailable. Never silently transmit because a backend is misconfigured.

### Audio

Provide independent selectors for:

- RX input device.
- TX output device.
- Backend: WinMM, PortAudio, or future native backend.
- Sample rate and channel mode, with the modem-required 48 kHz mono path clearly indicated.
- Input/output level meters and a short device test.
- Optional virtual-cable labels and a refresh-devices action.

The GUI should display friendly device names while storing stable identifiers where the backend supports them. If a device disappears, preserve the user's choice but mark it unavailable and prevent unsafe start-up.

### Modem

Expose only validated operator settings initially: mode/profile, center frequency, transmit gain, RX/TX timing margins, and diagnostic verbosity. Advanced experimental settings should be separated from normal operating controls and clearly marked.

### Logging and identity

Provide callsign/identity, received/transmitted text history, ADIF/QSO logging options, capture directory, and an export action. Settings must be versioned and migrate safely as fields are added.

## Visual and usability requirements

- Use a restrained dark/light theme with strong contrast and one accent color for active transmit/state indicators.
- Use color plus text/icon, never color alone, for RX/TX/error status.
- Make destructive or radio-affecting actions visually distinct and require an intentional action.
- Keep the transmit editor keyboard-friendly; any send shortcut must require
  an open audio session, a nonempty draft, and no active send, not a remote
  link. A `Ctrl+Enter` shortcut is a design requirement, not a documented
  implemented shortcut in the current GUI.
- Make the receive view selectable, copyable, searchable, and tolerant of long lines.
- Keep live diagnostics visible without making the operator stare at a
  waterfall to know whether valid frames are decoding. There is no link
  establishment or delivery acknowledgement in the current GUI.
- Keep the live waterfall, frequency scale, labelled RX/TX fields, linkage and
  brightness control visible while typing. Frequency controls tune audio, not
  RF. Live RX retunes at a worker boundary; TX tone settings stay fixed during
  a send. Painter dimensions follow widget size and font metrics in Qt logical
  coordinates; history storage is independent of monitor resolution.
- Support keyboard navigation and screen-reader labels for controls.
- Test at 100%, 125%, 150%, and 200% Windows scaling, 1920×1080, 2560×1440, and 3840×2160.

## Target service architecture

The service names below are a proposed refactoring, not existing public C++
APIs. The [source map](PROJECT_GUIDE.md#6-architecture-and-source-map) describes
the implementation actually present in the repository.

Introduce GUI-facing services rather than putting hardware work in widgets:

- `AudioDeviceService` — enumerate devices, open RX/TX streams, expose levels and errors.
- `RigControlService` — NullRig, rigctld/Hamlib, OmniRig, connection state, frequency, and PTT.
- `ModemSessionService` — start/stop, transmit queue, receive events, state and diagnostics.
- `SettingsService` — versioned persistence, validation, defaults, and migration.
- `LogService` — structured events and user-visible activity messages.

Services must be asynchronous from the GUI's point of view. Audio callbacks and modem processing must never run on the UI thread. Widgets consume signals/events and request actions through interfaces that can be mocked in tests.

## Staged GUI implementation

1. **GUI shell:** real Qt application, DPI-safe main window, theme, menu, status bar, dockable setup/diagnostics panels, and a testable navigation model.
2. **Settings and device pages:** versioned settings model, radio backend selector, audio device enumeration, refresh/test actions, and validation messages.
3. **Live session:** connect the GUI to `ModemSessionService`, display RX/TX state, text, meters, and diagnostics while keeping NullRig selected.
4. **Safety and usability:** transmit confirmation/state gating, keyboard access, error recovery, persistence, and accessibility pass.
5. **Packaging and visual QA:** Qt runtime packaging, clean install/upgrade behavior, screenshots and manual tests at the required resolutions/scales.

The GUI milestone must not weaken the current CLI/bench acceptance gates. Every GUI change continues to run core CTest, simulator/benchmark regressions, and the VB-Audio bench test where audio behavior is touched.
