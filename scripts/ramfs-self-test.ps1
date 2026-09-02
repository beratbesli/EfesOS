[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build'
$compiler = Get-Command clang -ErrorAction SilentlyContinue
if ($null -eq $compiler) {
    $fallback = Join-Path $env:ProgramFiles 'LLVM\bin\clang.exe'
    if (!(Test-Path -LiteralPath $fallback)) {
        throw 'clang bulunamadi.'
    }
    $compilerPath = $fallback
} else {
    $compilerPath = $compiler.Source
}
New-Item -ItemType Directory -Force -Path $build | Out-Null
$output = Join-Path $build 'ramfs-host-test.exe'
$arguments = @('-std=c11', '-Wall', '-Wextra', '-Werror', '-I', (Join-Path $root 'fs'),
    (Join-Path $root 'tests\ramfs_host_test.c'), (Join-Path $root 'fs\ramfs.c'), '-o', $output)
& $compilerPath @arguments
if ($LASTEXITCODE -ne 0) {
    throw 'RAMFS host testi derlenemedi.'
}
& $output
if ($LASTEXITCODE -ne 0) {
    throw 'RAMFS host testi basarisiz.'
}
