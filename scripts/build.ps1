[CmdletBinding()]
param(
    [switch]$Run,
    [switch]$AllowLocalTools
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build'
$toolsDirectory = Join-Path $projectRoot 'tools'
$bootSource = Join-Path $projectRoot 'boot\boot.asm'
$stage2Source = Join-Path $projectRoot 'boot\stage2.asm'
$kernelEntrySource = Join-Path $projectRoot 'kernel\kernel_entry.asm'
$kernelMainSource = Join-Path $projectRoot 'kernel\kernel.c'
$panicSource = Join-Path $projectRoot 'kernel\panic.c'
$syscallSource = Join-Path $projectRoot 'kernel\syscall.c'
$ipcSource = Join-Path $projectRoot 'kernel\ipc.c'
$languageSource = Join-Path $projectRoot 'kernel\language.c'
$splashSource = Join-Path $projectRoot 'kernel\splash.c'
$vgaSource = Join-Path $projectRoot 'drivers\vga.c'
$serialSource = Join-Path $projectRoot 'drivers\serial.c'
$keyboardSource = Join-Path $projectRoot 'drivers\keyboard.c'
$pciSource = Join-Path $projectRoot 'drivers\pci.c'
$ataSource = Join-Path $projectRoot 'drivers\ata.c'
$idtSource = Join-Path $projectRoot 'cpu\idt.c'
$pitSource = Join-Path $projectRoot 'cpu\pit.c'
$systemSource = Join-Path $projectRoot 'cpu\system.c'
$interruptSource = Join-Path $projectRoot 'cpu\interrupts.asm'
$pmmSource = Join-Path $projectRoot 'memory\pmm.c'
$pagingSource = Join-Path $projectRoot 'memory\paging.c'
$heapSource = Join-Path $projectRoot 'memory\heap.c'
$schedulerSource = Join-Path $projectRoot 'process\scheduler.c'
$userProcessSource = Join-Path $projectRoot 'process\user_process.c'
$elfLoaderSource = Join-Path $projectRoot 'process\elf_loader.c'
$userDemoSource = Join-Path $projectRoot 'process\user_demo.asm'
$programsSource = Join-Path $projectRoot 'process\programs.c'
$ramfsSource = Join-Path $projectRoot 'fs\ramfs.c'
$fatSource = Join-Path $projectRoot 'fs\fat.c'
$vfsSource = Join-Path $projectRoot 'fs\vfs.c'
$gamesSource = Join-Path $projectRoot 'games\games.c'
$shellSource = Join-Path $projectRoot 'shell\shell.c'
$linkerScript = Join-Path $projectRoot 'kernel\linker.ld'
$includeDirectory = Join-Path $projectRoot 'include'
$bootBinary = Join-Path $buildDirectory 'boot.bin'
$stage2Binary = Join-Path $buildDirectory 'stage2.bin'
$kernelEntryObject = Join-Path $buildDirectory 'kernel_entry.o'
$kernelMainObject = Join-Path $buildDirectory 'kernel.o'
$panicObject = Join-Path $buildDirectory 'panic.o'
$syscallObject = Join-Path $buildDirectory 'syscall.o'
$ipcObject = Join-Path $buildDirectory 'ipc.o'
$languageObject = Join-Path $buildDirectory 'language.o'
$splashObject = Join-Path $buildDirectory 'splash.o'
$vgaObject = Join-Path $buildDirectory 'vga.o'
$serialObject = Join-Path $buildDirectory 'serial.o'
$keyboardObject = Join-Path $buildDirectory 'keyboard.o'
$pciObject = Join-Path $buildDirectory 'pci.o'
$ataObject = Join-Path $buildDirectory 'ata.o'
$idtObject = Join-Path $buildDirectory 'idt.o'
$pitObject = Join-Path $buildDirectory 'pit.o'
$systemObject = Join-Path $buildDirectory 'system.o'
$interruptObject = Join-Path $buildDirectory 'interrupts.o'
$pmmObject = Join-Path $buildDirectory 'pmm.o'
$pagingObject = Join-Path $buildDirectory 'paging.o'
$heapObject = Join-Path $buildDirectory 'heap.o'
$schedulerObject = Join-Path $buildDirectory 'scheduler.o'
$userProcessObject = Join-Path $buildDirectory 'user_process.o'
$elfLoaderObject = Join-Path $buildDirectory 'elf_loader.o'
$userDemoObject = Join-Path $buildDirectory 'user_demo.o'
$programsObject = Join-Path $buildDirectory 'programs.o'
$ramfsObject = Join-Path $buildDirectory 'ramfs.o'
$fatObject = Join-Path $buildDirectory 'fat.o'
$vfsObject = Join-Path $buildDirectory 'vfs.o'
$gamesObject = Join-Path $buildDirectory 'games.o'
$shellObject = Join-Path $buildDirectory 'shell.o'
$kernelElf = Join-Path $buildDirectory 'kernel.elf'
$kernelBinary = Join-Path $buildDirectory 'kernel.bin'
$imagePath = Join-Path $buildDirectory 'efesos.img'
$floppySize = 1440 * 1024
$stage2Sectors = 8
$stage2Size = $stage2Sectors * 512
$kernelLoadLimit = 448 * 1024

function Get-RequiredCommand {
    param([Parameter(Mandatory = $true)][string]$Name)

    $path = Get-OptionalCommand $Name
    if ($null -ne $path) {
        return $path
    }

    throw "Gerekli arac bulunamadi: $Name"
}

function Get-OptionalCommand {
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
        'clang' = @(
            (Join-Path $env:ProgramFiles 'LLVM\bin\clang.exe')
        )
        'ld.lld' = @(
            (Join-Path $env:ProgramFiles 'LLVM\bin\ld.lld.exe')
        )
        'llvm-objcopy' = @(
            (Join-Path $env:ProgramFiles 'LLVM\bin\llvm-objcopy.exe')
        )
    }

    if ($fallbackPaths.ContainsKey($Name)) {
        foreach ($path in $fallbackPaths[$Name]) {
            if (Test-Path -LiteralPath $path) {
                return $path
            }
        }
    }

    if ($AllowLocalTools -and (Test-Path -LiteralPath $toolsDirectory)) {
        $localTool = Get-ChildItem -LiteralPath $toolsDirectory -File -Filter "$Name.exe" -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($null -ne $localTool) {
            Write-Warning "Yerel araç açıkça istendi: $($localTool.FullName). Çalıştırmadan önce kaynağını ve özetini doğrulayın."
            return $localTool.FullName
        }
    }

    return $null
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
$compiler = Get-OptionalCommand 'i686-elf-gcc'
$compilerTargetArguments = @()

if ($null -ne $compiler) {
    $linker = Get-RequiredCommand 'i686-elf-ld'
    $objcopy = Get-RequiredCommand 'i686-elf-objcopy'
    $toolchainName = 'i686-elf-gcc'
} else {
    $compiler = Get-RequiredCommand 'clang'
    $linker = Get-RequiredCommand 'ld.lld'
    $objcopy = Get-RequiredCommand 'llvm-objcopy'
    $compilerTargetArguments = @('--target=i686-none-elf')
    $toolchainName = 'LLVM i686-none-elf'
}

function Invoke-CCompile {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Output,
        [Parameter(Mandatory = $true)][string[]]$Includes,
        [Parameter(Mandatory = $true)][string]$FailureMessage
    )

    [string[]]$arguments = @($compilerTargetArguments) + @(
        '-m32', '-std=c11', '-ffreestanding', '-fno-builtin', '-fno-pic', '-fno-pie',
        '-fno-stack-protector', '-fno-unwind-tables', '-fno-asynchronous-unwind-tables',
        '-mno-mmx', '-mno-sse', '-mno-sse2', '-nostdlib',
        '-Wall', '-Wextra', '-Werror'
    )

    foreach ($include in $Includes) {
        $arguments += @('-I', $include)
    }

    $arguments += @('-c', $Source, '-o', $Output)
    Invoke-Tool -Path $compiler -Arguments $arguments -FailureMessage $FailureMessage
}

