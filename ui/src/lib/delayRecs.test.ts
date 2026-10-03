import { describe, it, expect } from 'vitest';
import { delayRecommendations, formatDelayRecommendations } from './delayRecs';

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

describe('formatDelayRecommendations', () => {
  it('matches Source/core/TempoSync.cpp: "<label> <ms> ms" joined with " | "', () => {
    expect(formatDelayRecommendations(120)).toBe('1/2 1000 ms | 1/4 500 ms | 1/8. 375 ms | 1/8 250 ms | 1/8T 167 ms | 1/16 125 ms');
    expect(formatDelayRecommendations(150)).toBe('1/2 800 ms | 1/4 400 ms | 1/8. 300 ms | 1/8 200 ms | 1/8T 133 ms | 1/16 100 ms');
  });
});
