// Sets up the Duet project "Tertiary Promo": the instruments Octavio already has (SSD5 drums, Kontakt bass and clean
// guitar, HALion pad) copied over with their saved sounds, and a Room Aux for the reverb piece.
const API = 'http://127.0.0.1:4300/api';
const FROM = 'Octavio', TO = 'Tertiary Promo';
const H = { 'content-type': 'application/json', 'x-duet-author': 'Claude' };

const src = await (await fetch(`${API}/projects/${encodeURIComponent(FROM)}`)).json();
const state = src.state || src;
const tracks = state.tracks;
const pick = (n) => structuredClone(tracks.find((t) => t.name === n));

async function copyState(ref) {
  if (!ref) return ref;
  const body = await (await fetch(`${API}/projects/${encodeURIComponent(FROM)}/plugin-states/${ref}`)).text();
  const r = await fetch(`${API}/projects/${encodeURIComponent(TO)}/plugin-states`, { method: 'POST', headers: { 'content-type': 'text/plain' }, body });
  const j = await r.json();
  return j.ref || j.state || j;
}
async function slot(s) {
  if (!s) return s;
  const out = { ...s };
  if (s.state) out.state = await copyState(s.state);
  return out;
}
async function op(o, label) {
  const r = await fetch(`${API}/projects/${encodeURIComponent(TO)}/ops`, { method: 'POST', headers: H, body: JSON.stringify({ op: o, label, author: 'Claude' }) });
  if (!r.ok) throw new Error(`${label}: ${r.status} ${await r.text()}`);
  return r.json();
}

const room = { id: 'trroom', name: 'Room', kind: 'fx', color: '#e0675f', mute: false, solo: false, volume: 1, pan: 0,
  inserts: [structuredClone(pick('Room').inserts[0])] };
// A bigger, longer room than Octavio's, for the reverb piece.
room.inserts[0].key = 'roomverb';
const want = [
  ['DRUMS', 'trdrums'], ['BASS', 'trbass'], ['GTR - CLEAN', 'trgtr'], ['PAD', 'trpad'],
];
await op({ type: 'add', coll: 'tracks', item: room }, 'Added the Room Aux');
for (const [name, id] of want) {
  const t = pick(name);
  const item = { id, name: name === 'GTR - CLEAN' ? 'GTR' : name, kind: 'midi', color: t.color, mute: false, solo: false, volume: t.volume,
    drums: !!t.drums, out: t.out, instrument: await slot(t.instrument), inserts: await Promise.all((t.inserts || []).map(slot)) };
  if (name === 'GTR - CLEAN') item.sends = [{ id: 'sdgtrroom', to: 'trroom', level: 0.5, pre: false, on: true }];
  if (name === 'PAD') item.sends = [{ id: 'sdpadroom', to: 'trroom', level: 0.18, pre: false, on: true }];
  await op({ type: 'add', coll: 'tracks', item }, `Added ${item.name} from Octavio`);
  console.log('added', item.name, item.instrument?.id?.name, item.instrument?.state);
}
