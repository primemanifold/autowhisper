// AutoWhisper — Interactive Settings Prototype
// The redesigned settings app, embedded inside an artboard. Fully interactive.

const { useState: useSP, useEffect: useEP, useRef: useRP } = React;

// IA from ia-map.jsx, materialized as nav.
const NAV = [
  { id: 'overview',   label: 'Overview',                hot: true,  icon: '◯' },
  { id: 'dictation',  label: 'Dictation',                          icon: '⏵' },
  { id: 'model',      label: 'Model & performance',                 icon: '◈' },
  { id: 'audio',      label: 'Audio input',                         icon: '◉' },
  { id: 'output',     label: 'Output & insertion',                  icon: '⇥' },
  { id: 'privacy',    label: 'Privacy & local',                     icon: '⌘' },
  { id: 'feedback',   label: 'Feedback & tray',                     icon: '◐' },
  { id: 'diagnostics',label: 'Diagnostics',                         icon: '⚑' },
  { id: 'advanced',   label: 'Advanced · raw config',               icon: '⌥' },
];

function SettingsPrototype() {
  const [section, setSection] = useSP('overview');
  const [config, setConfig] = useSP({
    'dictation.hotkey': 'Right-Ctrl',
    'dictation.trigger_mode': 'hold',
    'dictation.min_duration_ms': 200,
    'model.name': 'tiny.en',
    'model.device': 'cpu',
    'model.threads': 8,
    'model.beam_size': 5,
    'audio.device': 'pulse',
    'audio.sample_rate': 16000,
    'audio.gain_db': 0,
    'audio.vad_enabled': true,
    'output.method': 'xtest',
    'output.fallback': 'clipboard',
    'output.append_space': true,
    'output.smart_caps': true,
    'ui.recording_hud': 'standard',
    'ui.sound_start': true,
    'ui.notify_on_error': true,
  });
  const [dirty, setDirty] = useSP({});
  const [saved, setSaved] = useSP(null);
  const [recording, setRecording] = useSP(false);

  const set = (k, v) => {
    setConfig((c) => ({ ...c, [k]: v }));
    setDirty((d) => ({ ...d, [k]: v }));
  };
  const save = () => {
    const count = Object.keys(dirty).length;
    if (!count) return;
    setSaved({ count, at: Date.now() });
    setDirty({});
    setTimeout(() => setSaved(null), 2400);
  };
  const reset = () => setDirty({});
  const dirtyCount = Object.keys(dirty).length;

  return (
    <div style={{ display: 'grid', gridTemplateColumns: '220px 1fr', height: '100%', background: 'var(--aw-paper-0)', fontFamily: 'var(--aw-font-sans)' }}>
      {/* Sidebar */}
      <aside style={{ background: 'var(--aw-paper-2)', borderRight: '1px solid var(--aw-line)', padding: '20px 12px', display: 'flex', flexDirection: 'column' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: 8, padding: '0 8px 16px', borderBottom: '1px solid var(--aw-line-2)', marginBottom: 12 }}>
          <window.AwMark size={20} recording={recording} />
          <div>
            <div style={{ fontSize: 13, fontWeight: 600, lineHeight: 1.1 }}>AutoWhisper</div>
            <div className="aw-mono" style={{ fontSize: 10, color: 'var(--aw-ink-3)', marginTop: 2 }}>v0.4.2 · linux/x11</div>
          </div>
        </div>

        <nav style={{ display: 'flex', flexDirection: 'column', gap: 1 }}>
          {NAV.map((n) => (
            <button key={n.id} onClick={() => setSection(n.id)} style={{
              display: 'flex', alignItems: 'center', gap: 10, padding: '7px 10px',
              border: 'none', background: section === n.id ? 'var(--aw-paper-1)' : 'transparent',
              boxShadow: section === n.id ? 'var(--aw-shadow-1)' : 'none',
              color: section === n.id ? 'var(--aw-ink-0)' : 'var(--aw-ink-2)',
              fontSize: 13, fontWeight: section === n.id ? 500 : 400, textAlign: 'left',
              borderRadius: 4, cursor: 'pointer', fontFamily: 'inherit',
              borderLeft: `2px solid ${section === n.id ? 'var(--aw-signal)' : 'transparent'}`,
              paddingLeft: 8,
            }}>
              <span style={{ fontFamily: 'var(--aw-font-mono)', color: section === n.id ? 'var(--aw-signal)' : 'var(--aw-ink-3)', fontSize: 11, width: 14 }}>{n.icon}</span>
              <span style={{ flex: 1 }}>{n.label}</span>
            </button>
          ))}
        </nav>

        <div style={{ flex: 1 }} />

        {/* Always-on status footer */}
        <div style={{ padding: '12px 8px 4px', borderTop: '1px solid var(--aw-line-2)', display: 'flex', flexDirection: 'column', gap: 6 }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
            {recording ? <window.Pill tone="signal" dot>recording</window.Pill> : <window.Pill tone="ok" dot>ready</window.Pill>}
          </div>
          <div className="aw-mono" style={{ fontSize: 10, color: 'var(--aw-ink-3)', lineHeight: 1.6 }}>
            tiny.en · 16kHz<br />
            last 142ms · p95 287ms
          </div>
          <button onClick={() => setRecording((r) => !r)} style={{ marginTop: 4, fontSize: 10, fontFamily: 'var(--aw-font-mono)', color: 'var(--aw-ink-3)', background: 'none', border: '1px dashed var(--aw-line)', borderRadius: 3, padding: '3px 6px', cursor: 'pointer' }}>
            {recording ? 'stop' : 'simulate'} recording
          </button>
        </div>
      </aside>

      {/* Main */}
      <div style={{ display: 'flex', flexDirection: 'column', minHeight: 0 }}>
        <header style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '14px 28px', borderBottom: '1px solid var(--aw-line-2)', background: 'var(--aw-paper-1)' }}>
          <div style={{ display: 'flex', alignItems: 'baseline', gap: 14 }}>
            <h1 style={{ margin: 0, fontSize: 18, fontWeight: 600, letterSpacing: -0.2 }}>{NAV.find((n) => n.id === section).label}</h1>
            <span className="aw-mono" style={{ fontSize: 11, color: 'var(--aw-ink-3)' }}>~/.config/autowhisper/config.toml</span>
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
            {saved && (
              <span className="aw-mono" style={{ fontSize: 11, color: 'var(--aw-ok)', display: 'inline-flex', alignItems: 'center', gap: 6 }}>
                <span style={{ color: 'var(--aw-ok)' }}>✓</span> saved {saved.count} {saved.count === 1 ? 'change' : 'changes'}
              </span>
            )}
            {dirtyCount > 0 && (
              <span className="aw-mono" style={{ fontSize: 11, color: 'var(--aw-warn)' }}>{dirtyCount} unsaved</span>
            )}
            <window.AwBtn variant="ghost" disabled={!dirtyCount} onClick={reset}>Reset</window.AwBtn>
            <window.AwBtn variant="primary" disabled={!dirtyCount} onClick={save}>Save changes</window.AwBtn>
          </div>
        </header>

        <main style={{ flex: 1, overflow: 'auto', padding: '28px 36px 60px' }}>
          {section === 'overview' && <Overview config={config} setSection={setSection} />}
          {section === 'dictation' && <DictationSection config={config} set={set} />}
          {section === 'model' && <ModelSection config={config} set={set} />}
          {section === 'audio' && <AudioSection config={config} set={set} />}
          {section === 'output' && <OutputSection config={config} set={set} />}
          {section === 'privacy' && <PrivacySection />}
          {section === 'feedback' && <FeedbackSection config={config} set={set} />}
          {section === 'diagnostics' && <DiagSection />}
          {section === 'advanced' && <AdvancedSection config={config} dirty={dirty} />}
        </main>
      </div>
    </div>
  );
}

