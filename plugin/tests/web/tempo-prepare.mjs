// [Claude 2026-11-03] For Wonderlab's tempo tests (tests/web/tempo.test.mjs --prepare): they assume the plugin's
// Default sound (every band a synced Sine at 1/4 note, 75 % depth, 0 degrees), and Tertiary 1.1.1's web demo opens on
// "Gentle Movement". This chooses Default from the preset menu, the same way the web suite's choosePreset does, so
// the tempo tests run against the web build that ships. Needs the test build (the inspector's webtest_snapshot).
import config from './config.mjs';
import { snapshot, until, clickAt, clickControl, menuItem } from './browser.mjs';

export default async function chooseDefault(page) {
  const before = await snapshot(page);
  const name = before.programs[0];
  if (before.program === 0) return;
  await clickAt(page, config.preset.x, config.preset.y);
  const { item } = await menuItem(page, s => s.controls.find(c => c.key.startsWith('popup/') && c.visible && c.title === name),
    `Preset menu did not open with ${name}`);
  await clickControl(page, item);
  await until(() => snapshot(page), s => s.program === 0 && !s.controls.some(c => c.key.startsWith('popup/')),
    'Default preset did not load');
  await page.mouse.move(1, 1);
}
