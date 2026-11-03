# [Claude 2026-10-31] One command for the release candidate's one test run (Joe approved, 2026-10-31).
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tests\Run-Release-Check.ps1 -Report rc-1.1.0
#
# 1. Records this checkout's commit (it must be clean) and starts or reuses the live dashboard (port 4120).
# 2. Brings the Mac checkout to the same commit (git bundle over SSH; the Mac has no GitHub credentials),
#    then runs both machines at the same time: Windows builds and runs Run-All (web included), the Mac
#    builds and runs tests/mac/release-check.sh (Run-All with Logic and the microphone).
# 3. Only if both are green locally, pushes that commit to `development` (fast-forward only). If anything
#    failed locally it pushes nothing and stops with FAIL.
# 4. Waits for the GitHub run that push started, then writes the single report with tests/lib/combine.py:
#    tests/reports/<Report>.md and the same view as the dashboard, tests/reports/<Report>.html.
#    Prints PASS or FAIL and exits 0 or 1.
#
# Machine settings live outside the repo: $HOME\.wla-test\env.ps1 here, ~/.wla-test/env.sh on the Mac. Both are optional
# overrides only (2026-10-31): tools default to the pinned copies in .tools (scripts/lib/tools.json).
# The Mac comes from config.cmake (wTEST_DASHBOARD_MAC, wTEST_DASHBOARD_MAC_REPO).
#
# [Claude 2026-11-03] Every step reports itself and its time limit to the step watch (scripts/lib/stepwatch.py; Joe:
# "i'd prefer if its built into the testing suite/scripts"). A step past its limit is stopped with everything it started
# and the check FAILs naming it; the wait on GitHub polls the run itself. The job files (tertiary-win-release-<stamp>
# here, tertiary-mac-release-<stamp> on the Mac) feed the HQ Step Watchdog, the backstop if a stop itself fails.
param([Parameter(Mandatory)][string]$Report, [string]$Branch = 'development', [switch]$NoWeb)
# [Claude 2026-11-02] Continue, not Stop: in Windows PowerShell 5.1 a native command's stderr (a Python or CMake warning)
# becomes an error record, and under Stop that ended the script with no FAIL line. Every step is judged by its exit code;
# the trap below turns anything else that throws into the usual FAIL line and run time.
$ErrorActionPreference = 'Continue'
$Root = Split-Path $PSScriptRoot -Parent
Set-Location $Root
$Python = if ($env:WLA_TEST_PYTHON) { $env:WLA_TEST_PYTHON } else { 'python' }
$envFile = Join-Path $HOME '.wla-test\env.ps1'
if (Test-Path $envFile) { . $envFile }
function Say($text) { Write-Host ("[{0:HH:mm:ss}] {1}" -f (Get-Date), $text) }
$Watch = Join-Path $Root 'scripts/lib/stepwatch.py'
# Step "<what>" <minutes>: says it and reports it. Timed "<what>" <minutes> <log or ''> <command...>: runs it under the
# limit (exit 124 when it was stopped), output to the log, or to this console when the log is ''.
function Step($text, $minutes) { Say $text; & $Python $Watch beat --step $text --minutes $minutes | Out-Null }
function Timed($text, $minutes, $log, [string[]]$command) {
    Say $text
    $a = @('run', '--step', $text, '--minutes', $minutes)
    if ($log) { $a += @('--log', $log) }
    & $Python $Watch @a -- @command | Out-Host
    return $LASTEXITCODE
}
function Over($text, $minutes, $log) { Fail ("$text ran past its limit of $minutes min and was stopped" + $(if ($log) { "; see $log" } else { '' })) }
# [Claude 2026-10-31] Joe: "test suite started at xxx and ended at yyy so I can feel for how long they all take."
$RunStart = Get-Date
function RunTime { $end = Get-Date; $t = $end - $RunStart
    Say ("Release check started {0:yyyy-MM-dd HH:mm:ss}, ended {1:yyyy-MM-dd HH:mm:ss}, took {2} h {3} min {4} s" -f $RunStart, $end, [int][math]::Floor($t.TotalHours), $t.Minutes, $t.Seconds) }
