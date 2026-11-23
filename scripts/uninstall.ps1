param([string]$Manifest = (Join-Path $PSScriptRoot 'install-state.json'))
$ErrorActionPreference = 'Stop'
$state = Get-Content -LiteralPath $Manifest -Raw | ConvertFrom-Json
if ($state.project -ne 'vmd-cndb') { throw 'Unexpected install manifest.' }
$expectedNative = Join-Path $state.vmd_directory 'plugins/WIN64/molfile/vmdcndb.so'
$expectedRegistration = Join-Path $state.vmd_directory 'scripts/init.d/vmdcndb.tcl'
$expectedPackage = Join-Path $state.vmd_directory 'plugins/noarch/tcl/vmdcndb0.2.1'
if ($state.package_directory -ne $expectedPackage -or [IO.Path]::GetFullPath($PSScriptRoot) -ne [IO.Path]::GetFullPath($expectedPackage)) { throw 'Run the uninstaller from its installed package directory.' }
if ($state.native -ne $expectedNative -or $state.registration -ne $expectedRegistration) { throw 'Unexpected plugin paths in manifest.' }
foreach ($entry in @(@{path=$state.registration; hash=$state.registration_sha256},@{path=$state.native;hash=$state.native_sha256})) {
    if (Test-Path -LiteralPath $entry.path) {
        if ((Get-FileHash -LiteralPath $entry.path -Algorithm SHA256).Hash -ne $entry.hash) { throw "File was changed; preserving it: $($entry.path)" }
    }
}
foreach ($path in @($state.registration,$state.native,(Join-Path $PSScriptRoot 'pkgIndex.tcl'))) {
    if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
}
Write-Output 'Removed VMD registration, native reader and package index. Documentation, examples and migration records remain. Restart VMD.'
