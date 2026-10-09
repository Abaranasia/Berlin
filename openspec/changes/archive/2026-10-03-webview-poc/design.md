# Design: WebView PoC (UI Redesign Slice 1/6)

## Technical Approach

`WebEditor` is a thin JUCE shell around `WebBrowserComponent` (WebView2). All testable logic lives in juce_core-only helpers under `Source/ui/`. One shared factory makes the compile-time editor choice for both the plugin and the standalone app. One Node entry script (`ui/scripts/embed-assets.mjs`) is the pre-build step. It reads the flag from the `.jucer` file it is building, so the define and the build mode can't drift apart.

## Architecture Decisions

| # | Topic | Choice | Rejected | Rationale |
|---|---|---|---|---|
| D1 | Flag source of truth | `BERLIN_WEB_UI=1` lives only in `JUCERPROJECT@defines`. The script is called as `--jucer <path>`, parses that attribute (splitting on whitespace, newlines and commas, like Projucer's `parsePreprocessorDefs` in `extras/Projucer/Source/Utility/Helpers/jucer_MiscUtilities.cpp`) and picks the stub or full mode. If `BERLIN_WEB_UI` appears anywhere else in the `.jucer`, it fails. | A per-config argument or environment variable kept in sync by hand. Reading MSBuild `PreprocessorDefinitions` (that metadata isn't available in `PreBuildEvent`). | One edit flips both C++ and assets. |
| D2 | Mismatch guard | The generated header emits `BERLIN_EMBEDDED_ASSETS_FULL 0/1`. `WebEditor.cpp` does `static_assert(BERLIN_EMBEDDED_ASSETS_FULL)`. The whole body of `WebEditor.cpp` (and any other GUI-only file) is wrapped in `#if BERLIN_WEB_UI`, so the assert applies only when the flag is on; flag-off builds compile the file to nothing. | Trusting D1 alone | Stale stub assets fail the compile instead of showing a blank UI. |
| D3 | Node missing | The cmd wrapper runs `where node >nul 2>nul \|\| (echo Berlin prebuild : error BRL001: node not found on PATH & exit /b 1)`. The script checks for pnpm with `spawnSync` and emits `Berlin prebuild : error BRL002: pnpm not found on PATH` (the message MUST contain `pnpm not found`). The pre-build cwd is the vcxproj folder, so the script resolves the repo root from its own file location (`import.meta.url`) and spawns pnpm with cwd = repo root. | Letting VS report a bare exit 9009 | The MSBuild canonical format shows the error in the Error List. |
| D4 | Generated `.cpp` registration | `FILE compile="1"` entries for `Source/ui/generated/EmbeddedAssets.{h,cpp}` in `Berlin.jucer` and `BerlinPlugin.jucer` only. In MSBuild, `PreBuildEvent` runs before `ClCompile`, and a missing item at evaluation is fine. Before any `--resave`, run the script once so the files exist. | Committing the files. BinaryData (only regenerates on resave). | Clean checkouts build. Not registered in BerlinTests: tests inject fixture tables. |
| D5 | Repeated pre-build | The plugin emits the pre-build into every target vcxproj (`getPreBuildSteps` is per target). The script hashes the mode plus `ui/src/**`, `ui/index.html`, `ui/package.json`, `ui/pnpm-lock.yaml`, `ui/vite.config.ts`, `ui/tsconfig*.json` and `ui/scripts/*` into `generated/.stamp` and skips install, build and embed when the hash is unchanged and the generated files exist. It writes to a temp file and renames, and only writes when the content differs. | Running the full pipeline every time | Fast, idempotent and no needless recompiles. |
| D6 | Editor choice | `createBerlinEditor(BerlinAudioProcessor&)` in `Source/ui/EditorFactory.cpp` holds the single `#if BERLIN_WEB_UI`. `WebEditor` derives from `juce::AudioProcessorEditor`. The factory call stays inside the non-`BERLIN_HEADLESS` branch of `createEditor` (`Source/plugin/BerlinAudioProcessor.cpp`), so BerlinTests never pulls GUI code. | Duplicating the `#if` in `createEditor` and in `MainComponent` | One switch point. |
| D7 | Projucer resave | The apply agent runs `H:\Proyectos\Juce\Projucer\Projucer.exe --resave <jucer>` for both projects (the binary exists). The first NuGet restore and the go/no-go checks are manual and done by the user. | Manual GUI resave | Scriptable and reviewable. |
| D8 | Dev server | The `devServerOrigin` helper compiles in every configuration (for testing) but is used only under `#if JUCE_DEBUG`: environment variable `BERLIN_WEB_UI_DEV_URL`, accepted only as `http://localhost:<port>` or `http://127.0.0.1:<port>`. | Any URL | Release builds serve embedded assets only. |

