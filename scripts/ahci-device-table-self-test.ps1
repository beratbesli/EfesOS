$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $projectRoot 'build'
$testBinary = Join-Path $buildDirectory 'ahci-device-table-host-test.exe'
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
    -I (Join-Path $projectRoot 'drivers') `
    (Join-Path $projectRoot 'tests\ahci_device_table_host_test.c') `
    (Join-Path $projectRoot 'drivers\ahci_device_table.c') `
    (Join-Path $projectRoot 'drivers\ahci_layout.c') `
    (Join-Path $projectRoot 'drivers\block_device.c') `
    -o $testBinary
if ($LASTEXITCODE -ne 0) {
    throw 'AHCI aygit tablosu host testi derlenemedi.'
}
& $testBinary
if ($LASTEXITCODE -ne 0) {
    throw 'AHCI aygit tablosu host testi basarisiz.'
}
