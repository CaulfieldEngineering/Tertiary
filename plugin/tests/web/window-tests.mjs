// Joe's revised requirement: keep size fields disabled; browser dimensions own fitting.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import config from './config.mjs';
import { snapshot, until, clickAt, clickControl, point, start } from './browser.mjs';

export async function fitAudit(page) {
  await page.evaluate(() => scrollTo(0, 0));
  await page.waitForTimeout(350);
  const audit = await page.evaluate(() => {
    const rect = el => {
      const r = el.getBoundingClientRect(), css = getComputedStyle(el);
      return {left:r.left, top:r.top, right:r.right, bottom:r.bottom, width:r.width, height:r.height,
        overflowX:css.overflowX, overflowY:css.overflowY};
    };
    const canvas = document.getElementById(window._juceMainCanvasId), drawn = rect(canvas), clipped = [];
    for(let parent=canvas.parentElement;parent;parent=parent.parentElement) {
      const r=rect(parent), name=parent.id||parent.tagName;
      if (['hidden','clip','scroll','auto'].includes(r.overflowX) && (drawn.left<r.left-1||drawn.right>r.right+1)) clipped.push(`${name}: horizontal`);
      if (['hidden','clip','scroll','auto'].includes(r.overflowY) && (drawn.top<r.top-1||drawn.bottom>r.bottom+1)) clipped.push(`${name}: vertical`);
    }
    return {canvas:drawn, stage:rect(document.getElementById('stage')), width:innerWidth,
      height:visualViewport?.height||innerHeight, scrollWidth:document.documentElement.scrollWidth, clipped};
  });
  assert.deepEqual(audit.clipped, [], JSON.stringify(audit));
  assert.ok(audit.scrollWidth<=audit.width+1, `Horizontal page scroll: ${JSON.stringify(audit)}`);
  assert.ok(audit.canvas.left>=-1 && audit.canvas.right<=audit.width+1 && audit.canvas.top>=-1 && audit.canvas.bottom<=audit.height+1,
    `Canvas exceeds the visible browser area: ${JSON.stringify(audit)}`);
  return audit;
}

export async function tapControl(page, c) {
  const p=await point(page,c.x+c.width/2,c.y+c.height/2);
  await page.touchscreen.tap(p.x,p.y);
}

async function dismissMenus(page,touch) {
  if(!(await snapshot(page)).controls.some(c=>c.visible&&c.key.startsWith('popup/')))return;
  // Keyboard Escape belongs to a focused HTML player button. Dismiss by a real
  // outside gesture on an inert plugin label, as a mouse/touch visitor would.
  const p=await point(page,200,465);
  if(touch)await page.touchscreen.tap(p.x,p.y);else await page.mouse.click(p.x,p.y);
  await until(()=>snapshot(page),s=>!s.controls.some(c=>c.visible&&c.key.startsWith('popup/')),'Menu did not dismiss');
}

export async function disabledSizes(page, touch=false, capture) {
  const before=await snapshot(page), activate=touch?tapControl:clickControl;
  assert.deepEqual(before.sizeOptions.filter(o=>/ %$/.test(o.text)).map(o=>o.text), ['100 %','125 %','150 %','200 %']);
  assert.ok(before.sizeOptions.every(o=>!o.enabled), 'A size field is enabled');
  for (const pct of [100,125,150,200]) {
    await dismissMenus(page,touch);
    const menuPoint=await point(page,config.menu.x,config.menu.y);
    if(touch) await page.touchscreen.tap(menuPoint.x,menuPoint.y);
    else await clickAt(page,config.menu.x,config.menu.y);
    const menu=await until(()=>snapshot(page),s=>s.controls.some(c=>c.visible&&c.title==='Window size'),'Window size menu missing');
    await page.waitForTimeout(250);
    await activate(page,menu.controls.find(c=>c.visible&&c.title==='Window size'));
    const sub=await until(()=>snapshot(page),s=>s.controls.some(c=>c.visible&&c.title===`${pct} %`),'Size fields are not reachable by tap/click');
    assert.ok(sub.controls.some(c=>c.visible&&c.title==='Resize your browser to change the size'));
    await page.waitForTimeout(250);
    if(capture) await capture(pct);
    await activate(page,sub.controls.find(c=>c.visible&&c.title===`${pct} %`));
    await page.waitForTimeout(250);
    const after=await snapshot(page);
    assert.equal(after.width,before.width,`Disabled ${pct}% changed the editor width`);
    assert.equal(after.height,before.height,`Disabled ${pct}% changed the editor height`);
    assert.deepEqual(after.parameters,before.parameters);
  }
  await dismissMenus(page,touch);
  return {sizeOptions:before.sizeOptions, width:before.width, height:before.height};
}

export async function testWindowSizes({context,check,outDir}) {
  const {ctx,errors=[]}=await context(),page=ctx.pages()[0];
  try {
    await start(page);await page.locator('#btn-stop').click();await page.waitForTimeout(1200);
    await check('W-31-window-disabled','Size fields are present, disabled, and inert when clicked',()=>disabledSizes(page,false,async pct=>{
      await page.screenshot({path:path.join(outDir,`desktop-window-${pct}-disabled.png`),fullPage:true});
    }));
    await check('W-32-resize-open-menu','Resize dismisses menus with stale screen coordinates',async()=>{
      await clickAt(page,config.menu.x,config.menu.y);
      await until(()=>snapshot(page),s=>s.controls.some(c=>c.visible&&c.key.startsWith('popup/')),'Menu did not open');
      await page.setViewportSize({width:390,height:664});
      await until(()=>snapshot(page),s=>!s.controls.some(c=>c.visible&&c.key.startsWith('popup/')),'Menu stayed outside the resized viewport');
      return fitAudit(page);
    });
    for(const viewport of [
      {name:'desktop',width:1440,height:900}, {name:'desktop-short',width:1100,height:400},
      {name:'tablet',width:820,height:1180}, {name:'phone',width:390,height:664},
      {name:'phone-landscape',width:750,height:342}, {name:'desktop-restored',width:1440,height:900}
    ]) await check(`W-32-resize-${viewport.name}`,'Browser resize re-fits the entire canvas',async()=>{
      await page.setViewportSize({width:viewport.width,height:viewport.height});
      const audit=await fitAudit(page);
      await page.screenshot({path:path.join(outDir,`${viewport.name}-fit-after.png`),fullPage:true});
      fs.writeFileSync(path.join(outDir,`${viewport.name}-fit-after.json`),JSON.stringify(audit,null,2));
      assert.deepEqual(errors,[]);return audit;
    });
  } finally {await ctx.close();}
}
