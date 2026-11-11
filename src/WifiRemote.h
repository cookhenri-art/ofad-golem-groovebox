/*
    Groovebox — OFAD — AGPL-3.0-or-later

    1.1 : TÉLÉCOMMANDE PAR LE WI-FI (GOLEM ↔ GOLEM, sans pilote ni câble). Mêmes messages que par le
    câble (src/engine/RemoteProtocol.h, sans F0 ni F7), dans des paquets UDP sur le port 47474.

    Côté PC (WifiServer, application autonome, si « Accepter une télécommande Wi-Fi » est coché) :
      - le téléphone cherche les GOLEM du réseau par un « bonjour » diffusé (broadcast) ; chaque GOLEM
        qui écoute répond « je suis là » (commande 05 : identifiant + nom de l'ordinateur) ;
      - le dernier téléphone qui a dit bonjour devient LA télécommande : ses « régler » sont passés au
        fil audio par une file sans verrou ; l'état du PC lui est renvoyé (remoteFeedbackTick).
    Côté téléphone (WifiClient) : recherche, envoi des messages, réception de l'état et des noms.

    Sécurité : seulement les adresses du réseau local (privées), messages contrôlés (format, taille),
    cibles bornées comme pour le MIDI ; l'écoute est coupée par défaut (case à cocher des Réglages).
*/

#pragma once

