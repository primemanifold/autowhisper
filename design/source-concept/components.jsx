// AutoWhisper — Components artboard
// Buttons, inputs, switches, fields, status pills, cards, kbd, waveform, etc.

const { useState: useStateC } = React;

function ComponentsArtboard() {
  const [tog, setTog] = useStateC(true);
  const [seg, setSeg] = useStateC('cpu');
  const [val, setVal] = useStateC('Right-Ctrl');

  return (
    <div style={{ padding: '48px 56px', background: 'var(--aw-paper-1)', height: '100%', overflow: 'auto' }}>
      <div className="aw-eyebrow" style={{ marginBottom: 8 }}>07 · Components</div>
      <h2 style={{ fontSize: 28, fontWeight: 600, letterSpacing: -0.4, margin: 0, marginBottom: 24 }}>Building blocks.</h2>

      <Group label="Buttons">
        <AwBtn variant="primary">Save changes</AwBtn>
        <AwBtn>Reset</AwBtn>
        <AwBtn variant="ghost">Cancel</AwBtn>
        <AwBtn variant="danger">Delete model</AwBtn>
        <AwBtn disabled>Disabled</AwBtn>
        <AwBtn variant="primary" loading>Downloading</AwBtn>
      </Group>

      <Group label="Status pills">
        <Pill tone="ok" dot>Ready</Pill>
        <Pill tone="signal" dot>Recording</Pill>
        <Pill tone="info" dot>Transcribing</Pill>
        <Pill tone="warn" dot>Model not loaded</Pill>
        <Pill tone="err" dot>No microphone</Pill>
        <Pill tone="muted">Idle</Pill>
      </Group>

      <Group label="Form fields">
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 16, width: '100%' }}>
          <Field label="Hotkey" hint="Hold to dictate, release to transcribe." configKey="dictation.hotkey">
            <KbdInput value={val} onChange={setVal} />
          </Field>
          <Field label="Audio device" configKey="audio.device">
            <Select value="pulse" options={['pulse', 'alsa:hw:0,0', 'pipewire']} />
          </Field>
          <Field label="Model" configKey="model.name">
            <Select value="tiny.en (74 MB)" options={['tiny.en (74 MB)', 'base.en (142 MB)', 'small.en (466 MB)']} />
          </Field>
          <Field label="Compute" configKey="model.device">
            <Segmented value={seg} onChange={setSeg} options={[{ v: 'cpu', l: 'CPU' }, { v: 'cuda', l: 'CUDA' }, { v: 'rocm', l: 'ROCm' }]} />
          </Field>
        </div>
      </Group>

      <Group label="Toggle row">
        <ToggleRow label="Insert text at cursor" hint="Uses XTest. Falls back to clipboard if denied." configKey="output.insert" value={tog} onChange={setTog} />
      </Group>

      <Group label="Keyboard">
        <Kbd>Right-Ctrl</Kbd> <span style={{ color: 'var(--aw-ink-3)' }}>+</span> <Kbd>Space</Kbd>
        <span style={{ marginLeft: 16 }}><Kbd mono>⌃</Kbd> <Kbd>F12</Kbd></span>
      </Group>

      <Group label="Inline status">
        <ReadoutChip k="model" v="tiny.en" />
        <ReadoutChip k="device" v="cpu · 8t" />
        <ReadoutChip k="last" v="142ms" hot />
        <ReadoutChip k="audio" v="pulse · 16kHz" />
      </Group>

      <Group label="Banner / advisory">
        <Advisory tone="warn">
          <b>No model is loaded.</b> Download <Mono>tiny.en</Mono> (74 MB) to start dictating.
          <AwBtn variant="primary" size="sm" style={{ marginLeft: 12 }}>Download</AwBtn>
        </Advisory>
        <Advisory tone="ok" style={{ marginTop: 8 }}>
          <b>Saved.</b> 3 settings written to <Mono>~/.config/autowhisper/config.toml</Mono>.
        </Advisory>
      </Group>

      <Group label="Waveform / amplitude meter">
        <Waveform amplitude={0.6} />
      </Group>
    </div>
  );
}