# [Claude 2026-11-02] A Windows failure no longer leaves the Mac half running on its own (the 2026-11-02 12:37 run
# did): Fail waits for it, so no test on the Mac is cut off with a host or plug-in staged.
# [Claude 2026-11-03] That wait has a limit too (90 min); past it the Mac half is left running and the FAIL says so.
function Fail($text) {
    if ($script:mac -and -not $script:mac.HasExited -and -not $script:macOver) {
        Step "Waiting for the Mac half to finish before stopping (its log: $script:macLog)" 90
        if (-not $script:mac.WaitForExit(90 * 60 * 1000)) { $text += "; the Mac half was still running after 90 min and is left running (its log: $script:macLog)" }
    }
    & $Python $Watch done --note 'FAIL' | Out-Null
    Say "FAIL: $text"; RunTime; exit 1 }
trap { Fail "unexpected error: $_" }

# 1. Commit and dashboard ----------------------------------------------------------------------------
$commit = (git rev-parse HEAD).Trim()
if (git status --porcelain --untracked-files=no) { Fail 'this checkout has uncommitted changes; commit first' }
$cfg = & $Python -c "import sys;sys.path.insert(0,'tests/lib');from common import config;c=config();print(c.get('wTEST_DASHBOARD_MAC',''));print(c.get('wTEST_DASHBOARD_MAC_REPO',''));print(c['wPLUGIN_NAME'])"
$macHost, $macRepo, $product = $cfg[0].Trim(), $cfg[1].Trim(), $cfg[2].Trim()
$stamp = (Get-Date -Format 'yyyy-MM-dd_HHmm') + '-release'
if (-not $env:WLA_WATCHDOG_JOB) { $env:WLA_WATCHDOG_JOB = "tertiary-win-release-$stamp" }
Say "Release check of $commit as $Report; results tests/results/$stamp"
if (-not (Get-NetTCPConnection -LocalPort 4120 -State Listen -ErrorAction SilentlyContinue)) {
    Start-Process -WindowStyle Hidden -FilePath $Python -ArgumentList @('tests/dashboard/server.py', '--port', '4120')
    Say 'Started the dashboard: http://127.0.0.1:4120/'
} else { Say 'Dashboard already running on port 4120' }
# [Claude 2026-10-31] Open it on the whole plan (every test, in run order, filling in as it runs).
for ($i = 0; $i -lt 20 -and -not (Get-NetTCPConnection -LocalPort 4120 -State Listen -ErrorAction SilentlyContinue); $i++) { Start-Sleep -Milliseconds 500 }
Start-Process 'http://127.0.0.1:4120/?plan=all'

