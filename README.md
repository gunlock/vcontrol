# VControl

A minimal X-Plane plugin that exposes offline speech recognition to FlyWithLua scripts via datarefs. The plugin handles the hardware layer (audio capture and Vosk recognition). Grammar, command routing, view management, and all sim interactions are owned entirely by the Lua script.

---

## Project layout

```
vcontrol/
├── src/
│   ├── main.cpp                  XPluginStart/Stop/Enable/Disable entry points
│   ├── config.h.in               CMake-generated version/name constants
│   ├── logger.h / logger.cpp     fmt-backed logger with pluggable sinks
│   ├── recorder.h / recorder.cpp miniaudio capture device, s16 mono PCM buffer
│   ├── recognizer.h / recognizer.cpp  Vosk wrapper with reloadable grammar
│   └── controldatarefs.h / controldatarefs.cpp  All five plugin datarefs
├── cmake/
│   └── toolchains/               Zig cross-compilation toolchain files
│       ├── zig-linux-x64.cmake
│       ├── zig-windows-x64.cmake
│       ├── zig-macos-x64.cmake
│       └── zig-macos-arm64.cmake
├── scripts/
│   └── build-nomacos.sh          Docker release build script (Linux + Windows)
├── CMakeLists.txt
├── CMakePresets.json             Cross-compilation presets (used inside Docker)
├── CMakeUserPresets.json         Local developer presets (gitignored — see below)
└── Dockerfile                    Build image: Ubuntu 24.04 + Zig 0.14 + CMake 3.28
```

---

## Architecture

### Voice pipeline

```
Recorder  (miniaudio, audio thread)
  └─ raw s16 PCM → mutex-protected buffer
       └─ on vcontrol/recognize write (main thread)
            └─ Recognizer::accept() + flush()
                 └─ Vosk JSON result → result callback
                      └─ ControlDataRefs::m_result updated
                           └─ vcontrol/result readable by Lua
```

Audio is captured into a buffer for the duration of push-to-talk. On release the entire buffer is submitted to Vosk in one batch. The recognizer is constrained to the grammar JSON that Lua wrote into `vcontrol/grammar`, which improves accuracy and eliminates spurious matches.

### ControlDataRefs class

All five datarefs are owned by a single `ControlDataRefs` instance. Its `this` pointer is passed as refcon to every X-Plane callback, giving the static callback functions access to the `Recorder` and `Recognizer` without global state. Construction registers the datarefs; destruction unregisters them.

### Critical compiler flags

Two flags are required for correct plugin behaviour on reload and shutdown:

- **`-fno-c++-static-destructors`** — prevents the compiler from registering C++ static object destructors via `__cxa_atexit`. When X-Plane calls `dlclose()` on the plugin, those destructors can reference already-unmapped code and crash.
- **`LINKER:-z,nodelete`** (Linux) — marks the shared library as non-deletable so `dlclose()` skips `__cxa_finalize`, protecting against atexit handlers registered by linked libraries (Vosk, miniaudio).

---

## Dataref API

| Dataref | Type | Dir | Semantics |
|---|---|---|---|
| `vcontrol/record` | int | write | 1 = start audio capture, 0 = stop (audio stays in buffer) |
| `vcontrol/recognize` | int | write | write 1 = run Vosk on buffer → result written to `vcontrol/result` |
| `vcontrol/result` | string | read | last recognized text |
| `vcontrol/grammar` | string | write | Vosk grammar JSON array; plugin reinitializes recognizer. Empty = unload. |
| `vcontrol/recording` | int | read | 1 while capture is active, 0 otherwise |

### Separation of concerns

| Responsibility | Owner |
|---|---|
| Audio capture | Plugin (miniaudio) |
| Speech recognition | Plugin (Vosk, offline) |
| Grammar definition | Lua script |
| Command → action mapping | Lua script |
| View coordinate storage | Lua script (table) |
| Reading/writing sim datarefs | Lua script directly |
| Persistence (views, config) | Lua script (file I/O) |
| UI / feedback | Lua script |

The plugin has no knowledge of what phrases mean or what to do with them. It recognizes text and reports it. Everything above that is Lua's responsibility.

---

## Dependencies

All dependencies are fetched automatically by CMake via `FetchContent`.

