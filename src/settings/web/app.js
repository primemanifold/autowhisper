(async () => {
  const [schema, config] = await Promise.all([
    fetch("/api/schema").then(r => r.json()),
    fetch("/api/config").then(r => r.json()),
  ]);
  const el = document.getElementById("app");
  el.textContent = "Schema and config loaded. Full UI pending.";
  console.log("schema:", schema);
  console.log("config:", config);
})();
