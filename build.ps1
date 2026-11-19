param(
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

# Locate CMake
$cmake = Get-Command "cmake" -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -ErrorAction SilentlyContinue

if (-not $cmake) {
    # Check Visual Studio bundled CMake installations
    $vsPaths = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\*\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\*\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\*\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\*\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    )

    foreach ($pattern in $vsPaths) {
        $match = Resolve-Path $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($match) {
            $cmake = $match.Path
            break
        }
    }
}

if (-not $cmake) {
    Write-Error "CMake was not found in PATH or Visual Studio installations."
    exit 1
}

Write-Host "Using CMake: $cmake" -ForegroundColor Cyan
Write-Host "Configuring ToneForge ($Config)..." -ForegroundColor Cyan

& $cmake -B build
if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed."
    exit $LASTEXITCODE
}

Write-Host "Building ToneForge ($Config)..." -ForegroundColor Cyan

& $cmake --build build --config $Config --target ToneForge -- /nodeReuse:false
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed."
    exit $LASTEXITCODE
}

Write-Host "Build complete! Executable: bin\ToneForge.exe (also in build\bin\$Config\ToneForge.exe)" -ForegroundColor Green
