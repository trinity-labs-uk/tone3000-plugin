/** Preserve JUCE's custom scheme: URL.origin is "null" in some WebViews. */
export function redirectUriFromHref(href: string): string {
  const url = new URL(href);
  return `${url.protocol}//${url.host}${url.pathname}`;
}
