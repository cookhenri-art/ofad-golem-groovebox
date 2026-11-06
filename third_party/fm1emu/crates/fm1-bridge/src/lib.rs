//! Groovebox — OFAD — pont C entre le moteur de GOLEM et l'émulateur du FM-1.
//!
//! Adapté du fil d'émulation du greffon de fm1-emu (Joakim Eriksson, `crates/fm1-plugin/src/engine.rs`,
//! licence MIT) : le FM-1 émulé tourne dans un fil à lui et garde, dans un anneau, `lead` images d'avance
//! (44,1 kHz, stéréo) sur ce que GOLEM a pris. Les notes MIDI de la piste lui arrivent marquées de l'image
//! où elles doivent sonner (`lead` images après celle que l'audio jouait) : l'écart entre les notes est
//! gardé et le retard est constant (GOLEM le connaît).
//!
//! Règles temps réel : `fm1_midi` et `fm1_pull` sont appelées par le seul fil audio (un producteur, un
//! consommateur : anneaux sans verrou, aucune allocation) ; tout le reste vient du fil de l'interface.

use fm1_core::{Bus, Cpu};
use fm1_soc::{machine, Action, Board, Soc};
use std::cell::UnsafeCell;
use std::collections::VecDeque;
use std::sync::atomic::{AtomicBool, AtomicU64, AtomicUsize, Ordering};
use std::sync::{mpsc, Arc, Mutex};
use std::time::{Duration, Instant};

const RAM_BASE: u32 = 0x01c0_0000;
const RAM_SIZE: usize = 1 << 20;
/// Fréquence de sortie du firmware.
pub const RATE: f64 = 44_100.0;
const SECTOR: usize = 4096;
/// Côté de l'écran (240 × 240, RGB565).
pub const LCD_W: usize = 240;
pub const LCD_H: usize = 240;
pub const NLEDS: usize = 41;
/// Taille maximale d'un paquet de firmware accepté (le paquet d'origine fait 0,7 Mo).
const MAX_PKG: usize = 16 << 20;

// --- anneau sans verrou, un producteur et un consommateur ------------------------------------------

struct Ring<T: Copy + Default> {
    buf: Box<[UnsafeCell<T>]>,
    head: AtomicUsize, // écrits (producteur)
    tail: AtomicUsize, // lus (consommateur)
}
unsafe impl<T: Copy + Default> Sync for Ring<T> {}
unsafe impl<T: Copy + Default> Send for Ring<T> {}

impl<T: Copy + Default> Ring<T> {
    fn new(cap: usize) -> Self {
        Ring { buf: (0..cap).map(|_| UnsafeCell::new(T::default())).collect(), head: AtomicUsize::new(0), tail: AtomicUsize::new(0) }
    }
    fn len(&self) -> usize { self.head.load(Ordering::Acquire).wrapping_sub(self.tail.load(Ordering::Acquire)) }
    fn push(&self, v: T) -> bool {
        let h = self.head.load(Ordering::Relaxed);
        if h.wrapping_sub(self.tail.load(Ordering::Acquire)) >= self.buf.len() { return false; }
        unsafe { *self.buf[h % self.buf.len()].get() = v; }
        self.head.store(h.wrapping_add(1), Ordering::Release);
        true
    }
    fn pop(&self) -> Option<T> {
        let t = self.tail.load(Ordering::Relaxed);
        if t == self.head.load(Ordering::Acquire) { return None; }
        let v = unsafe { *self.buf[t % self.buf.len()].get() };
        self.tail.store(t.wrapping_add(1), Ordering::Release);
        Some(v)
    }
}

#[derive(Clone, Copy, Default)]
struct Midi { frame: u64, bytes: [u8; 3], len: u8 }
/// `Midi::len` d'une touche du boîtier (bytes : id, enfoncée, force) passée par l'anneau horodaté.
const KEY_EVENT: u8 = 0x80;
// GOLEM 1.0 : boutons rotatifs et potentiomètres pilotés par un réglage (contrôleur MIDI, automation),
// passés par la même file que les notes (fil audio, sans verrou)
const ENC_EVENT: u8 = 0x81;
const ADC_EVENT: u8 = 0x82;

