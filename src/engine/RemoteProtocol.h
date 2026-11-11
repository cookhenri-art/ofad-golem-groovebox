// Groovebox — OFAD — AGPL-3.0-or-later
// 1.1 : télécommande — le téléphone pilote GOLEM sur le PC par MIDI (câble USB, et plus tard Wi-Fi /
// Bluetooth). Messages exclusifs (SysEx) « non commerciaux » (7D), propres à GOLEM :
//   F0 7D 47 4C <version> <commande> <données…> F7          (47 4C = « GL »)
// Commande 01 « régler » : une ou plusieurs cibles de 5 octets chacune :
//   cible (entier signé + 2^20, 3 × 7 bits, poids fort d'abord) puis valeur 0..1 sur 14 bits (2 × 7 bits).
// Les cibles sont celles de l'apprentissage MIDI (réglage ≥ 0, coupure, solo, boutons assignables,
// réglages globaux, lecture…), appliquées en valeur absolue : la télécommande reste en phase.
// Commande 02 « bonjour » : la télécommande se présente ; octet facultatif 1 = « renvoie tout ».
// 1.1 — retour du PC vers la télécommande (application autonome) :
// Commande 03 « état » : mêmes groupes de 5 octets que « régler » (valeurs actuelles du PC).
// Commande 04 « nom » : piste, couleur (127 = piste libre), puis le nom en UTF-8, chaque octet en deux moitiés de 4 bits.
// Commande 05 « je suis là » (Wi-Fi, src/WifiRemote.h).
// Commande 06 « bouton » : bouton assignable, couleur, libellé (moitiés de 4 bits), 7F, piste ou zone.
// Commande 07 « niveaux » : crêtes des pistes, du principal, des retours et des groupes, 0..127.
// Commande 08 « MIDI » (Wi-Fi) : un message court (note, CC, molette, programme), chaque octet en deux moitiés.
// 1.1 : dans « état », deux cibles propres au retour (jamais réglables) : la position jouée du pavé vectoriel
// (avec le mouvement), envoyée toutes les 40 ms pour que l'orbe de la télécommande bouge comme sur le PC.

#pragma once

#include <cstdint>
#include <cstddef>

namespace remote
{
    constexpr uint8_t manufacturer = 0x7D, id0 = 0x47, id1 = 0x4C, version = 1;
    constexpr uint8_t cmdSet = 0x01, cmdHello = 0x02, cmdState = 0x03, cmdName = 0x04, cmdKnob = 0x06, cmdLevels = 0x07, cmdMidi = 0x08, cmdPreset = 0x09;
    constexpr int targetBias = 1 << 20;
    constexpr int vecShownX = -690, vecShownY = -691; // retour seulement : position jouée du pavé (0..1)

    /** Données d'un SysEx (sans F0 ni F7) : est-ce un message de la télécommande GOLEM ? */
    inline bool isRemote (const uint8_t* d, size_t n) noexcept
    {
        return n >= 5 && d[0] == manufacturer && d[1] == id0 && d[2] == id1 && d[3] == version;
    }

    /** Parcourt les cibles d'une commande « régler » ; f (cible, valeur 0..1). */
    template <typename F>
    inline void forEachSet (const uint8_t* d, size_t n, F&& f)
    {
        if (! isRemote (d, n) || d[4] != cmdSet) return;
        for (size_t i = 5; i + 5 <= n; i += 5)
        {
            const int raw = ((d[i] & 0x7F) << 14) | ((d[i + 1] & 0x7F) << 7) | (d[i + 2] & 0x7F);
            const int target = raw - targetBias;
            const int v14 = ((d[i + 3] & 0x7F) << 7) | (d[i + 4] & 0x7F);
            f (target, (float) v14 / 16383.0f);
        }
    }

    /** Message « bonjour » ? full : la télécommande demande tout l'état. */
    inline bool isHello (const uint8_t* d, size_t n, bool& full) noexcept
    {
        if (! isRemote (d, n) || d[4] != cmdHello) return false;
        full = n > 5 && d[5] == 1;
        return true;
    }

    /** Ajoute une cible (5 octets) à un message « régler » ou « état ». */
    inline void putTarget (uint8_t* o, int target, float v) noexcept
    {
        const int raw = target + targetBias;
        const int v14 = (int) (v < 0.0f ? 0.0f : v > 1.0f ? 16383.0f : v * 16383.0f + 0.5f);
        o[0] = (uint8_t) ((raw >> 14) & 0x7F); o[1] = (uint8_t) ((raw >> 7) & 0x7F); o[2] = (uint8_t) (raw & 0x7F);
        o[3] = (uint8_t) ((v14 >> 7) & 0x7F); o[4] = (uint8_t) (v14 & 0x7F);
    }

    /** Message du PC vers la télécommande (état, nom, bouton, niveaux, préréglages) ? */
    inline bool isFeedback (const uint8_t* d, size_t n) noexcept
    {
        return isRemote (d, n) && (d[4] == cmdState || d[4] == cmdName || d[4] == cmdKnob || d[4] == cmdLevels || d[4] == cmdPreset);
    }

    /** Message « MIDI » (Wi-Fi) : renvoie la longueur (1 à 3) et remplit out, 0 sinon. */
    inline int decodeMidi (const uint8_t* d, size_t n, uint8_t* out) noexcept
    {
        if (! isRemote (d, n) || d[4] != cmdMidi || n < 7 || n > 11 || ((n - 5) & 1) != 0) return 0;
        const int len = (int) (n - 5) / 2;
        for (int i = 0; i < len; ++i) out[i] = (uint8_t) (((d[5 + 2 * i] & 15) << 4) | (d[6 + 2 * i] & 15));
        if ((out[0] & 0x80) == 0 || out[0] >= 0xF0) return 0; // messages de canal seulement
        return len;
    }
}
