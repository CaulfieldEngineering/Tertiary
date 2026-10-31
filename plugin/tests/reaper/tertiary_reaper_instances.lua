-- tertiary_reaper_instances.lua
-- [Claude 2026-10-31] T2-03 in a real host. Joe: "Can you do the 30 instance test inside of a DAW? Reaper?"
-- The requirement is that 30 instances play without dropouts in a real DAW. REAPER runs this at start-up
-- (run_reaper_instances.py starts it with an isolated profile at 48 kHz / 128 samples and REAPER's normal
-- processing settings, anticipative FX processing included):
--   1. a 48 kHz project with TERTIARY_COUNT tracks (default 30), each playing the same looped test input
--      through its own Tertiary (TERTIARY_FX: "VST3:Tertiary" or "AU:Tertiary") at defaults
--   2. plays it in real time for TERTIARY_SECONDS (default 60) through the configured audio device
--   3. reads reaper.GetUnderrunTime() (last audio and media xrun timestamps) before, during and after, and
--      writes the measurements to out/<prefix>_instances.json for the driver to judge (pass = 0 audio xruns)
-- Paths come from the environment, set by the driver: TERTIARY_TEST_DIR, TERTIARY_PREFIX.

local dir = os.getenv("TERTIARY_TEST_DIR") or reaper.GetResourcePath()
local sep = package.config:sub(1, 1)
local function path(name) return dir .. sep .. name end
local prefix = os.getenv("TERTIARY_PREFIX") or "reaper"
local count = tonumber(os.getenv("TERTIARY_COUNT") or "30")
local seconds = tonumber(os.getenv("TERTIARY_SECONDS") or "60")
local want = os.getenv("TERTIARY_FX") or "VST3:Tertiary"
local log = io.open(path("out" .. sep .. prefix .. "_log.txt"), "w")
local function say(text) if log then log:write(text .. "\n") log:flush() end end
local function quit() if os.getenv('WLA_DRIVER_QUIT') ~= '1' then reaper.Main_OnCommand(40004, 0) end end

reaper.Main_OnCommand(40023, 0)                                  -- File: New project
reaper.GetSetProjectInfo(0, "PROJECT_SRATE", 48000, true)
reaper.GetSetProjectInfo(0, "PROJECT_SRATE_USE", 1, true)
reaper.SetCurrentBPM(0, 120, false)
-- Master at -inf: nothing is heard from the test machine; every track and its Tertiary still process.
reaper.SetMediaTrackInfo_Value(reaper.GetMasterTrack(0), "D_VOL", 0)
local loaded, idents = 0, {}
for i = 0, count - 1 do
    reaper.InsertTrackAtIndex(i, true)
    local track = reaper.GetTrack(0, i)
    reaper.SetOnlyTrackSelected(track)
    reaper.SetEditCurPos(0, false, false)
    reaper.InsertMedia(path("input.wav"), 0)
    local item = reaper.GetTrackMediaItem(track, 0)
    reaper.SetMediaItemInfo_Value(item, "B_LOOPSRC", 1)          -- loop the short input for the whole run
    reaper.SetMediaItemInfo_Value(item, "D_LENGTH", seconds + 10)
    reaper.SetMediaTrackInfo_Value(track, "D_VOL", 1 / count)    -- keep the mix level sane; DSP load is the same
    local fx = reaper.TrackFX_AddByName(track, want, false, -1)
    if fx >= 0 and not reaper.TrackFX_GetOffline(track, fx) then
        loaded = loaded + 1
        local _, ident = reaper.TrackFX_GetNamedConfigParm(track, fx, 'fx_ident')
        idents[ident] = true
    end
