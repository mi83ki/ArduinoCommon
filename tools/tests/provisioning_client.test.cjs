const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');

/** ブラウザーと同じ公開Clientを読み、時計と通信だけを置き換える。 */
function load(t, fetch) {
  t.mock.timers.enable({ apis: ['setTimeout', 'Date'], now: 0 });
  const context = { window: {}, TextEncoder, AbortController, fetch,
    setTimeout: (...args) => setTimeout(...args), clearTimeout: (...args) => clearTimeout(...args),
    Date: { now: () => Date.now() } };
  vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../../src/provisioning/web/common.js'), 'utf8'), context);
  return new context.window.CommonProvisioning.Client();
}
const response = (value, status = 200) => ({ ok: status < 400, status, json: async () => value });
const flush = () => new Promise(resolve => setImmediate(resolve));
const hang = signal => new Promise((resolve, reject) => signal.addEventListener('abort', () => reject(signal.reason)));

test('応答待ちの中断を利用者向けの日本語に変換する', async t => {
  const client = load(t, (url, options) => hang(options.signal));
  const result = assert.rejects(client.request('/api/status'), error =>
    error.retryable === true && /応答.*待ち時間/.test(error.message) && !/aborted/.test(error.message));
  t.mock.timers.tick(5000); await result;
});

test('検索開始の応答が消えても再送せず取得結果を待つ', async t => {
  const calls = [];
  const client = load(t, (url, options) => {
    calls.push(options.method);
    return options.method === 'POST' ? hang(options.signal) : response({ state: 'ready', networks: [{ ssid: 'target' }] });
  });
  const result = client.scanNetworks();
  t.mock.timers.tick(5000); await flush(); t.mock.timers.tick(1000); await flush();
  assert.equal((await result).networks[0].ssid, 'target');
  assert.deepEqual(calls, ['POST', 'GET']);
});

test('検索結果取得だけが一時停止しても再取得できる', async t => {
  let reads = 0;
  const client = load(t, (url, options) => options.method === 'POST' ? response({}) :
    ++reads === 1 ? hang(options.signal) : response({ state: 'ready', networks: [] }));
  const result = client.scanNetworks();
  await flush(); t.mock.timers.tick(1000); await flush();
  t.mock.timers.tick(5000); await flush(); t.mock.timers.tick(1000); await flush();
  assert.equal((await result).state, 'ready'); assert.equal(reads, 2);
});

test('通信が戻らなくても検索全体を20秒で打ち切る', async t => {
  const calls = [];
  const client = load(t, (url, options) => { calls.push(options.method); return hang(options.signal); });
  const result = assert.rejects(client.scanNetworks(), /検索.*待ち時間/);
  for (let i = 0; i < 20; ++i) { t.mock.timers.tick(1000); await flush(); }
  await result; assert.equal(Date.now(), 20000);
  assert.equal(calls.filter(method => method === 'POST').length, 1);
});

test('認証拒否を通信障害と扱わず即時に返す', async t => {
  let calls = 0;
  const client = load(t, () => { calls++; return response({ error: '再接続してください。' }, 403); });
  await assert.rejects(client.scanNetworks(), error => error.status === 403);
  assert.equal(calls, 1);
});

test('端末が検索失敗を返したとき成功扱いしない', async t => {
  const client = load(t, (url, options) => response(options.method === 'POST' ? {} : { state: 'failed' }));
  const result = assert.rejects(client.scanNetworks(), /検索を完了できません/);
  await flush(); t.mock.timers.tick(1000); await result;
});
