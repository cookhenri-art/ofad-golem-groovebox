/*
    Groovebox — OFAD
    Copyright (C) 2026 One For All Distribution (OFAD)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU Affero General Public License for more details.

    ---------------------------------------------------------------------------
    KickVoice — voix de grosse caisse du TEST TECHNIQUE (spike).

    But : valider la chaîne audio de bout en bout (pad React -> moteur C++ ->
    sortie audio), pas encore la fidélité sonore. Le vrai moteur type Rytm
    sera conçu à partir de sa fiche d'architecture (compétence
    modelisation-synthes-analogiques), puis calibré au banc A/B.

    Principe retenu ici, déjà proche des grosses caisses analogiques :
      - oscillateur sinusoïdal à phase continue (pas de clic au redéclenchement) ;
      - enveloppe de hauteur exponentielle (le « punch ») ;
      - enveloppe d'amplitude : attaque très courte, maintien, déclin exponentiel ;
      - transitoire de bruit filtré (le « click ») ;
      - saturation douce avec anti-repliement par antidérivée (ADAA 1er ordre) ;
      - filtre anti-continu en sortie.

    Classe pure C++ (aucune dépendance JUCE), testable, sans allocation.
*/

#pragma once

#include "Common.h"

namespace grv::dsp
{

//==============================================================================
class KickVoice
{
public:
    struct Params
    {
        double tuneHz     = 50.0;  // fondamentale (fin du balayage)
        double decayMs    = 450.0; // temps de déclin à -60 dB
        double holdMs     = 4.0;   // maintien après l'attaque
        double sweepSemis = 24.0;  // hauteur de départ au-dessus de la fondamentale
        double sweepMs    = 22.0;  // constante de temps du balayage de hauteur
        double click      = 0.35;  // niveau du transitoire
        double driveDb    = 6.0;   // gain avant saturation
        double levelDb    = 0.0;   // niveau de sortie
        double shape      = 0.0;   // 0 = sinusoïde, 1 = onde arrondie riche en harmoniques (étage poussé)
    };

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        dcCoef = 1.0 - kTwoPi * 8.0 / sampleRate;               // filtre anti-continu à ~8 Hz
        clickLpCoef = std::exp (-kTwoPi * 6000.0 / sampleRate); // bruit adouci au-dessus de ~6 kHz
        reset();
    }

    void reset() noexcept
    {
        phase = 0.0;
        amp = 0.0;
        pitchEnv = 0.0;
        clickEnv = 0.0;
        clickLp = 0.0;
        tail = lastBody = 0.0;
        dcX = dcY = 0.0;
        holdSamplesLeft = 0;
        stage = Stage::idle;
        shaper.reset();
    }

    void setParams (const Params& p) noexcept { params = p; }

    /** Déclenche la voix. velocity dans [0, 1], transposition en demi-tons (pads chromatiques). */
    /** Accent du prochain coup : 1 = coup normal, jusqu'à 1,8 (attaque plus marquée). */
    double accentAttack = 1.0;

    void trigger (double velocity, double transposeSemis = 0.0) noexcept
    {
        velocityGain = 0.25 + 0.75 * std::clamp (velocity, 0.0, 1.0);
        transpose = std::pow (2.0, transposeSemis / 12.0);

        // 1.1 : CHAQUE coup part du même point (attaque et claquant identiques d'un coup à l'autre,
        // comme l'oscillateur déclenché d'une boîte analogique). Avant, une voix qui sonnait encore
        // (amplitude > 0,03) gardait sa phase : aux tempos où la queue tombe autour de ce seuil,
        // un coup sur deux ou trois avait une autre attaque. La queue du coup précédent est fondue
        // en ~2 ms (le corps repart de sa dernière valeur et la rejoint), sans clic.
        tail = stage == Stage::idle ? 0.0 : lastBody;
        phase = 0.0; clickLp = 0.0;
        noise = WhiteNoise(); // transitoire : la même rafale à chaque coup
        pitchEnv = 1.0 + 0.25 * (accentAttack - 1.0); // accent : départ du balayage un peu plus haut
        clickEnv = accentAttack;                       // et transitoire plus fort (attaque plus marquée)
        holdSamplesLeft = (int) std::lround (params.holdMs * 0.001 * sampleRate);
        stage = Stage::attack;
    }

