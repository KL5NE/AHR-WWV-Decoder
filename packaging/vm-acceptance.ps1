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
$installDir = Join-Path $env:ProgramFiles 'AHR-Decoder'
$uninstaller = Join-Path $installDir 'Uninstall.exe'
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
    $nsisTrace = Join-Path $env:TEMP 'AHR-Decoder-NSIS-Trace.log'
    if (Test-Path -LiteralPath $nsisTrace) { $diagnostic += '; NSIS trace: ' + ((Get-Content -LiteralPath $nsisTrace -Tail 20) -join ' | ') }
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
  # Standard NSIS copied-uninstaller mode: validate actual removal via DLL/state
  # postconditions, rather than relying solely on the launcher exit status.
  $uninstall = Start-Process -FilePath $uninstaller -ArgumentList '/S' -Wait -PassThru
  if ($uninstall.ExitCode -ne 0) {
    $unTracePath = Join-Path $env:TEMP 'AHR-Decoder-Uninstall-Trace.log'
    $trace = if (Test-Path -LiteralPath $unTracePath) { (Get-Content -LiteralPath $unTracePath -Tail 20) -join ' | ' } else { 'uninstall trace absent' }
    $helperLog = Join-Path $env:ProgramData 'AHR-Decoder\\installer.log'
    $helperTail = if (Test-Path -LiteralPath $helperLog) { (Get-Content -LiteralPath $helperLog -Tail 12) -join ' | ' } else { 'helper log absent' }
    Note 'Normal uninstall diagnostic' 'FAIL' "exit=$($uninstall.ExitCode); dllPresent=$(Test-Path -LiteralPath $dll); statePresent=$(Test-Path -LiteralPath $state); uninstallerPresent=$(Test-Path -LiteralPath $uninstaller); trace=$trace; helper=$helperTail"
  }
  Demand ($uninstall.ExitCode -eq 0) "Normal uninstall returned $($uninstall.ExitCode)"
  if (Test-Path -LiteralPath $dll) {
    # Preserve the exact failure evidence before the assertion aborts the VM job.
    $helperLog = Join-Path $env:ProgramData 'AHR-Decoder\installer.log'
    $nsisLog = Join-Path $env:TEMP 'AHR-Decoder-NSIS-Trace.log'
    $helperTail = if (Test-Path -LiteralPath $helperLog) { (Get-Content -LiteralPath $helperLog -Tail 12) -join ' | ' } else { 'absent' }
    $nsisTail = if (Test-Path -LiteralPath $nsisLog) { (Get-Content -LiteralPath $nsisLog -Tail 12) -join ' | ' } else { 'absent' }
    $statePresent = Test-Path -LiteralPath $state
    $unTracePath = Join-Path $env:TEMP 'AHR-Decoder-Uninstall-Trace.log'
    $unTrace = if (Test-Path -LiteralPath $unTracePath) { (Get-Content -LiteralPath $unTracePath -Tail 12) -join ' | ' } else { 'uninstall trace absent' }
    Note 'Uninstall removal diagnostic' 'FAIL' "exit=$($uninstall.ExitCode); dllSha=$(Sha $dll); statePresent=$statePresent; helperLog=$helperTail; nsisTrace=$nsisTail; uninstallTrace=$unTrace"
  }
  Demand (-not (Test-Path $dll)) 'Uninstaller left AHR DLL installed'
  Demand (-not (Test-Path $state)) 'Uninstaller left active ownership record'
  # NSIS _?= runs in place to expose the real refusal exit code, so it cannot
  # delete its own executable. Clean up only after removal postconditions pass.
  Remove-Item -LiteralPath $uninstaller -Force -ErrorAction SilentlyContinue
  # Standard NSIS uninstall removes its own directory; do not attempt a second
  # recursive directory removal. Assert the owned DLL/state were removed above.
  if (Test-Path -LiteralPath $installDir -PathType Container) {
    $leftovers = @(Get-ChildItem -LiteralPath $installDir -Force -ErrorAction Stop)
    if ($leftovers.Count -eq 0) {
      [System.IO.Directory]::Delete($installDir)
    }
  }
  Note 'Uninstall owned DLL only' 'PASS'
  $remaining = @(Get-ChildItem (Join-Path $hostFolder 'plugins') -File | ForEach-Object { "$($_.Name) $(Sha $_.FullName)" })
  Demand ((Compare-Object $existing $remaining).Length -eq 0) 'Other SDRangel plugins changed'
  Note 'Other plugin files unaffected' 'PASS'
  [IO.File]::WriteAllBytes($dll, [byte[]](1,2,3,4,5,6,7))
  $before = Sha $dll
  $refusalExit = RunSetup
  Demand ($refusalExit -ne 0) 'Installer returned success for an unrecognized same-name DLL'
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
    $unsupportedExit = RunSetup
    Demand ($unsupportedExit -ne 0) 'Installer returned success for an unsupported host fingerprint'
    Demand (-not (Test-Path $dll)) 'Installer accepted a non-approved host executable'
  } finally {
    Move-Item $bak $hostExe -Force
  }
  Note 'Unsupported host fingerprint blocked' 'PASS'
  # Exercise the real Get-Process safety gate with an isolated, renamed Windows
  # executable. Do not launch the SDRangel GUI on a headless runner.
  $sentinelExe = Join-Path $env:RUNNER_TEMP 'sdrangel.exe'
  Copy-Item -LiteralPath (Join-Path $env:WINDIR 'System32\\cmd.exe') -Destination $sentinelExe -Force
  $sentinel = $null
  try {
    $sentinel = Start-Process -FilePath $sentinelExe -ArgumentList '/c ping -n 45 127.0.0.1 >NUL' -PassThru -WindowStyle Hidden
    Start-Sleep -Seconds 2
    Demand (-not $sentinel.HasExited) 'Simulated SDRangel process exited before protection test'
    Demand (@(Get-Process -Name sdrangel -ErrorAction SilentlyContinue).Count -gt 0) 'Simulated SDRangel process was not detectable'
    $blockedExit = RunSetup
    Demand ($blockedExit -ne 0) 'Installer returned success while SDRangel-named process was running'
    Demand (-not (Test-Path $dll)) 'Installer installed DLL while SDRangel-named process was running'
    Demand (-not (Test-Path $state)) 'Installer registered ownership while SDRangel-named process was running'
    Note 'SDRangel-running process install refusal' 'PASS'
  } finally {
    if ($sentinel -and -not $sentinel.HasExited) { Stop-Process -Id $sentinel.Id -Force -ErrorAction SilentlyContinue }
    Remove-Item -LiteralPath $sentinelExe -Force -ErrorAction SilentlyContinue
  }
  # Verify a modified AHR-owned DLL cannot be deleted by the uninstaller.
  Demand ((RunSetup) -eq 0) 'Reinstall before modified-DLL uninstall refusal failed'
  Demand ((Sha $dll) -eq $expected) 'Reinstalled DLL hash mismatch before tamper test'
  [IO.File]::AppendAllText($dll, 'AHR-VM-TAMPER-TEST')
  $tamperedHash = Sha $dll
  $tamperedUninstall = Start-Process -FilePath $uninstaller -ArgumentList @('/S', "_?=$installDir") -Wait -PassThru
  if ($tamperedUninstall.ExitCode -eq 0) {
    # Capture refusal-path evidence before the assertion fails. Do not weaken the test.
    $unTracePath = Join-Path $env:TEMP 'AHR-Decoder-Uninstall-Trace.log'
    $trace = if (Test-Path -LiteralPath $unTracePath) { (Get-Content -LiteralPath $unTracePath -Tail 15) -join ' | ' } else { 'uninstall trace absent' }
    $helperLog = Join-Path $env:ProgramData 'AHR-Decoder\\installer.log'
    $helperTail = if (Test-Path -LiteralPath $helperLog) { (Get-Content -LiteralPath $helperLog -Tail 12) -join ' | ' } else { 'helper log absent' }
    Note 'Modified DLL refusal diagnostic' 'FAIL' "exit=$($tamperedUninstall.ExitCode); dllPresent=$(Test-Path -LiteralPath $dll); statePresent=$(Test-Path -LiteralPath $state); trace=$trace; helper=$helperTail"
  }
  Demand ($tamperedUninstall.ExitCode -ne 0) 'Uninstaller accepted a modified AHR-owned DLL'
  Demand ((Sha $dll) -eq $tamperedHash) 'Uninstaller altered the modified DLL'
  Demand (Test-Path $state) 'Uninstaller removed ownership state despite refusing modified DLL'
  Note 'Modified owned DLL uninstall refusal' 'PASS'
  Note 'SDRangel live GUI launch/plugin menu' 'NOT RUN' 'Needs interactive VM'
  Note 'Forced copy failure rollback' 'NOT RUN' 'Needs controlled fault injection'
  Note 'Other SDRangel releases (7.x/8.x)' 'NOT RUN' 'Hash allowlist rejects all non-approved hosts by design'
} catch {
  Note 'VM test harness' 'FAIL' $_.Exception.Message
  throw
} finally {
  $report.Insert(0,'AHR-Decoder 0.9.1-beta installer disposable Windows runner acceptance')
  $report.Add('Not a HAMBEAST test; interactive SDRangel GUI validation remains required.')
  $report | Set-Content -Path output/VM-ACCEPTANCE.txt
  $nsisTrace = Join-Path $env:TEMP 'AHR-Decoder-NSIS-Trace.log'
  if (Test-Path -LiteralPath $nsisTrace) { Add-Content -Path output/VM-ACCEPTANCE.txt -Value '--- NSIS trace ---'; Get-Content -LiteralPath $nsisTrace | Add-Content -Path output/VM-ACCEPTANCE.txt }
  $report | ForEach-Object { Write-Host $_ }
}

# The installer negative tests intentionally return nonzero from native executables.
# Those values must not leak through PowerShell's LASTEXITCODE after every assertion passed.
exit 0
