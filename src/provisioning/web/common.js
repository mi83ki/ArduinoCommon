/* Wi-Fi設定の共通画面。機種・MQTT・校正の項目は利用側から拡張する。 */
(() => {
  'use strict';
  const bytes = value => new TextEncoder().encode(value).length;
  const element = (tag, text, className) => {
    const node = document.createElement(tag);
    if (text !== undefined) node.textContent = text;
    if (className) node.className = className;
    return node;
  };
  const option = (value, text) => { const node = element('option', text); node.value = value; return node; };
  const field = (container, title, input) => {
    const label = element('label', title); label.appendChild(input); container.appendChild(label); return input;
  };
  const input = (value = '', type = 'text') => {
    const node = element('input'); node.type = type; node.value = value; node.autocomplete = 'off'; return node;
  };
  const select = (entries, value) => {
    const node = element('select'); entries.forEach(entry => node.appendChild(option(...entry))); node.value = value; return node;
  };
  const failure = (message, fieldName) => { const error = new Error(message); error.field = fieldName; return error; };

  class Client {
    constructor() { this._token = ''; }
    async init() { const response = await this.request('/api/session'); this._token = response.token; return response; }
    async request(path, data, timeout = 5000) {
      const controller = new AbortController(); const timer = setTimeout(() => controller.abort(), timeout);
      const headers = {}; if (this._token) headers['X-Setup-Token'] = this._token;
      const options = { method: data === undefined ? 'GET' : 'POST', cache: 'no-store', credentials: 'omit', headers, signal: controller.signal };
      if (data !== undefined) {
        headers['Content-Type'] = 'application/json'; options.body = JSON.stringify(data);
        if (bytes(options.body) > 4096) { clearTimeout(timer); throw failure('設定の文字数が多すぎます。入力を短くしてください。'); }
      }
      try {
        const response = await fetch(path, options); const value = await response.json();
        if (!response.ok) {
          const detail = value.error || {};
          const error = failure(typeof detail === 'string' ? detail : detail.message || '設定を受け付けられませんでした。', detail.field);
          error.status = response.status; error.detail = value; throw error;
        }
        return value;
      } catch (error) {
        if (controller.signal.aborted || (!error.status && error.name === 'TypeError')) {
          const message = controller.signal.aborted ? '端末の応答が待ち時間内に届きませんでした。接続を確認して再度お試しください。' :
            '端末と通信できませんでした。BumbleEyeのWi-Fiへの接続を確認してください。';
          const translated = failure(message); translated.retryable = true; throw translated;
        }
        throw error;
      } finally { clearTimeout(timer); }
    }
    async scanNetworks() {
      const deadline = Date.now() + 20000;
      // 開始POSTの応答が消えても二重に開始せず、結果GETだけを再試行する。
      try { await this.request('/api/scan', {}); }
      catch (error) { if (!error.retryable) throw error; }
      while (Date.now() < deadline) {
        await new Promise(resolve => setTimeout(resolve, Math.min(1000, deadline - Date.now())));
        const remaining = deadline - Date.now(); if (remaining <= 0) break;
        let result;
        try { result = await this.request('/api/scan', undefined, Math.min(5000, remaining)); }
        catch (error) { if (error.retryable) continue; throw error; }
        if (result.state === 'ready') return result;
        if (result.state === 'failed') throw failure('Wi-Fiの検索を完了できませんでした。SSIDを直接入力することもできます。');
      }
      throw failure('検索結果が待ち時間内に届きませんでした。接続を確認するか、SSIDを直接入力してください。');
    }
    operationId() {
      const values = new Uint8Array(16); crypto.getRandomValues(values);
      return Array.from(values, value => value.toString(16).padStart(2, '0')).join('');
    }
  }

  class WiFiForm {
    constructor(client) { this._client = client; this._rows = []; this._networks = []; }
    mount(container, model, constraints = {}) {
      container.replaceChildren(); this._container = container; this._constraints = constraints; this._rows = [];
      this._maximum = Math.min(4, constraints.maximumProfiles || 4);
      const heading = element('div', undefined, 'row-head'); heading.appendChild(element('h2', '接続先のWi-Fi'));
      this._scan = element('button', '周囲のWi-Fiを探す'); this._scan.type = 'button'; heading.appendChild(this._scan); container.appendChild(heading);
      this._scanMessage = element('p', '一覧にないWi-Fiも、SSIDを直接入力できます。', 'hint'); container.appendChild(this._scanMessage);
      this._cards = element('div'); container.appendChild(this._cards);
      this._add = element('button', '予備のWi-Fiを追加'); this._add.type = 'button'; container.appendChild(this._add);
      this._scan.addEventListener('click', () => this._scanNetworks());
      this._add.addEventListener('click', () => this._append({ id: [0,1,2,3].find(id => !this._rows.some(row => row.id === id)), security: 'wpa-personal', ip: { mode: 'dhcp' } }));
      const profiles = model.profiles && model.profiles.length ? model.profiles : [{ id: 0, security: 'wpa-personal', ip: { mode: 'dhcp' } }];
      profiles.forEach(profile => this._append(profile, profile.id === model.primaryProfileId));
      if (!this._rows.some(row => row.primary.checked)) this._rows[0].primary.checked = true;
    }
    _append(model, primary = false) {
      if (this._rows.length >= this._maximum) return;
      const card = element('div', undefined, 'network-card'); const heading = element('div', undefined, 'row-head');
      const row = { id: model.id, card, fields: {}, original: model };
      row.primary = input('', 'radio'); row.primary.name = 'primary-wifi'; row.primary.checked = primary;
      const label = element('label'); label.append(row.primary, document.createTextNode('主Wi-Fiとして接続確認する')); heading.appendChild(label);
      const remove = element('button', '削除', 'danger'); remove.type = 'button'; heading.appendChild(remove); card.appendChild(heading);
      row.pick = select([['', '一覧から選択、または下へ手入力']], ''); field(card, '見つかったWi-Fi', row.pick);
      row.ssid = field(card, 'SSID', input(model.ssid || '')); row.ssid.required = true; row.ssid.maxLength = 31; row.ssid.spellcheck = false;
      row.security = field(card, '認証方式', select([['wpa-personal', 'パスワードあり（WPA/WPA2 Personal）'], ['open', 'パスワードなし（オープン）']], model.security || 'wpa-personal'));
      row.action = field(card, 'パスワードの扱い', select([['keep', '保存済みのパスワードを使う'], ['replace', 'パスワードを入力する'], ['clear', 'パスワードを使わない']], model.passwordSet ? 'keep' : 'replace'));
      row.password = field(card, 'Wi-Fiパスワード', input('', 'password')); row.password.maxLength = 64; row.password.autocomplete = 'new-password';
      const visibility = input('', 'checkbox'); const visibilityLabel = element('label', undefined, 'hint'); visibilityLabel.append(visibility, document.createTextNode('入力中のパスワードを表示')); card.appendChild(visibilityLabel);
      visibility.addEventListener('change', () => { row.password.type = visibility.checked ? 'text' : 'password'; });
      const updateSecret = () => {
        const open = row.security.value === 'open'; const matching = row.ssid.value === (model.ssid || '') && !!model.passwordSet;
        row.action.options[0].disabled = !matching || open; row.action.options[1].disabled = open; row.action.options[2].disabled = !open;
        if (open) row.action.value = 'clear'; else if (row.action.value === 'clear' || (row.action.value === 'keep' && !matching)) row.action.value = 'replace';
        row.password.disabled = row.action.value !== 'replace'; row.password.required = !row.password.disabled;
        row.password.parentElement.hidden = row.password.disabled; visibilityLabel.hidden = row.password.disabled;
      };
      row.ssid.addEventListener('input', updateSecret); row.security.addEventListener('change', updateSecret); row.action.addEventListener('change', updateSecret);
      row.pick.addEventListener('change', () => {
        if (!row.pick.value) return; row.ssid.value = row.pick.value;
        const network = this._networks.find(entry => entry.ssid === row.pick.value);
        if (network) row.security.value = network.open ? 'open' : 'wpa-personal';
        updateSecret(); row.ssid.dispatchEvent(new Event('input', { bubbles: true }));
      });
      const advanced = element('details'); advanced.appendChild(element('summary', '詳細なネットワーク設定'));
      row.mode = field(advanced, 'IPアドレス', select([['dhcp', '自動取得（DHCP）'], ['static', '固定IPを指定']], model.ip && model.ip.mode || 'dhcp'));
      const grid = element('div', undefined, 'grid'); advanced.appendChild(grid);
      [['address','IPアドレス'],['gateway','ゲートウェイ'],['subnet','サブネットマスク'],['dns1','DNS 1'],['dns2','DNS 2（任意）']].forEach(([key,title]) => {
        const box = element('div'); grid.appendChild(box); const node = field(box, title, input(model.ip && model.ip[key] || ''));
        node.inputMode = 'decimal'; node.maxLength = 15; row.fields[key] = node;
      });
      const updateMode = () => { grid.hidden = row.mode.value !== 'static'; Object.entries(row.fields).forEach(([key,node]) => { node.required = !grid.hidden && !key.startsWith('dns'); }); };
      row.mode.addEventListener('change', updateMode); updateMode(); card.appendChild(advanced);
      if (this._constraints.profileExtension) row.extension = this._constraints.profileExtension(card, model);
      row.error = element('p', '', 'field-error'); row.error.setAttribute('role', 'alert'); card.appendChild(row.error);
      remove.addEventListener('click', () => {
        if (this._rows.length === 1) return; this._rows = this._rows.filter(item => item !== row); card.remove();
        if (!this._rows.some(item => item.primary.checked)) this._rows[0].primary.checked = true; this._updateButtons();
      });
      row.remove = remove; updateSecret(); this._rows.push(row); this._cards.appendChild(card); this._updateButtons(); this._showNetworks();
    }
    _updateButtons() { this._add.disabled = this._rows.length >= this._maximum; this._rows.forEach(row => { row.remove.disabled = this._rows.length === 1; }); }
    _showNetworks() {
      this._rows.forEach(row => {
        row.pick.replaceChildren(option('', '一覧から選択、または下へ手入力'));
        this._networks.forEach(network => row.pick.appendChild(option(network.ssid, network.ssid + (network.open ? '（パスワードなし）' : ''))));
      });
    }
    async _scanNetworks() {
      this._scan.disabled = true; this._scanMessage.textContent = 'Wi-Fiを探しています…';
      try {
        const result = await this._client.scanNetworks();
        this._networks = result.networks; this._showNetworks();
        this._scanMessage.textContent = result.networks.length + '件のWi-Fiが見つかりました。主Wi-Fi以外は保存時に接続確認しません。';
      } catch (error) { this._scanMessage.textContent = error.message; }
      finally { this._scan.disabled = false; }
    }
    read() {
      const selected = this._rows.find(row => row.primary.checked);
      const profiles = this._rows.map(row => {
        if (!row.ssid.value || bytes(row.ssid.value) > 31) throw failure('SSIDはUTF-8で1〜31バイトにしてください。', 'profiles.' + row.id + '.ssid');
        const profile = { id: row.id, ssid: row.ssid.value, security: row.security.value, passwordAction: row.action.value, ip: { mode: row.mode.value } };
        if (row.action.value === 'replace') {
          const password = row.password.value;
          if (!((bytes(password) >= 8 && bytes(password) <= 63) || /^[0-9a-fA-F]{64}$/.test(password))) throw failure('パスワードは8〜63バイト、または64桁の16進数で入力してください。', 'profiles.' + row.id + '.password');
          profile.password = password;
        }
        if (row.mode.value === 'static') Object.entries(row.fields).forEach(([key,node]) => { profile.ip[key] = node.value.trim() || (key.startsWith('dns') ? '0.0.0.0' : ''); });
        if (row.extension) Object.assign(profile, row.extension.read()); return profile;
      });
      return { primaryProfileId: selected.id, profiles };
    }
    showErrors(errors) {
      this._rows.forEach(row => { row.error.textContent = ''; row.card.classList.remove('invalid'); });
      errors.forEach(error => {
        const row = this._rows.find(item => (error.field || '').startsWith('profiles.' + item.id + '.'));
        if (row) { row.error.textContent = error.message; row.card.classList.add('invalid'); }
      });
    }
    clearSecrets() { this._rows.forEach(row => { row.password.value = ''; }); }
  }

  function createStatus(container) {
    container.classList.add('status'); container.setAttribute('role', 'status'); container.setAttribute('aria-live', 'polite');
    const labels = { idle:'設定内容を入力してください。', queued:'設定を受け付けました。まだ保存は完了していません。', testing_wifi:'Wi-Fiへの接続を確認しています…', committing:'設定を保存しています…', complete:'設定を保存しました。', failed:'設定を保存できませんでした。', cancelled:'接続試験を取り消しました。' };
    return { show(state) { container.dataset.phase = state.phase || 'idle'; container.textContent = state.message || labels[state.phase] || '処理しています…'; } };
  }
  window.CommonProvisioning = { Client, WiFiForm, createStatus, element, field, input, select, failure };
})();
