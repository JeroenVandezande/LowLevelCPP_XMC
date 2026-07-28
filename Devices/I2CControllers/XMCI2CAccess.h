#pragma once

#include "LLE_I2C.h"
#include "xmc_i2c.h"

#include <cstddef>
#include <cstdint>

namespace LowLevelEmbedded
{
    enum class XMCI2CStatus
    {
        Success,
        InvalidArgument,
        Nack,
        ArbitrationLost,
        BusError,
        Timeout
    };

    class XMCI2CAccess final : public II2CAccess
    {
    public:
        explicit XMCI2CAccess(
            XMC_USIC_CH_t* channel,
            uint32_t max_poll_iterations = 100000)
            : channel_(channel),
              max_poll_iterations_(max_poll_iterations)
        {
        }

        bool I2C_ReadMethod(
            uint8_t address,
            uint8_t* data,
            size_t length) override
        {
            if (data == nullptr || length == 0)
            {
                return Fail(XMCI2CStatus::InvalidArgument);
            }
            return Read(address, data, length, false);
        }

        bool I2C_WriteMethod(
            uint8_t address,
            uint8_t* data,
            size_t length) override
        {
            if ((data == nullptr && length != 0) || channel_ == nullptr)
            {
                return Fail(XMCI2CStatus::InvalidArgument);
            }
            return Write(address, data, length, true, true);
        }

        bool I2C_ReadWriteMethod(
            uint8_t address,
            uint8_t* data,
            size_t readLength,
            size_t writeLength) override
        {
            if (data == nullptr || readLength == 0)
            {
                return Fail(XMCI2CStatus::InvalidArgument);
            }
            if (!Write(address, data, writeLength, true, false))
            {
                Stop();
                return false;
            }
            return Read(address, data, readLength, true);
        }

        bool I2C_Mem_Read(
            uint8_t address,
            uint8_t memAddress,
            uint8_t memAddSize,
            uint8_t* data,
            size_t readLength) override
        {
            if (memAddSize != 1U || data == nullptr || readLength == 0)
            {
                return Fail(XMCI2CStatus::InvalidArgument);
            }
            if (!Write(address, &memAddress, 1, true, false))
            {
                Stop();
                return false;
            }
            return Read(address, data, readLength, true);
        }

        bool I2C_Mem_Write(
            uint8_t address,
            uint8_t memAddress,
            uint8_t memAddSize,
            uint8_t* data,
            size_t writeLength) override
        {
            if (memAddSize != 1U || (data == nullptr && writeLength != 0))
            {
                return Fail(XMCI2CStatus::InvalidArgument);
            }
            if (!Write(address, &memAddress, 1, true, false))
            {
                Stop();
                return false;
            }
            for (size_t index = 0; index < writeLength; ++index)
            {
                XMC_I2C_CH_MasterTransmit(channel_, data[index]);
                if (!WaitForAck())
                {
                    Stop();
                    return false;
                }
            }
            Stop();
            last_status_ = XMCI2CStatus::Success;
            return true;
        }

        bool I2C_IsDeviceReady(uint8_t address) override
        {
            if (channel_ == nullptr)
            {
                return Fail(XMCI2CStatus::InvalidArgument);
            }
            ClearProtocolFlags();
            XMC_I2C_CH_MasterStart(
                channel_,
                address,
                XMC_I2C_CH_CMD_WRITE);
            const bool acknowledged = WaitForAck();
            Stop();
            if (acknowledged)
            {
                last_status_ = XMCI2CStatus::Success;
            }
            return acknowledged;
        }

        [[nodiscard]] XMC_USIC_CH_t* Handle() const
        {
            return channel_;
        }

        [[nodiscard]] XMCI2CStatus LastStatus() const
        {
            return last_status_;
        }

        void SetMaxPollIterations(uint32_t max_poll_iterations)
        {
            max_poll_iterations_ = max_poll_iterations;
        }

    private:
        static constexpr uint32_t error_flags_ =
            XMC_I2C_CH_STATUS_FLAG_NACK_RECEIVED |
            XMC_I2C_CH_STATUS_FLAG_ARBITRATION_LOST |
            XMC_I2C_CH_STATUS_FLAG_ERROR;

        bool Write(
            uint8_t address,
            const uint8_t* data,
            size_t length,
            bool send_start,
            bool send_stop)
        {
            if (channel_ == nullptr || (data == nullptr && length != 0))
            {
                return Fail(XMCI2CStatus::InvalidArgument);
            }

            if (send_start)
            {
                ClearProtocolFlags();
                XMC_I2C_CH_MasterStart(
                    channel_,
                    address,
                    XMC_I2C_CH_CMD_WRITE);
                if (!WaitForAck())
                {
                    return false;
                }
            }

            for (size_t index = 0; index < length; ++index)
            {
                XMC_I2C_CH_MasterTransmit(channel_, data[index]);
                if (!WaitForAck())
                {
                    return false;
                }
            }

            if (send_stop)
            {
                Stop();
            }
            last_status_ = XMCI2CStatus::Success;
            return true;
        }

