param(
  [switch]$Release
)
# From repo root, configure and build out-of-source into ./build
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path | Split-Path -Parent
$buildDir = Join-Path $root 'build'
if(!(Test-Path $buildDir)){
  New-Item -ItemType Directory -Path $buildDir | Out-Null
}
$cfg = if($Release){'Release'} else {'Debug'}
# Configure if cache missing
if(!(Test-Path (Join-Path $buildDir 'CMakeCache.txt'))){
  cmake -S $root -B $buildDir | Write-Host
}
cmake --build $buildDir --config $cfg | Write-Host
