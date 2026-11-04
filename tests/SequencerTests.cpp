/*
    Groovebox — OFAD — AGPL-3.0-or-later
    Tests du séquenceur : placement à l'échantillon près, indépendance des blocs, swing, ternaire,
    vitesse et longueur par piste (polyrythmie), sens de lecture, coupure et solo, conditions
    (probabilité, A:B, FILL, PRÉ, VOISIN, 1ER), enregistrement en direct sans double déclenchement,
    synchronisation sur le logiciel hôte.
*/

#include "engine/Sequencer.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace grv::engine;

namespace
{
int failures = 0;

void check (bool condition, const char* what)
{
    std::printf ("[%s] %s\n", condition ? " OK " : "\xC3\x89" "CHEC", what);
    if (! condition) ++failures;
}

struct Hit { int track, lane; long long sample; float velocity; int note; double len; float accent = 0.0f; float glide = 0.0f; };

std::vector<Hit> run (Sequencer& seq, double sr, double bpm, double swing, long long totalSamples, int blockSize,
                      const PatternExtras* extras = nullptr, bool start = true)
{
    std::vector<Hit> hits;
    if (start) seq.requestPlay();
    for (long long done = 0; done < totalSamples; done += blockSize)
    {
        const auto n = (int) std::min<long long> (blockSize, totalSamples - done);
        seq.beginBlock (n, sr, bpm, swing, {});
        seq.collect (extras, [&] (const SeqEvent& e) { hits.push_back ({ e.track, e.lane, done + e.offset, e.velocity, e.note, e.lengthSamples, e.accent, e.glide }); });
        seq.endBlock();
    }
    return hits;
}

bool near (long long a, long long b, long long tol = 1) { return std::llabs (a - b) <= tol; }
int count (const std::vector<Hit>& h, int track, int lane = -1)
{
    int c = 0;
    for (auto& x : h) if (x.track == track && (lane < 0 || x.lane == lane)) ++c;
    return c;
}
} // namespace

