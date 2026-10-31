// Device profiles are emulation, not a claim of testing physical iPhones/Android hardware.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import {devices} from 'playwright';
import config from './config.mjs';
import {snapshot,until,point,audioSummary,parameterChanges} from './browser.mjs';
import {fitAudit,disabledSizes,tapControl} from './window-tests.mjs';
import {noActivation,disabledActivation,waitPastNagCycle} from './activation-tests.mjs';

// Playwright supplies trusted taps. It has no cross-browser swipe API, so continuous
// drags use DOM Touch events (never a mouse drag) through JUCE's normal touch handlers.
export async function touchDrag(page,control) {
  const p=await point(page,control.x+control.width/2,control.y+control.height/2);
  const horizontal=control.width>control.height*2;
  if(page.context().browser().browserType().name()==='chromium') {
    const input=await page.context().newCDPSession(page);
    try {
      const contact=d=>[{x:p.x+(horizontal?d:0),y:p.y-(horizontal?0:d),radiusX:8,radiusY:8,force:1,id:1}];
      await input.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:contact(0)});
      for(let i=1;i<=16;i++) {
        await input.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:contact(i*2)});
        await page.waitForTimeout(16);
      }
      await input.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});
      return {moves:16,kind:'browser touch injection (CDP)'};
    } finally {await input.detach();}
  }
  return page.evaluate(async ({p,horizontal})=>{
    const target=document.elementFromPoint(p.x,p.y);let moves=0;
    const emit=(type,distance)=>{
      const x=p.x+(horizontal?distance:0),y=p.y-(horizontal?0:distance);
      const touch=new Touch({identifier:1,target,clientX:x,clientY:y,pageX:x+scrollX,pageY:y+scrollY,
        screenX:x,screenY:y,radiusX:8,radiusY:8,force:type==='touchend'?0:1});
      target.dispatchEvent(new TouchEvent(type,{bubbles:true,cancelable:true,
        touches:type==='touchend'?[]:[touch],targetTouches:type==='touchend'?[]:[touch],changedTouches:[touch]}));
    };
    emit('touchstart',0);
    for(let i=1;i<=16;i++){emit('touchmove',i*2);moves++;await new Promise(r=>setTimeout(r,16));}
    emit('touchend',32);return {moves,kind:'TouchEvent'};
  },{p,horizontal:control.width>control.height*2});
}

async function targets(page) {
  const host=await page.evaluate(()=>Array.from(document.querySelectorAll('#deck button,#upload-btn,#progress')).flatMap(el=>{
    const r=el.getBoundingClientRect();
    return r.width&&r.height&&!el.disabled?[{key:el.id||el.textContent.trim(),width:r.width,height:r.height}]:[];
  }));
  const state=await snapshot(page),plugin=[];
  for(const c of state.controls.filter(c=>c.visible&&c.enabled&&c.alpha>0&&['slider','button','combo','cursor','scrollpad'].includes(c.kind))) {
    const a=await point(page,c.x,c.y),b=await point(page,c.x+c.width,c.y+c.height);
    if(b.x>0&&a.x<page.viewportSize().width&&b.y>0&&a.y<page.viewportSize().height)
      plugin.push({key:c.name||c.tooltip||c.key,width:b.x-a.x,height:b.y-a.y});
  }
  // These are explicit warnings, not hidden passes. Full-view canvas controls shrink;
  // the phone zoom views are the existing way to reach the smaller plugin controls.
  return {minimum:40,host,plugin,smallTargets:[...host,...plugin].filter(c=>c.width<39.5||c.height<39.5)};
}