enum Cmd {
    Key { id: u8, down: bool },
    Enc { enc: u8, clicks: i32 },
    Adc { ch: u8, value: u16 },
    KeyVel(u8),
}

struct View { lcd: Vec<u16>, leds: [f32; NLEDS], booted: bool, halted: bool, speed: f32, board: Board }

struct Shared {
    quit: AtomicBool,
    /// GOLEM met la machine en pause (piste qui ne joue rien) : le fil d'émulation dort, rien n'avance.
    paused: AtomicBool,
    lead: AtomicUsize,
    view: Mutex<View>,
    sectors: Mutex<Vec<(u32, Vec<u8>)>>,
    starved: AtomicU64,
}

struct Machine { cpu: Cpu, cpu1: Option<Cpu>, soc: Soc, pending_tick: bool, halted: bool, flash0: Vec<u8> }

impl Machine {
    fn boot(pkg: &[u8], sectors: &[(u32, Vec<u8>)], mhz: u64) -> Result<Self, String> {
        let st = std::panic::catch_unwind(|| fm1_soc::stock_from_package(pkg, None))
            .map_err(|_| "paquet de firmware illisible".to_owned())??;
        let mut soc = Soc::new(RAM_SIZE, st.xip);
        soc.periph.hz = mhz * 1_000_000;
        soc.periph.skip_idle = true;
        soc.lcd_timed = false;
        let mut cpu = Cpu::new(st.entry);
        cpu.jit_on = true;
        cpu.user = true;
        let params = RAM_BASE + 0x7fc00;
        soc.write32(params, params + 0x40);
        cpu.regs[0] = fm1_soc::stock_boot(&mut soc, &st.flash, params, st.board);
        let flash0 = soc.flash.mem.clone();
        for (at, data) in sectors {
            let at = *at as usize;
            if at % SECTOR != 0 || data.len() != SECTOR || at + SECTOR > soc.flash.mem.len() { continue; }
            soc.flash.mem[at..at + SECTOR].copy_from_slice(data);
            let from = at.max(fm1_soc::XIP_FLASH_OFFSET);
            let end = (at + SECTOR).min(fm1_soc::XIP_FLASH_OFFSET + soc.xip.len());
            if from < end {
                soc.xip[from - fm1_soc::XIP_FLASH_OFFSET..end - fm1_soc::XIP_FLASH_OFFSET].copy_from_slice(&data[from - at..end - at]);
            }
        }
        Ok(Machine { cpu, cpu1: None, soc, pending_tick: false, halted: false, flash0 })
    }
    fn run(&mut self, n: u32) {
        let jit = self.cpu.jit_on;
        let Machine { cpu, cpu1, soc, pending_tick, .. } = self;
        if machine::run_dual(cpu1, soc, n, RAM_SIZE, jit, |soc, k| machine::run_blocks(cpu, soc, k, pending_tick)).is_err() {
            self.halted = true;
        }
    }
    fn changed_sectors(&self) -> Vec<(u32, Vec<u8>)> {
        let (now, then) = (&self.soc.flash.mem, &self.flash0);
        (0..now.len().min(then.len()) / SECTOR)
            .filter(|s| now[s * SECTOR..(s + 1) * SECTOR] != then[s * SECTOR..(s + 1) * SECTOR])
            .map(|s| ((s * SECTOR) as u32, now[s * SECTOR..(s + 1) * SECTOR].to_vec()))
            .collect()
    }
}

