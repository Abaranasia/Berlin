// The seed is an int64 carried as a decimal string (it exceeds the JS safe-integer range), so it is
// validated textually and never passed through Number (spec "Seed Validation").
export const SEED_ERROR = 'Seed must be a whole number.';

export const isValidSeed = (text: string): boolean => /^-?\d+$/.test(text);
