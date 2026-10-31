-- tertiary_reaper_test.lua
-- REAPER integration test. See README.md for the reference's transport and preroll contract.
--
-- A ReaScript that REAPER runs at start-up (run_reaper_test.py starts it). It builds the test project from
-- scratch, so there is no hand-made plugin state in it:
--   1. a 48 kHz / 120 BPM project with 0.5 s silence then the 2 s test input
--   2. Tertiary at defaults, rendered twice against HostTest's matching playing-transport reference
--   3. an automation envelope on "Depth Low Band" (0 to 100 % over the audible 2 s);
--      renders out/reaper_automation.wav (checked for sanity: no NaN, and different from the default render)
--   4. saves the project as tertiary_test.rpp (Tertiary with its automation), then quits REAPER
-- Paths come from the environment, set by run_reaper_test.py: TERTIARY_TEST_DIR (this folder).

local dir = os.getenv("TERTIARY_TEST_DIR") or reaper.GetResourcePath()
local sep = package.config:sub(1, 1)
local function path(name) return dir .. sep .. name end
local log = io.open(path("out" .. sep .. (os.getenv("TERTIARY_PREFIX") or "reaper") .. "_log.txt"), "w")
local function say(text) if log then log:write(text .. "\n") log:flush() end end

-- 1. Project and track ------------------------------------------------------------------------------------
reaper.Main_OnCommand(40023, 0)                                  -- File: New project
reaper.GetSetProjectInfo(0, "PROJECT_SRATE", 48000, true)
reaper.GetSetProjectInfo(0, "PROJECT_SRATE_USE", 1, true)
reaper.SetCurrentBPM(0, 120, false)
reaper.InsertTrackAtIndex(0, true)
local track = reaper.GetTrack(0, 0)
reaper.SetOnlyTrackSelected(track)
reaper.SetEditCurPos(0, false, false)
reaper.InsertMedia(path("input.wav"), 0)                         -- 0 = add to the selected track at the cursor
local item = reaper.GetTrackMediaItem(track, 0)
for _, key in ipairs({"D_FADEINLEN", "D_FADEOUTLEN", "D_FADEINLEN_AUTO", "D_FADEOUTLEN_AUTO"}) do
    reaper.SetMediaItemInfo_Value(item, key, 0)
end
reaper.SetMediaTrackInfo_Value(track, "D_VOL", 1)
reaper.SetMediaTrackInfo_Value(reaper.GetMasterTrack(0), "D_VOL", 1)
say("REAPER " .. reaper.GetAppVersion() .. "; 48000 Hz stereo; 120 BPM; 24000 silent + 96000 signal frames")

-- 2. Tertiary --------------------------------------------------------------------------------------------
-- [Claude 2026-10-30, Mac day] TERTIARY_FX picks the format ("VST3:Tertiary", "AU:Tertiary"); TERTIARY_PREFIX names
-- the renders and the project (default "reaper"), so the VST3 and AU runs on the Mac don't overwrite each other.
local want = os.getenv("TERTIARY_FX")
local prefix = os.getenv("TERTIARY_PREFIX") or "reaper"
local fx = -1
if want then fx = reaper.TrackFX_AddByName(track, want, false, -1) end
if fx < 0 and not want then fx = reaper.TrackFX_AddByName(track, "VST3:Tertiary (Wonderland Audio)", false, -1) end
if fx < 0 and not want then fx = reaper.TrackFX_AddByName(track, "Tertiary", false, -1) end
if fx < 0 then say("FAIL Tertiary not found in REAPER's plug-in list") reaper.Main_OnCommand(40004, 0) return end
local _, fxname = reaper.TrackFX_GetFXName(track, fx, "")
say("loaded Tertiary as FX " .. fx .. ": " .. fxname)
-- [Codex 2026-10-30] Keep REAPER's own identity/offline flag beside the mapped-module proof.
local _, ident = reaper.TrackFX_GetNamedConfigParm(track, fx, 'fx_ident')
say('fx_ident: ' .. ident .. '; offline=' .. tostring(reaper.TrackFX_GetOffline(track, fx)))

