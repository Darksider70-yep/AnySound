# UI spec: Chorus (working title)

Applies to the desktop app (Qt 6 QML, Phase 5) and the Android app (Kotlin + Jetpack Compose, Phase 6). Both use the same tokens, copy, and states. The name "Chorus" is a placeholder; keep it in one string resource.

## 1. Subject and audience

- **Product:** turns nearby devices into one louder, synchronized speaker.
- **Audience:** students and friends in one room, with a phone or laptop in hand. Not audio engineers.
- **Primary job:** get everyone playing in sync in under a minute, then stay out of the way. The only "advanced" control is fine-tuning delay per device.

## 2. Design direction: "sonar"

The interface is quiet and dark; **one element carries the personality: the Room view**, where every device emits sound rings. When devices are in sync their rings overlap and lock together; when one drifts, its rings visibly separate. Everything else is calm and utilitarian.

**Principles**
1. State is shown by shape and words, never color alone.
2. Sync is the product, so sync quality is the most prominent status on every screen.
3. Rows and hairlines, not stacks of cards. Surfaces differ by tone, not shadow.
4. Motion only where it carries information (rings) or answers an action.
5. Plain words. No jargon like "jitter buffer" or "clock skew" in the UI.

## 3. Design tokens

**Color, dark theme (default)**

| Token | Hex | Use |
|---|---|---|
| `harbor` | `#0F1E27` | Window background |
| `deck` | `#16303C` | Panels, list surface |
| `line` | `#21414F` | Hairlines, inactive tracks |
| `fog` | `#E6EEF2` | Primary text |
| `mist` | `#8FA6B2` | Secondary text, hints |
| `sonar` | `#4FD8E8` | Accent, primary actions, "in sync" |
| `drift` | `#F2B04C` | Warning, "drifting" |
| `lost` | `#F0625D` | Error, "out of sync / disconnected" |

Text on `sonar` uses `harbor`.

**Color, light theme**

| Token | Hex |
|---|---|
| `harbor` | `#EEF3F6` |
| `deck` | `#FFFFFF` |
| `line` | `#CBD8DF` |
| `fog` (text) | `#0F1E27` |
| `mist` | `#4A6472` |
| `sonar` | `#0A7A8C` |
| `drift` | `#9A5B00` |
| `lost` | `#C7362F` |

Follow the system theme by default; allow override in Settings. All text and control pairs must reach at least 4.5:1 contrast (3:1 for large text and graphics); re-verify when implementing.

**Type** (bundle both, OFL licensed; fall back to system UI font and Georgia)

- **Manrope** for all interface text (400, 500, 700). Enable tabular figures for every number so readouts do not jitter.
- **Instrument Serif** only for screen titles and the large sync readout.

| Role | Font | Size (dp/px) |
|---|---|---|
| Sync readout | Instrument Serif | 56 |
| Screen title | Instrument Serif | 28 |
| Body / rows | Manrope 500 | 16 |
| Secondary | Manrope 400 | 14 |
| Caption | Manrope 400 | 12 |

Sentence case everywhere. No all-caps labels.

**Spacing:** 4-point scale: 4, 8, 12, 16, 24, 32, 48.
**Radius:** 4 (inputs, small controls), 12 (panels and dialogs), full (toggles, pills). Do not apply one radius to everything.
**Elevation:** none. Separate layers with tone (`harbor`, `deck`) and 1 px `line` hairlines. No drop shadows.
**Focus ring:** 2 px `sonar` outline with 2 px offset on every interactive element.

## 4. Sync state model

| State | Shape | Label | Rule (from `stats`) | Color |
|---|---|---|---|---|
| Tight | filled circle | In sync | p95 sync error 5 ms or less | `sonar` |
| Drifting | half-filled circle | Drifting | 5 to 20 ms | `drift` |
| Out of sync | ring with slash | Out of sync | above 20 ms | `lost` |
| Connecting | dotted ring | Connecting | joining or resyncing | `mist` |
| Lost | empty ring | Lost connection | no packets for 1 s | `lost` |

