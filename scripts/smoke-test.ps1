[CmdletBinding()]
param(
    [ValidateRange(1, 120)][int]$TimeoutSeconds = 15,
    [ValidateRange(16, 2048)][int]$MemoryMiB = 128,
    [switch]$SkipBuild,
    [string]$DiskImage = ''
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build'
$imagePath = Join-Path $buildDirectory 'efesos.img'
$serialLog = Join-Path $buildDirectory 'smoke-serial.log'
$qemuErrorLog = Join-Path $buildDirectory 'smoke-qemu-error.log'
$successMarkers = @(
    'EfesOS: kernel entry reached.',
    'EfesOS: BIOS E820 entries available.',
    'EfesOS: PCI devices discovered=',
    'EfesOS: ATA primary-master present=',
    'EfesOS: FAT volume mounted=',
    'EfesOS: syscall ABI self-test passed.',
    'EfesOS: interrupt self-tests passed.',
    'EfesOS: VMM self-test passed.',
    'EfesOS: ELF loader validation self-test passed.',
    'EfesOS: kernel heap self-test passed.',
    'EfesOS: RAM filesystem self-test passed.',
    'EfesOS: ring3 syscall runtime test passed.',
    'EfesOS: user exception isolated.',
    'EfesOS: preemptive scheduler runtime test passed.'
)

function Get-QemuPath {
    $command = Get-Command 'qemu-system-i386' -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    $fallbacks = @(
        (Join-Path $env:ProgramFiles 'qemu\qemu-system-i386.exe'),
        (Join-Path $env:ProgramFiles 'QEMU\qemu-system-i386.exe')
    )
    foreach ($path in $fallbacks) {
        if (Test-Path -LiteralPath $path) {
            return $path
        }
    }

    throw 'Gerekli arac bulunamadi: qemu-system-i386'
}

if (!$SkipBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1')
    if (!$?) {
        throw 'Smoke test oncesi derleme basarisiz oldu.'
    }
}

if (!(Test-Path -LiteralPath $imagePath)) {
    throw "Disk imaji bulunamadi: $imagePath"
}
if ($DiskImage -ne '' -and !(Test-Path -LiteralPath $DiskImage)) {
    throw "Disk imaji bulunamadi: $DiskImage"
}
if ($DiskImage -ne '') {
    $successMarkers += 'EfesOS: FAT directory/file read self-test passed'
}

New-Item -ItemType Directory -Force -Path $buildDirectory | Out-Null
Remove-Item -LiteralPath $serialLog, $qemuErrorLog -Force -ErrorAction SilentlyContinue

$qemu = Get-QemuPath
$arguments = @(
    '-display', 'none',
    '-monitor', 'none',
    '-serial', "`"file:$serialLog`"",
    '-no-reboot',
    '-no-shutdown',
    '-m', $MemoryMiB,
    '-drive', "`"file=$imagePath,format=raw,if=floppy`"",
    '-boot', 'a'
)
if ($DiskImage -ne '') {
    $arguments += @('-drive', "`"file=$DiskImage,format=raw,if=ide`"")
}

$process = Start-Process -FilePath $qemu -ArgumentList $arguments -RedirectStandardError $qemuErrorLog -WindowStyle Hidden -PassThru
$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$passed = $false

try {
    while ([DateTime]::UtcNow -lt $deadline) {
        if (Test-Path -LiteralPath $serialLog) {
            $output = Get-Content -LiteralPath $serialLog -Raw -ErrorAction SilentlyContinue
            $allMarkersFound = $true
            foreach ($marker in $successMarkers) {
                if ($output -notlike "*$marker*") {
                    $allMarkersFound = $false
                    break
                }
            }
            if ($allMarkersFound) {
                $passed = $true
                break
            }
        }

        if ($process.HasExited) {
            break
        }
        Start-Sleep -Milliseconds 200
        $process.Refresh()
    }
} finally {
    if (!$process.HasExited) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    }
}

if (!$passed) {
    $serialOutput = if (Test-Path -LiteralPath $serialLog) { Get-Content -LiteralPath $serialLog -Raw } else { '<no serial output>' }
    $qemuError = if (Test-Path -LiteralPath $qemuErrorLog) { Get-Content -LiteralPath $qemuErrorLog -Raw } else { '<no qemu error output>' }
    throw "QEMU smoke test basarisiz oldu.`nSerial:`n$serialOutput`nQEMU:`n$qemuError"
}

Write-Host "QEMU smoke test passed: all $($successMarkers.Count) boot markers found."
