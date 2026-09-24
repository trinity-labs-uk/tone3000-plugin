/** The OAuth client ID is public. A device owner may supply one at runtime. */
export const RUNTIME_KEY_STORAGE = 't3k.publishableKey';

export function normalizePublishableKey(value: string): string {
  const key = value.trim();
  if (key && !/^t3k_pub_[A-Za-z0-9_-]+$/.test(key)) {
    throw new Error('Enter a TONE3000 publishable key beginning with t3k_pub_.');
  }
  return key;
}

export function resolvePublishableKey(buildKey: string, storage: Pick<Storage, 'getItem'>): string {
  const saved = storage.getItem(RUNTIME_KEY_STORAGE);
  return saved ? normalizePublishableKey(saved) : buildKey;
}

export function persistPublishableKey(value: string, storage: Pick<Storage, 'setItem' | 'removeItem'>): string {
  const key = normalizePublishableKey(value);
  if (key) storage.setItem(RUNTIME_KEY_STORAGE, key);
  else storage.removeItem(RUNTIME_KEY_STORAGE);
  return key;
}
