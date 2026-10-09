# Delta for Internal Synth Output

## ADDED Requirements

### Requirement: Live-Adjustable Delay And Reverb Parameters

The system MUST let the user change delay time, delay feedback, delay mix, reverb room size, reverb damping, reverb wet level, and reverb dry level at runtime, with the new value taking audible effect within one block and no dropout, click, or glitch at the moment of change. A parameter change MUST NOT allocate heap memory, acquire a lock, or call a logging/formatting function on the audio thread.

#### Scenario: Changing delay feedback takes effect without glitch
- GIVEN delay is enabled and a tail is audible
- WHEN the user changes delay feedback
- THEN the new feedback value is audible within one block, with no click or dropout

#### Scenario: Changing reverb room size takes effect without dropout
- GIVEN reverb is enabled and audible
- WHEN the user changes reverb room size
- THEN the new value is audible within one block, with no dropout

#### Scenario: Parameter change path introduces no allocation, lock, or logging call
- GIVEN any live delay/reverb setter added by this change
- WHEN its call path from UI action to audio-thread application is inspected during code review
- THEN no heap allocation, lock acquisition, or logging/formatting call is present on the audio-thread side

### Requirement: Delay Time Clamped To The Allocated Buffer Bound (Defensive Only)

The system MUST silently clamp any resolved delay time - whether entered manually in Free mode or loaded from an untrusted preset file - to `kMaxDelaySeconds` (3.0s, the delay line's allocated bound) rather than reject the change or resize the buffer at runtime. `kMaxDelaySeconds` MUST be large enough that every exposed sync division's division-derived seconds, at every legal BPM in [40, 240], stays within the bound without ever triggering the clamp - the clamp is a defensive bound for Free-mode entry and untrusted preset data, not a normal-path behavior for Sync mode. Any displayed delay-time value MUST reflect the clamped effective value, not the originally requested one.
(Previously (superseded): the bound was 2.0s, and the half-note division at BPM = 40 legitimately exceeded it, so clamping was routine, expected behavior in normal Sync-mode use, not merely defensive. Raised to 3.0s so every division is reachable at every legal BPM and the sync path never silently understates the requested time.)

#### Scenario: No sync-derived delay time ever reaches the clamp at any legal BPM
- GIVEN Sync mode and any of the 6 exposed divisions
- WHEN BPM is set to any value within [40, 240] and the division-derived delay time is computed
- THEN the result never exceeds `kMaxDelaySeconds` (3.0s) - the clamp is never triggered by normal Sync-mode use

#### Scenario: The longest division at the BPM floor lands exactly at the bound
- GIVEN Sync mode, BPM = 40 (the lowest legal value), and the half-note division selected
- WHEN the division-derived delay time is computed (`(60/40) x 2.0 = 3.0` seconds)
- THEN the effective delay time equals exactly `kMaxDelaySeconds` (3.0s) and is reachable without clamping

#### Scenario: Manual entry above the buffer bound is clamped (defensive path)
- GIVEN Free mode
- WHEN the user manually enters a delay time greater than 3.0 seconds
- THEN the effective delay time is clamped to 3.0 seconds and the control displays 3.0 seconds

#### Scenario: An out-of-range value from an untrusted preset file is clamped (defensive path)
- GIVEN a preset file whose stored delay time exceeds 3.0 seconds
- WHEN the preset is loaded
- THEN the effective delay time is clamped to 3.0 seconds, consistent with `preset-persistence`'s existing out-of-range clamping policy
