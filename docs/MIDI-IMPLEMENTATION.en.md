# OFAD GOLEM 1.1 — MIDI implementation

This document describes what GOLEM receives over MIDI, how it picks the track that plays, how to call
a sound by its number, what the remote (GOLEM on a phone) sends and what a MIDI track sends to an
external device (§ 8). It complements the manual
("MIDI controller" chapter).

GOLEM follows MIDI conventions like a music application: each track can listen to its own channel
(1 to 16), program change selects the instrument's sound, and bank select (CC 0 / CC 32) moves to the
next 128 sounds.

## 1. Ports

| Link | Windows PC | Mac | Android, iPhone, iPad | Notes |
|---|---|---|---|---|
| USB | Yes | Yes | Yes (USB-C "OTG" cable or hub) | Detected automatically, hot-plug |
| Bluetooth MIDI | If Windows exposes it as a MIDI port (bridge driver) | Yes (system pairing window) | Yes (Settings › Pair a Bluetooth controller…, or Bluetooth… on the MIDI track) | |
| Network (rtpMIDI…) | Through a network driver, seen as a device | Yes (system MIDI network) | — | Shows up in the device list |
| Host software (VST3, AU) | Yes | Yes | — | The host sends MIDI to GOLEM and receives the plug-in's MIDI output |
| Wi-Fi (remote) | Yes (Settings › Accept a Wi-Fi remote) | Yes | Yes | Between two GOLEMs on the same local network, see § 7 |

The MIDI track's "Device" list shows the link type (USB, Bluetooth, network, virtual) when the system
reports it.

## 2. Settings (Settings › MIDI)

| Setting | Values | Default | Role |
|---|---|---|---|
| Notes channel | All, 1 to 16 | All | Channel on which the **selected track** receives notes |
| Commands channel | All, 1 to 16 | All | Channel for learned commands and scene launching |
| MIDI commands | On / off | On | Learned commands, pads in Scenes / Mute / Solo / Steps mode, scenes by program change |
| Track's own channel | None, 1 to 16 | None | The track plays this channel, selected or not |
| T channels (FM Player T1 to T4, Pieuvre 8 T1 to T8) | 1 to 16 | Follow T1's channel | An external sequencer plays several Ts at once |

## 3. Which track receives a message

1. Tracks (and Ts) **set to the message's channel** receive it.
2. Otherwise the **selected track** receives it, if the notes channel accepts it and the track has no
   channel of its own.
3. **Program change**, bank select and CC 102 to 105 only change the sound of a track **set to that
   channel** (rule 1). With no track on the channel, program change launches a scene (§ 5).

## 4. Received messages

| Message | Received | Effect |
|---|---|---|
| Note On / Note Off | Yes | Plays the track, with velocity. Drum kits: notes 36 to 51 = pads 1 to 16. Pieuvre 8 and FM Player: the selected T over all octaves |
| Notes 36 to 51 in Scenes / Mute / Solo / Steps pad mode | Yes | Like the on-screen pads: launch a scene, mute or solo a track, place a step |
| Pitch bend | Yes | Playing track |
| Channel pressure | Yes | Playing track (depending on the instrument) |
| Polyphonic aftertouch | No | |
| CC 1 (mod wheel) | Yes | Playing track, unless learned |
| CC 64 (sustain) | Yes | Playing track, unless learned |
| CC 0 / CC 32 (bank) | Yes | Stored per channel for the next program change (§ 5) |
| CC 102 to 105 | Yes | Sound of the effect in inserts 1 to 4 of the track set to that channel (§ 6) |
| Other CCs | Yes | Learned command (parameter, button, transport, scene…); not learned: sent to the instrument (the FM Player passes it to the firmware) |
| Program change | Yes | Instrument sound (§ 5) or scene |
| MIDI clock, Start / Stop / Continue | No | In host software, GOLEM follows the host's tempo and transport |
| Song position pointer | No | |
| SysEx | Yes | GOLEM remote only (§ 7); other SysEx is ignored. FM Player firmwares receive their own .syx files from the FM Player screen |

### Learning

Long press (right click) on a control › **Learn MIDI**, then move a controller control. Notes and CCs can
be learned on instrument and effect parameters, the values at the bottom of the screens, tempo, swing,
crossfader, scenes A and B, vector pad, macros, the 16 assignable buttons, M / S, rows, patterns,
Photo, Recall and transport. A learned command is no longer sent to the instrument.

## 5. Changing the instrument's sound (bank + program)

**Numbering.** For each instrument:

- no. **1** = "Init";
- then the factory sounds bank by bank, in the order of the presets panel: Acid, Bass, Leads, Pads,
  Plucks, Keyboards, Strings, Brass, Arpeggios, Effects, Classics;
- then **your sounds**, after them (by user bank, then by name).

The number is shown in front of each sound, in the lists, in the presets panel and next to the
instrument's name.

**Message to send** on the track's own channel:

