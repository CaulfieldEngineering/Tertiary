#!/usr/bin/env python3
"""[Claude 2026-10-31] Pinned tools in a repo-local folder, shared by scripts/ (builds) and tests/ (checks).

Joe, 2026-10-31: "Anything like pluginval gets installed to a local folder during the script instead of
hardcoding its location on the mini PC." Each tool in tools.json is looked up in this order:

  1. its override env var (WLA_PLUGINVAL, WLA_ISCC, WLA_NINJA, EMSDK, WONDERLAB_DIR, WLA_REAPER), if it points at something
  2. <repo>/.tools/<tool>-<version>/ (gitignored; WLA_TOOLS_DIR moves it, e.g. to share one emsdk between checkouts)
  3. if the tool is "auto", downloaded there now: the pinned file, its SHA-256 checked before it is unpacked
     (git tools: the pinned commit). REAPER is opt-in only, see tools.json.

  python scripts/lib/tools.py path pluginval        prints the tool's path, fetching it if needed (exit 1 if impossible)
  python scripts/lib/tools.py path reaper --no-install
  python scripts/lib/tools.py install reaper        opt-in tools
  python scripts/lib/tools.py list

Nothing here needs admin rights, changes system settings or installs outside the tools folder.
"""
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tempfile
import urllib.request
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
SPEC = json.loads((HERE / 'tools.json').read_text(encoding='utf-8'))
PLATFORM = {'Windows': 'win', 'Darwin': 'mac'}.get(platform.system(), 'linux')


class ToolError(RuntimeError):
    pass


def tools_dir():
    return Path(os.environ.get('WLA_TOOLS_DIR') or ROOT / '.tools').expanduser().resolve()


def say(text):
    print('[tools] ' + text, file=sys.stderr, flush=True)


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def download(url, expected):
    cache = tools_dir() / 'downloads'
    cache.mkdir(parents=True, exist_ok=True)
    target = cache / url.rsplit('/', 1)[-1]
    if target.exists() and sha256(target) == expected:
        return target
    say('downloading ' + url)
    part = target.with_suffix(target.suffix + '.part')
    request = urllib.request.Request(url, headers={'User-Agent': 'wla-tools'})
    with urllib.request.urlopen(request, timeout=300) as r, open(part, 'wb') as f:
        shutil.copyfileobj(r, f)
    got = sha256(part)
    if got != expected:
        part.unlink()
        raise ToolError(f'checksum mismatch for {url}: got {got}, expected {expected}')
    part.replace(target)
    return target


def git(*args, cwd=None):
    done = subprocess.run(['git', *args], cwd=cwd, capture_output=True, text=True)
    if done.returncode:
        raise ToolError('git ' + ' '.join(args) + ': ' + (done.stderr.strip() or done.stdout.strip()))
    return done.stdout.strip()


def install_git(name, spec, folder):
    url, ref = spec['git']['url'], spec['git']['ref']
    if not (folder / '.git').exists():
        say(f'cloning {url} at {ref[:12]} into {folder}')
        tmp = folder.with_name(folder.name + '.part')
        shutil.rmtree(tmp, ignore_errors=True)
        try:
            git('clone', '--quiet', '--no-checkout', url, str(tmp))
        except ToolError as error:
            shutil.rmtree(tmp, ignore_errors=True)
            raise ToolError(f'{name}: could not clone {url} ({error}). Set {spec["override"]} to an existing checkout.')
        git('checkout', '--quiet', ref, cwd=tmp)
        tmp.replace(folder)
    if git('rev-parse', 'HEAD', cwd=folder) != ref:
        raise ToolError(f'{folder} is not at the pinned commit {ref}; delete it to re-fetch, or set {spec["override"]}')
    if name == 'emsdk':
        marker = folder / ('.wla-installed-' + spec['version'])
        if not marker.exists():
            exe = [str(folder / 'emsdk.bat')] if PLATFORM == 'win' else [str(folder / 'emsdk')]
            for step in ('install', 'activate'):
                say(f'emsdk {step} {spec["version"]} (first time only; large download)')
                if subprocess.run(exe + [step, spec['version']], cwd=folder).returncode:
                    raise ToolError(f'emsdk {step} {spec["version"]} failed')
            marker.write_text(spec['version'] + '\n')
    return folder