fn run_thread(mut m: Machine, shared: Arc<Shared>, cmds: mpsc::Receiver<Cmd>, midi: Arc<Ring<Midi>>, out: Arc<Ring<[f32; 2]>>) {
    let mut made: u64 = 0;
    let mut due: VecDeque<Midi> = VecDeque::with_capacity(256);
    let mut last_take = Instant::now();
    let mut free: Option<(Instant, u64)> = None;
    let (mut last_view, mut last_flash) = (Instant::now(), Instant::now());
    let (mut speed_t, mut speed_ms, mut busy) = (Instant::now(), 0u64, Duration::ZERO);
    if m.soc.board == Board::Fm1 { m.soc.periph.apply(&Action::Adc { ch: 4, value: 1023 }); } // MASTER à fond
    while !shared.quit.load(Ordering::Relaxed) {
        while let Ok(c) = cmds.try_recv() {
            let a = match c {
                Cmd::Key { id, down } => Action::Key { id, down },
                Cmd::Enc { enc, clicks } => Action::Enc { enc, clicks },
                Cmd::Adc { ch, value } => Action::Adc { ch, value: value.min(1023) },
                Cmd::KeyVel(v) => Action::KeyVel { vel: v.clamp(1, 127) },
            };
            m.soc.periph.apply(&a);
        }
        if shared.paused.load(Ordering::Relaxed) {
            // en pause : le temps du FM-1 s'arrête (rien n'est émulé, rien n'est jeté)
            std::thread::sleep(Duration::from_millis(5));
            last_take = Instant::now();
            free = None;
            continue;
        }
        while let Some(e) = midi.pop() { due.push_back(e); }
        let lead = shared.lead.load(Ordering::Relaxed).clamp(256, out.buf.len() - 64);
        if m.halted {
            // firmware arrêté : du silence, pour que les horloges restent ensemble
            while out.len() < lead && out.push([0.0, 0.0]) { made += 1; }
            let clock = made + shared.starved.load(Ordering::Relaxed);
            due.retain(|e| e.frame > clock);
            std::thread::sleep(Duration::from_millis(2));
            continue;
        }
        let fill = out.len();
        if fill < lead { last_take = Instant::now(); free = None; }
        if fill >= lead && last_take.elapsed() >= Duration::from_millis(50) {
            // GOLEM ne prend plus d'audio (piste arrêtée…) : le FM-1 continue en temps réel, son son est jeté
            let (t0, ms0) = *free.get_or_insert((Instant::now(), m.soc.periph.ms()));
            if m.soc.periph.ms() - ms0 < t0.elapsed().as_millis() as u64 {
                let t1 = Instant::now();
                m.run(20_000);
                busy += t1.elapsed();
                m.soc.periph.audio.clear();
            } else {
                std::thread::sleep(Duration::from_millis(1));
            }
        } else if fill >= lead {
            std::thread::sleep(Duration::from_micros(500));
        } else {
            // GOLEM 0.49 : horloge des notes = images faites + images manquées par l'audio (prises en
            // silence quand l'émulation n'a pas suivi) ; sans elles, chaque manque retardait les notes
            // pour de bon (des dizaines de secondes sur un téléphone)
            let clock = made + shared.starved.load(Ordering::Relaxed);
            while due.front().is_some_and(|e| e.frame <= clock) {
                let e = due.pop_front().unwrap();
                if e.len == ENC_EVENT {
                    m.soc.periph.apply(&Action::Enc { enc: e.bytes[0], clicks: e.bytes[1] as i8 as i32 });
                } else if e.len == ADC_EVENT {
                    m.soc.periph.apply(&Action::Adc { ch: e.bytes[0], value: (((e.bytes[1] as u16) << 8) | e.bytes[2] as u16).min(1023) });
                } else if e.len == KEY_EVENT {
                    // touche du boîtier jouée par GOLEM (fm1_key_at) : force de frappe, puis la touche
                    if e.bytes[1] != 0 { m.soc.periph.apply(&Action::KeyVel { vel: e.bytes[2].clamp(1, 127) }); }
                    m.soc.periph.apply(&Action::Key { id: e.bytes[0], down: e.bytes[1] != 0 });
                } else {
                    for &b in &e.bytes[..e.len.min(3) as usize] { m.soc.periph.apply(&Action::Midi { b }); }
                }
            }
            let t0 = Instant::now();
            m.run(20_000);
            busy += t0.elapsed();
            let a = std::mem::take(&mut m.soc.periph.audio);
            for f in a.chunks_exact(2) {
                if out.push([f[0] as f32 / 32768.0, f[1] as f32 / 32768.0]) { made += 1; }
            }
        }
        if last_view.elapsed() >= Duration::from_millis(30) {
            last_view = Instant::now();
            if let Ok(mut v) = shared.view.lock() {
                v.lcd.copy_from_slice(&m.soc.lcd.fb);
                v.leds = if m.soc.board == Board::Fm1 { m.soc.periph.led_take() } else { [0.0; NLEDS] };
                v.halted = m.halted;
                v.booted = true;
                v.board = m.soc.board;
                if speed_t.elapsed() >= Duration::from_secs(1) {
                    let ms = m.soc.periph.ms();
                    if speed_ms > 0 && busy > Duration::ZERO {
                        v.speed = ms.saturating_sub(speed_ms) as f32 / busy.as_secs_f32() / 1000.0;
                    }
                    (speed_t, speed_ms, busy) = (Instant::now(), ms.max(1), Duration::ZERO);
                }
            }
        }
        if last_flash.elapsed() >= Duration::from_secs(2) {
            last_flash = Instant::now();
            let now = m.changed_sectors();
            if let Ok(mut s) = shared.sectors.lock() { if *s != now { *s = now; } }
        }
    }
}

