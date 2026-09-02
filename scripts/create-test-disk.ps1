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
$journalRegionSectors = 65
$fatSectorCount = $sectorCount - $journalRegionSectors
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
Set-Word 19 $fatSectorCount
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
    Set-Word (($fatStart * $sectorSize) + 6) 0xFFF8
}

$rootOffset = 65 * $sectorSize
Set-Bytes $rootOffset ([Text.Encoding]::ASCII.GetBytes('HELLO   TXT'))
$image[$rootOffset + 11] = 0x20
Set-Word ($rootOffset + 26) 2
$contents = [Text.Encoding]::ASCII.GetBytes("EfesOS disk!`r`n")
Set-Dword ($rootOffset + 28) $contents.Length
Set-Bytes (67 * $sectorSize) $contents
Set-Bytes ($rootOffset + 32) ([Text.Encoding]::ASCII.GetBytes('EFESOS     '))
$image[$rootOffset + 32 + 11] = 0x08

# A small position-independent ELF probe for the shell's `run NAME` path.
$runImage = New-Object byte[] $sectorSize
function Set-RunWord([int]$offset, [int]$value) {
    $runImage[$offset] = [byte]($value -band 0xFF)
    $runImage[$offset + 1] = [byte](($value -shr 8) -band 0xFF)
}
function Set-RunDword([int]$offset, [int]$value) {
    Set-RunWord $offset ($value -band 0xFFFF)
    Set-RunWord ($offset + 2) (($value -shr 16) -band 0xFFFF)
}
$runMessage = [Text.Encoding]::ASCII.GetBytes("EfesOS: disk ELF run passed.`r`n")
$runCode = [byte[]](
    0xB8, 0x02, 0x00, 0x00, 0x00,
    0xE8, 0x00, 0x00, 0x00, 0x00,
    0x5E,
    0x81, 0xC6, 0x19, 0x00, 0x00, 0x00,
    0x89, 0xF3,
    0xB9, [byte]$runMessage.Length, 0x00, 0x00, 0x00,
    0xCD, 0x80,
    0xB8, 0x08, 0x00, 0x00, 0x00,
    0xCD, 0x80,
    0xEB, 0xFE
)
$runPayload = $runCode + $runMessage
$runFileSize = 116 + $runPayload.Length
$runImage[0] = 0x7F
$runImage[1] = [byte][char]'E'
$runImage[2] = [byte][char]'L'
$runImage[3] = [byte][char]'F'
$runImage[4] = 1
$runImage[5] = 1
$runImage[6] = 1
Set-RunWord 16 2
Set-RunWord 18 3
Set-RunDword 20 1
Set-RunDword 24 0x00400000
Set-RunDword 28 52
Set-RunWord 40 52
Set-RunWord 42 32
Set-RunWord 44 1
Set-RunDword 52 1
Set-RunDword 56 116
Set-RunDword 60 0x00400000
Set-RunDword 68 $runPayload.Length
Set-RunDword 72 4096
Set-RunDword 76 1
Set-RunDword 80 1
[Array]::Copy($runPayload, 0, $runImage, 116, $runPayload.Length)
$runEntryOffset = $rootOffset + 64
Set-Bytes $runEntryOffset ([Text.Encoding]::ASCII.GetBytes('RUN     ELF'))
$image[$runEntryOffset + 11] = 0x20
Set-Word ($runEntryOffset + 26) 3
Set-Dword ($runEntryOffset + 28) $runFileSize
Set-Bytes (68 * $sectorSize) $runImage

# Reserve the final 65 sectors for the read-only journal replay fixture. The
# FAT volume ends before this region, so future FAT allocation cannot overlap it.
function Get-Crc32([byte[]]$bytes, [int]$offset, [int]$length) {
    [uint32]$crc = [uint32]::MaxValue
    for ($index = 0; $index -lt $length; $index++) {
        $crc = $crc -bxor $bytes[$offset + $index]
        for ($bit = 0; $bit -lt 8; $bit++) {
            if (($crc -band 1) -ne 0) {
                $crc = ($crc -shr 1) -bxor 0xEDB88320
            } else {
                $crc = $crc -shr 1
            }
        }
    }
    return [uint32]($crc -bxor 0xFFFFFFFF)
}

$journalStartSector = $sectorCount - $journalRegionSectors
$journalSuperblockOffset = $journalStartSector * $sectorSize
Set-Dword $journalSuperblockOffset 0x314A5346
Set-Word ($journalSuperblockOffset + 4) 1
Set-Word ($journalSuperblockOffset + 6) ($journalRegionSectors - 1)
Set-Dword ($journalSuperblockOffset + 8) 0
Set-Dword ($journalSuperblockOffset + 12) (Get-Crc32 $image $journalSuperblockOffset 12)
Set-Dword ($journalSuperblockOffset + 508) 0xC0DE17ED

$journalRecordOffset = ($journalStartSector + 1) * $sectorSize
Set-Dword $journalRecordOffset 0x314A5346
Set-Word ($journalRecordOffset + 4) 1
Set-Word ($journalRecordOffset + 6) 1
Set-Dword ($journalRecordOffset + 8) 1
$journalName = [Text.Encoding]::ASCII.GetBytes('PERSIST')
$journalContent = [Text.Encoding]::ASCII.GetBytes("Journal replay!`r`n")
Set-Word ($journalRecordOffset + 12) $journalName.Length
Set-Word ($journalRecordOffset + 14) $journalContent.Length
Set-Bytes ($journalRecordOffset + 20) $journalName
Set-Bytes ($journalRecordOffset + 52) $journalContent
# The record CRC covers bytes 0..15 and the fixed 288-byte name/content area.
[uint32]$recordCrc = [uint32]::MaxValue
for ($part = 0; $part -lt 2; $part++) {
    $crcOffset = if ($part -eq 0) { $journalRecordOffset } else { $journalRecordOffset + 20 }
    $crcLength = if ($part -eq 0) { 16 } else { 288 }
    for ($index = 0; $index -lt $crcLength; $index++) {
        $recordCrc = $recordCrc -bxor $image[$crcOffset + $index]
        for ($bit = 0; $bit -lt 8; $bit++) {
            if (($recordCrc -band 1) -ne 0) {
                $recordCrc = ($recordCrc -shr 1) -bxor 0xEDB88320
            } else {
                $recordCrc = $recordCrc -shr 1
            }
        }
    }
}
Set-Dword ($journalRecordOffset + 16) ([uint32]($recordCrc -bxor 0xFFFFFFFF))
Set-Dword ($journalRecordOffset + 508) 0xC0DE17ED

$parent = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Force -Path $parent | Out-Null
[IO.File]::WriteAllBytes($OutputPath, $image)
Write-Host "FAT16 test disk: $OutputPath"
