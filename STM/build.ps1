param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$ProjectRoot = $PSScriptRoot
$Target = "F103First"
$BuildDir = Join-Path $ProjectRoot "build\cli\$Config"
$LinkerScript = Join-Path $ProjectRoot "STM32F103C8TX_FLASH.ld"

$Gcc = "arm-none-eabi-gcc"
$Objcopy = "arm-none-eabi-objcopy"
$Size = "arm-none-eabi-size"

foreach ($Tool in @($Gcc, $Objcopy, $Size)) {
    if (-not (Get-Command $Tool -ErrorAction SilentlyContinue)) {
        throw "$Tool was not found in PATH. Install ARM GNU Toolchain or add it to PATH."
    }
}

if ($Clean -and (Test-Path $BuildDir)) {
    Remove-Item -LiteralPath $BuildDir -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$CommonFlags = @(
    "-mcpu=cortex-m3",
    "-mthumb",
    "-DUSE_HAL_DRIVER",
    "-DSTM32F103xB",
    "-ffunction-sections",
    "-fdata-sections",
    "-Wall",
    "-ICore/Inc",
    "-ICore/MyLib",
    "-ICore/Src",
    "-IDrivers/STM32F1xx_HAL_Driver/Inc",
    "-IDrivers/STM32F1xx_HAL_Driver/Inc/Legacy",
    "-IDrivers/CMSIS/Device/ST/STM32F1xx/Include",
    "-IDrivers/CMSIS/Include"
)

if ($Config -eq "Debug") {
    $CommonFlags += @("-Og", "-g3", "-DDEBUG")
} else {
    $CommonFlags += @("-Os", "-g0")
}

# Автоподхват всех .c из этих директорий. Если через CubeMX добавляешь новую
# HAL периферию (например RTC) — соответствующий файл автоматически попадёт
# в билд без правки этого скрипта. Компиляция файлов, чей модуль не включён в
# stm32f1xx_hal_conf.h, безвредна — они дают почти пустой object.
$SourceDirs = @(
    "Core/Src",
    "Core/MyLib",
    "Drivers/STM32F1xx_HAL_Driver/Src"
)

$Sources = @()
Push-Location $ProjectRoot
try {
    foreach ($dir in $SourceDirs) {
        if (-not (Test-Path $dir)) {
            throw "Source directory not found: $dir"
        }
        Get-ChildItem -Path $dir -Filter "*.c" -File | ForEach-Object {
            $Sources += "$dir/$($_.Name)"
        }
    }
}
finally {
    Pop-Location
}

$AsmSources = @(
    "Core/Startup/startup_stm32f103c8tx.s"
)

$Objects = @()

Push-Location $ProjectRoot
try {
    foreach ($Source in $Sources) {
        if (-not (Test-Path $Source)) {
            throw "Source file not found: $Source"
        }

        $ObjectName = ($Source -replace "[\\/]", "_") -replace "\.c$", ".o"
        $ObjectPath = Join-Path $BuildDir $ObjectName
        & $Gcc -c @CommonFlags $Source -o $ObjectPath
        if ($LASTEXITCODE -ne 0) {
            throw "Compilation failed: $Source"
        }
        $Objects += $ObjectPath
    }

    foreach ($Source in $AsmSources) {
        if (-not (Test-Path $Source)) {
            throw "Startup file not found: $Source"
        }

        $ObjectName = ($Source -replace "[\\/]", "_") -replace "\.s$", ".o"
        $ObjectPath = Join-Path $BuildDir $ObjectName
        & $Gcc -x assembler-with-cpp -c @CommonFlags $Source -o $ObjectPath
        if ($LASTEXITCODE -ne 0) {
            throw "Assembly failed: $Source"
        }
        $Objects += $ObjectPath
    }

    $Elf = Join-Path $BuildDir "$Target.elf"
    $Hex = Join-Path $BuildDir "$Target.hex"
    $Bin = Join-Path $BuildDir "$Target.bin"
    $Map = Join-Path $BuildDir "$Target.map"

    $LdFlags = @(
        "-mcpu=cortex-m3",
        "-mthumb",
        "-specs=nano.specs",
        "-T$LinkerScript",
        "-Wl,-Map=$Map,--cref",
        "-Wl,--gc-sections",
        "-u",
        "_printf_float",
        "-lc",
        "-lm",
        "-lnosys"
    )

    & $Gcc @Objects @LdFlags -o $Elf
    if ($LASTEXITCODE -ne 0) {
        throw "Link failed"
    }

    & $Objcopy -O ihex $Elf $Hex
    if ($LASTEXITCODE -ne 0) {
        throw "HEX generation failed"
    }

    & $Objcopy -O binary -S $Elf $Bin
    if ($LASTEXITCODE -ne 0) {
        throw "BIN generation failed"
    }

    & $Size $Elf

    Write-Host ""
    Write-Host "Build complete:"
    Write-Host "  $Elf"
    Write-Host "  $Hex"
    Write-Host "  $Bin"
}
finally {
    Pop-Location
}