end
local identList = {}
for k in pairs(idents) do identList[#identList + 1] = k end
say("REAPER " .. reaper.GetAppVersion() .. "; " .. loaded .. " of " .. count .. " " .. want .. " instances online")
if loaded > 0 then say("loaded Tertiary as FX 0: " .. table.concat(identList, " | ")) end

local function jstr(s) return '"' .. tostring(s):gsub('\\', '\\\\'):gsub('"', '\\"') .. '"' end
local function finish(result)
    local parts = {}
    for k, v in pairs(result) do
        parts[#parts + 1] = jstr(k) .. ": " .. (type(v) == "string" and jstr(v) or tostring(v))
    end
    local f = io.open(path("out" .. sep .. prefix .. "_instances.json"), "w")
    f:write("{" .. table.concat(parts, ", ") .. "}\n")
    f:close()
    say("instances-done")
    if log then log:close() log = nil end
    quit()
end

local function play_and_measure()
    local ok_srate, srate = reaper.GetAudioDeviceInfo("SRATE", "")
    local ok_bsize, bsize = reaper.GetAudioDeviceInfo("BSIZE", "")
    local _, mode = reaper.GetAudioDeviceInfo("MODE", "")
    local _, idev = reaper.GetAudioDeviceInfo("IDENT_IN", "")
    local _, odev = reaper.GetAudioDeviceInfo("IDENT_OUT", "")
    local audio0, media0 = reaper.GetUnderrunTime()
    local lastAudio, lastMedia = audio0, media0
    local audioEvents, mediaEvents, notPlaying, polls = 0, 0, 0, 0
    local tRef, posRef = nil, nil
    local xrunTimes = {}
    reaper.SetEditCurPos(0, false, false)
    reaper.OnPlayButton()
    local t0 = reaper.time_precise()
    say("play-start")
    local function poll()
        local now = reaper.time_precise()
        local a, m = reaper.GetUnderrunTime()
        polls = polls + 1
        -- Each new timestamp is at least one xrun since the previous poll (about 30 per second).
        if a ~= lastAudio then audioEvents = audioEvents + 1 lastAudio = a xrunTimes[#xrunTimes + 1] = string.format('audio@%.2f', now - t0) end
        if m ~= lastMedia then mediaEvents = mediaEvents + 1 lastMedia = m xrunTimes[#xrunTimes + 1] = string.format('media@%.2f', now - t0) end
        if reaper.GetPlayState() & 1 ~= 1 then notPlaying = notPlaying + 1 end
        -- Lost time: a device that cannot keep up falls behind the clock (REAPER's dummy device does this
        -- instead of flagging an xrun). Reference taken 1 s in, after playback has settled.
        if not tRef and now - t0 >= 1 then tRef, posRef = now, reaper.GetPlayPosition() end
        if now - t0 < seconds then reaper.defer(poll) return end
        local position = reaper.GetPlayPosition()
        local lost = tRef and ((now - tRef) - (position - posRef)) or -1
        reaper.OnStopButton()
        say("play-end")
        finish({ count = count, loaded = loaded, fx = want, idents = table.concat(identList, " | "),
                 seconds = now - t0, play_position = position, lost_seconds = lost, polls = polls, not_playing_polls = notPlaying,
                 audio_xruns = audioEvents, media_xruns = mediaEvents, xrun_times = table.concat(xrunTimes, ' '),
                 device_mode = mode, device_in = idev, device_out = odev,
                 srate = ok_srate and srate or "unknown", block = ok_bsize and bsize or "unknown",
                 anticipative = reaper.SNM_GetIntConfigVar and tostring(reaper.SNM_GetIntConfigVar("workrender", -1)) or "default",
                 reaper = reaper.GetAppVersion() })
    end
    reaper.defer(poll)
end

if loaded ~= count then
    say("FAIL only " .. loaded .. " of " .. count .. " instances loaded")
    finish({ count = count, loaded = loaded, fx = want, audio_xruns = -1, media_xruns = -1, seconds = 0 })
    return
end
-- Wait for the driver: on Windows it closes startup dialogs and checks the mapped binary first.
local until_time = reaper.time_precise() + 60
local function wait_for_driver()
    local f = io.open(path('module-inspected'), 'r')
    if f then f:close() play_and_measure()
    elseif reaper.time_precise() > until_time then say('FAIL module inspection timed out') finish({ audio_xruns = -1, media_xruns = -1, seconds = 0 })
    else reaper.defer(wait_for_driver) end
end
wait_for_driver()
