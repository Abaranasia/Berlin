# Berlin School Sequence Conventions — Research Notes

Research conducted 2026-09-06 ahead of roadmap Phase 10 (Generation/Randomize), to ground the
semi-random sequence generator in actual Berlin School compositional technique rather than
generic uniform randomization. Sources are cited per finding; general genre background not tied
to a specific quote is marked as such.

## Sources consulted

1. http://www.filiprooms.be/index.php?id=berlin-school — a personal bookmark/link page (not
   itself a technical article), containing one directly quotable technique and links to further
   resources.
2. https://www.ucapps.de/midibox_seq_manual_tut4.html — MIDIbox SEQ V4 tutorial, "Berlin School"
   step, walking through building a Berlin-School-style pattern on real hardware.
3. https://blog.chriswirsig.com/2016/05/12/step-sequencing-like-klaus-schulze-the-easy-way/ —
   a producer's account of building a Klaus Schulze-style "ever evolving" sequence, linked from
   source 1.

Two other links from source 1 (reasonexperts.com, blog.zzounds.com) and one further link
(matrixsynth.com, referencing a biodiode.com tutorial) were attempted but returned no usable
technical content (connection errors, 403, or a promotional page pointing to a paywalled/external
video) — not included below.

## Finding 1 — Delay timing is treated as a compositional parameter, not just an effect

> "A dotted eighth delay in an echo is half of the job... As time setting in your delay, then you
> get that typical Berlin-School feeling when you have a 16 steps sequence." — T. Coppens, quoted
> on source 1.

A dotted-eighth (3/16 note) delay time against a 16-step sequence deliberately creates a
delay-tap pattern that does NOT line up with the underlying step grid — the echoes fall
*between* steps, thickening the rhythm without the generator itself needing to place more notes.
This is a mixing/FX-stage technique, not a note-generation technique — it operates on
`SynthEffects`' delay downstream of whatever `Sequence` is generated, so it doesn't change what
`PitchGenerator`/`RhythmGenerator` need to do, but it is worth noting as a *separate* lever
(delay time relative to step length) that already exists in this codebase (Phase 8's
`SynthEffects` delay) and could be tuned/exposed later for authenticity.

## Finding 2 — Scale constraint + a slowly-cycling root/transposer track (source 2)

The MIDIbox tutorial builds the sequence around:
- **Force-to-Scale**, specifically **Harmonic Minor**, applied to the melodic track so every
  generated note conforms to the active scale.
- A **separate, slow transpose track** (their "G4T4") that plays root notes on a very long
  divider (256 = whole notes, over 32 steps) and **cycles through related minor keys — D Minor,
  G Minor, F Minor** — determining what "Force-to-Scale" resolves to at any given time. The
  melodic content doesn't change generation logic; the *root it's interpreted against* drifts
  slowly underneath it.
- Rhythmic subdivisions are described via MIDIbox "divider" values: 32 = eighth notes, 64 =
  quarter notes, 256 = whole notes — i.e. the transpose/root layer moves far slower than the
  melodic layer.

**Implication for our codebase**: `Scale::minor(root)` already exists and is exactly this
constraint mechanism (harmonic minor specifically is not yet distinguished from natural minor in
`Scale` — worth checking whether that distinction exists). The "slowly drifting root" idea is a
real, separate feature (periodic transposition over time) — likely out of scope for this phase's
MVP (which explore scoped to Generate/Randomize/seed-lock only) but a strong candidate for a
later phase, and conceptually related to the Mutation Engine's "Transpose" idea from the
project's own proposal doc.

## Finding 3 — Generative techniques that turn few input notes into a denser pattern (source 2)

- **Echo/delay-as-generator**: "7 Repeats, 8d Delay" — entering just two notes produces sixteen
  notes across four measures through automated repetition. Distinct from Finding 1: here the
  repeat count and delay time are tuned specifically to *multiply note count*, not just add FX
  texture.
- **Step-repeat with interval stretching**: "Repeat value to 1, Interval (Itv.) to 7" — causes
  the last step to repeat, adding one new note per measure cycle, growing the pattern gradually
  over time rather than presenting a fixed density from the start.
- **Skip triggers**: a separate trigger layer marks specific steps (e.g. 4, 6, 8, 12, 14) to be
  skipped, reducing an even 16-step grid down to an odd 11 effective steps — a deliberate
  rhythmic-displacement technique, not randomness.