/// Le FM-1 émulé, vu de GOLEM.
pub struct Fm1 {
    shared: Arc<Shared>,
    cmds: Mutex<mpsc::Sender<Cmd>>,
    midi: Arc<Ring<Midi>>,
    audio: Arc<Ring<[f32; 2]>>,
    taken: AtomicU64,
    thread: Option<std::thread::JoinHandle<()>>,
}

impl Fm1 {
    /// Démarre un paquet (.fwsc), avec les secteurs de mémoire flash gardés d'une séance précédente.
    pub fn new(pkg: &[u8], sectors: Vec<(u32, Vec<u8>)>, mhz: u64) -> Result<Fm1, String> {
        if pkg.is_empty() || pkg.len() > MAX_PKG { return Err("paquet de firmware vide ou trop gros".into()); }
        let m = Machine::boot(pkg, &sectors, mhz.clamp(24, 240))?;
        if m.soc.board != Board::Fm1 {
            return Err("firmware fait pour un autre bo\u{ee}tier (clavier SMK-37) : non pris en charge".into());
        }
        let shared = Arc::new(Shared {
            quit: AtomicBool::new(false),
            paused: AtomicBool::new(false),
            lead: AtomicUsize::new(2048),
            view: Mutex::new(View { lcd: vec![0; LCD_W * LCD_H], leds: [0.0; NLEDS], booted: false, halted: false, speed: 0.0, board: m.soc.board }),
            sectors: Mutex::new(sectors),
            starved: AtomicU64::new(0),
        });
        let (tx, rx) = mpsc::channel();
        let midi = Arc::new(Ring::new(1024));
        let audio = Arc::new(Ring::new(16384));
        let (s2, m2, a2, s3) = (shared.clone(), midi.clone(), audio.clone(), shared.clone());
        // une erreur de l'émulateur (firmware imprévu) arrête cette machine seulement, jamais GOLEM
        let thread = std::thread::Builder::new().name("fm1-emu".into()).spawn(move || {
            let r = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| run_thread(m, s2, rx, m2, a2)));
            if r.is_err() { if let Ok(mut v) = s3.view.lock() { v.halted = true; v.booted = true; } }
        })
            .map_err(|e| e.to_string())?;
        Ok(Fm1 { shared, cmds: Mutex::new(tx), midi, audio, taken: AtomicU64::new(0), thread: Some(thread) })
    }
    fn send(&self, c: Cmd) { if let Ok(tx) = self.cmds.lock() { let _ = tx.send(c); } }
}

impl Drop for Fm1 {
    fn drop(&mut self) {
        self.shared.quit.store(true, Ordering::Relaxed);
        if let Some(t) = self.thread.take() { let _ = t.join(); }
    }
}

// --- interface C ------------------------------------------------------------------------------------

static LAST_ERROR: Mutex<String> = Mutex::new(String::new());

fn sectors_from_blob(blob: &[u8]) -> Vec<(u32, Vec<u8>)> {
    // [u32 adresse][u32 taille][octets] répétés (petit-boutiste) ; ce qui est mal formé est ignoré
    let mut out = Vec::new();
    let mut i = 0usize;
    while i + 8 <= blob.len() && out.len() < 4096 {
        let at = u32::from_le_bytes(blob[i..i + 4].try_into().unwrap());
        let n = u32::from_le_bytes(blob[i + 4..i + 8].try_into().unwrap()) as usize;
        i += 8;
        if n != SECTOR || i + n > blob.len() { break; }
        out.push((at, blob[i..i + n].to_vec()));
        i += n;
    }
    out
}