// ─── Overview ─────────────────────────────────────────────────
function Overview({ config, setSection }) {
  return (
    <div>
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: 14, marginBottom: 24 }}>
        <Stat n="142" u="ms" label="last latency" sub="p95 287ms over 50" />
        <Stat n="0.18" u="×" label="real-time factor" sub="lower is faster" />
        <Stat n="0" u="" label="outbound calls (14d)" sub="local-only · verified" tone="ok" />
      </div>

      <div style={{ display: 'grid', gridTemplateColumns: '1.4fr 1fr', gap: 16 }}>
        <Card>
          <CardHead title="Quick start" sub="one paragraph, three keys" />
          <p style={{ fontSize: 14, color: 'var(--aw-ink-1)', lineHeight: 1.6, marginTop: 0 }}>
            Hold <window.Kbd>{config['dictation.hotkey']}</window.Kbd>, speak, release. AutoWhisper records on-device,
            transcribes with <Mono>{config['model.name']}</Mono>, and inserts the result at your cursor.
            To pause the daemon for five minutes, click the tray icon and choose "Pause." To stop everything,
            <Mono> systemctl --user stop autowhisper</Mono>.
          </p>
          <div style={{ display: 'flex', gap: 8, marginTop: 12 }}>
            <window.AwBtn variant="primary" onClick={() => setSection('dictation')}>Tune dictation →</window.AwBtn>
            <window.AwBtn onClick={() => setSection('model')}>Change model</window.AwBtn>
          </div>
        </Card>
        <Card>
          <CardHead title="Health" sub="autowhisper doctor · live" />
          <HealthRow ok label="binary &amp; service" v="active 2h 14m" />
          <HealthRow ok label="model" v="tiny.en · 74 MB · loaded" />
          <HealthRow ok label="audio" v="pulse · 16kHz" />
          <HealthRow ok label="output" v="xtest · clipboard fallback" />
          <HealthRow warn label="model: base.en" v="not downloaded" />
          <HealthRow ok label="network" v="0 outbound" />
          <window.AwBtn variant="ghost" style={{ marginTop: 8, fontSize: 11 }}>Run full doctor →</window.AwBtn>
        </Card>
      </div>

      <div style={{ marginTop: 16 }}>
        <window.Advisory tone="info">
          <b>Welcome.</b> AutoWhisper saved 47 transcriptions this week, totaling 14 minutes of recorded audio (now freed). View history in <a style={{ color: 'inherit' }} onClick={() => setSection('privacy')}>Privacy</a>.
        </window.Advisory>
      </div>
    </div>
  );
}
function Stat({ n, u, label, sub, tone }) {
  return (
    <div style={{ padding: '14px 18px', background: 'var(--aw-paper-1)', border: '1px solid var(--aw-line-2)', borderRadius: 4 }}>
      <div className="aw-eyebrow" style={{ marginBottom: 8 }}>{label}</div>
      <div style={{ fontFamily: 'var(--aw-font-mono)', fontSize: 32, fontWeight: 500, color: tone === 'ok' ? 'var(--aw-ok)' : 'var(--aw-ink-0)', lineHeight: 1, fontVariantNumeric: 'tabular-nums' }}>
        {n}<span style={{ fontSize: 18, color: 'var(--aw-ink-3)', fontWeight: 400 }}>{u}</span>
      </div>
      <div style={{ fontSize: 11, color: 'var(--aw-ink-3)', marginTop: 8, fontFamily: 'var(--aw-font-mono)' }}>{sub}</div>
    </div>
  );
}
function Card({ children, style }) {
  return <div style={{ padding: 18, background: 'var(--aw-paper-1)', border: '1px solid var(--aw-line-2)', borderRadius: 4, ...style }}>{children}</div>;
}
function CardHead({ title, sub }) {
  return (
    <div style={{ marginBottom: 12 }}>
      <h3 style={{ margin: 0, fontSize: 15, fontWeight: 600 }}>{title}</h3>
      {sub && <div className="aw-mono" style={{ fontSize: 10, color: 'var(--aw-ink-3)', marginTop: 2 }}>{sub}</div>}
    </div>
  );
}
function HealthRow({ ok, warn, label, v }) {
  const c = warn ? 'var(--aw-warn)' : ok ? 'var(--aw-ok)' : 'var(--aw-err)';
  const tag = warn ? '[..]' : ok ? '[ok]' : '[ err ]';
  return (
    <div style={{ display: 'flex', alignItems: 'center', gap: 10, padding: '4px 0', fontFamily: 'var(--aw-font-mono)', fontSize: 11.5, borderBottom: '1px dashed var(--aw-line-2)' }}>
      <span style={{ color: c, width: 30 }}>{tag}</span>
      <span style={{ color: 'var(--aw-ink-2)', flex: 1 }} dangerouslySetInnerHTML={{ __html: label }} />
      <span style={{ color: 'var(--aw-ink-0)' }}>{v}</span>
    </div>
  );
}
function Mono({ children }) {
  return <code style={{ fontFamily: 'var(--aw-font-mono)', fontSize: '0.9em', background: 'var(--aw-paper-2)', padding: '1px 6px', borderRadius: 3 }}>{children}</code>;
}

