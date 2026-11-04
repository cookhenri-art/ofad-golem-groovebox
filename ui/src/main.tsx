// Groovebox — OFAD — AGPL-3.0-or-later
import "./boot";
import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import "./styles.css";
import "./skin/skin.css";

// v0.32.1 : pas de menu du navigateur au clic droit (Retour, Actualiser, Imprimer…) : « Actualiser »
// rechargerait l'interface en plein jeu. Les champs de texte gardent le leur (copier, coller).
document.addEventListener("contextmenu", (e) => {
  const t = e.target as HTMLElement | null;
  if (t && (t.tagName === "INPUT" || t.tagName === "TEXTAREA" || t.isContentEditable)) return;
  e.preventDefault();
});

// v0.46 fix28 : l'écran ne se zoome ni ne se déplace (pincer, Ctrl + molette, Ctrl + / - / 0 ; le double
// toucher est coupé par touch-action, sans perdre les touchers rapides des boutons) : le zoom du navigateur cassait la disposition. Les gestes à deux doigts des commandes
// (pavé vectoriel : taille et rotation) passent par les évènements de pointeur et restent actifs.
document.addEventListener("wheel", (e) => { if (e.ctrlKey) e.preventDefault(); }, { passive: false });
document.addEventListener("keydown", (e) => {
  if ((e.ctrlKey || e.metaKey) && ["+", "-", "=", "0", "_"].includes(e.key)) e.preventDefault();
}, { capture: true });
document.addEventListener("touchmove", (e) => { if (e.touches.length > 1) e.preventDefault(); }, { passive: false });
for (const g of ["gesturestart", "gesturechange", "gestureend"]) document.addEventListener(g, (e) => e.preventDefault(), { passive: false });

async function start() {
  // Dans un simple navigateur, le moteur simulé remplace le plugin (développement, captures).
  if (typeof window.__JUCE__ === "undefined") (await import("./engine/mock")).installMockBackend();
  // la bibliothèque JUCE lit window.__JUCE__ dès son chargement : tout le reste est chargé ensuite
  const [{ connect }, { App }] = await Promise.all([import("./engine/store"), import("./App")]);
  createRoot(document.getElementById("root")!).render(
    <StrictMode>
      <App />
    </StrictMode>,
  );
  await connect();
}

void start();