-- Render settings: the whole project, master mix, 48 kHz stereo, 32-bit float WAV, no tail
-- [Codex 2026-10-30] Let the Python driver verify the mapped binary before rendering.
local function run_tests()
local function renderTo(name)
    reaper.GetSetProjectInfo_String(0, "RENDER_FILE", path("out"), true)
    reaper.GetSetProjectInfo_String(0, "RENDER_PATTERN", name, true)
    reaper.GetSetProjectInfo_String(0, "RENDER_FORMAT", "ZXZhdyAAAQ==", true)   -- WAV, 32-bit float
    reaper.GetSetProjectInfo(0, "RENDER_SRATE", 48000, true)
    reaper.GetSetProjectInfo(0, "RENDER_CHANNELS", 2, true)
    reaper.GetSetProjectInfo(0, "RENDER_BOUNDSFLAG", 0, true)    -- exact bounds, including silent preroll
    reaper.GetSetProjectInfo(0, "RENDER_STARTPOS", 0, true)
    reaper.GetSetProjectInfo(0, "RENDER_ENDPOS", 2.5, true)
    reaper.GetSetProjectInfo(0, "RENDER_SETTINGS", 0, true)      -- master mix
    reaper.GetSetProjectInfo(0, "RENDER_TAILFLAG", 0, true)
    reaper.GetSetProjectInfo(0, "RENDER_DITHER", 0, true)
    reaper.GetSetProjectInfo(0, "RENDER_NORMALIZE", 0, true)
    reaper.Main_OnCommand(42230, 0)                              -- Render project, using the most recent settings
    say("rendered " .. name)
end
renderTo(prefix .. "_default")
renderTo(prefix .. "_repeat")                              -- the same project and instance, a second transport start
reaper.Main_SaveProjectEx(0, path("out" .. sep .. prefix .. "_default.rpp"), 8)

-- 3. Automation ------------------------------------------------------------------------------------------
local depth = -1
for i = 0, reaper.TrackFX_GetNumParams(track, fx) - 1 do
    local _, name = reaper.TrackFX_GetParamName(track, fx, i, "")
    if name == "Depth Low Band" then depth = i end
end
if depth >= 0 then
    local env = reaper.GetFXEnvelope(track, fx, depth, true)
    reaper.InsertEnvelopePoint(env, 0.0, 0.0, 0, 0, false, true)
    reaper.InsertEnvelopePoint(env, 0.5, 0.0, 0, 0, false, true)
    reaper.InsertEnvelopePoint(env, 2.5, 1.0, 0, 0, false, true)
    reaper.Envelope_SortPoints(env)
    renderTo(prefix .. "_automation")
else
    say("FAIL no 'Depth Low Band' parameter")
end

-- 4. Save the project and quit ---------------------------------------------------------------------------
reaper.Main_SaveProjectEx(0, path("out" .. sep .. prefix .. "_test.rpp"), 8) -- save supplied path without a dialog
say("saved " .. prefix .. "_test.rpp")
if log then log:close() end
-- [Codex 2026-10-30] Windows driver waits for startup informational dialogs before native Quit.
if os.getenv('WLA_DRIVER_QUIT') ~= '1' then reaper.Main_OnCommand(40004, 0) end
end
local until_time = reaper.time_precise() + 20
local function wait_for_module_check()
    local f = io.open(path('module-inspected'), 'r')
    if f then f:close() run_tests()
    elseif reaper.time_precise() > until_time then say('FAIL module inspection timed out') reaper.Main_OnCommand(40004, 0)
    else reaper.defer(wait_for_module_check) end
end
if os.getenv('WLA_VERIFY_MODULE') == '1' then wait_for_module_check() else run_tests() end
