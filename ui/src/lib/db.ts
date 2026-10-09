// Output-level readout: 20*log10(level) with one decimal; a silent level has no finite dB value.
export function formatDb(level: number): string {
  if (!(level > 0)) return '−∞ dB';
  return `${(20 * Math.log10(level)).toFixed(1)} dB`;
}
