export type CopyText = (text: string) => Promise<void>;

// A missing clipboard API (or a denied write) rejects; there is deliberately no execCommand fallback.
export const copyText: CopyText = (text) => Promise.resolve().then(() => navigator.clipboard.writeText(text));
