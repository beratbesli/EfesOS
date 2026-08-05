[CmdletBinding()]
param(
    [switch]$Run
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build'
$bootSource = Join-Path $projectRoot 'boot\boot.asm'
$kernelSource = Join-Path $projectRoot 'kernel\kernel_entry.asm'
$bootBinary = Join-Path $buildDirectory 'boot.bin'
$kernelBinary = Join-Path $buildDirectory 'kernel.bin'
$imagePath = Join-Path $buildDirectory 'beeros.img'

function Get-RequiredCommand {
    param([Parameter(Mandatory = $true)][string]$Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    # Chocolatey'nin NASM ve QEMU paketleri bazı Windows kurulumlarında shim
    # üretmez. Bu nedenle yaygın kurulum dizinlerini de güvenli biçimde tara.
    $fallbackPaths = @{
        'nasm' = @(
            (Join-Path $env:ProgramFiles 'NASM\\nasm.exe'),
            (Join-Path ${env:ProgramFiles(x86)} 'NASM\\nasm.exe')
        )
        'qemu-system-i386' = @(
            (Join-Path $env:ProgramFiles 'qemu\\qemu-system-i386.exe'),
            (Join-Path $env:ProgramFiles 'QEMU\\qemu-system-i386.exe')
        )
    }

    foreach ($path in $fallbackPaths[$Name]) {
        if (Test-Path -LiteralPath $path) {
            return $path
        }
    }

    throw "Gerekli arac bulunamadi: $Name. PATH'e ekleyin veya Chocolatey ile kurun."
}

function Assert-FileSize {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][int]$ExpectedBytes
    )

    $actualBytes = (Get-Item -LiteralPath $Path).Length
    if ($actualBytes -ne $ExpectedBytes) {
        throw "$Path beklenen $ExpectedBytes bayt yerine $actualBytes bayt oldu."
    }
}

$nasm = Get-RequiredCommand 'nasm'
New-Item -ItemType Directory -Force -Path $buildDirectory | Out-Null

& $nasm -f bin $bootSource -o $bootBinary
if ($LASTEXITCODE -ne 0) { throw 'Bootloader derlenemedi.' }

& $nasm -f bin $kernelSource -o $kernelBinary
if ($LASTEXITCODE -ne 0) { throw 'Kernel entry derlenemedi.' }

Assert-FileSize -Path $bootBinary -ExpectedBytes 512
Assert-FileSize -Path $kernelBinary -ExpectedBytes 512

[byte[]]$bootBytes = [System.IO.File]::ReadAllBytes($bootBinary)
[byte[]]$kernelBytes = [System.IO.File]::ReadAllBytes($kernelBinary)
[byte[]]$imageBytes = New-Object byte[] ($bootBytes.Length + $kernelBytes.Length)
[System.Array]::Copy($bootBytes, 0, $imageBytes, 0, $bootBytes.Length)
[System.Array]::Copy($kernelBytes, 0, $imageBytes, $bootBytes.Length, $kernelBytes.Length)
[System.IO.File]::WriteAllBytes($imagePath, $imageBytes)

Assert-FileSize -Path $imagePath -ExpectedBytes 1024
Write-Host "Olusturuldu: $imagePath"

if ($Run) {
    $qemu = Get-RequiredCommand 'qemu-system-i386'
    & $qemu -drive "file=$imagePath,format=raw,if=floppy" -boot a -no-reboot -no-shutdown
}
