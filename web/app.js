/* Tournament Manager UI (football cups: clubs, squads, bracket, fan predictions with virtual points). All tournament logic lives in the C engine; this file only renders
 * the JSON state returned by /api/* and sends user actions back. Owner: Member C */
'use strict';
const $ = (s, r = document) => r.querySelector(s);
let S = { exists: false }, tab = 'bracket', rankMode = 'standings', histTeam = 0, prevPhase = null, traceMin = false, selFan = 0;

/* ---------- helpers ---------- */
const esc = s => String(s ?? '').replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
const byId = id => (S.teams || []).find(t => t.id === id);
const matchById = id => (S.matches || []).find(m => m.id === id);
const rname = r => (S.roundNames || [])[r - 1] || ('Round ' + r);
const expect = (ra, rb) => 1 / (1 + Math.pow(10, (rb - ra) / 400));

function toast(msg, kind = 'ok') {
  const t = $('#toast'); t.textContent = msg; t.className = 'toast ' + kind; t.hidden = false;
  clearTimeout(toast.h); toast.h = setTimeout(() => t.hidden = true, 2600);
}
async function call(path, body) {
  try {
    const r = await fetch('/api/' + path, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body || {}) });
    const d = await r.json();
    if (!d.ok) { toast(d.error || 'Error', 'err'); return false; }
    setState(d); return true;
  } catch (e) { toast('Cannot reach the server', 'err'); return false; }
}
function setState(d) {
  const was = prevPhase; S = d; prevPhase = d.phase;
  render();
  if (was === 'running' && d.phase === 'finished') confetti();
}
async function load() { try { setState(await (await fetch('/api/state')).json()); } catch (e) { toast('Cannot reach the server', 'err'); } }

/* ---------- top bar ---------- */
function renderTop() {
  const running = S.phase === 'running';
  $('#top').innerHTML = `
    <div class="brand"><div class="logo"><svg width="18" height="18" viewBox="0 0 32 32"><path d="M5 8h8v5h6V8h8M13 13v11M19 13v11M5 24h8M19 24h8" stroke="#fff" stroke-width="3" fill="none" stroke-linecap="round"/></svg></div>Tournament Manager</div>
    ${S.exists ? `<div class="crumb">/ <b>${esc(S.name)}</b><span class="pill ${S.phase}">${S.phase}</span></div>` : ''}
    <div class="spacer"></div>
    ${S.exists ? `<div class="actions">
      ${running ? `<button class="btn primary" data-act="sim">▶ Play next</button>
      <button class="btn" data-act="simround">Sim round</button>
      <button class="btn" data-act="simall">Sim all</button>` : ''}
      ${S.phase !== 'setup' ? `<button class="btn" data-act="undo" ${S.undoDepth ? '' : 'disabled'}>↶ Undo${S.undoDepth ? ' (' + S.undoDepth + ')' : ''}</button>` : ''}
      ${S.phase === 'finished' ? `<a class="btn primary" href="/api/report" download="tournament-report.html">⬇ Download report</a>` : ''}
      <a class="btn" href="/api/export" download="tournament.tm">Export</a>
      <button class="btn danger" data-act="reset">New</button></div>` : ''}`;
}

/* ---------- welcome / create ---------- */
function renderWelcome() {
  $('#main').innerHTML = `
    <section class="hero"><h1>Run your football cup.<br>From first whistle to final.</h1>
      <p>Register clubs and squads, build the cup bracket, record results, follow the tables, and let fans predict matches with virtual points.</p>
      <div class="chips"><span class="chip">Clubs &amp; squads</span><span class="chip">Cup bracket</span><span class="chip">Live tables</span><span class="chip">Match history</span><span class="chip">Undo a result</span><span class="chip">Fan predictions</span></div></section>
    <div class="grid g2" style="max-width:480px;margin:0 auto;grid-template-columns:1fr">
      <div class="card"><h3>Create your own</h3>
        <div class="field"><label>Cup name</label><input class="input" id="cName" placeholder="City Champions Cup" maxlength="40"></div>
        <p class="hint">Then add your clubs and their players, and generate the bracket.</p>
        <button class="btn" style="width:100%" id="createGo">Create &amp; add clubs →</button></div>
    </div>`;
  $('#createGo').onclick = () => call('create', { name: $('#cName').value || 'Untitled Cup' });
}

