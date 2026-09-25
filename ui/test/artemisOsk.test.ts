import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { join } from 'node:path';
import { test } from 'node:test';

const uiDirectory = process.cwd();
const requireFromUi = createRequire(join(uiDirectory, 'package.json'));
const { JSDOM } = requireFromUi('jsdom') as typeof import('jsdom');

// This is the exact IIFE that JUCE embeds and injects at document start.
// Build it here so npm test catches missing shared-package imports too.
execFileSync('npm', ['run', 'build:artemis-osk'], { cwd: uiDirectory, stdio: 'pipe' });
const webviewDirectory = join(uiDirectory, '..', 'plugin', 'webview');
const script = readFileSync(join(webviewDirectory, 'artemis-osk.js'), 'utf8');
const css = readFileSync(join(webviewDirectory, 'artemis-osk.css'), 'utf8');

async function waitFor(check: () => boolean) {
  for (let i = 0; i < 40; i++) {
    if (check()) return;
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  assert.fail('keyboard did not become ready');
}

for (const [name, url, rootSize] of [
  ['bundled plugin UI', 'https://juce.backend/', '1.2px'],
  ['remote OAuth page', 'https://tone3000.com/login', '16px'],
] as const) {
  test(`Artemis keyboard accepts login text in ${name}`, async () => {
    const dom = new JSDOM(
      '<!doctype html><html><head></head><body><input type="email"><input type="password"><input type="number"></body></html>',
      {
        url,
        runScripts: 'outside-only',
        pretendToBeVisual: true,
      }
    );
    try {
      const { document } = dom.window;
      document.documentElement.style.fontSize = rootSize;
      const style = document.createElement('style');
      style.textContent = css;
      document.head.appendChild(style);
      dom.window.eval(script);
      await waitFor(() => Boolean(document.querySelector('.osk-root')));
      await waitFor(
        () => document.documentElement.style.getPropertyValue('--osk-safe-area') === '0px'
      );

      const expectedScale = String(16 / parseFloat(rootSize));
      assert.equal(
        document.documentElement.style.getPropertyValue('--osk-page-scale'),
        expectedScale
      );
      assert.equal(document.querySelectorAll('#tone3000-onscreen-keyboard').length, 1);

      for (const type of ['email', 'password']) {
        const input = document.querySelector<HTMLInputElement>(`input[type=${type}]`)!;
        const seen: string[] = [];
        input.addEventListener('input', () => seen.push(input.value));
        input.focus();
        await waitFor(() => Boolean(document.querySelector('.osk-root--visible')));
        const key = [...document.querySelectorAll<HTMLButtonElement>('.osk-key')].find(
          (button) => button.textContent === 'A'
        );
        assert.ok(key, 'alphabet key is present');
        key.click();
        await waitFor(() => input.value === 'A');
        assert.deepEqual(seen, ['A'], `${type} receives a real input event`);
      }

      const number = document.querySelector<HTMLInputElement>('input[type=number]')!;
      number.focus();
      await waitFor(() => Boolean(document.querySelector('.osk-root--visible')));
      const numbersButton = [...document.querySelectorAll<HTMLButtonElement>('.osk-key')].find(
        (button) => button.textContent === '123'
      );
      assert.ok(numbersButton);
      numbersButton.click();
      await waitFor(() =>
        Boolean(
          [...document.querySelectorAll<HTMLButtonElement>('.osk-key')].find(
            (button) => button.textContent === '1'
          )
        )
      );
      const one = [...document.querySelectorAll<HTMLButtonElement>('.osk-key')].find(
        (button) => button.textContent === '1'
      )!;
      one.click();
      await waitFor(() => number.value === '1');
    } finally {
      dom.window.close();
    }
  });
}
