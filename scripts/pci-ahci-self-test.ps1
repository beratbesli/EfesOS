$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build'
$testSource = Join-Path $projectRoot 'tests\pci_ahci_host_test.c'
$layoutSource = Join-Path $projectRoot 'drivers\pci_layout.c'
$testBinary = Join-Path $buildDirectory 'pci-ahci-host-test.exe'
$compiler = Get-Command 'clang' -ErrorAction SilentlyContinue

if ($null -eq $compiler -or $compiler.CommandType -ne 'Application' -or
    [string]::IsNullOrEmpty($compiler.Source)) {
    $fallback = Join-Path $env:ProgramFiles 'LLVM\bin\clang.exe'
    if (!(Test-Path -LiteralPath $fallback)) {
        throw 'clang bulunamadi.'
    }
    $compilerPath = $fallback
} else {
    $compilerPath = $compiler.Source
}

New-Item -ItemType Directory -Force -Path $buildDirectory | Out-Null
& $compilerPath -std=c11 -Wall -Wextra -Werror `
    -I (Join-Path $projectRoot 'include') `
    $testSource $layoutSource -o $testBinary
if ($LASTEXITCODE -ne 0) {
    throw 'PCI AHCI host testi derlenemedi.'
}
& $testBinary
if ($LASTEXITCODE -ne 0) {
    throw 'PCI AHCI host testi basarisiz.'
}