/* ---------- setup (team registration) ---------- */
function renderSetup() {
  const n = S.teams.length; let P = 1; while (P < n) P *= 2;
  const okN = n >= 2 && n <= 32 && (n & (n - 1)) === 0, lo = P > n ? P / 2 : n;
  $('#main').innerHTML = `<div class="grid g2" style="grid-template-columns:1.2fr .8fr">
    <div class="card"><h3>Clubs</h3>
      <form class="row" id="addForm" style="gap:8px"><input class="input" id="tName" placeholder="Club name" maxlength="40" autocomplete="off" style="flex:2">
        <select class="input" id="tRate" style="flex:1">${[[1900,'Elite (1900)'],[1700,'Strong (1700)'],[1500,'Average (1500)'],[1300,'Modest (1300)'],[1100,'Weak (1100)']].map(([v,l])=>`<option value="${v}" ${v===1500?'selected':''}>${l}</option>`).join('')}</select><button class="btn primary" style="flex:none">Add</button></form>
      <div class="row nofill" style="margin-top:10px"><button class="btn sm" data-fill="4">+4 sample clubs</button><button class="btn sm" data-fill="8">+8</button><button class="btn sm" data-fill="16">+16</button><button class="btn sm" data-squads="1">Sample squads for all</button></div>
      <div class="teamlist">${n ? S.teams.map((t, i) => `<div class="trow"><span class="idx">${i + 1}</span><span class="nm link" data-team="${t.id}">${esc(t.name)}</span><span class="rt">${t.squadSize} players</span><span class="rt">Str ${Math.round(t.rating)}</span><button class="btn sm" data-team="${t.id}">Squad</button><button class="x" data-rm="${t.id}" title="Remove">×</button></div>`).join('') : '<div class="empty-note">No clubs yet. Add some, or use a sample pack.</div>'}</div></div>
    <div class="card"><h3>Bracket preview</h3>
      <div class="stat-big">${n}<span class="hint" style="font-size:14px"> clubs · ${S.players} players</span></div>
      <p class="hint">${n < 2 ? 'Register at least 2 clubs. A cup needs <b>2, 4, 8, 16 or 32</b> clubs.' : n > 32 ? `Too many clubs: the maximum is <b>32</b>. Remove <b>${n - 32}</b>.` : okN ? `Ready: a <b>${n}</b>-club cup with <b>${Math.log2(n)}</b> round${n > 2 ? 's' : ''}.` : `A cup needs <b>2, 4, 8, 16 or 32</b> clubs. Add <b>${P - n}</b> more (to ${P}) or remove <b>${n - lo}</b> (to ${lo}).`}</p>
      <p class="hint">Clubs are seeded by strength so the best sides meet as late as possible. Click a club to manage its squad.</p>
      <button class="btn primary" style="width:100%;margin-top:14px" id="genGo" ${okN ? '' : 'disabled'}>Generate bracket</button></div></div>`;
  $('#addForm').onsubmit = async e => { e.preventDefault(); const nm = $('#tName').value.trim(); if (!nm) return; if (await call('team', { name: nm, rating: +$('#tRate').value || 1500 })) $('#tName').focus(); };
  document.querySelectorAll('[data-fill]').forEach(b => b.onclick = () => call('fill', { count: +b.dataset.fill }));
  document.querySelectorAll('[data-squads]').forEach(b => b.onclick = () => call('squad/fill', {}));
  document.querySelectorAll('[data-rm]').forEach(b => b.onclick = () => call('team/remove', { id: +b.dataset.rm }));
  $('#genGo').onclick = () => call('generate').then(ok => ok && toast('Bracket generated'));
  $('#tName').focus();
}

