[CmdletBinding()]
param(
    [string]$KernelPath = ''
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrEmpty($KernelPath)) {
    $KernelPath = Join-Path $projectRoot 'build\kernel.bin'
}
if (!(Test-Path -LiteralPath $KernelPath)) {
    throw "Kernel ikilisi bulunamadi: $KernelPath"
}

$python = Get-Command 'python' -ErrorAction SilentlyContinue
if ($null -eq $python) {
    $python = Get-Command 'python3' -ErrorAction SilentlyContinue
}
if ($null -eq $python) {
    throw 'SHA-256 self-test icin Python bulunamadi.'
}

$kernelHash = (Get-FileHash -LiteralPath $KernelPath -Algorithm SHA256).Hash.ToUpperInvariant()
$wordOutput = & $python.Source (Join-Path $PSScriptRoot 'sha256_words.py') $KernelPath
if ($LASTEXITCODE -ne 0) {
    throw 'SHA-256 kelime yardimcisi basarisiz oldu.'
}
$wordHash = (($wordOutput -split '\s+') | Where-Object { $_ -ne '' } | ForEach-Object { $_.Substring(2) }) -join ''
if ($wordHash -ne $kernelHash) {
    throw "SHA-256 kelime cikisi dosya hash'i ile eslesmedi: $wordHash / $kernelHash"
}

$bytes = [IO.File]::ReadAllBytes($KernelPath)
if ($bytes.Length -eq 0) {
    throw 'Bos kernel ikilisi self-test icin gecersiz.'
}
$mutated = [byte[]]$bytes.Clone()
$mutated[0] = [byte]($mutated[0] -bxor 0x01)
$mutatedPath = Join-Path (Split-Path -Parent $KernelPath) 'kernel.sha256-negative.bin'
try {
    [IO.File]::WriteAllBytes($mutatedPath, $mutated)
    $mutatedHash = (Get-FileHash -LiteralPath $mutatedPath -Algorithm SHA256).Hash.ToUpperInvariant()
    if ($mutatedHash -eq $kernelHash) {
        throw 'Tek baytlik bozulma SHA-256 ozeti degistirmedi.'
    }
}
finally {
    Remove-Item -LiteralPath $mutatedPath -Force -ErrorAction SilentlyContinue
}

Write-Host "SHA-256 self-test passed: digest matches and one-byte corruption is detected."