// ─── Sections (one section per IA group) ──────────────────────
function SectionHeader({ title, intent }) {
  return (
    <div style={{ marginBottom: 24, paddingBottom: 14, borderBottom: '1px solid var(--aw-line-2)' }}>
      <h2 style={{ margin: 0, fontSize: 22, fontWeight: 600, letterSpacing: -0.3 }}>{title}</h2>
      <div style={{ fontSize: 13, color: 'var(--aw-ink-2)', fontStyle: 'italic', marginTop: 4 }}>"{intent}"</div>
    </div>
  );
}

function DictationSection({ config, set }) {
  return (
    <div>
      <SectionHeader title="Dictation behavior" intent="How I trigger dictation and what happens around the edges." />
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 16, marginBottom: 20 }}>
        <window.Field label="Hotkey" hint="Hold to dictate, release to transcribe. Click and press a key to rebind." configKey="dictation.hotkey">
          <window.KbdInput value={config['dictation.hotkey']} onChange={(v) => set('dictation.hotkey', v)} />
        </window.Field>
        <window.Field label="Trigger mode" configKey="dictation.trigger_mode">
          <window.Segmented value={config['dictation.trigger_mode']} onChange={(v) => set('dictation.trigger_mode', v)}
            options={[{ v: 'hold', l: 'Hold' }, { v: 'toggle', l: 'Toggle' }, { v: 'tap', l: 'Tap' }]} />
        </window.Field>
      </div>
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 16, marginBottom: 20 }}>
        <window.Field label="Minimum duration" hint="Releases shorter than this are discarded. Avoids accidental triggers." configKey="dictation.min_duration_ms">
          <NumberInput value={config['dictation.min_duration_ms']} unit="ms" onChange={(v) => set('dictation.min_duration_ms', v)} />
        </window.Field>
        <window.Field label="Cooldown after transcription" configKey="dictation.cooldown_ms">
          <NumberInput value={150} unit="ms" onChange={() => {}} />
        </window.Field>
      </div>
    </div>
  );
}

