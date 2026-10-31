// [Codex 2026-10-30] Exercise the real Window size menu and detect clipping by the shipping web wrapper.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import config from './config.mjs';
import { snapshot, until, clickAt, clickControl, point, start } from './browser.mjs';

export async function testWindowSizes({ context, check, outDir }) {
  for (const percent of [100, 125, 150, 200]) {
    await check(`W-31-window-${percent}`, `Window size ${percent}% remains fully visible in the browser`, async () => {
      const { ctx, errors = [] } = await context();
      const page = ctx.pages()[0];
      try {
        await start(page);
        await page.locator('#btn-stop').click();
        await page.waitForTimeout(1200);
        const before = await snapshot(page);
        await clickAt(page, config.menu.x, config.menu.y);
        const menu = await until(() => snapshot(page), state => state.controls.some(c =>
          c.visible && c.key.startsWith('popup/') && c.title === 'Window size'), 'Window size menu item missing');
        const parent = menu.controls.find(c => c.visible && c.key.startsWith('popup/') && c.title === 'Window size');
        const hover = await point(page, parent.x + parent.width * .7, parent.y + parent.height / 2);
        await page.mouse.move(hover.x, hover.y);
        const submenu = await until(() => snapshot(page), state => state.controls.some(c =>
          c.visible && c.key.startsWith('popup/') && c.title === `${percent} %`), 'Window size submenu missing');
        const item = submenu.controls.find(c => c.visible && c.key.startsWith('popup/') && c.title === `${percent} %`);
        await page.waitForTimeout(230);
        await clickControl(page, item);
        const after = await until(() => snapshot(page), state =>
          !state.controls.some(c => c.visible && c.key.startsWith('popup/')) &&
          (percent === 100 || state.width > before.width), `Window size ${percent}% was not applied`);
        // The initial JUCE client area excludes its two border pixels, while an explicit
        // 100% menu selection reports the full 750px editor. Observe the resize without
        // treating that host border difference as a scale regression.
        await page.mouse.move(1, 1);
        await page.waitForTimeout(800);
        await page.evaluate(() => scrollTo(0, 0));
        const audit = await page.evaluate(() => {
          const rect = element => {
            const r = element.getBoundingClientRect(), css = getComputedStyle(element);
            return { left: r.left, top: r.top, right: r.right, bottom: r.bottom, width: r.width, height: r.height,
              overflowX: css.overflowX, overflowY: css.overflowY };
          };
          const canvas = document.getElementById(window._juceMainCanvasId);
          const drawn = rect(canvas), clipped = [];
          for (let ancestor = canvas.parentElement; ancestor; ancestor = ancestor.parentElement) {
            const r = rect(ancestor);
            const name = ancestor.id || ancestor.tagName.toLowerCase();
            if (['hidden', 'clip', 'scroll', 'auto'].includes(r.overflowX) &&
                (drawn.left < r.left - 1 || drawn.right > r.right + 1)) clipped.push(`${name}: horizontal clipping`);
            if (['hidden', 'clip', 'scroll', 'auto'].includes(r.overflowY) &&
                (drawn.top < r.top - 1 || drawn.bottom > r.bottom + 1)) clipped.push(`${name}: vertical clipping`);
          }
          const boxes = Object.fromEntries([['container', 'juce-container'], ['wrapper', 'wrapper'], ['stage', 'stage']]
            .map(([name, id]) => [name, rect(document.getElementById(id))]));
          return { canvas: { ...drawn, backingWidth: canvas.width, backingHeight: canvas.height }, ...boxes,
            viewportWidth: innerWidth, scrollWidth: document.documentElement.scrollWidth, clipped };
        });
        fs.mkdirSync(outDir, { recursive: true });
        const stem = `desktop-window-${percent}`;
        await page.screenshot({ path: path.join(outDir, `${stem}.png`), fullPage: true });
        const evidence = { percent, editor: { width: after.width, height: after.height }, ...audit };
        fs.writeFileSync(path.join(outDir, `${stem}.json`), JSON.stringify(evidence, null, 2));
        assert.deepEqual(errors, [], 'Console or request errors while resizing');
        assert.deepEqual(audit.clipped, [], `Window size ${percent}% crops the canvas: ${JSON.stringify(evidence)}`);
        assert.ok(audit.scrollWidth <= audit.viewportWidth + 1, `Horizontal page scroll: ${JSON.stringify(evidence)}`);
        assert.ok(audit.canvas.left >= -1 && audit.canvas.right <= audit.viewportWidth + 1,
          `Canvas extends outside the viewport: ${JSON.stringify(evidence)}`);
        return evidence;
      } catch(error) {
        if(fs.existsSync(path.join(outDir, `desktop-window-${percent}.png`)))
          error.screenshot=`desktop-window-${percent}.png`;
        throw error;
      } finally { await ctx.close(); }
    });
  }
}
