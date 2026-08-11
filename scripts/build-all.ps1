param(
    [string]$KicadCli = $env:KICAD_CLI,
    [string]$KicadPython = $env:KICAD_PYTHON
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $KicadPython -or -not (Test-Path -LiteralPath $KicadPython)) {
    throw "KiCad 10 Python was not found. Set KICAD_PYTHON or -KicadPython."
}
if (-not $KicadCli) {
    $command = Get-Command kicad-cli -ErrorAction SilentlyContinue
    if ($command) { $KicadCli = $command.Source }
}

Push-Location $repo
try {
    & $KicadPython generator\generate_flex_board.py --preset 50mm `
        --output hardware\flex-50mm\one_axis_stepper.kicad_pcb
    if ($LASTEXITCODE) { throw "Failed to generate the 50 mm flex board." }
    & $KicadPython generator\generate_flex_board.py --preset 4cards --with-art `
        --output hardware\flex-4cards\planar_stepper_flex_4cards.kicad_pcb
    if ($LASTEXITCODE) { throw "Failed to generate the four-card flex board." }
    python -m unittest discover -s generator\tests -v
    if ($LASTEXITCODE) { throw "Generator unit tests failed." }

    $items = @(
        @{ Board="hardware\pico2w-drv8835-controller\pico2w_drv8835_control.kicad_pcb"; Name="pico2w-drv8835-controller"; Bom="hardware\pico2w-drv8835-controller\bom.csv" },
        @{ Board="hardware\drv8835-driver-module\drv8835_driver_module.kicad_pcb"; Name="drv8835-driver-module"; Bom="hardware\drv8835-driver-module\bom.csv" },
        @{ Board="hardware\flex-50mm\one_axis_stepper.kicad_pcb"; Name="flex-50mm"; Bom="" },
        @{ Board="hardware\flex-4cards\planar_stepper_flex_4cards.kicad_pcb"; Name="flex-4cards"; Bom="" }
    )
    foreach ($item in $items) {
        & "$PSScriptRoot\package-manufacturing.ps1" -Board $item.Board -Name $item.Name `
            -Destination "manufacturing\v0.1.0\$($item.Name)" -Bom $item.Bom -KicadCli $KicadCli
    }
}
finally {
    Pop-Location
}
