# Delta for Internal Synth Voice

## Purpose Update (non-requirement text correction)

Current Purpose text ends: "...applies at the existing control-rate cadence via message-thread setters and lock-free atomics; no preset save/load yet."

Corrected Purpose text: "...applies at the existing control-rate cadence via message-thread setters and lock-free atomics. A preset system now exists (see `preset-persistence`), scoped to exactly these 11 live parameters plus the generation seed; the 8 non-live `SynthPatch` effects fields (delay/reverb/outputLevel) are not part of any preset and stay pinned to `kDefaultPatch`."

(Reason: the Purpose statement's "no preset save/load yet" claim is now false. This is a text-only correction — no Requirement in this spec changes behavior. Presets only invoke the same existing message-thread setters, at the same control-rate cadence, that already govern these 11 parameters.)

## MODIFIED Requirements

None. All existing Requirements (waveforms, filter, ADSR, LFO, monophonic voice, allocation-free rendering, silence-when-idle) are unaffected by the preset system.
