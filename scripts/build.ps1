[CmdletBinding()]
param(
    [switch]$Run
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build'
$toolsDirectory = Join-Path $projectRoot 'tools'
$bootSource = Join-Path $projectRoot 'boot\boot.asm'
$kernelEntrySource = Join-Path $projectRoot 'kernel\kernel_entry.asm'
$vgaSource = Join-Path $projectRoot 'drivers\vga.c'
$idtSource = Join-Path $projectRoot 'cpu\idt.c'
$interruptSource = Join-Path $projectRoot 'cpu\interrupts.asm'
$linkerScript = Join-Path $projectRoot 'kernel\linker.ld'
$includeDirectory = Join-Path $projectRoot 'include'
$bootBinary = Join-Path $buildDirectory 'boot.bin'
$kernelEntryObject = Join-Path $buildDirectory 'kernel_entry.o'
$vgaObject = Join-Path $buildDirectory 'vga.o'
$idtObject = Join-Path $buildDirectory 'idt.o'
$interruptObject = Join-Path $buildDirectory 'interrupts.o'
$kernelElf = Join-Path $buildDirectory 'kernel.elf'
$kernelBinary = Join-Path $buildDirectory 'kernel.bin'
$imagePath = Join-Path $buildDirectory 'beeros.img'

function Get-RequiredCommand {
    param([Parameter(Mandatory = $true)][string]$Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    $fallbackPaths = @{
        'nasm' = @(
            (Join-Path $env:ProgramFiles 'NASM\nasm.exe'),
            (Join-Path ${env:ProgramFiles(x86)} 'NASM\nasm.exe')
        )
        'qemu-system-i386' = @(
            (Join-Path $env:ProgramFiles 'qemu\qemu-system-i386.exe'),
            (Join-Path $env:ProgramFiles 'QEMU\qemu-system-i386.exe')
        )
    }

    foreach ($path in $fallbackPaths[$Name]) {
        if (Test-Path -LiteralPath $path) {
            return $path
        }
    }

    $localTool = Get-ChildItem -Path $toolsDirectory -Filter "$Name.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -ne $localTool) {
        return $localTool.FullName
    }

    throw "Gerekli arac bulunamadi: $Name"
}

function Invoke-Tool {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string[]]$Arguments,
        [Parameter(Mandatory = $true)][string]$FailureMessage
    )

    & $Path @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw $FailureMessage
    }
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
$gcc = Get-RequiredCommand 'i686-elf-gcc'
$ld = Get-RequiredCommand 'i686-elf-ld'
$objcopy = Get-RequiredCommand 'i686-elf-objcopy'

New-Item -ItemType Directory -Force -Path $buildDirectory | Out-Null

Invoke-Tool -Path $nasm -Arguments @('-f', 'elf32', $kernelEntrySource, '-o', $kernelEntryObject) -FailureMessage 'Kernel entry derlenemedi.'
Invoke-Tool -Path $gcc -Arguments @('-m32', '-std=c11', '-ffreestanding', '-fno-pic', '-fno-pie', '-fno-stack-protector', '-nostdlib', '-nostartfiles', '-nodefaultlibs', '-Wall', '-Wextra', '-I', $includeDirectory, '-c', $vgaSource, '-o', $vgaObject) -FailureMessage 'VGA surucusu derlenemedi.'
Invoke-Tool -Path $gcc -Arguments @('-m32', '-std=c11', '-ffreestanding', '-fno-pic', '-fno-pie', '-fno-stack-protector', '-nostdlib', '-nostartfiles', '-nodefaultlibs', '-Wall', '-Wextra', '-I', $includeDirectory, '-I', (Join-Path $projectRoot 'cpu'), '-c', $idtSource, '-o', $idtObject) -FailureMessage 'IDT derlenemedi.'
Invoke-Tool -Path $nasm -Arguments @('-f', 'elf32', $interruptSource, '-o', $interruptObject) -FailureMessage 'Kesme stub derlenemedi.'
Invoke-Tool -Path $ld -Arguments @('-m', 'elf_i386', '-T', $linkerScript, '-o', $kernelElf, $kernelEntryObject, $vgaObject, $idtObject, $interruptObject) -FailureMessage 'Kernel baglanamadi.'
Invoke-Tool -Path $objcopy -Arguments @('-O', 'binary', $kernelElf, $kernelBinary) -FailureMessage 'Kernel ikilisi olusturulamadi.'

[byte[]]$kernelRawBytes = [System.IO.File]::ReadAllBytes($kernelBinary)
$kernelSectors = [int][Math]::Ceiling($kernelRawBytes.Length / 512.0)
if ($kernelSectors -lt 1 -or $kernelSectors -gt 17) {
    throw "Kernel $kernelSectors sektor gerektiriyor; stage-1 loader en fazla 17 sektor okuyabilir."
}

[byte[]]$kernelBytes = New-Object byte[] ($kernelSectors * 512)
[System.Array]::Copy($kernelRawBytes, $kernelBytes, $kernelRawBytes.Length)
[System.IO.File]::WriteAllBytes($kernelBinary, $kernelBytes)

Invoke-Tool -Path $nasm -Arguments @('-D', "KERNEL_SECTORS=$kernelSectors", '-f', 'bin', $bootSource, '-o', $bootBinary) -FailureMessage 'Bootloader derlenemedi.'

Assert-FileSize -Path $bootBinary -ExpectedBytes 512
Assert-FileSize -Path $kernelBinary -ExpectedBytes ($kernelSectors * 512)

[byte[]]$bootBytes = [System.IO.File]::ReadAllBytes($bootBinary)
[byte[]]$imageBytes = New-Object byte[] ($bootBytes.Length + $kernelBytes.Length)
[System.Array]::Copy($bootBytes, 0, $imageBytes, 0, $bootBytes.Length)
[System.Array]::Copy($kernelBytes, 0, $imageBytes, $bootBytes.Length, $kernelBytes.Length)
[System.IO.File]::WriteAllBytes($imagePath, $imageBytes)

Assert-FileSize -Path $imagePath -ExpectedBytes (512 + ($kernelSectors * 512))
Write-Host "Olusturuldu: $imagePath ($kernelSectors kernel sektoru)"

if ($Run) {
    $qemu = Get-RequiredCommand 'qemu-system-i386'
    & $qemu -drive "file=$imagePath,format=raw,if=floppy" -boot a -no-reboot -no-shutdown
}
