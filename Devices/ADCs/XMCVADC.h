#pragma once

#include "LLE_ADC.h"
#include "xmc_vadc.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace LowLevelEmbedded
{
    struct XMCVADCChannel
    {
        XMC_VADC_GROUP_t* group;
        uint8_t channel;
        uint8_t result_register;
    };

    template <std::unsigned_integral ValueT>
    class XMCVADC final : public IADC<ValueT>
    {
    public:
        using TriggerConversionCallback =
            bool (*)(const XMCVADCChannel& channel, void* context);

        XMCVADC(
            std::span<const XMCVADCChannel> channels,
            float reference_voltage,
            uint8_t resolution_bits = 12,
            uint32_t max_poll_iterations = 100000,
            TriggerConversionCallback trigger_conversion = nullptr,
            void* callback_context = nullptr)
            : channels_(channels),
              reference_voltage_(reference_voltage),
              max_poll_iterations_(max_poll_iterations),
              trigger_conversion_(trigger_conversion),
              callback_context_(callback_context),
              max_adc_value_(ResolutionMax(resolution_bits))
        {
        }

        ValueT ReadADC(uint8_t channel) override
        {
            last_read_succeeded_ = false;
            if (channel >= channels_.size())
            {
                return 0;
            }

            const XMCVADCChannel& descriptor = channels_[channel];
            if (descriptor.group == nullptr)
            {
                return 0;
            }

            static_cast<void>(XMC_VADC_GROUP_GetDetailedResult(
                descriptor.group,
                descriptor.result_register));

            if (trigger_conversion_ != nullptr)
            {
                if (!trigger_conversion_(descriptor, callback_context_))
                {
                    return 0;
                }
            }
            else
            {
                XMC_VADC_QUEUE_ENTRY_t entry{};
                entry.channel_num = descriptor.channel;
                entry.refill_needed = 0U;
                entry.generate_interrupt = 0U;
                entry.external_trigger = 0U;
                XMC_VADC_GROUP_QueueInsertChannel(descriptor.group, entry);
                XMC_VADC_GROUP_QueueTriggerConversion(descriptor.group);
            }

            uint32_t detailed_result = 0;
            uint32_t poll_count = 0;
            do
            {
                detailed_result = XMC_VADC_GROUP_GetDetailedResult(
                    descriptor.group,
                    descriptor.result_register);
                ++poll_count;
            } while (
                (detailed_result & VADC_G_RES_VF_Msk) == 0U &&
                poll_count < max_poll_iterations_);

            if ((detailed_result & VADC_G_RES_VF_Msk) == 0U)
            {
                return 0;
            }

            uint32_t result = detailed_result & VADC_G_RES_RESULT_Msk;
            result >>= VADC_G_RES_RESULT_Pos;
            if (result > static_cast<uint32_t>(max_adc_value_))
            {
                result = static_cast<uint32_t>(max_adc_value_);
            }
            last_read_succeeded_ = true;
            return static_cast<ValueT>(result);
        }

        ValueT GetMaxADCValue() override
        {
            return max_adc_value_;
        }

        uint8_t GetMaxChannels() override
        {
            return channels_.size() > std::numeric_limits<uint8_t>::max()
                ? std::numeric_limits<uint8_t>::max()
                : static_cast<uint8_t>(channels_.size());
        }

        float ReadVoltage(uint8_t channel) override
        {
            const ValueT result = ReadADC(channel);
            return static_cast<float>(result) * reference_voltage_ /
                static_cast<float>(max_adc_value_);
        }

        IADCChannel<ValueT>* CreateChannelObject(uint8_t channel) override
        {
            return new ADCChannel_base<ValueT>(this, channel);
        }

        [[nodiscard]] bool LastReadSucceeded() const
        {
            return last_read_succeeded_;
        }

        void SetReferenceVoltage(float reference_voltage)
        {
            reference_voltage_ = reference_voltage;
        }

        void SetMaxPollIterations(uint32_t max_poll_iterations)
        {
            max_poll_iterations_ = max_poll_iterations;
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

        std::span<const XMCVADCChannel> channels_;
        float reference_voltage_;
        uint32_t max_poll_iterations_;
        TriggerConversionCallback trigger_conversion_;
        void* callback_context_;
        ValueT max_adc_value_;
        bool last_read_succeeded_ = false;
    };
} // namespace LowLevelEmbedded
