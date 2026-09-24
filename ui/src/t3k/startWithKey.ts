/** Make a missing OAuth client ID lead straight to the editable key field. */
export function startWithPublishableKey(
  key: string,
  clearError: () => void,
  openKeySettings: () => void,
  start: () => void
): void {
  if (key) {
    start();
    return;
  }
  clearError();
  openKeySettings();
}
