// [Codex 2026-10-30] Small, checked overrides of the pinned Wonderlab shell.
// Fail assembly if upstream changes an anchor, rather than silently dropping a fix.
export function patchShell(input) {
  let html = input.replaceAll('\r\n', '\n');
  const replace = (before, after) => {
    if (html.split(before).length !== 2) throw new Error(`Wonderlab shell anchor changed: ${before.slice(0, 90)}`);
    html = html.replace(before, after);
  };
  replace('    const avail = stage.clientWidth || window.innerWidth;', `    const avail = Math.min(stage.clientWidth || window.innerWidth, window.visualViewport?.width || window.innerWidth);
    // visualViewport excludes mobile browser chrome and changes as it opens/closes.
    const visibleHeight = window.visualViewport?.height || window.innerHeight;
    const availableHeight = Math.max(1, visibleHeight - 16);
    const heightScale = availableHeight / pluginH;`);
  replace('scale  = Math.min(1, avail / pluginW);', 'scale  = Math.min(1, avail / pluginW, heightScale);');
  replace('scale  = Math.min(ZOOM_MAX, avail / region.w);', 'scale  = Math.min(ZOOM_MAX, avail / region.w, availableHeight / region.h);');
  replace("window.addEventListener('resize', () => { updateScale(); updateTrackNameMarquee(); updateNotices(); });", `function refitViewport() {
    if (typeof Module !== 'undefined') Module._wonderlab_viewportChanged?.();
    updateScale(); updateTrackNameMarquee(); updateNotices();
}
window.addEventListener('resize', refitViewport);
window.visualViewport?.addEventListener('resize', refitViewport);
window.addEventListener('orientationchange', () => requestAnimationFrame(refitViewport));`);
  replace('if (wasCurrent) { fullStop(); audioBuffer = null; }', `if (wasCurrent) {
                    ++_loadSeq; // A decode already in flight must not restore the removed track.
                    fullStop();
                    _stopPlaytimeTimer();
                    audioBuffer = null;
                    currentTrackIdx = -1;
                    trackName.textContent = '';
                    document.getElementById('file-upload').value = '';
                    progressBar.value = 0;
                    progressBar.style.setProperty('--pct', '0%');
                    timeDisplay.textContent = '0:00 / 0:00';
                    timeSmall.textContent = '0:00 / 0:00';
                    btnPlay.disabled = btnStop.disabled = progressBar.disabled = true;
                    setStatus('Choose a track');
                }`);
  replace('    _decoded.set(track, buf);', '    if (TRACKS.includes(track)) _decoded.set(track, buf);');
  replace('        TRACKS[idx].duration = audioBuffer.duration;', '        track.duration = audioBuffer.duration;');
  replace('                const origLeft = parseInt(c.style.left) || 0;', `                // Native menu placement knows the full editor, not the phone zoom crop.
                // Constrain the JUCE peer itself so drawing and touch coordinates agree.
                const viewport = window.visualViewport;
                const left = viewport?.offsetLeft || 0, top = viewport?.offsetTop || 0;
                Module._wonderlab_fitPopup?.(Number(c.dataset.peer),
                    Math.ceil((left - rect.left) / scale), Math.ceil((top - rect.top) / scale),
                    Math.floor((viewport?.width || innerWidth) / scale),
                    Math.floor((viewport?.height || innerHeight) / scale));
                const origLeft = parseInt(c.style.left) || 0;`);
  replace('    </style>', `        /* Touch targets in the host stay usable in both orientations. */
        @media (pointer: coarse) {
            .zoom-btn, #upload-btn, .playlist-track__delete { min-width: 40px; min-height: 40px; }
        }
    </style>`);
  return html;
}
