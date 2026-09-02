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
$output = Join-Path $build 'e820-host-test.exe'
$arguments = @('-std=c11', '-Wall', '-Wextra', '-Werror',
    '-I', (Join-Path $root 'include'), '-I', (Join-Path $root 'memory'),
    (Join-Path $root 'tests\e820_host_test.c'), (Join-Path $root 'memory\e820.c'),
    '-o', $output)
& $compilerPath @arguments
if ($LASTEXITCODE -ne 0) {
    throw 'E820 host testi derlenemedi.'
}
& $output
if ($LASTEXITCODE -ne 0) {
    throw 'E820 host testi basarisiz.'
}
