param(
    [string]$VmdDir = 'C:\Program Files\University of Illinois\VMD2',
    [string]$Destination = '',
    [string[]]$StartupFile = @()
)
$ErrorActionPreference = 'Stop'
$package = $PSScriptRoot
if (!(Test-Path -LiteralPath (Join-Path $package 'bin/vmdcndb.dll'))) { throw 'Run install.ps1 from the extracted Windows ZIP.' }
if (!(Test-Path -LiteralPath (Join-Path $VmdDir 'vmd.exe'))) { throw "VMD not found: $VmdDir" }
if (!$Destination) { $Destination = Join-Path $env:LOCALAPPDATA 'VMD-CNDB/0.1.0' }
$Destination = [IO.Path]::GetFullPath($Destination)
if ($Destination -eq [IO.Path]::GetFullPath($package)) { throw 'Choose an installation directory separate from the extracted ZIP.' }
$manifestPath = Join-Path $Destination 'install-state.json'
if ((Test-Path -LiteralPath $Destination) -and !(Test-Path -LiteralPath $manifestPath)) {
    if ((Get-ChildItem -LiteralPath $Destination -Force | Measure-Object).Count) { throw 'Destination is nonempty and is not managed by this installer.' }
}
$personalStartup = !$StartupFile.Count
if ($personalStartup) {
    $personalDirectory = $env:HOME
    if (!$personalDirectory) { $personalDirectory = $env:USERPROFILE }
    if (!$personalDirectory) { throw 'HOME/USERPROFILE is unavailable; supply -StartupFile.' }
    $personalFile = Join-Path $personalDirectory 'vmd.rc'
    if (!(Test-Path -LiteralPath $personalFile)) {
        $defaultFile = Join-Path $VmdDir 'vmd.rc'
        if (!(Test-Path -LiteralPath $defaultFile)) { throw 'VMD default vmd.rc is missing; supply an existing -StartupFile.' }
        $defaultHex = ([BitConverter]::ToString([Text.Encoding]::UTF8.GetBytes($defaultFile.Replace('\','/')))).Replace('-','')
        # Keep VMD's defaults live without copying or modifying Program Files.
        [IO.File]::WriteAllText($personalFile, "# Personal VMD startup: retain installed defaults.`nsource [encoding convertfrom utf-8 [binary decode hex $defaultHex]]`n",[Text.UTF8Encoding]::new($false))
    }
    $StartupFile = @($personalFile)
}
$StartupFile = @($StartupFile | ForEach-Object { [IO.Path]::GetFullPath($_) } | Select-Object -Unique)
$plans = @()
foreach ($path in $StartupFile) {
    if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Startup file not found: $path. Supply an existing effective vmd.rc with -StartupFile." }
    $stream = [IO.StreamReader]::new($path, $true)
    try { $text = $stream.ReadToEnd(); $encoding = $stream.CurrentEncoding } finally { $stream.Dispose() }
    $open = ([regex]::Matches($text,'(?m)^# >>> vmd-cndb managed >>>\r?$')).Count
    $close = ([regex]::Matches($text,'(?m)^# <<< vmd-cndb managed <<<\r?$')).Count
    if ($open -ne $close -or $open -gt 1) { throw "Malformed managed block in $path; restore its backup before installing." }
    $access = [IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::Read)
    $access.Dispose()
    $plans += [pscustomobject]@{path=$path;text=$text;encoding=$encoding}
}
New-Item -ItemType Directory -Force -Path $Destination | Out-Null
foreach ($name in @('bin','tcl','examples','docs','licenses','README.md','LICENSE','CHANGELOG.md','install.ps1','uninstall.ps1')) {
    Copy-Item -LiteralPath (Join-Path $package $name) -Destination $Destination -Recurse -Force
}
$hex = ([BitConverter]::ToString([Text.Encoding]::UTF8.GetBytes((Join-Path $Destination 'tcl/bootstrap.tcl').Replace('\','/')))).Replace('-','').ToLowerInvariant()
$block = @(
    '# >>> vmd-cndb managed >>>',
    "set cndb_bootstrap [encoding convertfrom utf-8 [binary decode hex $hex]]",
    'if {[file exists $cndb_bootstrap]} {',
    '    if {[catch {source $cndb_bootstrap} cndb_error]} {puts stderr "CNDB Loader: $cndb_error"}',
    '}',
    'unset cndb_bootstrap',
    '# <<< vmd-cndb managed <<<'
) -join "`n"
$pattern = '(?ms)^# >>> vmd-cndb managed >>>\r?\n.*?^# <<< vmd-cndb managed <<<\r?\n?'
$state = @()
if (Test-Path -LiteralPath $manifestPath) {
    $old = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ($old.project -ne 'vmd-cndb') { throw 'Unexpected install manifest.' }
    $state = @($old.startup)
}
foreach ($plan in $plans) {
    $path=$plan.path; $text=$plan.text; $encoding=$plan.encoding
    $backup = "$path.cndb-backup-$(Get-Date -Format yyyyMMdd-HHmmss-fff)"
    Copy-Item -LiteralPath $path -Destination $backup
    if ($text -match $pattern) {
        $updated = [regex]::Replace($text,$pattern,[System.Text.RegularExpressions.MatchEvaluator]{param($m) $block + "`n"})
    } else {
        $updated = $text + "`n" + $block + "`n"
    }
    [IO.File]::WriteAllText($path,$updated,$encoding)
    $state += [pscustomobject]@{path=$path; backup=$backup}
    [pscustomobject]@{project='vmd-cndb';version='0.1.0';destination=$Destination;startup=$state} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding utf8
}
if ($personalStartup -and $env:APPDATA) {
    $menu = Join-Path $env:APPDATA 'Microsoft/Windows/Start Menu/Programs'
    New-Item -ItemType Directory -Force -Path $menu | Out-Null
    $shell = New-Object -ComObject WScript.Shell
    $shortcutPath = Join-Path $menu 'VMD CNDB Loader.lnk'
    $shortcut = $shell.CreateShortcut($shortcutPath)
    $shortcut.TargetPath = Join-Path $VmdDir 'vmd.exe'
    $shortcut.Arguments = '-startup "' + $personalFile + '"'
    $shortcut.WorkingDirectory = $personalDirectory
    $shortcut.Description = 'VMD with the native CNDB Loader'
    $shortcut.Save()
    Write-Output "Start menu shortcut: $shortcutPath"
}
Write-Output "Installed CNDB Loader: $Destination"
Write-Output 'Open VMD CNDB Loader from Start, then choose Extensions > CNDB Loader. Configurations have timestamped backups.'
Write-Output "To remove startup integration: & '$Destination\uninstall.ps1'"
