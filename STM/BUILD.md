# STM32 build and flash

This firmware can be built without opening STM32CubeIDE. CubeMX/CubeIDE can still
be used for pin/peripheral generation and debugging, but reproducible firmware
artifacts are produced from the command line.

## Build

From `STM/`:

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Config Debug -Clean
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Config Release -Clean
```

Outputs:

```text
build/cli/Debug/F103First.elf
build/cli/Debug/F103First.hex
build/cli/Debug/F103First.bin
build/cli/Release/F103First.elf
build/cli/Release/F103First.hex
build/cli/Release/F103First.bin
```

`Release` is the normal artifact to flash on the board. `Debug` keeps debug
symbols for debugger sessions.

## Flash

Install STM32CubeProgrammer and connect the board over ST-Link/SWD, then run:

```powershell
powershell -ExecutionPolicy Bypass -File .\flash.ps1 -Config Release -Format hex
```

For binary flashing:

```powershell
powershell -ExecutionPolicy Bypass -File .\flash.ps1 -Config Release -Format bin
```

## Toolchain

Required tools:

- `arm-none-eabi-gcc`
- `arm-none-eabi-objcopy`
- `arm-none-eabi-size`
- `STM32_Programmer_CLI` for flashing

The existing `Makefile` is kept for Linux/CI environments where `make` is
available.
