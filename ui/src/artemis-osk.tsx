import ReactDOM from 'react-dom/client';
import { OnScreenKeyboard } from '@artemis-os/on-screen-keyboard';

declare global {
  interface Window {
    __artemisSharedOnScreenKeyboard?: boolean;
  }
}

function mountKeyboard() {
  if (window.__artemisSharedOnScreenKeyboard) return;
  window.__artemisSharedOnScreenKeyboard = true;

  const host = document.createElement('div');
  host.id = 'tone3000-onscreen-keyboard';
  document.documentElement.appendChild(host);
  ReactDOM.createRoot(host).render(<OnScreenKeyboard />);

  // TONE3000's own UI uses one CSS rem per design pixel. The shared keyboard
  // uses normal browser rems, so restore its intended physical size there.
  // OAuth pages generally use the browser's 16px root and need no correction.
  const syncScale = () => {
    const root = document.documentElement;
    const rootPixels = parseFloat(getComputedStyle(root).fontSize) || 16;
    const scale = String(16 / rootPixels);
    if (root.style.getPropertyValue('--osk-page-scale') !== scale)
      root.style.setProperty('--osk-page-scale', scale);
  };
  syncScale();
  new MutationObserver(syncScale).observe(document.documentElement, {
    attributes: true,
    attributeFilter: ['style', 'class'],
  });
}

if (document.documentElement) mountKeyboard();
else document.addEventListener('DOMContentLoaded', mountKeyboard, { once: true });
