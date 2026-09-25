import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { join } from 'node:path';
import { test } from 'node:test';

const uiDirectory = process.cwd();
const requireFromUi = createRequire(join(uiDirectory, 'package.json'));
const { JSDOM } = requireFromUi('jsdom') as typeof import('jsdom');
const { build } = requireFromUi('esbuild') as typeof import('esbuild');

const fixture = `
  import React, { useState } from 'react';
  import ReactDOM from 'react-dom/client';
  import { KnobControl } from './src/components/KnobControl';
  function Demo() {
    const [value, setValue] = useState(0.5);
    return <KnobControl label="In" value={value} size={64} defaultValue={0.5}
      onChange={(next) => { window.changedValues.push(next); setValue(next); }} />;
  }
  window.changedValues = [];
  ReactDOM.createRoot(document.getElementById('root')).render(<Demo />);
`;

async function waitFor(check: () => boolean) {
  for (let i = 0; i < 50; i++) {
    if (check()) return;
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  assert.fail('knob did not update');
}

test('Artemis knob follows touch movement after WebKit cancels pointer events', async () => {
  const result = await build({
    stdin: {
      contents: fixture,
      resolveDir: uiDirectory,
      sourcefile: 'knob-fixture.tsx',
      loader: 'tsx',
    },
    bundle: true,
    write: false,
    platform: 'browser',
    format: 'iife',
    jsx: 'automatic',
    define: { 'process.env.NODE_ENV': '"production"' },
    logLevel: 'silent',
  });
  const dom = new JSDOM('<!doctype html><html><body><div id="root"></div></body></html>', {
    url: 'https://juce.backend/',
    runScripts: 'outside-only',
    pretendToBeVisual: true,
  });
  try {
    const { window } = dom;
    const { document } = window;
    Object.defineProperty(document.documentElement, 'clientWidth', { value: 1560 });
    Object.defineProperty(document.documentElement, 'clientHeight', { value: 720 });
    window.matchMedia = () => ({ matches: true }) as MediaQueryList;
    (window as unknown as { __T3K_PLATFORM__: string }).__T3K_PLATFORM__ = 'artemis';
    window.eval(result.outputFiles[0].text);
    await waitFor(() => Boolean(document.querySelector('.knob')));
    await new Promise((resolve) => setTimeout(resolve, 30)); // React's mount effect installs listeners.
    const knob = document.querySelector<HTMLElement>('.knob')!;
    const values = (window as unknown as { changedValues: number[] }).changedValues;

    const pointer = (type: string, y: number, pointerType = 'touch') => {
      const event = new window.MouseEvent(type, {
        bubbles: true,
        cancelable: true,
        clientX: 100,
        clientY: y,
        button: 0,
      });
      Object.defineProperties(event, {
        pointerId: { value: 1 },
        pointerType: { value: pointerType },
      });
      return event;
    };
    const touch = (type: string, y: number) => {
      const point = { identifier: 1, clientX: 100, clientY: y };
      const event = new window.Event(type, { bubbles: true, cancelable: true });
      Object.defineProperties(event, {
        touches: { value: type === 'touchend' ? [] : [point] },
        changedTouches: { value: [point] },
      });
      return event;
    };

    knob.dispatchEvent(pointer('pointerdown', 200));
    assert.equal(
      knob.dispatchEvent(touch('touchstart', 200)),
      false,
      'touch is kept out of page scrolling'
    );
    knob.dispatchEvent(pointer('pointerdown', 250, 'mouse')); // WebKit may synthesize this from touch.
    document.dispatchEvent(pointer('pointercancel', 200));
    window.dispatchEvent(touch('touchmove', 160));
    await waitFor(() => values.length > 0);
    assert.ok(values[0] > 0.5 && values[0] < 0.75, 'touch drag increases gain once');
    document.dispatchEvent(touch('touchend', 160));
    const count = values.length;
    window.dispatchEvent(touch('touchmove', 120));
    assert.equal(values.length, count, 'release stops changing the value');

    await waitFor(() => Number(knob.getAttribute('aria-valuenow')) > 0.5);
    knob.dispatchEvent(pointer('pointerdown', 200, 'mouse'));
    window.dispatchEvent(pointer('pointermove', 160, 'mouse'));
    await waitFor(() => values.length > count);
    assert.ok(values.at(-1)! > values[0], 'mouse drag still changes the value');
    document.dispatchEvent(pointer('pointerup', 160, 'mouse'));

    for (let tap = 0; tap < 2; tap++) {
      knob.dispatchEvent(pointer('pointerdown', 200));
      knob.dispatchEvent(touch('touchstart', 200));
      document.dispatchEvent(touch('touchend', 200));
      document.dispatchEvent(pointer('pointerup', 200));
    }
    await waitFor(() => values.at(-1) === 0.5);
    assert.equal(values.at(-1), 0.5, 'touch double tap still resets the knob');

    const afterReset = values.length;
    knob.dispatchEvent(touch('touchstart', 200));
    window.dispatchEvent(touch('touchmove', 160));
    await waitFor(() => values.length > afterReset);
    assert.ok(values.at(-1)! > 0.5, 'touch drag also works without PointerEvents');
    document.dispatchEvent(touch('touchend', 160));
  } finally {
    dom.window.close();
  }
});
