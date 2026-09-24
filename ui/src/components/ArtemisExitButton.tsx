import React from 'react';

/** Stays in the left screen margin even when the design box is letterboxed. */
export const ArtemisExitButton: React.FC<{ onExit: () => void }> = ({ onExit }) => (
  <button
    type="button"
    aria-label="Back to Launchpad"
    onClick={onExit}
    style={{
      position: 'fixed',
      left: 12,
      top: 12,
      minWidth: 132,
      minHeight: 48,
      border: '1px solid #606060',
      borderRadius: 10,
      background: '#171717',
      color: '#fff',
      fontSize: 15,
      fontWeight: 600,
      cursor: 'pointer',
      zIndex: 5000,
    }}
  >
    ← Launchpad
  </button>
);
