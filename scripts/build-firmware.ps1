param(
    [string]$EnvironmentScript = $env:PICO_ENV_SCRIPT,
    [string]$HostCompiler = $env:PICO_HOST_CC
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ($EnvironmentScript) {
    if (-not (Test-Path -LiteralPath $EnvironmentScript)) { throw "Environment script not found: $EnvironmentScript" }
    . $EnvironmentScript
}
if (-not $env:PICO_SDK_PATH) {
    throw "PICO_SDK_PATH is not set. Set PICO_ENV_SCRIPT or initialize the SDK environment."
}
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { throw "cmake was not found in PATH." }

function Invoke-Checked([scriptblock]$Command, [string]$Step) {
    & $Command
    if ($LASTEXITCODE -ne 0) { throw "$Step failed with exit code $LASTEXITCODE" }
}

Push-Location $repo
try {
    Invoke-Checked { cmake -S firmware\pico2-drv8835 -B build\firmware-pico2 -G Ninja -DPICO_BOARD=pico2 } "Pico 2 configure"
    Invoke-Checked { cmake --build build\firmware-pico2 --target pico2_gcode_drv8835_xy pico2_gcode_drv8835_xy_test } "Pico 2 build"
    Invoke-Checked { cmake -S firmware\pico2-drv8835 -B build\firmware-pico2w -G Ninja -DPICO_BOARD=pico2_w } "Pico 2 W configure"
    Invoke-Checked { cmake --build build\firmware-pico2w --target pico2w_gcode_drv8835_wifi } "Pico 2 W build"

    if ($HostCompiler) {
        Invoke-Checked { cmake -S firmware\pico2-drv8835\tests -B build\firmware-host-tests -G Ninja "-DCMAKE_C_COMPILER=$HostCompiler" } "Host test configure"
        Invoke-Checked { cmake --build build\firmware-host-tests } "Host test build"
        Invoke-Checked { ctest --test-dir build\firmware-host-tests --output-on-failure } "Host tests"
    }

    $release = Join-Path $repo "manufacturing\v0.1.0\firmware"
    New-Item -ItemType Directory -Force -Path $release | Out-Null
    $outputs = @(
        "build\firmware-pico2\pico2_gcode_drv8835_xy.uf2",
        "build\firmware-pico2\pico2_gcode_drv8835_xy_test.uf2",
        "build\firmware-pico2w\pico2w_gcode_drv8835_wifi.uf2"
    )
    $hashLines = foreach ($output in $outputs) {
        $source = Join-Path $repo $output
        Copy-Item -LiteralPath $source -Destination $release -Force
        $copied = Join-Path $release (Split-Path -Leaf $source)
        "{0}  {1}" -f (Get-FileHash -Algorithm SHA256 -LiteralPath $copied).Hash.ToLowerInvariant(), (Split-Path -Leaf $copied)
    }
    Set-Content -Encoding ASCII -LiteralPath (Join-Path $release "SHA256SUMS.txt") -Value $hashLines
}
finally {
    Pop-Location
}
