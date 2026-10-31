// [Codex 2026-10-30] Tertiary display options use real menu clicks and passive JUCE observations.
import assert from 'node:assert/strict';
import { snapshot, clickAt, clickControl, parameterChanges, toneWav, until } from './browser.mjs';

const scopeMenu = 'editor/0/2/1';
const fftMenu = 'editor/0/3/1';
const definitions = [
  { label: 'Low', parameter: 'Show Low Band In Scope', menu: scopeMenu, band: 'LOW' },
  { label: 'Mid', parameter: 'Show Mid Band In Scope', menu: scopeMenu, band: 'MID' },
  { label: 'High', parameter: 'Show High Band In Scope', menu: scopeMenu, band: 'HIGH' },
  { label: 'Stack Bands', parameter: 'Stack All Bands', menu: scopeMenu, stack: true },
  { label: 'Show FFT', parameter: 'Show FFT', menu: fftMenu, fft: true },
  { label: 'FFT Pre', parameter: 'FFT Pickoff Point', menu: fftMenu }
];
const parameter = (state, name) => state.parameters.find(value => value.name === name);
const bandBounds = (state, band) => state.controls.find(control =>
  control.key.startsWith('editor/0/2/0/') && control.kind === 'component' && control.name === band);

async function openOption(page, definition) {
  let state = await snapshot(page);
  const menu = state.controls.find(control => control.key === definition.menu);
  const button = state.controls.find(control => control.key === `${definition.menu}/0`);
  assert.ok(menu && button && button.enabled, `${definition.label} options button is missing`);
  // Options remain JUCE children after closing, but the panel clips them. Its height reveals whether it is open.
  if (menu.height < 40) await clickControl(page, button);
  state = await until(() => snapshot(page), current => {
    const parent = current.controls.find(control => control.key === definition.menu);
    return parent?.height > 40 && current.controls.some(control => control.key.startsWith(`${definition.menu}/`)
      && control.kind === 'button' && control.text === definition.label && control.visible);
  }, `${definition.label} option did not open`);
  const option = state.controls.find(control => control.key.startsWith(`${definition.menu}/`)
    && control.kind === 'button' && control.text === definition.label && control.visible);
  const parent = state.controls.find(control => control.key === definition.menu);
  assert.ok(option.x >= parent.x && option.y >= parent.y
    && option.x + option.width <= parent.x + parent.width + 1
    && option.y + option.height <= parent.y + parent.height + 1,
  `${definition.label} option is clipped by its menu panel`);
  return { state, option };
}

async function setOption(page, definition, wanted) {
  const { option } = await openOption(page, definition);
  if (Boolean(option.value) !== wanted) await clickControl(page, option);
  await until(() => snapshot(page), state => Boolean(parameter(state, definition.parameter)?.value) === wanted,
    `${definition.label} did not become ${wanted}`);
  await page.mouse.move(1, 1);
}

async function setMute(page, band, wanted) {
  const state = await snapshot(page);
  const button = state.controls.find(control => control.kind === 'button' && control.title === `${band} Mute`);
  assert.ok(button && button.visible && button.enabled, `${band} Mute button missing`);
  if (Boolean(button.value) !== wanted) await clickControl(page, button);
  await until(() => snapshot(page), current => Boolean(parameter(current, `Mute ${band} Band`)?.value) === wanted,
    `${band} mute did not become ${wanted}`);
}

async function defaultPreset(page) {
  const state = await snapshot(page);
  const previous = state.controls.find(control => control.tooltip === 'Previous preset');
  const next = state.controls.find(control => control.tooltip === 'Next preset');
  assert.ok(previous && next, 'Preset menu controls are missing');
  await clickAt(page, (previous.x + previous.width + next.x) / 2, previous.y + previous.height / 2);
  const menu = await until(() => snapshot(page), current => current.controls.some(control =>
    control.key.startsWith('popup/') && control.title === state.programs[0]), 'Default preset menu did not open');
  const item = menu.controls.find(control => control.key.startsWith('popup/') && control.title === state.programs[0]);
  await page.waitForTimeout(220);
  await clickControl(page, item);
  await until(() => snapshot(page), current => current.program === 0 && !current.controls.some(control => control.key.startsWith('popup/')),
    'Default preset did not load');
}

