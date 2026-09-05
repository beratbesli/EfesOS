[CmdletBinding()]
param(
    [ValidateRange(5, 120)][int]$TimeoutSeconds = 25,
    [switch]$SkipBuild,
    [switch]$DisableApic
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build'
$osImage = Join-Path $buildDirectory 'efesos.img'
$primaryDisk = Join-Path $buildDirectory 'test-disk.img'
$secondaryDisk = Join-Path $buildDirectory 'test-disk-second.img'
$profileName = if ($DisableApic) { 'polling' } else { 'msi' }
$serialLog = Join-Path $buildDirectory "ahci-multi-device-$profileName-serial.log"
$qemuErrorLog = Join-Path $buildDirectory "ahci-multi-device-$profileName-qemu-error.log"

function Get-QemuPath {
    $command = Get-Command 'qemu-system-i386' -ErrorAction SilentlyContinue
    if ($null -ne $command -and $command.CommandType -eq 'Application' -and
        ![string]::IsNullOrEmpty($command.Source)) {
        return $command.Source
    }
    foreach ($candidate in @(
        (Join-Path $env:ProgramFiles 'qemu\qemu-system-i386.exe'),
        (Join-Path $env:ProgramFiles 'QEMU\qemu-system-i386.exe'))) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }
    throw 'Gerekli arac bulunamadi: qemu-system-i386'
}

if (!$SkipBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1')
    if (!$?) {
        throw 'AHCI coklu aygit testi oncesi derleme basarisiz oldu.'
    }
}
if (!(Test-Path -LiteralPath $primaryDisk)) {
    & (Join-Path $PSScriptRoot 'create-test-disk.ps1')
    if (!$?) {
        throw 'AHCI coklu aygit disk fixture olusturulamadi.'
    }
}
foreach ($required in @($osImage, $primaryDisk)) {
    if (!(Test-Path -LiteralPath $required)) {
        throw "Gerekli test girdisi bulunamadi: $required"
    }
}
Copy-Item -LiteralPath $primaryDisk -Destination $secondaryDisk -Force

Remove-Item -LiteralPath $serialLog, $qemuErrorLog -Force `
    -ErrorAction SilentlyContinue
$arguments = @(
    '-machine', 'q35,hpet=on',
    '-display', 'none',
    '-monitor', 'none',
    '-serial', 'stdio',
    '-no-reboot',
    '-no-shutdown',
    '-m', '128',
    '-drive', "`"file=$osImage,format=raw,if=floppy`"",
    '-boot', 'a',
    '-drive', "`"file=$primaryDisk,format=raw,if=ide,index=0`"",
    '-drive', "`"file=$secondaryDisk,format=raw,if=ide,index=1`""
)
if ($DisableApic) {
    $arguments = @('-cpu', 'qemu32,apic=off') + $arguments
}
$startParameters = @{
    FilePath = Get-QemuPath
    ArgumentList = $arguments
    WorkingDirectory = $projectRoot
    RedirectStandardOutput = $serialLog
    RedirectStandardError = $qemuErrorLog
    PassThru = $true
}
if ([Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT) {
    $startParameters.WindowStyle = 'Hidden'
}
$process = Start-Process @startParameters
$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$passed = $false

try {
    while ([DateTime]::UtcNow -lt $deadline) {
        if (Test-Path -LiteralPath $serialLog) {
            $output = Get-Content -LiteralPath $serialLog -Raw `
                -ErrorAction SilentlyContinue
            if (![string]::IsNullOrEmpty($output)) {
                $completedInterruptState = if ($DisableApic) {
                    'EfesOS: AHCI interrupt completion self-test passed mode=0x00000000 irq-count=0x00000000 polling-fallbacks=0x00000000.'
                } else {
                    'EfesOS: AHCI interrupt completion self-test passed mode=0x00000001 irq-count=0x00000004 polling-fallbacks=0x00000000.'
                }
                $passed =
                    $output.Contains('EfesOS: AHCI controllers discovered=0x00000001 usable-mmio=0x00000001.') -and
                    $output.Contains('EfesOS: AHCI selection controller=0x00000000 probes=0x00000001 failovers=0x00000000 devices=0x00000002.') -and
                    $output.Contains('EfesOS: AHCI disk present=0x00000001 sectors=0x00002000 port=0x00000000') -and
                    $output.Contains('EfesOS: AHCI block devices verified=0x00000002.') -and
                    $output.Contains($completedInterruptState) -and
                    $output.Contains('EfesOS: AHCI FAT volume mounted=0x00000001') -and
                    $output.Contains('EfesOS: repeated user restart stress passed.') -and
                    !$output.Contains('KERNEL PANIC:')
                if ($passed) {
                    break
                }
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
    $serialOutput = if (Test-Path -LiteralPath $serialLog) {
        Get-Content -LiteralPath $serialLog -Raw
    } else { '<no serial output>' }
    $qemuError = if (Test-Path -LiteralPath $qemuErrorLog) {
        Get-Content -LiteralPath $qemuErrorLog -Raw
    } else { '<no qemu error output>' }
    throw "AHCI coklu aygit QEMU testi basarisiz oldu.`nSerial:`n$serialOutput`nQEMU:`n$qemuError"
}

$mode = if ($DisableApic) { 'polling' } else { 'MSI' }
Write-Host "AHCI multi-device QEMU test passed ($mode): two ports verified."
