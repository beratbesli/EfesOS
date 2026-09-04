[CmdletBinding()]
param(
    [ValidateRange(1, 30)][int]$TimeoutSeconds = 15
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build'
$sourceImage = Join-Path $buildDirectory 'efesos.img'
$mutatedImage = Join-Path $buildDirectory 'sha256-negative.img'
$serialLog = Join-Path $buildDirectory 'sha256-negative-serial.log'
$qemuErrorLog = Join-Path $buildDirectory 'sha256-negative-qemu-error.log'
$stage2Sectors = 12
$kernelOffset = (1 + $stage2Sectors) * 512

if (!(Test-Path -LiteralPath $sourceImage)) {
    throw "Disk imaji bulunamadi: $sourceImage"
}

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

[byte[]]$bytes = [IO.File]::ReadAllBytes($sourceImage)
if ($kernelOffset -ge $bytes.Length) {
    throw 'Kernel baslangic ofseti disk imajinin disinda.'
}
$bytes[$kernelOffset] = [byte]($bytes[$kernelOffset] -bxor 0x01)
[IO.File]::WriteAllBytes($mutatedImage, $bytes)

$process = $null
try {
    Remove-Item -LiteralPath $serialLog, $qemuErrorLog -Force -ErrorAction SilentlyContinue
    $qemu = Get-QemuPath
    $arguments = @(
        '-display', 'none',
        '-monitor', 'none',
        '-serial', "file:$serialLog",
        '-no-reboot',
        '-no-shutdown',
        '-m', '32',
        '-drive', "file=$mutatedImage,format=raw,if=floppy",
        '-boot', 'a'
    )
    $process = Start-Process -FilePath $qemu -ArgumentList $arguments -RedirectStandardError $qemuErrorLog -WindowStyle Hidden -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline -and !$process.HasExited) {
        Start-Sleep -Milliseconds 100
        $process.Refresh()
    }
    if (!$process.HasExited) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    }
    $serial = if (Test-Path -LiteralPath $serialLog) {
        Get-Content -LiteralPath $serialLog -Raw
    } else {
        ''
    }
    if ($null -eq $serial) {
        $serial = ''
    }
    if ($serial.IndexOf('!') -lt 0 -or $serial.IndexOf('EfesOS: kernel entry reached.') -ge 0) {
        throw "Bozuk kernel imaji stage-2 tarafindan reddedilmedi. Serial: $serial"
    }
    Write-Host 'SHA-256 boot negative test passed: corrupted kernel was rejected before kernel entry.'
}
finally {
    if ($null -ne $process -and !$process.HasExited) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    }
    Remove-Item -LiteralPath $mutatedImage, $serialLog, $qemuErrorLog -Force -ErrorAction SilentlyContinue
}
