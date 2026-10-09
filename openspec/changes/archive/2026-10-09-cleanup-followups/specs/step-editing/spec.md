# Delta for Step Editing

Verification legend: [V] Vitest, [T] BerlinTests, [M] manual, [B] build.

## MODIFIED Requirements

### Requirement: Keep-Position Edit Playback

A step edit MUST NOT reset the transport or playhead. If the edited step is the one currently sounding and its note or gate changed, the sounding note MUST receive a note-off at adoption. Otherwise no note-off MUST be sent. An edit MUST be rejected with `busy` while a publish is pending or while a state restore is pending; in both cases the busy check comes after index and note validation.
(Previously: busy covered only a pending publish; a pending state restore was not considered.)

#### Scenario: Playhead continues after edit [T]
- GIVEN playback is at step 9
- WHEN step 3 is edited and adopted
- THEN the playhead continues from step 9 and loop count is unchanged

#### Scenario: Editing the sounding step sends note-off [T]
- GIVEN step 5 is sounding
- WHEN step 5 is muted or its note changed, then adopted
- THEN a note-off for the old note is emitted

#### Scenario: Editing another step sends no note-off [T]
- GIVEN step 5 is sounding
- WHEN step 3 is edited and adopted
- THEN no note-off is emitted for the sounding note

#### Scenario: Busy rejects the edit [T]
- GIVEN a publish is pending
- WHEN a valid step edit is made
- THEN it fails with `busy` and state is unchanged

#### Scenario: Pending restore rejects a valid edit [T]
- GIVEN a state restore is pending and no publish is pending
- WHEN a valid step edit is made
- THEN it fails with `busy`, the sequence and `edited` are unchanged
- AND once the restore has published, the same edit succeeds

#### Scenario: Validation precedes busy [T]
- GIVEN a publish is pending, or a state restore is pending
- WHEN an edit with an out-of-range index or an out-of-scale note is made
- THEN it fails with `step-out-of-range` or `note-out-of-scale` (checked in that order), not `busy`, so the UI only retries edits that can succeed