- **Groove/velocity layer**: a 6-step velocity-offset pattern (`0`, `-32`, `-50`, ...) applied
  independently of the note pattern, so the same notes feel rhythmically alive without changing
  pitch/gate content — this requires per-step velocity, which this project's `Step`/`StepEvent`
  model does not yet have (a known, already-logged limitation from Phase 8's archive report).

**Implication**: our current `RhythmGenerator`'s `density` parameter (probability an active step
exists) is a reasonable analogue to *density* but is uniform-random, not one of these more
structured techniques (gradual growth, skip-based displacement, independent groove layer). A
"more Berlin School-flavored" randomizer could bias toward one of these structured patterns
(e.g. a skip-mask or a growing-density-over-repeats approach) rather than pure per-step coin
flips — worth raising explicitly in the proposal as a design choice, not assumed silently.

## Finding 4 — Polymetric layering: multiple patterns of *different lengths*, phasing against each other (source 3)

This is the single most load-bearing, well-documented Berlin School / Klaus Schulze technique
found in this research, and it is corroborated by well-established genre history (Tangerine
Dream's *Phaedra*/*Rubycon*-era use of two sequencers with different step counts running
simultaneously, a widely documented technique independent of these sources).

> "Klaus Schulze is famous for using step sequencer patterns that, although being repetitive in
> their parts, don't really repeat over time, as they weave into each other more or less
> randomly and create new patterns over a song of, say 15 minutes." — source 3, citing Wikipedia.

The article's own practical technique: multiple MIDI tracks (they used **8 pattern tracks**) of
**different lengths** (their example: one 3-bar pattern, one 12-bar pattern), each **sparsely
populated with notes within the active chord/scale** (not filled on every step), all routed to
one instrument. Because the tracks have different lengths, their combined pattern only repeats
after the LCM of all the individual lengths — long enough (with well-chosen lengths) that a
15-minute song never audibly repeats, while each individual layer stays simple and genuinely
repetitive on its own.

**Implication — this is an architectural mismatch worth flagging explicitly, not silently
working around**: this project's current architecture is ONE `Sequence` (fixed length, e.g. 16
steps) played by ONE `SequencePlayer` into ONE monophonic `SynthEngine`/`SynthVoice` (a
deliberate Phase 8 decision — "matches the sequencer's single-pendingNote design", explicitly
scoped as monophonic-only with polyphony deferred to a future phase). True polymetric layering as
described here needs multiple concurrent sequences/voices, which this codebase does not have yet
and is out of scope for Phase 10 as currently framed (Generate/Randomize on the existing single
sequence). This finding should be surfaced to the user/proposal as a real tension: either (a) this
phase stays scoped to single-sequence regeneration and polymetric layering becomes an explicit
later phase once polyphony/multi-voice exists, or (b) the scope is deliberately expanded now. Not
a decision this research makes on its own.

## Summary of concrete, sourced technique candidates for a "Berlin School-flavored" generator

| Technique | Source | Currently supported? | Notes |
|---|---|---|---|
| Minor-scale (esp. harmonic minor) pitch constraint | 2 | Partially — `Scale::minor()` exists; harmonic-vs-natural distinction unconfirmed | Cheap to verify/extend |
| Slowly drifting root/transposition over time | 2 | No | Real feature, likely a later phase (relates to Mutation Engine's Transpose) |
| Structured density patterns (growth-over-repeats, skip-mask) vs. uniform random density | 2 | Partially — `density` exists but is uniform-random | Worth an explicit design choice, not silent |
| Delay/echo tuned as a note-multiplying device | 1, 2 | Partially — `SynthEffects` delay exists but isn't tuned/exposed for this purpose | FX-level, downstream of Sequence generation |
| Independent groove/velocity layer | 2 | No — no velocity field in `Step`/`StepEvent` yet | Blocked on a data-model gap already logged from Phase 8 |
| Polymetric multi-length layering | 3 (+ well-established genre history) | No — architecture is single-sequence, monophonic | Real architectural tension, not a small addition; flag explicitly, don't silently scope out |

This research should inform (not replace) the Phase 10 proposal's scoping conversation — in
particular, whether "semi-random, Berlin-School-oriented" means (a) biasing the *existing*
single-sequence randomizer toward more musically structured density/scale choices, or (b) a
larger architectural step toward multi-layer/polymetric generation. Recommend surfacing this
choice explicitly before design, per the same "settle ambiguous semantics before design" practice
used in the exploration for Generate/Randomize/New Seed/Lock Seed.