/* ---------- running view ---------- */
function stats() {
  const played = S.history.length, total = S.matches.filter(m => m.status !== 'bye').length;
  const next = S.upcoming.length ? matchById(S.upcoming[0]) : null;
  const lead = S.power[0];
  return `<div class="strip">
    <div class="stat"><div class="k">Matches played</div><div class="v">${played} / ${total}</div><div class="progress"><i style="width:${total ? played / total * 100 : 0}%"></i></div></div>
    <div class="stat"><div class="k">Clubs</div><div class="v">${S.teams.length}</div><div class="s">${S.players} players · ${S.ds.tree.byes} byes</div></div>
    <div class="stat"><div class="k">Next match</div><div class="v">${next ? '#' + next.id : '—'}</div><div class="s">${next ? esc(next.a.name) + ' vs ' + esc(next.b.name) : (S.phase === 'finished' ? 'Cup complete' : '')}</div></div>
    <div class="stat"><div class="k">Strength leader</div><div class="v">${lead ? esc(lead.name) : '—'}</div><div class="s">${lead ? Math.round(lead.rating) + ' strength' : ''}</div></div>
    <div class="stat"><div class="k">Fans</div><div class="v">${S.fans.length}</div><div class="s">${S.predictions.length} predictions made</div></div></div>`;
}
function teamRow(t, sc, m, side) {
  if (!t) return `<div class="team empty"><i class="seed">–</i><span class="nm">${m.status === 'bye' ? 'BYE' : 'TBD'}</span></div>`;
  const done = m.status === 'done', win = done && m.winner === t.id, lose = done && m.winner !== t.id;
  return `<div class="team ${win ? 'win' : ''} ${lose ? 'lose' : ''}"><i class="seed">${t.seed}</i><span class="nm">${esc(t.name)}</span><b class="sc">${done ? sc : ''}</b></div>`;
}
function matchCard(m, nextId) {
  const sched = m.status === 'bye' ? 'bye' : '';
  const head = m.status === 'ready' ? `<span class="live">${m.id === nextId ? '● NEXT' : 'READY'}</span>` : `<span>${m.status === 'done' ? 'DONE' : m.status.toUpperCase()}</span>`;
  return `<div class="match ${m.status} ${m.id === nextId ? 'next' : ''}" data-m="${m.id}" id="m${m.id}">
    <div class="mhead"><span>#${m.id} ${head}</span><span>${sched}</span></div>${teamRow(m.a, m.scoreA, m, 'a')}${teamRow(m.b, m.scoreB, m, 'b')}</div>`;
}
function renderBracket() {
  const nextId = S.upcoming[0] || 0, cols = [];
  for (let r = 1; r <= S.rounds; r++) {
    const ms = S.matches.filter(m => m.round === r).sort((a, b) => a.pos - b.pos);
    cols.push(`<div class="round"><h4>${esc(rname(r))}</h4><div class="col">${ms.map(m => matchCard(m, nextId)).join('')}</div></div>`);
  }
  return `<div class="bracket-wrap"><div class="bracket" id="bracket"><svg class="links" id="links"></svg><div class="rounds">${cols.join('')}</div></div></div>
    <div class="legend"><span><i style="background:var(--accent2)"></i>Ready to play (click)</span><span><i style="background:var(--good)"></i>Winner</span><span><i style="background:var(--accent)"></i>Advancement path</span><span>Click a match to enter the score</span></div>`;
}
function drawLinks() {
  const b = $('#bracket'), svg = $('#links'); if (!b || !svg) return;
  const br = b.getBoundingClientRect(); let out = '';
  svg.setAttribute('viewBox', `0 0 ${b.scrollWidth} ${b.scrollHeight}`);
  for (const m of S.matches) {
    if (!m.parent) continue;
    const c = $('#m' + m.id), p = $('#m' + m.parent); if (!c || !p) continue;
    const a = c.getBoundingClientRect(), q = p.getBoundingClientRect();
    const x1 = a.right - br.left, y1 = a.top + a.height / 2 - br.top, x2 = q.left - br.left, y2 = q.top + q.height / 2 - br.top, xm = (x1 + x2) / 2;
    out += `<path class="${m.status === 'done' || m.status === 'bye' ? 'won' : ''}" d="M${x1} ${y1} H${xm} V${y2} H${x2}"/>`;
  }
  svg.innerHTML = out;
}

