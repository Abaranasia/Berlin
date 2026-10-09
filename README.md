# Berlin

A Berlin School style step sequencer and generator, built with JUCE. It runs as a
standalone app and as a VST3 plugin, with an internal synth, MIDI output and MIDI
export. The user interface is a React app hosted in a WebView2 control and embedded
into the binary at build time.

## Build prerequisites

- Visual Studio 2026 with the "Desktop development with C++" workload.
- [Projucer](https://juce.com/) to regenerate the Visual Studio projects from the
  `.jucer` files (`Projucer --resave <file>.jucer`). Never edit the generated
  `.vcxproj` files by hand.
- [Node.js](https://nodejs.org/), available on `PATH`.
- [pnpm](https://pnpm.io/), available on `PATH` (`corepack enable` is enough on a
  recent Node.js).
- The WebView2 runtime. It ships with current Windows 10 and 11 and with Microsoft
  Edge.
- The `Microsoft.Web.WebView2` NuGet package, which Visual Studio restores when the
  solution is built.

Every build runs a pre-build step (`ui/scripts/embed-assets.mjs`) that installs the
UI dependencies, builds the React app and embeds the result into
`Source/ui/generated/` (git-ignored). It is skipped when the UI inputs have not
changed. If a tool is missing, the build fails with a clear diagnostic:

| Code | Meaning |
|---|---|
| `BRL001` | `node` was not found on `PATH` |
| `BRL002` | `pnpm` was not found on `PATH` |

## Building

- Standalone: open `Builds/VisualStudio2026/Berlin.sln`.
- VST3 plugin: open `Plugin/Builds/VisualStudio2026/Berlin.sln`.
- Unit tests: build `Tests/Builds/VisualStudio2026/BerlinTests.sln`, then run
  `BerlinTests.exe --category=Berlin`.
- UI unit tests: `pnpm --dir ui test`.