def install_file(name, spec, folder):
    plat = spec.get(PLATFORM)
    if not plat:
        raise ToolError(f'{name} has no pinned download for {PLATFORM}')
    archive = download(plat['url'], plat['sha256'])
    tmp = folder.with_name(folder.name + '.part')
    shutil.rmtree(tmp, ignore_errors=True)
    tmp.mkdir(parents=True)
    kind = plat['kind']
    if kind == 'zip':
        if PLATFORM == 'mac':   # ditto keeps the app bundle's symlinks and permissions
            subprocess.run(['ditto', '-x', '-k', str(archive), str(tmp)], check=True)
        else:
            with zipfile.ZipFile(archive) as z:
                z.extractall(tmp)
    elif kind == 'inno':        # Inno Setup's own installer, per user, into this folder only (no admin, no icons)
        subprocess.run([str(archive), '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/CURRENTUSER',
                        '/PORTABLE=1', '/NOICONS', '/DIR=' + str(tmp)], check=True)
    elif kind == 'nsis-portable':   # REAPER's documented portable install
        subprocess.run([str(archive), '/S', '/PORTABLE', '/D=' + str(tmp)], check=True)
    elif kind == 'dmg-app':
        mount = Path(tempfile.mkdtemp(prefix='wla-dmg-'))
        subprocess.run(['hdiutil', 'attach', '-nobrowse', '-readonly', '-mountpoint', str(mount), str(archive)],
                       check=True, capture_output=True)
        try:
            app = next(mount.glob('*.app'))
            subprocess.run(['ditto', str(app), str(tmp / app.name)], check=True)
        finally:
            subprocess.run(['hdiutil', 'detach', str(mount)], capture_output=True)
    else:
        raise ToolError('unknown kind ' + kind)
    exe = tmp / plat['exe']
    if not exe.exists():
        raise ToolError(f'{name}: {plat["exe"]} not found after unpacking {archive.name}')
    if PLATFORM != 'win':
        exe.chmod(exe.stat().st_mode | 0o755)
    shutil.rmtree(folder, ignore_errors=True)
    tmp.replace(folder)
    return folder


def path(name, install=True, explicit=False):
    """The tool's executable (or folder, for git tools). Raises ToolError when it cannot be had."""
    if name not in SPEC or name.startswith('_'):
        raise ToolError('unknown tool ' + name)
    spec = SPEC[name]
    override = os.environ.get(spec['override'])
    if override and Path(override).expanduser().exists():
        return Path(override).expanduser()
    folder = tools_dir() / f'{name}-{spec["version"]}'
    exe = (lambda f: f) if 'git' in spec else (lambda f: f / spec[PLATFORM]['exe'] if PLATFORM in spec else None)
    if folder.exists() and (('git' in spec and (folder / '.git').exists()) or ('git' not in spec and exe(folder) and exe(folder).exists())):
        if 'git' in spec:
            return install_git(name, spec, folder)   # verifies the commit (and finishes an interrupted emsdk install)
        return exe(folder)
    if not install or not (spec.get('auto') or explicit):
        raise ToolError(f'{name} is not in {tools_dir()}' + ('' if spec.get('auto') else
                        f' (opt-in: python scripts/lib/tools.py install {name})') + f'; or set {spec["override"]}')
    folder.parent.mkdir(parents=True, exist_ok=True)
    return install_git(name, spec, folder) if 'git' in spec else exe(install_file(name, spec, folder))


def find(name, install=True):
    """path(), or None: for callers that report a missing tool themselves (the tests' SKIP)."""
    try:
        return path(name, install=install)
    except Exception as error:
        if install:   # a plain look-up (install=False) is allowed to come back empty quietly
            say(str(error))
        return None


def main(argv):
    if len(argv) < 1 or argv[0] not in ('path', 'install', 'list'):
        print(__doc__)
        return 2
    if argv[0] == 'list':
        for name, spec in SPEC.items():
            if name.startswith('_'):
                continue
            where = find(name, install=False)
            print(f'{name:10} {spec["version"]:10} {"auto" if spec.get("auto") else "opt-in":7} {where or "-"}')
        return 0
    try:
        print(path(argv[1], install='--no-install' not in argv, explicit=argv[0] == 'install'))
        return 0
    except (ToolError, subprocess.CalledProcessError, OSError) as error:
        say('ERROR: ' + str(error))
        return 1


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