## Data Flow

    JS dispatch(cmd,args) ─▶ NativeFunction (msg thread) ─▶ dispatchNativeCall ─▶ UiBridge::dispatch ─▶ completion(result)
    Timer 30Hz ─▶ {getPlayheadStep,isPlaying} ─▶ PlayheadChangeDetector ─changed && browser visible─▶ emitEventIfBrowserIsVisible("playhead") ─sent─▶ detector.commit(state)
    (the last-emitted state is committed only when the browser is visible and the event is sent, so a hidden→visible transition re-emits)
    WebView GET /x ─▶ ResourceProvider ─▶ findAsset(getEmbeddedAssets(), "/x") ─▶ Resource | nullopt

## File Changes

| File | Action |
|---|---|
| `Source/ui/WebAssets.{h,cpp}` | Create: path normalisation, lookup, MIME type |
| `Source/ui/WebEditorSupport.{h,cpp}` | Create: user data folder, adapter, change detector, dev origin |
| `Source/ui/WebEditor.{h,cpp}`, `Source/ui/EditorFactory.{h,cpp}` | Create (GUI only) |
| `Source/MainComponent.{h,cpp}`, `Source/plugin/BerlinAudioProcessor.cpp` | Modify: `std::unique_ptr<juce::Component> editor` (declared after `processor`); use the factory |
| `ui/package.json`, `pnpm-lock.yaml`, `tsconfig.json`, `vite.config.ts` (`base:'./'`), `index.html` | Create |
| `ui/src/{main.tsx,App.tsx,bridge/native.ts,state/steps.ts}`, `ui/src/bridge/juce/index.js` (copied) | Create |
| `ui/scripts/embed-assets.mjs` (CLI), `ui/scripts/embed-lib.mjs` (pure functions) | Create |
| `Tests/Source/WebAssetsTests.cpp`, `WebEditorSupportTests.cpp`; `Tests/BerlinTests.jucer` | Create / register the helpers only |
| `Berlin.jucer`, `Plugin/BerlinPlugin.jucer` | `JUCE_USE_WIN_WEBVIEW2="1"`; files; per-config `prebuildCommand` |
| `.gitignore` | `Source/ui/generated/`, `ui/node_modules/`, `ui/dist/` |