| Dependency | Version | Notes |
|---|---|---|
| X-Plane SDK | 4.3.0 | Headers + import libs only |
| Vosk | 0.3.45 | Pre-built shared library (Linux / Windows) |
| vosk-model-small-en-us | 0.15 | ~40 MB offline speech model |
| miniaudio | 0.11.25 | Header-only audio capture |
| fmtlib | 12.1.0 | String formatting for the logger |
| nlohmann/json | 3.11.3 | Vosk result JSON parsing |

---

## Build system

### Prerequisites

- Linux build host which will cross compile for windows targets.
- CMake ≥ 3.25
- Ninja
- Docker (for all builds — local dev also compiles inside Docker for ABI consistency)

### CMakeUserPresets.json

`CMakeUserPresets.json` is gitignored and must be created locally. Copy the template below and set `XPLANE_PLUGIN_DIR` to your X-Plane plugins path:

```json
{
  "version": 6,
  "cmakeMinimumRequired": { "major": 3, "minor": 25, "patch": 0 },
  "configurePresets": [
    {
      "name": "config",
      "displayName": "Local dev",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/dev",
      "cacheVariables": {
        "XPLANE_PLUGIN_DIR": "/path/to/X-Plane 12/Resources/plugins"
      }
    }
  ],
  "buildPresets": [
    { "name": "local:build",          "configurePreset": "config", "targets": ["build"] },
    { "name": "local:deploy",         "configurePreset": "config", "targets": ["deploy"] },
    { "name": "local:install",        "configurePreset": "config", "targets": ["install-plugin"] },
    { "name": "docker:deploy",        "configurePreset": "config", "targets": ["docker-deploy"] },
    { "name": "docker:rebuild-image", "configurePreset": "config", "targets": ["docker-rebuild"] },
    { "name": "docker:clean-image",   "configurePreset": "config", "targets": ["docker-clean-image"] }
  ]
}
```

### One-time configure

```sh
cmake --preset config
```

### Dev inner loop (Linux x64 via Docker + Zig)

```sh
cmake --build --preset local:build    # compile inside Docker
cmake --build --preset local:deploy   # assemble plugin directory
cmake --build --preset local:install  # copy to XPLANE_PLUGIN_DIR
```

Then reload the plugin in X-Plane: **Plugins → Plugin Admin → Reload**.

### Release build (Linux + Windows zip)

```sh
cmake --build --preset docker:deploy
```

Output: `build/docker/deploy/VControl.zip`

### Docker image management

```sh
cmake --build --preset docker:rebuild-image   # rebuild after Dockerfile changes
cmake --build --preset docker:clean-image     # remove cached image
```

---

## FlyWithLua example

The script below demonstrates a complete voice-controlled view manager:
- Custom grammar with named view slots
- Push-to-talk bound to `Ctrl+Alt+R`
- Saving and restoring pilot head position/orientation
- Persistent view storage written to a config file