function Group({ label, children }) {
  return (
    <div style={{ marginBottom: 28 }}>
      <div className="aw-eyebrow" style={{ marginBottom: 10 }}>{label}</div>
      <div style={{ display: 'flex', flexWrap: 'wrap', gap: 10, alignItems: 'center' }}>{children}</div>
    </div>
  );
}

function AwBtn({ variant = 'default', size = 'md', loading, disabled, children, style, ...rest }) {
  const base = {
    fontFamily: 'var(--aw-font-sans)',
    fontSize: size === 'sm' ? 12 : 13,
    fontWeight: 500,
    letterSpacing: 0.1,
    padding: size === 'sm' ? '4px 10px' : '7px 14px',
    border: '1px solid var(--aw-line)',
    borderRadius: 4,
    background: 'var(--aw-paper-1)',
    color: 'var(--aw-ink-0)',
    cursor: disabled ? 'not-allowed' : 'pointer',
    opacity: disabled ? 0.5 : 1,
    boxShadow: 'var(--aw-shadow-1)',
    transition: 'all var(--aw-dur-2) var(--aw-ease-out)',
    display: 'inline-flex', alignItems: 'center', gap: 6,
    ...style,
  };
  const variants = {
    primary: { background: 'var(--aw-ink-0)', color: 'var(--aw-paper-0)', borderColor: 'var(--aw-ink-0)' },
    danger: { background: 'var(--aw-paper-1)', color: 'var(--aw-err)', borderColor: 'var(--aw-err)' },
    ghost: { background: 'transparent', borderColor: 'transparent', boxShadow: 'none' },
  };
  return (
    <button {...rest} disabled={disabled} style={{ ...base, ...(variants[variant] || {}) }}>
      {loading && <Spinner size={10} />} {children}
    </button>
  );
}

function Spinner({ size = 12 }) {
  return (
    <span style={{ display: 'inline-block', width: size, height: size, border: '1.5px solid currentColor', borderRightColor: 'transparent', borderRadius: '50%', animation: 'awspin 700ms linear infinite' }} />
  );
}

function Pill({ tone = 'muted', dot, children }) {
  const colors = {
    ok: { bg: 'var(--aw-ok-soft)', fg: 'var(--aw-ok)', dot: 'var(--aw-ok)' },
    err: { bg: 'var(--aw-err-soft)', fg: 'var(--aw-err)', dot: 'var(--aw-err)' },
    warn: { bg: 'var(--aw-warn-soft)', fg: 'var(--aw-warn)', dot: 'var(--aw-warn)' },
    info: { bg: 'var(--aw-info-soft)', fg: 'var(--aw-info)', dot: 'var(--aw-info)' },
    signal: { bg: 'var(--aw-signal-soft)', fg: 'var(--aw-signal-ink)', dot: 'var(--aw-signal)' },
    muted: { bg: 'var(--aw-paper-2)', fg: 'var(--aw-ink-2)', dot: 'var(--aw-ink-3)' },
  };
  const c = colors[tone];
  return (
    <span style={{
      display: 'inline-flex', alignItems: 'center', gap: 6, padding: '3px 9px',
      background: c.bg, color: c.fg, borderRadius: 999, fontSize: 11, fontWeight: 500,
      fontFamily: 'var(--aw-font-mono)', letterSpacing: 0.04, textTransform: 'lowercase',
    }}>
      {dot && <span style={{ width: 6, height: 6, borderRadius: 999, background: c.dot, animation: tone === 'signal' ? 'awpulse 1.6s ease-in-out infinite' : 'none' }} />}
      {children}
    </span>
  );
}

function Field({ label, hint, configKey, children }) {
  return (
    <div>
      <div style={{ display: 'flex', alignItems: 'baseline', justifyContent: 'space-between', marginBottom: 4 }}>
        <label style={{ fontSize: 13, fontWeight: 500, color: 'var(--aw-ink-1)' }}>{label}</label>
        {configKey && <span className="aw-mono" style={{ fontSize: 10, color: 'var(--aw-ink-3)' }}>{configKey}</span>}
      </div>
      {children}
      {hint && <div style={{ fontSize: 12, color: 'var(--aw-ink-3)', marginTop: 4 }}>{hint}</div>}
    </div>
  );
}

