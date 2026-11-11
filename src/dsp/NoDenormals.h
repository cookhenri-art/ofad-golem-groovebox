/*
    Groovebox — OFAD — AGPL-3.0-or-later

    1.1 : les nombres « dénormaux » (très petits, dans les queues qui s'éteignent) rendent le calcul des
    filtres jusqu'à 10 fois plus lent. Le fil audio les coupe déjà (processBlock) ; les calculs faits
    hors du fil audio (traitement d'un échantillon, d'une prise) doivent le faire aussi.
    Garde à portée : met le processeur en « vidage à zéro » le temps du bloc, puis remet l'état d'avant.
*/

#pragma once

#include <cstdint>
#if defined (__SSE__) || defined (_M_X64) || (defined (_M_IX86_FP) && _M_IX86_FP >= 1)
 #include <xmmintrin.h>
 #define GRV_FTZ_SSE 1
#endif

namespace grv::dsp
{

class ScopedFlushDenormals
{
public:
    ScopedFlushDenormals() noexcept
    {
       #if defined (GRV_FTZ_SSE)
        saved = _mm_getcsr();
        _mm_setcsr (saved | 0x8040u);                              // FTZ + DAZ
       #elif defined (__aarch64__)
        std::uint64_t v; asm volatile ("mrs %0, fpcr" : "=r" (v)); saved = v;
        v |= (1ull << 24); asm volatile ("msr fpcr, %0" : : "r" (v));
       #elif defined (__arm__) && defined (__ARM_FP)
        std::uint32_t v; asm volatile ("vmrs %0, fpscr" : "=r" (v)); saved = v;
        v |= (1u << 24); asm volatile ("vmsr fpscr, %0" : : "r" (v));
       #endif
    }
    ~ScopedFlushDenormals() noexcept
    {
       #if defined (GRV_FTZ_SSE)
        _mm_setcsr ((unsigned int) saved);
       #elif defined (__aarch64__)
        std::uint64_t v = saved; asm volatile ("msr fpcr, %0" : : "r" (v));
       #elif defined (__arm__) && defined (__ARM_FP)
        std::uint32_t v = (std::uint32_t) saved; asm volatile ("vmsr fpscr, %0" : : "r" (v));
       #endif
    }
    ScopedFlushDenormals (const ScopedFlushDenormals&) = delete;
    ScopedFlushDenormals& operator= (const ScopedFlushDenormals&) = delete;
private:
    std::uint64_t saved = 0;
};

} // namespace grv::dsp
