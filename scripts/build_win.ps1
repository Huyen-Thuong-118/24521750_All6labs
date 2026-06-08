param([string]$Config = "Release")
$ErrorActionPreference = "Stop"

# Tự detect VCPKG_ROOT nếu chưa set
if (-not $env:VCPKG_ROOT) { $env:VCPKG_ROOT = "C:\vcpkg" }
if (-not (Test-Path $env:VCPKG_ROOT)) {
    Write-Error "VCPKG_ROOT không tìm thấy: $env:VCPKG_ROOT"
    exit 1
}

$root = Split-Path $PSScriptRoot -Parent
$buildDir = "$root\build\win"

Write-Host "Configure..." -ForegroundColor Cyan
cmake -B $buildDir -S $root `
      -G "Visual Studio 17 2022" -A x64 `
      "-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"

Write-Host "Build ($Config)..." -ForegroundColor Cyan
cmake --build $buildDir --config $Config --parallel

Write-Host "Test..." -ForegroundColor Cyan
ctest --test-dir $buildDir -C $Config --output-on-failure

Write-Host "Done." -ForegroundColor Green
