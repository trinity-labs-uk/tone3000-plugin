/** Fit the full design box into the actual WebView viewport. */
export function fitDesignScale(
  viewportWidth: number,
  viewportHeight: number,
  designWidth: number,
  designHeight: number
): number {
  return Math.max(0.05, Math.min(viewportWidth / designWidth, viewportHeight / designHeight));
}
