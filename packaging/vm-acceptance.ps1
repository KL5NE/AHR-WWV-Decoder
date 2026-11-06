$ErrorActionPreference = 'Stop'
$report = New-Object 'System.Collections.Generic.List[string]'
function Note($case,$result,$detail='') { $report.Add("$case | $result | $detail"); Write-Host "$case : $result $detail" }
function Demand($condition,$message) { if (-not $condition) { throw $message } }
function Sha($path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() }
$hostFolder = Join-Path $env:RUNNER_TEMP 'AHR SDRangel 800 VM Test'
$setup = @(Get-ChildItem output -Filter '*-Setup.exe')
Demand ($setup.Count -eq 1) 'Expected exactly one beta installer'
$dll = Join-Path $hostFolder 'plugins\ahrwwvtimedecoder.dll'
$state = Join-Path $env:ProgramData 'AHR-Decoder\installed.json'
$uninstaller = Join-Path $env:ProgramFiles 'AHR-Decoder\Uninstall.exe'
$expected = $env:DLL_HASH
try {
  $official = (Resolve-Path 'sdrangel-8.0.0-win64.exe').Path
  # Official v8.0.0 NSIS executable, verified against upstream SHA-256 earlier.
  $p = Start-Process -FilePath $official -ArgumentList "/S /D=$hostFolder" -Wait -PassThru
  Demand ($p.ExitCode -eq 0) "Official host installer failed: $($p.ExitCode)"
  Demand (Test-Path (Join-Path $hostFolder 'sdrangel.exe')) 'Official installed sdrangel.exe missing'
  Demand ((Sha (Join-Path $hostFolder 'sdrangel.exe')) -eq $env:HOST_HASH) 'Official host fingerprint unexpected after installation'
  Demand (Test-Path (Join-Path $hostFolder 'plugins')) 'Official plugin directory missing'
  Note 'Official SDRangel 8.0.0 custom path with spaces' 'PASS'
  $existing = @(Get-ChildItem (Join-Path $hostFolder 'plugins') -File | ForEach-Object { "$($_.Name) $(Sha $_.FullName)" })
  function RunSetup { 
    $x = Start-Process -FilePath $setup[0].FullName -ArgumentList "/S /D=$hostFolder" -Wait -PassThru
    return $x.ExitCode
  }
  $cleanExit = RunSetup
  if ($cleanExit -ne 0) {
    $installerLog = Join-Path $env:ProgramData 'AHR-Decoder\installer.log'
    $diagnostic = if (Test-Path -LiteralPath $installerLog) {
      (Get-Content -LiteralPath $installerLog -Tail 12) -join ' | '
    } else { 'installer.log absent (NSIS may have failed before invoking install-plugin.ps1)' }
    Note 'Silent clean install diagnostic' 'FAIL' "exit=$cleanExit; $diagnostic"
    Demand $false "AHR-Decoder silent clean setup failed (exit=$cleanExit): $diagnostic"
  }
  Demand (Test-Path $dll) 'AHR plugin DLL missing after setup'
  Demand ((Sha $dll) -eq $expected) 'Installed DLL checksum mismatch'
  Demand (Test-Path $state) 'Installer ownership state missing'
  Demand (Test-Path $uninstaller) 'Uninstaller missing from Installed apps location'
  Note 'Clean install into official host + ownership + DLL hash' 'PASS'
  Demand ((RunSetup) -eq 0) 'Recognized upgrade failed'
  Demand ((Sha $dll) -eq $expected) 'Upgraded DLL checksum mismatch'
  Note 'Recognized AHR upgrade' 'PASS'
  $upgraded = Get-Content $state -Raw | ConvertFrom-Json
  Demand ($upgraded.dllSha256 -eq $expected) 'Upgraded state ownership record mismatch'
  $uninstall = Start-Process -FilePath $uninstaller -ArgumentList '/S' -Wait -PassThru
  Demand ($uninstall.ExitCode -eq 0) 'Uninstall failed'
  Demand (-not (Test-Path $dll)) 'Uninstaller left AHR DLL installed'
  Demand (-not (Test-Path $state)) 'Uninstaller left active ownership record'
  Note 'Uninstall owned DLL only' 'PASS'
  $remaining = @(Get-ChildItem (Join-Path $hostFolder 'plugins') -File | ForEach-Object { "$($_.Name) $(Sha $_.FullName)" })
  Demand ((Compare-Object $existing $remaining).Length -eq 0) 'Other SDRangel plugins changed'
  Note 'Other plugin files unaffected' 'PASS'
  [IO.File]::WriteAllBytes($dll, [byte[]](1,2,3,4,5,6,7))
  $before = Sha $dll
  $ignore = RunSetup
  Demand ((Sha $dll) -eq $before) 'Installer overwrote an unrecognized same-name DLL'
  Demand (-not (Test-Path $state)) 'Installer claimed ownership of an unknown DLL'
  Remove-Item $dll -Force
  Note 'Unknown existing same-name DLL is refused' 'PASS'
  # Use a modified host executable to verify the fail-closed version fingerprint.
  $hostExe = Join-Path $hostFolder 'sdrangel.exe'
  $bak = Join-Path $hostFolder 'sdrangel.exe.knowngood'
  Copy-Item $hostExe $bak
  try {
    Add-Content -LiteralPath $hostExe -Value 'unsupported version fingerprint'
    $ignored = RunSetup
    Demand (-not (Test-Path $dll)) 'Installer accepted a non-approved host executable'
  } finally {
    Move-Item $bak $hostExe -Force
  }
  Note 'Unsupported host fingerprint blocked' 'PASS'
  Note 'SDRangel live GUI launch/plugin menu' 'NOT RUN' 'Needs interactive VM'
  Note 'SDRangel-running lock / forced copy failure rollback' 'NOT RUN' 'Needs controlled fault injection'
  Note 'Other SDRangel releases (7.x/8.x)' 'NOT RUN' 'Hash allowlist rejects all non-approved hosts by design'
} catch {
  Note 'VM test harness' 'FAIL' $_.Exception.Message
  throw
} finally {
  $report.Insert(0,'AHR-Decoder 0.9.0-beta installer disposable Windows runner acceptance')
  $report.Add('Not a HAMBEAST test; interactive SDRangel GUI validation remains required.')
  $report | Set-Content -Path output/VM-ACCEPTANCE.txt
  $report | ForEach-Object { Write-Host $_ }
}