function KbdInput({ value, onChange }) {
  return (
    <div style={{ height: 32, padding: '0 10px', display: 'flex', alignItems: 'center', gap: 6, background: 'var(--aw-paper-2)', border: '1px solid var(--aw-line)', borderRadius: 4 }}>
      <Kbd>{value}</Kbd>
      <span style={{ fontSize: 11, color: 'var(--aw-ink-3)', fontFamily: 'var(--aw-font-mono)' }}>press a key to rebind</span>
    </div>
  );
}

function Select({ value, options }) {
  return (
    <div style={{ position: 'relative', width: '100%' }}>
      <div style={{ height: 32, padding: '0 28px 0 10px', display: 'flex', alignItems: 'center', background: 'var(--aw-paper-1)', border: '1px solid var(--aw-line)', borderRadius: 4, fontSize: 13, fontFamily: 'var(--aw-font-mono)', color: 'var(--aw-ink-0)' }}>
        {value}
      </div>
      <svg width="10" height="10" viewBox="0 0 10 10" style={{ position: 'absolute', right: 10, top: '50%', transform: 'translateY(-50%)', color: 'var(--aw-ink-3)' }} fill="none" stroke="currentColor" strokeWidth="1.5"><path d="M2 4l3 3 3-3" /></svg>
    </div>
  );
}

function Segmented({ value, onChange, options }) {
  return (
    <div style={{ display: 'inline-flex', background: 'var(--aw-paper-2)', border: '1px solid var(--aw-line)', borderRadius: 4, padding: 2, gap: 2, height: 32 }}>
      {options.map((o) => (
        <button key={o.v} onClick={() => onChange(o.v)} style={{
          padding: '0 12px', fontSize: 12, fontFamily: 'var(--aw-font-mono)', fontWeight: 500,
          background: value === o.v ? 'var(--aw-paper-1)' : 'transparent',
          color: value === o.v ? 'var(--aw-ink-0)' : 'var(--aw-ink-2)',
          border: '1px solid', borderColor: value === o.v ? 'var(--aw-line)' : 'transparent',
          borderRadius: 3, cursor: 'pointer', boxShadow: value === o.v ? 'var(--aw-shadow-1)' : 'none',
        }}>{o.l}</button>
      ))}
    </div>
  );
}

function ToggleRow({ label, hint, configKey, value, onChange }) {
  return (
    <div style={{ display: 'flex', alignItems: 'flex-start', justifyContent: 'space-between', padding: '12px 16px', background: 'var(--aw-paper-2)', border: '1px solid var(--aw-line-2)', borderRadius: 4, width: '100%' }}>
      <div style={{ flex: 1 }}>
        <div style={{ display: 'flex', alignItems: 'baseline', gap: 10 }}>
          <span style={{ fontSize: 14, fontWeight: 500 }}>{label}</span>
          {configKey && <span className="aw-mono" style={{ fontSize: 10, color: 'var(--aw-ink-3)' }}>{configKey}</span>}
        </div>
        {hint && <div style={{ fontSize: 12, color: 'var(--aw-ink-3)', marginTop: 2 }}>{hint}</div>}
      </div>
      <Switch on={value} onChange={() => onChange(!value)} />
    </div>
  );
}

function Switch({ on, onChange }) {
  return (
    <button onClick={onChange} style={{
      width: 36, height: 20, padding: 2, borderRadius: 999,
      background: on ? 'var(--aw-ink-0)' : 'var(--aw-line)', border: 'none', cursor: 'pointer',
      transition: 'background var(--aw-dur-2) var(--aw-ease-out)', flexShrink: 0,
    }}>
      <span style={{ display: 'block', width: 16, height: 16, borderRadius: '50%', background: 'var(--aw-paper-1)', transform: `translateX(${on ? 16 : 0}px)`, transition: 'transform var(--aw-dur-2) var(--aw-ease-out)', boxShadow: '0 1px 2px rgba(0,0,0,0.15)' }} />
    </button>
  );
}

function Kbd({ children, mono }) {
  return (
    <kbd style={{
      display: 'inline-flex', alignItems: 'center', padding: '2px 7px',
      background: 'var(--aw-paper-1)', border: '1px solid var(--aw-line)',
      borderBottomWidth: 2, borderRadius: 3,
      fontFamily: 'var(--aw-font-mono)', fontSize: 11, fontWeight: 500, color: 'var(--aw-ink-0)',
      lineHeight: 1.4, letterSpacing: 0,
    }}>{children}</kbd>
  );
}