function renderUpNext() {
  const ups = S.upcoming.map(matchById);
  return `<div class="card" style="max-width:760px"><h3>Up next</h3><div class="upnext">${ups.length ? ups.map((m, i) => `
    <div class="up"><div class="time">#${m.id}</div><div class="vs">${esc(m.a.name)} <span class="hint">vs</span> ${esc(m.b.name)}<small>${esc(rname(m.round))}${i === 0 ? ' · plays next' : ''}</small></div>
    <button class="btn sm" data-m="${m.id}">Enter</button><button class="btn sm primary" data-simm="${m.id}">Sim</button></div>`).join('') : `<div class="empty-note">${S.phase === 'finished' ? 'All matches played.' : 'No matches ready.'}</div>`}</div>
    <p class="hint" style="margin-top:14px">When both clubs of a match are known, the match becomes ready. Matches are listed earliest round first, so the cup is always played in bracket order.</p></div>`;
}

/* ---------- clubs & squads ---------- */
const POS = ['GK', 'DEF', 'MID', 'FWD'];
function squadHtml(tid) {
  const sq = (S.squads || {})[tid] || [];
  if (!sq.length) return '<div class="empty-note">No players yet.</div>';
  return POS.map(pos => { const ps = sq.filter(p => p.pos === pos); if (!ps.length) return '';
    return `<div class="hint" style="margin:8px 0 4px">${pos} · ${ps.length}</div>` + ps.map(p => `<div class="trow"><span class="idx">#${p.num}</span><span class="nm">${esc(p.name)}</span><span class="rt">${p.pos}</span><button class="x" data-rmp="${p.id}" title="Remove player">×</button></div>`).join(''); }).join('');
}
function renderClubs() {
  return `<div class="grid g3">${S.teams.map(tm => { const sq = (S.squads || {})[tm.id] || [];
    const c = pos => sq.filter(p => p.pos === pos).length;
    return `<div class="card"><h3><span class="link" data-team="${tm.id}">${esc(tm.name)}</span> <span class="tag">seed ${tm.seed}</span></h3>
      <div class="cells"><span class="cell">${tm.squadSize} players</span><span class="cell">GK ${c('GK')}</span><span class="cell">DEF ${c('DEF')}</span><span class="cell">MID ${c('MID')}</span><span class="cell">FWD ${c('FWD')}</span></div>
      <div class="hint" style="margin-top:10px">${tm.wins}W-${tm.losses}L · ${tm.alive ? 'still in the cup' : 'out'}</div>
      <button class="btn sm" style="margin-top:10px" data-team="${tm.id}">Open squad</button></div>`; }).join('')}</div>
    <p class="hint" style="margin-top:14px">Every player is registered once across the cup. Duplicate names and shirt numbers are rejected.</p>`;
}

