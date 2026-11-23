param([string]$Manifest = (Join-Path $PSScriptRoot 'install-state.json'))
$ErrorActionPreference = 'Stop'
$state = Get-Content -LiteralPath $Manifest -Raw | ConvertFrom-Json
if ($state.project -ne 'vmd-cndb') { throw 'Unexpected install manifest.' }
$pattern = '(?ms)^# >>> vmd-cndb managed >>>\r?\n.*?^# <<< vmd-cndb managed <<<\r?\n?'
foreach ($path in @($state.startup.path | Select-Object -Unique)) {
    if (!(Test-Path -LiteralPath $path)) { continue }
    $stream = [IO.StreamReader]::new($path,$true)
    try { $text=$stream.ReadToEnd(); $encoding=$stream.CurrentEncoding } finally { $stream.Dispose() }
    $updated = [regex]::Replace($text,$pattern,'')
    if ($updated -ne $text) {
        Copy-Item -LiteralPath $path -Destination "$path.cndb-uninstall-$(Get-Date -Format yyyyMMdd-HHmmss-fff)"
        [IO.File]::WriteAllText($path,$updated,$encoding)
    }
}
Write-Output 'Removed CNDB startup integration. Restart VMD. Installed files and configuration backups remain available.'