function Mono({ children }) {
  return <span style={{ fontFamily: 'var(--aw-font-mono)', fontSize: '0.92em', background: 'var(--aw-paper-2)', padding: '1px 5px', borderRadius: 3, color: 'var(--aw-ink-1)' }}>{children}</span>;
}

function ReadoutChip({ k, v, hot }) {
  return (
    <span style={{
      display: 'inline-flex', alignItems: 'center', gap: 6, padding: '3px 8px',
      border: '1px solid var(--aw-line)', borderRadius: 3, fontFamily: 'var(--aw-font-mono)', fontSize: 11,
      background: 'var(--aw-paper-1)',
    }}>
      <span style={{ color: 'var(--aw-ink-3)' }}>{k}</span>
      <span style={{ width: 1, height: 11, background: 'var(--aw-line)' }} />
      <span style={{ color: hot ? 'var(--aw-signal)' : 'var(--aw-ink-0)', fontWeight: 500 }}>{v}</span>
    </span>
  );
}

function Advisory({ tone = 'info', children, style }) {
  const colors = {
    ok:   { bg: 'var(--aw-ok-soft)',   bd: 'var(--aw-ok)' },
    warn: { bg: 'var(--aw-warn-soft)', bd: 'var(--aw-warn)' },
    err:  { bg: 'var(--aw-err-soft)',  bd: 'var(--aw-err)' },
    info: { bg: 'var(--aw-info-soft)', bd: 'var(--aw-info)' },
  };
  const c = colors[tone];
  return (
    <div style={{
      display: 'flex', alignItems: 'center', gap: 10, padding: '10px 14px',
      background: c.bg, borderLeft: `3px solid ${c.bd}`, fontSize: 13, color: 'var(--aw-ink-0)',
      width: '100%', ...style,
    }}>
      <div style={{ flex: 1 }}>{children}</div>
    </div>
  );
}

// 16-bar quantized waveform — engineered, not smoothed.
function Waveform({ amplitude = 0.5, bars = 32, animated = true }) {
  const [seed, setSeed] = useStateC(0);
  React.useEffect(() => {
    if (!animated) return;
    const id = setInterval(() => setSeed((x) => x + 1), 90);
    return () => clearInterval(id);
  }, [animated]);
  // deterministic per-bar amplitude that shifts over time
  const heights = Array.from({ length: bars }, (_, i) => {
    const phase = (i / bars) * Math.PI * 4 + seed * 0.4;
    const env = Math.sin((i / bars) * Math.PI);
    const noise = Math.abs(Math.sin(seed * 7.13 + i * 1.31));
    const a = (Math.abs(Math.sin(phase)) * env * 0.7 + noise * 0.3) * amplitude;
    return Math.max(0.06, Math.min(1, a));
  });
  return (
    <div style={{ display: 'flex', alignItems: 'center', gap: 2, height: 36, padding: '0 4px', background: 'var(--aw-paper-2)', border: '1px solid var(--aw-line-2)', borderRadius: 3 }}>
      {heights.map((h, i) => (
        <span key={i} style={{ display: 'block', width: 3, height: `${Math.round(h * 32) / 32 * 100}%`, background: i < bars / 2 ? 'var(--aw-signal)' : 'var(--aw-ink-1)', transition: 'height 60ms steps(8, end)' }} />
      ))}
      <span className="aw-mono" style={{ marginLeft: 'auto', fontSize: 10, color: 'var(--aw-ink-2)', paddingRight: 6 }}>
        {(amplitude * 100 | 0).toString().padStart(2, '0')} dB · 16kHz
      </span>
    </div>
  );
}

window.ComponentsArtboard = ComponentsArtboard;
window.AwBtn = AwBtn;
window.Pill = Pill;
window.Field = Field;
window.Select = Select;
window.Segmented = Segmented;
window.ToggleRow = ToggleRow;
window.Switch = Switch;
window.Kbd = Kbd;
window.Mono = Mono;
window.ReadoutChip = ReadoutChip;
window.Advisory = Advisory;
window.Waveform = Waveform;
window.KbdInput = KbdInput;
window.Spinner = Spinner;
