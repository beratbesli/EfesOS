[CmdletBinding()]
param(
    [ValidateRange(1, 120)][int]$TimeoutSeconds = 30,
    [ValidateRange(1024, 65535)][int]$MonitorPort = 4555,
    [switch]$SkipBuild,
    [switch]$TestPersistentWrite
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
    if ($null -ne $command -and $command.CommandType -eq 'Application' -and
        ![string]::IsNullOrEmpty($command.Source)) {
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
        $fileStream = $null
        $reader = $null
        try {
            # QEMU keeps the serial file open. FileShare.ReadWrite prevents a
            # transient sharing violation from hiding a marker already flushed.
            $fileStream = [IO.File]::Open($serialLog, [IO.FileMode]::Open,
                [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
            $reader = New-Object IO.StreamReader($fileStream)
            return $reader.ReadToEnd()
        } catch [IO.IOException] {
            return ''
        } finally {
            if ($null -ne $reader) { $reader.Dispose() }
            elseif ($null -ne $fileStream) { $fileStream.Dispose() }
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
        if ($serial.IndexOf('EfesOS: deferred event loop ready.') -ge 0 -and
            $serial.IndexOf('EfesOS: persistent journal replay passed records=0x00000001.') -ge 0) {
            break
        }
        if ($process.HasExited) {
            break
        }
        Start-Sleep -Milliseconds 100
        $process.Refresh()
    }
    $serial = Read-Serial
    if ($serial.IndexOf('EfesOS: deferred event loop ready.') -lt 0) {
        throw 'Shell hazirlik isareti zamaninda gorulmedi.'
    }
    if ($serial.IndexOf('EfesOS: persistent journal replay passed records=0x00000001.') -lt 0) {
        throw 'Persistent journal replay isareti zamaninda gorulmedi.'
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

    if ($TestPersistentWrite) {
        foreach ($key in @('w','r','i','t','e','spc','p','e','r','s','i','s','t','2','spc','d','u','r','a','b','l','e','ret')) {
            Send-Key $stream $key
        }
        while ([DateTime]::UtcNow -lt $deadline) {
            $serial = Read-Serial
            if ($serial.IndexOf('EfesOS: persistent RAMFS write committed.') -ge 0) {
                break
            }
            if ($serial.IndexOf('KERNEL PANIC:') -ge 0 -or $process.HasExited) {
                break
            }
            Start-Sleep -Milliseconds 100
            $process.Refresh()
        }
        # QEMU can keep a write-back cache until the guest is stopped. The
        # final sector check after the finally block is the authoritative proof.
        Start-Sleep -Milliseconds 500
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
    if ($null -ne $stream -and !$process.HasExited) {
        try {
            $quitBytes = [Text.Encoding]::ASCII.GetBytes("quit`r`n")
            $stream.Write($quitBytes, 0, $quitBytes.Length)
            $quitDeadline = [DateTime]::UtcNow.AddSeconds(3)
            while (!$process.HasExited -and [DateTime]::UtcNow -lt $quitDeadline) {
                Start-Sleep -Milliseconds 100
                $process.Refresh()
            }
        } catch {
            # Fall back to forceful cleanup below if the monitor is gone.
        }
    }
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

if ($TestPersistentWrite) {
    [byte[]]$diskBytes = [IO.File]::ReadAllBytes($testDiskPath)
    $sectorSize = 512
    $journalSectors = 65
    $journalStart = ($diskBytes.Length / $sectorSize) - $journalSectors
    $recordOffset = ($journalStart + 2) * $sectorSize
    if ($recordOffset + $sectorSize -gt $diskBytes.Length -or
        [BitConverter]::ToUInt32($diskBytes, $recordOffset) -ne 0x314A5346 -or
        [BitConverter]::ToUInt16($diskBytes, $recordOffset + 6) -ne 1 -or
        [BitConverter]::ToUInt16($diskBytes, $recordOffset + 12) -ne 8 -or
        [BitConverter]::ToUInt16($diskBytes, $recordOffset + 14) -ne 7 -or
        [Text.Encoding]::ASCII.GetString($diskBytes, $recordOffset + 20, 8) -ne 'persist2' -or
        [Text.Encoding]::ASCII.GetString($diskBytes, $recordOffset + 52, 7) -ne 'durable' -or
        [BitConverter]::ToUInt32($diskBytes, $recordOffset + 508) -ne 3235780589) {
        throw 'Persistent journal disk kaydi beklenen formatta degil.'
    }
}

Write-Host 'Disk ELF run self-test passed.'
