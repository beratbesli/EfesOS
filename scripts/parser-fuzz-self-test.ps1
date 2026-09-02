[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

& (Join-Path $PSScriptRoot 'boot-info-self-test.ps1')
& (Join-Path $PSScriptRoot 'e820-self-test.ps1')
& (Join-Path $PSScriptRoot 'elf-self-test.ps1')
& (Join-Path $PSScriptRoot 'fat-self-test.ps1')

Write-Host 'Parser property-fuzz suite passed (boot metadata, E820, ELF and FAT).'
