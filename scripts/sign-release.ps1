[CmdletBinding()]
param(
    [string]$ImagePath = '',
    [string]$PrivateKeyPath = '',
    [string]$SignaturePath = ''
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
if ($ImagePath -eq '') {
    $ImagePath = Join-Path $projectRoot 'build\efesos.img'
}
if ($PrivateKeyPath -eq '' -or $SignaturePath -eq '') {
    throw 'ImagePath, PrivateKeyPath ve SignaturePath zorunludur.'
}
$openssl = Get-Command 'openssl' -ErrorAction SilentlyContinue
if ($null -eq $openssl -or $openssl.CommandType -ne 'Application') {
    throw 'Gerekli arac bulunamadi: openssl'
}
foreach ($path in @($ImagePath, $PrivateKeyPath)) {
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Dosya bulunamadi: $path"
    }
}
$signatureParent = Split-Path -Parent $SignaturePath
if ($signatureParent -ne '') {
    New-Item -ItemType Directory -Force -Path $signatureParent | Out-Null
}
& $openssl.Source dgst -sha256 -sign $PrivateKeyPath -out $SignaturePath $ImagePath
if (!$?) {
    throw 'Release imzasi olusturulamadi.'
}
Write-Host "Release signature written: $SignaturePath"
