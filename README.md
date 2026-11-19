# ToneForge 🎸

**ToneForge — Professional Guitar Tone Studio & Effects Engine**

A local-first Windows guitar tone studio and standalone amp simulation workstation, optimized for low-latency playing with audio interfaces like the **Focusrite Scarlett Solo (4th Gen)** on Windows 10/11.

---

## Highlights & Features

### 🎨 Modern Dark Studio UI
- **Pro Audio Theme:** Clean, high-contrast dark studio interface built around `#191A1C` (Studio Main), `#0BA4F0` (Electric Blue Secondary), and `#8049E2` (Purple Accent).
- **Responsive Studio HUD:** Ergonomic rack module layout designed for standard desktop monitors (1920×1080) and smaller screens alike.

### 🔌 Focusrite Scarlett Solo 4th Gen & Windows ASIO Support
- **Native ASIO Engine:** Full Steinberg ASIO SDK integration with low-latency drivers (e.g. `Focusrite USB ASIO`) for ultra-low latency (<3 ms at 64/128 samples).
- **Dedicated Guitar Channel Selector:** Focusrite Scarlett Solo interfaces map the 1/4" Instrument/Guitar jack to **Input 2** and the XLR microphone to **Input 1**. ToneForge defaults directly to **Channel 2 (Guitar)** with one-click toggling for Channel 1 (Mic) or Stereo Sum.
- **One-Click Audio Setup:** An **AUDIO SETUP** button directly on the top header gives instant access to driver selection (ASIO, WASAPI Exclusive, DirectSound), buffer sizes (64, 128, 256 samples), sample rates (44.1k, 48k, 96k), and channel routing. Settings are saved and restored automatically.
- **Live Hardware Telemetry:** Displays current active driver, sample rate, buffer size, and calculated round-trip latency in the bottom status bar.

### 🎯 High-Precision YIN Chromatic Tuner
- Real-time guitar pitch detection using the YIN algorithm with sub-sample parabolic interpolation.
- Displays detected note name (`E2`, `A2`, `D3`, `G3`, `B3`, `E4`, Drop D, 7-string low B, etc.), exact frequency in Hz, and a smooth cents needle deviation meter (-50 to +50 cents).
- Visual **IN TUNE** green lock indicator when within ±3 cents.
- Dedicated **Mute for Tuning** toggle.

### 🎛️ Complete Guitar Signal Chain
1. **Noise Gate:** High-speed downward expander with threshold and release controls, plus a real-time clamping LED to kill single-coil hum and high-gain hiss.
2. **Compressor:** Studio stompbox with sustain, attack time, and auto makeup gain for clean funk, country picking, and singing leads.
3. **Overdrive / Distortion Pedal & Custom Pedal Importer (4x Oversampled):**
   - *TS-9 Tube Screamer* (tight bottom end, pre-emphasis 720 Hz mid-hump boost)
   - *Klon Centaur* (transparent Germanium clipping with dynamic clean-dry blend)
   - *Rodent Distortion* (punchy hard diode clipping)
   - *Boss HM-2 Chainsaw* (legendary Swedish death metal dual mid-peak distortion)
   - *Horizon Precision Drive* (modern djent attack tightener with aggressive sub-bass filter)
   - *EHX Big Muff Pi* (saturated wall-of-sound fuzz)
   - *Custom Loaded Pedal:* Load external distortion models (`.tfpedal`, `.xml`, `.wav`, `.nam`, `.json`) via the **LOAD PEDAL** button!
4. **Tube Amp Head Simulator & High-Precision Active Biquad Tone Stack:**
   - *Clean Channel:* Sparkling Fender Twin style with high headroom and warm tube triode saturation.
   - *Crunch Channel:* Classic British Plexi / JCM crunch with 2-stage cascaded triode distortion.
   - *Lead Channel:* Modern 5150 / Dual Rectifier 3-stage cascaded high-gain distortion with pre-gain bass tightener filter to eliminate flubby mud.
   - *Active 3-Band Parametric Tone Stack:* High-precision biquad Low Shelf Bass filter (90 Hz, -24 dB to +12 dB cut/boost range to completely kill mud), Mid Peak (750 Hz), Treble High Shelf (2.8 kHz), and Presence High Shelf (5.5 kHz).
