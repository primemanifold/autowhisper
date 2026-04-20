(async () => {
  const [schema, config, defaults] = await Promise.all([
    fetch("/api/schema").then(r => r.json()),
    fetch("/api/config").then(r => r.json()),
    fetch("/api/defaults").then(r => r.json()),
  ]);

  const form = document.getElementById("settings-form");
  const pathEl = document.getElementById("config-path");
  const status = document.getElementById("status");

  const sections = Object.keys(schema);
  for (const section of sections) {
    const sec = document.createElement("section");
    sec.innerHTML = `<h2>${section}</h2>`;
    for (const keyDef of schema[section]) {
      sec.appendChild(renderRow(section, keyDef, config[section]?.[keyDef.key]));
    }
    form.appendChild(sec);
  }

  document.getElementById("save-btn").addEventListener("click", async () => {
    const body = collect();
    status.textContent = "Saving...";
    status.className = "";
    const res = await fetch("/api/config", {
      method: "PUT",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    if (res.status === 204) {
      status.textContent = "Saved.";
      status.className = "ok";
    } else {
      const j = await res.json();
      status.textContent = "Error: " + (j.errors?.join("; ") || j.error || res.statusText);
      status.className = "err";
    }
  });

  document.getElementById("reset-btn").addEventListener("click", () => {
    for (const section of sections) {
      for (const keyDef of schema[section]) {
        setInput(section, keyDef, defaults[section]?.[keyDef.key]);
      }
    }
    status.textContent = "Reset to defaults (not yet saved).";
    status.className = "";
  });

  function renderRow(section, keyDef, value) {
    const row = document.createElement("div");
    row.className = "row";
    const label = document.createElement("label");
    label.htmlFor = `${section}.${keyDef.key}`;
    label.textContent = `${section}.${keyDef.key}`;
    row.appendChild(label);
    row.appendChild(renderInput(section, keyDef, value));
    if (keyDef.description) {
      const d = document.createElement("div");
      d.className = "desc";
      d.textContent = keyDef.description;
      row.appendChild(d);
    }
    return row;
  }

  function renderInput(section, keyDef, value) {
    const id = `${section}.${keyDef.key}`;
    if (keyDef.type === "enum") {
      const sel = document.createElement("select");
      sel.id = id;
      for (const v of keyDef.enum_values) {
        const opt = document.createElement("option");
        opt.value = v; opt.textContent = v;
        if (v === value) opt.selected = true;
        sel.appendChild(opt);
      }
      return sel;
    }
    if (keyDef.type === "bool") {
      const cb = document.createElement("input");
      cb.type = "checkbox";
      cb.id = id;
      cb.checked = !!value;
      return cb;
    }
    if (keyDef.type === "int" || keyDef.type === "float") {
      const n = document.createElement("input");
      n.type = "number";
      n.id = id;
      if (keyDef.type === "float") n.step = "any";
      if (keyDef.min_numeric !== null) n.min = keyDef.min_numeric;
      if (keyDef.max_numeric !== null) n.max = keyDef.max_numeric;
      n.value = value ?? "";
      return n;
    }
    if (keyDef.type === "string_array") {
      const inp = document.createElement("input");
      inp.type = "text";
      inp.id = id;
      inp.placeholder = "comma-separated";
      inp.value = Array.isArray(value) ? value.join(", ") : "";
      return inp;
    }
    const inp = document.createElement("input");
    inp.type = "text";
    inp.id = id;
    inp.value = value ?? "";
    return inp;
  }

  function setInput(section, keyDef, value) {
    const id = `${section}.${keyDef.key}`;
    const el = document.getElementById(id);
    if (!el) return;
    if (el.type === "checkbox") el.checked = !!value;
    else if (keyDef.type === "string_array")
      el.value = Array.isArray(value) ? value.join(", ") : "";
    else el.value = value ?? "";
  }

  function collect() {
    const out = {};
    for (const section of sections) {
      out[section] = {};
      for (const keyDef of schema[section]) {
        const el = document.getElementById(`${section}.${keyDef.key}`);
        if (!el) continue;
        if (el.type === "checkbox") out[section][keyDef.key] = el.checked;
        else if (el.type === "number")
          out[section][keyDef.key] = keyDef.type === "int" ? parseInt(el.value, 10) : parseFloat(el.value);
        else if (keyDef.type === "string_array")
          out[section][keyDef.key] = el.value.split(",").map(s => s.trim()).filter(Boolean);
        else out[section][keyDef.key] = el.value;
      }
    }
    return out;
  }
})();
