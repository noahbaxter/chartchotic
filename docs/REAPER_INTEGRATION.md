# REAPER Integration

Everything about how Chart Preview connects to REAPER's API for direct timeline MIDI access.

---

## Why Custom JUCE Modifications?

Stock JUCE cannot access REAPER's API. Every developer who wants full REAPER integration modifies JUCE — there's no official support planned:

- **Xenakios** (2016-2018): Maintained JUCE fork with REAPER modifications
- **GavinRay97** (2021): Created patches for VST3 REAPER UI embedding ([repo](https://github.com/GavinRay97/JUCE-reaper-embedded-fx-gui))
- **This project** (2025): Custom modifications for VST2 + VST3 API access

---

## VST2 Integration

### The Handshake

REAPER exposes its API via `audioMasterCallback` with magic numbers:

```cpp
// Magic numbers: 0xdeadbeef (opcode), 0xdeadf00d (index)
auto reaperGetFunc = (void*(*)(const char*))audioMaster(nullptr, 0xdeadbeef, 0xdeadf00d, 0, nullptr, 0.0);
if (reaperGetFunc) {
    auto GetPlayState = (int(*)())reaperGetFunc("GetPlayState");
}
```

These magic numbers are standardized REAPER extension protocol — not documented anywhere obvious, found by digging through REAPER SDK examples and reverse-engineering existing plugins.

### JUCE Modifications for VST2

**File 1**: `third_party/JUCE/modules/juce_audio_processors/utilities/juce_VST2ClientExtensions.h`
```cpp
// Added virtual method
virtual void handleReaperApi (void* (*reaperGetFunc)(const char*)) {}
```

**File 2**: `third_party/JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_VST2.cpp`
```cpp
// Performs REAPER handshake automatically during plugin init
auto reaperGetFunc = (void*(*)(const char*))audioMaster(nullptr, 0xdeadbeef, 0xdeadf00d, 0, nullptr, 0.0);
if (reaperGetFunc) {
    if (auto* callbackHandler = processorPtr->getVST2ClientExtensions())
        callbackHandler->handleReaperApi(reaperGetFunc);
}
```

### Plugin-Side Implementation

```
Source/PluginProcessor.h:38-54    — ChartPreviewVST2Extensions declaration
Source/PluginProcessor.cpp:218-325 — VST2 extensions implementation
Source/Midi/ReaperMidiProvider.h/cpp — REAPER API wrapper
```

Integration points:
- `PluginProcessor.cpp:264` — `handleVstHostCallbackAvailable()` receives callback
- `PluginProcessor.cpp:273` — `tryGetReaperApi()` performs handshake
- `PluginProcessor.cpp:302` — Initializes `ReaperMidiProvider`

### REAPER Capabilities Advertised

The plugin advertises support for these REAPER extensions:
- `reaper_vst_extensions` → Returns 1
- `hasCockosExtensions` → Returns `0xbeef0000`
- `hasCockosNoScrollUI` → Returns 1
- `hasCockosSampleAccurateAutomation` → Returns 1
- `wantsChannelCountNotifications` → Returns 1

---

## VST3 Integration

Uses `IReaperHostApplication` interface instead of magic numbers.

### Implementation

**File**: `Source/ReaperVST3.h`
```cpp
using namespace Steinberg;
#include "reaper_vst3_interfaces.h"
DEF_CLASS_IID(IReaperHostApplication)
```

**VST3 Extensions** (in PluginProcessor.cpp):
```cpp
class ChartPreviewVST3Extensions : public juce::VST3ClientExtensions {
    void setIHostApplication(Steinberg::FUnknown* host) override {
        auto reaper = FUnknownPtr<IReaperHostApplication>(host);
        if (reaper) {
            // Create function pointer wrapper matching VST2 signature
            static FUnknownPtr<IReaperHostApplication> staticReaper = reaper;
            static auto reaperApiWrapper = [](const char* funcname) -> void* {
                return staticReaper ? staticReaper->getReaperApi(funcname) : nullptr;
            };
            processor->reaperGetFunc = reaperApiWrapper;
            processor->reaperMidiProvider.initialize(processor->reaperGetFunc);
        }
    }
};
```

Both VST2 and VST3 produce the same `void* (*)(const char*)` function pointer — everything downstream is shared.

### VST2 vs VST3 Comparison

| Aspect | VST2 | VST3 |
|--------|------|------|
| Entry point | `handleVstHostCallbackAvailable()` | `setIHostApplication()` |
| API access | Magic numbers `0xdeadbeef/0xdeadf00d` | Query `IReaperHostApplication` |
| Interface | Manual callback management | `FUnknownPtr<>` automatic COM |
| Function access | Direct audioMaster callback | `getReaperApi()` method |

---

## Why VST2 Was Built First (Historical Context)

VST3 was attempted first but hit blockers:
1. JUCE's `FUnknown` is forward-declared only — can't call `queryInterface()` without full VST3 SDK
2. Manual vtable calls crashed (wrong vtable layout)
3. Using `FUID` class caused linker errors
4. Linking full VST3 SDK conflicts with JUCE's wrapper

VST2 uses simple function pointer exchange instead of COM interfaces — much simpler. VST3 was solved later using GavinRay97's approach with `FUnknownPtr<IReaperHostApplication>`.

**Trade-off**: VST2 is deprecated by Steinberg, but REAPER still supports it and it's the pragmatic choice for REAPER-specific features.

---

## PPQ Resolution (Critical)

REAPER stores MIDI internally at **960 PPQ per quarter note**. VST playhead reports in **quarter notes** (1 PPQ = 1 QN). You must convert:

```cpp
const double REAPER_PPQ_RESOLUTION = 960.0;
double convertedPPQ = reaperInternalPPQ / REAPER_PPQ_RESOLUTION;
```

Example: Note at REAPER PPQ 23040 → 23040 / 960 = 24.0 QN (matches VST playhead).

---

## Two Pipelines

The plugin auto-detects REAPER and selects the appropriate pipeline in `prepareToPlay()`:

```
REAPER detected → ReaperMidiPipeline (direct timeline access)
Other DAWs     → StandardMidiPipeline (VST buffer processing)
```

### REAPER Pipeline Advantages
- **Unlimited lookahead** — reads any point in timeline, not limited to audio buffer
- **Scrubbing** — instant visual feedback when dragging playhead while paused
- **Multi-track** — can read MIDI from different tracks
- **Caching** — smart cache with invalidation (track switch, project change, MIDI edit)
- **Absolute positioning** — always knows exact PPQ from project start

### Standard Pipeline Advantages
- **Universal** — works in any VST host
- **Simpler** — fewer moving parts
- **No special extensions** — standard VST APIs only

### Shared Components (Both Pipelines)
`MidiInterpreter`, `MidiProcessor`, `HighwayRenderer`, `GridlineMap`, `MidiUtility`

### REAPER-Only Components
`ReaperMidiProvider`, `MidiCache`, `ReaperVST2Extensions`, `ReaperVST3Extensions`, `ReaperTrackDetector`

### Known Limitations

**REAPER Pipeline:**
- Manual track selection required (dropdown in UI)
- Cache invalidation on large projects can cause brief stutter

**Standard Pipeline:**
- Limited lookahead (buffer size only, ~512-2048 samples)
- Scrubbing depends on host implementation
- Cannot read from other tracks
- May miss MIDI events if buffer is very small

---

## Available REAPER API Functions

Once connected, you can access 1000+ functions from `reaper_plugin.h`:

**Timeline**: `GetPlayPosition()`, `GetPlayPosition2Ex()`, `GetCursorPosition()`, `GetPlayState()`
**MIDI**: `MIDI_CountEvts()`, `MIDI_GetNote()`, `MIDI_EnumSelNotes()`
**Project/Track**: `GetTrack()`, `GetActiveTake()`, `GetMediaItem()`, `CountTracks()`

Full list: `third_party/reaper-sdk/sdk/reaper_plugin.h`

---

## Verification & Debugging

**Visual Indicators** (in plugin UI):
- Green background → REAPER connected successfully
- Red → REAPER detected but connection failed
- Blue → No REAPER provider initialized
- Yellow → Buffer mode (non-REAPER)

**Debug Output**: Check for "REAPER API connected via VST2/VST3" message.

**Common Issues:**
- No green background → Make sure you loaded VST2 or VST3 (not AU)
- API functions returning nullptr → Check function name spelling matches REAPER API exactly
- No MIDI notes → Check PPQ 960:1 conversion is applied
- Build errors → Ensure JUCE submodule initialized, VST2 SDK headers present

---

## Resources

**REAPER SDK**: `third_party/reaper-sdk/sdk/reaper_plugin.h`, `reaper_vst3_interfaces.h`
**Reference Project**: [JUCE-reaper-embedded-fx-gui](https://github.com/GavinRay97/JUCE-reaper-embedded-fx-gui)
**Forums**: [JUCE Forum (VST2 vs VST3)](https://forum.juce.com/t/attaching-to-the-reaper-api-vst2-vs-vst3/45459), [Cockos Forum](https://forums.cockos.com/showthread.php?t=253505), [JUCE Issue #902](https://github.com/juce-framework/JUCE/issues/902)
