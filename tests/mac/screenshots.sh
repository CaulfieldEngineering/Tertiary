#!/bin/bash
# [Codex 2026-10-30] Moved do_screens from HQ/scripts/mac-day-build-checks.sh.
# Original Mac-day screenshot/Retina logic retained; all product paths supplied by the runner.
set -u
OUT="$WLA_TEST_OUT"
row() { echo "$*"; }
do_screens() (
  local work status=0 capture_pid="" app="$WLA_TEST_APP/Contents/MacOS/$WLA_TEST_PRODUCT"
  [ -x "$app" ] || { row FAIL SCREENS "standalone missing; run the standalone phase first"; return 1; }
  work=$(mktemp -d "${TMPDIR:-/tmp}/plugin-screen-tools.XXXXXX") || return 1
  cleanup_capture() {
    trap '' HUP INT TERM
    if [ -n "$capture_pid" ] && kill -0 "$capture_pid" 2>/dev/null; then
      kill -TERM "$capture_pid" 2>/dev/null || true
      wait "$capture_pid" || true
    fi
    rm -rf "$work"
  }
  trap cleanup_capture EXIT
  trap 'exit 129' HUP
  trap 'exit 130' INT
  trap 'exit 143' TERM
  cat > "$work/screen-helper.m" <<'OBJC'
#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
@interface NSObject (VirtualDisplay)
- (id)initWithDescriptor:(id)d;
- (id)initWithWidth:(unsigned)w height:(unsigned)h refreshRate:(double)r;
- (BOOL)applySettings:(id)s;
- (unsigned)displayID;
- (void)setSizeInMillimeters:(CGSize)s;
- (void)setQueue:(dispatch_queue_t)q;
@end
static void json(id x) { NSData *d=[NSJSONSerialization dataWithJSONObject:x options:NSJSONWritingPrettyPrinted error:nil]; puts([[NSString alloc] initWithData:d encoding:NSUTF8StringEncoding].UTF8String); fflush(stdout); }
static NSDictionary *screen(CGDirectDisplayID d) { CGRect b=CGDisplayBounds(d); CGDisplayModeRef m=CGDisplayCopyDisplayMode(d); NSDictionary *r=@{@"id":@(d),@"x":@(b.origin.x),@"y":@(b.origin.y),@"width":@(CGDisplayModeGetWidth(m)),@"height":@(CGDisplayModeGetHeight(m)),@"pixelWidth":@(CGDisplayModeGetPixelWidth(m)),@"pixelHeight":@(CGDisplayModeGetPixelHeight(m))}; CGDisplayModeRelease(m); return r; }
int main(int argc,char **argv) { @autoreleasepool {
 if(argc<2) return 2;
 NSString *cmd=@(argv[1]);
 if([cmd isEqual:@"retina"]) {
  Class dc=NSClassFromString(@"CGVirtualDisplayDescriptor"),vc=NSClassFromString(@"CGVirtualDisplay"),mc=NSClassFromString(@"CGVirtualDisplayMode"),sc=NSClassFromString(@"CGVirtualDisplaySettings");
  if(!dc||!vc||!mc||!sc||argc<3) return 3;
  id desc=[dc new]; [desc setValue:@"Plugin temporary Retina test" forKey:@"name"]; [desc setValue:@3840 forKey:@"maxPixelsWide"]; [desc setValue:@2160 forKey:@"maxPixelsHigh"]; [desc setValue:@0xF0F0 forKey:@"vendorID"]; [desc setValue:@110 forKey:@"productID"]; [desc setValue:@11030 forKey:@"serialNum"]; [desc setSizeInMillimeters:CGSizeMake(300,170)]; [desc setQueue:dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0)];
  id display=[[vc alloc] initWithDescriptor:desc]; if(!display) return 4;
  id mode=[[mc alloc] initWithWidth:1920 height:1080 refreshRate:60.0]; id settings=[sc new]; [settings setValue:@1 forKey:@"hiDPI"]; [settings setValue:@[mode] forKey:@"modes"];
  if(![display applySettings:settings]) return 5;
    sleep(2);
  CGDirectDisplayID did=[display displayID];
  NSArray *modes=(NSArray *)CGDisplayCopyAllDisplayModes(did, (CFDictionaryRef)@{(id)kCGDisplayShowDuplicateLowResolutionModes:@YES});
  BOOL selected=NO;
  for(id item in modes) { CGDisplayModeRef m=(CGDisplayModeRef)item;
   if(CGDisplayModeGetWidth(m)==1920 && CGDisplayModeGetHeight(m)==1080 && CGDisplayModeGetPixelWidth(m)==3840) { selected=CGDisplaySetDisplayMode(did,m,NULL)==kCGErrorSuccess; break; }
  }
  if(!selected) return 7;
  sleep(2); json(screen(did));
  for(int i=0;i<900 && ![[NSFileManager defaultManager] fileExistsAtPath:@(argv[2])];i++) { [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:1]]; }
  [display release]; return 0;
 }
 if([cmd isEqual:@"displays"]) { uint32_t count=0; CGDirectDisplayID ds[16]; CGGetActiveDisplayList(16,ds,&count); NSMutableArray *a=[NSMutableArray new]; for(unsigned i=0;i<count;i++) [a addObject:screen(ds[i])]; json(a); return 0; }
 if(argc<3) return 2;
 pid_t pid=atoi(argv[2]);
 if([cmd isEqual:@"quit"]) { NSRunningApplication *app=[NSRunningApplication runningApplicationWithProcessIdentifier:pid]; return app && [app terminate] ? 0:1; }
 if([cmd isEqual:@"window"]) {
  NSArray *ws=(NSArray *)CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly|kCGWindowListExcludeDesktopElements,kCGNullWindowID);
  for(NSDictionary *w in ws) if([w[(id)kCGWindowOwnerPID] intValue]==pid && [w[(id)kCGWindowLayer] intValue]==0) { json(w); return 0; } return 6;
 }
 return 2;
}}
OBJC
  clang -framework Cocoa "$work/screen-helper.m" -o "$work/screen-helper" > "$OUT/screens-build.txt" 2>&1 \
    || { row FAIL SCREENS "capture helper build failed (screens-build.txt)"; return 1; }
  cat > "$work/capture.py" <<'PY'
