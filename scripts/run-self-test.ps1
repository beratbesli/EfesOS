[CmdletBinding()]
param(
    [ValidateRange(1, 120)][int]$TimeoutSeconds = 30,
    [ValidateRange(1024, 65535)][int]$MonitorPort = 4555,
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build'
$imagePath = Join-Path $buildDirectory 'efesos.img'
$testDiskPath = Join-Path $buildDirectory 'test-disk.img'
$serialLog = Join-Path $buildDirectory 'run-serial.log'
$qemuErrorLog = Join-Path $buildDirectory 'run-qemu-error.log'

function Get-QemuPath {
    $command = Get-Command 'qemu-system-i386' -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }
    foreach ($path in @(
        (Join-Path $env:ProgramFiles 'qemu\qemu-system-i386.exe'),
        (Join-Path $env:ProgramFiles 'QEMU\qemu-system-i386.exe')
    )) {
        if (Test-Path -LiteralPath $path) {
            return $path
        }
    }
    throw 'Gerekli arac bulunamadi: qemu-system-i386'
}

if (!$SkipBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1')
    if (!$?) {
        throw 'run self-test oncesi derleme basarisiz oldu.'
    }
}
if (!(Test-Path -LiteralPath $imagePath)) {
    throw "Disk imaji bulunamadi: $imagePath"
}
& (Join-Path $PSScriptRoot 'create-test-disk.ps1') -OutputPath $testDiskPath

New-Item -ItemType Directory -Force -Path $buildDirectory | Out-Null
Remove-Item -LiteralPath $serialLog, $qemuErrorLog -Force -ErrorAction SilentlyContinue

$qemuArgs = @(
    '-display', 'none',
    '-monitor', "tcp:127.0.0.1:$MonitorPort,server,nowait",
    '-serial', "file:$serialLog",
    '-no-reboot',
    '-no-shutdown',
    '-m', '128',
    '-drive', "file=$imagePath,format=raw,if=floppy",
    '-boot', 'a',
    '-drive', "file=$testDiskPath,format=raw,if=ide"
)
$qemu = Get-QemuPath
$process = Start-Process -FilePath $qemu -ArgumentList $qemuArgs -RedirectStandardError $qemuErrorLog -WindowStyle Hidden -PassThru
$deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
$monitor = $null
$stream = $null
$passed = $false

function Read-Serial {
    if (Test-Path -LiteralPath $serialLog) {
        $serialText = Get-Content -LiteralPath $serialLog -Raw -ErrorAction SilentlyContinue
        if ($null -ne $serialText) {
            return $serialText
        }
    }
    return ''
}

function Send-Key([System.Net.Sockets.NetworkStream]$targetStream, [string]$key) {
    $bytes = [Text.Encoding]::ASCII.GetBytes("sendkey $key`r`n")
    $targetStream.Write($bytes, 0, $bytes.Length)
    Start-Sleep -Milliseconds 70
}

try {
    while ([DateTime]::UtcNow -lt $deadline) {
        $serial = Read-Serial
        if ($serial.IndexOf('EfesOS: deferred event loop ready.') -ge 0) {
            break
        }
        if ($process.HasExited) {
            break
        }
        Start-Sleep -Milliseconds 100
        $process.Refresh()
    }
    if ((Read-Serial).IndexOf('EfesOS: deferred event loop ready.') -lt 0) {
        throw 'Shell hazirlik isareti zamaninda gorulmedi.'
    }

    while ($null -eq $monitor -and [DateTime]::UtcNow -lt $deadline) {
        try {
            $monitor = New-Object System.Net.Sockets.TcpClient
            $monitor.Connect('127.0.0.1', $MonitorPort)
            $stream = $monitor.GetStream()
        } catch {
            if ($null -ne $monitor) { $monitor.Dispose() }
            $monitor = $null
            Start-Sleep -Milliseconds 100
        }
    }
    if ($null -eq $stream) {
        throw "QEMU monitor baglantisi kurulamadi: $MonitorPort"
    }

    foreach ($key in @('r','u','n','spc','r','u','n','dot','e','l','f','ret')) {
        Send-Key $stream $key
    }

    while ([DateTime]::UtcNow -lt $deadline) {
        $serial = Read-Serial
        if ($serial.IndexOf('EfesOS: disk ELF run passed.') -ge 0 -and
            $serial.IndexOf('KERNEL PANIC:') -lt 0) {
            $passed = $true
            break
        }
        if ($process.HasExited) {
            break
        }
        Start-Sleep -Milliseconds 100
        $process.Refresh()
    }
} finally {
    if ($null -ne $stream) { $stream.Dispose() }
    if ($null -ne $monitor) { $monitor.Dispose() }
    if (!$process.HasExited) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    }
}

if (!$passed) {
    $serialOutput = Read-Serial
    $qemuError = if (Test-Path -LiteralPath $qemuErrorLog) { Get-Content -LiteralPath $qemuErrorLog -Raw } else { '<no qemu error output>' }
    throw "Disk ELF run self-test basarisiz oldu.`nSerial:`n$serialOutput`nQEMU:`n$qemuError"
}

Write-Host 'Disk ELF run self-test passed.'
