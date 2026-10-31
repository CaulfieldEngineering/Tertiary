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
# Machine settings live outside the repo: $HOME\.wla-test\env.ps1 here, ~/.wla-test/env.sh on the Mac.
# The Mac comes from config.cmake (wTEST_DASHBOARD_MAC, wTEST_DASHBOARD_MAC_REPO).
param([Parameter(Mandatory)][string]$Report, [string]$Branch = 'development', [switch]$NoWeb)
$ErrorActionPreference = 'Stop'
$Root = Split-Path $PSScriptRoot -Parent
Set-Location $Root
$Python = if ($env:WLA_TEST_PYTHON) { $env:WLA_TEST_PYTHON } else { 'python' }
$envFile = Join-Path $HOME '.wla-test\env.ps1'
if (Test-Path $envFile) { . $envFile }
function Say($text) { Write-Host ("[{0:HH:mm:ss}] {1}" -f (Get-Date), $text) }
function Fail($text) { Say "FAIL: $text"; exit 1 }

# 1. Commit and dashboard ----------------------------------------------------------------------------
$commit = (git rev-parse HEAD).Trim()
if (git status --porcelain --untracked-files=no) { Fail 'this checkout has uncommitted changes; commit first' }
$cfg = & $Python -c "import sys;sys.path.insert(0,'tests/lib');from common import config;c=config();print(c.get('wTEST_DASHBOARD_MAC',''));print(c.get('wTEST_DASHBOARD_MAC_REPO',''));print(c['wPLUGIN_NAME'])"
$macHost, $macRepo, $product = $cfg[0].Trim(), $cfg[1].Trim(), $cfg[2].Trim()
$stamp = (Get-Date -Format 'yyyy-MM-dd_HHmm') + '-release'
Say "Release check of $commit as $Report; results tests/results/$stamp"
if (-not (Get-NetTCPConnection -LocalPort 4120 -State Listen -ErrorAction SilentlyContinue)) {
    Start-Process -WindowStyle Hidden -FilePath $Python -ArgumentList @('tests/dashboard/server.py', '--port', '4120')
    Say 'Started the dashboard: http://127.0.0.1:4120/'
} else { Say 'Dashboard already running on port 4120' }

# 2. Same commit on the Mac, then both machines at once -----------------------------------------------
$mac = $null
if ($macHost) {
    $macHead = (ssh -o BatchMode=yes $macHost "cd $macRepo && git rev-parse HEAD").Trim()
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
        -ArgumentList @('-o', 'BatchMode=yes', $macHost, "bash $macRepo/tests/mac/release-check.sh $commit $stamp")
}
Say 'Windows: building'
cmake --build build-tests --config Release -j 8 | Out-File (Join-Path $Root "tests/results/$stamp-win-build.log")
if ($LASTEXITCODE) { Fail 'Windows build failed' }
if (-not $NoWeb) {
    if ($env:WLA_EMSDK_ENV) { . $env:WLA_EMSDK_ENV *> $null }
    Push-Location plugin/tests/web; npm run build *> (Join-Path $Root "tests/results/$stamp-web-build.log"); $webCode = $LASTEXITCODE; Pop-Location
    if ($webCode) { Fail 'web build failed' }
}
Say 'Windows: Run-All'
& powershell -NoProfile -ExecutionPolicy Bypass -File tests/win/Run-All.ps1 -ResultsDir "tests/results/$stamp"
$winCode = $LASTEXITCODE
$macCode = 0
if ($mac) {
    Say 'Waiting for the Mac'
    $mac.WaitForExit(); $macCode = $mac.ExitCode
    $macCopy = Join-Path $Root "tests/results/$stamp-mac"
    scp -q -r -o BatchMode=yes "${macHost}:$macRepo/tests/results/$stamp" $macCopy
    if ($LASTEXITCODE) { Fail 'could not copy the Mac results' }
}
$runs = @('--run', "win=tests/results/$stamp") + $(if ($mac) { @('--run', "mac=tests/results/$stamp-mac") } else { @() })
if ($winCode -or $macCode) {
    & $Python tests/dashboard/server.py --export "tests/results/$stamp/report.html" @runs | Out-Null
    Fail "local tests failed (Windows exit $winCode, Mac exit $macCode); nothing pushed. View: tests/results/$stamp/report.html"
}

# 3. Push to the work branch only after local green ----------------------------------------------------
Say "Local green; pushing $commit to $Branch"
git push origin "${commit}:refs/heads/$Branch"
if ($LASTEXITCODE) { Fail "push to $Branch was refused (not a fast-forward?)" }

# 4. The GitHub run for that push, then the one report -------------------------------------------------
$token = (("protocol=https`nhost=github.com`n`n" | git credential fill) | Where-Object { $_ -like 'password=*' }) -replace '^password=', ''
$slug = ((git remote get-url origin) -replace '.*github\.com[:/]', '' -replace '\.git$', '')
$headers = @{ Accept = 'application/vnd.github+json'; Authorization = "Bearer $token" }
$runId = $null
for ($i = 0; $i -lt 240; $i++) {
    $found = (Invoke-RestMethod -Headers $headers "https://api.github.com/repos/$slug/actions/runs?head_sha=$commit&branch=$Branch&event=push").workflow_runs |
        Where-Object { $_.name -like 'Tests*' } | Select-Object -First 1
    if ($found) {
        $runId = $found.id
        if ($found.status -eq 'completed') { break }
    }
    Start-Sleep -Seconds 30
}
if (-not $runId) { Fail "no GitHub test run appeared for $commit on $Branch" }
Say "GitHub run $runId finished"
& $Python tests/lib/combine.py @runs --github-run $runId --out "tests/results/$stamp/release" --report "tests/reports/$Report.md" --title "$product $Report test report"
$code = $LASTEXITCODE
Say $(if ($code) { "FAIL. Report: tests/reports/$Report.md and .html" } else { "PASS. Report: tests/reports/$Report.md and .html" })
exit $code
