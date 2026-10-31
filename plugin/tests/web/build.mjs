// [Codex 2026-10-30] CI's native BinaryData -> Wonderlab/Emscripten -> assemble-web pipeline.
// No source edits, deployments or release actions. The optional inspector only reads JUCE state.
import fs from 'node:fs';
import path from 'node:path';
import { spawnSync, execFileSync } from 'node:child_process';
import config from './config.mjs';
import { here, root, name, version, webDir } from './paths.mjs';
const win = process.platform === 'win32';
const unix = p => path.resolve(p).replaceAll('\\', '/');
function run(exe, args, cwd = root) {
  console.log(`> ${exe} ${args.join(' ')}`);
  if (win && exe.endsWith('.bat')) {
    args = [exe.replace(/\.bat$/, '.py'), ...args];
    exe = process.env.EMSDK_PYTHON || 'python';
  }
  const r = spawnSync(exe, args, { cwd, stdio: 'inherit', shell: false });
  if (r.error) throw r.error;
  if (r.status !== 0) throw new Error(`${exe} exited ${r.status}`);
}
const wonderlab = path.resolve(process.env.WONDERLAB_DIR || path.join(root, 'build/Wonderlab'));
if (!fs.existsSync(path.join(wonderlab, 'cmake/WonderlabBuild.cmake')))
  throw new Error('Set WONDERLAB_DIR to a Wonderlab checkout (see README.md).');
// Template projects also compile WizardCore into the web targets, as their CI pipeline does.
const wizardcore = process.env.WIZARDCORE_DIR ? path.resolve(process.env.WIZARDCORE_DIR) : undefined;
if (wizardcore && !fs.existsSync(path.join(wizardcore, 'plugin/source')))
  throw new Error('WIZARDCORE_DIR must contain a WizardCore checkout with plugin/source.');
const sdk = process.env.EMSDK;
const emcmake = sdk ? path.join(sdk, 'upstream/emscripten', win ? 'emcmake.bat' : 'emcmake') : win ? 'emcmake.bat' : 'emcmake';
const emcc = sdk ? path.join(sdk, 'upstream/emscripten', win ? 'emcc.bat' : 'emcc') : win ? 'emcc.bat' : 'emcc';
const v = spawnSync(win ? (process.env.EMSDK_PYTHON || 'python') : emcc,
  win ? [emcc.replace(/\.bat$/, '.py'), '--version'] : ['--version'], { encoding: 'utf8', shell: false }).stdout || '';
if (!v.includes(` ${config.emsdkVersion} `)) throw new Error(`Activate Emscripten ${config.emsdkVersion} first; got ${v.trim()}`);
const nativeDir = path.join(root, 'build/native');
const nativeArgs = ['-S', root, '-B', nativeDir, '-DBUILD_DEMO=OFF'];
const nativeSources = {
  JUCE: process.env.CPM_JUCE_SOURCE,
  WizardCore: wizardcore || process.env.CPM_WizardCore_SOURCE
};
for (const [dependency, source] of Object.entries(nativeSources))
  if (source) nativeArgs.push(`-DCPM_${dependency}_SOURCE=${unix(source)}`);
run('cmake', nativeArgs);
run('cmake', ['--build', nativeDir, '--config', 'Release', '--target', `${name}BinaryData`]);
function find(dir, file) {
  for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
    const p = path.join(dir, e.name);
    if (e.isFile() && e.name === file) return p;
    if (e.isDirectory()) { const hit = find(p, file); if (hit) return hit; }
  }
}
const binary = find(nativeDir, 'BinaryData.h');
if (!binary) throw new Error('Native build did not produce BinaryData.h');
const extraDirs = [path.dirname(binary)];
if (wizardcore) extraDirs.push(path.join(wizardcore, 'plugin/source'));
const cmakeExtraDirs = extraDirs.map(unix).join(';');
const inspect = !process.argv.includes('--no-inspector');
fs.mkdirSync(webDir, { recursive: true });
fs.writeFileSync(path.join(webDir, 'CMakeLists.txt'), `cmake_minimum_required(VERSION 3.15)
project(${name}Web LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(PLUGIN_NAME "${name}")
set(PLUGIN_VERSION "${version}")
set(PLUGIN_SRC_DIR "${unix(path.join(root, 'plugin/source'))}")
set(WONDERLAB_DIR "${unix(wonderlab)}")
set(PLUGIN_PROCESSOR_HEADER "${config.processorHeader}")
set(PLUGIN_PROCESSOR_CLASS ${config.processorClass})
set(PLUGIN_EXTRA_INCLUDE_DIRS "${cmakeExtraDirs}")
set(PLUGIN_EXTRA_SOURCE_DIRS "${cmakeExtraDirs}")
include("${unix(path.join(root, "plugin/web/wonderlab.cmake"))}")
${inspect ? `target_sources(${name}Web PRIVATE "${unix(path.join(here, 'inspect.cpp'))}")\ntarget_sources(${name}WebFallback PRIVATE "${unix(path.join(here, 'inspect.cpp'))}")` : ''}
`);
run(emcmake, ['cmake', '-G', 'Ninja', '-S', webDir, '-B', webDir, '-DCMAKE_BUILD_TYPE=Release']);
run('cmake', ['--build', webDir, '--parallel', process.env.WEB_BUILD_JOBS || '4']);
const commit = execFileSync('git', ['rev-parse', 'HEAD'], { cwd: root, encoding: 'utf8' }).trim();
const shortCommit = execFileSync('git', ['rev-parse', '--short', 'HEAD'], {cwd:root,encoding:'utf8'}).trim();
const buildId = fs.readFileSync(path.join(webDir,'generated/build_id.h'),'utf8').match(/WLA_BUILD_ID "([^"]+)"/)[1];
const wlCommit = execFileSync('git', ['rev-parse', 'HEAD'], { cwd: wonderlab, encoding: 'utf8' }).trim();
const assembleArgs = [path.join(root, 'plugin/web/assemble.mjs'), '--wonderlab', wonderlab, '--name', name, '--build', webDir,
  '--samples', path.join(root, 'plugin/sample_audio'), '--build-id', commit.slice(0, 8)];
const extra = path.join(root, 'plugin/web_demo.json');
if (fs.existsSync(extra)) assembleArgs.push('--extra', extra);
run(process.execPath, assembleArgs);
fs.writeFileSync(path.join(webDir, 'test-build.json'), JSON.stringify({ name, version, commit, shortCommit, buildId, wonderlab: wlCommit,
  emsdk: config.emsdkVersion, inspector: inspect }, null, 2));
