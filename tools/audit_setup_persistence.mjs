// Source inventory, not a substitute for exercising every control in the GUI.
// Run from the repository root: node tools/audit_setup_persistence.mjs
import fs from 'node:fs';
import assert from 'node:assert/strict';
const bridgePath = 'src/bridge/DecodiumBridge.cpp';
const bridge = fs.readFileSync(bridgePath, 'utf8').replace(/\r/g, '');
const header = fs.readFileSync('src/bridge/DecodiumBridge.h', 'utf8');
const tabs = ['Station', 'Radio', 'Audio', 'TX', 'Display', 'Decode', 'Reporting',
    'Frequencies', 'Colours', 'Advanced', 'Alerts', 'Filters', 'UI Buttons', 'Callsign'];
const functions = [...bridge.matchAll(/^[\w:<>&* ]+DecodiumBridge::(\w+)\([^\n]*\)(?: const)?\n\{[\s\S]*?^\}/gm)];
const line = (s, n) => s.slice(0, n).split('\n').length;
const rows = [];
const escape = s => s.replaceAll('|', '\\|').replaceAll('\n', ' ');
for (let tab = 0; tab < tabs.length; ++tab) {
    const file = `qml/decodium/components/SettingsTab${tab}.qml`;
    const source = fs.readFileSync(file, 'utf8');
    // A construction/binding update is not a user edit.
    assert(!/^\s*onCheckedChanged:/.test(source));
    assert(!source.match(/^\s*onCheckedChanged:/m), file);
    assert(!source.match(/^\s*onValueChanged:/m), file);
    assert(!source.match(/checked:.*bridge\.getSetting/), file);
    const seen = new Set();
    const add = (name, match, route) => {
        if (seen.has(name)) return;
        seen.add(name);
        rows.push({tab, name, source: `${file}:${line(source, match.index)}`, route});
    };
    for (const m of source.matchAll(/(?:getSetting|setSetting|boolSetting|setBoolSettingIfChanged|territorySettingMatches|setTerritoryExcluded)\("([^"]+)"/g))
        add(m[1], m, m[1] === 'uiScaleFactor' ? 'Machine-wide Decodium3 root (startup scale)' : 'getSetting/setSetting: active profile; legacy aliases where required');
    for (const m of source.matchAll(/((?:bridge|dialog\.callsignService)(?:\.\w+)*)\.(\w+)\s*=(?!=)/g)) {
        const owner = m[1], prop = m[2];
        let route = 'Service property: paired load/save in owning service (see store map below)';
        if (owner === 'bridge') {
            const declaration = header.split('\n').find(l => l.includes('Q_PROPERTY') && l.includes(` ${prop} READ `));
            const setter = declaration?.match(/WRITE (\w+)/)?.[1];
            const impl = functions.find(f => f[1] === setter);
            route = impl ? `${bridgePath}:${line(bridge, impl.index)} (${setter}); immediate write or saveSettings snapshot` : `Bridge property ${prop}; setter/snapshot route`;
        }
        add(`${owner}.${prop}`, m, route);
    }
    for (const m of source.matchAll(/bridge\.(set\w+)\(/g)) {
        if (m[1] === 'setSetting') continue;
        const impl = functions.find(f => f[1] === m[1]);
        add(m[1], m, impl ? `${bridgePath}:${line(bridge, impl.index)}; setter or snapshot` : 'Bridge API; out-of-line implementation/model persistence');
    }
    // Dynamic colour/category/visibility controls must appear in the inventory.
    for (const m of source.matchAll(/(?:key|targetProp|settingKey)\s*:\s*"([^"]+)"/g)) {
        if (['phosphor', 'cyan', 'amber', 'red'].includes(m[1])) continue; // values of accentVariant, not keys
        add(m[1], m, 'Dynamic control: generic settings or colour property setter');
    }
    // Keep every edit handler, including controller/dialog helper dispatches.
    for (const m of source.matchAll(/^\s*(on(?:Toggled|Clicked|Activated|Moved|ValueModified|EditingFinished|TextEdited|TextChanged)):\s*([^\n]*)/gm))
        rows.push({tab, name: `${m[1]} ${m[2].trim()}`, source: `${file}:${line(source, m.index)}`, route: 'UI edit/action coverage; not every action is a persistent preference'});
}
// Fail on future root-only setters, while preserving the intentionally global GPU switch.
let profiledSetters = 0;
for (const f of functions) {
    if (!f[1].startsWith('set') || !f[0].includes('QSettings') || !f[0].includes('"Decodium3"')) continue;
    if (f[1] === 'setLowEndMode') continue;
    assert(f[0].includes('beginActiveSettingsProfile'), `Unscoped writer: ${f[1]}`);
    ++profiledSetters;
}
const shutdown = functions.find(f => f[1] === 'shutdown')[0];
assert(shutdown.includes('saveSettings();') && !shutdown.includes('saveSettingsAsync();'));
const boolSource = fs.readFileSync('qml/decodium/components/SettingsDialog.qml', 'utf8');
for (const m of boolSource.matchAll(/prop:\s*"(color\w+)"/g)) {
    for (const key of [m[1], `${m[1]}Enabled`, `bold_${m[1]}`, `bg_${m[1]}`, `bgEnabled_${m[1]}`])
        rows.push({tab:8, name:key, source:`qml/decodium/components/SettingsDialog.qml:${line(boolSource,m.index)}`,
                   route:'Expanded category: active-profile setter + loadSettings/saveSettingsInternal'});
}
for (const m of boolSource.matchAll(/(?:getSetting|setSetting)\("([^"]+)"/g)) {
    if (rows.some(r => r.name === m[1])) continue;
    const tab = m[1].startsWith('alert') ? 10 : m[1] === 'uiDisabledBands' ? 7 : 4;
    rows.push({tab, name:m[1], source:`qml/decodium/components/SettingsDialog.qml:${line(boolSource,m.index)}`,
               route:'SettingsDialog shared helper: generic settings'});
}
const boolBody = boolSource.match(/function boolSetting\(key, fallback\) \{([\s\S]*?)\n    \}/)[1];
for (const [stored, expected] of [[false,false], ['false',false], ['0',false], [0,false], ['true',true], ['1',true], [true,true]]) {
    const read = new Function('bridge', 'key', 'fallback', boolBody);
    assert.equal(read({getSetting: () => stored}, 'WeatherApiEnable', false), expected);
}
let report = `# Issue 84 — Setup persistence source inventory\n\n`;
report += `Generated by tools/audit_setup_persistence.mjs. ${rows.length} setting/API/handler entries across 14 Setup pages; ${profiledSetters} scoped bridge writers checked. Rows are source evidence, **not** a claim that each GUI control was clicked and restarted. No user values or credentials are collected.\n\n`;
report += `## Store map and review\n\n`;
report += `- Bridge preferences: IniFormat, UserScope, Decodium/Decodium3; named profile MultiSettings/<name>. New profiles inherit root once; subsequent edits must stay in that profile.\n`;
report += `- Machine-wide: LowEndMode and uiScaleFactor (read before profile selection); UI/Style uses the base application's native QSettings store. Logbook catalogue is shared, intentionally.\n`;
report += `- Other UI/quality/FPS/smooth-flow settings use matching default QSettings reads/writes, application/rig name scoped. Theme colours use DecodiumThemeManager's paired profiledThemeValue/setProfiledThemeValue.\n`;
report += `- CAT: DecodiumCatManager (CAT), DecodiumTransceiverManager (Transceiver, also TCI), DecodiumOmniRigManager, DecodiumCat4OmManager: matching active-profile load/save groups. Serial capability constraints and numeric bounds are intentional.\n`;
report += `- DX Cluster: DecodiumDxCluster loadSettings/saveSettings, DXCluster group and compatibility aliases; saved on final synchronous shutdown, even without connecting.\n`;
report += `- Callsign: CallsignIntelligenceService, CallsignIntelligence group; all ten editable properties have saveSetting/loadSettings pairs. Secret fields use SecureSettings, not plaintext export.\n`;
report += `- Generic/legacy options: setSetting synchronises writes; active-profile values take precedence over the embedded legacy root on read. Legacy store is decodium4.ini; its MultiSettings mechanism swaps a current root configuration, not independent concurrent per-profile files.\n`;
report += `- Frequencies/stations: bridge model editing/import/export and legacy synchronisation; calibration is stored by dedicated setters. Colours: category foreground, enabled, bold, background and background-enabled use profile keys and full-save snapshots.\n`;
report += `- Filters/alerts/UI buttons: generic keys, indexed lists and visibility/order keys are persistent; Wanted callsign reader now follows the active profile.\n`;
report += `- Runtime actions (connect, transmit, test sound, fetch weather, file picker, reset counter, current QSO) are not preferences. Fullscreen is explicitly session-only; forced monitoring/safety constraints are separate from preference storage.\n\n`;
report += `## Corrections\n\n35 formerly root-only bridge writers aligned with profile reads; Wanted alerts, filter snapshots, spectrum timing and CAT reconnect history aligned; machine-wide scale/style made consistent; boolean INI strings normalised; checkbox/spinbox/slider writes now use user-edit signals; final shutdown save made synchronous including service managers. No migration guesses which profile owned previously leaked root values.\n\n`;
report += `## Verification boundary\n\nThe source-contract script checks all 14 pages and profile-aware setters. test_settings_profile_restart launches 11 separate processes using synthetic values and the real DecodiumProfileSettings helper, in a temporary INI store. It verifies close/reopen, first-use inheritance, false booleans and no root/A/B cross-over. It does **not** instantiate the complete Decodium GUI, radio managers, keychain or legacy backend. Whole-application GUI restart tests and Windows/Linux runtime tests remain outstanding. Do not treat the generated source inventory as end-to-end certification of every control.\n\n`;
for (let tab = 0; tab < tabs.length; ++tab) {
    report += `## ${tab}: ${tabs[tab]}\n\n| Setting / handler | Source | Persistence path / classification |\n|---|---|---|\n`;
    for (const row of rows.filter(r => r.tab === tab)) report += `| ${escape(row.name)} | ${row.source} | ${escape(row.route)} |\n`;
    report += '\n';
}
fs.writeFileSync('doc/ISSUE_84_SETTINGS_AUDIT.md', report.trimEnd() + '\n');
// Key inventory for the isolated cross-process persistence test. Values are synthetic.
const keys = new Set(rows.filter(r => !r.name.includes('.') && !r.name.startsWith('on') && !r.name.startsWith('set')).map(r => r.name));
for (const f of functions.filter(f => f[1].startsWith('set') && f[0].includes('beginActiveSettingsProfile')))
    for (const m of f[0].matchAll(/setValue\((?:QStringLiteral\()?"([^"]+)"/g))
        if (!m[1].includes('%')) keys.add(m[1]);
fs.writeFileSync('tests/settings_audit_keys.json', JSON.stringify([...keys].sort(), null, 2) + '\n');
console.log(`${rows.length} inventory entries; ${keys.size} synthetic restart keys; ${profiledSetters} scoped writers; boolean coercion checks passed.`);