function ModelSection({ config, set }) {
  const models = [
    { n: 'tiny.en', size: '74 MB', lat: '~140ms', acc: 'good' },
    { n: 'base.en', size: '142 MB', lat: '~210ms', acc: 'better' },
    { n: 'small.en', size: '466 MB', lat: '~480ms', acc: 'high' },
    { n: 'medium.en', size: '1.5 GB', lat: '~1.2s', acc: 'higher' },
  ];
  return (
    <div>
      <SectionHeader title="Model & performance" intent="Which Whisper model runs, on what hardware, and how fast." />
      <div className="aw-eyebrow" style={{ marginBottom: 8 }}>Model</div>
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 8, marginBottom: 24 }}>
        {models.map((m) => {
          const sel = config['model.name'] === m.n;
          const installed = m.n === 'tiny.en';
          return (
            <button key={m.n} onClick={() => installed && set('model.name', m.n)} style={{
              padding: '12px 14px', textAlign: 'left', border: `1px solid ${sel ? 'var(--aw-ink-0)' : 'var(--aw-line-2)'}`,
              borderRadius: 4, background: sel ? 'var(--aw-paper-1)' : 'transparent', cursor: installed ? 'pointer' : 'default',
              opacity: installed ? 1 : 0.62, fontFamily: 'inherit',
            }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: 10, marginBottom: 4 }}>
                <span style={{ width: 12, height: 12, borderRadius: '50%', border: `1.5px solid ${sel ? 'var(--aw-signal)' : 'var(--aw-line)'}`, background: sel ? 'var(--aw-signal)' : 'transparent', boxShadow: sel ? 'inset 0 0 0 2px var(--aw-paper-1)' : 'none' }} />
                <span style={{ fontFamily: 'var(--aw-font-mono)', fontWeight: 600, fontSize: 14 }}>{m.n}</span>
                <span className="aw-mono" style={{ fontSize: 11, color: 'var(--aw-ink-2)' }}>{m.size}</span>
                <span style={{ flex: 1 }} />
                {installed ? (
                  <window.Pill tone="ok">installed</window.Pill>
                ) : (
                  <button onClick={(e) => { e.stopPropagation(); }} style={{ fontSize: 11, fontFamily: 'var(--aw-font-mono)', padding: '2px 8px', border: '1px solid var(--aw-line)', background: 'var(--aw-paper-1)', borderRadius: 3, cursor: 'pointer' }}>↓ download</button>
                )}
              </div>
              <div style={{ fontSize: 11, color: 'var(--aw-ink-3)', fontFamily: 'var(--aw-font-mono)', paddingLeft: 22 }}>{m.lat} · {m.acc}</div>
            </button>
          );
        })}
      </div>

      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 16 }}>
        <window.Field label="Compute device" hint="CUDA / ROCm options appear when supported drivers are detected." configKey="model.device">
          <window.Segmented value={config['model.device']} onChange={(v) => set('model.device', v)}
            options={[{ v: 'cpu', l: 'CPU' }, { v: 'cuda', l: 'CUDA · n/a' }, { v: 'rocm', l: 'ROCm · n/a' }]} />
        </window.Field>
        <window.Field label="CPU threads" hint="Logical cores: 16 detected." configKey="model.threads">
          <NumberInput value={config['model.threads']} unit="t" onChange={(v) => set('model.threads', v)} />
        </window.Field>
        <window.Field label="Beam size" hint="Higher = more accurate, slower. 1 is greedy decoding." configKey="model.beam_size">
          <NumberInput value={config['model.beam_size']} onChange={(v) => set('model.beam_size', v)} />
        </window.Field>
        <window.Field label="Target latency" hint="Advisory only. Drives recommendations." configKey="performance.target_latency_ms">
          <NumberInput value={200} unit="ms" onChange={() => {}} />
        </window.Field>
      </div>
    </div>
  );
}

