param(
  [Parameter(Mandatory=$true)][ValidateSet('Install','Uninstall')][string]$Action,
  [Parameter(Mandatory=$true)][string]$HostFolder,
  [string]$PayloadDll,
  [string]$ApprovedHostHash,
  [string]$PayloadHash,
  [string]$Version = '0.9.0-beta',
  [string]$BuildCommit = '0b0cb1858ec808259d7b7e1b4fae43b9e61e0943',
  [string]$BuildRun = '42299074954'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$store = Join-Path $env:ProgramData 'AHR-Decoder'
$statePath = Join-Path $store 'installed.json'
$logPath = Join-Path $store 'installer.log'
function Fail($why) { throw $why }
function Sha($path) { return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() }
function AssertNativeX64($path) {
  $f = [IO.File]::OpenRead($path)
  try {
    $b = New-Object byte[] 4
    [void]$f.Read($b,0,2)
    if ($b[0] -ne 77 -or $b[1] -ne 90) { Fail 'Not a PE executable.' }
    $f.Position = 0x3c
    [void]$f.Read($b,0,4)
    $p = [BitConverter]::ToInt32($b,0)
    if ($p -lt 64 -or $p -gt ($f.Length - 6)) { Fail 'Invalid PE header.' }
    $f.Position = $p
    [void]$f.Read($b,0,4)
    if ([BitConverter]::ToUInt32($b,0) -ne 0x4550) { Fail 'Invalid PE signature.' }
    $f.Position = $p + 4
    [void]$f.Read($b,0,2)
    if ([BitConverter]::ToUInt16($b,0) -ne 0x8664) { Fail 'Expected a 64-bit Windows executable.' }
  } finally { $f.Dispose() }
}
try {
  if (-not [Environment]::Is64BitOperatingSystem) { Fail 'Windows x64 is required.' }
  if (Get-Process -Name 'sdrangel' -ErrorAction SilentlyContinue) { Fail 'Close SDRangel completely before installing or uninstalling.' }
  if (-not (Test-Path -LiteralPath $HostFolder -PathType Container)) { Fail 'SDRangel installation directory does not exist.' }
  $hostDir = (Resolve-Path -LiteralPath $HostFolder).Path.TrimEnd('\')
  $exe = Join-Path $hostDir 'sdrangel.exe'
  $plugins = Join-Path $hostDir 'plugins'
  $dll = Join-Path $plugins 'ahrwwvtimedecoder.dll'
  foreach ($p in @($exe,(Join-Path $hostDir 'sdrbase.dll'),(Join-Path $hostDir 'sdrgui.dll'),$plugins)) {
    if (-not (Test-Path -LiteralPath $p)) { Fail "Not a complete SDRangel install: missing $p" }
  }
  AssertNativeX64 $exe
  if (-not (Test-Path -LiteralPath $store)) { New-Item -ItemType Directory -Path $store -Force | Out-Null }
  if ($Action -eq 'Install') {
    if (-not (Test-Path -LiteralPath $PayloadDll -PathType Leaf)) { Fail 'Plugin payload missing.' }
    if (-not (Test-Path -LiteralPath $ApprovedHostHash -PathType Leaf)) { Fail 'Approved official SDRangel fingerprint missing.' }
    $official = (Get-Content -LiteralPath $ApprovedHostHash -Raw).Trim().ToLowerInvariant()
    if ($official -notmatch '^[0-9a-f]{64}$' -or (Sha $exe) -ne $official) {
      Fail 'Unsupported SDRangel host. Only the fingerprinted official v8.0.0 x64 release is allowed; no override.'
    }
    AssertNativeX64 $PayloadDll
    $expected = $PayloadHash.ToLowerInvariant()
    if ($expected -notmatch '^[0-9a-f]{64}$' -or (Sha $PayloadDll) -ne $expected) { Fail 'DLL SHA-256 does not match the approved payload.' }
  }
  $oldState = $null
  if (Test-Path -LiteralPath $statePath) {
    $oldState = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    if ($oldState.hostFolder -ine $hostDir -or $oldState.dllPath -ine $dll) { Fail 'Another AHR installation is registered at a different host path. Refusing to change it.' }
  }
  if ($Action -eq 'Uninstall') {
    if ($null -eq $oldState) { Fail 'No AHR-owned installation record exists. Refusing to delete an unknown DLL.' }
    if (Test-Path -LiteralPath $dll) {
      if ((Sha $dll) -ne $oldState.dllSha256.ToLowerInvariant()) { Fail 'The installed DLL was modified; refusing to delete it.' }
      Remove-Item -LiteralPath $dll -Force
    }
    Remove-Item -LiteralPath $statePath -Force
    Write-Output "Uninstalled AHR-owned DLL only."
  } else {
    if (Test-Path -LiteralPath $dll) {
      if ($null -eq $oldState) { Fail 'An unrecognized same-name DLL exists. Back it up manually; installer will not overwrite it.' }
      if ((Sha $dll) -ne $oldState.dllSha256.ToLowerInvariant()) { Fail 'Existing DLL hash does not match our recorded installation. Refusing to overwrite it.' }
    } elseif ($null -ne $oldState) {
      Fail 'Registered AHR DLL missing. Investigate manually before upgrading.'
    }
    $stage = Join-Path $plugins ('ahrwwv-stage-' + [guid]::NewGuid().ToString('N') + '.tmp')
    $backup = $null
    $replaced = $false
    try {
      Copy-Item -LiteralPath $PayloadDll -Destination $stage -ErrorAction Stop
      if ((Sha $stage) -ne $expected) { Fail 'Staged DLL hash mismatch.' }
      if (Test-Path -LiteralPath $dll) {
        $backup = Join-Path $store ('backup-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N') + '.dll')
        Copy-Item -LiteralPath $dll -Destination $backup -ErrorAction Stop
        if ((Sha $backup) -ne $oldState.dllSha256.ToLowerInvariant()) { Fail 'Upgrade backup hash mismatch.' }
        Remove-Item -LiteralPath $dll -Force
        $replaced = $true
      }
      Move-Item -LiteralPath $stage -Destination $dll -ErrorAction Stop
      if ((Sha $dll) -ne $expected) { Fail 'Installed DLL hash mismatch.' }
      $record = [ordered]@{
        hostFolder = $hostDir; dllPath = $dll; dllSha256 = $expected
        version = $Version; buildCommit = $BuildCommit; buildRun = $BuildRun
        installedUtc = (Get-Date).ToUniversalTime().ToString('o')
      }
      $stateTmp = Join-Path $store 'installed.json.tmp'
      $record | ConvertTo-Json | Set-Content -LiteralPath $stateTmp -Encoding UTF8
      Move-Item -LiteralPath $stateTmp -Destination $statePath -Force
      Write-Output "Installed verified $Version DLL ($expected)"
    } catch {
      if ($replaced -and $backup -and (Test-Path -LiteralPath $backup)) {
        if (Test-Path -LiteralPath $dll) { Remove-Item -LiteralPath $dll -Force }
        Copy-Item -LiteralPath $backup -Destination $dll -Force
      } elseif ($null -eq $oldState -and (Test-Path -LiteralPath $dll)) {
        if ((Sha $dll) -eq $expected) { Remove-Item -LiteralPath $dll -Force }
      }
      throw
    } finally {
      if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Force }
    }
  }
  Add-Content -LiteralPath $logPath -Value "$(Get-Date -Format o) $Action successful, host=$hostDir, version=$Version"
  exit 0
} catch {
  $msg = $_.Exception.Message
  Write-Error $msg
  try { if (!(Test-Path $store)) { New-Item -ItemType Directory -Path $store -Force | Out-Null }; Add-Content -LiteralPath $logPath -Value "$(Get-Date -Format o) $Action blocked: $msg" } catch {}
  exit 1
}
