#include <JuceHeader.h>
#include <windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "Dbghelp.lib")
#include <dwmapi.h>
#pragma comment(lib, "Dwmapi.lib")
#include "AudioEngine.h"
#include "MainPanel.h"

static LONG WINAPI unhandledCrashFilter (EXCEPTION_POINTERS* ep)
{
    auto crashFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ToneForge_crash.log");
    juce::String msg = "CRASH OCCURRED!\n";
    if (ep && ep->ExceptionRecord)
    {
        msg += "Exception Code: 0x" + juce::String::toHexString (static_cast<int> (ep->ExceptionRecord->ExceptionCode)) + "\n";
        msg += "Address: " + juce::String::toHexString (reinterpret_cast<juce::pointer_sized_int> (ep->ExceptionRecord->ExceptionAddress)) + "\n";
    }
    crashFile.replaceWithText (msg);
    return EXCEPTION_CONTINUE_SEARCH;
}

class ToneForgeWindow final : public juce::DocumentWindow
{
public:
    ToneForgeWindow (ToneForgeAudio& engine, juce::AudioDeviceManager& devMgr, juce::ApplicationProperties& appProps)
        : DocumentWindow ("ToneForge - Guitar Tone Studio",
                          juce::Colour (0xff191a1c),
                          DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);

        auto appIcon = juce::ImageFileFormat::loadFrom (BinaryData::icon_png, BinaryData::icon_pngSize);
        if (appIcon.isValid())
            setIcon (appIcon);

        setContentOwned (new MainPanel (engine, devMgr, appProps), false);
        setResizable (true, true);
        setResizeLimits (960, 540, 3840, 2160);

#if JUCE_WINDOWS
        if (auto* peer = getPeer())
        {
            if (auto hwnd = reinterpret_cast<HWND> (peer->getNativeHandle()))
            {
                // Modern Windows dark mode title bar & border
                BOOL dark = TRUE;
                DwmSetWindowAttribute (hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof (dark));
                DwmSetWindowAttribute (hwnd, 19 /* DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 */, &dark, sizeof (dark));

                // Force native Win32 window background brush to studio dark (#191A1C)
                // This eliminates any white flashes or blank canvas during window creation or sizing
                static HBRUSH darkBrush = CreateSolidBrush (RGB (25, 26, 28));
                SetClassLongPtr (hwnd, GCLP_HBRBACKGROUND, reinterpret_cast<LONG_PTR> (darkBrush));

                auto hInst = reinterpret_cast<HINSTANCE> (juce::Process::getCurrentModuleInstanceHandle());
                if (auto hIconBig = reinterpret_cast<HICON> (LoadImage (hInst, TEXT ("IDI_ICON1"), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE)))
                    SendMessage (hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM> (hIconBig));

                if (auto hIconSmall = reinterpret_cast<HICON> (LoadImage (hInst, TEXT ("IDI_ICON1"), IMAGE_ICON,
                                                                         GetSystemMetrics (SM_CXSMICON),
                                                                         GetSystemMetrics (SM_CYSMICON), 0)))
                    SendMessage (hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM> (hIconSmall));
            }
        }
#endif
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToneForgeWindow)
};

class ToneForgeApplication final : public juce::JUCEApplication
{
public:
    ToneForgeApplication() {}

    const juce::String getApplicationName() override { return "ToneForge"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String&) override
    {
        SetUnhandledExceptionFilter (unhandledCrashFilter);

        // 1. Initialise Settings Storage
        juce::PropertiesFile::Options options;
        options.applicationName = "ToneForge";
        options.filenameSuffix = ".settings";
        options.folderName = "ToneForge";
        options.osxLibrarySubFolder = "Application Support";
        options.storageFormat = juce::PropertiesFile::storeAsXML;
        appProperties.setStorageParameters (options);

        // 2. Initialise Audio Device FIRST (fast, non-blocking registration + restore)
        initialiseAudio();

        // 3. Connect Audio Engine Callback
        audioDeviceManager.addAudioCallback (&audio);

        // 4. Create Main Window with loaded audio context
        window = std::make_unique<ToneForgeWindow> (audio, audioDeviceManager, appProperties);

        // 5. Calculate and apply geometry before showing
        auto* userSettings = appProperties.getUserSettings();
        bool restored = false;
        if (userSettings != nullptr)
        {
            juce::String savedState = userSettings->getValue ("windowState");
            if (savedState.isNotEmpty())
                restored = window->restoreWindowStateFromString (savedState);
        }

        if (! restored)
        {
            int targetW = 1280;
            int targetH = 740;
            if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            {
                targetW = juce::jlimit (1000, 1400, display->userArea.getWidth() - 60);
                targetH = juce::jlimit (560, 840, display->userArea.getHeight() - 60);
            }
            window->centreWithSize (targetW, targetH);
        }

        // 6. Present the fully-rendered dark UI immediately
        window->setVisible (true);
        window->toFront (true);

#if JUCE_WINDOWS
        if (auto* peer = window->getPeer())
        {
            if (auto hwnd = reinterpret_cast<HWND> (peer->getNativeHandle()))
            {
                ShowWindow (hwnd, SW_SHOW);
                UpdateWindow (hwnd);
                SetForegroundWindow (hwnd);
                BringWindowToTop (hwnd);
            }
        }
#endif

        saveAppSettings();
    }

