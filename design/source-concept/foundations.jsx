// AutoWhisper — Foundations artboards
// Thesis, Principles, Color, Type, Spacing, Motion, Focus, Iconography, Voice

const { useState, useEffect, useRef } = React;

// ─────────────────────────────────────────────────────────────
// THESIS
// ─────────────────────────────────────────────────────────────
function ThesisArtboard() {
  return (
    <div style={{
      padding: '64px 72px', background: 'var(--aw-paper-1)', height: '100%',
      display: 'flex', flexDirection: 'column', gap: 32,
    }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: 14 }}>
        <AwMark size={28} />
        <div className="aw-eyebrow">AutoWhisper · Design Thesis</div>
      </div>
      <div style={{
        fontFamily: 'var(--aw-font-serif)', fontSize: 32, lineHeight: 1.32,
        color: 'var(--aw-ink-0)', letterSpacing: -0.5, maxWidth: 880,
        textWrap: 'pretty',
      }}>
        AutoWhisper should feel like a <em style={{ color: 'var(--aw-signal)', fontStyle: 'normal', borderBottom: '2px solid var(--aw-signal)' }}>well-tuned instrument</em>{" "}
        sitting next to your keyboard — quiet between presses, instantly responsive when you reach for it,
        and entirely yours. It is not a chatbot, not an assistant, and not a service.
        It runs on your machine, transcribes your voice with a model you chose and downloaded,
        and then gets out of the way. The interface should reward operators who want to know
        exactly what's happening — model, latency, device, sample rate — without making
        anyone read a manual to dictate a sentence.
      </div>

      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: 24, marginTop: 24 }}>
        <PrincipleStub n="01" t="Local by construction" b="No telemetry. No cloud. No 'optional' upload." />
        <PrincipleStub n="02" t="Legible mechanism" b="Surface the model, the latency, the device. Always." />
        <PrincipleStub n="03" t="Calm at rest" b="When idle, AutoWhisper is invisible. When working, it is precise." />
      </div>

      <div style={{ flex: 1 }} />
      <div style={{
        fontFamily: 'var(--aw-font-mono)', fontSize: 12, color: 'var(--aw-ink-3)',
        display: 'flex', justifyContent: 'space-between', borderTop: '1px solid var(--aw-line-2)', paddingTop: 16,
      }}>
        <span>autowhisper · v0.4.x · linux/x11</span>
        <span>design system · v1.0 · 2026</span>
      </div>
    </div>
  );
}
function PrincipleStub({ n, t, b }) {
  return (
    <div style={{ borderTop: '1px solid var(--aw-line)', paddingTop: 12 }}>
      <div className="aw-mono" style={{ fontSize: 11, color: 'var(--aw-signal)' }}>{n}</div>
      <div style={{ fontWeight: 600, fontSize: 16, marginTop: 4 }}>{t}</div>
      <div style={{ fontSize: 13, color: 'var(--aw-ink-2)', marginTop: 4, lineHeight: 1.5 }}>{b}</div>
    </div>
  );
}

// AutoWhisper "mark" — a record dot inside a bracket. Not a logo, but a recurring visual signature.
function AwMark({ size = 24, recording = false }) {
  return (
    <svg width={size} height={size} viewBox="0 0 24 24" fill="none">
      <path d="M5 4 L3 4 L3 20 L5 20" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round" fill="none" />
      <path d="M19 4 L21 4 L21 20 L19 20" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round" fill="none" />
      <circle cx="12" cy="12" r="3.5" fill={recording ? 'var(--aw-signal)' : 'var(--aw-ink-0)'} />
      {recording && <circle cx="12" cy="12" r="3.5" fill="var(--aw-signal)" opacity="0.4">
        <animate attributeName="r" values="3.5;7;3.5" dur="1.6s" repeatCount="indefinite" />
        <animate attributeName="opacity" values="0.5;0;0.5" dur="1.6s" repeatCount="indefinite" />
      </circle>}
    </svg>
  );
}