        bool Read(
            uint8_t address,
            uint8_t* data,
            size_t length,
            bool repeated_start)
        {
            if (channel_ == nullptr || data == nullptr || length == 0)
            {
                return Fail(XMCI2CStatus::InvalidArgument);
            }

            ClearProtocolFlags();
            if (repeated_start)
            {
                XMC_I2C_CH_MasterRepeatedStart(
                    channel_,
                    address,
                    XMC_I2C_CH_CMD_READ);
            }
            else
            {
                XMC_I2C_CH_MasterStart(
                    channel_,
                    address,
                    XMC_I2C_CH_CMD_READ);
            }
            if (!WaitForAck())
            {
                Stop();
                return false;
            }

            for (size_t index = 0; index < length; ++index)
            {
                if (index + 1U == length)
                {
                    XMC_I2C_CH_MasterReceiveNack(channel_);
                }
                else
                {
                    XMC_I2C_CH_MasterReceiveAck(channel_);
                }
                if (!WaitForReceive())
                {
                    Stop();
                    return false;
                }
                data[index] = XMC_I2C_CH_GetReceivedData(channel_);
            }

            Stop();
            last_status_ = XMCI2CStatus::Success;
            return true;
        }

        bool WaitForAck()
        {
            for (uint32_t count = 0; count < max_poll_iterations_; ++count)
            {
                const uint32_t flags = XMC_I2C_CH_GetStatusFlag(channel_);
                if ((flags & error_flags_) != 0U)
                {
                    return SetProtocolError(flags);
                }
                if ((flags & XMC_I2C_CH_STATUS_FLAG_ACK_RECEIVED) != 0U)
                {
                    XMC_I2C_CH_ClearStatusFlag(
                        channel_,
                        XMC_I2C_CH_STATUS_FLAG_ACK_RECEIVED);
                    return true;
                }
            }
            return Fail(XMCI2CStatus::Timeout);
        }

        bool WaitForReceive()
        {
            constexpr uint32_t receive_flags =
                XMC_I2C_CH_STATUS_FLAG_RECEIVE_INDICATION |
                XMC_I2C_CH_STATUS_FLAG_ALTERNATIVE_RECEIVE_INDICATION;
            for (uint32_t count = 0; count < max_poll_iterations_; ++count)
            {
                const uint32_t flags = XMC_I2C_CH_GetStatusFlag(channel_);
                if ((flags & error_flags_) != 0U)
                {
                    return SetProtocolError(flags);
                }
                if ((flags & receive_flags) != 0U)
                {
                    XMC_I2C_CH_ClearStatusFlag(
                        channel_,
                        flags & receive_flags);
                    return true;
                }
            }
            return Fail(XMCI2CStatus::Timeout);
        }

        bool SetProtocolError(uint32_t flags)
        {
            XMC_I2C_CH_ClearStatusFlag(channel_, flags & error_flags_);
            if ((flags & XMC_I2C_CH_STATUS_FLAG_NACK_RECEIVED) != 0U)
            {
                return Fail(XMCI2CStatus::Nack);
            }
            if ((flags & XMC_I2C_CH_STATUS_FLAG_ARBITRATION_LOST) != 0U)
            {
                return Fail(XMCI2CStatus::ArbitrationLost);
            }
            return Fail(XMCI2CStatus::BusError);
        }

        void ClearProtocolFlags()
        {
            XMC_I2C_CH_ClearStatusFlag(
                channel_,
                XMC_I2C_CH_STATUS_FLAG_ACK_RECEIVED |
                    error_flags_ |
                    XMC_I2C_CH_STATUS_FLAG_RECEIVE_INDICATION |
                    XMC_I2C_CH_STATUS_FLAG_ALTERNATIVE_RECEIVE_INDICATION);
        }

        void Stop()
        {
            if (channel_ != nullptr)
            {
                XMC_I2C_CH_MasterStop(channel_);
            }
        }

        bool Fail(XMCI2CStatus status)
        {
            last_status_ = status;
            return false;
        }

        XMC_USIC_CH_t* channel_;
        uint32_t max_poll_iterations_;
        XMCI2CStatus last_status_ = XMCI2CStatus::Success;
    };
} // namespace LowLevelEmbedded
