# Delta for Preset Persistence

## ADDED Requirements

### Requirement: Preset Listing Caches Only Per-File Validity

`PresetManager::listPresetNames()` MUST enumerate the preset directory on every call. The per-file parse/validity result MUST be cached inside `PresetManager`, keyed by the file's full path + size + last-modified time, and it MUST NOT cache anything else (in particular not the name list). Entries for files no longer present MUST be pruned on every call. The returned list MUST equal the list from a fresh `PresetManager` on the same directory, and a file whose key is unchanged MUST NOT be re-parsed. `PresetManager` MUST expose `std::size_t parseCount() const` (total preset files parsed by listing so far) as the test seam. `save()` MUST NOT need explicit cache invalidation.

#### Scenario: Cached result equals uncached scan
- GIVEN a directory with valid and invalid preset files
- WHEN `listPresetNames()` is called twice
- THEN both results equal the result of a fresh `PresetManager` on the same directory

#### Scenario: Unchanged file is not re-parsed
- GIVEN a first listing has parsed every file
- WHEN `listPresetNames()` is called again with no file changed
- THEN `parseCount()` is unchanged

#### Scenario: Changed file is re-parsed
- GIVEN "A" was listed as valid
- WHEN "A" is overwritten with invalid content (size or mtime changes) and listed again
- THEN `parseCount()` increases by exactly 1 and "A" is no longer listed

#### Scenario: Added and removed files are reflected
- GIVEN a cached listing
- WHEN a valid file is added and another is deleted externally
- THEN the next call lists the added file and omits the deleted one

#### Scenario: Slider drags do not re-parse presets (MANUAL)
- GIVEN the web UI is open with several presets
- WHEN a slider is dragged continuously
- THEN no preset file is re-parsed per command