| Sound | Bank (CC 0) | Bank (CC 32) | Program change |
|---|---|---|---|
| no. 1 to 128 | 0 | 0 | no. − 1 (0 to 127) |
| no. 129 to 256 | 1 | 0 | no. − 129 |
| no. 257 to 384 | 2 | 0 | no. − 257 |

- The bank can also be sent with **CC 32 alone** (CC 0 at 0, CC 32 = 1, 2…): GOLEM accepts it. If both
  are non-zero, the bank is CC 0 × 128 + CC 32.
- The bank stays stored per channel until the next CC 0 / CC 32.
- Example: no. 140 = CC 0 at 1, CC 32 at 0, program 11.
- A number with no sound changes nothing.

**Scenes.** If no track is set to the channel, program change 0 to 31 received on the commands channel
(MIDI commands on) launches scene 1 to 32.

## 6. Changing an effect's sound (inserts 1 to 4)

| Command | Insert | Value |
|---|---|---|
| CC 102 | Insert 1 | sound no. − 1 (0 = no. 1) |
| CC 103 | Insert 2 | same |
| CC 104 | Insert 3 | same |
| CC 105 | Insert 4 | same |

- Same numbering as instruments (Init = 1, factory sounds by bank, then your sounds), limited to 128
  sounds per effect.
- Only for a track set to that channel, and if the command is not learned. With no track on the
  channel, these CCs are free commands like any other.
- With no effect in the insert, nothing changes.

## 7. The remote (GOLEM on a phone)

The remote mode of the Android application controls:

- **GOLEM on a PC**: over Wi-Fi (same local network) or USB cable (the phone switches to MIDI mode);
- **any MIDI device**: over USB cable or Bluetooth MIDI.

### With GOLEM (Wi-Fi or USB)

- The screens (Mixer, Buttons, Chaos, FM-1) control GOLEM directly: volumes, mute, solo, returns,
  groups, 16 assignable buttons, crossfader, scenes, transport.
- The PC sends back track names and colours, levels, button values and labels, and each track's sound
  (number and name): the phone follows what you touch on the PC.
- FM-1 tab: keyboard, pads, wheels and notes (Note On / Off, pitch bend, CC 1) on the chosen channel;
  "**Change: instrument / insert 1 to 4 effect**" chooses what the sound changes. For the instrument the
  phone sends bank + program (§ 5); for an insert, CC 102 to 105 (§ 6). Beyond 128 sounds, a set
  selector (1–128, 129–256…) appears.
- Exchanges use GOLEM's own SysEx (undocumented, may change between versions). Over Wi-Fi: UDP port
  47474, local network addresses only, one remote at a time.

### With another device (USB or Bluetooth MIDI)

| Phone control | Sends | Settings |
|---|---|---|
| 16 buttons | CC | Name, channel 1 to 16, CC number |
| 8 pads (FM-1 tab) | Gate CC, toggle CC, or note | Name, mode, channel, CC or note number |
| FM-1 tab knobs | CC | Name, channel, CC number, or link to one of the 16 buttons |
| Chaos (4 corners) | 4 buttons or 4 CCs | Per corner |
| Keyboard, wheels | Note On / Off, pitch bend, CC 1 | Notes channel, octave |
| Sound | Bank (CC 0, CC 32 at 0) then program change | Bank 0 to 127, sound 1 to 128 |
| .syx file | SysEx | Sends a chosen file |

The phone only listens to a device other than GOLEM for **Learn**: touch "Learn" next to a setting, turn the
device's knob (or play the note), the CC (or note) number and the channel are set; nothing is replayed.

## 8. What GOLEM sends: the MIDI track (1.1)

Since 1.1, the **MIDI track** instrument (External MIDI family) plays an external device. Each MIDI track
sends on **its send channel** (1 to 16), to the device chosen in its "Device" list (PC, Mac, phone
application) or to the **plug-in's MIDI output** (host software).

| Message | When |
|---|---|
| Note On / Off | Notes from the sequencer, piano, pads, MIDI keyboard; after the MIDI effects (arpeggiator, chord, scale, note echo, probability, velocity, transposition) |
| CC 0 to 119 | The 32 knobs (each with its own number and channel), whenever the value changes: by hand, step lock, scene, modulation, curve |
| Pad CCs | CC held (127 / 0), CC on / off, CC steps (2 to 8 values); or note |
| Bank (CC 0, then CC 32 = 0) + program change | Bank and Sound lists of the top bar, and **step sound**: sent just before the step's note |
| Pitch bend, CC 1 (wheel), pressure | Follow the MIDI keyboard playing the track |
| Note Off | All notes in progress on stop, mute (M) and when another track is soloed |

- At the same instant, bank and program go out **before** the notes.
- CC numbers and channels are **fixed**: no scene, lock or modulation changes them. CC 120 to 127 (channel
  mode messages: all off, reset…) are never sent.
- Nothing is sent when a project loads (the device's sound is not overwritten), unless "Send at start" is
  ticked: the values go out when playback starts.
- What the device sends back does not replay the track (no MIDI loop).
- GOLEM sends **no MIDI clock, start / stop or SysEx** (apart from the remote).