export async function testOptions({ page, check }) {
  const initial = await snapshot(page);
  for (const definition of definitions) {
    for (let direction = 0; direction < 2; direction++) {
      await check(`W-30-option-${definition.label.replaceAll(' ', '-')}-${direction}`,
        `${definition.label} display option changes parameter and presentation`, async () => {
          const { state: before, option } = await openOption(page, definition);
          const oldParameter = parameter(before, definition.parameter);
          assert.ok(oldParameter, `Missing parameter ${definition.parameter}`);
          const wanted = !Boolean(option.value);
          await clickControl(page, option);
          const after = await until(() => snapshot(page), current => {
            const value = parameter(current, definition.parameter);
            const control = current.controls.find(control => control.key.startsWith(`${definition.menu}/`)
              && control.kind === 'button' && control.text === definition.label);
            return value && Math.abs(value.value - oldParameter.value) > .5 && control?.value === wanted;
          }, `${definition.label} did not change its parameter and toggle state`);
          const changed = parameterChanges(before, after);
          assert.deepEqual(changed, [definition.parameter], 'A display option changed unrelated plugin parameters');
          let display;
          if (definition.band) {
            // Tertiary hides a scope by setting its size to zero, not by setting isVisible(false).
            const displayed = await until(() => snapshot(page), current => {
              const bounds = bandBounds(current, definition.band);
              return bounds && Boolean(bounds.width > 0 && bounds.height > 0) === wanted;
            }, `${definition.label} scope display did not follow its option`);
            const bounds = bandBounds(displayed, definition.band);
            display = { band: definition.band, width: bounds.width, height: bounds.height, drawn: bounds.width > 0 && bounds.height > 0 };
          } else if (definition.stack) {
            const displayed = await until(() => snapshot(page), current => {
              const bands = ['LOW', 'MID', 'HIGH'].map(band => bandBounds(current, band))
                .filter(bounds => bounds && bounds.width > 0 && bounds.height > 0);
              if (bands.length < 2) return false;
              const yPositions = new Set(bands.map(bounds => bounds.y));
              return wanted ? yPositions.size === bands.length : yPositions.size === 1;
            }, 'Stack Bands did not switch the scopes between separate rows and an overlap');
            display = ['LOW', 'MID', 'HIGH'].map(band => {
              const bounds = bandBounds(displayed, band);
              return { band, x: bounds?.x, y: bounds?.y, width: bounds?.width, height: bounds?.height };
            });
          } else if (definition.fft) {
            const displayed = await until(() => snapshot(page), current => current.fftVisible === wanted,
              'FFT visibility did not follow the Show FFT control');
            display = { fftVisible: displayed.fftVisible };
          } else {
            // FFT Pre changes the analysis source, not the audio output. With stopped audio no spectral change is promised.
            display = { pickoff: parameter(after, definition.parameter).text,
              note: 'Analysis source state checked; no claim that stopped audio changes the spectrum.' };
          }
          return { label: definition.label, value: wanted, parameters: changed, display };
        });
    }
    // Leaving the panel closes it through the shipping hover handler.
    await page.mouse.move(1, 1);
  }
  await check('W-30-options-restored', 'Display options return to their initial state', async () => {
    const final = await snapshot(page);
    for (const definition of definitions)
      assert.equal(parameter(final, definition.parameter)?.value, parameter(initial, definition.parameter)?.value,
        `${definition.label} did not return to its initial setting`);
    return { restored: definitions.map(definition => definition.parameter) };
  });
  await check('W-31-fft-pickoff-display', 'FFT Pre displays live input while muted output falls to the floor', async () => {
    const show = definitions.find(definition => definition.fft);
    const pre = definitions.find(definition => definition.label === 'FFT Pre');
    const originalShow = Boolean(parameter(initial, show.parameter)?.value);
    const originalPre = Boolean(parameter(initial, pre.parameter)?.value);
    const looped = await page.evaluate(() => looping);
    const title = 'web-options-spectrum';
    let uploaded = false;
    try {
      assert.ok(Number.isFinite((await snapshot(page)).fftPeak), 'Read-only FFT peak observation is missing');
      await defaultPreset(page);
      // Muting all bands through the actual controls gives an unambiguous silent output, independent of LFO phase.
      for (const band of ['Low', 'Mid', 'High']) await setMute(page, band, true);
      if (!looped) await page.locator('#btn-loop').click();
      await page.locator('#file-upload').setInputFiles({ name: `${title}.wav`, mimeType: 'audio/wav', buffer: toneWav(8) });
      uploaded = true;
      await page.waitForFunction(title => TRACKS[currentTrackIdx]?.title === title && playing
        && sourceNode?.buffer === audioBuffer && audioBuffer?.duration > 7.9 && audioCtx?.state === 'running', title);
      await setOption(page, show, true);
      await setOption(page, pre, true);
      const before = await until(() => snapshot(page), state => state.fftVisible && state.fftPeak > .25,
        'FFT Pre did not display the live input spectrum', 8000);
      await setOption(page, pre, false);
      const muted = await until(() => snapshot(page), state => state.fftVisible && state.fftPeak < .01,
        'Output FFT did not fall to the floor with all three bands muted', 8000);
      await setOption(page, pre, true);
      const restored = await until(() => snapshot(page), state => state.fftVisible && state.fftPeak > .25,
        'FFT Pre did not restore the live input spectrum', 8000);
      assert.ok(before.fftPeak - muted.fftPeak > .25 && restored.fftPeak - muted.fftPeak > .25);
      return { inputPeak: before.fftPeak, mutedOutputPeak: muted.fftPeak, inputAgainPeak: restored.fftPeak };
    } finally {
      // Restore through UI gestures even when an assertion fails; never leave a muted plugin playing a test file.
      await page.locator('#btn-stop').click();
      for (const band of ['Low', 'Mid', 'High']) await setMute(page, band, false);
      await defaultPreset(page);
      await setOption(page, show, originalShow);
      await setOption(page, pre, originalPre);
      if (await page.evaluate(() => looping) !== looped) await page.locator('#btn-loop').click();
      if (uploaded) {
        const index = await page.evaluate(title => TRACKS.findIndex(track => track._userUpload && track.title === title), title);
        if (index >= 0) await page.locator('#track-list .playlist-track').nth(index).locator('.playlist-track__delete').click();
      }
      // Removal already stops and disables transport when its selected file is gone.
      if (await page.locator('#btn-stop').isEnabled()) await page.locator('#btn-stop').click();
      await page.mouse.move(1, 1);
    }
  });
}