Each device row shows shape, label, and the number, for example `In sync  ±3 ms`.

## 5. Screens

### 5.1 Home

Two large, equal choices. No marketing, no tutorial.

```
Chorus
Make every device a speaker.

  ┌────────────────────────┐  ┌────────────────────────┐
  │ Share this device's    │  │ Play sound from        │
  │ sound                  │  │ another device         │
  └────────────────────────┘  └────────────────────────┘
                                                [Settings]
```

On phones, "Share" is hidden in v1 (Android is client-only); show only "Play sound from another device".

### 5.2 Host

Two columns on desktop (Room view left, device list right); stacked on narrow windows.

```
┌───────────────────────────────────────────────────────────────┐
│ Sharing this computer's sound             Code 4821   [QR]    │
├────────────────────────────────────┬──────────────────────────┤
│                                    │ Listening devices (3)    │
│           ( ( ( ◉ ) ) )            │ ─────────────────────────│
│      ◦ Priya's laptop              │ ● Priya's laptop  ±2 ms  │
│  ◦ Pixel 8          ◦ Lab Dell     │   Volume ━━━●━━  Delay 0 │
│                                    │ ─────────────────────────│
│         Room view                  │ ◐ Pixel 8         ±11 ms │
│                                    │   Volume ━━●━━━  Delay +8│
├────────────────────────────────────┴──────────────────────────┤
│ Volume ━━━━━●━━   Delay between devices 300 ms [−][+]  [Stop sharing] │
└───────────────────────────────────────────────────────────────┘
```

- **Room view:** host node in the center, one node per device (positions auto-arranged, draggable for fun, purely visual). Ring behavior in section 7.
- **Device row:** name, platform icon, sync state, volume slider, mute toggle, delay stepper (±1 ms, hold to repeat, ±10 ms on Shift/long-press), overflow menu (Rename, Remove).
- **Bottom bar:** master volume, "Delay between devices" (the target latency; explain in a tooltip: "Higher is steadier on busy Wi-Fi but delays sound"), and the primary action.
- **Silent host notice:** show once at start: "This computer's own speakers should be off while sharing, or they'll play ahead of the others." Provide a "Got it" action.
- **Empty state:** "Nobody's listening yet. Open Chorus on another device and enter code 4821, or scan the QR code."

### 5.3 Client: find a host

```
Nearby
─────────────────────────────────────
Daksh's laptop                [Join]
Lab PC                        [Join]
─────────────────────────────────────
Not listed?  [Enter code or address]  [Scan QR]
```

- Joining prompts for the 4-digit code (auto-advance digits, paste supported). Skip if joined by QR.
- Empty state: "No devices found yet. Make sure you're on the same Wi-Fi as the sharing device, then try again." Action: "Search again". If discovery keeps failing, add: "Some public and college networks block devices from seeing each other. A phone hotspot usually works."

### 5.4 Client: listening

The sync readout is the hero: large serif `±3 ms`, the state shape and label beneath, then controls.

```
Listening to Daksh's laptop
        ±3 ms
        ● In sync

Volume   ━━━━━━●━━
Delay    −  +8 ms  +      [Fine-tune]
[Leave]
```

- Keep the screen awake option on phones ("Keep screen on"); playback continues with the screen off via the foreground service.
- Persistent Android notification: "Playing from Daksh's laptop" with a Leave action.

### 5.5 Calibrate (fine-tune)

Purpose: fix per-device output latency (especially phones and Bluetooth).

1. Explain in one line: "Both devices will play a short click together. Adjust delay until you hear one click."
2. Button "Play test clicks" (host and client emit a click once per second).
3. Delay control: ±1 ms and ±10 ms buttons plus a slider (±500 ms). Live value shown.
4. "Save" stores the offset per device name and applies it next time.

