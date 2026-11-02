// Groovebox — OFAD — AGPL-3.0-or-later
// Jeu d'icônes unique (trait 0, formes pleines), dessiné pour l'appareil.

const base = { width: 14, height: 14, viewBox: "0 0 14 14", "aria-hidden": true } as const;

export const IconPlay = () => <svg {...base}><path d="M3 2 L12 7 L3 12 Z" fill="currentColor" /></svg>;
export const IconStop = () => <svg {...base}><rect x="3" y="3" width="8" height="8" rx="1" fill="currentColor" /></svg>;
export const IconRec = () => <svg {...base}><circle cx="7" cy="7" r="4.5" fill="currentColor" /></svg>;
export const IconPrev = () => <svg {...base}><path d="M9.5 2.5 L4.5 7 L9.5 11.5" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" /></svg>;
export const IconNext = () => <svg {...base}><path d="M4.5 2.5 L9.5 7 L4.5 11.5" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" /></svg>;
export const IconLoad = () => <svg {...base}><path d="M7 2 V9 M4 6 L7 9 L10 6 M2.5 11.5 H11.5" fill="none" stroke="currentColor" strokeWidth="1.6" strokeLinecap="round" strokeLinejoin="round" /></svg>;
