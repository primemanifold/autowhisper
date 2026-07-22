(async () => {
  const form = document.getElementById("settings-form");
  const status = document.getElementById("status");
  const saveBtn = document.getElementById("save-btn");
  const resetBtn = document.getElementById("reset-btn");
  const dirtyPill = document.getElementById("dirty-pill");
  const sectionTitle = document.getElementById("active-section-title");
  const sectionLede = document.getElementById("active-section-lede");
  const nav = document.getElementById("settings-nav");
  const themeSwitch = document.getElementById("theme-switch");

  // Appearance: "system" follows the OS, "light"/"dark" force a scheme,
  // "dev" is the phosphor-terminal theme. index.html applies the stored
  // choice before first paint; this block keeps the switcher in sync.
  const THEME_KEY = "aw-theme";
  function applyTheme(choice) {
    if (choice === "system") delete document.documentElement.dataset.theme;
    else document.documentElement.dataset.theme = choice;
    themeSwitch?.querySelectorAll("[data-theme-choice]").forEach((button) => {
      button.setAttribute("aria-pressed", String(button.dataset.themeChoice === choice));
    });
  }
  function storedTheme() {
    try {
      const value = localStorage.getItem(THEME_KEY);
      return ["light", "dark", "dev"].includes(value) ? value : "system";
    } catch (_) {
      return "system";
    }
  }
  applyTheme(storedTheme());
  themeSwitch?.addEventListener("click", (event) => {
    const button = event.target.closest("button[data-theme-choice]");
    if (!button) return;
    const choice = button.dataset.themeChoice;
    applyTheme(choice);
    try {
      if (choice === "system") localStorage.removeItem(THEME_KEY);
      else localStorage.setItem(THEME_KEY, choice);
    } catch (_) {}
  });

  // Session token issued by the local settings server. It arrives once via
  // the launch URL; keep it in sessionStorage so same-tab reloads work, and
  // send it on every API call as a Bearer header.
  const urlToken = new URLSearchParams(window.location.search).get("token");
  if (urlToken) {
    sessionStorage.setItem("aw-session-token", urlToken);
  }
  const sessionToken = urlToken || sessionStorage.getItem("aw-session-token") || "";
  const authHeaders = sessionToken ? { Authorization: "Bearer " + sessionToken } : {};

  const IA_SECTIONS = [
    {
      id: "dictation",
      title: "Dictation behavior",
      lede: "Choose separate shortcuts for local Dictate and opt-in Ask Fabric capture.",
      sections: ["hotkeys"],
    },
    {
      id: "fabric",
      title: "Ask Fabric",
      lede: "Speak a question, let your configured Fabric agent answer it, then insert the answer into the focused app.",
      sections: ["fabric"],
      note: "Ask Fabric is disabled by default. When enabled, only its dedicated shortcut sends a transcript to Fabric using Fabric's non-terminal safe toolset. Dictate always stays on the local transcription-and-insertion path.",
    },
    {
      id: "model",
      title: "Model & performance",
      lede: "Pick the local Whisper model, hardware target, precision, language, and CPU thread budget.",
      sections: ["model"],
    },
    {
      id: "audio",
      title: "Audio input",
      lede: "Set the microphone path and capture behavior before the model ever sees audio.",
      sections: ["audio"],
    },
    {
      id: "output",
      title: "Output & insertion",
      lede: "Control how text reaches the current app, how transcripts are cleaned up, and what fallback behavior is allowed.",
      sections: ["output", "formatting"],
    },
    {
      id: "privacy",
      title: "Privacy",
      lede: "Local dictation stays local; network-capable agent use is explicit and mode-bound.",
      sections: [],
      note: "AutoWhisper has no telemetry. Dictate never invokes Fabric. If you enable Ask Fabric, that shortcut sends its transcript over a private stdin pipe to your configured Fabric CLI, whose model provider and retention policy then apply.",
    },
    {
      id: "feedback",
      title: "Feedback & tray",
      lede: "Tones, tray visibility, and the floating companion that listens and writes with you.",
      sections: ["feedback", "tray", "avatar"],
    },
    {
      id: "diagnostics",
      title: "Diagnostics",
      lede: "Keep logs and daemon settings legible so failures lead to action instead of guesswork.",
      sections: ["daemon"],
    },
    {
      id: "advanced",
      title: "Advanced",
      lede: "All schema-backed settings in their raw sections for operators who want the full config surface.",
      sections: "all",
    },
  ];

  // The cast: presentation metadata for the companion's character picker.
  // The schema enum stays the source of truth — unknown values fall back to
  // the plain select, so a new spirit degrades gracefully.
  const CHARACTER_META = {
    echo:      { title: "Echo",      epithet: "the one who answers",  accent: "#db4233", sig: "none" },
    hermes:    { title: "Hermes",    epithet: "the swift herald",     accent: "#4078f2", sig: "wings" },
    mnemosyne: { title: "Mnemosyne", epithet: "keeper of memory",     accent: "#d4942e", sig: "inner" },
    kalliope:  { title: "Kalliope",  epithet: "the beautiful-voiced", accent: "#9e52e0", sig: "crown" },
    morpheus:  { title: "Morpheus",  epithet: "the shape of dreams",  accent: "#2e9994", sig: "motes" },
  };


  const dirtyKeys = new Set();
  let schema;
  let config;
  let defaults;
  let platformDiagnostics;
  let modelCatalog;
  let permissions;
  let sections = [];

  setBusy(true, "Loading local settings...");
  try {
    [schema, config, defaults, platformDiagnostics, modelCatalog, permissions] = await Promise.all([
      loadJson("/api/schema"),
      loadJson("/api/config"),
      loadJson("/api/defaults"),
      loadJson("/api/platform"),
      loadJson("/api/models").catch(() => null),
      loadJson("/api/permissions").catch(() => null),
    ]);
    sections = Object.keys(schema);
  } catch (e) {
    setBusy(false);
    setStatus("Failed to load settings: " + e.message, "err");
    saveBtn.disabled = true;
    resetBtn.disabled = true;
    return;
  }

  renderAllPanes();
  bindNavigation();
  activatePane(paneIdFromHash());
  window.addEventListener("hashchange", () => activatePane(paneIdFromHash()));
  bindFormDirtyTracking();
  updateDirtyState();
  showVersion();
  setBusy(false);
  setStatus("Loaded from the local AutoWhisper settings server.", "ok");

  // Surface the running version in the sidebar and the window title so it's
  // obvious which build this is (it reported "I don't know what version").
  function showVersion() {
    const v = platformDiagnostics?.version;
    if (!v) return;
    const note = document.querySelector(".aw-brand-note");
    if (note) note.textContent = `local dictation · v${v}`;
    document.title = `AutoWhisper Settings · v${v}`;
  }

  saveBtn.addEventListener("click", async () => {
    const body = collect();
    clearIssues();
    setBusy(true, "Saving settings...");
    try {
      const res = await fetch("/api/config", {
        method: "PUT",
        headers: { "Content-Type": "application/json", ...authHeaders },
        body: JSON.stringify(body),
      });
      if (res.status === 204) {
        config = body;
        dirtyKeys.clear();
        clearIssues();
        updateDirtyState();
        setStatus("Saved. Shortcuts and Ask Fabric update when AutoWhisper is idle; other changes apply on the next run.", "ok");
      } else {
        let detail = res.statusText;
        try {
          const body = await res.json();
          const normalized = normalizeSaveError(body, detail);
          detail = normalized.summary;
          if (normalized.issues.length > 0) {
            applyIssues(normalized.issues);
          }
        } catch (_) {}
        setStatus("Could not save: " + detail, "err");
      }
    } catch (e) {
      setStatus("Could not save: " + e.message, "err");
    } finally {
      setBusy(false);
    }
  });

  // Save from anywhere with the keyboard; never lose edits to a stray close.
  window.addEventListener("keydown", (event) => {
    if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "s") {
      event.preventDefault();
      if (!saveBtn.disabled) saveBtn.click();
    }
  });
  window.addEventListener("beforeunload", (event) => {
    if (dirtyKeys.size > 0) {
      event.preventDefault();
      event.returnValue = "";
    }
  });

  resetBtn.addEventListener("click", () => {
    for (const section of sections) {
      for (const keyDef of schema[section]) {
        setInput(section, keyDef, defaults[section]?.[keyDef.key]);
        dirtyKeys.add(`${section}.${keyDef.key}`);
      }
    }
    updateDirtyState();
    setStatus("Defaults staged. Review, then save to write them to config.toml.", "warn");
  });

  async function loadJson(url) {
    const r = await fetch(url, { headers: authHeaders });
    if (!r.ok) {
      let detail = r.statusText;
      try {
        const body = await r.json();
        detail = body.error || (body.errors && body.errors.join("; ")) || detail;
      } catch (_) {}
      throw new Error(`${url} -> HTTP ${r.status}: ${detail}`);
    }
    return r.json();
  }

  function renderAllPanes() {
    form.textContent = "";
    for (const pane of IA_SECTIONS) {
      const article = document.createElement("article");
      article.className = "aw-pane" + (pane.id === "dictation" ? " is-active" : "");
      article.id = `pane-${pane.id}`;
      article.dataset.pane = pane.id;
      article.setAttribute("aria-labelledby", `heading-${pane.id}`);

      const head = document.createElement("div");
      head.className = "aw-pane-head";
      const h2 = document.createElement("h2");
      h2.id = `heading-${pane.id}`;
      h2.textContent = pane.title;
      const p = document.createElement("p");
      p.textContent = pane.lede;
      head.append(h2, p);
      article.appendChild(head);

      if (pane.note) {
        const note = document.createElement("div");
        note.className = "aw-note";
        note.innerHTML = `<strong>Local-first proof point.</strong> ${escapeHtml(pane.note)}`;
        article.appendChild(note);
      }

      if (pane.id === "diagnostics" && platformDiagnostics) {
        article.appendChild(renderPlatformDiagnostics(platformDiagnostics));
      }

      if (pane.id === "model" && modelCatalog?.models?.length) {
        article.appendChild(renderModelCard(modelCatalog));
      }

      if (pane.id === "privacy" && permissions?.applicable && permissions.permissions?.length) {
        article.appendChild(renderPermissions(permissions));
      }

      const paneSections = pane.sections === "all" ? sections : pane.sections;
      if (paneSections.length === 0 && !pane.note) {
        const empty = document.createElement("div");
        empty.className = "aw-empty";
        empty.textContent = "No schema-backed settings are exposed for this section yet.";
        article.appendChild(empty);
      }

      for (const section of paneSections) {
        if (!schema[section]) continue;
        article.appendChild(renderSectionCard(section, schema[section], pane.id, pane.id === "advanced"));
      }
      form.appendChild(article);
    }
  }

  function renderPlatformDiagnostics(info) {
    const card = document.createElement("section");
    card.className = "aw-card aw-platform-card";
    card.setAttribute("aria-label", "Desktop platform readiness");

    const header = document.createElement("div");
    header.className = "aw-card-header";
    const title = document.createElement("div");
    title.className = "aw-card-title";
    title.textContent = "Desktop platform readiness";
    const meta = document.createElement("div");
    meta.className = "aw-card-meta";
    const platformLabel = info.platform || "unknown";
    const targetLabel = info.build_target || "host";
    meta.textContent = platformLabel === targetLabel ? platformLabel : `${platformLabel} · ${targetLabel}`;
    header.append(title, meta);
    card.appendChild(header);

    const summary = document.createElement("p");
    summary.className = "aw-platform-summary";
    summary.textContent = info.summary || "Platform capability status is reported by the local build.";
    card.appendChild(summary);

    const grid = document.createElement("div");
    grid.className = "aw-platform-grid";
    for (const feature of info.features || []) {
      const item = document.createElement("article");
      item.className = `aw-platform-feature ${feature.state || "unknown"}`;
      const featureTitle = document.createElement("h3");
      featureTitle.textContent = feature.name || feature.id || "Feature";
      const state = document.createElement("span");
      state.className = "aw-platform-state";
      state.textContent = feature.state || "unknown";
      const detail = document.createElement("p");
      detail.textContent = feature.detail || "No detail available.";
      item.append(featureTitle, state, detail);
      grid.appendChild(item);
    }
    card.appendChild(grid);
    return card;
  }

  // The model availability card. Both the headline state and every row badge
  // derive from one boolean — `downloaded` from /api/models (an on-disk
  // check) — so the UI can never show "download recommended" and "ready" at
  // once (the macOS bug). It re-evaluates when the model select changes.
  function renderModelCard(catalog) {
    const card = document.createElement("section");
    card.className = "aw-card aw-model-card";
    card.setAttribute("aria-label", "Model availability");

    const header = document.createElement("div");
    header.className = "aw-card-header";
    const title = document.createElement("div");
    title.className = "aw-card-title";
    title.textContent = "Model availability";
    const meta = document.createElement("div");
    meta.className = "aw-card-meta";
    meta.textContent = `${catalog.models.length} models`;
    header.append(title, meta);
    card.appendChild(header);

    const headline = document.createElement("div");
    headline.className = "aw-model-headline";
    card.appendChild(headline);

    const grid = document.createElement("div");
    grid.className = "aw-model-grid";
    for (const m of catalog.models) {
      const row = document.createElement("div");
      row.className = "aw-model-row" + (m.downloaded ? " is-ready" : "");
      row.dataset.model = m.name;
      const name = document.createElement("div");
      name.className = "aw-model-name";
      name.textContent = m.name;
      const desc = document.createElement("div");
      desc.className = "aw-model-desc";
      desc.textContent = `${m.description} · ${m.size}${m.english_only ? " · English" : " · multilingual"}`;
      const badge = document.createElement("span");
      badge.className = "aw-model-badge " + (m.downloaded ? "ready" : "absent");
      badge.textContent = m.downloaded ? "Downloaded" : "Not downloaded";
      row.append(name, desc, badge);
      grid.appendChild(row);
    }
    card.appendChild(grid);

    const byName = Object.fromEntries(catalog.models.map((m) => [m.name, m]));
    function refresh(selected) {
      const current = byName[selected];
      grid.querySelectorAll(".aw-model-row").forEach((r) =>
        r.classList.toggle("is-current", r.dataset.model === selected));
      if (!current) {
        headline.className = "aw-model-headline";
        headline.textContent = `Selected model "${selected}" is not in the catalog.`;
        return;
      }
      if (current.downloaded) {
        headline.className = "aw-model-headline ready";
        headline.textContent = `${current.name} is downloaded and ready.`;
      } else {
        headline.className = "aw-model-headline absent";
        const text = document.createElement("span");
        text.textContent = `${current.name} is not downloaded yet. Fetch it with `;
        const code = document.createElement("code");
        code.textContent = `autowhisper model download ${current.name}`;
        headline.replaceChildren(text, code);
      }
    }
    refresh(config.model?.size);
    // Keep the headline honest as the user picks a different model.
    document.addEventListener("change", (event) => {
      const el = event.target;
      if (el?.dataset?.configKey === "model.size") refresh(el.value);
    });
    return card;
  }

  // OS permission status (macOS TCC). A denied "Input Monitoring" is exactly
  // why push-to-talk died silently for the tester — so we surface each grant
  // with a status dot and a button that deep-links to the right System
  // Settings pane.
  function renderPermissions(info) {
    const card = document.createElement("section");
    card.className = "aw-card aw-perms-card";
    card.setAttribute("aria-label", "System permissions");

    const header = document.createElement("div");
    header.className = "aw-card-header";
    const title = document.createElement("div");
    title.className = "aw-card-title";
    title.textContent = "System permissions";
    const meta = document.createElement("div");
    meta.className = "aw-card-meta";
    meta.textContent = info.platform || "macOS";
    header.append(title, meta);
    card.appendChild(header);

    const lede = document.createElement("p");
    lede.className = "aw-platform-summary";
    lede.textContent = "AutoWhisper needs these grants to listen and to type. " +
      "If a required one is denied, the matching feature stays silent.";
    card.appendChild(lede);

    const list = document.createElement("div");
    list.className = "aw-perms-list";
    const GRANTED = new Set(["granted", "not_applicable"]);
    for (const p of info.permissions) {
      const row = document.createElement("div");
      const ok = GRANTED.has(p.state);
      const denied = p.state === "denied" || p.state === "restricted";
      row.className = "aw-perm-row " + (ok ? "ok" : denied ? "err" : "warn");
      const dot = document.createElement("span");
      dot.className = "aw-perm-dot";
      const body = document.createElement("div");
      const name = document.createElement("div");
      name.className = "aw-perm-name";
      name.textContent = `${p.name} — ${humanize(p.state)}`;
      const detail = document.createElement("div");
      detail.className = "aw-perm-detail";
      detail.textContent = p.detail || "";
      body.append(name, detail);
      row.append(dot, body);
      if (p.deep_link && !ok) {
        const open = document.createElement("a");
        open.className = "aw-button aw-perm-open";
        open.href = p.deep_link;
        open.textContent = "Open settings";
        row.appendChild(open);
      }
      list.appendChild(row);
    }
    card.appendChild(list);
    return card;
  }

  function renderSectionCard(section, keys, paneId, showSectionPrefix) {
    const card = document.createElement("section");
    card.className = "aw-card";
    const header = document.createElement("div");
    header.className = "aw-card-header";
    const title = document.createElement("div");
    title.className = "aw-card-title";
    title.textContent = humanize(section);
    const meta = document.createElement("div");
    meta.className = "aw-card-meta";
    meta.textContent = `[${section}] · ${keys.length} settings`;
    header.append(title, meta);
    card.appendChild(header);
    for (const keyDef of keys) {
      card.appendChild(renderRow(paneId, section, keyDef, config[section]?.[keyDef.key], showSectionPrefix));
    }
    return card;
  }

  function renderRow(paneId, section, keyDef, value, showSectionPrefix) {
    const row = document.createElement("div");
    row.className = "aw-field";
    row.dataset.configKey = `${section}.${keyDef.key}`;
    const labelWrap = document.createElement("div");
    const label = document.createElement("label");
    const id = inputId(paneId, section, keyDef.key);
    label.htmlFor = id;
    label.textContent = humanize(keyDef.key);
    const key = document.createElement("span");
    key.className = "aw-config-key";
    key.textContent = showSectionPrefix ? `${section}.${keyDef.key}` : keyDef.key;
    label.appendChild(key);
    labelWrap.appendChild(label);

    const controlWrap = document.createElement("div");
    controlWrap.className = "aw-control";
    controlWrap.appendChild(renderInput(paneId, section, keyDef, value));
    const describedBy = [];
    if (keyDef.description) {
      const desc = document.createElement("div");
      desc.className = "aw-desc";
      desc.id = id + "-desc";
      desc.textContent = keyDef.description;
      controlWrap.appendChild(desc);
      describedBy.push(desc.id);
    }
    const issue = document.createElement("div");
    issue.className = "aw-issue";
    issue.id = id + "-issue";
    issue.hidden = true;
    controlWrap.appendChild(issue);
    describedBy.push(issue.id);
    const describedControl = controlWrap.querySelector("input, select");
    describedControl?.setAttribute("aria-describedby", describedBy.join(" "));

    row.append(labelWrap, controlWrap);
    return row;
  }

  // The sigil (ring + side dashes) plus each spirit's signature, as a small
  // inline SVG — the same silhouettes the C++ rasterizer draws.
  function characterGlyph(sig) {
    const svgNS = "http://www.w3.org/2000/svg";
    const svg = document.createElementNS(svgNS, "svg");
    svg.setAttribute("viewBox", "0 0 48 48");
    svg.setAttribute("class", "aw-cast-glyph");
    svg.setAttribute("aria-hidden", "true");
    const add = (tag, attrs) => {
      const el = document.createElementNS(svgNS, tag);
      for (const [k, v] of Object.entries(attrs)) el.setAttribute(k, v);
      svg.appendChild(el);
    };
    const stroke = { fill: "none", stroke: "currentColor", "stroke-width": "2", "stroke-linecap": "round" };
    add("circle", { cx: 24, cy: 26, r: 8.5, ...stroke });
    add("line", { x1: 9, y1: 26, x2: 13, y2: 26, ...stroke });
    add("line", { x1: 35, y1: 26, x2: 39, y2: 26, ...stroke });
    if (sig === "wings") {
      add("line", { x1: 11, y1: 19, x2: 7, y2: 14, ...stroke });
      add("line", { x1: 37, y1: 19, x2: 41, y2: 14, ...stroke });
    } else if (sig === "inner") {
      add("circle", { cx: 24, cy: 26, r: 4, ...stroke, "stroke-width": "1.6" });
    } else if (sig === "crown") {
      add("circle", { cx: 17, cy: 13, r: 1.6, fill: "currentColor" });
      add("circle", { cx: 24, cy: 11, r: 2.1, fill: "currentColor" });
      add("circle", { cx: 31, cy: 13, r: 1.6, fill: "currentColor" });
    } else if (sig === "motes") {
      add("circle", { cx: 33, cy: 14, r: 2.1, fill: "currentColor" });
      add("circle", { cx: 37, cy: 10, r: 1.5, fill: "currentColor" });
      add("circle", { cx: 40, cy: 6.5, r: 1.0, fill: "currentColor" });
    }
    return svg;
  }

  function renderCharacterPicker(paneId, section, keyDef, value) {
    const name = `${section}.${keyDef.key}`;
    const cast = document.createElement("fieldset");
    cast.className = "aw-cast";
    cast.setAttribute("aria-label", "Companion character");
    for (const v of keyDef.enum_values || []) {
      const meta = CHARACTER_META[v];
      const card = document.createElement("label");
      card.className = "aw-cast-card";
      const radio = document.createElement("input");
      radio.type = "radio";
      radio.name = `${paneId}:${name}`;
      radio.value = v;
      radio.id = `${inputId(paneId, section, keyDef.key)}-${v}`;
      radio.setAttribute("data-config-key", name);
      radio.checked = v === value;
      const head = document.createElement("span");
      head.className = "aw-cast-name";
      const dot = document.createElement("span");
      dot.className = "aw-cast-dot";
      dot.style.background = meta?.accent || "currentColor";
      head.append(dot, document.createTextNode(meta?.title || v));
      const epithet = document.createElement("span");
      epithet.className = "aw-cast-epithet";
      epithet.textContent = meta?.epithet || "";
      card.append(radio, characterGlyph(meta?.sig), head, epithet);
      cast.appendChild(card);
    }
    return cast;
  }

  function renderInput(paneId, section, keyDef, value) {
    const id = inputId(paneId, section, keyDef.key);
    const name = `${section}.${keyDef.key}`;
    if (keyDef.type === "enum" && name === "avatar.character" &&
        (keyDef.enum_values || []).every((v) => CHARACTER_META[v])) {
      return renderCharacterPicker(paneId, section, keyDef, value);
    }
    if (name === "hotkeys.trigger" || name === "hotkeys.ask_trigger") {
      return renderHotkeyCapture(paneId, section, keyDef, value);
    }
    if (keyDef.type === "enum") {
      const sel = document.createElement("select");
      sel.id = id;
      sel.name = name;
      sel.setAttribute("data-config-key", name);
      for (const v of keyDef.enum_values || []) {
        const opt = document.createElement("option");
        opt.value = v;
        opt.textContent = v;
        if (v === value) opt.selected = true;
        sel.appendChild(opt);
      }
      return sel;
    }
    if (keyDef.type === "bool") {
      // A label wrapper makes the whole row (box + state text) one target.
      const wrap = document.createElement("label");
      wrap.className = "aw-check";
      const cb = document.createElement("input");
      cb.type = "checkbox";
      cb.id = id;
      cb.name = name;
      cb.setAttribute("data-config-key", name);
      cb.checked = !!value;
      const text = document.createElement("span");
      text.textContent = value ? "Enabled" : "Disabled";
      cb.addEventListener("change", () => { text.textContent = cb.checked ? "Enabled" : "Disabled"; });
      wrap.append(cb, text);
      return wrap;
    }
    const inp = document.createElement("input");
    inp.id = id;
    inp.name = name;
    inp.setAttribute("data-config-key", name);
    inp.type = keyDef.type === "int" || keyDef.type === "float" ? "number" : "text";
    if (keyDef.type === "float") inp.step = "any";
    if (keyDef.type === "string_array") inp.placeholder = "comma-separated";
    if (keyDef.min_numeric !== null) inp.min = keyDef.min_numeric;
    if (keyDef.max_numeric !== null) inp.max = keyDef.max_numeric;
    inp.value = keyDef.type === "string_array" && Array.isArray(value) ? value.join(", ") : (value ?? "");
    return inp;
  }

  // Hotkey trigger: the schema field is a comma-separated string_array, but
  // typing "shift+super" is exactly what confused the Mac tester. This keeps
  // the editable field (collect/setInput stay unchanged) and adds a Record
  // button that captures a real key chord and writes the canonical token.
  function renderHotkeyCapture(paneId, section, keyDef, value) {
    const wrap = document.createElement("div");
    wrap.className = "aw-hotkey";

    const inp = document.createElement("input");
    inp.id = inputId(paneId, section, keyDef.key);
    inp.name = `${section}.${keyDef.key}`;
    inp.type = "text";
    inp.setAttribute("data-config-key", inp.name);
    inp.placeholder = "e.g. ctrl+alt+space (comma-separated for more)";
    inp.value = Array.isArray(value) ? value.join(", ") : (value ?? "");

    const record = document.createElement("button");
    record.type = "button";
    record.className = "aw-button aw-hotkey-record";
    record.textContent = "Record shortcut";

    // JS modifiers/keys -> the C++ KeyCombo grammar (super = Command/Win).
    const MODS = [["ctrlKey", "ctrl"], ["altKey", "alt"], ["shiftKey", "shift"], ["metaKey", "super"]];
    const BARE = { " ": "space", "spacebar": "space", "escape": "esc", "enter": "enter",
                   "return": "enter", "tab": "tab", "backspace": "backspace", "delete": "delete" };
    function baseKey(e) {
      const k = e.key;
      if (["Control", "Alt", "Shift", "Meta", "OS"].includes(k)) return "";
      const low = k.toLowerCase();
      if (BARE[low]) return BARE[low];
      if (low.length === 1) return low;          // letters, digits, punctuation
      if (/^f\d{1,2}$/.test(low)) return low;    // function keys
      if (low.startsWith("arrow")) return low.slice(5);
      return low;
    }
    function modsOf(e) { return MODS.filter(([p]) => e[p]).map(([, n]) => n); }

    let capturing = false;
    let maxMods = [];
    function stop(commit) {
      capturing = false;
      window.removeEventListener("keydown", onDown, true);
      window.removeEventListener("keyup", onUp, true);
      record.textContent = "Record shortcut";
      record.classList.remove("is-recording");
      if (commit) {
        inp.value = commit;
        inp.dispatchEvent(new Event("input", { bubbles: true }));
        inp.dispatchEvent(new Event("change", { bubbles: true }));
      }
    }
    function onDown(e) {
      e.preventDefault();
      e.stopPropagation();
      if (e.key === "Escape" && modsOf(e).length === 0) { stop(null); return; }
      const mods = modsOf(e);
      maxMods = MODS.map(([, n]) => n).filter((n) => mods.includes(n) || maxMods.includes(n));
      const key = baseKey(e);
      if (key) stop([...mods, key].join("+"));   // modifier+key: commit at once
    }
    function onUp(e) {
      e.preventDefault();
      // Modifier-only chord: commit the held set once everything is released.
      if (!e.ctrlKey && !e.altKey && !e.shiftKey && !e.metaKey && maxMods.length) {
        stop(maxMods.join("+"));
      }
    }
    record.addEventListener("click", () => {
      if (capturing) { stop(null); return; }
      capturing = true;
      maxMods = [];
      record.textContent = "Press keys… (Esc to cancel)";
      record.classList.add("is-recording");
      window.addEventListener("keydown", onDown, true);
      window.addEventListener("keyup", onUp, true);
    });

    wrap.append(inp, record);
    return wrap;
  }

  function bindNavigation() {
    nav.addEventListener("click", (event) => {
      const button = event.target.closest("button[data-target]");
      if (!button) return;
      activatePane(button.dataset.target, { updateHash: true });
    });
  }

  function paneIdFromHash() {
    const rawHash = window.location.hash.replace(/^#/, "");
    if (!rawHash) return IA_SECTIONS[0].id;
    try {
      const decoded = decodeURIComponent(rawHash);
      return decoded.startsWith("pane-") ? decoded.slice(5) : decoded;
    } catch (_) {
      return IA_SECTIONS[0].id;
    }
  }

  function activatePane(id, options = {}) {
    const pane = IA_SECTIONS.find((candidate) => candidate.id === id) || IA_SECTIONS[0];
    document.querySelectorAll(".aw-pane").forEach((el) => el.classList.toggle("is-active", el.dataset.pane === pane.id));
    document.querySelectorAll(".aw-nav-item").forEach((el) => {
      const isActive = el.dataset.target === pane.id;
      el.classList.toggle("is-active", isActive);
      if (isActive) el.setAttribute("aria-current", "page");
      else el.removeAttribute("aria-current");
    });
    if (options.updateHash) {
      const nextHash = `#pane-${pane.id}`;
      if (window.location.hash !== nextHash) history.replaceState(null, "", nextHash);
    }
    sectionTitle.textContent = pane.title;
    sectionLede.textContent = pane.lede;
  }

  function bindFormDirtyTracking() {
    form.addEventListener("input", (event) => markDirtyFromEvent(event));
    form.addEventListener("change", (event) => markDirtyFromEvent(event));
  }

  function markDirtyFromEvent(event) {
    const input = event.target.closest("input, select");
    if (!input?.name) return;
    // Radios carry a pane-prefixed group name; the config key is canonical.
    const key = input.dataset.configKey || input.name;
    dirtyKeys.add(key);
    clearIssues(key);
    syncMatchingInputs(input);
    updateDirtyState();
  }

  function setInput(section, keyDef, value) {
    document.querySelectorAll(`[data-config-key="${section}.${keyDef.key}"]`).forEach((el) => {
      if (!el.type) return; // the row div carries the key too — controls only
      if (el.type === "radio") {
        el.checked = el.value === value;
      } else if (el.type === "checkbox") {
        el.checked = !!value;
        const label = el.parentElement?.querySelector("span");
        if (label) label.textContent = el.checked ? "Enabled" : "Disabled";
      } else if (keyDef.type === "string_array") {
        el.value = Array.isArray(value) ? value.join(", ") : "";
      } else {
        el.value = value ?? "";
      }
    });
  }

  function collect() {
    const out = {};
    for (const section of sections) {
      out[section] = {};
      for (const keyDef of schema[section]) {
        const el = document.querySelector(
          `input[data-config-key="${section}.${keyDef.key}"], select[data-config-key="${section}.${keyDef.key}"]`);
        if (!el) continue;
        if (el.type === "radio") {
          const checked = document.querySelector(`input[data-config-key="${section}.${keyDef.key}"]:checked`);
          out[section][keyDef.key] = checked ? checked.value : el.value;
        }
        else if (el.type === "checkbox") out[section][keyDef.key] = el.checked;
        else if (el.type === "number") out[section][keyDef.key] = keyDef.type === "int" ? parseInt(el.value, 10) : parseFloat(el.value);
        else if (keyDef.type === "string_array") out[section][keyDef.key] = el.value.split(",").map((s) => s.trim()).filter(Boolean);
        else out[section][keyDef.key] = el.value;
      }
    }
    return out;
  }

  function syncMatchingInputs(source) {
    const key = source.dataset.configKey || source.name;
    document.querySelectorAll(`[data-config-key="${key}"]`).forEach((target) => {
      if (target === source || !target.type) return;
      if (target.type === "radio") {
        target.checked = target.value === source.value;
      } else if (target.type === "checkbox") {
        target.checked = source.checked;
        const label = target.parentElement?.querySelector("span");
        if (label) label.textContent = target.checked ? "Enabled" : "Disabled";
      } else {
        target.value = source.value;
      }
    });
  }

  function normalizeSaveError(body, fallback) {
    const issues = Array.isArray(body?.issues) ? body.issues : [];
    const errors = Array.isArray(body?.errors) ? body.errors : [];
    const issueMessages = issues.map((issue) => issue?.message).filter(Boolean);
    return {
      issues,
      summary: issueMessages.join("; ") || errors.join("; ") || body?.error || fallback,
    };
  }

  function applyIssues(issues) {
    for (const issue of issues) {
      if (!issue?.path) continue;
      const tone = issue.severity === "warning" ? "warning" : "error";
      controlsForConfigKey(issue.path).forEach((control) => {
        const row = control.closest(".aw-field");
        const message = row?.querySelector(".aw-issue");
        row?.classList.add(tone === "warning" ? "has-warning" : "has-error");
        if (tone === "error") control.setAttribute("aria-invalid", "true");
        if (message) {
          message.hidden = false;
          message.className = `aw-issue ${tone === "warning" ? "warn" : "err"}`;
          message.textContent = `${tone === "warning" ? "Warning" : "Error"}: ${issue.message || issue.code || "Invalid value"}`;
        }
      });
    }
  }

  function clearIssues(configKey = "") {
    const controls = configKey ? controlsForConfigKey(configKey) : document.querySelectorAll(".aw-field input, .aw-field select");
    controls.forEach((control) => {
      control.removeAttribute("aria-invalid");
      const row = control.closest(".aw-field");
      row?.classList.remove("has-error", "has-warning");
      const message = row?.querySelector(".aw-issue");
      if (message) {
        message.hidden = true;
        message.className = "aw-issue";
        message.textContent = "";
      }
    });
  }

  function controlsForConfigKey(configKey) {
    return Array.from(document.querySelectorAll("input[data-config-key], select[data-config-key]"))
      .filter((control) => control.dataset.configKey === configKey);
  }

  function updateDirtyState() {
    const count = dirtyKeys.size;
    document.querySelectorAll(".aw-field[data-config-key]").forEach((row) => {
      row.classList.toggle("is-dirty", dirtyKeys.has(row.dataset.configKey));
    });
    saveBtn.disabled = count === 0;
    dirtyPill.textContent = count === 0 ? "saved" : `unsaved · ${count}`;
    dirtyPill.className = count === 0 ? "aw-pill aw-pill-ok" : "aw-pill aw-pill-warn";
  }

  function setBusy(isBusy, message = "") {
    saveBtn.disabled = isBusy || dirtyKeys.size === 0;
    resetBtn.disabled = isBusy;
    if (message) setStatus(message, "");
  }

  function setStatus(message, tone) {
    status.textContent = message;
    status.className = tone ? `aw-status ${tone}` : "aw-status";
  }

  function inputId(paneId, section, key) {
    return `field-${paneId}-${section}-${key}`.replace(/[^a-zA-Z0-9_-]/g, "-");
  }

  function humanize(value) {
    return value.replace(/_/g, " ").replace(/\b\w/g, (char) => char.toUpperCase());
  }

  function escapeHtml(value) {
    return String(value).replace(/[&<>'"]/g, (char) => ({
      "&": "&amp;",
      "<": "&lt;",
      ">": "&gt;",
      "'": "&#39;",
      '"': "&quot;",
    }[char]));
  }
})();
