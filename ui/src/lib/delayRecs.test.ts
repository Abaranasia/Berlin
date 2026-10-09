import { describe, it, expect } from 'vitest';
import { delayRecommendations } from './delayRecs';

describe('delayRecommendations', () => {
  it('at 120 bpm gives 1000, 500, 375, 250, 167 and 125 ms with the legacy labels', () => {
    expect(delayRecommendations(120)).toEqual([
      { label: '1/2', ms: 1000 },
      { label: '1/4', ms: 500 },
      { label: '1/8.', ms: 375 },
      { label: '1/8', ms: 250 },
      { label: '1/8T', ms: 167 },
      { label: '1/16', ms: 125 },
    ]);
  });

  it('recomputes at another bpm (150: 1/4 is 400 ms)', () => {
    const at150 = delayRecommendations(150);
    expect(at150.find((r) => r.label === '1/4')!.ms).toBe(400);
    expect(at150.map((r) => r.ms)).toEqual([800, 400, 300, 200, 133, 100]);
  });

  it('rounds to whole milliseconds (1/8T at 100 bpm is 200)', () => {
    expect(delayRecommendations(100).find((r) => r.label === '1/8T')!.ms).toBe(200);
    expect(delayRecommendations(130).every((r) => Number.isInteger(r.ms))).toBe(true);
  });
});
