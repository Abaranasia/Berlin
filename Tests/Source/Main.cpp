/*
  ==============================================================================

   Berlin test runner — adapted from JUCE/extras/UnitTestRunner/Source/Main.cpp.

   Berlin.jucer and Tests/BerlinTests.jucer both set addUsingNamespaceToJuceHeader="0",
   so every JUCE symbol below is explicitly juce::-qualified (the stock JUCE
   UnitTestRunner relies on the implicit "using namespace juce;" and will not
   compile as-is under this project's settings).

   This target depends on juce_core only, so <JuceHeader.h> (which is generated
   per-project and would pull in whatever modules the *other* project uses) is
   replaced with a direct module include, and DeletedAtShutdown::deleteAll()
   (declared in juce_events, not linked here) is dropped from the exit guard.

  ==============================================================================
*/

#include <juce_core/juce_core.h>

#include <cstdlib>
#include <functional>
#include <iostream>
#include <vector>

//==============================================================================
class ConsoleLogger final : public juce::Logger
{
    void logMessage (const juce::String& message) override
    {
        std::cout << message << std::endl;

       #if JUCE_WINDOWS
        juce::Logger::outputDebugString (message);
       #endif
    }
};

//==============================================================================
class ConsoleUnitTestRunner final : public juce::UnitTestRunner
{
    void logMessage (const juce::String& message) override
    {
        juce::Logger::writeToLog (message);
    }
};

//==============================================================================
// UnitTestRunner::runTests()/runTestsInCategory()/runTestsWithName() each call
// results.clear() internally, so any results from an earlier filtered run are
// gone by the time the next one starts. Harvest failures immediately after
// each individual run, before the next call wipes them.
static void appendFailures (const juce::UnitTestRunner& runner, std::vector<juce::String>& failures)
{
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        auto* result = runner.getResult (i);

        if (result->failures > 0)
        {
            const auto testName = result->unitTestName + " / " + result->subcategoryName;
            const auto testSummary = juce::String (result->failures) + " test failure" + (result->failures > 1 ? "s" : "");
            const auto newLineAndTab = juce::newLine + "\t";

            failures.push_back (testName + ": " + testSummary + newLineAndTab
                                + result->messages.joinIntoString (newLineAndTab));
        }
    }
}

//==============================================================================
int main (int argc, char** argv)
{
    constexpr auto helpOption = "--help|-h";
    constexpr auto listOption = "--list-categories|-l";
    constexpr auto categoryOption = "--category|-c";
    constexpr auto seedOption = "--seed|-s";
    constexpr auto nameOption = "--name|-n";

    juce::ArgumentList args (argc, argv);

    if (args.containsOption (helpOption))
    {
        std::cout << argv[0]
                  << " [" << helpOption << "]"
                  << " [" << listOption << "]"
                  << " [" << categoryOption << "=category]"
                  << " [" << seedOption << "=seed]"
                  << " [" << nameOption << "=name]"
                  << std::endl;
        return 0;
    }

    if (args.containsOption (listOption))
    {
        for (auto& category : juce::UnitTest::getAllCategories())
            std::cout << category << std::endl;

        return 0;
    }

    ConsoleLogger logger;
    juce::Logger::setCurrentLogger (&logger);

    const juce::ScopeGuard onExit { [&]
    {
        juce::Logger::setCurrentLogger (nullptr);
    }};

    ConsoleUnitTestRunner runner;

    const juce::int64 seed = std::invoke ([&]() -> juce::int64
    {
        if (args.containsOption (seedOption))
        {
            auto seedValueString = args.getValueForOption (seedOption);

            if (seedValueString.startsWith ("0x"))
            {
                if (seedValueString.substring (2).containsOnly ("0123456789abcdefABCDEF"))
                    return seedValueString.getHexValue64();
            }
            else if (seedValueString.containsOnly ("0123456789"))
            {
                return seedValueString.getLargeIntValue();
            }

            std::cerr << "Invalid --seed value: " << seedValueString << std::endl;
            std::exit (2);
        }

        return juce::Random::getSystemRandom().nextInt64();
    });

    std::vector<juce::String> failures;

    if (args.containsOption (categoryOption) || args.containsOption (nameOption))
    {
        int totalResultsCollected = 0;

        while (args.containsOption (categoryOption))
        {
            runner.runTestsInCategory (args.removeValueForOption (categoryOption), seed);
            totalResultsCollected += runner.getNumResults();
            appendFailures (runner, failures);
        }

        while (args.containsOption (nameOption))
        {
            runner.runTestsWithName (args.removeValueForOption (nameOption), seed);
            totalResultsCollected += runner.getNumResults();
            appendFailures (runner, failures);
        }

        if (totalResultsCollected == 0)
        {
            logger.writeToLog ("No tests matched the given --category/--name filter(s).");
            return 1;
        }
    }
    else
    {
        runner.runAllTests (seed);
        appendFailures (runner, failures);
    }

    logger.writeToLog (juce::newLine + juce::String::repeatedString ("-", 65));

    if (! failures.empty())
    {
        logger.writeToLog ("Test failure summary:");

        for (const auto& failure : failures)
            logger.writeToLog (juce::newLine + failure);

        return 1;
    }

    logger.writeToLog ("All tests completed successfully");
    return 0;
}
