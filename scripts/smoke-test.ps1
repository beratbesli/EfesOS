[CmdletBinding()]
param(
    [ValidateRange(1, 120)][int]$TimeoutSeconds = 15,
    [ValidateRange(16, 2048)][int]$MemoryMiB = 128,
    [switch]$SkipBuild,
    [string]$DiskImage = '',
    [string]$Cpu = ''
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build'
$imagePath = Join-Path $buildDirectory 'efesos.img'
$serialLog = Join-Path $buildDirectory 'smoke-serial.log'
$qemuErrorLog = Join-Path $buildDirectory 'smoke-qemu-error.log'
$successMarkers = @(
    'EfesOS: kernel entry reached.',
    'EfesOS: CPU features cpuid=',
    'EfesOS: BIOS E820 entries available.',
    'EfesOS: stage-2 kernel integrity check passed.',
    'EfesOS: PCI devices discovered=',
    'EfesOS: ATA primary-master present=',
    'EfesOS: ATA write path fail-closed self-test passed.',
    'EfesOS: FAT volume mounted=',
    'EfesOS: syscall ABI self-test passed.',
    'EfesOS: interrupt self-tests passed.',
    'EfesOS: VMM self-test passed.',
    'EfesOS: paging mode=',
    'EfesOS: ELF loader validation self-test passed.',
    'EfesOS: ELF loader runtime self-test passed.',
    'EfesOS: kernel heap self-test passed.',
    'EfesOS: RAM filesystem self-test passed.',
    'EfesOS: journal record self-test passed.',
    'EfesOS: bounded IPC queue self-test passed.',
    'EfesOS: user stack guard self-test passed.',
    'EfesOS: user address-space isolation self-test passed.',
    'EfesOS: multiple user process isolation self-test passed.',
    'EfesOS: scheduler priority self-test passed.',
    'EfesOS: ring3 syscall runtime test passed.',
    'EfesOS: user pointer validation runtime test passed.',
    'EfesOS: user process resource cleanup passed.',
    'EfesOS: user process restart and slot reuse passed.',
    'EfesOS: repeated user process cleanup passed.',
    'EfesOS: repeated user restart stress passed.',
    'EfesOS: user address-space switch runtime test passed.',
    'EfesOS: user IPC syscall runtime test passed.',
    'EfesOS: invalid user IPC pointer rejected.',
    'EfesOS: targeted user IPC runtime test passed.',
    'EfesOS: blocking user IPC runtime test passed.',
    'EfesOS: user exit lifecycle runtime test passed.',
    'EfesOS: generation-based user PID runtime test passed.',
    'EfesOS: scheduler stack resource cleanup passed.',
    'EfesOS: scheduler block/wake lifecycle self-test passed.',
    'EfesOS: user exception isolated.',
    'EfesOS: preemptive scheduler runtime test passed.'
)

function Get-QemuPath {
    $command = Get-Command 'qemu-system-i386' -ErrorAction SilentlyContinue
    if ($null -ne $command -and $command.CommandType -eq 'Application' -and
        ![string]::IsNullOrEmpty($command.Source)) {
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
    $successMarkers += 'EfesOS: persistent journal replay passed records=0x00000001.'
}

New-Item -ItemType Directory -Force -Path $buildDirectory | Out-Null
Remove-Item -LiteralPath $serialLog, $qemuErrorLog -Force -ErrorAction SilentlyContinue
if (Test-Path -LiteralPath $serialLog) {
    throw "Eski seri log dosyasi silinemedi: $serialLog"
}
if (Test-Path -LiteralPath $qemuErrorLog) {
    throw "Eski QEMU hata logu silinemedi: $qemuErrorLog"
}

$qemu = Get-QemuPath
$arguments = @(
    '-display', 'none',
    '-monitor', 'none',
    '-serial', 'stdio',
    '-no-reboot',
    '-no-shutdown',
    '-m', $MemoryMiB,
    '-drive', "`"file=$imagePath,format=raw,if=floppy`"",
    '-boot', 'a'
)
if ($Cpu -ne '') {
    $arguments = @('-cpu', $Cpu) + $arguments
}
if ($DiskImage -ne '') {
    $arguments += @('-drive', "`"file=$DiskImage,format=raw,if=ide`"")
}

$process = Start-Process -FilePath $qemu -ArgumentList $arguments -RedirectStandardOutput $serialLog -RedirectStandardError $qemuErrorLog -WindowStyle Hidden -PassThru
$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$passed = $false

try {
    while ([DateTime]::UtcNow -lt $deadline) {
        if (Test-Path -LiteralPath $serialLog) {
            $output = Get-Content -LiteralPath $serialLog -Raw -ErrorAction SilentlyContinue
            $allMarkersFound = $true
            foreach ($marker in $successMarkers) {
                if ([string]::IsNullOrEmpty($output) -or $output.IndexOf([string]$marker) -lt 0) {
                    $allMarkersFound = $false
                    break
                }
            }
            if ($allMarkersFound -and $output.IndexOf('KERNEL PANIC:') -lt 0) {
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
