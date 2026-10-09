# Regression check for the Windows app. Run from the repository root:  powershell -File tools\verify.ps1
# Builds the app, checks that every text fits (EN/TR/ES), starts the app, and measures what it costs while idle.
# It never shows a permission prompt: the app is ended like a Windows shutdown, which gives the controller back
# without the "disconnect on exit" step. If HidHide or the controller is missing, those checks are skipped.

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$bin = Join-Path $root 'build\Release'
$failed = 0
function Check($name, $ok, $detail = '') {
    if ($ok) { Write-Host ("  PASS  {0} {1}" -f $name, $detail) -ForegroundColor Green }
    else { Write-Host ("  FAIL  {0} {1}" -f $name, $detail) -ForegroundColor Red; $script:failed++ }
}

Write-Host '1. Build'
$out = & cmake --build (Join-Path $root 'build') --config Release 2>&1 | Out-String
Check 'build has no warnings or errors' (($out -notmatch 'warning|error') -and $LASTEXITCODE -eq 0)
$exe = Join-Path $bin 'Ultimate2CFixer.exe'
$size = (Get-Item $exe).Length
Check 'exe size under 400 KB' ($size -lt 400KB) ("($([int]($size/1KB)) KB)")

Write-Host '2. Layout (all languages)'
& cmake --build (Join-Path $root 'build') --config Release --target LayoutCheck 2>&1 | Out-Null
$layout = & (Join-Path $bin 'LayoutCheck.exe') 2>&1 | Out-String
Check 'no text is too wide' ($layout -match '0 too wide')

Write-Host '3. Run, hide, measure, give back'
Add-Type @'
using System; using System.Text; using System.Runtime.InteropServices;
public static class VerifyWin {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc p, IntPtr l);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  public static IntPtr Find(uint pid) { IntPtr f = IntPtr.Zero;
    EnumWindows((h, l) => { uint q; GetWindowThreadProcessId(h, out q); var sb = new StringBuilder(256); GetClassName(h, sb, 256);
      if (q == pid && sb.ToString() == "Ultimate2CFixer_Class") { f = h; return false; } return true; }, IntPtr.Zero); return f; }
}
'@
$probe = Join-Path $bin 'InputProbe.exe'
if (-not (Test-Path $probe)) { & cmake --build (Join-Path $root 'build') --config Release --target InputProbe 2>&1 | Out-Null }
$hasHidHide = (& $probe hidhide 2>&1 | Out-String) -match 'Active'
$p = Start-Process -FilePath $exe -PassThru
Start-Sleep -Seconds 10
$h = [VerifyWin]::Find([uint32]$p.Id)
Check 'window created' ($h -ne [IntPtr]::Zero)
if ($hasHidHide) {
    $state = & $probe hidhide 2>&1 | Out-String
    Check 'hiding is on while the app runs' ($state -match 'Active\s*:\s*yes')
}
function CpuPerSecond($seconds) {
    $p.Refresh(); $a = $p.TotalProcessorTime.TotalMilliseconds; Start-Sleep -Seconds $seconds; $p.Refresh()
    return ($p.TotalProcessorTime.TotalMilliseconds - $a) / $seconds
}
$cpu = CpuPerSecond 30
Check 'idle CPU under 20 ms per second (2% of one core)' ($cpu -lt 20) ('({0:N1} ms/s)' -f $cpu)
[void][VerifyWin]::PostMessage($h, 0x0111, [IntPtr]104, [IntPtr]0)   # the _ button: hide to the tray
Start-Sleep -Seconds 4
$p.Refresh()
Check 'tray memory under 12 MB' ($p.WorkingSet64 -lt 12MB) ('({0:N1} MB)' -f ($p.WorkingSet64 / 1MB))
$cpuTray = CpuPerSecond 20
Check 'tray CPU under 15 ms per second' ($cpuTray -lt 15) ('({0:N1} ms/s)' -f $cpuTray)
[void][VerifyWin]::PostMessage($h, 0x0016, [IntPtr]1, [IntPtr]0)     # WM_ENDSESSION: gives the controller back
Start-Sleep -Seconds 3
Stop-Process -Id $p.Id -Force   # Windows ends the process after WM_ENDSESSION
if ($hasHidHide) {
    $after = & $probe hidhide 2>&1 | Out-String
    Check 'controller given back' (($after -match 'Active\s*:\s*no') -and ($after -match 'No unrestored'))
}

Write-Host ''
if ($failed -eq 0) { Write-Host 'All checks passed.' -ForegroundColor Green; exit 0 }
Write-Host "$failed check(s) failed." -ForegroundColor Red; exit 1
