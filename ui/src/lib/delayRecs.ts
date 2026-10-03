// Delay recommendations (legacy formatDelayRecommendations): one beat is 60000 / bpm ms, times each factor.
const DIVISIONS = [
  { label: '1/2', factor: 2 },
  { label: '1/4', factor: 1 },
  { label: '1/8.', factor: 0.75 },
  { label: '1/8', factor: 0.5 },
  { label: '1/8T', factor: 1 / 3 },
  { label: '1/16', factor: 0.25 },
] as const;

export interface DelayRecommendation {
  label: string;
  ms: number;
}

export const delayRecommendations = (bpm: number): DelayRecommendation[] =>
  DIVISIONS.map(({ label, factor }) => ({ label, ms: Math.round((60000 / bpm) * factor) }));

export const formatDelayRecommendations = (bpm: number): string =>
  delayRecommendations(bpm)
    .map(({ label, ms }) => `${label} ${ms} ms`)
    .join(' | '); // Source/core/TempoSync.cpp formatDelayRecommendations
