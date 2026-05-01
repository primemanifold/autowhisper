// AutoWhisper — Settings prototype
// Hi-fi clickable settings app. Embedded in the design canvas as one artboard.
// Mirrors the redesigned IA from ia-map.jsx.

const { useState: useSS, useEffect: useSE } = React;

const NAV = [
  { id: 'dictation', n: '01', name: 'Dictation behavior' },
  { id: 'model',     n: '02', name: 'Model & performance' },
  { id: 'audio',     n: '03', name: 'Audio input' },
  { id: 'output',    n: '04', name: 'Output & insertion' },
  { id: 'privacy',   n: '05', name: 'Privacy' },
  { id: 'feedback',  n: '06', name: 'Feedback & tray' },
  { id: 'diag',      n: '07', name: 'Diagnostics' },
  { id: 'advanced',  n: '08', name: 'Advanced' },
];

function SettingsAppArtboard() {
  const [section, setSection] = useSS('model');
  const [hotkey, setHotkey] = useSS('Right-Ctrl');
  const [mode, setMode] = useSS('hold');
  const [model, setModel] = useSS('tiny.en');
  const [device, setDevice] = useSS('cpu');
  const [threads, setThreads] = useSS(8);
  const [insert, setInsert] = useSS(true);
  const [hud, setHud] = useSS('compact');
  const [dirty, setDirty] = useSS(false);
  const mark = (fn) => (v) => { fn(v); setDirty(true); };

  return (
    <div style={{
      height: '100%', display: 'grid', gridTemplateColumns: '232px 1fr',
      background: 'var(--aw-paper-0)', fontFamily: 'var(--aw-font-sans)',
    }}>
      {/* sidebar */}
      <aside style={{
        background: 'var(--aw-paper-2)', borderRight: '1px solid var(--aw-line)',
        display: 'flex', flexDirection: 'column',
      }}>
        <div style={{ padding: '18px 20px', borderBottom: '1px solid var(--aw-line)', display: 'flex', alignItems: 'center', gap: 10 }}>
          <window.AwMark size={18} />
          <span style={{ fontWeight: 600, fontSize: 13, letterSpacing: -0.1 }}>AutoWhisper</span>
          <span style={{ flex: 1 }} />
          <span className="aw-mono" style={{ fontSize: 10, color: 'var(--aw-ink-3)' }}>v0.4.2</span>
        </div>
        <div style={{ padding: '12px 8px', flex: 1, overflow: 'auto' }}>
          {NAV.map((n) => (
            <button key={n.id} onClick={() => setSection(n.id)} style={{
              display: 'flex', alignItems: 'baseline', gap: 10, width: '100%',
              padding: '7px 12px', border: 'none', borderRadius: 4,
              background: section === n.id ? 'var(--aw-paper-1)' : 'transparent',
              color: section === n.id ? 'var(--aw-ink-0)' : 'var(--aw-ink-1)',
              boxShadow: section === n.id ? 'var(--aw-shadow-1)' : 'none',
              cursor: 'pointer', textAlign: 'left',
              fontFamily: 'inherit', fontSize: 13,
              borderLeft: section === n.id ? '2px solid var(--aw-signal)' : '2px solid transparent',
              marginBottom: 1,
            }}>
              <span className="aw-mono" style={{ fontSize: 10, color: 'var(--aw-ink-3)', minWidth: 14 }}>{n.n}</span>
              <span style={{ fontWeight: section === n.id ? 600 : 400 }}>{n.name}</span>
            </button>
          ))}
        </div>
        {/* status footer */}
        <div style={{ borderTop: '1px solid var(--aw-line)', padding: '12px 16px', display: 'flex', flexDirection: 'column', gap: 6 }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
            <span style={{ width: 6, height: 6, borderRadius: 999, background: 'var(--aw-ok)' }} />
            <span className="aw-mono" style={{ fontSize: 10, color: 'var(--aw-ink-2)' }}>daemon · 2h 14m</span>
          </div>
          <div className="aw-mono" style={{ fontSize: 10, color: 'var(--aw-ink-3)', lineHeight: 1.5 }}>
            tiny.en · cpu · 142ms<br />pulse · 16kHz
          </div>
        </div>
      </aside>

      {/* main */}
      <main style={{ display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
        {/* header bar */}
        <div style={{
          padding: '14px 28px', borderBottom: '1px solid var(--aw-line-2)',
          display: 'flex', alignItems: 'center', gap: 16,
          background: 'var(--aw-paper-0)',
        }}>
          <span className="aw-eyebrow">{NAV.find((n) => n.id === section)?.n} · Settings</span>
          <span style={{ flex: 1 }} />
          {dirty && (
            <>
              <window.Pill tone="warn" dot>unsaved · 2 keys</window.Pill>
              <window.AwBtn variant="ghost" size="sm" onClick={() => setDirty(false)}>Discard</window.AwBtn>
              <window.AwBtn variant="primary" size="sm" onClick={() => setDirty(false)}>Save changes</window.AwBtn>
            </>
          )}
          {!dirty && <window.Pill tone="ok" dot>saved</window.Pill>}
        </div>

        <div style={{ flex: 1, overflow: 'auto', padding: '32px 36px 80px' }}>
          {section === 'dictation' && <DictationPane hotkey={hotkey} setHotkey={mark(setHotkey)} mode={mode} setMode={mark(setMode)} />}
          {section === 'model' && <ModelPane model={model} setModel={mark(setModel)} device={device} setDevice={mark(setDevice)} threads={threads} setThreads={mark(setThreads)} />}
          {section === 'audio' && <AudioPane />}
          {section === 'output' && <OutputPane insert={insert} setInsert={mark(setInsert)} />}
          {section === 'privacy' && <PrivacyPane />}
          {section === 'feedback' && <FeedbackPane hud={hud} setHud={mark(setHud)} />}
          {section === 'diag' && <DiagPane />}
          {section === 'advanced' && <AdvancedPane />}
        </div>
      </main>
    </div>
  );
}

function PaneHeader({ title, lede }) {
  return (
    <div style={{ marginBottom: 24, maxWidth: 640 }}>
      <h1 style={{ fontSize: 26, fontWeight: 600, letterSpacing: -0.4, margin: 0, marginBottom: 6 }}>{title}</h1>
      <p style={{ fontSize: 14, color: 'var(--aw-ink-2)', margin: 0, lineHeight: 1.55, textWrap: 'pretty' }}>{lede}</p>
    </div>
  );
}
function PaneSection({ title, children }) {
  return (
    <section style={{ marginBottom: 32 }}>
      <h2 style={{ fontSize: 13, fontWeight: 600, letterSpacing: 0.04, textTransform: 'uppercase', color: 'var(--aw-ink-2)', margin: 0, marginBottom: 14, fontFamily: 'var(--aw-font-mono)', fontSize: 11 }}>{title}</h2>
      <div style={{ display: 'flex', flexDirection: 'column', gap: 14 }}>{children}</div>
    </section>
  );
}

function DictationPane({ hotkey, setHotkey, mode, setMode }) {
  return (
    <>
      <PaneHeader title="Dictation behavior" lede="How you trigger dictation and what AutoWhisper does around the edges of your speech." />
      <PaneSection title="Trigger">
        <window.Field label="Hotkey" hint="Hold to dictate; release to transcribe. Click to rebind." configKey="dictation.hotkey">
          <window.KbdInput value={hotkey} onChange={setHotkey} />
        </window.Field>
        <window.Field label="Trigger mode" hint="Hold-to-talk avoids accidental capture. Toggle is hands-free." configKey="dictation.trigger_mode">
          <window.Segmented value={mode} onChange={setMode} options={[{ v: 'hold', l: 'Hold' }, { v: 'toggle', l: 'Toggle' }, { v: 'tap', l: 'Tap (auto-stop on silence)' }]} />
        </window.Field>
      </PaneSection>
      <PaneSection title="Edges">
        <window.ToggleRow label="Ignore presses shorter than 250ms" hint="Filters accidental key bumps." configKey="dictation.min_duration_ms" value={true} onChange={() => {}} />
        <window.ToggleRow label="Cancel with Esc while recording" configKey="dictation.cancel_key" value={true} onChange={() => {}} />
      </PaneSection>
    </>
  );
}

function ModelPane({ model, setModel, device, setDevice, threads, setThreads }) {
  const models = [
    { name: 'tiny.en',  size: '74 MB',  lat: '~140ms', desc: 'Fast, English-only. Recommended.' },
    { name: 'base.en',  size: '142 MB', lat: '~210ms', desc: 'Better accuracy, still CPU-friendly.' },
    { name: 'small.en', size: '466 MB', lat: '~480ms', desc: 'Higher accuracy. Benefits from a GPU.' },
    { name: 'medium.en',size: '1.5 GB', lat: '~1.2s',  desc: 'Best English accuracy. Needs a GPU.' },
  ];
  return (
    <>
      <PaneHeader title="Model & performance" lede="Which Whisper model runs, on what hardware, and how fast." />

      {/* live readout */}
      <div style={{ display: 'flex', gap: 24, padding: '20px 24px', background: 'var(--aw-paper-1)', border: '1px solid var(--aw-line-2)', borderRadius: 6, marginBottom: 28 }}>
        <Readout label="latency p50" value="142" unit="ms" />
        <Divider />
        <Readout label="real-time factor" value="0.18" unit="×" />
        <Divider />
        <Readout label="last transcript" value="2.3" unit="s · 47w" small />
        <Divider />
        <Readout label="memory" value="312" unit="MB" small />
      </div>

      <PaneSection title="Model">
        <div style={{ display: 'grid', gap: 6 }}>
          {models.map((m) => (
            <button key={m.name} onClick={() => setModel(m.name)} style={{
              padding: '12px 14px', border: `1px solid ${model === m.name ? 'var(--aw-ink-0)' : 'var(--aw-line-2)'}`,
              borderRadius: 4, background: model === m.name ? 'var(--aw-paper-1)' : 'transparent',
              boxShadow: model === m.name ? 'var(--aw-shadow-1)' : 'none', cursor: 'pointer',
              textAlign: 'left', fontFamily: 'inherit',
            }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: 10, marginBottom: 4 }}>
                <span style={{ width: 12, height: 12, borderRadius: '50%', border: `1.5px solid ${model === m.name ? 'var(--aw-signal)' : 'var(--aw-line)'}`, background: model === m.name ? 'var(--aw-signal)' : 'transparent', boxShadow: model === m.name ? 'inset 0 0 0 2px var(--aw-paper-1)' : 'none' }} />
                <span style={{ fontFamily: 'var(--aw-font-mono)', fontWeight: 600, fontSize: 14 }}>{m.name}</span>
                <span className="aw-mono" style={{ fontSize: 11, color: 'var(--aw-ink-2)' }}>{m.size}</span>
                <span style={{ flex: 1 }} />
                <span className="aw-mono" style={{ fontSize: 11, color: 'var(--aw-ink-3)' }}>{m.lat}</span>
              </div>
              <div style={{ fontSize: 12, color: 'var(--aw-ink-2)', paddingLeft: 22 }}>{m.desc}</div>
            </button>
          ))}
        </div>
      </PaneSection>

      <PaneSection title="Compute">
        <window.Field label="Device" hint="CPU is reliable; CUDA/ROCm need a matching driver." configKey="model.device">
          <window.Segmented value={device} onChange={setDevice} options={[{ v: 'cpu', l: 'CPU' }, { v: 'cuda', l: 'CUDA' }, { v: 'rocm', l: 'ROCm' }]} />
        </window.Field>
        <window.Field label={`Threads · ${threads}`} hint="More threads reduce latency until your CPU saturates. Try 8 on a modern laptop." configKey="model.threads">
          <input type="range" min={1} max={16} value={threads} onChange={(e) => setThreads(+e.target.value)} style={{ width: '100%' }} />
        </window.Field>
      </PaneSection>
    </>
  );
}

function Readout({ label, value, unit, small }) {
  return (
    <div>
      <div className="aw-eyebrow" style={{ fontSize: 9, marginBottom: 4 }}>{label}</div>
      <div style={{ display: 'flex', alignItems: 'baseline', gap: 4 }}>
        <span className="aw-mono" style={{ fontSize: small ? 22 : 32, fontWeight: 500, color: 'var(--aw-ink-0)', letterSpacing: -0.4 }}>{value}</span>
        <span className="aw-mono" style={{ fontSize: 12, color: 'var(--aw-ink-3)' }}>{unit}</span>
      </div>
    </div>
  );
}
function Divider() { return <div style={{ width: 1, background: 'var(--aw-line-2)' }} />; }

function AudioPane() {
  return (
    <>
      <PaneHeader title="Audio input" lede="Which microphone, what sample rate, and what to do about background noise." />
      <PaneSection title="Device">
        <div style={{ padding: '12px 14px', background: 'var(--aw-paper-1)', border: '1px solid var(--aw-line-2)', borderRadius: 4, display: 'flex', alignItems: 'center', gap: 14 }}>
          <window.Waveform amplitude={0.55} />
          <span className="aw-mono" style={{ fontSize: 11, color: 'var(--aw-ink-2)', whiteSpace: 'nowrap' }}>live · pulse · 16kHz · 1ch</span>
        </div>
        <window.Field label="Input device" configKey="audio.device">
          <window.Select value="pulse · default" options={['pulse · default', 'pulse · USB Headset (HyperX)', 'alsa:hw:0,0', 'pipewire']} />
        </window.Field>
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 16 }}>
          <window.Field label="Sample rate" configKey="audio.sample_rate"><window.Select value="16000" options={['16000', '24000', '44100', '48000']} /></window.Field>
          <window.Field label="Gain" configKey="audio.gain_db"><window.Select value="0 dB" options={['-6 dB', '-3 dB', '0 dB', '+3 dB', '+6 dB']} /></window.Field>
        </div>
      </PaneSection>
      <PaneSection title="Voice activity">
        <window.ToggleRow label="Voice activity detection (VAD)" hint="Trims silence at the start and end of each capture." configKey="audio.vad_enabled" value={true} onChange={() => {}} />
        <window.ToggleRow label="Noise floor calibration on launch" hint="2-second sample of room noise; subtracts before transcription." configKey="audio.noise_floor_db" value={false} onChange={() => {}} />
      </PaneSection>
    </>
  );
}

