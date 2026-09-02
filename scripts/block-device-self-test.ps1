[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build'
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
New-Item -ItemType Directory -Force -Path $build | Out-Null
$output = Join-Path $build 'block-device-host-test.exe'
$arguments = @('-std=c11', '-Wall', '-Wextra', '-Werror', '-I',
    (Join-Path $root 'include'), (Join-Path $root 'tests\block_device_host_test.c'),
    (Join-Path $root 'drivers\block_device.c'), '-o', $output)
& $compilerPath @arguments
if ($LASTEXITCODE -ne 0) {
    throw 'Blok aygit host testi derlenemedi.'
}
& $output
if ($LASTEXITCODE -ne 0) {
    throw 'Blok aygit host testi basarisiz.'
}