function AudioSection({ config, set }) {
  const [amp, setAmp] = useSP(0.4);
  useEP(() => { const i = setInterval(() => setAmp(0.2 + Math.random() * 0.6), 200); return () => clearInterval(i); }, []);
  return (
    <div>
      <SectionHeader title="Audio input" intent="Which mic, sample rate, and what to do about noise." />
      <div style={{ marginBottom: 20 }}>
        <window.Field label="Live signal" hint="From the device selected below.">
          <window.Waveform amplitude={amp} />
        </window.Field>
      </div>
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 16, marginBottom: 16 }}>
        <window.Field label="Device" configKey="audio.device">
          <window.Select value="pulse · default · 16kHz · 1ch" options={['pulse · default']} />
        </window.Field>
        <window.Field label="Sample rate" configKey="audio.sample_rate">
          <window.Segmented value="16k" onChange={() => {}} options={[{ v: '8k', l: '8 kHz' }, { v: '16k', l: '16 kHz' }, { v: '48k', l: '48 kHz' }]} />
        </window.Field>
      </div>
      <div style={{ marginBottom: 16 }}>
        <window.Field label="Input gain" hint="Software gain applied before transcription. Negative reduces clipping; positive boosts quiet mics." configKey="audio.gain_db">
          <Slider value={config['audio.gain_db']} min={-12} max={12} step={1} unit="dB" onChange={(v) => set('audio.gain_db', v)} />
        </window.Field>
      </div>
      <window.ToggleRow label="Voice activity detection (VAD)" hint="Trim silence from start and end. Reduces tokens decoded; rarely affects accuracy." configKey="audio.vad_enabled" value={config['audio.vad_enabled']} onChange={(v) => set('audio.vad_enabled', v)} />
    </div>
  );
}

function OutputSection({ config, set }) {
  return (
    <div>
      <SectionHeader title="Output & insertion" intent="How transcribed text reaches the cursor." />
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 16, marginBottom: 20 }}>
        <window.Field label="Primary method" hint="XTest types directly into the focused window. Clipboard pastes via xdotool." configKey="output.method">
          <window.Segmented value={config['output.method']} onChange={(v) => set('output.method', v)}
            options={[{ v: 'xtest', l: 'XTest' }, { v: 'clipboard', l: 'Clipboard' }, { v: 'stdout', l: 'stdout' }]} />
        </window.Field>
        <window.Field label="Fallback" configKey="output.fallback">
          <window.Segmented value={config['output.fallback']} onChange={(v) => set('output.fallback', v)}
            options={[{ v: 'clipboard', l: 'Clipboard' }, { v: 'none', l: 'None' }]} />
        </window.Field>
      </div>
      <window.ToggleRow label="Append space after insert" configKey="output.append_space" value={config['output.append_space']} onChange={(v) => set('output.append_space', v)} />
      <div style={{ height: 8 }} />
      <window.ToggleRow label="Smart capitalization" hint="Capitalize first letter of sentence; lowercase otherwise." configKey="output.smart_caps" value={config['output.smart_caps']} onChange={(v) => set('output.smart_caps', v)} />
    </div>
  );
}