function OutputPane({ insert, setInsert }) {
  return (
    <>
      <PaneHeader title="Output & insertion" lede="How transcribed text reaches your cursor." />
      <PaneSection title="Insertion method">
        <window.ToggleRow label="Insert text at cursor" hint="Uses XTest synthetic key events. Falls back to clipboard if blocked." configKey="output.insert" value={insert} onChange={setInsert} />
        <window.Field label="Fallback" hint="What to do when XTest is blocked (Wayland, sandbox, focus-locked window)." configKey="output.fallback">
          <window.Segmented value="clipboard" onChange={() => {}} options={[{ v: 'clipboard', l: 'Clipboard' }, { v: 'notify', l: 'Notify' }, { v: 'discard', l: 'Discard' }]} />
        </window.Field>
      </PaneSection>
      <PaneSection title="Text processing">
        <window.ToggleRow label="Append a space after each insertion" configKey="output.append_space" value={true} onChange={() => {}} />
        <window.ToggleRow label="Smart capitalization at sentence starts" configKey="output.smart_caps" value={true} onChange={() => {}} />
      </PaneSection>
    </>
  );
}

function PrivacyPane() {
  return (
    <>
      <PaneHeader title="Privacy" lede="Proof — not promise — that nothing leaves this machine." />
      {/* zero-toggles section: live network readout */}
      <div style={{ padding: '20px 24px', background: 'var(--aw-paper-1)', border: '1px solid var(--aw-line-2)', borderRadius: 6, marginBottom: 24 }}>
        <div className="aw-eyebrow" style={{ marginBottom: 10 }}>Outbound network · live</div>
        <div style={{ display: 'flex', alignItems: 'baseline', gap: 12, marginBottom: 14 }}>
          <span className="aw-mono" style={{ fontSize: 44, fontWeight: 500, color: 'var(--aw-ok)', letterSpacing: -1 }}>0</span>
          <span className="aw-mono" style={{ fontSize: 13, color: 'var(--aw-ink-2)' }}>active sockets</span>
          <span style={{ flex: 1 }} />
          <window.Pill tone="ok" dot>local-only</window.Pill>
        </div>
        <div style={{ borderTop: '1px solid var(--aw-line-2)', paddingTop: 12 }}>
          <table style={{ width: '100%', fontFamily: 'var(--aw-font-mono)', fontSize: 11.5, color: 'var(--aw-ink-1)' }}>
            <thead style={{ color: 'var(--aw-ink-3)', textAlign: 'left' }}>
              <tr><th style={{ fontWeight: 400, padding: '4px 8px 4px 0' }}>process</th><th style={{ fontWeight: 400, padding: '4px 8px' }}>last call</th><th style={{ fontWeight: 400, padding: '4px 8px' }}>destination</th></tr>
            </thead>
            <tbody>
              <tr><td style={{ padding: '4px 8px 4px 0' }}>autowhisperd</td><td style={{ padding: '4px 8px', color: 'var(--aw-ink-3)' }}>never (since boot)</td><td style={{ padding: '4px 8px', color: 'var(--aw-ink-3)' }}>—</td></tr>
              <tr><td style={{ padding: '4px 8px 4px 0' }}>autowhisper-ui</td><td style={{ padding: '4px 8px', color: 'var(--aw-ink-3)' }}>14d 3h ago</td><td style={{ padding: '4px 8px' }}>huggingface.co (model dl)</td></tr>
            </tbody>
          </table>
        </div>
      </div>

      <PaneSection title="On disk">
        <KVRow k="audio in RAM" v="duration of recording, then freed" />
        <KVRow k="transcripts" v="not stored by default" mono />
        <KVRow k="model file" v="~/.local/share/autowhisper/models/tiny.en.bin" mono />
        <KVRow k="logs" v="~/.cache/autowhisper/log (rotated, 7 days)" mono />
        <KVRow k="config" v="~/.config/autowhisper/config.toml" mono />
      </PaneSection>

      <PaneSection title="Optional retention">
        <window.ToggleRow label="Keep last 50 transcripts (for search)" hint="Stored at ~/.local/share/autowhisper/history.jsonl. Off by default." configKey="storage.transcript_history" value={false} onChange={() => {}} />
      </PaneSection>
    </>
  );
}
function KVRow({ k, v, mono }) {
  return (
    <div style={{ display: 'flex', alignItems: 'baseline', padding: '8px 0', borderBottom: '1px solid var(--aw-line-2)' }}>
      <span style={{ width: 180, fontSize: 13, color: 'var(--aw-ink-2)' }}>{k}</span>
      <span style={{ flex: 1, fontSize: 13, color: 'var(--aw-ink-0)', fontFamily: mono ? 'var(--aw-font-mono)' : 'var(--aw-font-sans)' }}>{v}</span>
    </div>
  );
}

