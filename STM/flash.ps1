param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",
    [ValidateSet("hex", "bin")]
    [string]$Format = "hex"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = $PSScriptRoot
$FirmwareBase = Join-Path $ProjectRoot "build\cli\$Config\F103First"
$Firmware = "$FirmwareBase.$Format"

if (-not (Test-Path $Firmware)) {
    throw "Firmware not found: $Firmware. Run ./build.ps1 -Config $Config first."
}

$Programmer = Get-Command STM32_Programmer_CLI -ErrorAction SilentlyContinue
if (-not $Programmer) {
    $Candidates = @(
        "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
        "C:\Program Files (x86)\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
    )

    foreach ($Candidate in $Candidates) {
        if (Test-Path $Candidate) {
            $Programmer = @{ Source = $Candidate }
            break
        }
    }
}

if (-not $Programmer) {
    throw "STM32_Programmer_CLI was not found. Install STM32CubeProgrammer or add it to PATH."
}

if ($Format -eq "bin") {
    & $Programmer.Source -c port=SWD -w $Firmware 0x08000000 -v -rst
} else {
    & $Programmer.Source -c port=SWD -w $Firmware -v -rst
}

if ($LASTEXITCODE -ne 0) {
    throw "Flashing failed"
}