function PrivacySection() {
  return (
    <div>
      <SectionHeader title="Privacy & local operation" intent="Proof — not promise — that nothing leaves this machine." />
      <div style={{ background: 'var(--aw-paper-1)', border: '1px solid var(--aw-line-2)', borderRadius: 4, padding: 18, marginBottom: 16 }}>
        <div className="aw-eyebrow" style={{ marginBottom: 12 }}>Network · live</div>
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 12 }}>
          <DataRow label="Outbound sockets" v="0 active" tone="ok" />
          <DataRow label="Last network call" v="never" tone="ok" />
          <DataRow label="Telemetry" v="off · compiled-out" tone="ok" />
          <DataRow label="Crash reporting" v="off · compiled-out" tone="ok" />
        </div>
      </div>
      <div style={{ background: 'var(--aw-paper-1)', border: '1px solid var(--aw-line-2)', borderRadius: 4, padding: 18, marginBottom: 16 }}>
        <div className="aw-eyebrow" style={{ marginBottom: 12 }}>Data on disk</div>
        <DataRow label="Models" v="~/.local/share/autowhisper/models/ · 74 MB" />
        <DataRow label="Config" v="~/.config/autowhisper/config.toml · 2.1 KB" />
        <DataRow label="Logs (rotated weekly)" v="~/.cache/autowhisper/log · 312 KB" />
        <DataRow label="Transcript history" v="off — not persisted" tone="ok" />
        <DataRow label="Audio retention" v="0 ms after transcription" tone="ok" />
      </div>
      <window.Advisory tone="info">
        AutoWhisper is built with a compile-time flag <Mono>AW_NO_NETWORK</Mono> that removes all socket APIs from the binary except the model-download CLI subcommand. You can verify with <Mono>strings autowhisper | grep socket</Mono>.
      </window.Advisory>
    </div>
  );
}
function DataRow({ label, v, tone }) {
  return (
    <div style={{ display: 'flex', alignItems: 'baseline', gap: 12, padding: '7px 0', borderBottom: '1px dashed var(--aw-line-2)', fontSize: 13 }}>
      <span style={{ color: 'var(--aw-ink-2)', flex: 1 }}>{label}</span>
      <span style={{ fontFamily: 'var(--aw-font-mono)', fontSize: 12, color: tone === 'ok' ? 'var(--aw-ok)' : 'var(--aw-ink-0)' }}>{v}</span>
    </div>
  );
}

function FeedbackSection({ config, set }) {
  return (
    <div>
      <SectionHeader title="Feedback & tray" intent="What I see and hear when AutoWhisper is working." />
      <window.Field label="Recording HUD" hint="On-screen indicator while you dictate. Compact stays small; standard shows waveform; off keeps tray-only." configKey="ui.recording_hud">
        <window.Segmented value={config['ui.recording_hud']} onChange={(v) => set('ui.recording_hud', v)}
          options={[{ v: 'standard', l: 'Standard' }, { v: 'compact', l: 'Compact' }, { v: 'off', l: 'Off' }]} />
      </window.Field>
      <div style={{ height: 16 }} />
      <window.ToggleRow label="Sound when recording starts" configKey="ui.sound_start" value={config['ui.sound_start']} onChange={(v) => set('ui.sound_start', v)} />
      <div style={{ height: 8 }} />
      <window.ToggleRow label="Notify on error" hint="Desktop notification when transcription fails. Errors are always logged regardless." configKey="ui.notify_on_error" value={config['ui.notify_on_error']} onChange={(v) => set('ui.notify_on_error', v)} />
    </div>
  );
}