/* ---------- fan predictions (virtual points only) ---------- */
function renderPredict() {
  const readyM = S.upcoming.map(matchById), fans = S.fans;
  if (!selFan || !fans.find(f => f.id === selFan)) selFan = fans.length ? fans[0].id : 0;
  const fan = fans.find(f => f.id === selFan);
  const board = fans.length ? `<table><thead><tr><th>#</th><th>Fan</th><th class="num">Points</th><th class="num">At stake</th><th class="num">Right</th></tr></thead><tbody>${fans.map((f, i) =>
    `<tr><td class="rank ${i < 3 ? 'r' + (i + 1) : ''}">${i + 1}</td><td><span class="link" data-fan="${f.id}">${esc(f.name)}</span></td><td class="mono num">${f.points}</td><td class="num">${f.open}</td><td class="num">${f.won}/${f.total}</td></tr>`).join('')}</tbody></table>`
    : '<div class="empty-note">No fans yet. Add a fan to start predicting.</div>';
  const cards = readyM.map(m => { const pa = expect(m.a.rating, m.b.rating), clamp = p => Math.min(.85, Math.max(.15, p)); const oa = (1 / clamp(pa)).toFixed(2), ob = (1 / clamp(1 - pa)).toFixed(2);
    const mine = S.predictions.find(p => p.fanId === selFan && p.matchId === m.id);
    return `<div class="up" style="flex-wrap:wrap"><div class="time">#${m.id}</div><div class="vs">${esc(m.a.name)} <span class="hint">vs</span> ${esc(m.b.name)}<small>${esc(rname(m.round))} · pays ×${oa} / ×${ob}</small></div>
      ${!fan ? '' : mine ? `<span class="hint">${esc(fan.name)} backs <b>${esc(mine.pick)}</b> (${mine.stake})</span>` :
        `<select class="input" style="width:auto" id="pk${m.id}"><option value="${m.a.id}">${esc(m.a.name)}</option><option value="${m.b.id}">${esc(m.b.name)}</option></select>
         <select class="input" style="width:auto" id="st${m.id}">${[5,10,20,50,100].filter(v=>v<=fan.points||v===5).map(v=>`<option ${v===10?'selected':''}>${v}</option>`).join('')}</select><button class="btn sm primary" data-pred="${m.id}">Predict</button>`}</div>`; }).join('');
  const preds = S.predictions.slice(0, 40).map(p => `<div class="trow"><span class="nm">${esc(p.fan)} → ${esc(p.pick)}</span><span class="rt">match #${p.matchId} · stake ${p.stake}</span><span class="status ${p.state === 'won' ? 'alive' : ''}">${p.state === 'open' ? 'open' : p.state === 'won' ? '+' + p.payout : '−' + p.stake}</span></div>`).join('');
  return `<div class="grid g2">
    <div class="card"><h3>Fan leaderboard</h3>${board}
      <form class="row" id="fanForm" style="gap:8px;margin-top:12px"><input class="input" id="fName" placeholder="Fan name" maxlength="30" autocomplete="off" style="flex:2"><button class="btn primary" style="flex:none">Add fan</button><button class="btn sm" type="button" data-fanfill="1" style="flex:none">+ sample fans</button></form>
      <p class="hint" style="margin-top:12px">Virtual points only, no real money. Everyone starts with 100 points. A correct pick returns the stake times the odds, so backing the underdog pays more. Undo a result and the points go back automatically.</p></div>
    <div class="card"><h3>Place a prediction ${fan ? `<span class="tag">as ${esc(fan.name)} · ${fan.points} pts</span>` : ''}</h3>
      ${fans.length ? `<div class="field"><label>Predicting as</label><select class="input" id="fanSel">${fans.map(f => `<option value="${f.id}" ${f.id === selFan ? 'selected' : ''}>${esc(f.name)} (${f.points} pts)</option>`).join('')}</select></div>` : ''}
      <div class="upnext">${S.phase === 'finished' ? '<div class="empty-note">The cup is over.</div>' : cards || '<div class="empty-note">No match is open for predictions.</div>'}</div>
      <div class="hint" style="margin:16px 0 6px">Recent predictions</div><div class="teamlist" style="max-height:220px">${preds || '<div class="empty-note">None yet.</div>'}</div></div></div>`;
}

function statusLabel(t) {
  if (S.champion && S.champion.id === t.id) return ['★ Champion', 'champ'];
  if (t.alive) return ['In contention · ' + rname(t.reached), 'alive'];
  return ['Out · ' + rname(t.reached), ''];
}
function renderRankings() {
  const list = rankMode === 'standings' ? S.standings : S.power;
  const lo = Math.min(...S.teams.map(t => t.rating)) - 20, hi = Math.max(...S.teams.map(t => t.rating)) + 20;
  return `<div class="card"><div class="seg"><button data-rm2="standings" class="${rankMode === 'standings' ? 'on' : ''}">Cup standings</button><button data-rm2="power" class="${rankMode === 'power' ? 'on' : ''}">Club strength table</button></div>
    
    <table><thead><tr><th>#</th><th>Club</th><th>Seed</th><th class="num">W-L</th><th class="num">PF:PA</th><th>Strength</th><th>Status</th></tr></thead><tbody>
    ${list.map((t, i) => { const [lb, cl] = statusLabel(t); const d = Math.round(t.rating - t.rating0);
      return `<tr><td class="rank ${i < 3 ? 'r' + (i + 1) : ''}">${i + 1}</td><td><span class="link" data-team="${t.id}">${esc(t.name)}</span></td><td class="mono">${t.seed}</td>
      <td class="num">${t.wins}-${t.losses}</td><td class="num">${t.pf}:${t.pa}</td>
      <td class="mono">${Math.round(t.rating)} <span class="${d >= 0 ? 'up-c' : 'dn-c'}">${d >= 0 ? '+' : ''}${d}</span><span class="bar"><i style="width:${(t.rating - lo) / (hi - lo) * 100}%"></i></span></td>
      <td class="status ${cl}">${lb}</td></tr>`; }).join('')}</tbody></table></div>`;
}