#include <JuceHeader.h>
#include "engine/RemoteProtocol.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace grv
{
constexpr int wifiRemotePort = 47474;
constexpr uint8_t cmdAnnounce = 0x05; // « je suis là » : identifiant (4 × 7 bits) puis nom (moitiés de 4 bits)

/** Adresse du réseau local (192.168.x, 10.x, 172.16-31.x, 169.254.x, boucle locale). */
inline bool isLocalNetworkAddress (const juce::String& ip)
{
    if (ip.startsWith ("192.168.") || ip.startsWith ("10.") || ip.startsWith ("169.254.") || ip.startsWith ("127.")) return true;
    if (ip.startsWith ("172."))
    {
        const auto second = ip.fromFirstOccurrenceOf (".", false, false).upToFirstOccurrenceOf (".", false, false).getIntValue();
        return second >= 16 && second <= 31;
    }
    return false;
}

inline void putId (std::vector<uint8_t>& m, uint32_t id) { for (int s = 21; s >= 0; s -= 7) m.push_back ((uint8_t) ((id >> s) & 0x7F)); }
inline uint32_t getId (const uint8_t* d) { return ((uint32_t) d[0] << 21) | ((uint32_t) d[1] << 14) | ((uint32_t) d[2] << 7) | (uint32_t) d[3]; }
inline void putText (std::vector<uint8_t>& m, const juce::String& s)
{
    const auto utf8 = s.toStdString();
    for (size_t i = 0; i < utf8.size() && i < 40; ++i) { const auto c = (uint8_t) utf8[i]; m.push_back ((uint8_t) (c >> 4)); m.push_back ((uint8_t) (c & 15)); }
}
inline juce::String getText (const uint8_t* d, size_t n)
{
    std::string s;
    for (size_t i = 0; i + 1 < n; i += 2) s += (char) (((d[i] & 15) << 4) | (d[i + 1] & 15));
    return juce::String::fromUTF8 (s.data(), (int) s.size());
}

// ============================================================================================
/** Côté PC : écoute du port, une télécommande à la fois. */
class WifiServer final : private juce::Thread
{
public:
    explicit WifiServer (uint32_t instanceId) : juce::Thread ("GOLEM Wi-Fi"), id (instanceId) {}
    ~WifiServer() override { stop(); }

    bool start()
    {
        if (isThreadRunning()) return true;
        sock = std::make_unique<juce::DatagramSocket> (true);
        if (! sock->bindToPort (wifiRemotePort)) { sock.reset(); return false; }
        startThread();
        return true;
    }
    void stop()
    {
        signalThreadShouldExit();
        if (sock != nullptr) sock->shutdown();
        stopThread (2000);
        sock.reset();
        const juce::SpinLock::ScopedLockType l (clientLock);
        clientIp.clear();
    }
    bool running() const { return isThreadRunning(); }

    /** Fil audio : les « régler » reçus, dans l'ordre (aucun verrou, aucune allocation). */
    template <typename F> void drain (F&& f)
    {
        int s1, n1, s2, n2;
        fifo.prepareToRead (fifo.getNumReady(), s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) f (slots[(size_t) (s1 + i)].data(), (size_t) sizes[(size_t) (s1 + i)]);
        for (int i = 0; i < n2; ++i) f (slots[(size_t) (s2 + i)].data(), (size_t) sizes[(size_t) (s2 + i)]);
        fifo.finishedRead (n1 + n2);
    }

    /** Fil des messages : un message (sans F0 ni F7) vers la télécommande, s'il y en a une. */
    void send (const std::vector<uint8_t>& m)
    {
        juce::String ip; int port = 0;
        { const juce::SpinLock::ScopedLockType l (clientLock); ip = clientIp; port = clientPort; }
        if (ip.isNotEmpty() && sock != nullptr && ! m.empty()) sock->write (ip, port, m.data(), (int) m.size());
    }
    bool hasClient() { const juce::SpinLock::ScopedLockType l (clientLock); return clientIp.isNotEmpty(); }

    std::atomic<int> helloSeq { 0 };    // nouvelle télécommande (ou nouveau « bonjour »)
    std::atomic<bool> fullRequest { false };

private:
    void run() override
    {
        std::array<uint8_t, 1500> buf {};
        while (! threadShouldExit() && sock != nullptr)
        {
            if (sock->waitUntilReady (true, 250) != 1) continue;
            juce::String ip; int port = 0;
            const auto n = sock->read (buf.data(), (int) buf.size(), false, ip, port);
            if (n <= 0 || ! isLocalNetworkAddress (ip)) continue;
            handle (buf.data(), (size_t) n, ip, port);
        }
    }

    void handle (const uint8_t* d, size_t n, const juce::String& ip, int port)
    {
        if (! remote::isRemote (d, n)) return;
        bool full = false;
        if (remote::isHello (d, n, full))
        {
            if (n >= 10 && getId (d + 6) == id) return; // notre propre recherche (même appareil)
            {
                const juce::SpinLock::ScopedLockType l (clientLock);
                if (ip != clientIp || port != clientPort) full = true;
                clientIp = ip; clientPort = port;
            }
            if (full) fullRequest.store (true);
            helloSeq.fetch_add (1);
            std::vector<uint8_t> a { remote::manufacturer, remote::id0, remote::id1, remote::version, cmdAnnounce };
            putId (a, id);
            putText (a, juce::SystemStats::getComputerName());
            sock->write (ip, port, a.data(), (int) a.size());
            return;
        }
        if ((d[4] != remote::cmdSet && d[4] != remote::cmdMidi) || n > 1024) return; // réglages, ou notes / CC du clavier
        {
            const juce::SpinLock::ScopedLockType l (clientLock);
            if (ip != clientIp) return; // seulement la télécommande en cours
        }
        int s1, n1, s2, n2;
        fifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 == 1) { std::copy (d, d + n, slots[(size_t) s1].begin()); sizes[(size_t) s1] = (int) n; }
        fifo.finishedWrite (n1);
    }

    const uint32_t id;
    std::unique_ptr<juce::DatagramSocket> sock;
    juce::SpinLock clientLock;
    juce::String clientIp;
    int clientPort = 0;
    static constexpr int numSlots = 64;
    juce::AbstractFifo fifo { numSlots };
    std::array<std::array<uint8_t, 1024>, numSlots> slots {};
    std::array<int, numSlots> sizes {};
};
} // namespace grv
