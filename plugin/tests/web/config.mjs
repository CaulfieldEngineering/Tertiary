// [Codex 2026-10-30] Product data only; the runner, inspector and build are shared with the template.
export default {
  approvedCommit: '7760137',
  wonderlabCommit: '5248a15',
  emsdkVersion: '5.0.3',
  processorHeader: 'PluginProcessor.h',
  processorClass: 'audio_plugin::AudioPluginAudioProcessor',
  adapter: 'tertiary',
  viewports: [
    { name: 'phone', width: 390, height: 844, touch: true },
    { name: 'tablet', width: 820, height: 1180, touch: true },
    { name: 'desktop', width: 1440, height: 900, touch: false }
  ],
  pixels: { threshold: 0.12, maxDiffRatio: 0.001 },
  // Analytics is outside this local test's scope. Every local request is checked, without exclusions.
  analyticsHosts: ['www.googletagmanager.com', 'www.google-analytics.com', 'region1.google-analytics.com'],
  scope: { x: 1, y: 76, width: 369, height: 177 },
  preset: { x: 330, y: 26 },
  menu: { x: 727, y: 26 },
  aboutBuild: { x: 570, y: 104 },
  bands: ['Low', 'Mid', 'High'],
  expectedInventory: { sliders: 14, combos: 6, parameters: 48, cursors: 5, bandButtons: 15 },
  gainSliderKeys: ['editor/0/1/0/0', 'editor/0/1/1/0', ...[2,3,4].map(b=>`editor/0/1/${b}/8/0`)],
  bandButtonKeys: [2,3,4].flatMap(b=>[0,1,9,10,11].map(c=>`editor/0/1/${b}/${c}/0`)),
  syncButtonKeys: [2,3,4].map(b=>`editor/0/1/${b}/0/0`)
};