function renderHistory() {
  const hs = S.history.filter(h => !histTeam || h.teamA === histTeam || h.teamB === histTeam);
  const d = (a, b) => { const x = Math.round(b - a); return `<span class="${x >= 0 ? 'up-c' : 'dn-c'}">${x >= 0 ? '+' : ''}${x}</span>`; };
  return `<div class="grid g2">
    <div class="card"><h3>Match history</h3>
      <div class="field"><select class="input" id="hTeam"><option value="0">All clubs</option>${S.teams.map(t => `<option value="${t.id}" ${t.id === histTeam ? 'selected' : ''}>${esc(t.name)}</option>`).join('')}</select></div>
      <div class="hist">${hs.length ? hs.map(h => `<div class="res ${h.seq === S.history[0].seq ? 'top-of-stack' : ''}"><div class="top2"><span>#${h.matchId} · ${esc(rname(h.round))}</span><span>result ${h.seq}</span></div>
        <div class="line"><span class="${h.winnerId === h.teamA ? 'w' : ''}">${esc(h.nameA)}</span><span class="sc">${h.scoreA}</span><span class="hint">–</span><span class="sc">${h.scoreB}</span><span class="${h.winnerId === h.teamB ? 'w' : ''}">${esc(h.nameB)}</span></div>
        <div class="elo">Strength ${esc(h.nameA)} ${Math.round(h.ra1)} (${d(h.ra0, h.ra1)}) · ${esc(h.nameB)} ${Math.round(h.rb1)} (${d(h.rb0, h.rb1)})</div></div>`).join('') : '<div class="empty-note">No results yet. Play a match from the bracket.</div>'}</div></div>
    <div class="card"><h3>Activity feed</h3><div class="feed">${S.events.map(e => `<div class="ev"><span class="dot ${e.kind}"></span><small>${e.seq}</small><span>${esc(e.msg)}</span></div>`).join('')}</div></div></div>`;
}

function renderRunning() {
  const tabs = [['bracket', 'Bracket'], ['upnext', 'Up next'], ['clubs', 'Clubs & squads'], ['predict', 'Predictions'], ['rankings', 'Tables'], ['history', 'History']];
  const champ = S.champion ? `<div class="champ-banner"><span class="trophy">★</span><div><div class="hint">Cup winner</div><b>${esc(S.champion.name)}</b></div></div>` : '';
  const view = { bracket: renderBracket, upnext: renderUpNext, clubs: renderClubs, predict: renderPredict, rankings: renderRankings, history: renderHistory }[tab]();
  $('#main').innerHTML = champ + stats() + `<div class="tabs">${tabs.map(([k, l]) => `<button class="tab ${tab === k ? 'on' : ''}" data-tab="${k}">${l}</button>`).join('')}</div>${view}`;
  if (tab === 'bracket') requestAnimationFrame(drawLinks);
}