function DiagSection() {
  return (
    <div>
      <SectionHeader title="Diagnostics" intent="When something is wrong, where do I look first?" />
      <div style={{ display: 'flex', gap: 8, marginBottom: 16 }}>
        <window.AwBtn variant="primary">Run doctor</window.AwBtn>
        <window.AwBtn>Open log file</window.AwBtn>
        <window.AwBtn>Export diagnostic bundle</window.AwBtn>
      </div>
      <Card>
        <CardHead title="Last doctor run · 2m ago" />
        <div style={{ fontFamily: 'var(--aw-font-mono)', fontSize: 12, lineHeight: 1.7 }}>
          <div><span style={{ color: 'var(--aw-ok)' }}>[ok]</span> binary &amp; service</div>
          <div><span style={{ color: 'var(--aw-ok)' }}>[ok]</span> audio · pulse · 16kHz</div>
          <div><span style={{ color: 'var(--aw-ok)' }}>[ok]</span> model · tiny.en loaded</div>
          <div><span style={{ color: 'var(--aw-warn)' }}>[..]</span> model · base.en not downloaded</div>
          <div><span style={{ color: 'var(--aw-ok)' }}>[ok]</span> output · xtest available</div>
          <div><span style={{ color: 'var(--aw-ok)' }}>[ok]</span> network · 0 outbound</div>
        </div>
      </Card>
    </div>
  );
}

function AdvancedSection({ config, dirty }) {
  const dirtyKeys = Object.keys(dirty);
  return (
    <div>
      <SectionHeader title="Advanced · raw config" intent="Edit config.toml directly with a live diff back to the form." />
      {dirtyKeys.length > 0 && (
        <div style={{ marginBottom: 16, padding: 12, background: 'var(--aw-paper-2)', border: '1px solid var(--aw-line-2)', borderRadius: 4, fontFamily: 'var(--aw-font-mono)', fontSize: 12 }}>
          <div className="aw-eyebrow" style={{ marginBottom: 6 }}>Pending diff</div>
          {dirtyKeys.map((k) => (
            <div key={k}>
              <span style={{ color: 'var(--aw-signal)' }}>~ </span>
              <span style={{ color: 'var(--aw-ink-2)' }}>{k}</span>
              <span style={{ color: 'var(--aw-ink-3)' }}> = </span>
              <span style={{ color: 'var(--aw-ink-0)' }}>{JSON.stringify(dirty[k])}</span>
            </div>
          ))}
        </div>
      )}
      <pre style={{ background: '#1c1a15', color: '#d9d6c8', padding: 16, borderRadius: 4, fontSize: 12, fontFamily: 'var(--aw-font-mono)', lineHeight: 1.6, overflow: 'auto', margin: 0 }}>
{`# ~/.config/autowhisper/config.toml — view-only preview

[dictation]
hotkey         = "${config['dictation.hotkey']}"
trigger_mode   = "${config['dictation.trigger_mode']}"
min_duration_ms = ${config['dictation.min_duration_ms']}

[model]
name      = "${config['model.name']}"
device    = "${config['model.device']}"
threads   = ${config['model.threads']}
beam_size = ${config['model.beam_size']}

[audio]
device      = "${config['audio.device']}"
sample_rate = ${config['audio.sample_rate']}
gain_db     = ${config['audio.gain_db']}
vad_enabled = ${config['audio.vad_enabled']}

[output]
method        = "${config['output.method']}"
fallback      = "${config['output.fallback']}"
append_space  = ${config['output.append_space']}
smart_caps    = ${config['output.smart_caps']}
`}
      </pre>
    </div>
  );
}

function NumberInput({ value, unit, onChange }) {
  return (
    <div style={{ height: 32, padding: '0 10px', display: 'inline-flex', alignItems: 'center', gap: 6, background: 'var(--aw-paper-1)', border: '1px solid var(--aw-line)', borderRadius: 4, width: '100%' }}>
      <input type="number" value={value} onChange={(e) => onChange(Number(e.target.value))} style={{ width: '100%', border: 'none', background: 'none', fontFamily: 'var(--aw-font-mono)', fontSize: 13, color: 'var(--aw-ink-0)', outline: 'none' }} />
      {unit && <span style={{ fontFamily: 'var(--aw-font-mono)', fontSize: 11, color: 'var(--aw-ink-3)' }}>{unit}</span>}
    </div>
  );
}
function Slider({ value, min, max, step, unit, onChange }) {
  return (
    <div style={{ display: 'flex', alignItems: 'center', gap: 12 }}>
      <input type="range" min={min} max={max} step={step} value={value} onChange={(e) => onChange(Number(e.target.value))} style={{ flex: 1, accentColor: 'var(--aw-signal)' }} />
      <span style={{ fontFamily: 'var(--aw-font-mono)', fontSize: 12, color: 'var(--aw-ink-0)', width: 56, textAlign: 'right', fontVariantNumeric: 'tabular-nums' }}>{value > 0 ? '+' : ''}{value} {unit}</span>
    </div>
  );
}

window.SettingsPrototype = SettingsPrototype;
