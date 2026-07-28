#pragma once

#include "LLE_IOPIN.h"
#include "xmc_gpio.h"

#include <cstdint>

namespace LowLevelEmbedded
{
    class XMCIOPin final : public IOPIN
    {
    public:
        XMCIOPin(XMC_GPIO_PORT_t* port, uint8_t pin)
            : port_(port), pin_(pin)
        {
        }

        void Set() override
        {
            XMC_GPIO_SetOutputHigh(port_, pin_);
        }

        void Clear() override
        {
            XMC_GPIO_SetOutputLow(port_, pin_);
        }

        bool GetValue() override
        {
            return XMC_GPIO_GetInput(port_, pin_) != 0U;
        }

        void Toggle() override
        {
            XMC_GPIO_ToggleOutput(port_, pin_);
        }

        [[nodiscard]] XMC_GPIO_PORT_t* Port() const
        {
            return port_;
        }

        [[nodiscard]] uint8_t Pin() const
        {
            return pin_;
        }

    private:
        XMC_GPIO_PORT_t* port_;
        uint8_t pin_;
    };

    class XMCIOInvertedPin final : public IOPIN
    {
    public:
        XMCIOInvertedPin(XMC_GPIO_PORT_t* port, uint8_t pin)
            : port_(port), pin_(pin)
        {
        }

        void Set() override
        {
            XMC_GPIO_SetOutputLow(port_, pin_);
        }

        void Clear() override
        {
            XMC_GPIO_SetOutputHigh(port_, pin_);
        }

        bool GetValue() override
        {
            return XMC_GPIO_GetInput(port_, pin_) == 0U;
        }

        void Toggle() override
        {
            XMC_GPIO_ToggleOutput(port_, pin_);
        }

        [[nodiscard]] XMC_GPIO_PORT_t* Port() const
        {
            return port_;
        }

        [[nodiscard]] uint8_t Pin() const
        {
            return pin_;
        }

    private:
        XMC_GPIO_PORT_t* port_;
        uint8_t pin_;
    };
} // namespace LowLevelEmbedded