/* ---------- modals ---------- */
function closeModal() { $('#modal').hidden = true; $('#modal').innerHTML = ''; }
function openModal(html) { const m = $('#modal'); m.innerHTML = `<div class="modal">${html}</div>`; m.hidden = false; m.onclick = e => { if (e.target === m) closeModal(); }; }
function matchModal(id) {
  const m = matchById(id); if (!m) return;
  const rn = rname(m.round);
  if (m.status === 'done') {
    const h = S.history.find(x => x.matchId === id);
    return openModal(`<h2>Match #${m.id} · ${esc(rn)}</h2><div class="sub">Full time</div>
      <div class="vs-grid"><div class="tm"><b>${esc(m.a.name)}</b><small>seed ${m.a.seed}</small><div class="score" style="color:${m.winner === m.a.id ? 'var(--good)' : 'var(--muted)'}">${m.scoreA}</div></div><span class="hint">vs</span>
      <div class="tm"><b>${esc(m.b.name)}</b><small>seed ${m.b.seed}</small><div class="score" style="color:${m.winner === m.b.id ? 'var(--good)' : 'var(--muted)'}">${m.scoreB}</div></div></div>
      ${h ? `<p class="hint" style="margin-top:16px">Strength: ${esc(h.nameA)} ${Math.round(h.ra0)} → ${Math.round(h.ra1)} · ${esc(h.nameB)} ${Math.round(h.rb0)} → ${Math.round(h.rb1)}</p>` : ''}
      <div class="foot"><button class="btn" onclick="closeModal()">Close</button></div>`);
  }
  if (m.status !== 'ready') {
    return openModal(`<h2>Match #${m.id} · ${esc(rn)}</h2><div class="sub">${m.status === 'bye' ? 'First-round bye: the top seed advances automatically.' : 'Waiting for earlier matches to finish.'}</div><div class="foot"><button class="btn" onclick="closeModal()">Close</button></div>`);
  }
  const p = expect(m.a.rating, m.b.rating) * 100;
  openModal(`<h2>Match #${m.id} · ${esc(rn)}</h2><div class="sub">Ready to play</div>
    <div class="vs-grid"><div class="tm"><b>${esc(m.a.name)}</b><small>seed ${m.a.seed} · strength ${Math.round(m.a.rating)}</small><select class="input score" id="sA">${Array.from({length:16},(_,i)=>`<option>${i}</option>`).join('')}</select></div><span class="hint">vs</span>
    <div class="tm"><b>${esc(m.b.name)}</b><small>seed ${m.b.seed} · strength ${Math.round(m.b.rating)}</small><select class="input score" id="sB">${Array.from({length:16},(_,i)=>`<option>${i}</option>`).join('')}</select></div></div>
    <div class="prob"><i class="a" style="width:${p}%"></i><i class="b" style="width:${100 - p}%"></i></div><div class="prob-l"><span>${p.toFixed(0)}% win chance</span><span>${(100 - p).toFixed(0)}%</span></div>
    <div class="foot"><button class="btn" onclick="closeModal()">Cancel</button><button class="btn" id="mSim">Simulate</button><button class="btn primary" id="mGo">Record result</button></div>`);
  $('#sA').focus();
  $('#mGo').onclick = async () => {
    const a = +$('#sA').value, b = +$('#sB').value;
    if (a === b) return toast('Cup matches need a winner (no draws)', 'err');
    if (await call('play', { matchId: id, scoreA: a, scoreB: b })) closeModal();
  };
  $('#mSim').onclick = async () => { if (await call('sim', { matchId: id })) closeModal(); };
  $('#modal').onkeydown = e => { if (e.key === 'Enter') $('#mGo').click(); if (e.key === 'Escape') closeModal(); };
}
function teamModal(id) {
  const t = byId(id); if (!t) return;
  const hs = S.history.filter(h => h.teamA === id || h.teamB === id);
  const [lb] = statusLabel(t);
  openModal(`<h2>${esc(t.name)}</h2><div class="sub">Seed ${t.seed || '–'} · strength ${Math.round(t.rating)} · ${S.phase === 'setup' ? 'registered' : esc(lb)}</div>
    <div class="cells" style="margin-bottom:12px"><span class="cell">${t.wins} wins</span><span class="cell">${t.losses} losses</span><span class="cell">${t.byes} bye</span><span class="cell">goals ${t.pf}:${t.pa}</span></div>
    <div class="hint" style="margin-bottom:6px">Squad · ${t.squadSize}/25 players</div>
    <div class="hist" style="max-height:200px" id="squadBox">${squadHtml(id)}</div>
    <form class="row" id="plForm" style="gap:6px;margin-top:10px"><input class="input" id="pName" placeholder="Player name" maxlength="38" autocomplete="off" style="flex:3">
      <select class="input" id="pPos" style="flex:1">${POS.map(p => `<option ${p === 'MID' ? 'selected' : ''}>${p}</option>`).join('')}</select>
      <select class="input" id="pNum" style="flex:1">${Array.from({length:99},(_,i)=>`<option>${i+1}</option>`).join('')}</select><button class="btn primary" style="flex:none">Add</button></form>
    <div class="row nofill" style="margin-top:8px"><button class="btn sm" id="plFill">Fill sample squad</button></div>
    ${S.history.length ? `<div class="hint" style="margin:14px 0 6px">Match history (newest first)</div>
    <div class="hist" style="max-height:150px">${hs.length ? hs.map(h => { const opp = h.teamA === id ? h.nameB : h.nameA, mine = h.teamA === id ? h.scoreA : h.scoreB, th = h.teamA === id ? h.scoreB : h.scoreA;
      return `<div class="res"><div class="line"><span class="${h.winnerId === id ? 'w' : ''}">${h.winnerId === id ? 'W' : 'L'}</span><span class="sc">${mine}-${th}</span><span>vs ${esc(opp)}</span></div><div class="top2"><span>${esc(rname(h.round))}</span><span>#${h.matchId}</span></div></div>`; }).join('') : '<div class="empty-note">No matches played yet.</div>'}</div>` : ''}
    <div class="foot"><button class="btn" onclick="closeModal()">Close</button></div>`);
  const reopen = () => { if (!$('#modal').hidden) teamModal(id); };
  $('#plForm').onsubmit = async e => { e.preventDefault(); const nm = $('#pName').value.trim(); if (!nm) return;
    if (await call('player', { teamId: id, name: nm, pos: $('#pPos').value, number: +$('#pNum').value })) { reopen(); $('#pName').focus(); } };
  $('#plFill').onclick = async () => { if (await call('squad/fill', { teamId: id })) reopen(); };
  document.querySelectorAll('#squadBox [data-rmp]').forEach(b => b.onclick = async () => { if (await call('player/remove', { id: +b.dataset.rmp })) reopen(); });
  $('#pName').focus();
}