New-Item -ItemType Directory -Force -Path $buildDirectory | Out-Null
Write-Host "Toolchain: $toolchainName"

Invoke-Tool -Path $nasm -Arguments @('-w+error', '-f', 'elf32', $kernelEntrySource, '-o', $kernelEntryObject) -FailureMessage 'Kernel entry derlenemedi.'
Invoke-CCompile -Source $kernelMainSource -Output $kernelMainObject -Includes @($includeDirectory, (Join-Path $projectRoot 'cpu'), (Join-Path $projectRoot 'fs'), (Join-Path $projectRoot 'games'), (Join-Path $projectRoot 'kernel'), (Join-Path $projectRoot 'memory'), (Join-Path $projectRoot 'process'), (Join-Path $projectRoot 'shell')) -FailureMessage 'Kernel ana kodu derlenemedi.'
Invoke-CCompile -Source $panicSource -Output $panicObject -Includes @($includeDirectory, (Join-Path $projectRoot 'kernel')) -FailureMessage 'Kernel panic altyapisi derlenemedi.'
Invoke-CCompile -Source $syscallSource -Output $syscallObject -Includes @($includeDirectory, (Join-Path $projectRoot 'cpu'), (Join-Path $projectRoot 'kernel'), (Join-Path $projectRoot 'memory'), (Join-Path $projectRoot 'process')) -FailureMessage 'Syscall katmani derlenemedi.'
Invoke-CCompile -Source $ipcSource -Output $ipcObject -Includes @((Join-Path $projectRoot 'kernel'), (Join-Path $projectRoot 'process')) -FailureMessage 'IPC kuyrugu derlenemedi.'
Invoke-CCompile -Source $languageSource -Output $languageObject -Includes @($includeDirectory) -FailureMessage 'Dil yoneticisi derlenemedi.'
Invoke-CCompile -Source $splashSource -Output $splashObject -Includes @($includeDirectory, (Join-Path $projectRoot 'kernel'), (Join-Path $projectRoot 'shell')) -FailureMessage 'Acilis gorseli derlenemedi.'
Invoke-CCompile -Source $vgaSource -Output $vgaObject -Includes @($includeDirectory) -FailureMessage 'VGA surucusu derlenemedi.'
Invoke-CCompile -Source $serialSource -Output $serialObject -Includes @($includeDirectory, (Join-Path $projectRoot 'cpu')) -FailureMessage 'Seri port surucusu derlenemedi.'
Invoke-CCompile -Source $keyboardSource -Output $keyboardObject -Includes @($includeDirectory, (Join-Path $projectRoot 'cpu'), (Join-Path $projectRoot 'kernel'), (Join-Path $projectRoot 'shell')) -FailureMessage 'Klavye surucusu derlenemedi.'
Invoke-CCompile -Source $pciSource -Output $pciObject -Includes @($includeDirectory, (Join-Path $projectRoot 'cpu')) -FailureMessage 'PCI surucusu derlenemedi.'
Invoke-CCompile -Source $ataSource -Output $ataObject -Includes @($includeDirectory, (Join-Path $projectRoot 'cpu')) -FailureMessage 'ATA surucusu derlenemedi.'
Invoke-CCompile -Source $idtSource -Output $idtObject -Includes @($includeDirectory, (Join-Path $projectRoot 'cpu'), (Join-Path $projectRoot 'kernel'), (Join-Path $projectRoot 'process')) -FailureMessage 'IDT derlenemedi.'
Invoke-CCompile -Source $pitSource -Output $pitObject -Includes @((Join-Path $projectRoot 'cpu'), (Join-Path $projectRoot 'process')) -FailureMessage 'PIT surucusu derlenemedi.'
Invoke-CCompile -Source $systemSource -Output $systemObject -Includes @((Join-Path $projectRoot 'cpu')) -FailureMessage 'Sistem denetimi derlenemedi.'
Invoke-Tool -Path $nasm -Arguments @('-w+error', '-f', 'elf32', $interruptSource, '-o', $interruptObject) -FailureMessage 'Kesme stub derlenemedi.'
Invoke-CCompile -Source $pmmSource -Output $pmmObject -Includes @($includeDirectory, (Join-Path $projectRoot 'memory')) -FailureMessage 'Fiziksel bellek yoneticisi derlenemedi.'
Invoke-CCompile -Source $pagingSource -Output $pagingObject -Includes @($includeDirectory, (Join-Path $projectRoot 'memory')) -FailureMessage 'Paging derlenemedi.'
Invoke-CCompile -Source $heapSource -Output $heapObject -Includes @($includeDirectory, (Join-Path $projectRoot 'kernel'), (Join-Path $projectRoot 'memory')) -FailureMessage 'Kernel heap derlenemedi.'
Invoke-CCompile -Source $schedulerSource -Output $schedulerObject -Includes @($includeDirectory, (Join-Path $projectRoot 'cpu'), (Join-Path $projectRoot 'kernel'), (Join-Path $projectRoot 'memory'), (Join-Path $projectRoot 'process')) -FailureMessage 'Scheduler derlenemedi.'
Invoke-CCompile -Source $userProcessSource -Output $userProcessObject -Includes @($includeDirectory, (Join-Path $projectRoot 'kernel'), (Join-Path $projectRoot 'memory'), (Join-Path $projectRoot 'process')) -FailureMessage 'Kullanici sureci derlenemedi.'
Invoke-CCompile -Source $elfLoaderSource -Output $elfLoaderObject -Includes @($includeDirectory, (Join-Path $projectRoot 'memory'), (Join-Path $projectRoot 'process')) -FailureMessage 'ELF yukleyici derlenemedi.'
Invoke-Tool -Path $nasm -Arguments @('-w+error', '-f', 'elf32', $userDemoSource, '-o', $userDemoObject) -FailureMessage 'Kullanici demo kodu derlenemedi.'
Invoke-CCompile -Source $programsSource -Output $programsObject -Includes @((Join-Path $projectRoot 'process')) -FailureMessage 'Demo programlari derlenemedi.'
Invoke-CCompile -Source $ramfsSource -Output $ramfsObject -Includes @((Join-Path $projectRoot 'fs')) -FailureMessage 'RAM dosya sistemi derlenemedi.'
Invoke-CCompile -Source $fatSource -Output $fatObject -Includes @((Join-Path $projectRoot 'fs')) -FailureMessage 'FAT katmani derlenemedi.'
Invoke-CCompile -Source $vfsSource -Output $vfsObject -Includes @($includeDirectory, (Join-Path $projectRoot 'cpu'), (Join-Path $projectRoot 'fs')) -FailureMessage 'VFS katmani derlenemedi.'
Invoke-CCompile -Source $gamesSource -Output $gamesObject -Includes @($includeDirectory, (Join-Path $projectRoot 'cpu'), (Join-Path $projectRoot 'games')) -FailureMessage 'Oyunlar derlenemedi.'
Invoke-CCompile -Source $shellSource -Output $shellObject -Includes @($includeDirectory, (Join-Path $projectRoot 'cpu'), (Join-Path $projectRoot 'fs'), (Join-Path $projectRoot 'games'), (Join-Path $projectRoot 'memory'), (Join-Path $projectRoot 'process'), (Join-Path $projectRoot 'shell')) -FailureMessage 'Shell derlenemedi.'
Invoke-Tool -Path $linker -Arguments @('-m', 'elf_i386', '-T', $linkerScript, '-o', $kernelElf, $kernelEntryObject, $kernelMainObject, $panicObject, $syscallObject, $ipcObject, $languageObject, $splashObject, $vgaObject, $serialObject, $keyboardObject, $pciObject, $ataObject, $idtObject, $pitObject, $systemObject, $interruptObject, $pmmObject, $pagingObject, $heapObject, $schedulerObject, $userProcessObject, $elfLoaderObject, $userDemoObject, $programsObject, $ramfsObject, $fatObject, $vfsObject, $gamesObject, $shellObject) -FailureMessage 'Kernel baglanamadi.'
Invoke-Tool -Path $objcopy -Arguments @('-O', 'binary', $kernelElf, $kernelBinary) -FailureMessage 'Kernel ikilisi olusturulamadi.'