// ─────────────────────────────────────────────────────────────
// PRINCIPLES — full card
// ─────────────────────────────────────────────────────────────
const PRINCIPLES = [
  {
    n: '01', name: 'Local by construction',
    body: 'AutoWhisper never makes a network call we did not document. There is no analytics, no "anonymous" telemetry, no model that "may use the cloud." This is a constraint, not a setting.',
    example: 'The Privacy section in Settings does not contain a single toggle. Instead, it shows a live readout of every outbound socket the process holds open — usually zero.',
  },
  {
    n: '02', name: 'Legible mechanism',
    body: 'A user who wants to know what the app is doing should always be able to find out. Model name, sample rate, last latency, current device, log path — surfaced, not buried.',
    example: 'The tray tooltip reads "tiny.en · 16kHz · pulse · 142ms last." The status bar in Settings shows the same. doctor reads from the same shared store.',
  },
  {
    n: '03', name: 'Calm at rest',
    body: 'When AutoWhisper is not recording, it should be visually quiet — a single tray glyph and nothing else. No ambient animations, no dashboards begging to be opened.',
    example: 'The recording HUD is opt-in and dismisses to a tray-only state. There is no "welcome back" notification. The app does not announce itself.',
  },
  {
    n: '04', name: 'Precise when working',
    body: 'The moment a user holds the hotkey, the interface becomes mechanical and exact. Waveform, elapsed time, device — all monospaced, all stable, no easing curves that lie.',
    example: 'Recording HUD shows a real-time amplitude bar quantized to 16 levels — not a smoothed gradient. Time advances in 100ms ticks. No bounce, no ease-in-out.',
  },
  {
    n: '05', name: 'Operator-grade by default',
    body: 'AutoWhisper is built for people who like a CLI. The GUI must hold its own with that audience: respect keyboard, expose everything, never hide behind progressive disclosure.',
    example: 'Every setting has an exact config-file key shown beside its UI control. Save buttons preview the JSON diff. Power users can edit config.toml directly and the UI reloads.',
  },
  {
    n: '06', name: 'Trust through specificity',
    body: 'Instead of vague reassurance ("your data is safe"), be specific ("audio is held in RAM for the duration of the recording, then freed; the model file is at ~/.local/share/autowhisper/models/").',
    example: 'The first-run privacy screen lists exact paths, exact lifetimes, and exact processes. No marketing copy. No padlock icon.',
  },
];

