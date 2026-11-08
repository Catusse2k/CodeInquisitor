# CodeInquisitor

**Static triage for Windows PE files - directly in your terminal.**

CodeInquisitor is a console application that performs deep **static analysis**
of Windows Portable Executable files (`.exe`, `.dll`, `.sys`). It parses the
binary structure, profiles imports and exports, measures entropy, detects
packing and suspicious patterns, and produces a heuristic suspicion score -
all without ever executing the target.

The terminal interface is built on the excellent
[cpp-tui](https://github.com/jonoton/cpp-tui) single-header library by
**jonoton** (MIT) - big thanks to the author for making this possible.

> **Never executed.** CodeInquisitor only reads bytes from disk. The analyzed
> file is treated as inert data - no code inside it is ever run.

[![Version](https://img.shields.io/badge/version-1.0.0-blue)]()
[![License](https://img.shields.io/badge/license-MIT-green)]()
[![Platform](https://img.shields.io/badge/platform-Windows-lightgrey)]()
[![Language](https://img.shields.io/badge/language-C%2B%2B17-orange)]()
[![UI](https://img.shields.io/badge/UI-cpp--tui-purple)]()

---

## Table of Contents

- [Features](#features)
- [Screenshots](#screenshots)
- [Requirements](#requirements)
- [Building](#building)
  - [Command line (g++)](#command-line-g)
  - [Makefile](#makefile)
  - [Embarcadero Dev-C++](#embarcadero-dev-c)
- [Usage](#usage)
  - [Interactive mode](#interactive-mode)
  - [Command-line mode](#command-line-mode)
  - [Batch reports](#batch-reports)
- [Opening a file](#opening-a-file)
- [The interface](#the-interface)
- [Keyboard shortcuts](#keyboard-shortcuts)
- [Views](#views)
- [What gets analyzed](#what-gets-analyzed)
- [Exported report](#exported-report)
- [Project structure](#project-structure)
- [Troubleshooting](#troubleshooting)
- [Limitations](#limitations)
- [Credits & License](#credits--license)

---

## Features

### PE structure parsing

- DOS header, PE signature, COFF header and Optional header (PE32 and PE32+)
- All 16 data directories with RVA/size resolution to raw file offsets
- Section table: virtual/raw addresses, sizes, characteristics, permissions
- ImageBase, EntryPoint, SizeOfImage, timestamps, checksums, subsystem,
  machine type and DLL characteristics

### Imports & exports

- Regular **and delay-load** import tables, grouped by DLL
- Import-by-ordinal detection
- **imphash** computation (MD5 of the import list, sorted and normalized)
- Export table with ordinal, RVA, name and **forwarder** resolution
- Suspicious API classification across categories: process injection,
  anti-debug, persistence, network, cryptography, memory manipulation...

### Entropy & packing analysis

- Shannon entropy of the whole file and of every section
- Per-section 4 KB chunk histogram - detects uniformly high-entropy
  regions typical of packed or encrypted payloads
- Entropy transitions inside sections (encrypted tail / appended data)
- Overlay detection and entropy profiling
- Known-packer signatures (UPX and similar) and W+X section flags

### Anomaly detection & suspicion score

A heuristic engine inspects the binary for dozens of conditions and assigns
a **suspicion index from 0 to 100**:

- Overlapping or malformed sections, sections inside headers,
  sections beyond `SizeOfImage`, duplicate section names
- Entry point outside any section or inside an unusual section
- TLS callbacks (code that runs before `main`)
- Missing or forged Rich header, abnormal timestamps
- Imports located in unexpected sections, ordinal-only imports
- Non-standard DLL names, suspicious API combinations
- Authenticode signature presence check, PDB path extraction

Findings are tagged by severity: `INFO`, `LOW`, `MEDIUM`, `HIGH`.

### Inspection tools

- Interactive **hex viewer** with offset/RVA jump (`F7`)
- ASCII and UTF-16LE **string extraction** with live filter and
  minimum-length control
- Structural validation view (header consistency, alignment checks)
- MD5, SHA-256 and imphash **clipboard copy** (`F8`)

### Interface

- Full-screen interactive TUI with 10 tabbed views
- Mouse support: click, double-click, scroll, drag & drop of PE files
- 5 color themes (Dark, Light, Nord, Tokyo Night, Solarized Light)
- Startup splash screen, notifications, modal dialogs
- Plain-text report export (`F4`) or headless `--report` mode

---

## Screenshots

> *(Placeholder - drop your captures here, e.g. `docs/splash.png`,
> `docs/home.png`, `docs/overview.png`, `docs/anomalies.png`)*

| Splash | Home | Overview |
|---|---|---|
| ![splash](docs/splash.png) | ![home](docs/home.png) | ![overview](docs/overview.png) |

| Anomalies | Strings | Hex |
|---|---|---|
| ![anomalies](docs/anomalies.png) | ![strings](docs/strings.png) | ![hex](docs/hex.png) |

---

## Requirements

| Requirement | Notes |
|---|---|
| OS | Windows 10 / 11 (the tool analyzes Windows PE files) |
| Compiler | C++17 - tested with TDM-GCC 9.2 (MinGW-w64, bundled with Dev-C++) |
| Terminal | Any Windows console / terminal (conhost, Windows Terminal) |
| Dependencies | **None** - single-header UI library, no external packages |

The code is standard C++17 plus the Win32 API for clipboard and console
handling. It builds with any MinGW-w64 or MSVC-compatible toolchain that
supports C++17.

---

## Building

### Command line (g++)

From a MinGW-w64 / TDM-GCC prompt inside the project folder:

```bat
cd /d C:\path\to\CodeInquisitor
g++ -std=c++17 -O2 src\main.cpp -o CodeInquisitor.exe
```

Or with the bundled compiler shipped with Dev-C++:

```bat
"C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin\g++.exe" -std=c++17 -O2 src\main.cpp -o CodeInquisitor.exe
```

### Makefile

`Makefile.win` contains ready-to-use rules for the TDM-GCC toolchain
(including the include/library paths). Build and clean:

```bat
set PATH=C:\Program Files (x86)\Embarcadero\Dev-Cpp\TDM-GCC-64\bin;%PATH%
mingw32-make -f Makefile.win
mingw32-make -f Makefile.win clean
```

### Embarcadero Dev-C++

1. Install [Dev-C++](https://www.embarcadero.com/products/dev-cpp) (the free
   version bundles TDM-GCC 9.2).
2. Open the project file **`CodeInquisitor.dev`** with *File -> Open Project*.
3. In the project panel only **`src/main.cpp`** must be listed. Do **not**
   add `include/cpptui.hpp` - it is a header included automatically by
   `main.cpp`. If it appears in the project, right-click -> *Remove from
   project*.
4. Click the `main.cpp` tab, then press **Ctrl+F9** (Compile & Run) or
   **F11**.

> The project is configured as a **Console Application** and uses the
> custom `Makefile.win` (`UseCustomMakefile=1`), so no manual linker flags
> are needed.

---

## Usage

### Interactive mode

```bat
CodeInquisitor.exe
CodeInquisitor.exe "C:\path\to\sample.exe"   :: optional: pre-load a file
```

At startup a splash screen is shown for ~3 seconds - press any key to skip
it. You land on the **Home** screen with a quick-start guide and shortcut
tables.

### Opening a file

There are four ways to load a PE file:

| Method | How |
|---|---|
| **F2 dialog** | Press `F2`, then type or paste a full/relative path and hit `Enter`. The dialog also includes a file browser starting from *This PC* - navigate drives and folders with arrows, `Enter` to enter/select, `Left` to go up. |
| **Drag & drop** | Drag an `.exe`, `.dll` or `.sys` file from Explorer onto the terminal window. Works from any folder or drive. |
| **Paste path** | Copy a file path anywhere and paste it (`Ctrl+V`) - if it points to an existing file, it is loaded automatically. |
| **Command line** | Pass the path as the first argument: `CodeInquisitor.exe file.dll` |

Analysis runs on a background thread; a notification reports completion.

### Command-line mode

```bat
CodeInquisitor.exe --help
CodeInquisitor.exe --report <file> [out.txt]
```

`--report` analyzes the file and writes a plain-text report **without
opening the TUI** - ideal for scripts and batch triage:

```bat
CodeInquisitor.exe --report C:\Windows\System32\kernel32.dll kernel32_report.txt
CodeInquisitor.exe --report sample.exe                     :: writes sample.exe.codeinquisitor.txt
```

Exit codes: `0` success, `1` missing argument, `2` unreadable/non-PE file,
`3` report write failure.

---

## The interface

```
+ File      Views      Help ----------------------------------------------+
| <file name>  <type> | <machine> | <size> | <N> sections | <N> imports |
+-------------------------------------------------------------------------+
|                                                                         |
|                          <active tab view>                              |
|                                                                         |
+ F1 Help   F2 Open   F4 Report   F5 Theme   F6 Home   1-0 Views  F12 Quit+
+-------------------------------------------------------------------------+
```

- **Menu bar** - `File` (open/reload/export/home/goto/copy/theme/help/about/exit),
  `Views` (jump to any tab), `Help` (about).
- **Header** - current file, PE type, architecture, size, import count and
  the live suspicion score.
- **Content** - the 10 tabbed views (see below).
- **Footer** - the most common shortcuts plus author credit.

---

## Keyboard shortcuts

| Key | Action |
|---|---|
| `F1` | Keyboard shortcuts help |
| `F2` | Open file dialog (browse, type or paste a path) |
| `F3` | Reload the current file analysis |
| `F4` | Export analysis report (.txt) |
| `F5` | Cycle color theme (5 themes) |
| `F6` / `F11` | **Home** - close the file, show the welcome screen |
| `F7` | Go to offset / RVA (hex view) |
| `F8` | Copy hashes to clipboard (MD5, SHA-256, imphash) |
| `F9` / `F10` | Previous / next view |
| `F12` | Quit (`Ctrl+Q` also works) |
| `1` ... `0` | Jump directly to views 1-10 |
| `Enter` | Confirm dialog / submit path |
| `Esc` | Close dialog / cancel |
| `Tab` / `Shift+Tab` | Move focus between fields and buttons |
| Arrows, `PgUp`/`PgDn`, `Home`/`End` | Navigate lists, tables and the file browser |
| Mouse | Click, double-click, scroll, drag & drop |

> Digits `1`-`0` switch views only when no text field is focused, so they
> can still be typed inside inputs (offset jump, string filter, path field).

---

## Views

| # | Tab | Content |
|---|---|---|
| 1 | **Overview** | File hashes (MD5, SHA-256, imphash), PE type, file entropy, size, architecture, subsystem, image base, entry point, timestamp, checksum, characteristics |
| 2 | **Sections** | Section table: name, virtual/raw addresses and sizes, permissions (R/W/X), Shannon entropy, notes (entry point marker, anomalies) |
| 3 | **Directories** | All 16 data directories with RVA/size and resolution status |
| 4 | **Imports** | Functions grouped by DLL (regular + delay-load), ordinals, category classification |
| 5 | **Exports** | Export ordinals, RVAs, names and forwarder targets |
| 6 | **Entropy** | Per-section entropy chart, 4 KB block distribution, overlay entropy |
| 7 | **Anomalies** | Heuristic findings sorted by severity with explanatory details |
| 8 | **Structure** | Header and section consistency checks (overlaps, alignments, hidden gaps) |
| 9 | **Hex** | Hex dump of the file; use `F7` to jump to any offset or RVA |
| 10 | **Strings** | Extracted ASCII and UTF-16LE strings with filter and minimum length |

---

## What gets analyzed

For every loaded file CodeInquisitor computes:

- **Identity** - path, size, MD5, SHA-256, PE type (EXE/DLL/SYS/driver),
  architecture (x86/x64/ARM), subsystem
- **Headers** - COFF and Optional header fields, characteristics,
  DLL characteristics, timestamp, image base, entry point
- **Sections** - layout, permissions, entropy, packing indicators
- **Imports** - full import and delay-load tables, imphash,
  ordinal imports, suspicious API hits
- **Exports** - names, ordinals, RVAs, forwarders
- **Directories** - all data directories (export, import, resource,
  exception, security, basereloc, debug, architecture, globalptr, tls,
  load_config, bound_import, iat, delay_import, clr)
- **Integrity** - PE checksum vs. computed checksum, Authenticode
  signature presence, PDB path
- **Anomalies** - dozens of structural and behavioral heuristics
  aggregated into the 0-100 suspicion score

---

## Exported report

`F4` (or `--report`) produces a plain-text `.txt` report containing:

```
FILE            path, size, MD5, SHA-256, imphash, type, entropy
HEADER          architecture, subsystem, imagebase, entry point,
                sizeofimage, timestamp, checksum, characteristics
SECTIONS        per-section VA/VS/RAW/SIZE, permissions, entropy, <EP>
DATA DIRECTORIES  every populated directory with RVA and size
IMPORT          functions grouped by DLL (delay-load marked)
EXPORT          ordinals, RVAs, names, forwarders
ENTROPY         file entropy, per-section values, 4K high-entropy
                block counts, overlay entropy
SUSPICION INDEX score /100 and label
FINDINGS        every anomaly tagged [INFO]/[LOW]/[MEDIUM]/[HIGH]
HEX DUMP        first 256 bytes of the file
```

---

## Project structure

```
CodeInquisitor/
+-- include/
|   +-- cpptui.hpp          # TUI library (single header, MIT)
+-- src/
|   +-- main.cpp            # PE parser, heuristics, report, TUI app
+-- Makefile.win            # Build rules for TDM-GCC / Dev-C++
+-- CodeInquisitor.dev      # Dev-C++ project file (optional)
+-- README.md
+-- .gitignore
```

Everything lives in `src/main.cpp` (~4 000 lines): the PE parser, the
heuristic engine, the report writer and the application UI. The only
dependency is `include/cpptui.hpp`, a single-header C++17 TUI library
([jonoton/cpp-tui](https://github.com/jonoton/cpp-tui), MIT).

---

## Troubleshooting

| Problem | Solution |
|---|---|
| `undefined reference to WinMain` | The project must be a **Console Application**, not a GUI app. In Dev-C++ use the provided `CodeInquisitor.dev` (already configured) or compile from the command line. |
| `#pragma once in main file` | You are compiling `cpptui.hpp` directly. Only `src/main.cpp` is a source file - the header is included automatically. |
| Function keys do nothing | Some terminals need *VT input* enabled. Windows Terminal and recent conhost handle it automatically; if keys are ignored, try running inside Windows Terminal. |
| Drag & drop doesn't load | The drop works by detecting a valid file path - make sure you drop the file onto the terminal window itself, and that the file is `.exe`, `.dll` or `.sys`. |
| Screen looks garbled | Resize the terminal window - the TUI adapts to the current size. A minimum of ~80x24 columns is recommended. |
| Report says `n/a` for imphash | The file has no imports (e.g. a driver or packed binary) - this is normal. |

---

## Limitations

- **Static analysis only** - CodeInquisitor never executes, debugs or
  instruments the target. It does not replace dynamic analysis, sandboxing
  or AV engines.
- **No disassembly/decompilation** - it maps structure and behavior hints,
  not instruction-level semantics.
- Packed or heavily obfuscated binaries may show high entropy and few
  meaningful imports - expected, and itself a useful signal.
- The suspicion score is a **triage heuristic**, not a verdict: always
  corroborate findings with other tools before drawing conclusions.

---

## Credits & License

- **CodeInquisitor** - developed by **Catusse**
- **Terminal UI framework** - the entire TUI is built on
  [cpp-tui](https://github.com/jonoton/cpp-tui), a single-header C++17
  library created by **jonoton**. Vendored here as `include/cpptui.hpp`
  under the MIT license. Special thanks to jonoton for this great project.
- PE parsing, heuristic engine, report generator and application logic
  are original work written for CodeInquisitor.

Released under the **MIT License**:

```
MIT License

Copyright (c) 2026 Catusse

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
