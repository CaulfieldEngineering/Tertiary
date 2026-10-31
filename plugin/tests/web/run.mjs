// [Codex 2026-10-30] Matches wla-site-tests: Node ESM + Playwright, sequential checks, JSON/HTML reports and screenshots.
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { chromium, webkit, devices } from 'playwright';
import { testMobile } from './mobile-tests.mjs';
import { noActivation, disabledActivation, waitPastNagCycle } from './activation-tests.mjs';
import { PNG } from 'pngjs';
import pixelmatch from 'pixelmatch';
import config from './config.mjs';
import { here, root, name, webDir, outDir, refsDir } from './paths.mjs';
import { serve } from './serve.mjs';
import { compareScreenshot } from './visual.mjs';
import { reviewDifferences } from './ai-review.mjs';
import { testPlayer } from './wrapper-tests.mjs';
import { snapshot, until, point, clickAt, clickControl, dragControl, selectItem, controlChanged,
  parameterChanges, audioSummary, renderAudio, toneWav, start } from './browser.mjs';

fs.mkdirSync(outDir, {recursive:true});
const update = process.argv.includes('--update-refs');
const engineArg=process.argv.find(a=>a.startsWith('--engine='));
const engine=engineArg?.split('=')[1];
if(engine && !['chromium','webkit'].includes(engine))throw new Error('Expected --engine=chromium or --engine=webkit');
if (update && process.env.CI) throw new Error('CI must never approve screenshot references');
const meta = JSON.parse(fs.readFileSync(path.join(webDir,'test-build.json'),'utf8'));
const commit = execFileSync('git',['rev-parse','HEAD'],{cwd:root,encoding:'utf8'}).trim();
const rows=[], differences=[];
let page, browser, mobileWebkit, server;
function safe(s) { return s.replace(/[^a-zA-Z0-9_-]+/g,'-').slice(0,140); }
async function check(id, title, fn) {
  const row = {id,title,status:'pass'};
  try { row.data = await fn(); }
  catch(e) {
    row.status='fail'; row.error=e.message;
    if(e.screenshot && path.basename(e.screenshot)===e.screenshot) row.screenshot=e.screenshot;
    else if(page && !page.isClosed()) { row.screenshot=`${safe(id)}-failure.png`; await page.screenshot({path:path.join(outDir,row.screenshot),fullPage:true}).catch(()=>{}); }
    if(page && !page.isClosed()) await page.keyboard.press('Escape').catch(()=>{});
  }
  rows.push(row); console.log(`${row.status.toUpperCase()} ${id} ${title}${row.error ? ': '+row.error.slice(0,350) : ''}`);
  return row.status==='pass';
}
async function shot(id) {
  const actual=path.join(outDir,`${id}.png`), reference=path.join(refsDir,`${id}.png`), diff=path.join(outDir,`${id}-diff.png`);
  await page.mouse.move(1,1);
  await page.screenshot({path:actual, fullPage:true, animations:'disabled'});
  const result=compareScreenshot({actual,reference,diff,update,...config.pixels});
  if(result.changedPixels || (!result.pass && fs.existsSync(reference))) differences.push({name:id,actual,reference,diff});
  assert.ok(result.pass, result.reason + ` (${result.changedPixels} pixels, ${(result.ratio*100).toFixed(3)}%)`);
  return result;
}
function watch(p) {
  const errors=[];
  p.on('console',m=>{if(m.type()==='error')errors.push(`console: ${m.text()}`);});
  p.on('pageerror',e=>errors.push(`uncaught: ${e.message}`));
  p.on('requestfailed',r=>errors.push(`request: ${r.url()} ${r.failure()?.errorText}`));
  p.on('response',r=>{if(r.status()>=400)errors.push(`HTTP ${r.status()}: ${r.url()}`);});
  return errors;
}
async function context(view, fallback=false, engine=browser) {
  const profile=view.device ? devices[view.device] : {viewport:{width:view.width,height:view.height},deviceScaleFactor:1,
    isMobile:!!view.touch,hasTouch:!!view.touch};
  const ctx=await engine.newContext({...profile,locale:'en-US',timezoneId:'UTC',reducedMotion:'reduce'});
  // Explicit analytics-only allowlist. No plugin, WASM, font, audio or UI requests are stubbed.
  await ctx.route(url=>config.analyticsHosts.includes(url.hostname), r=>r.fulfill({status:200,contentType:'text/javascript',body:''}));
  if(fallback) await ctx.addInitScript(()=>Object.defineProperty(window,'SharedArrayBuffer',{value:undefined}));
  page=await ctx.newPage(); const errors=watch(page);
  await page.goto(server.url,{waitUntil:'load'});
  await page.waitForFunction(()=>typeof Module !== 'undefined' && typeof Module._webtest_snapshot==='function' &&
    !document.querySelector('#big-play').disabled,null,{timeout:60000});
  await page.evaluate(()=>document.fonts.ready);
  return {ctx,errors};
}
async function stop() {
  await page.locator('#btn-stop').click();
  await page.waitForTimeout(1200);
}
async function cropScope(region=config.scope) {
  if(!region) return null;
  const a=await point(page,region.x,region.y), b=await point(page,region.x+region.width,region.y+region.height);
  return PNG.sync.read(await page.screenshot({clip:{x:a.x,y:a.y,width:b.x-a.x,height:b.y-a.y}}));
}
function scopeDifference(a,b) { if(!a||!b)return 0; return pixelmatch(a.data,b.data,null,a.width,a.height,{threshold:.15}); }
function controls(s,kind) { return s.controls.filter(c=>c.kind===kind && c.visible && c.enabled && c.alpha!==0 && c.key.startsWith('editor') && c.width>0 && c.height>0); }
async function choosePreset(index=0) {
  if(!config.preset)return;
  const before=await snapshot(page);
  await clickAt(page,config.preset.x,config.preset.y);
  const menu=await until(()=>snapshot(page),s=>s.controls.some(c=>c.key.startsWith('popup/')),'Preset menu did not open');
  const item=menu.controls.find(c=>c.key.startsWith('popup/')&&c.title===before.programs[index]);
  assert.ok(item,`Preset ${before.programs[index]} missing`);
  await page.waitForTimeout(230);await clickControl(page,item);
  await until(()=>snapshot(page),s=>s.program===index&&!s.controls.some(c=>c.key.startsWith('popup/')),'Preset did not close and load');
  await page.mouse.move(1,1);await page.waitForTimeout(300);
}
async function changedState(before,c) {
  return until(()=>snapshot(page),s=>controlChanged(c,s.controls.find(x=>x.key===c.key)),`Control ${c.key} did not change`);
}
async function gestureCheck(id,c,gesture,{reset=true, prepare}={}) {
  await check(id,`${c.kind} ${c.name || c.tooltip || c.text || c.key}`,async()=>{
    if(reset)await choosePreset(0);
    if(prepare)await prepare();
    c=(await snapshot(page)).controls.find(x=>x.key===c.key);
    assert.ok(c?.visible,'Control missing or hidden');
    const sound=config.gainSliderKeys?.includes(c.key) || /\/(9|10|11)\/0$/.test(c.key);
    const audioBefore=sound?await renderAudio(page):null;
    const before=await snapshot(page);
    await gesture(c);
    const after=await changedState(before,c), params=parameterChanges(before,after);
    assert.ok(params.length,`UI changed without any parameter change (${c.key})`);
    await page.waitForTimeout(500);
    const settled=await snapshot(page);
    const scopeChanged=JSON.stringify(before.scopes)!==JSON.stringify(settled.scopes);
    const audioAfter=sound?await renderAudio(page):null;
    const audioDelta=sound?Math.abs(audioBefore.rms-audioAfter.rms)/Math.max(audioBefore.rms,audioAfter.rms):null;
    const displayOnly=params.every(p=>/Show |Scope |Stack |FFT/.test(p));
    if(sound&&!displayOnly) assert.ok(audioDelta>.03,`No measured level change: ${audioDelta}`);
    else if(config.scope) assert.ok(scopeChanged,`Parameters changed but scope path/state did not: ${params}`);
    return {control:c.key,parameters:params,valueBefore:c.value,valueAfter:after.controls.find(x=>x.key===c.key)?.value,audioDelta,scopeChanged};
  });
}
try {
  server=await serve();
  if(engine!=='webkit')browser=await chromium.launch({headless:true,args:['--autoplay-policy=user-gesture-required']});
  await check('W-00','Built commit is the checkout being tested',async()=>{assert.equal(meta.commit,commit);return meta;});
  if (engine!=='webkit' && !process.argv.includes('--mobile-only')) {
  for(const view of config.viewports) {
    let ctx,errors;
    const ok=await check(`W-01-${view.name}`,'Loads, ready, correct config build ID and isolation',async()=>{
      ({ctx,errors}=await context(view));
      const state=await page.evaluate(()=>({id:WONDERLAB_CONFIG.buildId,isolated:crossOriginIsolated,state:audioCtx?.state||'not-created',playing}));
      assert.equal(state.id,meta.commit.slice(0,8)); assert.equal(state.isolated,true);
      assert.equal(state.playing,false); assert.notEqual(state.state,'running');
      assert.deepEqual(errors,[]); return state;
    });
    if(!ok){await ctx?.close();continue;}
    await check(`W-02-${view.name}`,'User gesture starts real audio',async()=>{
      await start(page); const a=await audioSummary(page,1000); assert.equal(a.state,'running'); assert.ok(a.mean>.0001,`Silent output ${JSON.stringify(a)}`);return a;
    });
    await check(`W-03-${view.name}`,'Viewport has no clipping or horizontal scroll; controls are reachable',async()=>{
      await stop();
      const state=await snapshot(page);
      const invalid=state.controls.filter(c=>c.visible && c.enabled && c.alpha!==0 && ['slider','button','combo'].includes(c.kind) && c.key.startsWith('editor') &&
        (c.x<0 || c.y<0 || c.x+c.width>state.width+1 || c.y+c.height>state.height+1));
      assert.deepEqual(invalid,[],'JUCE controls outside the editor');
      const audit=await page.evaluate(()=>{
        const bad=[];
        for(const el of document.querySelectorAll('#deck button,#progress,#upload-btn')) {
          const r=el.getBoundingClientRect(); if(r.width===0||r.height===0)continue;
          if(r.left < -1 || r.right > innerWidth+1) bad.push(`${el.id||el.textContent}: x=${r.left}..${r.right}`);
        }
        return {width:innerWidth,scrollWidth:document.documentElement.scrollWidth,bad};
      });
      assert.ok(audit.scrollWidth<=audit.width+1,JSON.stringify(audit));assert.deepEqual(audit.bad,[]);
      for(const selector of ['#btn-play','#btn-loop','#btn-bypass','#upload-btn']) await page.locator(selector).click({trial:true});
      await page.evaluate(()=>scrollTo(0,0));
      return {audit,controlCount:state.controls.filter(c=>c.visible&&['slider','button','combo'].includes(c.kind)).length};
    });
    await check(`W-04-${view.name}`,'Approved screenshot reference',()=>shot(`${view.name}-baseline`));
    if(view.name==='phone' && config.scope) {
      for(const label of ['Crossover','Low','Mid','High']) await check(`W-05-phone-${label}`,`Phone zoom ${label} reaches controls`,async()=>{
        await page.getByRole('button',{name:label,exact:true}).click();await page.waitForTimeout(350);
        const active=await page.locator('#zoom-bar .is-on').innerText();assert.equal(active,label);
        const state=await snapshot(page);
        const candidates=controls(state,'slider').filter(c=> label==='Low'? c.x>108&&c.x<310 : label==='Mid'?c.x>=310&&c.x<510:label==='High'?c.x>=510&&c.x<716:c.y<258);
        const c=label==='Crossover'?controls(state,'cursor')[0]:candidates[0];assert.ok(c,`No reachable control for ${label}`);
        {const p=await point(page,c.x+c.width/2,c.y+c.height/2);
          assert.ok(p.x>0&&p.x<view.width&&p.y>0&&p.y<view.height,JSON.stringify(p));
          if(c.kind==='cursor') { await page.mouse.move(p.x,p.y);await page.mouse.down();await page.mouse.move(p.x+20,p.y,{steps:10});await page.mouse.up();
            await until(()=>snapshot(page),s=>parameterChanges(state,s).length>0,'Crossover did not respond'); }
          else {await dragControl(page,c);await changedState(state,c);}}
        await shot(`phone-zoom-${label.toLowerCase()}`);
      });
    }
    await check(`W-06-${view.name}`,'No console errors or failed requests during use',async()=>{assert.deepEqual(errors,[]);return errors;});
    await ctx.close();
  }
  const desktop=config.viewports.find(v=>v.name==='desktop');
  const {ctx,errors}=await context(desktop);
  await start(page);
  await check('W-07',config.adapter==='passthrough'?'Template passthrough preserves the measured output':'Tremolo changes measured output against bypass',async()=>{
    await page.locator('#file-upload').setInputFiles({name:'web-test-tone.wav',mimeType:'audio/wav',buffer:toneWav()});
    await page.waitForFunction(()=>playing && audioBuffer?.duration>5);
    await page.waitForTimeout(400);
    const on=await audioSummary(page,2400);
    await page.locator('#btn-bypass').click();await page.waitForTimeout(100);
    const off=await audioSummary(page,2400);
    await page.locator('#btn-bypass').click();
    assert.ok(off.mean>.05,`Bypassed output is silent ${JSON.stringify(off)}`);
    if(config.adapter==='passthrough')assert.ok(Math.abs(on.mean-off.mean)/off.mean<.02,`Passthrough changed level ${JSON.stringify({on,off})}`);
    else {assert.ok(on.mean < off.mean*.97,`Tremolo did not reduce RMS ${JSON.stringify({on,off})}`);
      assert.ok(on.variation>off.variation*1.5,`No tremolo envelope ${JSON.stringify({on,off})}`);}
    return {on,off};
  });
  await stop();
  const initial=await snapshot(page);
  fs.writeFileSync(path.join(outDir,'controls.json'),JSON.stringify(initial,null,2));
  await check('W-08','Expected control and parameter inventory is present',async()=>{
    const expected=config.expectedInventory;
    assert.equal(controls(initial,'slider').length,expected.sliders,'Slider inventory');
    assert.equal(controls(initial,'combo').length,expected.combos,'Combo inventory');
    assert.equal(initial.parameters.length,expected.parameters,'Parameter inventory');
    if(expected.bandButtons!==undefined)assert.equal(config.bandButtonKeys.filter(key=>controls(initial,'button').some(c=>c.key===key)).length,expected.bandButtons);
    if(expected.cursors!==undefined)assert.equal(controls(initial,'cursor').length,expected.cursors);
    return expected;
  });
  await check('W-09','Unchanged settings have stable measured RMS (negative control)',async()=>{
    await choosePreset();const a=await renderAudio(page),b=await renderAudio(page);
    const drift=Math.abs(a.rms-b.rms)/Math.max(a.rms,b.rms);assert.ok(drift<.02,`RMS drifts without a gesture: ${drift}`);return {rms1:a.rms,rms2:b.rms,drift};
  });
  for(const c of controls(initial,'slider')) {
    await gestureCheck(`W-10-slider-${safe(c.key)}`,c,async current=>dragControl(page,current,current.value>=current.max ? 45 : -45));
  }
  for(const c0 of controls(initial,'cursor')) {
    await check(`W-18-${c0.name}`,'Crossover/gain handle changes its parameter and frequency display',async()=>{
      await choosePreset(9); // Three Rhythms makes the bands audibly distinct.
      const before=await snapshot(page),c=before.controls.find(x=>x.key===c0.key);
      const region={x:376,y:79,width:365,height:173};
      const imageBefore=await cropScope(region),p=await point(page,c.x+c.width/2,c.y+c.height/2);
      await page.mouse.move(p.x,p.y);await page.mouse.down();
      await page.mouse.move(p.x+(/GAIN/.test(c.name)?0:18),p.y+(/GAIN/.test(c.name)?-18:0),{steps:12});await page.mouse.up();await page.mouse.move(1,1);
      const after=await until(()=>snapshot(page),s=>parameterChanges(before,s).length>0,'Handle did not change any parameter');
      await page.waitForTimeout(250);const pixels=scopeDifference(imageBefore,await cropScope(region));assert.ok(pixels>50,'Frequency display did not change');
      return {parameters:parameterChanges(before,after),pixels};
    });
  }
  if(config.scope) {
    for(const gesture of ['pan','zoom']) await check(`W-19-${gesture}`,`Scope ${gesture} changes its view`,async()=>{
      await choosePreset();const before=await snapshot(page),c=controls(before,'scrollpad')[0];assert.ok(c,'Scope scroll/zoom control missing');
      const x=c.x+c.width*.5,y=c.y+c.height/2,p=await point(page,x,y);
      await page.mouse.move(p.x,p.y);await page.mouse.down();await page.mouse.move(p.x+(gesture==='pan'?25:0),p.y+(gesture==='zoom'?-25:0),{steps:12});await page.mouse.up();await page.mouse.move(1,1);
      const after=await until(()=>snapshot(page),s=>parameterChanges(before,s).length>0,'Scope view did not change');
      await page.waitForTimeout(200);assert.notDeepEqual((await snapshot(page)).scopes,before.scopes);return parameterChanges(before,after);
    });
  }
  for(const c0 of controls(initial,'combo')) {
    for(const item of c0.items) {
      await gestureCheck(`W-11-menu-${safe(c0.key)}-${safe(item)}`,c0,async current=>selectItem(page,current,item),
        {prepare:async()=>{const c=(await snapshot(page)).controls.find(c=>c.key===c0.key);if(c.text===item)await selectItem(page,c,c.items.find(x=>x!==item));}});
    }
  }
  // Each band toggle is checked in both directions. Sync also exposes the free-rate slider.
  const buttonState=await snapshot(page);
  for(const c0 of controls(buttonState,'button').filter(c=>config.bandButtonKeys?.includes(c.key))) {
    for(let direction=0;direction<2;direction++){
      const c=(await snapshot(page)).controls.find(c=>c.key===c0.key);
      await gestureCheck(`W-12-toggle-${safe(c.key)}-${direction}`,c,async current=>clickControl(page,current),{reset:direction===0});
      if(config.syncButtonKeys.includes(c.key)) {
        const state=await snapshot(page);
        for(const rate of controls(state,'slider').filter(x=>!controls(initial,'slider').some(y=>y.key===x.key)))
          await gestureCheck(`W-13-rate-${safe(rate.key)}-${direction}`,rate,async current=>dragControl(page,current),{reset:false});
      }
    }
  }
  if(config.adapter==='tertiary') {
    await choosePreset();
    const { testOptions } = await import('./options-tests.mjs');
    await testOptions({page,check});
    for(const [index,presetName] of initial.programs.entries()) await check(`W-24-preset-${safe(presetName)}`,`Factory preset ${presetName} loads through the menu`,async()=>{
      await choosePreset(index===0?1:0);const before=await snapshot(page);await choosePreset(index);const after=await snapshot(page);
      assert.equal(after.programName,presetName);assert.ok(parameterChanges(before,after).length>0,'Preset did not change parameters');
      assert.notDeepEqual(after.scopes,before.scopes,'Preset did not update the scope');return {parameters:parameterChanges(before,after)};
    });
    await check('W-14','Factory presets menu and arrows change state and sound',async()=>{
      await choosePreset();
      const before=await snapshot(page), a=await renderAudio(page);
      const next=controls(before,'button').find(c=>c.tooltip==='Next preset');assert.ok(next,'Next preset control missing');
      await clickControl(page,next); const after=await until(()=>snapshot(page),s=>s.program!==before.program,'Next preset did not load');
      assert.ok(parameterChanges(before,after).length>0);const b=await renderAudio(page);assert.ok(Math.abs(a.rms-b.rms)/a.rms>.05);
      const previous=controls(after,'button').find(c=>c.tooltip==='Previous preset');assert.ok(previous);
      await clickControl(page,previous);await until(()=>snapshot(page),s=>s.program===before.program,'Previous preset did not load');
      await choosePreset();
      return {from:before.programName,to:after.programName};
    });
    await check('W-15','About displays this commit as its build ID',async()=>{
      await clickAt(page,config.menu.x,config.menu.y);
      const menu=await until(()=>snapshot(page),s=>s.controls.some(c=>c.key.startsWith('popup/')),'Main menu did not open');
      const about=menu.controls.find(c=>c.key.startsWith('popup/')&&/About /.test(c.title||''));assert.ok(about,'About item missing');
      await page.waitForTimeout(230);await clickControl(page,about);await page.waitForTimeout(250);
      const aboutState=await snapshot(page),r=aboutState.aboutBounds;
      const p=await point(page,r.x+r.width*.75,r.y+52);
      await page.keyboard.down('Control');await page.keyboard.down('Shift');await page.mouse.click(p.x,p.y);
      await page.keyboard.up('Shift');await page.keyboard.up('Control');
      await page.screenshot({path:path.join(outDir,'desktop-about-build-id.png'),fullPage:true});
      const state=await snapshot(page);
      assert.equal(state.aboutBuildVisible,true,'Build line did not appear after Ctrl+Shift+click');
      assert.equal(state.compiledBuildId,meta.buildId);
      assert.equal(state.compiledBuildId.split(' \u00b7 ')[0].replace(/\+$/,''),meta.shortCommit);
      assert.match(state.compiledBuildId,/^[a-f0-9]+\+? \u00b7 \d{4}-\d{2}-\d{2} \d{2}:\d{2}$/);
    });
  }
  await check('W-16','No errors after all desktop gestures',async()=>{assert.deepEqual(errors,[]);});
  await ctx.close();
  if(config.adapter==='tertiary') {
    const { testWindowSizes } = await import('./window-tests.mjs');
    await testWindowSizes({context:()=>context(desktop),check,outDir});
  }
  await check('W-17','Non-SharedArrayBuffer fallback produces audio',async()=>{
    const {ctx,errors}=await context(desktop,true);
    try {await start(page);const a=await audioSummary(page);assert.ok(a.mean>.0001,'Fallback silent');assert.deepEqual(errors,[]);return a;}finally{await ctx.close();}
  });
  {
    const {ctx,errors}=await context(desktop);
    await check('W-33-activation-load','No activation UI at load',()=>noActivation(page));
    await start(page);await testPlayer({page,check});
    if(config.adapter==='tertiary') await check('W-34-activation-disabled','Activation fields stay disabled after clicks',()=>disabledActivation(page));
    await check('W-35-activation-timer','No activation or nag past the 50/10-second cycle',()=>waitPastNagCycle(page));
    await check('W-30','Player controls produce no console or request errors',async()=>assert.deepEqual(errors,[]));
    await ctx.close();
  }
  }
  if(engine!=='chromium')mobileWebkit=await webkit.launch({headless:true});
  await testMobile({context:profile=>context({device:profile},false,devices[profile].defaultBrowserType==='webkit'?mobileWebkit:browser),
    check,shot,outDir,meta,engine});
} catch(e) { rows.push({id:'HARNESS',title:'Runner completed',status:'fail',error:e.stack});console.error(e); }
finally {
  await mobileWebkit?.close();await browser?.close();await server?.close();
  const warnings=rows.flatMap(r=>(r.data?.warnings||[]).map(w=>`${r.id}: ${w}`));
  const summary={warnings:warnings.length,passed:rows.filter(r=>r.status==='pass').length,failed:rows.filter(r=>r.status==='fail').length};
  fs.writeFileSync(path.join(outDir,'report.json'),JSON.stringify({meta,summary,warnings,rows},null,2));
  fs.writeFileSync(path.join(outDir,'differences.json'),JSON.stringify(differences,null,2));
  const esc=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  fs.writeFileSync(path.join(outDir,'report.html'),`<!doctype html><meta charset="utf-8"><title>${esc(name)} web tests</title><style>body{font:16px system-ui;max-width:1100px;margin:40px auto;background:#14161a;color:#ddd}td,th{text-align:left;padding:9px;border-bottom:1px solid #444}.fail{color:#ff9c93}a{color:#8fc9ff}pre{white-space:pre-wrap}</style><h1>${esc(name)} web tests</h1><p>${summary.passed} passed, ${summary.failed} failed; ${summary.warnings} touch-target warnings. Commit ${esc(meta.commit)}</p><table><tr><th>ID</th><th>Check</th><th>Result</th></tr>${rows.map(r=>`<tr class="${r.status}"><td>${esc(r.id)}</td><td>${esc(r.title)}${r.error?`<pre>${esc(r.error)}</pre>`:''}${r.data?.warnings?.length?`<details><summary>${r.data.warnings.length} targets under 40 px</summary><pre>${esc(r.data.warnings.join("\n"))}</pre></details>`:""}${r.screenshot?`<a href="${esc(r.screenshot)}">Screenshot</a>`:''}</td><td>${r.status}</td></tr>`).join('')}</table>`);
  if(update) fs.writeFileSync(path.join(refsDir,'build.json'),JSON.stringify({...meta,browser:chromium.executablePath().split(/[\\/]/).slice(-4).join('/'),platform:process.platform},null,2));
  if(process.argv.includes('--no-ai')) fs.writeFileSync(path.join(outDir,'ai-review.md'),'# Advisory AI review\n\nSkipped: --no-ai (CI does not run AI review).\n');
  else await reviewDifferences(differences,{outDir});
  console.log(`${summary.passed} passed, ${summary.failed} failed. ${path.join(outDir,'report.html')}`);
  process.exitCode=summary.failed?1:0;
}