export async function testMobile({context,check,shot,outDir,meta,engine}) {
  for(const device of ['iPhone 13','iPhone 15','Pixel 7']) for(const landscape of [false,true]) {
    const profile=device+(landscape?' landscape':'');
    if(engine && devices[profile].defaultBrowserType!==engine) continue;
    const id=profile.toLowerCase().replaceAll(' ','-');
    let ctx,page,errors;
    const loaded=await check(`M-01-${id}`,'Mobile profile loads without autoplay or activation UI',async()=>{
      ({ctx,errors}=await context(profile));page=ctx.pages()[0];
      const actual=await page.evaluate(()=>({ua:navigator.userAgent,touch:navigator.maxTouchPoints,
        touchEvents:'ontouchstart' in window,coarse:matchMedia('(pointer: coarse)').matches,playing,context:audioCtx?.state||'not-created'}));
      assert.equal(actual.ua,devices[profile].userAgent);assert.ok(actual.touch>0||actual.touchEvents);assert.equal(actual.coarse,true);
      assert.equal(actual.playing,false);assert.notEqual(actual.context,'running');
      await noActivation(page);assert.deepEqual(errors,[]);return actual;
    });
    if(!loaded){await ctx?.close();continue;}
    try {
      await check(`M-02-${id}`,'One TAP unlocks audio and starts audible playback',async()=>{
        await page.locator('#big-play').tap();
        await page.waitForFunction(()=>audioCtx?.state==='running'&&juceReady&&playing,null,{timeout:20000});
        const audio=await audioSummary(page,1200);assert.ok(audio.mean>.0001,`Silent mobile output: ${JSON.stringify(audio)}`);return audio;
      });
      await page.locator('#btn-stop').tap();await page.waitForTimeout(1200);
      await check(`M-03-${id}`,'Plugin fits browser chrome viewport without horizontal scroll',()=>fitAudit(page));
      await check(`M-04-${id}`,'Reviewed device screenshot',()=>shot(`${id}-baseline`));
      await check(`M-05-${id}`,'Compiled build ID equals the checkout commit and desktop generator',async()=>{
        const state=await snapshot(page);assert.equal(state.compiledBuildId,meta.buildId);
        assert.equal(state.compiledBuildId.split(' · ')[0].replace(/\+$/,''),meta.shortCommit);return state.compiledBuildId;
      });
      if(config.adapter==='tertiary') {
        await check(`M-06-${id}`,'Knob drag and menu choice respond to touch without hovering',async()=>{
          const zoom=page.getByRole('button',{name:'Low',exact:true});
          try {
          if(await zoom.isVisible()) {await zoom.tap();await page.waitForTimeout(350);await page.evaluate(()=>scrollTo(0,0));}
          const before=await snapshot(page),slider=before.controls.find(c=>c.key==='editor/0/1/2/3/0'&&c.visible)
            ||before.controls.find(c=>c.kind==='slider'&&c.visible&&c.enabled&&c.x>108&&c.x<310);
          assert.ok(slider);const gesture=await touchDrag(page,slider);
          const dragged=await until(()=>snapshot(page),s=>parameterChanges(before,s).length>0,'Touch drag did not change a parameter');
          const combo=dragged.controls.find(c=>c.kind==='combo'&&c.visible&&c.x>108&&c.x<310);assert.ok(combo);
          await tapControl(page,combo);
          const menu=await until(()=>snapshot(page),s=>s.controls.some(c=>c.visible&&c.key.startsWith('popup/')),'Touch did not open the menu');
          const item=menu.controls.find(c=>c.visible&&c.key.startsWith('popup/')&&combo.items.includes(c.title)&&c.title!==combo.text);assert.ok(item);
          await page.waitForTimeout(250);await tapControl(page,item);
          const after=await until(()=>snapshot(page),s=>s.controls.find(c=>c.key===combo.key)?.text===item.title,'Touch menu did not select the item');
          const audit=await targets(page);
          fs.writeFileSync(path.join(outDir,`${id}-zoom-touch-targets.json`),JSON.stringify(audit,null,2));
          return {gesture,parameters:parameterChanges(before,after),zoomSmallTargets:audit.smallTargets};
          } finally {
            if(await zoom.isVisible()){await page.getByRole('button',{name:'All',exact:true}).tap();await page.waitForTimeout(350);}
            await page.evaluate(()=>scrollTo(0,0));
          }
        });
        await check(`M-07-${id}`,'Size fields are greyed out and do nothing when tapped',()=>disabledSizes(page,true));
        await check(`M-08-${id}`,'About retains disabled activation fields and an explanation',()=>disabledActivation(page,true));
      }
      await check(`M-09-${id}`,'Audit touch targets and flag everything smaller than 40 CSS px',async()=>{
        const audit=await targets(page);
        fs.writeFileSync(path.join(outDir,`${id}-touch-targets.json`),JSON.stringify(audit,null,2));
        return {...audit,warnings:audit.smallTargets.map(c=>`${c.key}: ${c.width.toFixed(1)} x ${c.height.toFixed(1)} CSS px (under 40)`)};
      });
      await check(`M-10-${id}`,'Orientation change and browser chrome resize re-fit the canvas',async()=>{
        const rotated=device+(!landscape?' landscape':''),views=[];
        await page.setViewportSize(devices[rotated].viewport);views.push(await fitAudit(page));
        await page.setViewportSize({...devices[rotated].viewport,height:devices[rotated].viewport.height-60});views.push(await fitAudit(page));
        await page.setViewportSize(devices[profile].viewport);views.push(await fitAudit(page));
        if(devices[profile].defaultBrowserType==='chromium') {
          const input=await page.context().newCDPSession(page);
          const before=await page.evaluate(()=>({layoutHeight:innerHeight,visualHeight:visualViewport.height}));
          try {
            // A real visual-viewport-only resize, without any window resize.
            await input.send('Emulation.setPageScaleFactor',{pageScaleFactor:1.25});
            await page.waitForFunction(h=>visualViewport.height<h,before.visualHeight);
            const after=await page.evaluate(()=>({layoutHeight:innerHeight,visualHeight:visualViewport.height}));
            assert.equal(after.layoutHeight,before.layoutHeight);assert.ok(after.visualHeight<before.visualHeight);
            views.push({visualOnly:await fitAudit(page),before,after});
          } finally {
            await input.send('Emulation.setPageScaleFactor',{pageScaleFactor:1});await input.detach();
          }
        }
        return views;
      });
      await check(`M-11-${id}`,'No activation after gestures or across a complete nag cycle',async()=>{
        await noActivation(page);
        const waited=await waitPastNagCycle(page);
        return waited;
      });
      await check(`M-12-${id}`,'No mobile console errors or failed requests',async()=>assert.deepEqual(errors,[]));
    } finally {await ctx.close();}
  }
}
