[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$toolsRoot = Join-Path $projectRoot 'tools'
$toolchainRoot = Join-Path $toolsRoot 'i686-elf'
$archivePath = Join-Path $toolsRoot 'i686-elf-tools-windows.zip'
$downloadUrl = 'https://github.com/lordmilko/i686-elf-tools/releases/download/7.1.0/i686-elf-tools-windows.zip'

if (Get-ChildItem -Path $toolchainRoot -Filter 'i686-elf-gcc.exe' -Recurse -ErrorAction SilentlyContinue) {
    Write-Host "Arac zinciri hazir: $toolchainRoot"
    exit 0
}

New-Item -ItemType Directory -Force -Path $toolsRoot | Out-Null
New-Item -ItemType Directory -Force -Path $toolchainRoot | Out-Null
Invoke-WebRequest -Uri $downloadUrl -OutFile $archivePath
Expand-Archive -Path $archivePath -DestinationPath $toolchainRoot -Force
Remove-Item -LiteralPath $archivePath -Force

if (-not (Get-ChildItem -Path $toolchainRoot -Filter 'i686-elf-gcc.exe' -Recurse -ErrorAction SilentlyContinue)) {
    throw 'i686-elf-gcc.exe arsivden cikarilamadi.'
}

Write-Host "Arac zinciri hazir: $toolchainRoot"
