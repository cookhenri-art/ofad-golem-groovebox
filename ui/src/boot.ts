// Groovebox — OFAD — AGPL-3.0-or-later
// Hors du plugin (navigateur de développement), le moteur simulé est chargé à part
// (voir main.tsx) : il n'alourdit pas l'interface embarquée dans le plugin.

// Dans le plugin : les erreurs de l'interface sont recopiées côté moteur (sortie d'erreur),
// pour pouvoir diagnostiquer sans outils de développement.
if (window.__JUCE__?.initialisationData?.__juce__functions?.includes("uiLog")) {
  const send = (text: string) => {
    try {
      window.__JUCE__.backend.emitEvent("__juce__invoke", { name: "uiLog", params: [text], resultId: -1 });
    } catch { /* rien */ }
  };
  window.addEventListener("error", (e) => send(`${e.message} @ ${e.filename}:${e.lineno}`));
  window.addEventListener("unhandledrejection", (e) => send(`Promesse rejetée : ${String(e.reason)}`));
  const original = console.error.bind(console);
  console.error = (...args: unknown[]) => { send(args.map(String).join(" ")); original(...args); };
  const warn = console.warn.bind(console);
  console.warn = (...args: unknown[]) => { send(args.map(String).join(" ")); warn(...args); };
}


// Écrans tactiles (tablette Android) : un appui long vaut un clic droit. Le navigateur intégré
// envoie souvent l'événement « contextmenu » de lui-même ; sinon on l'envoie après 550 ms
// d'appui immobile. Le menu du système (copier, sélectionner) est désactivé.
{
  let timer = 0;
  let start: { x: number; y: number; target: EventTarget | null } | null = null;
  let fired = false;
  let touching = false;
  let swallowClickUntil = 0;
  window.addEventListener("contextmenu", () => { fired = true; if (start || touching) swallowClickUntil = performance.now() + 700; }, true);
  // le relâchement qui suit un appui long ne doit pas compter comme un toucher
  window.addEventListener("click", (e) => {
    if (performance.now() < swallowClickUntil) { e.preventDefault(); e.stopPropagation(); swallowClickUntil = 0; }
  }, true);
  window.addEventListener("pointerdown", (e) => {
    if (e.pointerType !== "touch") return;
    fired = false;
    touching = true;
    start = { x: e.clientX, y: e.clientY, target: e.target };
    window.clearTimeout(timer);
    timer = window.setTimeout(() => {
      if (!start || fired || !(start.target instanceof Element)) return;
      start.target.dispatchEvent(new MouseEvent("contextmenu", { bubbles: true, cancelable: true, clientX: start.x, clientY: start.y, button: 2 }));
      swallowClickUntil = performance.now() + 2000; // jusqu'au relâchement
      start = null;
    }, 550);
  }, true);
  const cancel = () => { window.clearTimeout(timer); start = null; };
  window.addEventListener("pointerup", () => { cancel(); window.setTimeout(() => { touching = false; }, 0); }, true);
  window.addEventListener("pointercancel", cancel, true);
  window.addEventListener("pointermove", (e) => {
    if (start && Math.hypot(e.clientX - start.x, e.clientY - start.y) > 10) cancel();
  }, true);
}
