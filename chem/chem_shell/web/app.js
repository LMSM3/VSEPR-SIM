// Poll chem state from the pipeline FastAPI host or fall back to local JSON.
const API_BASE  = window.CHEM_API_BASE || '';
const STATE_URL = API_BASE
    ? API_BASE + '/api/chem/state'
    : '../state/current_state.json';
const POLL_MS   = 1000;

async function loadState() {
  try {
    const res  = await fetch(STATE_URL + (STATE_URL.includes('?') ? '&' : '?') + 'ts=' + Date.now());
    if (!res.ok) return;
    const data = await res.json();
    if (!data.reaction) return;

    document.getElementById('rxn').textContent    = data.reaction;
    document.getElementById('cls').textContent    = data['class'] || '--';
    document.getElementById('energy').textContent = data.energy_kj != null
        ? data.energy_kj + ' kJ' : '--';
    document.getElementById('mode').textContent   = data.mode || '--';
    document.getElementById('anim').textContent   = data.web_anim || '--';
    document.getElementById('ts').textContent     =
        'last update: ' + new Date(data.timestamp * 1000).toLocaleTimeString();

    var card = document.getElementById('reaction-card');
    card.className = 'anim-' + (data.web_anim || 'none');
  } catch (_) { /* retry next cycle */ }
}

setInterval(loadState, POLL_MS);
loadState();