/// Démarre un firmware. Renvoie NULL en cas d'échec (`fm1_last_error` dit pourquoi).
/// # Safety
/// `pkg` (et `sectors`, s'il n'est pas nul) doivent pointer sur `pkg_len` (`sectors_len`) octets lisibles.
#[no_mangle]
pub unsafe extern "C" fn fm1_new(pkg: *const u8, pkg_len: usize, sectors: *const u8, sectors_len: usize, mhz: u32) -> *mut Fm1 {
    if pkg.is_null() || pkg_len == 0 { return std::ptr::null_mut(); }
    let pkg = std::slice::from_raw_parts(pkg, pkg_len);
    let sec = if sectors.is_null() || sectors_len == 0 { Vec::new() } else { sectors_from_blob(std::slice::from_raw_parts(sectors, sectors_len)) };
    match std::panic::catch_unwind(|| Fm1::new(pkg, sec, mhz as u64)) {
        Ok(Ok(f)) => Box::into_raw(Box::new(f)),
        Ok(Err(e)) => { if let Ok(mut l) = LAST_ERROR.lock() { *l = e; } std::ptr::null_mut() }
        Err(_) => { if let Ok(mut l) = LAST_ERROR.lock() { *l = "le firmware a fait tomber l'émulateur".into(); } std::ptr::null_mut() }
    }
}

/// Dernière erreur de `fm1_new`, copiée (terminée par zéro) dans `out` ; renvoie sa longueur.
/// # Safety
/// `out` doit pointer sur `cap` octets inscriptibles.
#[no_mangle]
pub unsafe extern "C" fn fm1_last_error(out: *mut u8, cap: usize) -> usize {
    let e = LAST_ERROR.lock().map(|l| l.clone()).unwrap_or_default();
    if !out.is_null() && cap > 0 {
        let n = e.len().min(cap - 1);
        std::ptr::copy_nonoverlapping(e.as_ptr(), out, n);
        *out.add(n) = 0;
    }
    e.len()
}

/// Arrête le firmware et libère tout.
/// # Safety
/// `f` vient de `fm1_new` et n'est plus utilisé ensuite.
#[no_mangle]
pub unsafe extern "C" fn fm1_free(f: *mut Fm1) { if !f.is_null() { drop(Box::from_raw(f)); } }

/// Avance gardée par le fil d'émulation, en images à 44,1 kHz (256 à 16 000).
/// # Safety
/// `f` vient de `fm1_new`.
#[no_mangle]
pub unsafe extern "C" fn fm1_set_lead(f: *mut Fm1, frames: u32) { if let Some(f) = f.as_ref() { f.shared.lead.store(frames as usize, Ordering::Relaxed); } }

/// Met la machine en pause (true) ou la relance : en pause, le fil d'émulation dort (aucun calcul).
/// # Safety
/// `f` vient de `fm1_new`.
#[no_mangle]
pub unsafe extern "C" fn fm1_set_paused(f: *mut Fm1, paused: bool) { if let Some(f) = f.as_ref() { f.shared.paused.store(paused, Ordering::Relaxed); } }

/// Un message MIDI (1 à 3 octets), joué `lead` images après l'image que l'audio prend en ce moment.
/// Fil audio seulement.
/// # Safety
/// `f` vient de `fm1_new`.
#[no_mangle]
pub unsafe extern "C" fn fm1_midi(f: *mut Fm1, b0: u8, b1: u8, b2: u8, len: u8) {
    if let Some(f) = f.as_ref() {
        let frame = f.taken.load(Ordering::Relaxed) + f.shared.lead.load(Ordering::Relaxed) as u64;
        let _ = f.midi.push(Midi { frame, bytes: [b0, b1, b2], len: len.min(3) });
    }
}

/// Prend `frames` images stéréo entrelacées (44,1 kHz) ; ce qui manque est rempli de silence.
/// Renvoie le nombre d'images réellement fournies par le FM-1. Fil audio seulement.
/// # Safety
/// `f` vient de `fm1_new` ; `out` pointe sur `2 * frames` flottants.
#[no_mangle]
pub unsafe extern "C" fn fm1_pull(f: *mut Fm1, out: *mut f32, frames: u32) -> u32 {
    let Some(f) = f.as_ref() else { return 0 };
    if out.is_null() { return 0; }
    let o = std::slice::from_raw_parts_mut(out, frames as usize * 2);
    let mut got = 0u32;
    for k in 0..frames as usize {
        match f.audio.pop() {
            Some([l, r]) => { o[2 * k] = l; o[2 * k + 1] = r; got += 1; }
            None => { o[2 * k] = 0.0; o[2 * k + 1] = 0.0; }
        }
    }
    f.taken.fetch_add(frames as u64, Ordering::Relaxed);
    if got < frames { f.shared.starved.fetch_add((frames - got) as u64, Ordering::Relaxed); }
    got
}

