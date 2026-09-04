[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build'
$testSource = Join-Path $projectRoot 'tests\ahci_recovery_state_host_test.c'
$implementationSource = Join-Path $projectRoot 'drivers\ahci_recovery_state.c'
$testBinary = Join-Path $buildDirectory 'ahci-recovery-state-host-test.exe'
$compiler = Get-Command clang -ErrorAction SilentlyContinue

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
    -I (Join-Path $projectRoot 'drivers') `
    $testSource $implementationSource -o $testBinary
if ($LASTEXITCODE -ne 0) {
    throw 'AHCI recovery durum makinesi host testi derlenemedi.'
}
& $testBinary
if ($LASTEXITCODE -ne 0) {
    throw 'AHCI recovery durum makinesi host testi basarisiz.'
}
