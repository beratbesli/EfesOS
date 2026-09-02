[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$buildScript = Join-Path $PSScriptRoot 'build.ps1'
$imagePath = Join-Path (Split-Path -Parent $PSScriptRoot) 'build\efesos.img'

& $buildScript
if ($LASTEXITCODE -ne 0) {
    throw 'Ilk reproducible build basarisiz oldu.'
}
$firstHash = (Get-FileHash -LiteralPath $imagePath -Algorithm SHA256).Hash

& $buildScript
if ($LASTEXITCODE -ne 0) {
    throw 'Ikinci reproducible build basarisiz oldu.'
}
$secondHash = (Get-FileHash -LiteralPath $imagePath -Algorithm SHA256).Hash

if ($firstHash -ne $secondHash) {
    throw "Build imajlari ayni degil: $firstHash / $secondHash"
}

Write-Host "Reproducible build self-test passed: $firstHash"