import os, json, pathlib, signal, struct, subprocess, sys, tempfile, time, xml.etree.ElementTree as ET
out=pathlib.Path(sys.argv[1]); out.mkdir(parents=True,exist_ok=True)
helper=pathlib.Path(sys.argv[2])
app=pathlib.Path(sys.argv[3])
settings=pathlib.Path(os.environ['WLA_TEST_SETTINGS']).expanduser()
settings.parent.mkdir(parents=True,exist_ok=True)
product=os.environ['WLA_TEST_PRODUCT']
layout_w,layout_h=map(float,os.environ['WLA_TEST_LAYOUT'].split('x'))
if subprocess.run(['pgrep','-x',product],stdout=subprocess.DEVNULL).returncode==0:
 raise RuntimeError(product + ' is already running; leave that session alone')
original=settings.read_bytes() if settings.exists() else None
proc=None; virtual=None
manifest=[]
def interrupted(signum, frame): raise KeyboardInterrupt('screen capture interrupted')
signal.signal(signal.SIGTERM,interrupted)
signal.signal(signal.SIGINT,interrupted)
signal.signal(signal.SIGHUP,interrupted)
scratch=tempfile.TemporaryDirectory(prefix='plugin-screens-')
stop=pathlib.Path(scratch.name)/'stop'
def run(*args): return subprocess.check_output([str(x) for x in args],text=True)
def quit_app():
 global proc
 if proc is None: return
 if proc.poll() is None:
  subprocess.run([str(helper),'quit',str(proc.pid)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  try: proc.wait(timeout=10)
  except subprocess.TimeoutExpired:
   proc.terminate()
   try: proc.wait(timeout=5)
   except subprocess.TimeoutExpired: proc.kill(); proc.wait()
 proc=None
def capture(label,display):
 global proc
 for percent in [100,125,150,200]:
  tree=ET.fromstring(original or b'<PROPERTIES/>')
  for k,v in [('uiScale',str(percent/100)),('windowX',str(int(display['x'])+60)),('windowY',str(int(display['y'])+60))]:
   e=tree.find("./VALUE[@name='%s']"%k)
   if e is None: e=ET.SubElement(tree,'VALUE',{'name':k})
   e.set('val',v)
  settings.write_bytes(ET.tostring(tree,encoding='utf-8',xml_declaration=True))
  with (out/f'{label}-{percent}-run.txt').open('w') as log:
   proc=subprocess.Popen([str(app)],stdout=log,stderr=subprocess.STDOUT)
   time.sleep(5)
   if proc.poll() is not None: raise RuntimeError('standalone exited early')
   w=json.loads(run(helper,'window',proc.pid))
   bounds=w['kCGWindowBounds']
   expected_width=round(layout_w*percent/100)+2
   editor_height=round(layout_h*percent/100)
   # JUCE's title/border adds 28 points; its optional muted-input notice adds 30.
   if bounds['Width']!=expected_width or bounds['Height'] not in (editor_height+28,editor_height+58):
    raise RuntimeError(f'Not the requested {percent}% standalone editor: {bounds}')
   if not (display['x']<=bounds['X'] and display['y']<=bounds['Y'] and
           bounds['X']+bounds['Width']<=display['x']+display['width'] and
           bounds['Y']+bounds['Height']<=display['y']+display['height']):
    raise RuntimeError(f'Window does not fit the selected display: {bounds}, {display}')
   path=out/f'{product}-{percent}-{label}.png'
   subprocess.run(['screencapture','-x','-o','-l',str(w['kCGWindowNumber']),str(path)],check=True)
   data=path.read_bytes(); width,height=struct.unpack('>II',data[16:24])
   bounds=w['kCGWindowBounds']; scale=display['pixelWidth']/display['width']
   if width != round(bounds['Width']*scale) or height != round(bounds['Height']*scale):
    raise RuntimeError(f'Capture dimensions do not match display scale: {width}x{height}, {bounds}, {scale}')
   row={'file':path.name,'sizePercent':percent,'display':display,'windowBounds':bounds,'pngWidth':width,'pngHeight':height,'backingScale':scale}
   manifest.append(row); print(json.dumps(row),flush=True)
   quit_app()
try:
 displays=json.loads(run(helper,'displays'))
 (out/'displays-before.json').write_text(json.dumps(displays,indent=2))
 native=next((d for d in sorted(displays,key=lambda d:d['pixelWidth']!=d['width'])
              if d['width']>=1562 and d['height']>=1074),None)
 if native is None: raise RuntimeError('No display large enough for all four sizes')
 native_label='retina-2x' if native['pixelWidth']==2*native['width'] else '1x'
 if native['pixelWidth']/native['width'] not in (1,2): raise RuntimeError('Unsupported display scale')
 capture(native_label,native)
 retina=next((d for d in displays if d['pixelWidth']==2*d['width'] and d['width']>=1562 and d['height']>=1074),None)
 if retina is None:
  info=pathlib.Path(scratch.name)/'retina.json'
  with info.open('w') as f, (out/'retina-helper.txt').open('w') as log:
   virtual=subprocess.Popen([str(helper),'retina',str(stop)],stdout=f,stderr=log)
   for attempt in range(30):
    time.sleep(1)
    if virtual.poll() is not None: raise RuntimeError(f'Retina helper exited {virtual.returncode}')
    try: retina=json.loads(info.read_text()); break
    except (ValueError,OSError): pass
  if retina is None or retina['pixelWidth']!=2*retina['width']: raise RuntimeError('No true 2x Retina display')
  retina['temporaryVirtualDisplay']=True
 if native_label!='retina-2x': capture('retina-2x',retina)
finally:
 for sig in (signal.SIGHUP,signal.SIGINT,signal.SIGTERM): signal.signal(sig,signal.SIG_IGN)
 errors=[]
 def clean(label,fn):
  try: fn()
  except Exception as e: errors.append(f'{label}: {e}')
 def restore():
  if original is None: settings.unlink(missing_ok=True)
  else:
   settings.write_bytes(original)
   if settings.read_bytes()!=original: raise RuntimeError('settings read-back mismatch')
 def release_display():
  if virtual is None: return
  stop.touch()
  try: virtual.wait(timeout=10)
  except subprocess.TimeoutExpired:
   virtual.terminate()
   try: virtual.wait(timeout=5)
   except subprocess.TimeoutExpired: virtual.kill(); virtual.wait(timeout=5)
 def check_displays():
  keys=('x','y','width','height','pixelWidth','pixelHeight')
  geometry=lambda ds:sorted(tuple(d[k] for k in keys) for d in ds)
  for attempt in range(10):
   after=json.loads(run(helper,'displays'))
   if geometry(after)==geometry(displays): break
   time.sleep(1)
  (out/'displays-after.json').write_text(json.dumps(after,indent=2))
  if geometry(after)!=geometry(displays): raise RuntimeError('original display geometry was not restored')
 clean('close standalone',quit_app)
 clean('restore settings',restore)
 clean('release virtual display',release_display)
 clean('write manifest',lambda:(out/'manifest.json').write_text(json.dumps(manifest,indent=2)))
 clean('check restored displays',check_displays)
 clean('remove temporary directory',scratch.cleanup)
 if errors: raise RuntimeError('; '.join(errors))
print(f'PASS screens: {len(manifest)} PNGs; all four sizes including Retina 2x; settings restored; standalone closed')
PY
  python3 "$work/capture.py" "$OUT/screens" "$work/screen-helper" "$app" > "$OUT/screens-run.txt" 2>&1 & capture_pid=$!
  wait "$capture_pid" || status=$?
  capture_pid=""
  if [ "$status" -eq 0 ]; then
    row PASS SCREENS "$(tail -1 "$OUT/screens-run.txt"); inspect screens/*.png against the Windows reference"
  else
    row FAIL SCREENS "capture exit $status (screens-run.txt); partial captures are not a visual pass"
  fi
  return "$status"
)

do_screens
