<#
.SYNOPSIS
    Build, deploy, and optionally run ToneForge.
.DESCRIPTION
    Configures and builds ToneForge with CMake in Release mode (default) and
    copies the compiled binaries to %LOCALAPPDATA%\xPrograms\ToneForge.
#>
[CmdletBinding()]
param(
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release",

    [switch]$Clean,
    [switch]$Run
)

Clear-Host
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = $PSScriptRoot
$buildDir = Join-Path $repoRoot "build"
$deployDir = (Join-Path $env:LOCALAPPDATA "xPrograms\ToneForge")

if (-not (Get-Command "cmake" -ErrorAction SilentlyContinue)) {
    Write-Error "The 'cmake' CLI was not found in PATH."
    exit 1
}

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Path $buildDir -Recurse -Force -ErrorAction SilentlyContinue
}

# Purge every stale CMake cache (main build, FetchContent subbuilds, juceaide) that points at a
# Visual Studio instance that no longer exists (e.g., Community -> BuildTools).
if (Test-Path $buildDir) {
    Get-ChildItem -Path $buildDir -Recurse -Filter CMakeCache.txt -ErrorAction SilentlyContinue | ForEach-Object {
        $cache = $_.FullName
        $genInstLine = Select-String -Path $cache -Pattern '^CMAKE_GENERATOR_INSTANCE:INTERNAL=(.+)$' | Select-Object -First 1
        if ($genInstLine) {
            $cachedInst = $genInstLine.Matches[0].Groups[1].Value.Trim()
            if ($cachedInst -and -not (Test-Path $cachedInst)) {
                Write-Host "Stale Visual Studio instance in $cache ($cachedInst). Resetting..." -ForegroundColor Yellow
                $dir = Split-Path $cache -Parent
                Remove-Item -Path $cache -Force -ErrorAction SilentlyContinue
                $cmakeFilesDir = Join-Path $dir "CMakeFiles"
                if (Test-Path $cmakeFilesDir) {
                    Remove-Item -Path $cmakeFilesDir -Recurse -Force -ErrorAction SilentlyContinue
                }
            }
        }
    }
}
$cacheFile = Join-Path $buildDir "CMakeCache.txt"

if (-not (Test-Path $cacheFile)) {
    Write-Host "Configuring ToneForge with CMake..." -ForegroundColor Cyan
    & cmake -S $repoRoot -B $buildDir
    if ($LASTEXITCODE -ne 0) {
        Write-Error "CMake configure failed with exit code $LASTEXITCODE."
        exit $LASTEXITCODE
    }
}

Write-Host "Building ToneForge ($Configuration)..." -ForegroundColor Cyan
& cmake --build $buildDir --config $Configuration --target ToneForge --parallel
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed with exit code $LASTEXITCODE."
    exit $LASTEXITCODE
}

$outDir = Join-Path $buildDir "bin\$Configuration"
if (-not (Test-Path (Join-Path $outDir "ToneForge.exe"))) {
    $outDir = Join-Path $buildDir "ToneForge_artefacts\$Configuration"
}
if (-not (Test-Path (Join-Path $outDir "ToneForge.exe"))) {
    $outDir = Join-Path $repoRoot "bin"
}
if (-not (Test-Path (Join-Path $outDir "ToneForge.exe"))) {
    Write-Error "Could not locate built ToneForge.exe."
    exit 1
}

if ($Configuration -eq "Release") {
    Get-Process -Name "ToneForge" -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Milliseconds 300

    New-Item -ItemType Directory -Path $deployDir -Force | Out-Null
    Copy-Item -Path "$outDir\*" -Destination $deployDir -Recurse -Force

    # Deploy factory Resources (Models, IRs, Pedals) alongside the executable
    $resSrc = Join-Path $repoRoot "Resources"
    $resDst = Join-Path $deployDir "Resources"
    foreach ($sub in @("Models", "IRs", "Pedals")) {
        $srcSub = Join-Path $resSrc $sub
        if (Test-Path $srcSub) {
            $dstSub = Join-Path $resDst $sub
            New-Item -ItemType Directory -Path $dstSub -Force | Out-Null
            Copy-Item -Path "$srcSub\*" -Destination $dstSub -Force
        }
    }
    Write-Host "Deployed Release binaries and Resources to $deployDir" -ForegroundColor Green
}

if ($Run) {
    $exeToRun = if ($Configuration -eq "Release") { Join-Path $deployDir "ToneForge.exe" } else { Join-Path $outDir "ToneForge.exe" }
    Start-Process -FilePath $exeToRun
}

