import assert from 'node:assert/strict';
import config from './config.mjs';
import {snapshot,until,point,clickControl,toneWav,audioSummary} from './browser.mjs';
import {tapControl} from './window-tests.mjs';

export async function noActivation(page) {
  const state=await snapshot(page);
  if(config.adapter==='tertiary') {
    assert.equal(state.aboutVisible,false,'About/activation opened without being requested');
    assert.equal(state.demo,false,'Browser processor entered the licensed demo bypass cycle');
    assert.equal(state.activationBadge,false,'Activation badge/nag is visible');
    assert.equal(state.licenseThreadRunning,false,'Browser started a licence check');
  }
  assert.equal(state.controls.some(c=>c.visible&&c.enabled&&/^(Activate|Deactivate)/.test(c.text||c.title||'')),false);
  return {aboutVisible:state.aboutVisible,demo:state.demo,activationBadge:state.activationBadge,licenseThreadRunning:state.licenseThreadRunning};
}

export async function disabledActivation(page,touch=false) {
  const activate=touch?tapControl:clickControl;
  const p=await point(page,config.menu.x,config.menu.y);
  if(touch)await page.touchscreen.tap(p.x,p.y);else await page.mouse.click(p.x,p.y);
  const menu=await until(()=>snapshot(page),s=>s.controls.some(c=>c.visible&&/^About /.test(c.title||'')),'About menu missing');
  await page.waitForTimeout(250);await activate(page,menu.controls.find(c=>c.visible&&/^About /.test(c.title||'')));
  const state=await until(()=>snapshot(page),s=>s.aboutVisible,'About panel did not open');
  assert.ok(state.controls.some(c=>c.visible&&c.text==='Activation is not needed in the browser demo'));
  const fields=state.controls.filter(c=>c.visible&&c.kind==='text');assert.equal(fields.length,2);
  const button=state.controls.find(c=>c.visible&&c.text==='Activate'&&c.kind==='button');assert.ok(button);
  for(const c of [...fields,button]) {
    assert.equal(c.enabled,false,`${c.key} is not disabled`);
    assert.ok(c.alpha<=.51,`${c.key} is not greyed out`);
    await activate(page,c);
  }
  await page.keyboard.type('should-not-activate');
  const after=await snapshot(page);
  assert.deepEqual(after.controls.filter(c=>fields.some(f=>f.key===c.key)).map(c=>c.text),fields.map(c=>c.text));
  assert.equal(after.licenseThreadRunning,false);
  const close=after.controls.find(c=>c.visible&&c.kind==='button'&&c.text==='X');assert.ok(close);
  await activate(page,close);
  await until(()=>snapshot(page),s=>!s.aboutVisible,'About panel did not close');
  return noActivation(page);
}

// The native bypass starts at 50 seconds and lasts 10. Observe 65 real seconds,
// including transient UI, without faking timers or processor state.
export async function waitPastNagCycle(page) {
  // Known looping input lets the audio check detect a mid-cycle interruption,
  // not just audio which happens to have resumed when the wait finishes.
  if(await page.locator('#btn-loop').getAttribute('aria-pressed')!=='true')await page.locator('#btn-loop').click();
  await page.locator('#file-upload').setInputFiles({name:'activation-wait-tone.wav',mimeType:'audio/wav',buffer:toneWav(2)});
  await page.waitForFunction(()=>playing&&audioBuffer?.duration===2&&audioCtx?.state==='running');
  const start=Date.now(),audio=[];let samples=0;
  while(Date.now()-start<65000) {
    await noActivation(page);samples++;
    const measured=await audioSummary(page,900);
    assert.ok(measured.mean>.0001,`Audio interrupted during nag interval at ${Date.now()-start}ms: ${JSON.stringify(measured)}`);
    audio.push(measured.mean);
    await page.waitForTimeout(100);
  }
  await noActivation(page);
  return {elapsedMs:Date.now()-start,samples,audioRms:audio};
}