/// Une touche du FM-1 (numéro du firmware), enfoncée ou relâchée.
/// # Safety
/// `f` vient de `fm1_new`.
#[no_mangle]
pub unsafe extern "C" fn fm1_key(f: *mut Fm1, id: u8, down: bool) { if let Some(f) = f.as_ref() { f.send(Cmd::Key { id, down }); } }

/// Une touche du boîtier (0..40), enfoncée ou relâchée, avec sa force (1..127), jouée comme une note
/// MIDI : `lead` images après l'image que l'audio prend en ce moment. Fil audio seulement (sans
/// verrou ni allocation) : pour les firmwares qui ne lisent pas la prise MIDI.
/// # Safety
/// `f` vient de `fm1_new`.
#[no_mangle]
pub unsafe extern "C" fn fm1_key_at(f: *mut Fm1, id: u8, down: bool, vel: u8) {
    if let Some(f) = f.as_ref() {
        let frame = f.taken.load(Ordering::Relaxed) + f.shared.lead.load(Ordering::Relaxed) as u64;
        let _ = f.midi.push(Midi { frame, bytes: [id.min(40), down as u8, vel], len: KEY_EVENT });
    }
}

/// GOLEM 1.0 : un bouton rotatif (0..7) tourné de `clicks` crans, ou une entrée analogique (canal,
/// 0..1023), à l'image des notes : `lead` images plus tard. Fil audio seulement (sans verrou ni allocation).
/// # Safety
/// `f` vient de `fm1_new`.
#[no_mangle]
pub unsafe extern "C" fn fm1_encoder_at(f: *mut Fm1, enc: u8, clicks: i32) {
    if let Some(f) = f.as_ref() {
        let frame = f.taken.load(Ordering::Relaxed) + f.shared.lead.load(Ordering::Relaxed) as u64;
        let _ = f.midi.push(Midi { frame, bytes: [enc.min(7), clicks.clamp(-64, 64) as i8 as u8, 0], len: ENC_EVENT });
    }
}

/// # Safety
/// `f` vient de `fm1_new`.
#[no_mangle]
pub unsafe extern "C" fn fm1_analog_at(f: *mut Fm1, ch: u8, value: u16) {
    if let Some(f) = f.as_ref() {
        let frame = f.taken.load(Ordering::Relaxed) + f.shared.lead.load(Ordering::Relaxed) as u64;
        let v = value.min(1023);
        let _ = f.midi.push(Midi { frame, bytes: [ch.min(22), (v >> 8) as u8, (v & 0xFF) as u8], len: ADC_EVENT });
    }
}

/// Force de frappe des prochaines touches (1 à 127).
/// # Safety
/// `f` vient de `fm1_new`.
#[no_mangle]
pub unsafe extern "C" fn fm1_key_velocity(f: *mut Fm1, vel: u8) { if let Some(f) = f.as_ref() { f.send(Cmd::KeyVel(vel)); } }

/// Un bouton rotatif tourné de `clicks` crans (+ : sens horaire).
/// # Safety
/// `f` vient de `fm1_new`.
#[no_mangle]
pub unsafe extern "C" fn fm1_encoder(f: *mut Fm1, enc: u8, clicks: i32) { if let Some(f) = f.as_ref() { f.send(Cmd::Enc { enc, clicks }); } }

/// Une entrée analogique (potentiomètre MASTER : canal 4), 0 à 1023.
/// # Safety
/// `f` vient de `fm1_new`.
#[no_mangle]
pub unsafe extern "C" fn fm1_analog(f: *mut Fm1, ch: u8, value: u16) { if let Some(f) = f.as_ref() { f.send(Cmd::Adc { ch, value }); } }