```lua
-- vcontrol.lua
-- Place in: X-Plane 12/Resources/plugins/FlyWithLua/Scripts/

-- ----------------------------------------------------------------------------
-- View storage (in-memory table, saved/loaded from file)
-- ----------------------------------------------------------------------------
local VIEWS_FILE = SCRIPT_DIRECTORY .. "vcontrol_views.cfg"
local views = {}   -- views[slot] = {x, y, z, psi, the, phi}

local function saveViewsToFile()
  local f = io.open(VIEWS_FILE, "w")
  if not f then return end
  for slot, v in pairs(views) do
    f:write(string.format("%d|%f|%f|%f|%f|%f|%f\n",
      slot, v.x, v.y, v.z, v.psi, v.the, v.phi))
  end
  f:close()
end

local function loadViewsFromFile()
  local f = io.open(VIEWS_FILE, "r")
  if not f then return end
  for line in f:lines() do
    local slot, x, y, z, psi, the, phi =
      line:match("(%d+)|([^|]+)|([^|]+)|([^|]+)|([^|]+)|([^|]+)|([^|]+)")
    if slot then
      views[tonumber(slot)] = {
        x = tonumber(x), y = tonumber(y), z = tonumber(z),
        psi = tonumber(psi), the = tonumber(the), phi = tonumber(phi)
      }
    end
  end
  f:close()
end

local function captureView(slot)
  views[slot] = {
    x   = get("sim/graphics/view/pilots_head_x"),
    y   = get("sim/graphics/view/pilots_head_y"),
    z   = get("sim/graphics/view/pilots_head_z"),
    psi = get("sim/graphics/view/pilots_head_psi"),
    the = get("sim/graphics/view/pilots_head_the"),
    phi = get("sim/graphics/view/pilots_head_phi"),
  }
  saveViewsToFile()
  logMsg(string.format("VControl: saved view %d", slot))
end

local function restoreView(slot)
  local v = views[slot]
  if not v then
    logMsg(string.format("VControl: no view saved for slot %d", slot))
    return
  end
  set("sim/graphics/view/pilots_head_x",   v.x)
  set("sim/graphics/view/pilots_head_y",   v.y)
  set("sim/graphics/view/pilots_head_z",   v.z)
  set("sim/graphics/view/pilots_head_psi", v.psi)
  set("sim/graphics/view/pilots_head_the", v.the)
  set("sim/graphics/view/pilots_head_phi", v.phi)
end

-- ----------------------------------------------------------------------------
-- Grammar — every phrase Vosk should recognize
-- ----------------------------------------------------------------------------
local grammar = [[
  ["view one", "view two", "view three", "view four",
   "save view one", "save view two", "save view three", "save view four",
   "reset view", "[unk]"]
]]

-- ----------------------------------------------------------------------------
-- Command dispatch
-- ----------------------------------------------------------------------------
local commands = {
  ["view one"]       = function() restoreView(1) end,
  ["view two"]       = function() restoreView(2) end,
  ["view three"]     = function() restoreView(3) end,
  ["view four"]      = function() restoreView(4) end,
  ["save view one"]  = function() captureView(1) end,
  ["save view two"]  = function() captureView(2) end,
  ["save view three"]= function() captureView(3) end,
  ["save view four"] = function() captureView(4) end,
  ["reset view"]     = function() restoreView(0) end,  -- slot 0 = default
}

-- ----------------------------------------------------------------------------
-- Plugin datarefs
-- ----------------------------------------------------------------------------
DataRef("vcontrol_record",    "vcontrol/record",    "writable")
DataRef("vcontrol_recognize", "vcontrol/recognize", "writable")
DataRef("vcontrol_recording", "vcontrol/recording", "readonly")
DataRef("vcontrol_result",    "vcontrol/result",    "readonly")
DataRef("vcontrol_grammar",   "vcontrol/grammar",   "writable")

-- React whenever vcontrol/result changes
do_on_new_data("vcontrol_result", function()
  local text = vcontrol_result  -- FlyWithLua exposes string datarefs as variables
  local action = commands[text]
  if action then
    action()
  else
    logMsg("VControl: unrecognized: '" .. tostring(text) .. "'")
  end
end)

-- ----------------------------------------------------------------------------
-- Push-to-talk — Ctrl+Alt+R
-- FlyWithLua key modifiers: CONTROL=1, OPTION/ALT=8, SHIFT=2
-- ----------------------------------------------------------------------------
local function ptt_begin()
  vcontrol_record = 1
end

local function ptt_end()
  vcontrol_record = 0
  vcontrol_recognize = 1
end

add_key_stroke_handler("r", CONTROL + OPTION, ptt_begin, ptt_end)

-- ----------------------------------------------------------------------------
-- Startup
-- ----------------------------------------------------------------------------
loadViewsFromFile()
set("vcontrol/grammar", grammar)
logMsg("VControl: script loaded, grammar active")
```

### How the FlyWithLua script works

1. **Script load** — `loadViewsFromFile()` restores any previously saved views; `set("vcontrol/grammar", grammar)` causes the plugin to initialize the Vosk recognizer with the phrase list.
2. **PTT press** — `ptt_begin()` writes `vcontrol/record = 1`; the plugin starts miniaudio capture.
3. **PTT release** — `ptt_end()` writes `vcontrol/record = 0` (stop capture), then `vcontrol/recognize = 1` (run Vosk on the buffer). The plugin writes the result to `vcontrol/result` and clears the audio buffer.
4. **Result** — the `do_on_new_data` handler fires, looks up the recognized phrase in the `commands` table, and calls the appropriate function.
5. **Save view** — `captureView()` reads the six `sim/graphics/view/pilots_head_*` datarefs directly and writes them to `views[slot]` and to the config file.
6. **Restore view** — `restoreView()` writes those six values back to the sim datarefs.

### Hotkey binding

`add_key_stroke_handler` registers a begin/end handler directly in Lua — no X-Plane command binding required. The modifier constants (`CONTROL`, `OPTION`, `SHIFT`) are provided by FlyWithLua and can be combined with `+`. `ptt_begin` fires on key down and `ptt_end` fires on key up, giving the hold-to-talk semantics the plugin expects.

To use a different key combination, replace `"r"` with any single character and adjust the modifier mask.
