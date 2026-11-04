// Groovebox — OFAD — AGPL-3.0-or-later
// v0.46 fix30 : interface en anglais. Le code garde ses textes français (ce sont aussi des clés : familles,
// noms de réglages…) ; la traduction se fait à l'AFFICHAGE : un observateur relit chaque texte posé dans
// la page (et les attributs title, aria-label, placeholder, alt) et le remplace par sa traduction.
// Recherche d'un texte, dans l'ordre :
//   1. le texte exact ;
//   2. le texte dont les nombres sont remplacés par « # » (« Pas 9 » -> « Step 9 ») ;
//   3. les modèles « … {1} … » repérés dans le code (« Scène {1} copiée dans la scène {2} ») ; chaque
//      morceau variable est traduit à son tour ;
//   4. les noms composés « A · B · C » (réglages des modules), morceau par morceau ;
//   5. sinon le texte reste tel quel.
// Rien ne passe par le fil audio ; les résultats sont gardés en mémoire (les valeurs qui changent sans
// cesse, « 4.67 kHz », ne coûtent qu'une recherche).

import en from "./en.json";

export type Lang = "fr" | "en";
const DICT = en as Record<string, string>;

/** Langue choisie (Réglages), sinon celle du système : français si le système est en français, anglais sinon. */
export function currentLang(): Lang {
  try {
    const v = window.localStorage.getItem("groovebox.lang");
    if (v === "fr" || v === "en") return v;
  } catch { /* stockage indisponible */ }
  const nav = (typeof navigator !== "undefined" && (navigator.languages?.[0] ?? navigator.language)) || "fr";
  return nav.toLowerCase().startsWith("fr") ? "fr" : "en";
}

/** Change de langue : gardée sur cet appareil, l'interface se recharge (le projet en cours reste dans le moteur). */
export function setLang(l: Lang) {
  try { window.localStorage.setItem("groovebox.lang", l); } catch { /* stockage indisponible */ }
  window.location.reload();
}

// --- modèles « … {1} … » -> expressions régulières (les plus longs d'abord : les plus précis) -------------
type Pat = { re: RegExp; out: string };
const PATS: Pat[] = Object.keys(DICT)
  .filter((k) => /\{\d\}/.test(k))
  .sort((a, b) => b.replace(/\{\d\}/g, "").length - a.replace(/\{\d\}/g, "").length)
  .map((k) => {
    const src = k.split(/(\{\d\})/).map((p) => (/^\{\d\}$/.test(p) ? "([\\s\\S]*?)" : p.replace(/[.*+?^${}()|[\]\\]/g, "\\$&").replace(/#/g, "(-?\\d+(?:[.,]\\d+)?)"))).join("");
    return { re: new RegExp(`^${src}$`), out: DICT[k] };
  });

const NUM = /-?\d+(?:[.,]\d+)?/g;
const cache = new Map<string, string>();

function lookup(s: string, depth: number): string {
  if (!s || depth > 4) return s;
  const hit = DICT[s];
  if (hit !== undefined) return hit;
  // nombres -> « # »
  if (/\d/.test(s)) {
    const nums = s.match(NUM) ?? [];
    const k = s.replace(NUM, "#");
    const t = DICT[k];
    if (t !== undefined) { let i = 0; return t.replace(/#/g, () => nums[i++] ?? "#"); }
  }
  // modèles du code
  if (s.length < 220) {
    for (const p of PATS) {
      const m = p.re.exec(s);
      if (!m) continue;
      return p.out.replace(/\{(\d)\}/g, (_, n: string) => translate(m[Number(n)] ?? "", depth + 1));
    }
  }
  // morceau de texte qui commence ou finit par un séparateur (« Machine électronique · »)
  const sep = /^([·:,–—-]\s*)?([\s\S]*?)(\s*[·:,–—-])?$/.exec(s);
  if (sep && (sep[1] || sep[3]) && sep[2]) {
    const inner = translate(sep[2], depth + 1);
    if (inner !== sep[2]) return (sep[1] ?? "") + inner + (sep[3] ?? "");
  }
  // noms composés « A · B »
  if (s.includes(" · ")) {
    const parts = s.split(" · ");
    const out = parts.map((p) => translate(p, depth + 1));
    if (out.some((o, i) => o !== parts[i])) return out.join(" · ");
  }
  return s;
}

/** Traduit un texte affiché (espaces de début et de fin gardés). */
export function translate(text: string, depth = 0): string {
  const m = /^(\s*)([\s\S]*?)(\s*)$/.exec(text);
  if (!m || !m[2] || !/[A-Za-zÀ-ÿ]/.test(m[2])) return text;
  const core = m[2].replace(/\s+/g, " ");
  let t = cache.get(core);
  if (t === undefined) {
    t = lookup(core, depth);
    if (cache.size > 20000) cache.clear();
    cache.set(core, t);
  }
  return t === core ? text : m[1] + t + m[3];
}

// --- observateur de la page -----------------------------------------------------------------------------
const ATTRS = ["title", "aria-label", "placeholder", "alt", "data-ctl-label"];
const written = new WeakMap<Node, string>(); // dernier texte écrit par nous : ne pas le retraduire

function doText(n: Text) {
  const v = n.nodeValue ?? "";
  if (written.get(n) === v) return;
  const p = n.parentNode as Element | null;
  if (p && (p.nodeName === "SCRIPT" || p.nodeName === "STYLE" || (p as HTMLElement).isContentEditable || p.closest?.('[translate="no"]'))) return;
  const t = translate(v);
  if (t !== v) { written.set(n, t); n.nodeValue = t; }
}
function doAttrs(el: Element) {
  if (el.closest('[translate="no"]')) return;
  for (const a of ATTRS) {
    const v = el.getAttribute(a);
    if (!v) continue;
    if ((el as unknown as { __i18n?: Record<string, string> }).__i18n?.[a] === v) continue;
    const t = translate(v);
    if (t !== v) {
      const store = ((el as unknown as { __i18n?: Record<string, string> }).__i18n ??= {});
      store[a] = t;
      el.setAttribute(a, t);
    }
  }
}
function doTree(root: Node) {
  if (root.nodeType === Node.TEXT_NODE) { doText(root as Text); return; }
  if (root.nodeType !== Node.ELEMENT_NODE) return;
  doAttrs(root as Element);
  const w = document.createTreeWalker(root, NodeFilter.SHOW_TEXT | NodeFilter.SHOW_ELEMENT);
  for (let n = w.nextNode(); n; n = w.nextNode()) {
    if (n.nodeType === Node.TEXT_NODE) doText(n as Text);
    else doAttrs(n as Element);
  }
}

/** Lance la traduction de la page si la langue est l'anglais.
    v0.46 fix30i : la langue est celle que l'application a donnée (main.tsx, lang.ts). Elle était relue ici dans
    la mémoire de la page (localStorage), que la WebView d'Android ne garde pas : on retombait sur la langue du
    téléphone (français) et l'interface restait en français même après avoir choisi English. */
export function startI18n(chosen?: Lang) {
  const lang = chosen ?? currentLang();
  document.documentElement.lang = lang;
  if (lang === "fr") return;
  doTree(document.body);
  new MutationObserver((list) => {
    for (const m of list) {
      if (m.type === "characterData") doText(m.target as Text);
      else if (m.type === "attributes") doAttrs(m.target as Element);
      else m.addedNodes.forEach(doTree);
    }
  }).observe(document.body, { subtree: true, childList: true, characterData: true, attributes: true, attributeFilter: ATTRS });
}