function PrinciplesArtboard() {
  return (
    <div style={{ padding: '56px 64px', background: 'var(--aw-paper-1)', height: '100%', overflow: 'auto' }}>
      <div className="aw-eyebrow" style={{ marginBottom: 8 }}>02 · Principles</div>
      <h2 style={{
        fontSize: 36, fontWeight: 600, letterSpacing: -0.6, margin: 0, marginBottom: 40,
        fontFamily: 'var(--aw-font-sans)',
      }}>
        Six commitments that shape every screen.
      </h2>
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '40px 56px' }}>
        {PRINCIPLES.map((p) => (
          <div key={p.n} style={{ borderTop: '1.5px solid var(--aw-ink-0)', paddingTop: 18 }}>
            <div style={{ display: 'flex', alignItems: 'baseline', gap: 12, marginBottom: 8 }}>
              <span className="aw-mono" style={{ fontSize: 12, color: 'var(--aw-signal)', fontWeight: 600 }}>{p.n}</span>
              <h3 style={{ margin: 0, fontSize: 20, fontWeight: 600, letterSpacing: -0.2 }}>{p.name}</h3>
            </div>
            <p style={{ margin: 0, fontSize: 14, lineHeight: 1.55, color: 'var(--aw-ink-1)', textWrap: 'pretty' }}>{p.body}</p>
            <div style={{
              marginTop: 14, padding: '10px 12px', background: 'var(--aw-paper-2)',
              borderLeft: '2px solid var(--aw-ink-3)',
              fontSize: 13, lineHeight: 1.5, color: 'var(--aw-ink-2)',
            }}>
              <span className="aw-eyebrow" style={{ display: 'block', marginBottom: 4, fontSize: 9 }}>In practice</span>
              {p.example}
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}

// ─────────────────────────────────────────────────────────────
// COLOR
// ─────────────────────────────────────────────────────────────
function ColorArtboard() {
  const Group = ({ title, swatches }) => (
    <div>
      <div className="aw-eyebrow" style={{ marginBottom: 10 }}>{title}</div>
      <div style={{ display: 'flex', gap: 1, marginBottom: 8, border: '1px solid var(--aw-line)' }}>
        {swatches.map((s) => (
          <div key={s.name} style={{ flex: 1, background: s.bg, color: s.ink ?? 'var(--aw-ink-0)', padding: '24px 12px 12px', minHeight: 96, display: 'flex', flexDirection: 'column', justifyContent: 'space-between' }}>
            <div className="aw-mono" style={{ fontSize: 10, opacity: 0.7 }}>{s.name}</div>
            <div className="aw-mono" style={{ fontSize: 11, fontWeight: 500 }}>{s.hex}</div>
          </div>
        ))}
      </div>
    </div>
  );
  return (
    <div style={{ padding: '48px 56px', background: 'var(--aw-paper-1)', height: '100%', overflow: 'auto' }}>
      <div className="aw-eyebrow" style={{ marginBottom: 8 }}>03 · Color</div>
      <h2 style={{ fontSize: 28, fontWeight: 600, letterSpacing: -0.4, margin: 0, marginBottom: 8 }}>One signal. Four states. The rest is paper.</h2>
      <p style={{ fontSize: 14, color: 'var(--aw-ink-2)', marginTop: 0, marginBottom: 32, maxWidth: 640, lineHeight: 1.55 }}>
        Color carries meaning. We use a single warm-red signal accent for the recording moment and brand recall, four functional states (ok / warn / err / info), and a long paper-neutral ramp for everything else. Backgrounds are never pure white.
      </p>

      <div style={{ display: 'flex', flexDirection: 'column', gap: 32 }}>
        <Group title="Paper · surfaces" swatches={[
          { name: 'paper-0', hex: '#FAFAF7', bg: '#FAFAF7' },
          { name: 'paper-1', hex: '#FFFFFF', bg: '#FFFFFF' },
          { name: 'paper-2', hex: '#F4F3EE', bg: '#F4F3EE' },
          { name: 'paper-3', hex: '#ECEBE4', bg: '#ECEBE4' },
          { name: 'line-2',  hex: '#E6E4DD', bg: '#E6E4DD' },
          { name: 'line',    hex: '#D9D7CF', bg: '#D9D7CF' },
        ]} />
        <Group title="Ink · text" swatches={[
          { name: 'ink-4', hex: '#B5B0A0', bg: '#B5B0A0', ink: '#16140f' },
          { name: 'ink-3', hex: '#8A8472', bg: '#8A8472', ink: '#fff' },
          { name: 'ink-2', hex: '#5A5448', bg: '#5A5448', ink: '#fff' },
          { name: 'ink-1', hex: '#2A2620', bg: '#2A2620', ink: '#fff' },
          { name: 'ink-0', hex: '#16140F', bg: '#16140F', ink: '#fff' },
        ]} />
        <Group title="Signal · brand accent (use sparingly)" swatches={[
          { name: 'signal-soft', hex: '#F3E0D9', bg: '#F3E0D9' },
          { name: 'signal',      hex: '#B8412A', bg: '#B8412A', ink: '#fff' },
          { name: 'signal-ink',  hex: '#7A2A1A', bg: '#7A2A1A', ink: '#fff' },
        ]} />
        <Group title="Functional states" swatches={[
          { name: 'ok',     hex: '#3F7A3A', bg: '#3F7A3A', ink: '#fff' },
          { name: 'warn',   hex: '#A8721B', bg: '#A8721B', ink: '#fff' },
          { name: 'err',    hex: '#A83232', bg: '#A83232', ink: '#fff' },
          { name: 'info',   hex: '#2E5D8A', bg: '#2E5D8A', ink: '#fff' },
        ]} />

        <div>
          <div className="aw-eyebrow" style={{ marginBottom: 10 }}>Usage rules</div>
          <ul style={{ fontSize: 13, lineHeight: 1.7, color: 'var(--aw-ink-1)', margin: 0, paddingLeft: 18 }}>
            <li><b>Signal red is reserved.</b> Recording dot · primary CTA · current selection accent · the brand mark. Never used as a section background or decoration.</li>
            <li><b>Ink-0 is for headings and bold UI.</b> Body copy is ink-1 or ink-2.</li>
            <li><b>Paper-0 is the canvas, paper-1 is a surface.</b> Cards always sit on paper-0 with paper-1 fills, never the other way around.</li>
            <li><b>State backgrounds use the soft variant.</b> Never put white text on saturated state colors except in toasts and pills.</li>
          </ul>
        </div>
      </div>
    </div>
  );
}

// ─────────────────────────────────────────────────────────────
// TYPE
// ─────────────────────────────────────────────────────────────
function TypeArtboard() {
  const Row = ({ name, size, weight, family, sample, lh = 1.3 }) => (
    <div style={{ display: 'grid', gridTemplateColumns: '180px 1fr', gap: 24, padding: '14px 0', borderBottom: '1px solid var(--aw-line-2)', alignItems: 'baseline' }}>
      <div>
        <div className="aw-mono" style={{ fontSize: 11, color: 'var(--aw-ink-2)' }}>{name}</div>
        <div className="aw-mono" style={{ fontSize: 10, color: 'var(--aw-ink-3)', marginTop: 2 }}>{size}px · {weight} · {family}</div>
      </div>
      <div style={{ fontFamily: family === 'mono' ? 'var(--aw-font-mono)' : family === 'serif' ? 'var(--aw-font-serif)' : 'var(--aw-font-sans)', fontSize: size, fontWeight: weight, lineHeight: lh, letterSpacing: size > 24 ? -0.4 : size > 18 ? -0.2 : 0, color: 'var(--aw-ink-0)' }}>
        {sample}
      </div>
    </div>
  );

  return (
    <div style={{ padding: '48px 56px', background: 'var(--aw-paper-1)', height: '100%', overflow: 'auto' }}>
      <div className="aw-eyebrow" style={{ marginBottom: 8 }}>04 · Type</div>
      <h2 style={{ fontSize: 28, fontWeight: 600, letterSpacing: -0.4, margin: 0, marginBottom: 8 }}>IBM Plex — three voices, one machine.</h2>
      <p style={{ fontSize: 14, color: 'var(--aw-ink-2)', marginTop: 0, marginBottom: 24, maxWidth: 640, lineHeight: 1.55 }}>
        Plex Sans for product UI, Plex Mono for everything numeric or technical (latency, paths, log lines), Plex Serif reserved for editorial moments — the thesis, marketing headlines, the empty-state aphorism.
      </p>

      <Row name="display" size={56} weight={600} family="sans" sample="Speak. Release. Done." />
      <Row name="h1" size={36} weight={600} family="sans" sample="Dictation behavior" />
      <Row name="h2" size={28} weight={600} family="sans" sample="Model and performance" />
      <Row name="h3" size={22} weight={600} family="sans" sample="Whisper tiny.en is loaded" />
      <Row name="h4 / lead" size={18} weight={500} family="sans" sample="Hold Right-Ctrl to dictate." />
      <Row name="body" size={14} weight={400} family="sans" sample="AutoWhisper transcribes locally using whisper.cpp. Audio is never uploaded — model files live in your home directory and run on your CPU or GPU." lh={1.55} />
      <Row name="small" size={12} weight={400} family="sans" sample="Last transcription: 2.3s · 142ms latency · 47 words" />
      <Row name="micro / eyebrow" size={10} weight={500} family="mono" sample="DICTATION BEHAVIOR · §1.2" />
      <Row name="num-xl" size={44} weight={500} family="mono" sample="142ms" />
      <Row name="num-lg" size={22} weight={500} family="mono" sample="16.0kHz · 32bit · 1ch" />
      <Row name="mono body" size={13} weight={400} family="mono" sample="$ autowhisper doctor --verbose" />
      <Row name="serif lead" size={20} weight={400} family="serif" sample="A well-tuned instrument, sitting next to your keyboard." lh={1.4} />
    </div>
  );
}

// ─────────────────────────────────────────────────────────────
// SPACING + RADIUS + ELEVATION + MOTION
// ─────────────────────────────────────────────────────────────
function SpacingArtboard() {
  const steps = [
    { name: '1', v: 4 }, { name: '2', v: 8 }, { name: '3', v: 12 }, { name: '4', v: 16 },
    { name: '5', v: 24 }, { name: '6', v: 32 }, { name: '7', v: 48 }, { name: '8', v: 64 }, { name: '9', v: 96 },
  ];
  const radii = [
    { name: 'r-1', v: 2 }, { name: 'r-2', v: 4 }, { name: 'r-3', v: 6 }, { name: 'r-4', v: 10 }, { name: 'pill', v: 999 },
  ];

  return (
    <div style={{ padding: '48px 56px', background: 'var(--aw-paper-1)', height: '100%', overflow: 'auto' }}>
      <div className="aw-eyebrow" style={{ marginBottom: 8 }}>05 · Space, Radius, Elevation</div>
      <h2 style={{ fontSize: 28, fontWeight: 600, letterSpacing: -0.4, margin: 0, marginBottom: 24 }}>4px base · small radii · paper shadows.</h2>

      <div className="aw-eyebrow" style={{ marginBottom: 12 }}>Spacing scale</div>
      <div style={{ display: 'flex', flexDirection: 'column', gap: 4, marginBottom: 32 }}>
        {steps.map((s) => (
          <div key={s.name} style={{ display: 'grid', gridTemplateColumns: '60px 60px 1fr', gap: 12, alignItems: 'center' }}>
            <div className="aw-mono" style={{ fontSize: 11, color: 'var(--aw-ink-2)' }}>s-{s.name}</div>
            <div className="aw-mono" style={{ fontSize: 11, color: 'var(--aw-ink-3)', textAlign: 'right' }}>{s.v}px</div>
            <div style={{ height: 12, width: s.v, background: 'var(--aw-signal)' }} />
          </div>
        ))}
      </div>

      <div className="aw-eyebrow" style={{ marginBottom: 12 }}>Radius</div>
      <div style={{ display: 'flex', gap: 16, marginBottom: 32 }}>
        {radii.map((r) => (
          <div key={r.name} style={{ textAlign: 'center' }}>
            <div style={{ width: 64, height: 64, background: 'var(--aw-paper-2)', border: '1px solid var(--aw-line)', borderRadius: r.v }} />
            <div className="aw-mono" style={{ fontSize: 10, color: 'var(--aw-ink-2)', marginTop: 6 }}>{r.name}</div>
            <div className="aw-mono" style={{ fontSize: 10, color: 'var(--aw-ink-3)' }}>{r.v === 999 ? '∞' : r.v + 'px'}</div>
          </div>
        ))}
      </div>

      <div className="aw-eyebrow" style={{ marginBottom: 12 }}>Elevation</div>
      <div style={{ display: 'flex', gap: 32, marginBottom: 32 }}>
        {['shadow-1', 'shadow-2', 'shadow-3'].map((s, i) => (
          <div key={s} style={{ width: 120, height: 80, background: 'var(--aw-paper-1)', border: '1px solid var(--aw-line-2)', borderRadius: 4, boxShadow: `var(--aw-${s})`, display: 'flex', alignItems: 'center', justifyContent: 'center', fontFamily: 'var(--aw-font-mono)', fontSize: 11, color: 'var(--aw-ink-2)' }}>
            {s}
          </div>
        ))}
      </div>

      <div className="aw-eyebrow" style={{ marginBottom: 12 }}>Motion</div>
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 12, fontSize: 13, color: 'var(--aw-ink-1)' }}>
        <Cell><span className="aw-mono" style={{ color: 'var(--aw-signal)' }}>dur-1 · 80ms</span> — state flips, focus rings, key feedback</Cell>
        <Cell><span className="aw-mono" style={{ color: 'var(--aw-signal)' }}>dur-2 · 140ms</span> — hover, button press, dropdown</Cell>
        <Cell><span className="aw-mono" style={{ color: 'var(--aw-signal)' }}>dur-3 · 220ms</span> — panel slide, drawer, modal</Cell>
        <Cell><span className="aw-mono" style={{ color: 'var(--aw-signal)' }}>ease-out</span> — default. 0.2, 0.7, 0.3, 1</Cell>
        <Cell><span className="aw-mono" style={{ color: 'var(--aw-signal)' }}>ease-step(8)</span> — used for waveform, latency ticks, terminal feel</Cell>
        <Cell style={{ color: 'var(--aw-ink-3)' }}><i>No spring. No bounce. No glow pulses.</i></Cell>
      </div>
    </div>
  );
}
function Cell({ children, style }) {
  return <div style={{ padding: '10px 12px', background: 'var(--aw-paper-2)', border: '1px solid var(--aw-line-2)', borderRadius: 4, ...style }}>{children}</div>;
}

