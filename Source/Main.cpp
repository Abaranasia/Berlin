/*
  ==============================================================================

    This file contains the basic startup code for a JUCE application.

  ==============================================================================
*/

#include <JuceHeader.h>
#include "MainComponent.h"

//==============================================================================
class BerlinApplication  : public juce::JUCEApplication
{
public:
    //==============================================================================
    BerlinApplication() {}

    const juce::String getApplicationName() override       { return ProjectInfo::projectName; }
    const juce::String getApplicationVersion() override    { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    //==============================================================================
    void initialise (const juce::String& commandLine) override
    {
        // This method is where you should put your application's initialisation code..

        mainWindow.reset (new MainWindow (getApplicationName()));
    }

    void shutdown() override
    {
        // Add your application's shutdown code here..

        mainWindow = nullptr; // (deletes our window)
    }

    //==============================================================================
    void systemRequestedQuit() override
    {
        // This is called when the app is being asked to quit: you can ignore this
        // request and let the app carry on running, or call quit() to allow the app to close.
        quit();
    }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        // When another instance of the app is launched while this one is running,
        // this method is invoked, and the commandLine parameter tells you what
        // the other instance's command-line arguments were.
    }

    //==============================================================================
    /*
        This class implements the desktop window that contains an instance of
        our MainComponent class.
    */
    class MainWindow    : public juce::DocumentWindow
    {
    public:
        MainWindow (juce::String name)
            : DocumentWindow (name,
                              juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                          .findColour (juce::ResizableWindow::backgroundColourId),
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            auto* content = new MainComponent();
            setContentOwned (content, true);

           #if JUCE_IOS || JUCE_ANDROID
            setFullScreen (true);
           #else
            // The WebEditor is always resizable within its own limits.
            editorLimits = &content->getEditorConstrainer();
            setResizable (true, false);
            applyEditorLimits();   // provisional: the native frame size is unknown until the window is shown

            centreWithSize (getWidth(), getHeight());
           #endif

            setVisible (true);

           #if ! (JUCE_IOS || JUCE_ANDROID)
            // The native frame (title bar, borders) exists only once the peer does. The resize
            // constrainer works on the whole window, so the limits must include the frame for the
            // CONTENT to stay within the editor's limits.
            applyEditorLimits();
           #endif
        }

        void closeButtonPressed() override
        {
            // This is called when the user tries to close this window. Here, we'll just
            // ask the app to quit when this happens, but you can change this to do
            // whatever you need.
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

        /* Note: Be careful if you override any DocumentWindow methods - the base
           class uses a lot of them, so by overriding you might break its functionality.
           It's best to do all your work in your content component instead, but if
           you really have to override any DocumentWindow methods, make sure your
           subclass also calls the superclass's method.
        */

    private:
        // Window limits = editor (content) limits + window frame (native title bar and borders).
        // Message thread only; the audio path is untouched.
        void applyEditorLimits()
        {
            auto frame = getContentComponentBorder();

            if (auto* peer = getPeer())
            {
                const auto native = peer->getFrameSize();
                frame = juce::BorderSize<int> (frame.getTop() + native.getTop(), frame.getLeft() + native.getLeft(),
                                               frame.getBottom() + native.getBottom(), frame.getRight() + native.getRight());
            }

            const auto extraW = frame.getLeftAndRight();
            const auto extraH = frame.getTopAndBottom();
            setResizeLimits (editorLimits->getMinimumWidth() + extraW, editorLimits->getMinimumHeight() + extraH,
                             editorLimits->getMaximumWidth() + extraW, editorLimits->getMaximumHeight() + extraH);
        }

        const juce::ComponentBoundsConstrainer* editorLimits = nullptr;   // set in the constructor; owned by the editor, which MainComponent owns

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (BerlinApplication)