### 5.6 Settings

Output device, capture device (host), theme (System, Dark, Light), "Keep screen on" (mobile), network ports (Advanced), "Show connection details". Keep to one screen.

## 6. Components

- **Sync badge:** shape + label + `±N ms`. Used in rows and readouts.
- **Slider:** 4 px track (`line`), `sonar` fill, 20 px thumb (44 px touch target). Value announced to screen readers.
- **Stepper:** [−] value [+]; buttons at least 44 px on touch.
- **Row:** 56 px minimum height, hairline separator, no card background.
- **Primary button:** `sonar` fill, `harbor` text, radius 12. **Secondary:** 1 px `line` outline, `fog` text.
- **Dialog:** `deck` surface, radius 12, one primary action.
- **Toast:** `deck` surface, 4 s, matches the action's verb ("Removed Pixel 8").

## 7. Room view and motion

- Each device emits a ring every 1.2 s that expands and fades over about 2.4 s. This is a visual metaphor and is **not** tied to audio rate.
- **Phase offset encodes sync error:** `phase = clamp(syncErrorMs / 20, -1, 1) * 0.5` cycle. In sync means rings overlap; larger error visibly separates them.
- Ring color follows the state color; a Lost device shows a single static dim ring.
- All other motion is a response to an action, 150 ms or less (toggle, expand, confirm). No entrance animations, no hover flourishes.
- **Reduced motion:** replace pulsing with static concentric rings and rely on the numeric readout.
- Redraw the Room view at 30 fps maximum and pause when the window is hidden.

## 8. Copy rules

- Sentence case, active voice, plain verbs. Buttons say what happens: "Share this device's sound", "Stop sharing", "Join", "Leave", "Play test clicks", "Save".
- Keep names consistent across the flow: "Share" produces "Sharing"; "Join" produces "Listening".
- Errors state what happened and how to fix it, without apologizing:
  - No source: "Couldn't find sound to share. Choose an output device in Settings."
  - Wrong code: "That code doesn't match. Check the number on the sharing device."
  - Lost connection: "Lost connection to Daksh's laptop. Trying again…" with a "Leave" action.
  - Drifting: "Sound is drifting. Move closer to the router or raise the delay between devices."
  - Bluetooth output: "Bluetooth speakers add delay. Use Fine-tune to line this device up."
- Numbers show units (`ms`, `%`). No jargon in labels (see principle 5).

## 9. Accessibility

- Full keyboard navigation on desktop with a visible focus ring; logical tab order; Esc closes dialogs.
- Screen reader labels on every control, for example "Volume for Pixel 8, 60 percent" and "Pixel 8, in sync, plus or minus 3 milliseconds". Announce state changes politely (not on every stats tick; only when the state category changes).
- Touch targets at least 44 px. Respect system font scaling to 200% without clipping.
- No information by color alone (section 4 shapes and labels).
- Honor system reduced-motion and theme settings.

## 10. Implementation notes

- **Desktop:** QML `Theme` singleton holds all tokens; one `AppController` (QObject) wraps the core `AppController` and exposes properties and signals. Bundle fonts as resources.
- **Android:** a Compose `ChorusTheme` mirrors the same tokens; a ViewModel wraps the JNI bridge and exposes `StateFlow<AppState>`.
- **State shape** (both platforms):

```
AppState {
  role: none | host | client
  connection: idle | discovering | connecting | connected | error(reason)
  pin, qrPayload (host)
  hosts[]: { id, name }                      // discovered
  devices[]: { id, name, platform, syncState, syncErrorMs,
               volume, muted, offsetMs, lossPct }
  masterVolume, targetLatencyMs
}
```

- Live values arrive throttled to 10-30 Hz. UI code never blocks, never touches audio threads, and never parses network data.
- Minimum desktop window 900 x 600; Android portrait first, landscape supported.
