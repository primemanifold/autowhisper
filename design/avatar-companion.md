# The companion — Echo and the pantheon

A floating desktop spirit that embodies what AutoWhisper is doing: it
glows when summoned by the hotkey, its halo follows your voice while you
speak, a mote orbits while the model thinks, and it visibly *writes* as
your words are inserted. Opt-in (`[avatar] enabled`), draggable anywhere,
always quiet at rest — it extends the original design direction (a calm,
precise, local instrument) rather than replacing it: the companion is the
product's status made visible, never a decoration layer.

## Why Greek, why Echo

The product's whole job is mythological already: you speak, something
unseen repeats you faithfully. **Echo** — the nymph cursed to only repeat
the words of others — is the exact archetype of a transcriber, so she is
the default form. Four more forms ship, chosen from the same register:

| id | form | epithet | signature | accent | tempo |
|---|---|---|---|---|---|
| `echo` | Echo | the one who answers | none — the pure sigil | record red | 1.00 |
| `hermes` | Hermes | the swift herald | wings above the dashes | message blue | 0.85 (quicker) |
| `mnemosyne` | Mnemosyne | keeper of memory | a ring within the ring | amber | 1.20 (calmer) |
| `kalliope` | Kalliope | the beautiful-voiced | a crown of three muse-stars | violet | 0.95 |
| `morpheus` | Morpheus | the shape of dreams | dream motes drifting away | teal | 1.35 (slowest) |

The body is the AutoWhisper sigil itself — the ring with two side dashes —
so the companion, the menu bar, and the brand are one mark. Each spirit
adds one **signature**: a quiet ink-drawn tell, so every form is
recognizable in silhouette alone (64px, greyscale), not just by hue.
Signatures are never accent-colored — identity lives in shape; color
keeps carrying meaning only. The full review and the overlay gotcha
inventory live in `design/2026-06-13-companion-design-review.md`.

## States (wired to the daemon)

| state | trigger | look |
|---|---|---|
| Idle | daemon idle | slow breathing float, faint ink aura |
| Summoned | hotkey press | accent glow ramps in, a ring blooms outward (0.45s, auto-advances) |
| Listening | recording | halo radius and glow follow the **live mic peak**; inner mote rises with your voice |
| Thinking | transcribing | a mote orbits the ring |
| Writing | text injection | the orb stills and three strokes are written beneath it, a bright nib leading (latched ≥1.4s so even instant injections read) |
| Ambient | other audio playing while idle (Linux: `pactl` sink-inputs) | hue shimmers toward the accent, two soft notes drift |
| Error | injection/transcription failure | amber blink |

Reduced-motion users: the companion is opt-in, and every state also reads
as a still (the watch always-on principle from the motion research).

## The floating button (Wispr-Flow-style)

The companion is also the product's primary control surface, like Wispr
Flow's floating button:

- **Click to dictate.** A bare click on the orb toggles recording (START
  when idle, STOP while listening) — the shells discriminate click from
  drag by movement threshold, so you can still reposition it. Wired to the
  same daemon path as the hotkey.
- **State caption.** Active states show a short uppercase word beneath the
  orb (READY / LISTENING / THINKING / WRITING / ERROR) on a soft plate, so
  the button reads its own status at a glance. Idle stays silent — just the
  mark. The caption uses a built-in 5x7 bitmap font (no font dependency,
  pixel-identical on every OS).
- **Live voice halo.** While listening, the ring and glow track the real
  microphone level.

This makes the companion a complete, mouse-only way to dictate for users
who don't want a global hotkey — the experience Wispr Flow popularized.

## Architecture — one renderer, three thin shells

All animation **math and pixels** live in shared, unit-tested code
(`src/avatar/avatar_core.cpp`): a state machine with latch rules and a
software rasterizer emitting premultiplied BGRA — the byte order X11
ARGB32, Win32 layered windows, and CoreGraphics all consume natively.
Platform shells are dumb blitters (~150 lines each):

- **Linux** `avatar_x11.cpp` — pure Xlib override-redirect ARGB window on
  its own thread (no GTK coupling); composites onto a dark plate when no
  compositor is present.
- **Windows** `avatar_win32.cpp` — topmost layered tool window via
  `UpdateLayeredWindow`, own message pump; click toggles dictation, drag
  past a threshold repositions.
- **macOS** `avatar_macos.mm` — non-activating floating `NSPanel`, layer
  contents from a `CGImage`, timer on the main run loop the daemon owns;
  `movableByWindowBackground` for dragging.

`autowhisper avatar demo` cycles every state with a synthetic voice — the
runtime smoke used on each platform, no model or microphone needed.

## Verification record (2026-06-12)

| platform | build | runtime |
|---|---|---|
| Linux x86_64 | release binary | full demo under Xvfb, recorded; compositing-fallback bug found by the recording and fixed |
| Windows x86_64 | MinGW PE32+, **self-contained** (Wine smoke caught missing MinGW runtime DLLs → static link + `GGML_OPENMP=OFF`) | full demo under Wine, recorded, pixel-identical to Linux |
| macOS | written to the proven `.mm` patterns; compiled by the macos-14 branch CI | needs a real Mac session (tracked) |

Unit tests pin the registry, the latch rules, renderer invariants
(determinism, premultiplication, level-reactivity, size independence), and
the state-label captions.

## Future ("talks to you, understands you")

The shells expose only `set_state` + providers, so the planned
conversational layer (M4 LLM tier) can drive the same body — a speech
bubble panel is additive. No groundwork in this slice pretends otherwise.
