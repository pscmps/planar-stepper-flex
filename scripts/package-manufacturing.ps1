param(
    [Parameter(Mandatory = $true)][string]$Board,
    [Parameter(Mandatory = $true)][string]$Name,
    [Parameter(Mandatory = $true)][string]$Destination,
    [string]$Bom,
    [string]$KicadCli = $env:KICAD_CLI
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

if (-not $KicadCli) {
    $command = Get-Command kicad-cli -ErrorAction SilentlyContinue
    if ($command) { $KicadCli = $command.Source }
}
if (-not $KicadCli -or -not (Test-Path -LiteralPath $KicadCli)) {
    throw "KiCad 10 CLI was not found. Set KICAD_CLI or -KicadCli."
}

$boardPath = (Resolve-Path -LiteralPath (Join-Path $repo $Board)).Path
$destinationPath = [System.IO.Path]::GetFullPath((Join-Path $repo $Destination))
$buildRoot = [System.IO.Path]::GetFullPath((Join-Path $repo "build\manufacturing"))
$work = Join-Path $buildRoot $Name
$gerbers = Join-Path $work "gerbers"

if (-not $work.StartsWith($buildRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "The work directory must remain under the repository build directory."
}
if (Test-Path -LiteralPath $work) {
    Remove-Item -LiteralPath $work -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $gerbers, $destinationPath | Out-Null

function Assert-NativeSuccess([string]$Step) {
    if ($LASTEXITCODE -ne 0) { throw "$Step failed with exit code $LASTEXITCODE" }
}

$drc = Join-Path $destinationPath "drc.rpt"
& $KicadCli pcb drc --all-track-errors --severity-all --format report `
    --output $drc $boardPath
Assert-NativeSuccess "DRC"
$drcText = Get-Content -LiteralPath $drc -Raw -Encoding UTF8
if ($drcText -notmatch "Found 0 DRC violations" -or
    $drcText -notmatch "Found 0 unconnected pads") {
    throw "DRC violations or unconnected pads were found: $drc"
}

& $KicadCli pcb render --output (Join-Path $destinationPath "front.png") `
    --width 1800 --height 1200 --side top --quality high --background opaque $boardPath
Assert-NativeSuccess "Front render"
& $KicadCli pcb render --output (Join-Path $destinationPath "back.png") `
    --width 1800 --height 1200 --side bottom --quality high --background opaque $boardPath
Assert-NativeSuccess "Back render"

& $KicadCli pcb export gerbers --check-zones --output $gerbers `
    --layers "F.Cu,B.Cu,F.Mask,B.Mask,F.Silkscreen,B.Silkscreen,Edge.Cuts" $boardPath
Assert-NativeSuccess "Gerber export"
& $KicadCli pcb export drill --output $gerbers --format excellon `
    --excellon-units mm --excellon-separate-th --generate-report `
    --report-path (Join-Path $gerbers "drill-report.txt") $boardPath
Assert-NativeSuccess "Drill export"

$zip = Join-Path $destinationPath "$Name-gerbers.zip"
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
Compress-Archive -Path (Join-Path $gerbers "*") -DestinationPath $zip -CompressionLevel Optimal

if ($Bom) {
    $bomPath = (Resolve-Path -LiteralPath (Join-Path $repo $Bom)).Path
    Copy-Item -LiteralPath $bomPath -Destination (Join-Path $destinationPath "bom.csv") -Force
}
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zip).Hash.ToLowerInvariant()
Set-Content -Encoding ASCII -LiteralPath (Join-Path $destinationPath "SHA256SUMS.txt") `
    -Value "$hash  $(Split-Path -Leaf $zip)"
Write-Host "Packaged: $zip"