[byte[]]$kernelRawBytes = [System.IO.File]::ReadAllBytes($kernelBinary)
$kernelSectors = [int][Math]::Ceiling($kernelRawBytes.Length / 512.0)
if ($kernelSectors -lt 1 -or ($kernelSectors * 512) -gt $kernelLoadLimit) {
    throw "Kernel yukleme alani sinirini asiyor: $kernelSectors sektor."
}
if ((1 + $stage2Sectors + $kernelSectors) * 512 -gt $floppySize) {
    throw "Boot, stage-2 ve kernel floppy imajina sigmiyor."
}

[byte[]]$kernelBytes = New-Object byte[] ($kernelSectors * 512)
[System.Array]::Copy($kernelRawBytes, $kernelBytes, $kernelRawBytes.Length)
[System.IO.File]::WriteAllBytes($kernelBinary, $kernelBytes)

[uint64]$kernelChecksum = 0
foreach ($byte in $kernelBytes) {
    $kernelChecksum = ($kernelChecksum + [uint64]$byte) -band 0xFFFFFFFF
}
$kernelChecksumLiteral = '0x{0:X8}' -f [uint32]$kernelChecksum

Invoke-Tool -Path $nasm -Arguments @('-w+error', '-D', "STAGE2_SECTORS=$stage2Sectors", '-f', 'bin', $bootSource, '-o', $bootBinary) -FailureMessage 'Stage-1 bootloader derlenemedi.'
Invoke-Tool -Path $nasm -Arguments @('-w+error', '-D', "STAGE2_SECTORS=$stage2Sectors", '-D', "KERNEL_SECTORS=$kernelSectors", '-D', "KERNEL_CHECKSUM=$kernelChecksumLiteral", '-f', 'bin', $stage2Source, '-o', $stage2Binary) -FailureMessage 'Stage-2 bootloader derlenemedi.'

