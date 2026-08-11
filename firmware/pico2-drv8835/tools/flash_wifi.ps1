param(
    [string]$Port = "COM6",
    [string]$Picotool = $env:PICOTOOL_EXE,
    [string]$Uf2 = (Join-Path $PSScriptRoot "..\build-wifi\pico2w_gcode_drv8835_wifi.uf2")
)

$ErrorActionPreference = "Stop"
if (-not $Picotool) {
    $command = Get-Command picotool -ErrorAction SilentlyContinue
    if ($command) { $Picotool = $command.Source }
}
if (-not $Picotool -or -not (Test-Path -LiteralPath $Picotool)) {
    throw "picotool was not found. Set PICOTOOL_EXE or -Picotool."
}
if (-not (Test-Path -LiteralPath $Uf2)) {
    throw "UF2 was not found: $Uf2"
}

$device = Get-PnpDevice -Class Ports -PresentOnly |
    Where-Object { $_.FriendlyName -match "\($([regex]::Escape($Port))\)$" } |
    Select-Object -First 1
if (-not $device) { throw "USB serial device $Port was not found." }

$parentId = ($device | Get-PnpDeviceProperty "DEVPKEY_Device_Parent").Data
$serial = ($parentId -split "\\")[-1]
if (-not $serial -or $serial -match "&") {
    throw "Could not determine the Pico serial number from $Port: $parentId"
}

Write-Host "Pico serial: $serial"
Write-Host "UF2: $Uf2"
& $Picotool load -f --ser $serial -v -x $Uf2
if ($LASTEXITCODE -ne 0) { throw "picotool failed with exit code $LASTEXITCODE" }
