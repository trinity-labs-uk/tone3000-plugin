import test from 'node:test';
import assert from 'node:assert/strict';

import { fitDesignScale } from '../src/hooks/uiScaleMath.ts';
import {
  normalizePublishableKey,
  persistPublishableKey,
  resolvePublishableKey,
  RUNTIME_KEY_STORAGE,
} from '../src/t3k/publishableKey.ts';

test('1560x720 Artemis viewport contains the complete UI and chrome', () => {
  for (const designHeight of [578, 614, 738]) {
    const scale = fitDesignScale(1560, 720, 1024, designHeight);
    assert.ok(scale * 1024 <= 1560);
    assert.ok(scale * designHeight <= 720);
    assert.ok(scale >= 0.05);
  }
});

test('OAuth client ID is chosen on the device and can be reset', () => {
  const values = new Map<string, string>();
  const storage = {
    getItem: (name: string) => values.get(name) ?? null,
    setItem: (name: string, value: string) => { values.set(name, value); },
    removeItem: (name: string) => { values.delete(name); },
  };
  assert.equal(resolvePublishableKey('', storage), '');
  assert.equal(persistPublishableKey(' t3k_pub_device123 ', storage), 't3k_pub_device123');
  assert.equal(values.get(RUNTIME_KEY_STORAGE), 't3k_pub_device123');
  assert.equal(resolvePublishableKey('t3k_pub_build123', storage), 't3k_pub_device123');
  assert.equal(persistPublishableKey('', storage), '');
  assert.equal(resolvePublishableKey('t3k_pub_build123', storage), 't3k_pub_build123');
  assert.throws(() => normalizePublishableKey('private_secret'), /publishable key/);
});
