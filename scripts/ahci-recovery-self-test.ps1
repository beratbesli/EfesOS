[CmdletBinding()]
param(
    [ValidateRange(5, 120)][int]$TimeoutSeconds = 25,
    [switch]$SkipBuild,
    [switch]$PersistentFailure,
    [switch]$DisableHpet
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build'
$osImage = Join-Path $buildDirectory 'efesos.img'
$diskImage = Join-Path $buildDirectory 'test-disk.img'
$serialLog = Join-Path $buildDirectory 'ahci-recovery-serial.log'
$qemuErrorLog = Join-Path $buildDirectory 'ahci-recovery-qemu-error.log'
$faultConfigName = if ($PersistentFailure) {
    'ahci-read-persistent-error.blkdebug'
} else {
    'ahci-read-error.blkdebug'
}
$faultConfig = Join-Path $projectRoot "tests\$faultConfigName"

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
        throw 'AHCI recovery testi oncesi derleme basarisiz oldu.'
    }
}
if (!(Test-Path -LiteralPath $diskImage)) {
    & (Join-Path $PSScriptRoot 'create-test-disk.ps1')
    if (!$?) {
        throw 'AHCI recovery disk fixture olusturulamadi.'
    }
}
foreach ($required in @($osImage, $diskImage, $faultConfig)) {
    if (!(Test-Path -LiteralPath $required)) {
        throw "Gerekli test girdisi bulunamadi: $required"
    }
}

Remove-Item -LiteralPath $serialLog, $qemuErrorLog -Force -ErrorAction SilentlyContinue
$qemu = Get-QemuPath
$faultUri = "blkdebug:tests/$faultConfigName`:build/test-disk.img"
$machine = if ($DisableHpet) { 'q35,hpet=off' } else { 'q35,hpet=on' }
$arguments = @(
    '-machine', $machine,
    '-display', 'none',
    '-monitor', 'none',
    '-serial', 'stdio',
    '-no-reboot',
    '-no-shutdown',
    '-m', '128',
    '-drive', "`"file=$osImage,format=raw,if=floppy`"",
    '-boot', 'a',
    '-drive', "`"file=$faultUri,format=raw,if=ide,rerror=report`""
)
$startParameters = @{
    FilePath = $qemu
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
                if ($PersistentFailure) {
                    $passed =
                        $output.Contains('EfesOS: AHCI read failure fail-closed=0x00000001 present=0x00000000') -and
                        $output.Contains('attempts=0x00000001.') -and
                        $output.Contains('KERNEL PANIC: AHCI read-only block device self-test failed.') -and
                        !$output.Contains('EfesOS: AHCI read path self-test passed.')
                } else {
                    $passed =
                        $output.Contains('EfesOS: AHCI recovery state attempts=0x00000001 completed=0x00000001.') -and
                        $output.Contains('EfesOS: AHCI read path self-test passed.') -and
                        $output.Contains('EfesOS: AHCI FAT volume mounted=0x00000001') -and
                        !$output.Contains('KERNEL PANIC:')
                }
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
    throw "AHCI recovery QEMU testi basarisiz oldu.`nSerial:`n$serialOutput`nQEMU:`n$qemuError"
}

if ($PersistentFailure) {
    Write-Host 'AHCI fail-closed QEMU test passed: persistent read error revoked DMA.'
} else {
    Write-Host 'AHCI recovery QEMU test passed: injected read error recovered once.'
}
