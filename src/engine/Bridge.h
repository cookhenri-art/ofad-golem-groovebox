/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Textes JSON échangés avec l'interface. Tout est écrit en ASCII (les caractères accentués en
    \uXXXX) : la passerelle WebView ne voit jamais d'octet non ASCII. Pur C++ (testable seul).
*/

#pragma once

#include "Engine.h"

#include <string>

namespace grv::engine::bridge
{

/** Chaîne JSON entre guillemets, échappée (UTF-8 → \uXXXX). */
std::string quote (const std::string& utf8);

/** Catalogue complet : instruments, effets, réglages de mixage et des sources de modulation. */
std::string catalogJson();

/** Les 16 pistes : nom, couleur, instrument, préréglage ; inserts de tous les canaux. */
std::string tracksJson (const Engine& e);

/** Un canal : valeurs de tous ses réglages (normalisées), modulations, préréglages. */
std::string channelJson (const Engine& e, int channel);

/** Séquence d'une piste : longueur, vitesse, coupure, solo, 16 lignes (pas en hexadécimal). */
std::string patternJson (const Engine& e, int track);

/** Conditions et verrous des pas du pattern édité d'une piste. */
std::string extrasJson (const Engine& e, int track);

/** Spectre de la sortie d'un insert qui a un analyseur (égaliseur) : niveaux en dB par bande logarithmique. */
std::string spectrumJson (const Engine& e, int channel, int slot, int bands);

/** Lancement des patterns : quantification, et pour chaque piste pattern joué, en attente, édité, remplis. */
std::string launchJson (const Engine& e);

/** Les 33 scènes (32 + Break) : coupure et verrous. */
std::string scenesJson (const Engine& e);

/** Forme d'onde réduite d'un échantillon (points min/max) pour l'affichage. */
std::string waveJson (const Engine& e, int track, int index, int points);

} // namespace grv::engine::bridge
