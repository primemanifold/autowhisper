// AutoWhisper — IA Map artboard
// Redesigned settings IA: from raw config sections to user intent.

function IAMapArtboard() {
  const groups = [
    {
      id: '01', name: 'Dictation behavior',
      intent: 'How I trigger dictation and what happens around the edges.',
      keys: ['dictation.hotkey', 'dictation.trigger_mode', 'dictation.min_duration_ms', 'dictation.cooldown_ms', 'dictation.cancel_key'],
      legacy: 'Was scattered across [hotkey], [behavior], [debounce]',
      tag: 'Most-changed',
    },
    {
      id: '02', name: 'Model & performance',
      intent: 'Which Whisper model runs, on what hardware, and how fast.',
      keys: ['model.name', 'model.device', 'model.threads', 'model.beam_size', 'model.language', 'performance.target_latency_ms'],
      legacy: 'Was [whisper], [compute] — config-shape-driven, not user-driven',
      tag: 'Has dashboard',
    },
    {
      id: '03', name: 'Audio input',
      intent: 'Which mic, sample rate, and what to do about noise.',
      keys: ['audio.device', 'audio.sample_rate', 'audio.channels', 'audio.gain_db', 'audio.vad_enabled', 'audio.noise_floor_db'],
      legacy: 'Was [audio], [vad]',
      tag: '',
    },
    {
      id: '04', name: 'Output & insertion',
      intent: 'How transcribed text reaches the cursor.',
      keys: ['output.method', 'output.fallback', 'output.append_space', 'output.smart_caps', 'output.replacements'],
      legacy: 'Was [x11], [clipboard], [text_processing]',
      tag: 'New',
    },
    {
      id: '05', name: 'Privacy & local operation',
      intent: 'Proof — not promise — that nothing leaves this machine.',
      keys: ['(read-only) network.outbound', '(read-only) telemetry.enabled', 'storage.audio_retention', 'storage.transcript_history'],
      legacy: 'Did not exist',
      tag: 'New',
    },
    {
      id: '06', name: 'Feedback & tray',
      intent: 'What I see and hear when AutoWhisper is working.',
      keys: ['ui.tray_style', 'ui.recording_hud', 'ui.sound_start', 'ui.sound_end', 'ui.notify_on_error'],
      legacy: 'Partially in [tray]',
      tag: '',
    },
    {
      id: '07', name: 'Diagnostics',
      intent: 'When something is wrong, where do I look first?',
      keys: ['logs.level', 'logs.path', 'doctor.last_run', '(actions) export bundle, run doctor'],
      legacy: 'Was the CLI only',
      tag: 'New',
    },
    {
      id: '08', name: 'Advanced & raw config',
      intent: 'Edit config.toml directly with a live diff back to the form.',
      keys: ['(passthrough) all keys', 'plugins.enabled', 'experimental.*'],
      legacy: 'Was the form itself, exposed',
      tag: 'Escape hatch',
    },
  ];

  return (
    <div style={{ padding: '48px 56px', background: 'var(--aw-paper-1)', height: '100%', overflow: 'auto' }}>
      <div className="aw-eyebrow" style={{ marginBottom: 8 }}>08 · Information architecture</div>
      <h2 style={{ fontSize: 28, fontWeight: 600, letterSpacing: -0.4, margin: 0, marginBottom: 8 }}>From config sections to user intent.</h2>
      <p style={{ fontSize: 14, color: 'var(--aw-ink-2)', marginTop: 0, marginBottom: 32, maxWidth: 720, lineHeight: 1.55 }}>
        Today the settings UI mirrors the JSON schema: <Mono>[whisper]</Mono>, <Mono>[audio]</Mono>, <Mono>[x11]</Mono>. That structure is convenient for the parser, not the operator. We regroup around the eight questions a user actually asks.
      </p>

      <div style={{ borderTop: '1.5px solid var(--aw-ink-0)' }}>
        {groups.map((g) => (
          <div key={g.id} style={{ display: 'grid', gridTemplateColumns: '40px 1fr 1.4fr 100px', gap: 24, padding: '20px 0', borderBottom: '1px solid var(--aw-line-2)', alignItems: 'baseline' }}>
            <div className="aw-mono" style={{ fontSize: 12, color: 'var(--aw-signal)', fontWeight: 600 }}>{g.id}</div>
            <div>
              <div style={{ fontSize: 17, fontWeight: 600, marginBottom: 3, letterSpacing: -0.2 }}>{g.name}</div>
              <div style={{ fontSize: 13, color: 'var(--aw-ink-2)', lineHeight: 1.5, fontStyle: 'italic' }}>"{g.intent}"</div>
              <div style={{ fontSize: 11, color: 'var(--aw-ink-3)', marginTop: 6 }}>
                <span className="aw-eyebrow" style={{ fontSize: 9 }}>Was: </span>{g.legacy}
              </div>
            </div>
            <div style={{ display: 'flex', flexWrap: 'wrap', gap: 4 }}>
              {g.keys.map((k) => (
                <span key={k} style={{ fontFamily: 'var(--aw-font-mono)', fontSize: 10, padding: '2px 6px', background: 'var(--aw-paper-2)', border: '1px solid var(--aw-line-2)', borderRadius: 3, color: 'var(--aw-ink-1)' }}>{k}</span>
              ))}
            </div>
            <div style={{ textAlign: 'right' }}>
              {g.tag && <span style={{ fontSize: 10, fontFamily: 'var(--aw-font-mono)', color: g.tag === 'New' ? 'var(--aw-signal)' : 'var(--aw-ink-3)', textTransform: 'uppercase', letterSpacing: 0.06, fontWeight: 500 }}>{g.tag}</span>}
            </div>
          </div>
        ))}
      </div>
    </div>
  );
}

window.IAMapArtboard = IAMapArtboard;
