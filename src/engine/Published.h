/*
    Groovebox — OFAD — AGPL-3.0-or-later

    Publication sans verrou d'objets construits sur le fil de l'interface et lus par le fil audio.

    Le fil de l'interface construit un nouvel objet immuable (structure des pistes, scènes,
    modulations…) puis le publie d'un seul échange atomique. Le fil audio lit le pointeur au début
    de chaque bloc et ne le garde que le temps du bloc. L'ancien objet est mis à la corbeille et
    n'est détruit que plusieurs secondes plus tard, sur le fil de l'interface : le fil audio
    n'alloue ni ne libère jamais rien.
*/

#pragma once

#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>

namespace grv::engine
{

/** Corbeille à retardement (fil de l'interface uniquement). */
class RetireBin
{
public:
    static constexpr double graceMs = 3000.0;

    template <typename T>
    void retire (std::unique_ptr<T> object, double nowMs)
    {
        if (object != nullptr)
            items.push_back ({ std::shared_ptr<void> (std::move (object)), nowMs });
    }

    void collect (double nowMs)
    {
        for (size_t i = 0; i < items.size();)
        {
            if (nowMs - items[i].time > graceMs) { items[i] = std::move (items.back()); items.pop_back(); }
            else ++i;
        }
    }

    /** Fin d'un long travail groupé : rien de ce qui a été jeté pendant ce travail ne part avant
        le délai de grâce compté à partir de maintenant (le fil audio a pu garder l'objet). */
    void restamp (double nowMs) noexcept { for (auto& i : items) i.time = std::max (i.time, nowMs); }

    size_t size() const noexcept { return items.size(); }

private:
    struct Item { std::shared_ptr<void> object; double time; };
    std::vector<Item> items;
};

template <typename T>
class Published
{
public:
    /** Fil de l'interface : publie un nouvel objet ; l'ancien part à la corbeille. */
    void publish (std::unique_ptr<T> next, RetireBin& bin, double nowMs)
    {
        current.store (next.get(), std::memory_order_release);
        bin.retire (std::move (owned), nowMs);
        owned = std::move (next);
    }

    /** Fil audio (et interface) : objet en cours, jamais nul après la première publication. */
    const T* get() const noexcept { return current.load (std::memory_order_acquire); }

    /** Fil de l'interface : accès à l'objet en cours pour en faire une copie modifiée. */
    const T* editable() const noexcept { return owned.get(); }

private:
    std::atomic<const T*> current { nullptr };
    std::unique_ptr<T> owned;
};

/** File sans verrou à un producteur et un consommateur. */
template <typename T, size_t Capacity>
class SpscQueue
{
public:
    bool push (const T& v) noexcept
    {
        const auto w = write.load (std::memory_order_relaxed);
        const auto next = (w + 1) % Capacity;
        if (next == read.load (std::memory_order_acquire)) return false; // pleine
        buffer[w] = v;
        write.store (next, std::memory_order_release);
        return true;
    }

    bool pop (T& v) noexcept
    {
        const auto r = read.load (std::memory_order_relaxed);
        if (r == write.load (std::memory_order_acquire)) return false;
        v = buffer[r];
        read.store ((r + 1) % Capacity, std::memory_order_release);
        return true;
    }

private:
    T buffer[Capacity] {};
    std::atomic<size_t> write { 0 }, read { 0 };
};

} // namespace grv::engine