// ─────────────────────────────────────────────────────────────
// ICONOGRAPHY + VOICE
// ─────────────────────────────────────────────────────────────
function VoiceArtboard() {
  const Pair = ({ bad, good, why }) => (
    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 12, marginBottom: 14 }}>
      <div style={{ padding: 12, background: 'var(--aw-err-soft)', borderLeft: '3px solid var(--aw-err)', fontSize: 13, color: 'var(--aw-ink-1)' }}>
        <div className="aw-eyebrow" style={{ color: 'var(--aw-err)', fontSize: 9, marginBottom: 4 }}>✗ AVOID</div>
        {bad}
      </div>
      <div style={{ padding: 12, background: 'var(--aw-ok-soft)', borderLeft: '3px solid var(--aw-ok)', fontSize: 13, color: 'var(--aw-ink-1)' }}>
        <div className="aw-eyebrow" style={{ color: 'var(--aw-ok)', fontSize: 9, marginBottom: 4 }}>✓ USE</div>
        {good}
        {why && <div style={{ fontSize: 11, color: 'var(--aw-ink-3)', marginTop: 6, fontStyle: 'italic' }}>{why}</div>}
      </div>
    </div>
  );
  return (
    <div style={{ padding: '48px 56px', background: 'var(--aw-paper-1)', height: '100%', overflow: 'auto' }}>
      <div className="aw-eyebrow" style={{ marginBottom: 8 }}>06 · Voice</div>
      <h2 style={{ fontSize: 28, fontWeight: 600, letterSpacing: -0.4, margin: 0, marginBottom: 24 }}>Three registers. Know which surface you're on.</h2>

      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: 14, marginBottom: 32 }}>
        <RegisterCard name="UI · Plainspoken" font="sans" example="Recording. Release Right-Ctrl to transcribe." note="Default for buttons, labels, inline help. Active voice. No hedges." />
        <RegisterCard name="CLI · Technical" font="mono" example="[ok] tiny.en  74M  loaded in 312ms (cpu)" note="In doctor / logs / status output. Bracketed states, fixed-width tables, exact numbers." />
        <RegisterCard name="Privacy · Specific" font="serif" example="Audio is held in RAM for the duration of the recording, then freed." note="On privacy screens and first-run. Reassure with facts, not adjectives." />
      </div>

      <div className="aw-eyebrow" style={{ marginBottom: 12 }}>Rules of thumb</div>
      <Pair
        bad="🎙️ Listening… we're getting your voice safely!"
        good="Recording. Hold Right-Ctrl, release to transcribe."
        why="No emoji. No 'we'. No reassurance words; the action itself is the reassurance."
      />
      <Pair
        bad="Something went wrong. Please try again."
        good="No microphone found. Run autowhisper doctor or open Audio input in Settings."
        why="Always specify the failure and at least one path forward — CLI command + GUI location."
      />
      <Pair
        bad="Your data is private and secure."
        good="No outbound connections active. Last network call: never."
        why="Replace adjectives with measurable facts."
      />
      <Pair
        bad="Loading your model…"
        good="Loading tiny.en (74 MB) · 312ms"
        why="Always say which model. Always show elapsed."
      />
    </div>
  );
}
function RegisterCard({ name, font, example, note }) {
  return (
    <div style={{ border: '1px solid var(--aw-line)', padding: 14 }}>
      <div className="aw-eyebrow" style={{ marginBottom: 8 }}>{name}</div>
      <div style={{ fontFamily: font === 'mono' ? 'var(--aw-font-mono)' : font === 'serif' ? 'var(--aw-font-serif)' : 'var(--aw-font-sans)', fontSize: font === 'mono' ? 13 : 15, color: 'var(--aw-ink-0)', marginBottom: 10, fontStyle: font === 'serif' ? 'italic' : 'normal' }}>
        "{example}"
      </div>
      <div style={{ fontSize: 12, color: 'var(--aw-ink-2)', lineHeight: 1.5 }}>{note}</div>
    </div>
  );
}

window.ThesisArtboard = ThesisArtboard;
window.PrinciplesArtboard = PrinciplesArtboard;
window.ColorArtboard = ColorArtboard;
window.TypeArtboard = TypeArtboard;
window.SpacingArtboard = SpacingArtboard;
window.VoiceArtboard = VoiceArtboard;
window.AwMark = AwMark;
