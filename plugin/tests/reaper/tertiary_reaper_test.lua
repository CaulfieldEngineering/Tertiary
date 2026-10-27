-- tertiary_reaper_test.lua
-- Written by Claude (2026-10-27). PREPARED, NOT RUN: REAPER isn't installed on the mini PC (Joe hasn't decided;
-- REAPER is a free evaluation, then a $60 licence). See README.md in this folder.
--
-- A ReaScript that REAPER runs at start-up (run_reaper_test.py starts it). It builds the test project from
-- scratch, so there is no hand-made plugin state in it:
--   1. a 48 kHz project, one track with the golden input (input.wav, written by HostTest --write-input)
--   2. Tertiary (VST3) on the track, all defaults; renders out/reaper_default.wav (compared with golden/default.wav)
--   3. an automation envelope on "Depth Low Band" (0 to 100 % over 2 s);
--      renders out/reaper_automation.wav (checked for sanity: no NaN, and different from the default render)
--   4. saves the project as tertiary_test.rpp (Tertiary with its automation), then quits REAPER
-- Paths come from the environment, set by run_reaper_test.py: TERTIARY_TEST_DIR (this folder).

local dir = os.getenv("TERTIARY_TEST_DIR") or reaper.GetResourcePath()
local sep = package.config:sub(1, 1)
local function path(name) return dir .. sep .. name end
local log = io.open(path("out" .. sep .. "reaper_log.txt"), "w")
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

-- 2. Tertiary --------------------------------------------------------------------------------------------
local fx = reaper.TrackFX_AddByName(track, "VST3:Tertiary (Wonderland Audio)", false, -1)
if fx < 0 then fx = reaper.TrackFX_AddByName(track, "Tertiary", false, -1) end
if fx < 0 then say("FAIL Tertiary not found in REAPER's plug-in list") reaper.Main_OnCommand(40004, 0) return end
say("loaded Tertiary as FX " .. fx)

-- Render settings: the whole project, master mix, 48 kHz stereo, 32-bit float WAV, no tail
local function renderTo(name)
    reaper.GetSetProjectInfo_String(0, "RENDER_FILE", path("out"), true)
    reaper.GetSetProjectInfo_String(0, "RENDER_PATTERN", name, true)
    reaper.GetSetProjectInfo_String(0, "RENDER_FORMAT", "ZXZhdyAAAQ==", true)   -- "evaw", 32-bit float (unverified)
    reaper.GetSetProjectInfo(0, "RENDER_SRATE", 48000, true)
    reaper.GetSetProjectInfo(0, "RENDER_CHANNELS", 2, true)
    reaper.GetSetProjectInfo(0, "RENDER_BOUNDSFLAG", 1, true)    -- entire project
    reaper.GetSetProjectInfo(0, "RENDER_SETTINGS", 0, true)      -- master mix
    reaper.GetSetProjectInfo(0, "RENDER_TAILFLAG", 0, true)
    reaper.Main_OnCommand(42230, 0)                              -- Render project, using the most recent settings
    say("rendered " .. name)
end
renderTo("reaper_default")

-- 3. Automation ------------------------------------------------------------------------------------------
local depth = -1
for i = 0, reaper.TrackFX_GetNumParams(track, fx) - 1 do
    local _, name = reaper.TrackFX_GetParamName(track, fx, i, "")
    if name == "Depth Low Band" then depth = i end
end
if depth >= 0 then
    local env = reaper.GetFXEnvelope(track, fx, depth, true)
    reaper.InsertEnvelopePoint(env, 0.0, 0.0, 0, 0, false, true)
    reaper.InsertEnvelopePoint(env, 2.0, 1.0, 0, 0, false, true)
    reaper.Envelope_SortPoints(env)
    renderTo("reaper_automation")
else
    say("FAIL no 'Depth Low Band' parameter")
end

-- 4. Save the project and quit ---------------------------------------------------------------------------
reaper.Main_SaveProjectEx(0, path("tertiary_test.rpp"), 0)
say("saved tertiary_test.rpp")
if log then log:close() end
reaper.Main_OnCommand(40004, 0)                                  -- File: Quit REAPER