5. **Cabinet Simulator & Custom IR Loader:**
   - *4×12 Vintage 30:* Multi-pole speaker response with steep 4.2 kHz lowpass and 85 Hz highpass filters.
   - *2×12 British Greenback:* Warm 3.8 kHz speaker roll-off with wood resonance boost.
   - *1×12 American Deluxe:* Open chime with mid scoop.
   - *Custom IR Loader:* Load any third-party 24-bit `.wav` impulse response directly via the built-in convolution engine!
6. **Neural Amp Modeler (NAM) Engine (Steven Atkinson's NAMCore):**
   - **Full Native NAM Support:** Load any official `.nam` neural network guitar amplifier, preamp, or pedal model directly inside ToneForge!
   - **WaveNet & LSTM Architecture:** Powered by `NeuralAmpModelerCore` for 100% accurate, high-fidelity neural network guitar modeling.
   - **Dedicated Input/Output Trim:** Independent input and output level controls for matching model gain staging.
7. **External VST3 Plugin Hosting Engine:**
   - **Load Any 64-Bit VST3 Plugin:** Load external 3rd-party VST3 audio plugins directly into ToneForge's live rack!
   - **Native Floating Plugin Window:** Click **OPEN PLUGIN UI** to view and manipulate the plugin's graphical user interface in a dedicated floating window.
   - **Bypass / Enable & Preset Memory:** Seamlessly save and restore loaded VST3 plugins inside custom profiles.
8. **Stereo Tape Delay:** Ping-pong stereo offset, analog high-frequency tape damping, feedback, and mix controls.
9. **Lush Spatial Reverb & Studio Reverb IR Loader:** Smooth plate/hall algorithmic reverb plus custom space impulse response loader (**LOAD IR (.WAV)**) for importing studio plate and spring tank `.wav` files.
10. **Master Section:** Input gain trim, output volume, peak/RMS meters, and a transparent safety brickwall limiter to protect your ears and speakers.

---

## Factory Presets Included
- ⚡ **Modern Metal Lead** (High Gain Amp, TS-9 Boost, 4x12 V30, Ping-Pong Delay, Lush Reverb)
- 🤘 **Heavy Metal Rhythm** (Tight Gate, TS-9 Boost, High Gain Amp, 4x12 V30, Dry)
- 🎸 **Classic British Crunch** (Plexi Crunch, 2x12 Greenback, Vintage Delay, Plate Reverb)
- 🌧️ **Texas Blues Breakup** (Warm Clean Amp pushed, Overdrive, 1x12 American, Spring Reverb)
- ✨ **Ambient Dream Clean** (Clean Amp, Compressor, 1x12 American, Space Delay & Lush Reverb)
- 🎶 **Warm Jazz Chords** (Clean Amp, Gentle Compressor, 2x12 Greenback, Subtle Room Reverb)

You can tweak any preset and click **SAVE** to keep your custom settings.

---

## Project Structure

```text
ToneForge/
├── bin/                 # Compiled executable (ToneForge.exe) deployed here
├── build/               # CMake build cache and intermediate artifacts
├── Resources/           # Icons and application visual assets
│   ├── icon.ico         # Multi-resolution Windows app icon
│   └── icon.png         # High-resolution application logo
├── scripts/             # Utility and automation scripts
│   └── screenshot.ps1   # Window capture utility script
├── Source/              # C++ application source code
│   ├── AudioEngine.h    # Core DSP signal chain & amp simulation
│   ├── CustomLookAndFeel.h # Custom dark studio UI styling
│   ├── Main.cpp         # JUCE application entry point & window management
│   ├── MainPanel.h/.cpp # Studio HUD user interface & controls
│   ├── PresetManager.h  # Preset configuration & storage
│   └── Tuner.h          # YIN chromatic tuner algorithm
├── ThirdParty/          # Vendor SDKs
│   └── asiosdk/         # Steinberg ASIO SDK 2.3
├── build.ps1            # Automated build script for Windows
├── CMakeLists.txt       # Primary CMake build specification
└── README.md            # Documentation
```

---

## Build & Run

### Quick Build (PowerShell)
To build the Release binary in one step (auto-detects CMake & Visual Studio):
```powershell
.\build.ps1
```

### Manual CMake Build
```powershell
# Configure with ASIO & JUCE
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Compile Release binary
cmake --build build --config Release
```

### Executable Locations
After building, the executable is available in:
- **`bin/ToneForge.exe`** (convenient top-level access)
- **`build/bin/Release/ToneForge.exe`** (standard CMake output directory)

---

## License
Project code is released under GPL-3.0-or-later. Steinberg ASIO SDK headers are covered by Steinberg's licensing terms.