int main()
{
    constexpr double sr = 48000.0;

    {
        Sequencer seq;
        for (int s = 0; s < 16; s += 4) seq.setStep (0, 0, s, 100);
        const auto hits = run (seq, sr, 120.0, 50.0, 48000, 512);
        check (hits.size() == 2, "2 noires en une seconde \xC3\xA0 120 BPM");
        check (hits.size() >= 2 && near (hits[0].sample, 0) && near (hits[1].sample, 24000), "noires \xC3\xA0 l'\xC3\xA9" "chantillon pr\xC3\xA8s (0 et 24000)");
        check (! hits.empty() && std::abs (hits[0].velocity - 100.0f / 127.0f) < 1e-6f, "v\xC3\xA9locit\xC3\xA9 transmise");
    }
    {
        Sequencer a, b;
        for (int s = 0; s < 16; ++s) { a.setStep (1, 3, s, 90); b.setStep (1, 3, s, 90); }
        const auto h1 = run (a, sr, 97.0, 50.0, 96000, 64);
        const auto h2 = run (b, sr, 97.0, 50.0, 96000, 1031);
        bool same = h1.size() == h2.size();
        for (size_t i = 0; same && i < h1.size(); ++i) same = near (h1[i].sample, h2[i].sample);
        check (same && ! h1.empty(), "m\xC3\xAAmes instants quelle que soit la taille des blocs");
    }
    {
        // Pas exactement sur une frontière de bloc : déclenché une seule fois
        Sequencer seq;
        seq.setStep (0, 0, 8, 100);
        const auto hits = run (seq, sr, 120.0, 50.0, 96000, 48000);
        check (count (hits, 0) == 1, "pas sur une fronti\xC3\xA8re de bloc : une seule fois");
    }
    {
        Sequencer seq;
        seq.setStep (0, 0, 0, 100); seq.setStep (0, 0, 1, 100);
        const auto hits = run (seq, sr, 120.0, 75.0, 12000, 256);
        check (hits.size() == 2 && near (hits[1].sample, 6000 + 3000), "swing 75 % : pas impair retard\xC3\xA9 d'un demi-pas");
    }
    {
        Sequencer seq;
        seq.setDivision (Division::sixteenthTriplet);
        seq.setTrackLength (0, 24);
        seq.setStep (0, 0, 1, 100);
        const auto hits = run (seq, sr, 120.0, 50.0, 8000, 128);
        check (hits.size() == 1 && near (hits[0].sample, 4000), "ternaire 1/16T : 4000 \xC3\xA9" "chantillons par pas");
    }
    {
        // Vitesse ×2 sur la piste 1, ×1/2 sur la piste 2
        Sequencer seq;
        for (int s = 0; s < 16; ++s) { seq.setStep (0, 0, s, 100); seq.setStep (1, 0, s, 100); }
        seq.setTrackSpeed (0, 6); // ×2
        seq.setTrackSpeed (1, 2); // ×1/2
        const auto hits = run (seq, sr, 120.0, 50.0, 96000, 256);
        check (count (hits, 0) == 32 && count (hits, 1) == 8, "vitesses \xC3\x97" "2 et \xC3\x97" "1/2 par piste");
    }
    {
        // Longueurs différentes : polyrythmie 3 contre 4
        Sequencer seq;
        seq.setTrackLength (0, 3); seq.setStep (0, 0, 0, 100);
        seq.setTrackLength (1, 4); seq.setStep (1, 0, 0, 100);
        const auto hits = run (seq, sr, 120.0, 50.0, 6000 * 12, 256);
        check (count (hits, 0) == 4 && count (hits, 1) == 3, "longueurs 3 et 4 : 4 et 3 frappes en 12 pas");
    }
    {
        Sequencer seq;
        seq.setTrackLength (2, 4);
        seq.setStep (2, 0, 0, 10); seq.setStep (2, 0, 1, 20); seq.setStep (2, 0, 2, 30); seq.setStep (2, 0, 3, 40);
        seq.setDirection (2, 0, Direction::reverse);
        const auto hits = run (seq, sr, 120.0, 50.0, 6000 * 4, 300);
        check (hits.size() == 4 && hits[0].velocity > hits[3].velocity, "sens arri\xC3\xA8re");
    }
    {
        Sequencer seq;
        seq.setTrackLength (2, 4);
        for (int s = 0; s < 4; ++s) seq.setStep (2, 0, s, 10 * (s + 1));
        seq.setDirection (2, 0, Direction::pingPong);
        const auto hits = run (seq, sr, 120.0, 50.0, 6000 * 6, 300);
        bool ok = hits.size() == 6;
        const int expect[6] { 10, 20, 30, 40, 30, 20 };
        for (size_t i = 0; ok && i < 6; ++i) ok = std::abs (hits[i].velocity * 127.0f - (float) expect[i]) < 0.5f;
        check (ok, "aller-retour : 1 2 3 4 3 2");
    }
    {
        Sequencer seq;
        seq.setStep (0, 0, 0, 100); seq.setStep (1, 0, 0, 100); seq.setStep (1, 1, 0, 100);
        seq.setTrackMute (0, true);
        seq.setLaneSolo (1, 1, true);
        const auto hits = run (seq, sr, 120.0, 50.0, 2000, 256);
        // v0.43 : la coupure de piste coupe le son (moteur), pas le jeu des notes
        check (count (hits, 0) == 1 && count (hits, 1, 0) == 0 && count (hits, 1, 1) == 1, "coupure de piste (notes jouées) et solo de ligne");
        Sequencer seq2;
        seq2.setStep (0, 0, 0, 100);
        seq2.setTrackMute (0, true); seq2.setMuteTails (true);
        check (count (run (seq2, sr, 120.0, 50.0, 2000, 256), 0) == 0, "coupure en laissant les tra\xC3\xAEnes : nouvelles notes sautées");
        Sequencer seq3;
        seq3.setStep (0, 0, 0, 100); seq3.setStep (0, 1, 0, 100);
        seq3.setLaneMute (0, 1, true); seq3.setLaneAudioMute (0, true);
        const auto h3 = run (seq3, sr, 120.0, 50.0, 2000, 256);
        check (count (h3, 0, 0) == 1 && count (h3, 0, 1) == 1, "lignes coup\xC3\xA9" "es par l'instrument : notes jou\xC3\xA9" "es");
    }
    {
        // Notes et durées des pistes mélodiques
        Sequencer seq;
        seq.setStep (3, 0, 0, 100); seq.setNote (3, 0, 0, 43); seq.setLength8 (3, 0, 0, 16);
        const auto hits = run (seq, sr, 120.0, 50.0, 2000, 256);
        check (hits.size() == 1 && hits[0].note == 43 && std::abs (hits[0].len - 12000.0) < 1.0, "note et dur\xC3\xA9" "e (2 pas = 12000 \xC3\xA9" "chantillons)");
    }
    {
        // Conditions A:B, 1ER, FILL, probabilité
        Sequencer seq;
        PatternExtras ex;
        seq.setTrackLength (0, 1);
        seq.setStep (0, 0, 0, 100); ex.steps[PatternExtras::key (0, 0, 0)].cond = Condition::ab;
        ex.steps[PatternExtras::key (0, 0, 0)].a = 2; ex.steps[PatternExtras::key (0, 0, 0)].b = 4;
        seq.setTrackLength (1, 1);
        seq.setStep (1, 0, 0, 100); ex.steps[PatternExtras::key (1, 0, 0)].cond = Condition::first;
        seq.setTrackLength (2, 1);
        seq.setStep (2, 0, 0, 100); ex.steps[PatternExtras::key (2, 0, 0)].cond = Condition::fill;
        seq.setTrackLength (3, 1);
        seq.setStep (3, 0, 0, 100); ex.steps[PatternExtras::key (3, 0, 0)].prob = 50;
        const auto hits = run (seq, sr, 120.0, 50.0, 6000 * 400, 512, &ex);
        check (count (hits, 0) == 100, "A:B 2:4 : une boucle sur quatre (100 sur 400)");
        check (count (hits, 1) == 1, "1ER : seulement la premi\xC3\xA8re boucle");
        check (count (hits, 2) == 0, "FILL inactif : rien");
        const auto p = count (hits, 3);
        check (p > 150 && p < 250, "probabilit\xC3\xA9 50 % : environ la moiti\xC3\xA9");

        Sequencer seq2;
        seq2.setTrackLength (2, 1);
        seq2.setStep (2, 0, 0, 100);
        seq2.setFill (true);
        const auto h2 = run (seq2, sr, 120.0, 50.0, 6000 * 10, 512, &ex);
        check (count (h2, 2) == 10, "FILL actif : toutes les boucles");
    }
    {
        // Répétitions dans le pas : 4 coups réguliers, vélocité en pente, une seule évaluation de condition
        for (int block : { 64, 512, 4096 })
        {
            Sequencer seq;
            PatternExtras ex;
            seq.setStep (0, 0, 0, 100);
            auto& x = ex.steps[PatternExtras::key (0, 0, 0)];
            x.ratchet = 4; x.ratchetSlope = -50;
            seq.setStep (0, 1, 0, 100);
            auto& y = ex.steps[PatternExtras::key (0, 1, 0)];
            y.ratchet = 3; y.prob = 0; // condition fausse : aucune répétition
            const auto hits = run (seq, sr, 120.0, 50.0, 6000, block, &ex);
            bool ok = count (hits, 0, 0) == 4 && count (hits, 0, 1) == 0;
            for (int j = 0; ok && j < 4; ++j)
            {
                ok = near (hits[(size_t) j].sample, 1500 * j) && hits[(size_t) j].len <= 1275.5
                  && std::abs (hits[(size_t) j].velocity * 127.0f - 100.0f * (1.0f - 0.5f * (float) j / 3.0f)) < 0.6f;
            }
            check (ok, block == 64 ? "r\xC3\xA9p\xC3\xA9titions \xC3\x97" "4 (blocs de 64)" : block == 512 ? "r\xC3\xA9p\xC3\xA9titions \xC3\x97" "4 (blocs de 512)" : "r\xC3\xA9p\xC3\xA9titions \xC3\x97" "4 (blocs de 4096)");
        }
    }
    {
        // Répétitions avec swing : toutes avant le pas suivant (pas impair retardé par le swing)
        Sequencer seq;
        PatternExtras ex;
        seq.setStep (0, 0, 1, 100); seq.setStep (0, 0, 2, 100);
        ex.steps[PatternExtras::key (0, 0, 1)].ratchet = 4;
        const auto hits = run (seq, sr, 120.0, 75.0, 18000, 256, &ex);
        // pas 1 à 6000 + swing 3000 = 9000 ; pas 2 à 12000 : 4 coups dans [9000, 12000)
        bool ok = count (hits, 0, 0) == 5;
        for (int j = 0; ok && j < 4; ++j) ok = hits[(size_t) j].sample >= 9000 - 1 && hits[(size_t) j].sample < 12000;
        ok = ok && near (hits[4].sample, 12000);
        check (ok, "r\xC3\xA9p\xC3\xA9titions et swing : avant le pas suivant");
    }
    {
        // PRÉ : suit le résultat de la condition précédente sur la ligne
        Sequencer seq;
        PatternExtras ex;
        seq.setTrackLength (0, 2);
        seq.setStep (0, 0, 0, 100); ex.steps[PatternExtras::key (0, 0, 0)].cond = Condition::ab;
        ex.steps[PatternExtras::key (0, 0, 0)].a = 1; ex.steps[PatternExtras::key (0, 0, 0)].b = 2;
        seq.setStep (0, 0, 1, 100); ex.steps[PatternExtras::key (0, 0, 1)].cond = Condition::pre;
        const auto hits = run (seq, sr, 120.0, 50.0, 6000 * 8, 512, &ex);
        check (count (hits, 0) == 4, "PR\xC3\x89 : le pas suit le pas pr\xC3\xA9" "c\xC3\xA9" "dent conditionnel");
    }
    {
        // Enregistrement : la frappe à la main juste avant un pas n'est pas rejouée par ce pas
        Sequencer seq;
        seq.setRecording (true);
        seq.requestPlay();
        seq.beginBlock (5990, sr, 120.0, 50.0, {});
        seq.collect (nullptr, [] (const SeqEvent&) {});
        seq.endBlock();
        seq.beginBlock (64, sr, 120.0, 50.0, {});
        const auto lane = seq.recordHit (0, 2, 0, 0.8f, 0, false);
        int played = 0;
        seq.collect (nullptr, [&] (const SeqEvent&) { ++played; });
        seq.endBlock();
        check (lane == 2 && seq.getStep (0, 2, 1) > 0, "frappe enregistr\xC3\xA9" "e sur le pas le plus proche");
        check (played == 0, "pas tout juste enregistr\xC3\xA9 : pas de double d\xC3\xA9" "clenchement");
    }
    {
        // Synchro sur le logiciel hôte : position en noires
        Sequencer seq;
        seq.setStep (0, 0, 4, 100); // 1re noire
        HostTime h { true, 0.99 };
        seq.beginBlock (480, sr, 120.0, 50.0, h); // de 0,99 à 1,01 noire
        int got = -1;
        seq.collect (nullptr, [&] (const SeqEvent& e) { got = e.offset; });
        seq.endBlock();
        check (got >= 239 && got <= 241, "synchro h\xC3\xB4te : pas \xC3\xA0 la noire 1");
    }

    {
        // Patterns : un pattern lancé remplace le pattern joué à la mesure suivante et repart de son début
        Sequencer seq;
        seq.setStep (0, 0, 0, 100);                         // pattern 1 : une note au temps 1
        seq.setStepP (0, 1, 0, 2, 100);                     // pattern 2 : une note au 3e pas
        std::vector<Hit> hits;
        seq.requestPlay();
        long long done = 0;
        auto block = [&] (int n)
        {
            seq.beginBlock (n, sr, 120.0, 50.0, {});
            seq.collect (nullptr, [&] (const SeqEvent& e) { hits.push_back ({ e.track, e.lane, done + e.offset, e.velocity, e.note, e.lengthSamples, e.accent }); });
            seq.endBlock();
            done += n;
        };
        block (12000);                                      // un quart de mesure (120 BPM : 1 mesure = 96000)
        seq.launch (0, 1);
        while (done < 96000 * 2) block (512);
        // mesure 1 : pattern 1 au début (0) ; mesure 2 : pattern 2, 3e pas = 96000 + 2 × 6000
        check (hits.size() == 2 && near (hits[0].sample, 0) && near (hits[1].sample, 96000 + 12000),
               "pattern lanc\xC3\xA9 : bascule \xC3\xA0 la mesure suivante, depuis son d\xC3\xA9" "but");
        check (seq.getPlaying (0) == 1 && seq.getQueued (0) == -1, "pattern jou\xC3\xA9 mis \xC3\xA0 jour");
    }
    {
        // Arrêt d'une piste et rangée de lancement
        Sequencer seq;
        for (int t = 0; t < 3; ++t) { seq.setStepP (t, 2, 0, 0, 100); seq.setStep (t, 0, 0, 100); }
        seq.launch (1, Sequencer::stopPattern);
        const auto hits = run (seq, sr, 120.0, 50.0, 96000, 512);
        check (count (hits, 0) == 1 && count (hits, 1) == 0 && count (hits, 2) == 1, "piste arr\xC3\xAAt\xC3\xA9" "e : silencieuse");
        seq.launchRow (2);
        std::vector<Hit> h2;
        long long done = 96000;
        for (; done < 96000 * 3; done += 512)
        {
            seq.beginBlock (512, sr, 120.0, 50.0, {});
            seq.collect (nullptr, [&] (const SeqEvent& e) { h2.push_back ({ e.track, e.lane, done + e.offset, e.velocity, e.note, e.lengthSamples }); });
            seq.endBlock();
        }
        check (seq.getPlaying (0) == 2 && seq.getPlaying (1) == 2 && seq.getPlaying (2) == 2, "rang\xC3\xA9" "e : pattern 3 sur toutes les pistes");
        // v0.23 : rangée dont la case est vide sur une piste → cette piste s'arrête
        seq.launchRow (4); // aucune note en pattern 5 sauf sur la piste 0
        seq.setStepP (0, 4, 0, 0, 100);
        seq.launchRow (4);
        for (; done < 96000 * 5; done += 512) { seq.beginBlock (512, sr, 120.0, 50.0, {}); seq.collect (nullptr, [] (const SeqEvent&) {}); seq.endBlock(); }
        check (seq.getPlaying (0) == 4 && seq.getPlaying (1) == -1 && seq.getPlaying (2) == -1, "rang\xC3\xA9" "e : case vide = piste arr\xC3\xAAt\xC3\xA9" "e");
    }
    {
        // Copie de pattern et longueur par pattern
        Sequencer seq;
        seq.setPatternLength (0, 0, 32);
        seq.setStepP (0, 0, 3, 17, 90);
        seq.copyPattern (0, 0, 5);
        check (seq.getStepP (0, 5, 3, 17) == 90 && seq.getPatternLength (0, 5) == 32 && seq.hasNotes (0, 5) && ! seq.hasNotes (0, 4),
               "copie de pattern (notes et longueur)");
        seq.setEditing (0, 5);
        check (seq.getStep (0, 3, 17) == 90 && seq.getTrackLength (0) == 32, "pattern \xC3\xA9" "dit\xC3\xA9 : lecture et \xC3\xA9" "criture");
    }
    {
        // Division par pattern : la piste 1 en ternaire, la piste 2 reste en double croches
        Sequencer seq;
        seq.setPatternDivision (0, 0, Division::sixteenthTriplet);
        seq.setStep (0, 0, 1, 100); seq.setStep (1, 0, 1, 100);
        const auto hits = run (seq, sr, 120.0, 50.0, 8000, 128);
        bool t0 = false, t1 = false;
        for (const auto& h : hits) { if (h.track == 0) t0 = near (h.sample, 4000); if (h.track == 1) t1 = near (h.sample, 6000); }
        check (t0 && t1 && seq.getPatternDivision (1, 0) == Division::sixteenth && seq.getPatternDivision (0, 1) == Division::sixteenth,
               "division par pattern : seule la piste et le pattern é" "crits changent");
        seq.copyPattern (0, 0, 2);
        check (seq.getPatternDivision (0, 2) == Division::sixteenthTriplet, "copie de pattern : la division suit");
    }
    {
        // Divisions ajoutées
        check (stepsPerBar (Division::quarter) == 4 && stepsPerBar (Division::thirtySecondTriplet) == 48 && stepsPerBar (Division::sixtyFourth) == 64,
               "divisions 1/4, 1/32T, 1/64");
        Sequencer seq;
        seq.setDivision (Division::sixtyFourth);
        seq.setStep (0, 0, 0, 100); seq.setStep (0, 0, 1, 100);
        seq.setTrackLength (0, 2);
        const auto hits = run (seq, sr, 120.0, 50.0, 6000, 512);
        check (count (hits, 0) == 4, "1/64 : 4 pas par double croche");
    }
    {
        // Enregistrement à quantification partielle : l'écart est gardé en décalage fin
        Sequencer seq;
        seq.setRecording (true);
        seq.setRecordQuantize (0);
        seq.requestPlay();
        seq.beginBlock (1500, sr, 120.0, 50.0, {}); // un quart de pas (1 pas = 6000)
        seq.collect (nullptr, [] (const SeqEvent&) {});
        const auto lane = seq.recordHit (0, 0, 0, 0.8f, 0, false);
        seq.endBlock();
        check (lane == 0 && seq.getStep (0, 0, 0) > 0 && seq.getMicro (0, 0, 0) == 0, "quantification 0 % : frappe au d\xC3\xA9" "but du pas");
        seq.beginBlock (64, sr, 120.0, 50.0, {});
        seq.recordHit (0, 1, 0, 0.8f, 0, false);    // 1500 éch. = 0,25 pas après le pas 0
        seq.endBlock();
        check (seq.getMicro (0, 1, 0) == 16, "quantification 0 % : d\xC3\xA9" "calage fin de 1/4 de pas");
    }

    {
        // Boucle du logiciel hôte (2 mesures) : un pattern lancé au milieu continue de jouer à chaque tour,
        // et un pattern en attente avec une quantification plus longue que la boucle finit par partir
        Sequencer seq;
        seq.setLaunchQuant (4); // 4 mesures, plus long que la boucle
        for (int s = 0; s < 16; s += 4) seq.setStepP (0, 1, 0, s, 100);
        seq.setPatternLength (0, 1, 16);
        int hits = 0;
        double ppq = 0.0;
        for (int b = 0; b < 2000; ++b)
        {
            if (ppq >= 8.0 - 1.0e-9) ppq -= 8.0; // boucle 0..8 noires
            if (b == 100) seq.launch (0, 1);       // lancé à la noire 2, au milieu de la boucle
            const HostTime h { true, ppq };
            seq.beginBlock (480, sr, 120.0, 50.0, h);
            seq.collect (nullptr, [&] (const SeqEvent&) { ++hits; });
            seq.endBlock();
            ppq += 480.0 * 2.0 / sr;
        }
        // 2000 blocs de 480 = 20 s = 40 noires = 5 tours ; le pattern part au 1er rebouclage (8 noires),
        // puis 4 noires par mesure pendant 32 noires = 32 frappes
        std::printf ("  (pattern joué %d, en attente %d, frappes %d)\n", seq.getPlaying (0), seq.getQueued (0), hits);
        check (seq.getPlaying (0) == 1 && hits >= 30 && hits <= 33, "boucle de l'h\xC3\xB4te : pattern lanc\xC3\xA9 puis jou\xC3\xA9 \xC3\xA0 chaque tour");
    }

    {
        // Accent de piste : la vélocité monte vers le maximum selon la force d'accent
        Sequencer seq;
        seq.setStep (0, 0, 0, 64); seq.setStep (0, 0, 1, 64);
        seq.setStepFlags (0, 0, stepAccent);
        seq.setAccentAmount (0, 50);
        const auto hits = run (seq, sr, 120.0, 50.0, 12000, 256);
        const auto v0 = 64.0f / 127.0f;
        check (hits.size() == 2 && std::abs (hits[0].velocity - (v0 + (1.0f - v0) * 0.5f)) < 1e-5f && std::abs (hits[1].velocity - v0) < 1e-6f
               && std::abs (hits[0].accent - 0.5f) < 1e-6f && hits[1].accent == 0.0f,
               "accent : pas accentu\xC3\xA9 renforc\xC3\xA9, pas suivant inchang\xC3\xA9");
    }
    {
        // v0.46 fix27 : force d'accent propre au pas, durée du glissé portée par la note qui arrive
        Sequencer seq;
        seq.setStep (0, 0, 0, 64); seq.setStep (0, 0, 1, 64); seq.setStep (0, 0, 2, 64);
        seq.setStepFlags (0, 0, stepAccent); seq.setStepAmounts (0, 0, 100);         // accent 100 % sur le pas 1
        seq.setStepFlags (0, 1, stepAccent | stepSlide); seq.setStepAmounts (0, 1, 25 << 8); // accent de la piste, glissé 25 %
        seq.setAccentAmount (0, 50);
        const auto hits = run (seq, sr, 120.0, 50.0, 17000, 256);
        const auto v0 = 64.0f / 127.0f;
        std::printf ("  (valeurs de pas : v %.3f %.3f %.3f, glissé %.2f %.2f %.2f)\n", hits.size() > 2 ? hits[0].velocity : -1.0f, hits.size() > 2 ? hits[1].velocity : -1.0f,
                     hits.size() > 2 ? hits[2].velocity : -1.0f, hits.size() > 2 ? hits[0].glide : -1.0f, hits.size() > 2 ? hits[1].glide : -1.0f, hits.size() > 2 ? hits[2].glide : -1.0f);
        check (hits.size() == 3 && std::abs (hits[0].velocity - 1.0f) < 1e-5f && std::abs (hits[1].velocity - (v0 + (1.0f - v0) * 0.5f)) < 1e-5f
               && hits[0].glide == 0.0f && hits[1].glide == 0.0f && std::abs (hits[2].glide - 0.25f) < 1e-6f,
               "valeurs de pas : force d'accent du pas, gliss\xC3\xA9 r\xC3\xA9gl\xC3\xA9 port\xC3\xA9 par la note suivante");
        Sequencer copy;
        copy.setStepAmountsP (0, 0, 3, 40 | (60 << 8)); copy.copyPattern (0, 0, 1);
        check (copy.getStepAmountsP (0, 1, 3) == (40 | (60 << 8)), "copie de pattern : valeurs de pas");
    }
    {
        // Glissé : la note tient au-delà du début du pas suivant (chevauchement), même si sa durée est courte
        Sequencer seq;
        seq.setStep (0, 0, 0, 100); seq.setStep (0, 0, 1, 100);
        seq.setLength8 (0, 0, 0, 4); seq.setLength8 (0, 0, 1, 4);
        seq.setStepFlags (0, 0, stepSlide);
        const auto hits = run (seq, sr, 120.0, 50.0, 12000, 128);
        check (hits.size() == 2 && hits[0].len > (double) (hits[1].sample - hits[0].sample) && hits[1].len < 6000.0 * 0.6,
               "gliss\xC3\xA9 : la note se prolonge jusque sur la suivante, la suivante garde sa dur\xC3\xA9" "e");
    }
    {
        // Glissé avec swing : le chevauchement tient compte du pas suivant décalé
        Sequencer seq;
        seq.setStep (0, 0, 0, 100); seq.setStep (0, 0, 1, 100);
        seq.setStepFlags (0, 0, stepSlide);
        const auto hits = run (seq, sr, 120.0, 66.0, 12000, 100);
        check (hits.size() == 2 && hits[0].len > (double) (hits[1].sample - hits[0].sample), "gliss\xC3\xA9 avec swing");
    }
    {
        // Flam : un coup léger sur le pas puis le coup principal 20 ms après, quelle que soit la taille des blocs
        for (int block : { 32, 333, 4096 })
        {
            Sequencer seq;
            seq.setStep (0, 2, 4, 120);
            PatternExtras ex;
            ex.steps[PatternExtras::key (0, 2, 4)].flam = 20;
            const auto hits = run (seq, sr, 120.0, 50.0, 30000, block, &ex);
            const bool ok = hits.size() == 2 && near (hits[0].sample, 24000) && near (hits[1].sample, 24000 + 960)
                         && hits[0].velocity < hits[1].velocity && std::abs (hits[1].velocity - 120.0f / 127.0f) < 1e-5f;
            check (ok, block == 32 ? "flam : 2 coups, 20 ms d'\xC3\xA9" "cart (blocs de 32)" : block == 333 ? "flam (blocs de 333)" : "flam (blocs de 4096)");
        }
    }
    {
        // Longueur par ligne : ligne de 3 pas dans un pattern de 16 (polymétrie)
        Sequencer seq;
        seq.setStep (0, 0, 0, 100);   // ligne 1 : pattern de 16, un coup par mesure
        seq.setStep (0, 1, 0, 100);   // ligne 2 : boucle sur 3 pas
        seq.setLaneLength (0, 1, 3);
        const auto hits = run (seq, sr, 120.0, 50.0, 96000, 512); // 16 pas
        std::vector<long long> lane1;
        for (auto& h : hits) if (h.lane == 1) lane1.push_back (h.sample);
        check (count (hits, 0, 0) == 1 && lane1.size() == 6 && near (lane1[1], 18000) && near (lane1[5], 90000),
               "longueur par ligne : ligne de 3 pas, 6 coups sur 16 pas");
        Sequencer copy;
        copy.setLaneLengthP (0, 0, 1, 3); copy.setStepFlagsP (0, 0, 5, stepAccent | stepSlide);
        copy.copyPattern (0, 0, 2);
        check (copy.getLaneLengthP (0, 2, 1) == 3 && copy.getStepFlagsP (0, 2, 5) == (stepAccent | stepSlide), "copie de pattern : fonctions de pas et longueurs de ligne");
        copy.resetTrack (0);
        check (copy.getLaneLengthP (0, 2, 1) == 0 && copy.getStepFlagsP (0, 2, 5) == 0 && copy.getAccentAmount (0) == 50, "remise \xC3\xA0 z\xC3\xA9ro de la piste");
    }

    std::printf (failures == 0 ? "\nTOUS LES TESTS PASSENT (0 \xC3\xA9" "chec)\n" : "\nDES TESTS \xC3\x89" "CHOUENT (%d \xC3\xA9" "chec(s))\n", failures);
    return failures == 0 ? 0 : 1;
}