function FeedbackPane({ hud, setHud }) {
  return (
    <>
      <PaneHeader title="Feedback & tray" lede="What you see and hear when AutoWhisper is working." />
      <PaneSection title="On-screen indicator">
        <window.Field label="Recording HUD" hint="A small floating pill that appears while you dictate. Compact is a 28px capsule; full shows a waveform." configKey="ui.recording_hud">
          <window.Segmented value={hud} onChange={setHud} options={[{ v: 'off', l: 'Off' }, { v: 'compact', l: 'Compact' }, { v: 'full', l: 'Full' }]} />
        </window.Field>
      </PaneSection>
      <PaneSection title="Sounds">
        <window.ToggleRow label="Soft tick on record start" configKey="ui.sound_start" value={true} onChange={() => {}} />
        <window.ToggleRow label="Soft tock on record end" configKey="ui.sound_end" value={true} onChange={() => {}} />
        <window.ToggleRow label="Notify on error" configKey="ui.notify_on_error" value={true} onChange={() => {}} />
      </PaneSection>
    </>
  );
}

function DiagPane() {
  return (
    <>
      <PaneHeader title="Diagnostics" lede="When something is wrong, find the fix here — or run autowhisper doctor in a terminal." />
      <window.Advisory tone="ok" style={{ marginBottom: 20 }}>
        <b>All systems nominal.</b> Last <window.Mono>doctor</window.Mono> run completed 14m ago · 11 ok · 0 errors.
      </window.Advisory>
      <PaneSection title="Quick actions">
        <div style={{ display: 'flex', gap: 8 }}>
          <window.AwBtn>Run doctor</window.AwBtn>
          <window.AwBtn>Open log file</window.AwBtn>
          <window.AwBtn>Export diagnostics bundle</window.AwBtn>
          <window.AwBtn variant="ghost">Reload daemon</window.AwBtn>
        </div>
      </PaneSection>
      <PaneSection title="Recent">
        <div style={{ fontFamily: 'var(--aw-font-mono)', fontSize: 12, lineHeight: 1.7, padding: '12px 14px', background: 'var(--aw-paper-2)', border: '1px solid var(--aw-line-2)', borderRadius: 4, color: 'var(--aw-ink-1)' }}>
          <div><span style={{ color: 'var(--aw-ok)' }}>[ok]</span>  04-30 14:22:06  transcribe finished · 142ms · 47w</div>
          <div><span style={{ color: 'var(--aw-ok)' }}>[ok]</span>  04-30 14:21:48  capture started · pulse · 16kHz</div>
          <div><span style={{ color: 'var(--aw-warn)' }}>[..]</span>  04-30 13:08:12  XTest blocked · fell back to clipboard</div>
          <div><span style={{ color: 'var(--aw-ok)' }}>[ok]</span>  04-30 11:47:01  daemon started · v0.4.2</div>
        </div>
      </PaneSection>
    </>
  );
}

function AdvancedPane() {
  const toml = `[dictation]
hotkey = "Right-Ctrl"
trigger_mode = "hold"

[model]
name = "tiny.en"
device = "cpu"
threads = 8
beam_size = 1

[audio]
device = "pulse"
sample_rate = 16000
vad_enabled = true

[output]
method = "xtest"
fallback = "clipboard"
append_space = true`;
  return (
    <>
      <PaneHeader title="Advanced" lede="Edit config.toml directly. Changes here update the form above; the form updates this." />
      <window.Advisory tone="info" style={{ marginBottom: 16 }}>
        Reads from <window.Mono>~/.config/autowhisper/config.toml</window.Mono>. The daemon hot-reloads on save.
      </window.Advisory>
      <pre style={{ margin: 0, padding: 20, background: '#1c1a15', color: '#d9d6c8', borderRadius: 6, fontFamily: 'var(--aw-font-mono)', fontSize: 12.5, lineHeight: 1.65, overflow: 'auto', border: '1px solid #2c2a23' }}>
{toml}
      </pre>
    </>
  );
}

window.SettingsAppArtboard = SettingsAppArtboard;
