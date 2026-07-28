#pragma once

#include "Delay.h"
#include "xmc_common.h"

#include <cstdint>
#include <functional>
#include <utility>

namespace LowLevelEmbedded::XMC
{
    namespace Detail
    {
        inline void InitCycleCounter()
        {
#if defined(DWT) && defined(DWT_CTRL_CYCCNTENA_Msk) && \
    defined(CoreDebug_DEMCR_TRCENA_Msk)
            CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
            DWT->CYCCNT = 0U;
            DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
#endif
        }

        inline void DelayMicroseconds(uint32_t microseconds)
        {
            const uint32_t cyclesPerMicrosecond = SystemCoreClock / 1000000U;
            if (microseconds == 0U || cyclesPerMicrosecond == 0U)
            {
                return;
            }

#if defined(DWT) && defined(DWT_CTRL_CYCCNTENA_Msk)
            const uint32_t maximumChunk =
                UINT32_MAX / cyclesPerMicrosecond;
            while (microseconds != 0U)
            {
                const uint32_t chunk =
                    microseconds > maximumChunk
                        ? maximumChunk
                        : microseconds;
                const uint32_t targetCycles =
                    chunk * cyclesPerMicrosecond;
                const uint32_t start = DWT->CYCCNT;
                while (static_cast<uint32_t>(DWT->CYCCNT - start) <
                       targetCycles)
                {
                    __NOP();
                }
                microseconds -= chunk;
            }
#else
            const uint32_t systickPeriod = SysTick->LOAD + 1U;
            while (microseconds-- != 0U)
            {
                const uint32_t start = SysTick->VAL;
                uint32_t elapsed = 0U;
                while (elapsed < cyclesPerMicrosecond)
                {
                    const uint32_t current = SysTick->VAL;
                    elapsed = start >= current
                        ? start - current
                        : start + systickPeriod - current;
                }
            }
#endif
        }
    } // namespace Detail

    /**
     * Connect LowLevelEmbedded delay utilities to an XMC system tick.
     *
     * A DAVE project can pass SYSTIMER_GetTickCount. Other projects can pass
     * their RTOS or application millisecond counter.
     */
    inline bool InitDelays(std::function<uint32_t()> millisecondsSinceStartup)
    {
        if (!millisecondsSinceStartup)
        {
            return false;
        }

        Detail::InitCycleCounter();
        Utility::millis = std::move(millisecondsSinceStartup);
        Utility::Delay_us = Detail::DelayMicroseconds;
        Utility::Delay_ms = [](uint32_t milliseconds)
        {
            const uint32_t start = Utility::millis();
            while (static_cast<uint32_t>(Utility::millis() - start) <
                   milliseconds)
            {
            }
        };
        return true;
    }
} // namespace LowLevelEmbedded::XMC
