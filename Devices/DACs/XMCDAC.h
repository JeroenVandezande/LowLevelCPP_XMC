#pragma once

#include "LLE_DAC.h"
#include "xmc_dac.h"

#include <concepts>
#include <cstdint>
#include <limits>

#if defined(DAC)

namespace LowLevelEmbedded
{
    template <std::unsigned_integral ValueT>
    class XMCDAC final : public IDAC<ValueT>
    {
    public:
        XMCDAC(
            XMC_DAC_t* dac,
            float reference_voltage,
            uint8_t max_channels = 2,
            uint8_t resolution_bits = 12)
            : dac_(dac),
              reference_voltage_(reference_voltage),
              max_channels_(max_channels),
              max_value_(ResolutionMax(resolution_bits))
        {
        }

        bool WriteDAC(uint8_t channel, ValueT value) override
        {
            if (dac_ == nullptr || channel >= max_channels_ ||
                value > max_value_)
            {
                return false;
            }
            XMC_DAC_CH_Write(dac_, channel, static_cast<uint16_t>(value));
            return true;
        }

        ValueT GetMaxDAValue() override
        {
            return max_value_;
        }

        uint8_t GetMaxChannels() override
        {
            return max_channels_;
        }

        bool WriteDACVoltage(uint8_t channel, float value) override
        {
            if (reference_voltage_ <= 0.0F ||
                value < 0.0F ||
                value > reference_voltage_)
            {
                return false;
            }
            return WriteDAC(
                channel,
                static_cast<ValueT>(
                    value * static_cast<float>(max_value_) /
                    reference_voltage_));
        }

        IDACChannel<ValueT>* CreateChannelObject(uint8_t channel) override
        {
            return new DACChannel_base<ValueT>(this, channel);
        }

        [[nodiscard]] XMC_DAC_t* Handle() const
        {
            return dac_;
        }

    private:
        static ValueT ResolutionMax(uint8_t resolution_bits)
        {
            const uint32_t type_max = std::numeric_limits<ValueT>::max();
            const uint32_t resolution_max = resolution_bits >= 32
                ? std::numeric_limits<uint32_t>::max()
                : (uint32_t{1} << resolution_bits) - 1U;
            return static_cast<ValueT>(
                resolution_max < type_max ? resolution_max : type_max);
        }

        XMC_DAC_t* dac_;
        float reference_voltage_;
        uint8_t max_channels_;
        ValueT max_value_;
    };
} // namespace LowLevelEmbedded

#endif