Pre-build command (Debug and Release; one level more of `..\` for the plugin): `where node … & node ..\..\ui\scripts\embed-assets.mjs --jucer ..\..\Berlin.jucer`. With the flag on, the script runs `pnpm --dir ui install --frozen-lockfile` and then `run build` itself.

## Interfaces / Contracts

```cpp
struct EmbeddedAsset { const char* path; const unsigned char* data; size_t size; };
struct AssetTable    { const EmbeddedAsset* entries; size_t count; };
struct AssetResource { std::vector<std::byte> data; juce::String mimeType; };
std::optional<juce::String>  normaliseAssetPath (const juce::String&); // "/"->"index.html"; strips ?#; rejects "..", "\\", ":", "%"
std::optional<AssetResource> findAsset (AssetTable, const juce::String& requestPath);
juce::String mimeTypeForPath (const juce::String&);  // case-insensitive extension: html js(text/javascript) css svg json png woff2 ico; else application/octet-stream
AssetTable getEmbeddedAssets();                      // generated
juce::File webViewUserDataFolder (const juce::File& appDataBase); // base/Berlin/WebView2
juce::var  dispatchNativeCall (UiBridge&, const juce::Array<juce::var>&);
// args[0] absent -> {ok:false,error:"missing arg: command"}; args[0] == "" -> "missing arg: command";
// args[0] non-string -> "invalid type: command"; args[1] absent/void -> treated as {} (no error);
// args[1] present but not an object -> "invalid type: args"
struct PlayheadState { int step; bool playing; };
class PlayheadChangeDetector { public: bool hasChanged (PlayheadState) const; void commit (PlayheadState); }; // true until first commit
std::optional<juce::String> devServerOrigin (const juce::String& env);
```

WebEditor lifecycle. Member order: `processor&`, `UiBridge`, `detector`, `fallbackLabel`, `unique_ptr<WebBrowserComponent> browser` (the browser is destroyed first).
- Constructor: `setSize(800,680)`. Create the folder with `createDirectory()`. Build the options. If the folder or `areOptionsSupported` fails, show the label and no timer. Otherwise `goToURL(getResourceProviderRoot())` and `startTimerHz(30)`.
- Destructor: `stopTimer()`, then `browser.reset()`.
- Reopening needs no extra handling: engine state lives in the processor, the new detector emits on its first visible tick, and the JS calls `getSnapshot` on mount.
- Two instances share the data folder with identical environment options. WebView2 shares one browser process for them.

## Testing Strategy

| Layer | Cases |
|---|---|
| BerlinTests (RED first) | `/`, `/index.html`, `/assets/a.js` hit; unknown → nullopt; `../x`, `/a/../b`, `\x`, `%2e` → nullopt; query stripped; MIME per extension and default; folder path under a temp base; `findAsset` on empty table `{nullptr, 0}` → nullopt; adapter: valid `setBpm` → ok + snapshot, no command / `""` → `missing arg: command`, non-string command → `invalid type: command`, args absent/void → treated as `{}`, args array/number → `invalid type: args`, no crash; detector first/same/changed, no commit → still changed; devOrigin accepts localhost, rejects https/remote/empty |
| Vitest | `readWebUiFlag` (on, off, absent, stray occurrence → throw, `JUCE_VST3_CAN_REPLACE_VST2=0` combined with `BERLIN_WEB_UI=1` separated by newline/comma/space → on); `renderAssetSources` (stub is empty and FULL 0; full escapes bytes); pipeline with injected `spawn` (pnpm missing → BRL002, non-zero build → throws); `stepRow`; native wrapper against a mock `__JUCE__` |
| Manual | Proposal go/no-go list, plus: flag off on a machine without pnpm builds; first NuGet restore |

## Threat Matrix

| Boundary | Applicability |
|---|---|
| Documentation-like paths | N/A: there's no executable-file classification |
| Git repo selection / Commit / Push / PR | N/A: there's no VCS automation |
| Pre-build subprocess (supplemental) | Applicable. The arguments are fixed literals, with no user input interpolated. `shell:true` is used only for the `pnpm` resolution on Windows. Any spawn error or non-zero exit makes the script exit 1. RED: the Vitest pipeline cases above |

## Size Forecast

C++ production ~320, C++ tests ~150, scripts ~135, `ui/src` + config ~165, Vitest ~95, `.jucer`/`.gitignore` ~45: **~910 > 800**. Excluded: the lockfile, `index.js`, the generated files and the Projucer-regenerated vcxproj files. Split: PR1 is D1-D5 plumbing, `WebAssets`/`WebEditorSupport` with their tests, the scripts and the `.jucer` files (~540). PR2 is `WebEditor`, the factory, `MainComponent` and the React UI (~370).

## Migration / Rollout

The flag defaults to off. To try it, add `BERLIN_WEB_UI=1` to `JUCERPROJECT@defines` and run `--resave`. In the plugin, put it on a new line (`&#10;`) after the existing `JUCE_VST3_CAN_REPLACE_VST2=0`. The parser splits the defines on whitespace, newlines and commas, like Projucer.

## Open Questions

- [x] Spec and design use the same adapter error tokens (reconciled 2026-10-03).
- [ ] A Visual Studio fast up-to-date check can skip the pre-build after edits made only under `ui/`. Use Rebuild in that case. Accepted for the PoC.