/* ---------- confetti ---------- */
function confetti() {
  const c = $('#fx'), x = c.getContext('2d'); c.width = innerWidth; c.height = innerHeight;
  const cols = ['#6c8cff', '#22d3ee', '#fbbf24', '#34d399', '#f472b6', '#a78bfa'];
  const ps = Array.from({ length: 160 }, () => ({ x: Math.random() * c.width, y: -20 - Math.random() * c.height * .5, vx: Math.random() * 4 - 2, vy: 2 + Math.random() * 4, r: 3 + Math.random() * 4, c: cols[Math.random() * cols.length | 0], a: Math.random() * 6 }));
  let f = 0;
  (function step() {
    x.clearRect(0, 0, c.width, c.height);
    for (const p of ps) { p.x += p.vx; p.y += p.vy; p.a += .1; x.save(); x.translate(p.x, p.y); x.rotate(p.a); x.fillStyle = p.c; x.fillRect(-p.r, -p.r / 2, p.r * 2, p.r); x.restore(); }
    if (++f < 260) requestAnimationFrame(step); else x.clearRect(0, 0, c.width, c.height);
  })();
}

/* ---------- main render + events ---------- */
function render() {
  renderTop();
  if (!S.exists) return renderWelcome();
  if (S.phase === 'setup') return renderSetup();
  renderRunning();
}
document.addEventListener('click', e => {
  const t = e.target.closest('[data-act],[data-tab],[data-m],[data-simm],[data-team],[data-rm2],[data-pred],[data-fan],[data-fanfill]'); if (!t) return;
  const d = t.dataset;
  if (d.act === 'reset') { if (confirm('Discard this tournament and start over?')) call('reset'); }
  else if (d.act) call(d.act === 'sim' ? 'sim' : d.act, {});
  else if (d.tab) { tab = d.tab; render(); }
  else if (d.simm) call('sim', { matchId: +d.simm });
  else if (d.m) matchModal(+d.m);
  else if (d.team) teamModal(+d.team);
  else if (d.rm2) { rankMode = d.rm2; render(); }
  else if (d.fan) { selFan = +d.fan; render(); }
  else if (d.fanfill) call('fan/fill', {});
  else if (d.pred) {
    const mid = +d.pred;
    call('predict', { fanId: selFan, matchId: mid, pickId: +$('#pk' + mid).value, stake: +$('#st' + mid).value }).then(ok => ok && toast('Prediction placed (virtual points)'));
  }
});
document.addEventListener('submit', e => {
  if (e.target.id !== 'fanForm') return;
  e.preventDefault(); const nm = $('#fName').value.trim(); if (nm) call('fan', { name: nm }).then(ok => { if (ok) { const f = S.fans.find(x => x.name === nm); if (f) selFan = f.id; render(); } });
});
document.addEventListener('change', e => {
  if (e.target.id === 'hTeam') { histTeam = +e.target.value; render(); }
  if (e.target.id === 'fanSel') { selFan = +e.target.value; render(); }
});
document.addEventListener('keydown', e => { if (e.key === 'Escape') closeModal(); });
addEventListener('resize', () => { if (tab === 'bracket') drawLinks(); });
load();
