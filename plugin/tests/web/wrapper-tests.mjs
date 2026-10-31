// [Codex 2026-10-30] Test Wonderlab's real player controls independently of a plugin's parameter layout.
import assert from 'node:assert/strict';
import { audioSummary, toneWav } from './browser.mjs';

export async function testPlayer({ page, check }) {
  const initial = await page.evaluate(() => ({
    bypassed, looping,
    tracks: TRACKS.map((track, index) => ({ index, title: track.title, uploaded: !!track._userUpload }))
      .filter(track => !track.uploaded)
  }));
  const read = () => page.evaluate(() => ({
    playing, looping, bypassed, index: currentTrackIdx, offset: pauseOffset, time: getCurrentTime(),
    duration: audioBuffer?.duration || 0, context: audioCtx?.state || 'not-created',
    title: trackName.textContent, sourceLoop: sourceNode?.loop, sourcePresent: !!sourceNode
  }));
  const playingReady = () => page.waitForFunction(() =>
    playing && !!sourceNode && !!audioBuffer && audioCtx?.state === 'running', null, { timeout: 20000 });
  async function selectTrack(track) {
    await page.locator('#track-list .playlist-track').nth(track.index).click();
    await page.waitForFunction(({ index, title }) => {
      const track = TRACKS[index];
      return currentTrackIdx === index && track?.title === title && !track._loading
        && audioBuffer?.duration > 0 && audioBuffer === _decoded.get(track)
        && sourceNode?.buffer === audioBuffer && playing && audioCtx?.state === 'running';
    }, track, { timeout: 20000 });
    const state = await read();
    assert.equal(state.title, track.title);
    assert.equal(await page.locator('#track-list .playlist-track').nth(track.index).getAttribute('aria-current'), 'true');
    assert.equal(await page.locator('#status').evaluate(element => element.classList.contains('is-error')), false);
    return state;
  }
  async function setBypass(wanted) {
    if ((await read()).bypassed !== wanted) await page.locator('#btn-bypass').click();
    await page.waitForFunction(wanted => bypassed === wanted, wanted);
    assert.equal(await page.locator('#btn-bypass').getAttribute('aria-pressed'), String(wanted));
  }
  async function setLoop(wanted) {
    if ((await read()).looping !== wanted) await page.locator('#btn-loop').click();
    await page.waitForFunction(wanted => looping === wanted && (!sourceNode || sourceNode.loop === wanted), wanted);
    assert.equal(await page.locator('#btn-loop').getAttribute('aria-pressed'), String(wanted));
  }
  async function stopPlayer() {
    await page.locator('#btn-stop').click();
    await page.waitForFunction(() => !playing && pauseOffset === 0 && getCurrentTime() === 0 && sourceNode === null);
  }

  const uploadTitle = 'wonderlab-wrapper-tone';
  let uploadedIndex = -1;
  await check('W-20-playlist', 'Player exposes its built-in sample tracks', async () => {
    assert.ok(initial.tracks.length > 0, 'No built-in sample tracks were found');
    assert.ok(await page.locator('#track-list .playlist-track').count() >= initial.tracks.length);
    return { tracks: initial.tracks };
  });
  for (const track of initial.tracks) {
    await check(`W-21-track-${track.index + 1}`, `Playlist loads and plays ${track.title}`, async () => {
      const state = await selectTrack(track);
      // The runner's existing request/console watcher checks the fetch and decoding path.
      return { ...state, decodedCorrectTrack: true };
    });
  }
  await check('W-22-player-bypass', 'Player A/B switch changes state in both directions', async () => {
    await setBypass(false);
    await setBypass(true);
    assert.equal(await page.locator('#btn-bypass .ab-off').evaluate(element => element.classList.contains('is-on')), true);
    await setBypass(false);
    assert.equal(await page.locator('#btn-bypass .ab-on').evaluate(element => element.classList.contains('is-on')), true);
    // A known dry input makes subsequent transport checks independent of plugin settings.
    await setBypass(true);
    return { finalBypassed: (await read()).bypassed };
  });
  await check('W-23-upload', 'Upload label opens a file chooser and plays the decoded tone', async () => {
    const beforeCount = await page.locator('#track-list .playlist-track').count();
    const chooserPromise = page.waitForEvent('filechooser');
    await page.locator('#upload-btn').click();
    const chooser = await chooserPromise;
    await chooser.setFiles({ name: `${uploadTitle}.wav`, mimeType: 'audio/wav', buffer: toneWav(8) });
    await page.waitForFunction(title => {
      const track = TRACKS[currentTrackIdx];
      return track?.title === title && track._userUpload && !track._loading
        && audioBuffer === _decoded.get(track) && Math.abs(audioBuffer.duration - 8) < .01
        && sourceNode?.buffer === audioBuffer && playing && audioCtx?.state === 'running';
    }, uploadTitle, { timeout: 20000 });
    uploadedIndex = (await read()).index;
    assert.equal(await page.locator('#track-list .playlist-track').count(), beforeCount + 1);
    assert.equal(await page.locator('#track-name').innerText(), uploadTitle);
    await page.waitForTimeout(300);
    const audio = await audioSummary(page, 700);
    assert.ok(audio.mean > .01, `Uploaded tone is silent: ${JSON.stringify(audio)}`);
    return { index: uploadedIndex, duration: (await read()).duration, audio };
  });
  await check('W-24-pause', 'Pause freezes position and the actual output becomes silent', async () => {
    assert.ok(uploadedIndex >= 0, 'Uploaded test tone is unavailable');
    await playingReady();
    const before = await read();
    await page.locator('#btn-play').click();
    await page.waitForFunction(() => !playing && sourceNode === null);
    assert.equal(await page.locator('#btn-play').getAttribute('aria-label'), 'Play');
    const paused = await read();
    assert.equal(paused.context, 'running');
    assert.ok(paused.offset > 0, 'Pause did not retain the playback offset');
    assert.ok(Math.abs(paused.time - before.time) < .75, 'Pause unexpectedly jumped to another position');
    await page.waitForTimeout(400); // Allow the shipping 150 ms fade and analyser window to drain.
    const audio = await audioSummary(page, 400);
    assert.ok(audio.max < .0001, `Paused output still contains audio: ${JSON.stringify(audio)}`);
    const after = await read();
    assert.equal(after.time, paused.time);
    return { before, paused, audio };
  });
  await check('W-25-resume', 'Play resumes from the paused position and produces audio', async () => {
    assert.ok(uploadedIndex >= 0, 'Uploaded test tone is unavailable');
    const paused = await read();
    assert.equal(paused.playing, false, 'Expected the preceding pause to have stopped playback');
    await page.locator('#btn-play').click();
    await playingReady();
    assert.equal(await page.locator('#btn-play').getAttribute('aria-label'), 'Pause');
    const resumed = await read();
    assert.ok(resumed.time >= paused.offset && resumed.time < paused.offset + 1,
      `Resume did not preserve the paused offset: ${JSON.stringify({ paused, resumed })}`);
    await page.waitForTimeout(250);
    const audio = await audioSummary(page, 500);
    assert.ok(audio.mean > .01, `Resumed tone is silent: ${JSON.stringify(audio)}`);
    return { paused, resumed, audio };
  });
  await check('W-26-stop', 'Stop clears position and silences actual output', async () => {
    assert.ok(uploadedIndex >= 0, 'Uploaded test tone is unavailable');
    await stopPlayer();
    await page.waitForTimeout(400);
    const audio = await audioSummary(page, 400);
    assert.ok(audio.max < .0001, `Stopped output still contains audio: ${JSON.stringify(audio)}`);
    assert.equal(Number(await page.locator('#progress').inputValue()), 0);
    return { state: await read(), audio };
  });
  await check('W-27-seek', 'Keyboard seek changes the paused offset and resumed playback position', async () => {
    assert.ok(uploadedIndex >= 0, 'Uploaded test tone is unavailable');
    await stopPlayer();
    const progress = page.locator('#progress');
    await progress.focus();
    await progress.press('Home');
    for (let i = 0; i < 3; i++) await progress.press('PageUp');
    const value = Number(await progress.inputValue());
    assert.ok(value > 0 && value < 1000, `Seek did not move the range control: ${value}`);
    const sought = await read();
    const expected = value / 1000 * sought.duration;
    assert.ok(Math.abs(sought.offset - expected) < .02, `Seek offset ${sought.offset} differs from ${expected}`);
    assert.ok(Math.abs(sought.time - expected) < .02);
    assert.equal(sought.playing, false);
    await page.waitForFunction(expected => {
      const currentLabel = document.getElementById('time').textContent.split(' / ')[0];
      return currentLabel === formatTime(expected);
    }, expected);
    await page.locator('#btn-play').click();
    await playingReady();
    const resumed = await read();
    assert.ok(resumed.time >= expected && resumed.time < expected + 1,
      `Play ignored the seek position: ${JSON.stringify({ expected, resumed })}`);
    return { value, expected, sought, resumed };
  });
  await check('W-28-loop', 'Loop changes aria state and the playing AudioBufferSource in both directions', async () => {
    await playingReady();
    await setLoop(false);
    const off = await read();
    assert.equal(off.sourceLoop, false);
    await setLoop(true);
    const on = await read();
    assert.equal(on.sourceLoop, true);
    return { off, on };
  });
  await check('W-29-remove-upload', 'Remove track deletes the upload and stops its source', async () => {
    assert.ok(uploadedIndex >= 0, 'Uploaded test tone is unavailable');
    const before = await page.locator('#track-list .playlist-track').count();
    await page.locator('#track-list .playlist-track').nth(uploadedIndex).locator('.playlist-track__delete').click();
    await page.waitForFunction(title => !TRACKS.some(track => track._userUpload && track.title === title)
      && !playing && audioBuffer === null, uploadTitle);
    assert.equal(await page.locator('#track-list .playlist-track').count(), before - 1);
    assert.equal(await page.locator('#track-name').innerText(), '', 'Removed upload filename remains visible');
    assert.equal((await read()).index, -1, 'A different track is falsely selected');
    assert.equal(await page.locator('#track-list [aria-current="true"]').count(), 0);
    assert.equal((await read()).sourcePresent, false);
    assert.equal((await read()).offset, 0);
    assert.equal(Number(await page.locator('#progress').inputValue()), 0);
    assert.equal(await page.locator('#file-upload').inputValue(), '');
    assert.equal(await page.locator('#btn-play').isDisabled(), true);
    assert.equal(await page.locator('#btn-stop').isDisabled(), true);
    assert.equal(await page.locator('#progress').isDisabled(), true);
    await page.waitForTimeout(500);
    assert.equal(await page.locator('#track-name').innerText(), '', 'A stale load restored the filename');
    return { count: before - 1, state: await read() };
  });
  await check('W-29-restore-player', 'Player returns to its first built-in track, stopped', async () => {
    assert.ok(initial.tracks.length > 0, 'No first built-in track to restore');
    await selectTrack(initial.tracks[0]);
    await setBypass(initial.bypassed);
    await setLoop(initial.looping);
    await stopPlayer();
    return read();
  });
}
