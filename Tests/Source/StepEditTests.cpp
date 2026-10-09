/*
  ==============================================================================

   StepEdit tests (step-editing spec, design.md D3): scaleNotesFor and
   validateStepEdit. Pure functions, no processor, no audio device.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include <algorithm>
#include <vector>

#include "core/Scale.h"
#include "core/Sequence.h"
#include "generation/GenerationParams.h"
#include "generation/StepEdit.h"

class StepEditTests final : public juce::UnitTest
{
public:
    StepEditTests() : juce::UnitTest ("StepEdit", "Berlin") {}

    void runTest() override
    {
        using berlin::GenerationParams;
        using berlin::ScaleType;
        using berlin::StepEditError;

        beginTest ("scaleNotesFor: C minor 36..72 gives the exact ascending list");
        {
            GenerationParams params;   // defaults: minor, root C, 36..72
            const std::vector<int> expected { 36, 38, 39, 41, 43, 44, 46, 48, 50, 51, 53, 55, 56, 58,
                                              60, 62, 63, 65, 67, 68, 70, 72 };
            expect (berlin::scaleNotesFor (params) == expected);
        }

        beginTest ("scaleNotesFor: major, root 0, 48..60 gives [48,50,52,53,55,57,59,60]");
        {
            GenerationParams params;
            params.scaleType = ScaleType::major;
            params.rangeLow = 48;
            params.rangeHigh = 60;
            const std::vector<int> expected { 48, 50, 52, 53, 55, 57, 59, 60 };
            expect (berlin::scaleNotesFor (params) == expected);
        }

        beginTest ("scaleNotesFor: root and scale changes move the list; D Dorian 40..64 stays in scale and range");
        {
            GenerationParams params;
            params.scaleType = ScaleType::dorian;
            params.rootPitchClass = 2;
            params.rangeLow = 40;
            params.rangeHigh = 64;

            const auto notes = berlin::scaleNotesFor (params);
            const auto scale = berlin::Scale::fromPitchClass (ScaleType::dorian, 2);

            expect (! notes.empty());
            expect (std::is_sorted (notes.begin(), notes.end()));
            expect (std::all_of (notes.begin(), notes.end(), [&] (int n) { return scale.contains (n) && n >= 40 && n <= 64; }));
            expect (std::count (notes.begin(), notes.end(), 41) == 1);   // F is in D Dorian
            expect (std::count (notes.begin(), notes.end(), 42) == 0);   // F# is not
            expect (berlin::scaleNotesFor (GenerationParams {}) != notes);
        }

        beginTest ("scaleNotesFor: inverted and narrow ranges are normalized like generation; never empty");
        {
            GenerationParams inverted;
            inverted.rangeLow = 72;
            inverted.rangeHigh = 36;
            expect (berlin::scaleNotesFor (inverted) == berlin::scaleNotesFor (GenerationParams {}));

            GenerationParams narrow;
            narrow.rangeLow = 60;
            narrow.rangeHigh = 62;   // span 2 -> widened upward to 60..72
            const auto notes = berlin::scaleNotesFor (narrow);
            expect (! notes.empty());
            expectEquals (notes.front(), 60);
            expectEquals (notes.back(), 72);
        }

        beginTest ("validateStepEdit: index limits (negative, >=16, >=size) are stepOutOfRange; 15 on 16 steps is none");
        {
            const std::vector<int> scaleNotes { 48, 50, 51, 53 };
            const berlin::Sequence sixteen (16), thirtyTwo (32), eight (8);

            static_assert (noexcept (berlin::validateStepEdit (sixteen, scaleNotes, 0, 48)));

            expect (berlin::validateStepEdit (sixteen, scaleNotes, -1, 48) == StepEditError::stepOutOfRange);
            expect (berlin::validateStepEdit (thirtyTwo, scaleNotes, 16, 48) == StepEditError::stepOutOfRange);
            expect (berlin::validateStepEdit (eight, scaleNotes, 8, 48) == StepEditError::stepOutOfRange);
            expect (berlin::validateStepEdit (sixteen, scaleNotes, 15, 48) == StepEditError::none);
            expect (berlin::validateStepEdit (thirtyTwo, scaleNotes, 15, 48) == StepEditError::none);
        }

        beginTest ("validateStepEdit: out-of-scale and out-of-range notes are noteOutOfScale; index is checked before the note");
        {
            const std::vector<int> scaleNotes { 48, 50, 51, 53 };
            const berlin::Sequence sequence (16);

            expect (berlin::validateStepEdit (sequence, scaleNotes, 3, 49) == StepEditError::noteOutOfScale);
            expect (berlin::validateStepEdit (sequence, scaleNotes, 3, 100) == StepEditError::noteOutOfScale);
            expect (berlin::validateStepEdit (sequence, scaleNotes, 3, -5) == StepEditError::noteOutOfScale);
            expect (berlin::validateStepEdit (sequence, scaleNotes, 16, 49) == StepEditError::stepOutOfRange);
        }

        beginTest ("validateStepEdit: a muted step's note stays a valid in-scale note (gate is not validated)");
        {
            const std::vector<int> scaleNotes { 48, 50 };
            berlin::Sequence sequence (16);
            sequence[3] = { 48, false };

            expect (berlin::validateStepEdit (sequence, scaleNotes, 3, 48) == StepEditError::none);
        }
    }
};

static StepEditTests stepEditTests;
