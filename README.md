# LowLevelCPP_XMC

[![CPM.cmake ready](https://img.shields.io/badge/CPM.cmake-ready-blue.svg)](https://github.com/cpm-cmake/CPM.cmake)

Infineon XMC XMCLib adapters for
[LowLevelCPPClasses](https://github.com/JeroenVandezande/LowLevelCPPClasses).
The generic interfaces and device drivers remain in the core package; this
repository contains only XMC-specific implementations.

The initial implementation was validated against the XMCLib 2.1.22 headers
used by the L17 Type B and L18 Type B XMC4800 firmware projects.

## Implementations

| Adapter | XMC1 | XMC4 | Notes |
|---|---:|---:|---|
| `XMCIOPin`, `XMCIOInvertedPin` | Yes | Yes | XMCLib GPIO |
| `XMCSPIAccess` | Yes | Yes | Blocking USIC SPI; `IOPIN` chip selection |
| `XMCI2CAccess` | Yes | Yes | Blocking USIC I2C master |
| `XMCVADC` | Yes | Yes | VADC queue or application trigger callback |
| `XMCDAC` | No | Yes | Compiled only on devices exposing `DAC` |

XMC1 devices do not contain the XMC4000 DAC peripheral, so the DAC adapter is
intentionally XMC4-only. Exact peripheral counts and pin mappings still depend
on the selected MCU.

## Requirements

* C++20
* CMake 3.20 or newer
* LowLevelCPPClasses
* XMCLib and CMSIS headers for the selected XMC device

The application remains responsible for selecting the MCU, initializing clocks
and peripherals, configuring pin routing, and linking the required XMCLib
sources. The adapters accept already initialized XMCLib peripheral handles, so
they work with either DAVE-generated initialization or hand-written XMCLib
configuration.

The blocking SPI adapter currently expects standard USIC transmit and receive
buffers rather than FIFO mode. The I2C adapter follows the left-adjusted address
convention of `II2CAccess` and supports one-byte memory addresses.

## Add with CPM.cmake

```cmake
if(NOT COMMAND CPMAddPackage)
    set(CPM_VERSION 0.43.1)
    file(
        DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_VERSION}/CPM.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/CPM_${CPM_VERSION}.cmake
    )
    include("${CMAKE_CURRENT_BINARY_DIR}/CPM_${CPM_VERSION}.cmake")
endif()

CPMAddPackage("gh:JeroenVandezande/LowLevelCPPClasses@4.0.0")

CPMAddPackage(
    NAME LowLevelCPP_XMC
    GITHUB_REPOSITORY JeroenVandezande/LowLevelCPP_XMC
    GIT_TAG main
)

target_link_libraries(your_target PRIVATE LowLevelCPP::XMC)
```

`LowLevelCPP_XMC` does not have a release tag yet, so this example currently
tracks `main`. The XMC device support package and XMCLib remain
application-provided dependencies.

LowLevelCPPClasses v4 uses UnitsNet-CPP quantities for VADC and DAC voltages.
Pass reference voltages as `ElectricPotential::from_volts(...)`;
`ReadVoltage()` returns an `ElectricPotential`, while raw converter values
remain integer counts.

## Local CMake integration

```cmake
add_subdirectory(path/to/LowLevelCPPClasses)
add_subdirectory(path/to/LowLevelCPP_XMC)

target_link_libraries(your_target PRIVATE LowLevelCPP::XMC)
```

## Usage

```cpp
#include "XMCIOPin.h"
#include "XMCSPIAccess.h"

LowLevelEmbedded::XMCIOInvertedPin display_cs(XMC_GPIO_PORT1, 8);
LowLevelEmbedded::XMCSPIAccess spi(XMC_SPI0_CH0, &display_cs);
```

`IOPIN::Set()` means logically active. Therefore, an `XMCIOInvertedPin` is the
natural chip-select object for a conventional active-low CS signal.

For multiple SPI devices, pass a `std::span<IOPIN* const>`; `cs_ID` indexes that
span. An optional callback can apply `SPIMode` before each transaction.

`XMCVADC` receives a span of logical-to-hardware channel descriptors. By
default it inserts the selected channel into an already configured VADC queue
and triggers conversion. Applications using DAVE measurement groups, scan
sources, or another trigger strategy can supply a trigger callback instead.

## Delay initialization

For a DAVE project using the SYSTIMER APP, initialize the default time base
after clocks and SYSTIMER have started:

```cpp
#include "XMCDelay.h"

LowLevelEmbedded::XMC::InitDelays();
```

When `systimer.h` is present, the no-argument overload uses
`SYSTIMER_GetTickCount`. To use an RTOS or application time base, pass its
uptime and delay providers explicitly:

```cpp
LowLevelEmbedded::XMC::InitDelays(
    ApplicationMillisecondsSinceStartup,
    ApplicationDelayMilliseconds);
```

Passing only the uptime provider installs a blocking millisecond delay based
on that counter. Supplying both callbacks allows an RTOS delay to yield.

This initializes all three shared LowLevelCPPClasses callbacks. Microsecond
delays use the CMSIS DWT cycle counter, with a SysTick fallback for XMC1
Cortex-M0 devices.
