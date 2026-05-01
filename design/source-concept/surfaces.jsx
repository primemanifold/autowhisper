// AutoWhisper — Surface designs
// Tray menu + state icons, Recording HUD, CLI, First-run, Diagnostics

const { useState: useS, useEffect: useE } = React;

// ─────────────────────────────────────────────────────────────
// TRAY: states + dropdown menu
// ─────────────────────────────────────────────────────────────
function TrayArtboard() {
  return (
    <div style={{ padding: 32, background: 'var(--aw-paper-2)', height: '100%', display: 'flex', flexDirection: 'column', gap: 20 }}>
      <div className="aw-eyebrow">09a · Tray states & menu</div>

      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: 12 }}>
        <TrayState name="idle" icon={<TrayIcon kind="idle" />} caption="paper-1 mark" />
        <TrayState name="recording" icon={<TrayIcon kind="recording" />} caption="signal dot pulses" />
        <TrayState name="processing" icon={<TrayIcon kind="processing" />} caption="rotating tick" />
        <TrayState name="error" icon={<TrayIcon kind="error" />} caption="err underline" />
      </div>

      {/* The dropdown */}
      <div style={{ marginTop: 8, display: 'flex', justifyContent: 'center' }}>
        <div style={{
          width: 280, background: 'var(--aw-paper-1)', border: '1px solid var(--aw-line)',
          borderRadius: 8, boxShadow: 'var(--aw-shadow-3)', padding: 6, fontSize: 13,
        }}>
          <div style={{ padding: '10px 12px 8px', borderBottom: '1px solid var(--aw-line-2)', marginBottom: 4 }}>
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
              <span style={{ display: 'inline-flex', alignItems: 'center', gap: 8 }}>
                <span style={{ width: 7, height: 7, borderRadius: 999, background: 'var(--aw-ok)' }} />
                <b style={{ fontSize: 13 }}>AutoWhisper</b>
              </span>
              <span className="aw-mono" style={{ fontSize: 10, color: 'var(--aw-ink-3)' }}>v0.4.2</span>
            </div>
            <div className="aw-mono" style={{ fontSize: 11, color: 'var(--aw-ink-2)', marginTop: 6, lineHeight: 1.5 }}>
              tiny.en · 16kHz · pulse<br />
              <span style={{ color: 'var(--aw-ink-3)' }}>last:</span> 142ms · 2.3s audio · 47w
            </div>
          </div>
          <MenuItem icon="●" label="Start dictation" hint="Right-Ctrl" hot />
          <MenuItem icon="□" label="Open Settings" hint="⌃," />
          <MenuItem icon="⌥" label="Run doctor" />
          <MenuItem icon="↗" label="Open log" hint="~/.cache/autowhisper/log" />
          <div style={{ height: 1, background: 'var(--aw-line-2)', margin: '4px 0' }} />
          <MenuItem icon="⏻" label="Quit AutoWhisper" />
        </div>
      </div>
    </div>
  );
}
function TrayState({ name, icon, caption }) {
  return (
    <div style={{ background: '#2a2620', borderRadius: 6, padding: '14px 12px', display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 6 }}>
      <div style={{ width: 22, height: 22, color: '#f0eee5', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>{icon}</div>
      <div className="aw-mono" style={{ fontSize: 10, color: '#a8a392', textTransform: 'uppercase', letterSpacing: 0.08 }}>{name}</div>
      <div style={{ fontSize: 10, color: '#7a7464', textAlign: 'center' }}>{caption}</div>
    </div>
  );
}
function TrayIcon({ kind }) {
  if (kind === 'idle')
    return (
      <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
        <path d="M5 4 L3 4 L3 20 L5 20 M19 4 L21 4 L21 20 L19 20" stroke="currentColor" strokeWidth="1.6" />
        <circle cx="12" cy="12" r="3" fill="currentColor" />
      </svg>
    );
  if (kind === 'recording')
    return (
      <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
        <path d="M5 4 L3 4 L3 20 L5 20 M19 4 L21 4 L21 20 L19 20" stroke="currentColor" strokeWidth="1.6" />
        <circle cx="12" cy="12" r="3.5" fill="#d96148">
          <animate attributeName="r" values="3.5;5;3.5" dur="1.4s" repeatCount="indefinite" />
        </circle>
      </svg>
    );
  if (kind === 'processing')
    return (
      <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
        <path d="M5 4 L3 4 L3 20 L5 20 M19 4 L21 4 L21 20 L19 20" stroke="currentColor" strokeWidth="1.6" />
        <g style={{ transformOrigin: '12px 12px', animation: 'awspin 1.2s steps(8) infinite' }}>
          <line x1="12" y1="8" x2="12" y2="10.5" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" />
          <line x1="14.8" y1="9.2" x2="13.6" y2="11" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" opacity="0.7" />
          <line x1="16" y1="12" x2="13.5" y2="12" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" opacity="0.5" />
          <line x1="14.8" y1="14.8" x2="13.6" y2="13" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" opacity="0.35" />
        </g>
      </svg>
    );
  if (kind === 'error')
    return (
      <svg width="20" height="20" viewBox="0 0 24 24" fill="none">
        <path d="M5 4 L3 4 L3 20 L5 20 M19 4 L21 4 L21 20 L19 20" stroke="currentColor" strokeWidth="1.6" />
        <line x1="6" y1="18" x2="18" y2="18" stroke="#d56862" strokeWidth="2" />
        <text x="12" y="14" textAnchor="middle" fontSize="11" fontFamily="monospace" fill="currentColor">!</text>
      </svg>
    );
}
function MenuItem({ icon, label, hint, hot }) {
  return (
    <div style={{
      display: 'flex', alignItems: 'center', gap: 10, padding: '7px 10px', borderRadius: 4,
      cursor: 'pointer', background: hot ? 'var(--aw-paper-2)' : 'transparent',
    }}>
      <span style={{ width: 16, color: hot ? 'var(--aw-signal)' : 'var(--aw-ink-3)', fontFamily: 'var(--aw-font-mono)', fontSize: 12, textAlign: 'center' }}>{icon}</span>
      <span style={{ flex: 1, color: 'var(--aw-ink-0)' }}>{label}</span>
      {hint && <span className="aw-mono" style={{ fontSize: 10, color: 'var(--aw-ink-3)' }}>{hint}</span>}
    </div>
  );
}

// ─────────────────────────────────────────────────────────────
// RECORDING HUD — minimal floating pill, opt-in.
// ─────────────────────────────────────────────────────────────
function HudArtboard() {
  return (
    <div style={{ padding: 32, background: 'linear-gradient(180deg, #2a2620 0%, #16140f 100%)', height: '100%', display: 'flex', flexDirection: 'column', gap: 20 }}>
      <div className="aw-eyebrow" style={{ color: '#a8a392' }}>09b · Recording HUD (opt-in, dismissible)</div>

      <div style={{ display: 'flex', flexDirection: 'column', gap: 14 }}>
        <HudState label="State 1 · Recording" sub="press the hotkey, the pill appears">
          <Hud state="recording" />
        </HudState>
        <HudState label="State 2 · Transcribing" sub="release, audio routes to the model">
          <Hud state="processing" />
        </HudState>
        <HudState label="State 3 · Inserted" sub="text written, pill fades after 600ms">
          <Hud state="done" />
        </HudState>
        <HudState label="State 4 · Error · cannot insert" sub="x11 injection blocked, falls back to clipboard">
          <Hud state="error" />
        </HudState>
        <HudState label="Variant · Compact" sub="ui.recording_hud = 'compact'">
          <Hud state="recording" compact />
        </HudState>
      </div>

      <div style={{ marginTop: 'auto', fontSize: 11, fontFamily: 'var(--aw-font-mono)', color: '#7a7464', lineHeight: 1.6 }}>
        position: bottom-center · 24px from edge<br />
        keyboard-dismissable with Esc · click-through when fading out<br />
        respects dnd / fullscreen unless ui.recording_hud_force = true
      </div>
    </div>
  );
}
function HudState({ label, sub, children }) {
  return (
    <div style={{ display: 'flex', alignItems: 'center', gap: 24 }}>
      <div style={{ width: 200, flexShrink: 0 }}>
        <div className="aw-mono" style={{ fontSize: 11, color: '#d9d6c8' }}>{label}</div>
        <div style={{ fontSize: 11, color: '#7a7464', marginTop: 2 }}>{sub}</div>
      </div>
      {children}
    </div>
  );
}
function Hud({ state, compact }) {
  const [t, setT] = useS(0);
  useE(() => { const i = setInterval(() => setT((x) => x + 1), 90); return () => clearInterval(i); }, []);

  if (compact)
    return (
      <div style={{ display: 'inline-flex', alignItems: 'center', gap: 8, height: 28, padding: '0 12px', background: 'rgba(20,16,8,0.85)', backdropFilter: 'blur(20px)', borderRadius: 999, border: '1px solid rgba(255,255,255,0.08)' }}>
        <span style={{ width: 6, height: 6, borderRadius: 999, background: '#d96148', animation: 'awpulse 1.4s ease-in-out infinite' }} />
        <span style={{ fontFamily: 'var(--aw-font-mono)', fontSize: 11, color: '#f0eee5', fontVariantNumeric: 'tabular-nums' }}>0:0{t % 9}.{(t * 9) % 10}{(t * 3) % 10}</span>
      </div>
    );

  const colorByState = {
    recording: '#d96148', processing: '#6a9bcb', done: '#6fa868', error: '#d56862',
  };
  const labelByState = {
    recording: 'recording', processing: 'transcribing', done: 'inserted · 47w', error: 'cannot insert · using clipboard',
  };
  const c = colorByState[state];

  return (
    <div style={{
      display: 'inline-flex', alignItems: 'center', gap: 12, padding: '8px 14px',
      background: 'rgba(20,16,8,0.88)', backdropFilter: 'blur(24px)', borderRadius: 8,
      border: '1px solid rgba(255,255,255,0.08)', minWidth: 320,
      boxShadow: '0 8px 32px rgba(0,0,0,0.5), 0 0 0 1px rgba(255,255,255,0.04)',
    }}>
      <span style={{ width: 8, height: 8, borderRadius: 999, background: c, animation: state === 'recording' ? 'awpulse 1.4s ease-in-out infinite' : 'none' }} />
      <div style={{ display: 'flex', alignItems: 'center', gap: 2, height: 18 }}>
        {state !== 'done' && state !== 'error' && Array.from({ length: 22 }).map((_, i) => {
          const phase = (i / 22) * Math.PI * 3 + t * 0.4;
          const env = Math.sin((i / 22) * Math.PI);
          const h = (Math.abs(Math.sin(phase)) * env * (state === 'processing' ? 0.5 : 0.85) + 0.1);
          return <span key={i} style={{ width: 2, height: `${Math.round(h * 8) / 8 * 100}%`, background: i < 11 ? c : '#f0eee5', minHeight: 2 }} />;
        })}
        {(state === 'done' || state === 'error') && <span style={{ fontFamily: 'var(--aw-font-mono)', fontSize: 12, color: c }}>{state === 'done' ? '✓' : '⚠'}</span>}
      </div>
      <span style={{ fontFamily: 'var(--aw-font-mono)', fontSize: 11, color: '#a8a392', textTransform: 'lowercase' }}>{labelByState[state]}</span>
      <span style={{ flex: 1 }} />
      <span style={{ fontFamily: 'var(--aw-font-mono)', fontSize: 11, color: '#f0eee5', fontVariantNumeric: 'tabular-nums' }}>0:0{(t / 11 | 0) % 9}.{(t * 9) % 10}{(t * 3) % 10}</span>
    </div>
  );
}

// ─────────────────────────────────────────────────────────────
// CLI — autowhisper doctor styled output
// ─────────────────────────────────────────────────────────────
function CliArtboard() {
  return (
    <div style={{ padding: 32, background: 'var(--aw-paper-2)', height: '100%' }}>
      <div className="aw-eyebrow" style={{ marginBottom: 16 }}>09c · CLI · doctor & status</div>

      <div style={{
        background: '#1c1a15', borderRadius: 6, padding: '20px 24px', fontFamily: 'var(--aw-font-mono)',
        fontSize: 12.5, lineHeight: 1.7, color: '#d9d6c8', border: '1px solid #2c2a23',
        boxShadow: '0 6px 24px rgba(0,0,0,0.2)',
      }}>
        <Line><CliPrompt /> autowhisper doctor</Line>
        <Line color="#7a7464">─────────────────────────────────────────────────────────────</Line>
        <Line color="#a8a392">[1/6]  binary &amp; service</Line>
        <Line><CliOk /> autowhisper        <Path>v0.4.2</Path>            <Faint>build 2026-04-12</Faint></Line>
        <Line><CliOk /> systemd --user      <Path>active (running)</Path>   <Faint>since 2h 14m</Faint></Line>
        <Line>&nbsp;</Line>
        <Line color="#a8a392">[2/6]  audio</Line>
        <Line><CliOk /> input device       <Path>pulse (default)</Path>     <Faint>16kHz · 1ch</Faint></Line>
        <Line><CliOk /> sample buffer       <Path>1024 frames</Path>        <Faint>~64ms</Faint></Line>
        <Line>&nbsp;</Line>
        <Line color="#a8a392">[3/6]  model</Line>
        <Line><CliOk /> tiny.en             <Path>74 MB</Path>               <Faint>loaded · cpu · 8 threads</Faint></Line>
        <Line><CliWarn /> base.en             <Path>not downloaded</Path>      <Faint>autowhisper model download base.en</Faint></Line>
        <Line>&nbsp;</Line>
        <Line color="#a8a392">[4/6]  output</Line>
        <Line><CliOk /> XTest available     <Path>x11</Path>                 <Faint>direct text injection</Faint></Line>
        <Line><CliOk /> clipboard fallback   <Path>xclip 0.13</Path></Line>
        <Line>&nbsp;</Line>
        <Line color="#a8a392">[5/6]  network · privacy</Line>
        <Line><CliOk /> outbound sockets    <Path>0 active</Path>            <Faint>no remote calls in 14d</Faint></Line>
        <Line><CliOk /> telemetry            <Path>off · compiled-out</Path></Line>
        <Line>&nbsp;</Line>
        <Line color="#a8a392">[6/6]  performance · last 50 transcriptions</Line>
        <Line><CliOk /> p50 latency        <Path>142ms</Path>               <Faint>p95 287ms · max 412ms</Faint></Line>
        <Line><CliOk /> rtf                 <Path>0.18×</Path>               <Faint>real-time factor (lower is faster)</Faint></Line>
        <Line color="#7a7464">─────────────────────────────────────────────────────────────</Line>
        <Line><span style={{ color: '#6fa868' }}>summary:</span> 11 ok · 1 advisory · 0 errors</Line>
        <Line color="#7a7464">run <span style={{ color: '#d96148' }}>autowhisper doctor --fix</span> to address advisories</Line>
        <Line>&nbsp;</Line>
        <Line><CliPrompt /> <span style={{ animation: 'awblink 1s steps(2) infinite' }}>▌</span></Line>
      </div>
    </div>
  );
}
function Line({ children, color }) { return <div style={{ color: color || 'inherit' }}>{children}</div>; }
function CliPrompt() { return <span style={{ color: '#d96148' }}>$</span>; }
function CliOk() { return <span style={{ color: '#6fa868', display: 'inline-block', width: 22 }}>[ok]</span>; }
function CliWarn() { return <span style={{ color: '#d4a247', display: 'inline-block', width: 22 }}>[..]</span>; }
function Path({ children }) { return <span style={{ color: '#f0eee5' }}>{children}</span>; }
function Faint({ children }) { return <span style={{ color: '#7a7464' }}>· {children}</span>; }

// ─────────────────────────────────────────────────────────────
// FIRST-RUN — three-step setup
// ─────────────────────────────────────────────────────────────
function FirstRunArtboard() {
  return (
    <div style={{ padding: 0, background: 'var(--aw-paper-0)', height: '100%', display: 'flex', flexDirection: 'column' }}>
      <div style={{ padding: '20px 32px', borderBottom: '1px solid var(--aw-line)', display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: 10 }}>
          <window.AwMark size={22} />
          <b style={{ fontSize: 14 }}>AutoWhisper</b>
          <span className="aw-eyebrow" style={{ marginLeft: 6 }}>First run · 1 of 3</span>
        </div>
        <div style={{ display: 'flex', gap: 6 }}>
          <Step active />
          <Step />
          <Step />
        </div>
      </div>

      <div style={{ flex: 1, padding: '40px 56px', display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 56, alignItems: 'flex-start' }}>
        <div>
          <div className="aw-eyebrow" style={{ marginBottom: 12 }}>Step 1 · Pick a model</div>
          <h2 style={{ fontSize: 30, fontWeight: 600, letterSpacing: -0.5, margin: 0, marginBottom: 12, fontFamily: 'var(--aw-font-serif)', fontStyle: 'italic' }}>
            One model. On your machine.
          </h2>
          <p style={{ fontSize: 14, color: 'var(--aw-ink-2)', lineHeight: 1.6, maxWidth: 380, marginTop: 0 }}>
            AutoWhisper transcribes locally with whisper.cpp. Smaller models start faster and use less RAM; larger ones are more accurate.
            You can swap models any time from Settings &rsaquo; Model &amp; performance.
          </p>
          <div style={{ marginTop: 24, display: 'grid', gap: 8 }}>
            <ModelCard sel name="tiny.en" size="74 MB" lat="~140ms" desc="Fast, English-only, runs comfortably on a laptop CPU. Recommended to start." />
            <ModelCard name="base.en" size="142 MB" lat="~210ms" desc="Better accuracy, still CPU-friendly." />
            <ModelCard name="small.en" size="466 MB" lat="~480ms" desc="Higher accuracy. Benefits from a GPU." />
          </div>
        </div>
        <div style={{ background: 'var(--aw-paper-1)', border: '1px solid var(--aw-line-2)', borderRadius: 6, padding: 24 }}>
          <div className="aw-eyebrow" style={{ marginBottom: 12 }}>What this means</div>
          <ul style={{ fontSize: 13, lineHeight: 1.7, color: 'var(--aw-ink-1)', margin: 0, paddingLeft: 16 }}>
            <li>The file downloads from <Mono>huggingface.co/ggerganov/whisper.cpp</Mono> over HTTPS.</li>
            <li>It is saved to <Mono>~/.local/share/autowhisper/models/</Mono>.</li>
            <li>After download, AutoWhisper makes <b>no further outbound network calls</b> for this model.</li>
            <li>You can verify by running <Mono>autowhisper doctor</Mono> — it lists every open socket the daemon holds.</li>
          </ul>
        </div>
      </div>
      <div style={{ padding: '16px 32px', borderTop: '1px solid var(--aw-line)', display: 'flex', alignItems: 'center', justifyContent: 'space-between', background: 'var(--aw-paper-1)' }}>
        <span className="aw-mono" style={{ fontSize: 11, color: 'var(--aw-ink-3)' }}>
          <window.Kbd>↵</window.Kbd> continue · <window.Kbd>Esc</window.Kbd> cancel
        </span>
        <div style={{ display: 'flex', gap: 8 }}>
          <window.AwBtn variant="ghost">Skip — I'll set this up later</window.AwBtn>
          <window.AwBtn variant="primary">Download tiny.en →</window.AwBtn>
        </div>
      </div>
    </div>
  );
}
function Step({ active }) {
  return <div style={{ width: 24, height: 3, background: active ? 'var(--aw-signal)' : 'var(--aw-line)' }} />;
}
function ModelCard({ name, size, lat, desc, sel }) {
  return (
    <div style={{
      padding: '12px 14px', border: `1px solid ${sel ? 'var(--aw-ink-0)' : 'var(--aw-line-2)'}`,
      borderRadius: 4, background: sel ? 'var(--aw-paper-1)' : 'transparent',
      boxShadow: sel ? 'var(--aw-shadow-1)' : 'none',
    }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: 10, marginBottom: 4 }}>
        <span style={{ width: 12, height: 12, borderRadius: '50%', border: `1.5px solid ${sel ? 'var(--aw-signal)' : 'var(--aw-line)'}`, background: sel ? 'var(--aw-signal)' : 'transparent', boxShadow: sel ? 'inset 0 0 0 2px var(--aw-paper-1)' : 'none' }} />
        <span style={{ fontFamily: 'var(--aw-font-mono)', fontWeight: 600, fontSize: 14 }}>{name}</span>
        <span className="aw-mono" style={{ fontSize: 11, color: 'var(--aw-ink-2)' }}>{size}</span>
        <span style={{ flex: 1 }} />
        <span className="aw-mono" style={{ fontSize: 11, color: 'var(--aw-ink-3)' }}>{lat}</span>
      </div>
      <div style={{ fontSize: 12, color: 'var(--aw-ink-2)', paddingLeft: 22 }}>{desc}</div>
    </div>
  );
}

// ─────────────────────────────────────────────────────────────
// DIAGNOSTICS — error remediation
// ─────────────────────────────────────────────────────────────
function DiagnosticsArtboard() {
  return (
    <div style={{ padding: '40px 48px', background: 'var(--aw-paper-1)', height: '100%', overflow: 'auto' }}>
      <div className="aw-eyebrow" style={{ marginBottom: 8 }}>09d · Diagnostics &amp; remediation</div>
      <h2 style={{ fontSize: 26, fontWeight: 600, letterSpacing: -0.4, margin: 0, marginBottom: 8 }}>When it breaks, point to the fix.</h2>
      <p style={{ fontSize: 13, color: 'var(--aw-ink-2)', marginTop: 0, marginBottom: 24, maxWidth: 600, lineHeight: 1.55 }}>
        Every error names the failing component, the exact file or syscall, and at least one CLI command + GUI path that resolves it.
      </p>

      <DiagCard
        sev="err" title="Microphone not detected"
        body={<>No PulseAudio source matched <Mono>audio.device = "default"</Mono>. The daemon is running but cannot open an input stream.</>}
        fix={[
          { label: 'List devices', cmd: 'autowhisper doctor --audio' },
          { label: 'Pick a device', cmd: 'Settings → Audio input → Device' },
          { label: 'Reset to default', cmd: 'autowhisper config reset audio.device' },
        ]}
        tech={['pulse · pa_context_connect → CONNECTION_REFUSED', 'fallback alsa: no card found at hw:0,0', 'last successful capture: never']}
      />
      <DiagCard
        sev="warn" title="Text injection blocked — using clipboard"
        body={<>The active window does not accept synthetic key events (Wayland surface, sandboxed app, or focus locked). AutoWhisper copied the transcript to the clipboard as fallback.</>}
        fix={[
          { label: 'Check method', cmd: 'autowhisper doctor --output' },
          { label: 'Force clipboard', cmd: 'output.method = "clipboard"' },
          { label: 'Pause auto-paste', cmd: 'Tray → Pause for 5 min' },
        ]}
        tech={['XTestFakeKeyEvent → BadWindow', 'detected window class: org.gnome.Calculator', 'clipboard set via xclip · 1.2KB']}
      />
      <DiagCard
        sev="info" title="Latency spike on last transcription"
        body={<>p95 latency rose to 487ms over the last 5 transcriptions, up from 142ms baseline. This is usually CPU contention or a long utterance.</>}
        fix={[
          { label: 'See breakdown', cmd: 'autowhisper logs --filter timing' },
          { label: 'Drop to tiny.en', cmd: 'Settings → Model → tiny.en' },
          { label: 'Limit beam width', cmd: 'model.beam_size = 1' },
        ]}
        tech={['load: 1.2 · cpu: 73% (other)', 'utterance: 11.2s · 18 segments', 'whisper.cpp: 487ms (encode 320ms · decode 167ms)']}
      />
    </div>
  );
}
function DiagCard({ sev, title, body, fix, tech }) {
  const sevColor = sev === 'err' ? 'var(--aw-err)' : sev === 'warn' ? 'var(--aw-warn)' : 'var(--aw-info)';
  return (
    <div style={{ border: '1px solid var(--aw-line-2)', borderRadius: 4, marginBottom: 14, overflow: 'hidden' }}>
      <div style={{ padding: '14px 16px', borderLeft: `3px solid ${sevColor}` }}>
        <div style={{ display: 'flex', alignItems: 'baseline', gap: 10, marginBottom: 6 }}>
          <span className="aw-mono" style={{ fontSize: 10, color: sevColor, textTransform: 'uppercase', fontWeight: 600 }}>{sev === 'err' ? 'ERROR' : sev === 'warn' ? 'WARN' : 'INFO'}</span>
          <h3 style={{ fontSize: 15, fontWeight: 600, margin: 0 }}>{title}</h3>
        </div>
        <p style={{ fontSize: 13, color: 'var(--aw-ink-1)', margin: 0, marginBottom: 12, lineHeight: 1.55 }}>{body}</p>
        <div className="aw-eyebrow" style={{ marginBottom: 6 }}>Try one of these</div>
        <div style={{ display: 'flex', flexDirection: 'column', gap: 4, marginBottom: 12 }}>
          {fix.map((f, i) => (
            <div key={i} style={{ display: 'flex', alignItems: 'center', gap: 10, fontSize: 12 }}>
              <span style={{ width: 90, color: 'var(--aw-ink-2)', flexShrink: 0 }}>{f.label}</span>
              <code style={{ fontFamily: 'var(--aw-font-mono)', fontSize: 12, padding: '2px 7px', background: 'var(--aw-paper-2)', border: '1px solid var(--aw-line-2)', borderRadius: 3, color: 'var(--aw-ink-0)' }}>{f.cmd}</code>
            </div>
          ))}
        </div>
        <details style={{ fontSize: 11, color: 'var(--aw-ink-3)' }}>
          <summary style={{ cursor: 'pointer', color: 'var(--aw-ink-2)', fontFamily: 'var(--aw-font-mono)' }}>technical detail</summary>
          <div style={{ marginTop: 6, padding: 10, background: 'var(--aw-paper-2)', borderRadius: 3, fontFamily: 'var(--aw-font-mono)', lineHeight: 1.6 }}>
            {tech.map((t, i) => <div key={i}>{t}</div>)}
          </div>
        </details>
      </div>
    </div>
  );
}

window.TrayArtboard = TrayArtboard;
window.HudArtboard = HudArtboard;
window.CliArtboard = CliArtboard;
window.FirstRunArtboard = FirstRunArtboard;
window.DiagnosticsArtboard = DiagnosticsArtboard;