Assert-FileSize -Path $bootBinary -ExpectedBytes 512
Assert-FileSize -Path $stage2Binary -ExpectedBytes $stage2Size
Assert-FileSize -Path $kernelBinary -ExpectedBytes ($kernelSectors * 512)

[byte[]]$bootBytes = [System.IO.File]::ReadAllBytes($bootBinary)
[byte[]]$stage2Bytes = [System.IO.File]::ReadAllBytes($stage2Binary)
[int]$bootSignatureOffset = 510
if ($bootBytes[$bootSignatureOffset] -ne 0x55 -or $bootBytes[$bootSignatureOffset + 1] -ne 0xAA) {
    throw 'Stage-1 boot imzası 0x55AA değil.'
}
[byte[]]$imageBytes = New-Object byte[] $floppySize
[System.Array]::Copy($bootBytes, 0, $imageBytes, 0, $bootBytes.Length)
[System.Array]::Copy($stage2Bytes, 0, $imageBytes, $bootBytes.Length, $stage2Bytes.Length)
[System.Array]::Copy($kernelBytes, 0, $imageBytes, ($bootBytes.Length + $stage2Bytes.Length), $kernelBytes.Length)
[System.IO.File]::WriteAllBytes($imagePath, $imageBytes)

Assert-FileSize -Path $imagePath -ExpectedBytes $floppySize
Write-Host "Olusturuldu: $imagePath ($kernelSectors kernel sektoru)"

if ($Run) {
    $qemu = Get-RequiredCommand 'qemu-system-i386'
    & $qemu -vga std -drive "file=$imagePath,format=raw,if=floppy" -boot a -no-reboot -no-shutdown
}