/// Copie l'écran (240 × 240, RGB565) et les diodes ; renvoie bit 0 : démarré, bit 1 : arrêté (faute).
/// # Safety
/// `f` vient de `fm1_new` ; `lcd` (s'il n'est pas nul) pointe sur 57 600 `u16`, `leds` sur 41 `f32`.
#[no_mangle]
pub unsafe extern "C" fn fm1_view(f: *mut Fm1, lcd: *mut u16, leds: *mut f32) -> u32 {
    let Some(f) = f.as_ref() else { return 0 };
    let Ok(v) = f.shared.view.lock() else { return 0 };
    if !lcd.is_null() { std::ptr::copy_nonoverlapping(v.lcd.as_ptr(), lcd, LCD_W * LCD_H); }
    if !leds.is_null() { std::ptr::copy_nonoverlapping(v.leds.as_ptr(), leds, NLEDS); }
    (v.booted as u32) | ((v.halted as u32) << 1)
}

/// Vitesse de l'émulation (temps émulé / temps de calcul ; > 1 : il tient le temps réel).
/// # Safety
/// `f` vient de `fm1_new`.
#[no_mangle]
pub unsafe extern "C" fn fm1_speed(f: *mut Fm1) -> f32 { f.as_ref().and_then(|f| f.shared.view.lock().ok().map(|v| v.speed)).unwrap_or(0.0) }

/// Images manquantes depuis le départ (le FM-1 n'a pas suivi).
/// # Safety
/// `f` vient de `fm1_new`.
#[no_mangle]
pub unsafe extern "C" fn fm1_starved(f: *mut Fm1) -> u64 { f.as_ref().map_or(0, |f| f.shared.starved.load(Ordering::Relaxed)) }

/// Ce que le firmware a écrit dans sa mémoire flash (ses sons, ses réglages), pour le garder dans le
/// projet : copie au plus `cap` octets dans `out` (format de `fm1_new`) et renvoie la taille nécessaire.
/// # Safety
/// `f` vient de `fm1_new` ; `out` (s'il n'est pas nul) pointe sur `cap` octets inscriptibles.
#[no_mangle]
pub unsafe extern "C" fn fm1_sectors(f: *mut Fm1, out: *mut u8, cap: usize) -> usize {
    let Some(f) = f.as_ref() else { return 0 };
    let Ok(s) = f.shared.sectors.lock() else { return 0 };
    let need = s.len() * (8 + SECTOR);
    if !out.is_null() && cap >= need {
        let mut i = 0usize;
        for (at, d) in s.iter() {
            std::ptr::copy_nonoverlapping(at.to_le_bytes().as_ptr(), out.add(i), 4);
            std::ptr::copy_nonoverlapping((d.len() as u32).to_le_bytes().as_ptr(), out.add(i + 4), 4);
            std::ptr::copy_nonoverlapping(d.as_ptr(), out.add(i + 8), d.len());
            i += 8 + d.len();
        }
    }
    need
}

/// Fréquence de sortie du firmware (44 100).
#[no_mangle]
pub extern "C" fn fm1_rate() -> u32 { RATE as u32 }

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn ring_keeps_order_and_bounds() {
        let r: Ring<u32> = Ring::new(4);
        assert!(r.push(1) && r.push(2) && r.push(3) && r.push(4));
        assert!(!r.push(5));
        assert_eq!(r.pop(), Some(1));
        assert!(r.push(5));
        assert_eq!((r.pop(), r.pop(), r.pop(), r.pop(), r.pop()), (Some(2), Some(3), Some(4), Some(5), None));
    }
    #[test]
    fn sector_blob_roundtrip_ignores_garbage() {
        let mut b = Vec::new();
        b.extend_from_slice(&0x74000u32.to_le_bytes());
        b.extend_from_slice(&(SECTOR as u32).to_le_bytes());
        b.extend(std::iter::repeat(7u8).take(SECTOR));
        b.extend_from_slice(&[1, 2, 3]);
        let s = sectors_from_blob(&b);
        assert_eq!(s.len(), 1);
        assert_eq!(s[0].0, 0x74000);
        assert!(sectors_from_blob(&[0xff; 7]).is_empty());
    }
    #[test]
    fn bad_package_is_refused() {
        assert!(Fm1::new(&[0u8; 100], Vec::new(), 240).is_err());
    }
}
