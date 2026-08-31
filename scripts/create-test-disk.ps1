[CmdletBinding()]
param(
    [string]$OutputPath = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if ($OutputPath -eq '') {
    $OutputPath = Join-Path $projectRoot 'build\test-disk.img'
}
$sectorSize = 512
$sectorCount = 8192
[byte[]]$image = New-Object byte[] ($sectorSize * $sectorCount)

function Set-Word([int]$offset, [int]$value) {
    $image[$offset] = [byte]($value -band 0xFF)
    $image[$offset + 1] = [byte](($value -shr 8) -band 0xFF)
}

function Set-Dword([int]$offset, [int]$value) {
    Set-Word $offset ($value -band 0xFFFF)
    Set-Word ($offset + 2) (($value -shr 16) -band 0xFFFF)
}

function Set-Bytes([int]$offset, [byte[]]$bytes) {
    [Array]::Copy($bytes, 0, $image, $offset, $bytes.Length)
}

Set-Bytes 3 ([Text.Encoding]::ASCII.GetBytes('EFES16  '))
Set-Word 11 512
$image[13] = 1
Set-Word 14 1
$image[16] = 2
Set-Word 17 32
Set-Word 19 $sectorCount
$image[21] = 0xF8
Set-Word 22 32
Set-Word 24 32
Set-Word 26 64
Set-Bytes 54 ([Text.Encoding]::ASCII.GetBytes('FAT16   '))
$image[510] = 0x55
$image[511] = 0xAA

# FAT copies: cluster 0/1 reserved, cluster 2 stores HELLO.TXT.
foreach ($fatStart in @(1, 33)) {
    Set-Bytes ($fatStart * $sectorSize) ([byte[]](0xF8, 0xFF, 0xFF, 0xFF, 0xF8, 0xFF))
}

$rootOffset = 65 * $sectorSize
Set-Bytes $rootOffset ([Text.Encoding]::ASCII.GetBytes('HELLO   TXT'))
$image[$rootOffset + 11] = 0x20
Set-Word ($rootOffset + 26) 2
$contents = [Text.Encoding]::ASCII.GetBytes("EfesOS disk!`r`n")
Set-Dword ($rootOffset + 28) $contents.Length
Set-Bytes (67 * $sectorSize) $contents

$parent = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Force -Path $parent | Out-Null
[IO.File]::WriteAllBytes($OutputPath, $image)
Write-Host "FAT16 test disk: $OutputPath"
