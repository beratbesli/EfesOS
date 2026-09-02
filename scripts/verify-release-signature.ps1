[CmdletBinding()]
param(
    [string]$ImagePath = '',
    [string]$PublicKeyPath = '',
    [string]$SignaturePath = ''
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
if ($ImagePath -eq '') {
    $ImagePath = Join-Path $projectRoot 'build\efesos.img'
}
if ($PublicKeyPath -eq '' -or $SignaturePath -eq '') {
    throw 'ImagePath, PublicKeyPath ve SignaturePath zorunludur.'
}
$openssl = Get-Command 'openssl' -ErrorAction SilentlyContinue
if ($null -eq $openssl -or $openssl.CommandType -ne 'Application') {
    throw 'Gerekli arac bulunamadi: openssl'
}
foreach ($path in @($ImagePath, $PublicKeyPath, $SignaturePath)) {
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Dosya bulunamadi: $path"
    }
}
& $openssl.Source dgst -sha256 -verify $PublicKeyPath -signature $SignaturePath $ImagePath
if (!$?) {
    throw 'Release imza dogrulamasi basarisiz.'
}
Write-Host 'Release signature verified.'