    bool isActive() const noexcept { return stage != Stage::idle; }

    /** Ajoute (mixe) le rendu de la voix dans out[0..numSamples). */
    void renderAdding (float* out, int numSamples) noexcept
    {
        if (stage == Stage::idle)
            return;

        const auto attackCoef = timeConstantCoef (0.25, sampleRate);
        const auto decayCoef  = decay60Coef (params.decayMs, sampleRate);
        const auto pitchCoef  = timeConstantCoef (params.sweepMs, sampleRate);
        const auto clickCoef  = timeConstantCoef (1.5, sampleRate);
        const auto tailCoef   = timeConstantCoef (0.5, sampleRate);
        const auto drive      = dbToGain (params.driveDb);
        const auto driveNorm  = 1.0 / std::max (1.0e-3, softClip (drive));
        const auto level      = dbToGain (params.levelDb);
        const auto baseHz     = params.tuneHz * transpose;
        const auto nyquistSafe = 0.45 * sampleRate;

        for (int i = 0; i < numSamples; ++i)
        {
            // Enveloppe d'amplitude
            switch (stage)
            {
                case Stage::attack:
                    // Montée vers une cible au-dessus de 1, arrêtée à 1 : courbe RC d'attaque.
                    amp = 1.2 + (amp - 1.2) * attackCoef;
                    if (amp >= 1.0) { amp = 1.0; stage = Stage::hold; }
                    break;

                case Stage::hold:
                    if (--holdSamplesLeft <= 0) stage = Stage::decay;
                    break;

                case Stage::decay:
                    amp *= decayCoef;
                    if (amp < 1.0e-5 && clickEnv < 1.0e-5) { stage = Stage::idle; amp = 0.0; }
                    break;

                case Stage::idle:
                    break;
            }

            // Oscillateur avec balayage de hauteur exponentiel
            const auto freq = std::min (nyquistSafe, baseHz * fastExp2 (params.sweepSemis * pitchEnv / 12.0));
            phase += freq / sampleRate;
            phase -= std::floor (phase);
            pitchEnv *= pitchCoef;

            auto body = fastSin2Pi (phase);
            if (params.shape > 0.001)
            {
                // mise en forme de l'oscillateur (sinusoïde vers forme arrondie), crête conservée
                const auto k = 1.0 + 7.0 * params.shape;
                body += (softClip (body * k) / softClip (k) - body) * params.shape;
            }
            body *= amp;
            lastBody = body;
            body += tail; // fondu de la queue du coup précédent (2 ms)
            tail *= tailCoef;

            // Transitoire : bruit filtré, enveloppe très courte
            clickLp = noise.next() * (1.0 - clickLpCoef) + clickLp * clickLpCoef;
            const auto click = clickLp * clickEnv * params.click * 2.0;
            clickEnv *= clickCoef;

            // Saturation anti-repliée puis filtre anti-continu
            const auto shaped = shaper.process ((body + click) * velocityGain * drive) * driveNorm;
            const auto dc = shaped - dcX + dcCoef * dcY;
            dcX = shaped;
            dcY = dc;

            out[i] += (float) (dc * level);

            if (stage == Stage::idle)
                break;
        }
    }

private:
    enum class Stage { idle, attack, hold, decay };

    Params params;
    double sampleRate = 48000.0;
    double phase = 0.0, amp = 0.0, pitchEnv = 0.0, clickEnv = 0.0, clickLp = 0.0, tail = 0.0, lastBody = 0.0;
    double velocityGain = 1.0, transpose = 1.0;
    double dcX = 0.0, dcY = 0.0, dcCoef = 0.999, clickLpCoef = 0.5;
    int holdSamplesLeft = 0;
    Stage stage = Stage::idle;
    AdaaSoftClip shaper;
    WhiteNoise noise;
};

} // namespace grv::dsp
