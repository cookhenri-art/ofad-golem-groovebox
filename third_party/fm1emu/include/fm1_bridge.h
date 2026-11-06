// Groovebox — OFAD — interface C de l'émulateur FM-1 (third_party/fm1emu/crates/fm1-bridge, MIT).
// fm1_midi et fm1_pull : fil audio seulement (sans verrou ni allocation). Le reste : fil de l'interface.
#pragma once
#include <cstddef>
#include <cstdint>

extern "C" {
typedef struct Fm1 Fm1;
enum { FM1_LCD_W = 240, FM1_LCD_H = 240, FM1_NLEDS = 41 };
Fm1* fm1_new (const uint8_t* pkg, size_t pkgLen, const uint8_t* sectors, size_t sectorsLen, uint32_t mhz);
size_t fm1_last_error (uint8_t* out, size_t cap);
void fm1_free (Fm1* f);
void fm1_set_lead (Fm1* f, uint32_t frames);
/** v0.49 : pause (true) ou relance de l'émulation (fil audio ou interface : simple drapeau). */
void fm1_set_paused (Fm1* f, bool paused);
void fm1_midi (Fm1* f, uint8_t b0, uint8_t b1, uint8_t b2, uint8_t len);
uint32_t fm1_pull (Fm1* f, float* interleaved, uint32_t frames);
void fm1_key (Fm1* f, uint8_t id, bool down);
/** v0.49 : touche du boîtier jouée par le fil audio (horodatée comme le MIDI, sans verrou ni allocation). */
void fm1_key_at (Fm1* f, uint8_t id, bool down, uint8_t vel);
/* GOLEM 1.0 : bouton rotatif ou entrée analogique pilotés par un réglage (fil audio, sans verrou). */
void fm1_encoder_at (Fm1* f, uint8_t enc, int32_t clicks);
void fm1_analog_at (Fm1* f, uint8_t ch, uint16_t value);
void fm1_key_velocity (Fm1* f, uint8_t vel);
void fm1_encoder (Fm1* f, uint8_t enc, int32_t clicks);
void fm1_analog (Fm1* f, uint8_t ch, uint16_t value);
uint32_t fm1_view (Fm1* f, uint16_t* lcd, float* leds);
float fm1_speed (Fm1* f);
uint64_t fm1_starved (Fm1* f);
size_t fm1_sectors (Fm1* f, uint8_t* out, size_t cap);
uint32_t fm1_rate();
}
