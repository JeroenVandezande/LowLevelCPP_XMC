#pragma once

#include "LLE_IOPIN.h"
#include "LLE_SPI.h"
#include "xmc_spi.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace LowLevelEmbedded
{
    enum class XMCSPIStatus
    {
        Success,
        InvalidHandle,
        InvalidChipSelect,
        ModeConfigurationFailed,
        Timeout
    };

    class XMCSPIAccess final : public ISPIAccess
    {
    public:
        using ConfigureModeCallback =
            bool (*)(XMC_USIC_CH_t* channel, SPIMode mode, void* context);

        explicit XMCSPIAccess(
            XMC_USIC_CH_t* channel,
            uint32_t max_poll_iterations = 100000,
            uint8_t receive_fill = 0xFF,
            ConfigureModeCallback configure_mode = nullptr,
            void* callback_context = nullptr)
            : channel_(channel),
              max_poll_iterations_(max_poll_iterations),
              receive_fill_(receive_fill),
              configure_mode_(configure_mode),
              callback_context_(callback_context)
        {
        }

        XMCSPIAccess(
            XMC_USIC_CH_t* channel,
            IOPIN* chip_select,
            uint32_t max_poll_iterations = 100000,
            uint8_t receive_fill = 0xFF,
            ConfigureModeCallback configure_mode = nullptr,
            void* callback_context = nullptr)
            : XMCSPIAccess(
                  channel,
                  max_poll_iterations,
                  receive_fill,
                  configure_mode,
                  callback_context)
        {
            single_chip_select_ = chip_select;
        }

        XMCSPIAccess(
            XMC_USIC_CH_t* channel,
            std::span<IOPIN* const> chip_selects,
            uint32_t max_poll_iterations = 100000,
            uint8_t receive_fill = 0xFF,
            ConfigureModeCallback configure_mode = nullptr,
            void* callback_context = nullptr)
            : XMCSPIAccess(
                  channel,
                  max_poll_iterations,
                  receive_fill,
                  configure_mode,
                  callback_context)
        {
            chip_selects_ = chip_selects;
        }

        void WriteSPI(
            uint8_t* data,
            size_t length,
            uint8_t cs_ID,
            SPIMode mode) override
        {
            if (!BeginTransaction(cs_ID, mode))
            {
                return;
            }

            for (size_t index = 0; index < length; ++index)
            {
                uint8_t ignored = 0;
                if (!TransferByte(data[index], ignored))
                {
                    break;
                }
            }
            EndTransaction();
        }

        void ReadWriteSPI(
            uint8_t* data,
            size_t length,
            uint8_t cs_ID,
            SPIMode mode) override
        {
            if (!BeginTransaction(cs_ID, mode))
            {
                return;
            }

            for (size_t index = 0; index < length; ++index)
            {
                if (!TransferByte(data[index], data[index]))
                {
                    break;
                }
            }
            EndTransaction();
        }

        void WriteThenReadSPI(
            uint8_t* writedata,
            size_t writelength,
            uint8_t* readdata,
            size_t readlength,
            uint8_t cs_ID,
            SPIMode mode) override
        {
            if (!BeginTransaction(cs_ID, mode))
            {
                return;
            }

            for (size_t index = 0; index < writelength; ++index)
            {
                uint8_t ignored = 0;
                if (!TransferByte(writedata[index], ignored))
                {
                    EndTransaction();
                    return;
                }
            }
            for (size_t index = 0; index < readlength; ++index)
            {
                if (!TransferByte(receive_fill_, readdata[index]))
                {
                    break;
                }
            }
            EndTransaction();
        }

        [[nodiscard]] XMC_USIC_CH_t* Handle() const
        {
            return channel_;
        }

        [[nodiscard]] XMCSPIStatus LastStatus() const
        {
            return last_status_;
        }

        void SetChipSelect(IOPIN* chip_select)
        {
            single_chip_select_ = chip_select;
            chip_selects_ = {};
        }

        void SetChipSelects(std::span<IOPIN* const> chip_selects)
        {
            single_chip_select_ = nullptr;
            chip_selects_ = chip_selects;
        }

        void SetModeCallback(
            ConfigureModeCallback configure_mode,
            void* callback_context = nullptr)
        {
            configure_mode_ = configure_mode;
            callback_context_ = callback_context;
        }

        void SetMaxPollIterations(uint32_t max_poll_iterations)
        {
            max_poll_iterations_ = max_poll_iterations;
        }

        void SetReceiveFill(uint8_t receive_fill)
        {
            receive_fill_ = receive_fill;
        }

    private:
        bool BeginTransaction(uint8_t cs_id, SPIMode mode)
        {
            last_status_ = XMCSPIStatus::Success;
            if (channel_ == nullptr)
            {
                last_status_ = XMCSPIStatus::InvalidHandle;
                return false;
            }
            if (configure_mode_ != nullptr &&
                !configure_mode_(channel_, mode, callback_context_))
            {
                last_status_ = XMCSPIStatus::ModeConfigurationFailed;
                return false;
            }

            active_chip_select_ = ResolveChipSelect(cs_id);
            if ((single_chip_select_ != nullptr || !chip_selects_.empty()) &&
                active_chip_select_ == nullptr)
            {
                last_status_ = XMCSPIStatus::InvalidChipSelect;
                return false;
            }
            if (active_chip_select_ != nullptr)
            {
                active_chip_select_->Set();
            }
            return true;
        }

        void EndTransaction()
        {
            if (active_chip_select_ != nullptr)
            {
                active_chip_select_->Clear();
                active_chip_select_ = nullptr;
            }
        }

        bool TransferByte(uint8_t transmit, uint8_t& receive)
        {
            uint32_t poll_count = 0;
            while (XMC_USIC_CH_GetTransmitBufferStatus(channel_) ==
                       XMC_USIC_CH_TBUF_STATUS_BUSY &&
                   poll_count < max_poll_iterations_)
            {
                ++poll_count;
            }
            if (poll_count == max_poll_iterations_)
            {
                last_status_ = XMCSPIStatus::Timeout;
                return false;
            }

            XMC_SPI_CH_Transmit(
                channel_,
                transmit,
                XMC_SPI_CH_MODE_STANDARD);

            poll_count = 0;
            while (XMC_USIC_CH_GetReceiveBufferStatus(channel_) == 0U &&
                   poll_count < max_poll_iterations_)
            {
                ++poll_count;
            }
            if (poll_count == max_poll_iterations_)
            {
                last_status_ = XMCSPIStatus::Timeout;
                return false;
            }

            receive = static_cast<uint8_t>(
                XMC_SPI_CH_GetReceivedData(channel_));
            return true;
        }

        IOPIN* ResolveChipSelect(uint8_t cs_id) const
        {
            if (single_chip_select_ != nullptr)
            {
                return single_chip_select_;
            }
            if (cs_id < chip_selects_.size())
            {
                return chip_selects_[cs_id];
            }
            return nullptr;
        }

        XMC_USIC_CH_t* channel_;
        uint32_t max_poll_iterations_;
        uint8_t receive_fill_;
        IOPIN* single_chip_select_ = nullptr;
        std::span<IOPIN* const> chip_selects_;
        IOPIN* active_chip_select_ = nullptr;
        ConfigureModeCallback configure_mode_;
        void* callback_context_;
        XMCSPIStatus last_status_ = XMCSPIStatus::Success;
    };
} // namespace LowLevelEmbedded