    void shutdown() override
    {
        saveAppSettings();

        audioDeviceManager.removeAudioCallback (&audio);
        audioDeviceManager.closeAudioDevice();
        window.reset();
    }

    void systemRequestedQuit() override
    {
        saveAppSettings();
        quit();
    }

    void anotherInstanceStarted (const juce::String&) override
    {
        if (window != nullptr)
        {
            window->setVisible (true);
            window->toFront (true);
#if JUCE_WINDOWS
            if (auto* peer = window->getPeer())
            {
                if (auto hwnd = reinterpret_cast<HWND> (peer->getNativeHandle()))
                {
                    if (IsIconic (hwnd))
                        ShowWindow (hwnd, SW_RESTORE);
                    else
                        ShowWindow (hwnd, SW_SHOW);
                    SetForegroundWindow (hwnd);
                    BringWindowToTop (hwnd);
                }
            }
#endif
        }
    }

private:
    void initialiseAudio()
    {
        // Register available audio device types without running slow redundant hardware scans
        juce::OwnedArray<juce::AudioIODeviceType> types;
        audioDeviceManager.createAudioDeviceTypes (types);
        for (auto* t : types)
            audioDeviceManager.addAudioDeviceType (std::unique_ptr<juce::AudioIODeviceType> (t));
        types.clear (false);

        auto* userSettings = appProperties.getUserSettings();
        bool restored = false;

        if (userSettings != nullptr)
        {
            if (auto audioXml = userSettings->getXmlValue ("audioSetup"))
            {
                juce::String err = audioDeviceManager.initialise (2, 2, audioXml.get(), true);
                if (err.isEmpty() && audioDeviceManager.getCurrentAudioDevice() != nullptr)
                    restored = true;
            }
        }

        // If no saved settings or couldn't restore, AUTO-DETECT FOCUSRITE ASIO!
        if (! restored)
        {
            bool asioConfigured = false;
            for (auto* t : audioDeviceManager.getAvailableDeviceTypes())
            {
                if (t->getTypeName().equalsIgnoreCase ("ASIO"))
                {
                    audioDeviceManager.setCurrentAudioDeviceType ("ASIO", true);
                    t->scanForDevices();
                    auto inDevs = t->getDeviceNames (true);
                    juce::String chosenDevice;
                    for (const auto& dev : inDevs)
                    {
                        if (dev.containsIgnoreCase ("Focusrite"))
                        {
                            chosenDevice = dev;
                            break;
                        }
                    }
                    if (chosenDevice.isEmpty() && ! inDevs.isEmpty())
                        chosenDevice = inDevs[0];

                    if (chosenDevice.isNotEmpty())
                    {
                        juce::AudioDeviceManager::AudioDeviceSetup setup;
                        setup.inputDeviceName = chosenDevice;
                        setup.outputDeviceName = chosenDevice;
                        setup.sampleRate = 48000;
                        setup.bufferSize = 128; // 2.7 ms ultra-low latency!
                        setup.inputChannels.setRange (0, 2, true);
                        setup.outputChannels.setRange (0, 2, true);
                        setup.useDefaultInputChannels = false;
                        setup.useDefaultOutputChannels = false;

                        juce::String err = audioDeviceManager.setAudioDeviceSetup (setup, true);
                        if (err.isEmpty() && audioDeviceManager.getCurrentAudioDevice() != nullptr)
                        {
                            asioConfigured = true;
                            saveAppSettings();
                        }
                    }
                    break;
                }
            }

            if (! asioConfigured)
                audioDeviceManager.initialiseWithDefaultDevices (2, 2);
        }
    }

    void saveAppSettings()
    {
        if (auto* userSettings = appProperties.getUserSettings())
        {
            if (window != nullptr)
                userSettings->setValue ("windowState", window->getWindowStateAsString());

            if (auto audioXml = audioDeviceManager.createStateXml())
                userSettings->setValue ("audioSetup", audioXml.get());

            appProperties.saveIfNeeded();
        }
    }

    juce::ApplicationProperties appProperties;
    juce::AudioDeviceManager audioDeviceManager;
    ToneForgeAudio audio;
    std::unique_ptr<ToneForgeWindow> window;
};

START_JUCE_APPLICATION (ToneForgeApplication)
