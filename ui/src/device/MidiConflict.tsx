// Groovebox — OFAD — AGPL-3.0-or-later
// Apprentissage MIDI sur une commande déjà affectée : on demande avant de remplacer.

import { actions, useStore } from "../engine/store";
import { midiSource, targetName } from "../screens/SettingsScreen";

export function MidiConflict() {
  const c = useStore((s) => s.midi.conflict);
  if (!c) return null;
  const [kind, ch, num, oldT, newT] = c;
  return (
    <div className="midi-conflict" role="alertdialog" aria-label="Commande MIDI déjà affectée">
      <p><b>{midiSource(kind, ch, num)}</b> est déjà affecté à <b>{targetName(oldT)}</b>. Le remplacer par <b>{targetName(newT)}</b> ?</p>
      <div>
        <button type="button" className="touch touch--small touch--on" onClick={() => void actions.midiResolve(true)}>Remplacer</button>
        <button type="button" className="touch touch--small touch--quiet" onClick={() => void actions.midiResolve(false)}>Garder l'ancienne</button>
      </div>
    </div>
  );
}