# 2. Same commit on the Mac, then both machines at once -----------------------------------------------
$mac = $null
if ($macHost) {
    $macHead = "$(ssh -o BatchMode=yes $macHost "cd $macRepo && git rev-parse HEAD")".Trim()
    if ($LASTEXITCODE) { Fail "the Mac ($macHost) is not reachable" }
    if ($macHead -ne $commit) {
        $bundle = Join-Path $env:TEMP "release-$stamp.bundle"
        git bundle create $bundle "$macHead..HEAD" 2>$null
        if ($LASTEXITCODE) { Fail "could not bundle $macHead..$commit (is the Mac on a commit this checkout has?)" }
        scp -q -o BatchMode=yes $bundle "${macHost}:/tmp/release.bundle"
        ssh -o BatchMode=yes $macHost "cd $macRepo && git checkout -q -- .DS_Store 2>/dev/null; git fetch -q /tmp/release.bundle HEAD && git merge -q --ff-only $commit"
        if ($LASTEXITCODE) { Fail 'the Mac checkout could not fast-forward to this commit' }
    }
    Say "Mac on $commit; starting its build and Run-All"
    $macLog = Join-Path $Root "tests/results/$stamp-mac-console.log"
    New-Item -ItemType Directory -Force (Join-Path $Root 'tests/results') | Out-Null
    $mac = Start-Process -PassThru -WindowStyle Hidden -FilePath 'ssh' -RedirectStandardOutput $macLog -RedirectStandardError "$macLog.err" `
        -ArgumentList @('-o', 'BatchMode=yes', $macHost, "WLA_WATCHDOG_JOB=tertiary-mac-release-$stamp bash $macRepo/tests/mac/release-check.sh $commit $stamp")
    # [Claude 2026-11-02] Without this, Windows PowerShell 5.1 loses the exit code of a process started with
    # redirection: every run so far printed "Mac exit " (empty), so a failed Mac half would have counted as 0 and
    # been pushed. Same fix as plugin/tests/ci/installer_test_windows.ps1.
    $null = $mac.Handle
}
New-Item -ItemType Directory -Force (Join-Path $Root 'tests/results') | Out-Null
# A fresh checkout has no build folder, web packages or Cubase input audio (none are in git). Make them here so the
# release check runs from a clean pull, as Joe wants (2026-10-31: "remove the folders that are not in git so it looks like a fresh pull").
if (-not (Test-Path (Join-Path $Root 'build-tests/CMakeCache.txt'))) {
    $log = Join-Path $Root "tests/results/$stamp-win-configure.log"
    $code = Timed 'Windows: configuring build-tests (fresh checkout)' 30 $log @('cmake', '-S', $Root, '-B', (Join-Path $Root 'build-tests'), '-DWLA_TESTS=ON', '-DWLA_REVIEW_RENDER=ON', '-DWLA_COPY_PLUGIN_AFTER_BUILD=OFF')
    if ($code -eq 124) { Over 'Windows configure' 30 $log }
    if ($code) { Fail 'Windows configure failed' }
}
if (-not $NoWeb -and -not (Test-Path (Join-Path $Root 'plugin/tests/web/node_modules'))) {
    $log = Join-Path $Root "tests/results/$stamp-web-setup.log"
    Push-Location plugin/tests/web
    $setupCode = Timed 'Web: installing test packages (fresh checkout)' 20 $log @('npm', 'ci')
    if (-not $setupCode) { $setupCode = Timed 'Web: installing the Playwright browsers' 30 $log @('npx', 'playwright', 'install', 'chromium', 'webkit', 'firefox') }
    Pop-Location
    if ($setupCode -eq 124) { Over 'web test setup' 30 $log }
    if ($setupCode) { Fail 'web test setup failed' }
}
$log = Join-Path $Root "tests/results/$stamp-win-build.log"
$code = Timed 'Windows: building' 90 $log @('cmake', '--build', 'build-tests', '--config', 'Release', '-j', '8')
if ($code -eq 124) { Over 'Windows build' 90 $log }
if ($code) { Fail 'Windows build failed' }
# [Claude 2026-11-02] After the build: the harness audio is rendered by HostTest, which a fresh checkout has only now.
if (-not (Test-Path (Join-Path $Root 'plugin/tests/daw/cubase/Tertiary harness/Audio/harness-input.wav'))) {
    $log = Join-Path $Root "tests/results/$stamp-cubase-media.log"
    $code = Timed 'Cubase: making the harness input audio (fresh checkout)' 15 $log @($Python, (Join-Path $Root 'plugin/tests/daw/cubase/tertiary_harness_build.py'), 'audio')
    if ($code -eq 124) { Over 'Cubase harness audio' 15 $log }
    if ($code) { Fail "Cubase harness audio could not be made; see tests/results/$stamp-cubase-media.log" }
}
if (-not $NoWeb) {
    # [Claude 2026-10-31] The one web build script (scripts/web/build-web.ps1 -Test: the same plugin/tests/web/build.mjs as
    # `npm run build`). It takes Emscripten and Wonderlab from WLA_EMSDK_ENV / EMSDK and WONDERLAB_DIR when env.ps1 sets
    # them, else the pinned copies in .tools (scripts/lib/tools.json), so a fresh machine needs no settings file.
    # B-06 also limits itself (scripts/sequence.json); this outer limit only matters if that one fails.
    $log = Join-Path $Root "tests/results/$stamp-web-build.log"
    $webCode = Timed 'Web: building (B-06)' 100 $log @('powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', 'scripts/web/build-web.ps1', '-Test')
    if ($webCode -eq 124) { Over 'web build' 100 $log }
    if ($webCode) { Fail "web build failed; see tests/results/$stamp-web-build.log" }
}
# Each test inside reports itself and has its own limit (limit_minutes in tests/sequence.json); this one is the whole run.
$winCode = Timed 'Windows: Run-All' 150 '' @('powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', 'tests/win/Run-All.ps1', '-ResultsDir', "tests/results/$stamp")
if ($winCode -eq 124) { Over 'Windows Run-All' 150 '' }
$macCode = 0
if ($mac) {
    Step 'Waiting for the Mac' 90
    if (-not $mac.WaitForExit(90 * 60 * 1000)) { $script:macOver = $true; Fail "the Mac half ran past its limit of 90 min; it is left running (its log: $macLog)" }
    $macCode = $mac.ExitCode
    if ($null -eq $macCode) { Fail 'the Mac half ended without an exit code; nothing pushed' }
    $macCopy = Join-Path $Root "tests/results/$stamp-mac"
    $code = Timed 'Copying the Mac results' 15 '' @('scp', '-q', '-r', '-o', 'BatchMode=yes', "${macHost}:$macRepo/tests/results/$stamp", $macCopy)
    if ($code -eq 124) { Over 'copying the Mac results' 15 '' }
    if ($code) { Fail 'could not copy the Mac results' }
}
$runs = @('--run', "win=tests/results/$stamp") + $(if ($mac) { @('--run', "mac=tests/results/$stamp-mac") } else { @() })
if ($winCode -or $macCode) {
    & $Python tests/dashboard/server.py --export "tests/results/$stamp/report.html" @runs | Out-Null
    Fail "local tests failed (Windows exit $winCode, Mac exit $macCode); nothing pushed. View: tests/results/$stamp/report.html"
}

# 3. Push to the work branch only after local green ----------------------------------------------------
Step "Local green; pushing $commit to $Branch" 10
git push origin "${commit}:refs/heads/$Branch"
if ($LASTEXITCODE) { Fail "push to $Branch was refused (not a fast-forward?)" }

# 4. The GitHub run for that push, then the one report -------------------------------------------------
# [Claude 2026-11-03] The wait polls the run itself (scripts/lib/stepwatch.py gh-wait: token from git credential, the
# public API on a 401, a dropped request retried) and gives up at 120 min. It reports the run it waits on, so the HQ
# Step Watchdog would also see a run that finished while nothing moved here.
$slug = ((git remote get-url origin) -replace '.*github\.com[:/]', '' -replace '\.git$', '')
$waitOut = & $Python $Watch gh-wait --minutes 120 --repo $slug --sha $commit --branch $Branch --event push --workflow 'Tests*' --step "Waiting on the GitHub run for $commit"
$ghCode = $LASTEXITCODE
$waitOut | ForEach-Object { Say $_ }
$runLine = $waitOut | Where-Object { $_ -like 'RUN *' } | Select-Object -Last 1
if ($ghCode -eq 4 -or -not $runLine) { Fail "no GitHub test run appeared for $commit on $Branch within 120 min" }
$runId = ($runLine -split ' ')[1]
if ($ghCode -eq 124) { Fail "GitHub run $runId was still running after 120 min; stopped waiting" }
Step "GitHub run $runId finished; writing the report" 15
& $Python tests/lib/combine.py @runs --github-run $runId --out "tests/results/$stamp/release" --report "tests/reports/$Report.md" --title "$product $Report test report"
$code = $LASTEXITCODE
& $Python $Watch done --note $(if ($code) { 'FAIL' } else { 'PASS' }) | Out-Null
Say $(if ($code) { "FAIL. Report: tests/reports/$Report.md and .html" } else { "PASS. Report: tests/reports/$Report.md and .html" })
RunTime
exit $code
