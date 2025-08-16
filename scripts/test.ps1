param(
  [switch]$Release
)
# From repo root, run ctest pointing at build dir without changing cwd
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path | Split-Path -Parent
$buildDir = Join-Path $root 'build'
$cfg = if($Release){'Release'} else {'Debug'}
# Ensure build exists
if(!(Test-Path (Join-Path $buildDir 'CMakeCache.txt'))){
  Write-Error "Build directory missing. Run scripts/build.ps1 first."
}
ctest --test-dir $buildDir -C $cfg --output-on-failure | Write-Host
