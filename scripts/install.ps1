param(
    [string]$VmdDir = 'C:\Program Files\University of Illinois\VMD2',
    [string]$LegacyStartupFile = (Join-Path $env:USERPROFILE 'vmd.rc')
)
$ErrorActionPreference = 'Stop'
$version = '0.2.1'
$package = $PSScriptRoot
$VmdDir = [IO.Path]::GetFullPath($VmdDir)
if (!(Test-Path -LiteralPath (Join-Path $package 'bin/vmdcndb.dll'))) { throw 'Run install.ps1 from the extracted Windows ZIP.' }
if (!(Test-Path -LiteralPath (Join-Path $VmdDir 'vmd.exe'))) { throw "VMD not found: $VmdDir" }
$native = Join-Path $VmdDir 'plugins/WIN64/molfile/vmdcndb.so'
$destination = Join-Path $VmdDir "plugins/noarch/tcl/vmdcndb$version"
$registration = Join-Path $VmdDir 'scripts/init.d/vmdcndb.tcl'
$manifest = Join-Path $destination 'install-state.json'
foreach ($path in @($native,$registration)) {
    if ((Test-Path -LiteralPath $path) -and !(Test-Path -LiteralPath $manifest)) { throw "Existing plugin has no ownership manifest: $path" }
}
if ((Test-Path -LiteralPath $destination) -and !(Test-Path -LiteralPath $manifest)) {
    if ((Get-ChildItem -LiteralPath $destination -Force | Measure-Object).Count) { throw "Unmanaged package directory: $destination" }
}
if (Test-Path -LiteralPath $manifest) {
    $old = Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
    if ($old.project -ne 'vmd-cndb' -or $old.vmd_directory -ne $VmdDir) { throw 'Unexpected ownership manifest.' }
}
$probe = Join-Path $VmdDir ('.vmd-cndb-write-' + [Guid]::NewGuid().ToString('N'))
try {
    $stream = [IO.File]::Open($probe,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    $stream.Dispose()
    Remove-Item -LiteralPath $probe
} catch { throw "VMD directory requires write access. Run this installer in an administrator PowerShell. No plugin files changed. $($_.Exception.Message)" }
$legacy = $null
$pattern = '(?ms)^# >>> vmd-cndb managed >>>\r?\n.*?^# <<< vmd-cndb managed <<<\r?\n?'
if ($LegacyStartupFile -and (Test-Path -LiteralPath $LegacyStartupFile)) {
    $stream = [IO.StreamReader]::new($LegacyStartupFile,$true)
    try { $text=$stream.ReadToEnd();$encoding=$stream.CurrentEncoding } finally { $stream.Dispose() }
    $open=([regex]::Matches($text,'(?m)^# >>> vmd-cndb managed >>>\r?$')).Count
    $close=([regex]::Matches($text,'(?m)^# <<< vmd-cndb managed <<<\r?$')).Count
    if ($open -ne $close -or $open -gt 1) { throw 'Malformed legacy startup block; restore its backup first.' }
    if ($open) { $legacy = @{path=$LegacyStartupFile; text=$text; encoding=$encoding} }
}
New-Item -ItemType Directory -Force -Path $destination,(Split-Path $native -Parent),(Split-Path $registration -Parent) | Out-Null
$state = @{project='vmd-cndb';version=$version;vmd_directory=$VmdDir;package_directory=$destination;native=$native;registration=$registration}
$state | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifest -Encoding utf8
foreach ($name in @('cndb.tcl','package.tcl','pkgIndex.tcl')) {
    Copy-Item -LiteralPath (Join-Path $package "tcl/$name") -Destination $destination -Force
}
foreach ($name in @('examples','docs','licenses','README.md','LICENSE','CHANGELOG.md','uninstall.ps1')) {
    Copy-Item -LiteralPath (Join-Path $package $name) -Destination $destination -Recurse -Force
}
Copy-Item -LiteralPath (Join-Path $package 'bin/vmdcndb.dll') -Destination $native -Force
# Register last so VMD never discovers a partly copied package.
Copy-Item -LiteralPath (Join-Path $package 'tcl/register.tcl') -Destination $registration -Force
$state.native_sha256 = (Get-FileHash -LiteralPath $native -Algorithm SHA256).Hash
$state.registration_sha256 = (Get-FileHash -LiteralPath $registration -Algorithm SHA256).Hash
if ($legacy) {
    $backup = "$($legacy.path).cndb-migration-$(Get-Date -Format yyyyMMdd-HHmmss-fff)"
    Copy-Item -LiteralPath $legacy.path -Destination $backup
    [IO.File]::WriteAllText($legacy.path,[regex]::Replace($legacy.text,$pattern,''),$legacy.encoding)
    $state.legacy_backup = $backup
}
$state | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifest -Encoding utf8
Write-Output "Installed package: $destination"
Write-Output "Installed native reader: $native"
Write-Output "VMD extension registration: $registration"
Write-Output 'Start the original VMD executable normally; choose Plugins > CNDB Loader. No custom launcher or -startup option is required.'
