import test from 'node:test';
import assert from 'node:assert/strict';
import React from 'react';
import { renderToStaticMarkup } from 'react-dom/server';

import { ArtemisExitButton } from '../src/components/ArtemisExitButton.tsx';
import { fitDesignScale } from '../src/hooks/uiScaleMath.ts';
import { redirectUriFromHref } from '../src/t3k/redirectUri.ts';
import { startWithPublishableKey } from '../src/t3k/startWithKey.ts';
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
    setItem: (name: string, value: string) => {
      values.set(name, value);
    },
    removeItem: (name: string) => {
      values.delete(name);
    },
  };
  assert.equal(resolvePublishableKey('', storage), '');
  assert.equal(persistPublishableKey(' t3k_pub_device123 ', storage), 't3k_pub_device123');
  assert.equal(values.get(RUNTIME_KEY_STORAGE), 't3k_pub_device123');
  assert.equal(resolvePublishableKey('t3k_pub_build123', storage), 't3k_pub_device123');
  assert.equal(persistPublishableKey('', storage), '');
  assert.equal(resolvePublishableKey('t3k_pub_build123', storage), 't3k_pub_build123');
  assert.throws(() => normalizePublishableKey('private_secret'), /publishable key/);
});

test('OAuth redirect preserves the Linux JUCE scheme and strips callback parameters', () => {
  assert.equal(
    redirectUriFromHref('juce://juce.backend/index.html?code=abc&state=xyz#return'),
    'juce://juce.backend/index.html'
  );
  assert.equal(
    redirectUriFromHref('https://juce.backend/index.html?code=abc'),
    'https://juce.backend/index.html'
  );
});

test('kiosk exit control is named and touch sized', () => {
  const html = renderToStaticMarkup(React.createElement(ArtemisExitButton, { onExit: () => {} }));
  assert.match(html, /aria-label="Back to Launchpad"/);
  assert.match(html, /min-height:48px/);
});

test('sign-in without a key clears the blocking overlay and opens the key field', () => {
  const events: string[] = [];
  const clear = () => events.push('clear overlay');
  const settings = () => events.push('open key settings');
  const login = () => events.push('start login');
  startWithPublishableKey('', clear, settings, login);
  assert.deepEqual(events, ['clear overlay', 'open key settings']);
  events.length = 0;
  startWithPublishableKey('t3k_pub_owner', clear, settings, login);
  assert.deepEqual(events, ['start login']);
});
