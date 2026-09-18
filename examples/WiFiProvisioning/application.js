(() => {
  'use strict';
  const { Client, WiFiForm, createStatus, field, input } = CommonProvisioning;
  const client = new Client(), wifi = new WiFiForm(client), status = createStatus(document.getElementById('status'));
  const form = document.getElementById('setup-form'), save = document.getElementById('save'), cancel = document.getElementById('cancel');
  const displayName = field(document.getElementById('application-fields'), '表示名', input()); displayName.required = true; displayName.maxLength = 48;
  let revision = 0, busy = false, jobId = 0;
  const showError = error => { status.show({ phase: 'failed', message: error.message || 'デバイスのWi-Fiへの接続を確認してください。' }); wifi.showErrors([error]); };
  const poll = async () => {
    if (!busy) return;
    try {
      const result = await client.request('/api/status'); status.show(result); jobId = result.jobId;
      if (['failed','cancelled'].includes(result.phase)) { busy = false; save.disabled = false; cancel.hidden = true; return; }
      if (result.phase === 'complete') { busy = false; cancel.hidden = true; wifi.clearSecrets(); return; }
    } catch (_) { status.show({ phase: 'queued', message: '接続確認中です。接続が切れた場合は、このデバイスのWi-Fiへ接続し直してください。' }); }
    setTimeout(poll, 1000);
  };
  form.addEventListener('submit', async event => {
    event.preventDefault(); if (busy || !form.reportValidity()) return;
    try {
      const networks = wifi.read(); busy = true; save.disabled = true;
      const result = await client.request('/api/config', { operationId: client.operationId(), expectedRevision: revision, testProfileId: networks.primaryProfileId,
        config: { ...networks, displayName: displayName.value } });
      jobId = result.jobId; cancel.hidden = false; status.show({ phase: 'queued' }); poll();
    } catch (error) { busy = false; save.disabled = false; showError(error); }
  });
  cancel.addEventListener('click', async () => { try { await client.request('/api/cancel', { jobId }); } catch (error) { showError(error); } });
  (async () => {
    try {
      await client.init(); const result = await client.request('/api/config'); revision = result.revision;
      displayName.value = result.config.displayName || ''; wifi.mount(document.getElementById('networks'), result.config, { maximumProfiles: 1 });
      status.show({ phase: 'idle' });
      if (result.readOnly) { save.disabled = true; status.show({ phase: 'failed', message: '保存領域を読み出せません。USBで状態を確認してください。現在の設定は上書きしません。' }); return; }
      const current = await client.request('/api/status');
      if (!['idle','failed','cancelled'].includes(current.phase)) { busy = true; save.disabled = true; cancel.hidden = false; poll(); }
    } catch (error) { save.disabled = true; showError(error); }
  })();
})();
