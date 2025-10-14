<p align="center">
  <img src="texxy/data/texxy.svg" alt="Texxy logo" width="160">
</p>

# Texxy

A lightweight Qt plain-text editor for Linux — desktop-environment agnostic, fast to start, and focused on pragmatic editing workflows ✨

## Features ✨

- **Tabs & windows** 🗂️  
  - Drag-and-drop tabs between windows, including detachment and reattachment  
  - X11 virtual desktop awareness: open new windows on other desktops while keeping tabs on the current one  

- **Search & replace** 🔎✏️  
  - Optional always-visible search bar, one search entry per tab  
  - Instant match highlighting as you type  
  - Docked window for multi-step text replacement  
  - Jump to line  

- **Editing & viewing** 📝  
  - Line numbers and optional selection highlighting  
  - Column selection  
  - Syntax highlighting for common programming languages  
  - Text zoom  

- **Integration** 🌐  
  - Open URLs with the system’s default applications  
  - Spell checking via Hunspell  

- **Workflow & reliability** 🧭  
  - Session management  
  - Side-pane mode  
  - Auto-saving  
  - Printing  
  - Non-intrusive prompts designed to stay out of your way  

---

## Install 📦

### From source

**Requirements**
- Qt 6.2+ (Core, Gui, Widgets, Svg, PrintSupport, DBus)
- Hunspell 1.6+ for spell checking (optional but recommended)
- CMake 3.16+ and a C++17 compiler

**Build**
```bash
git clone https://example.com/texxy.git
cd texxy
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
sudo cmake --install build
````

> If Hunspell is not found, spell checking will be disabled at build time ℹ️

---

## Usage ▶️

```bash
texxy [file1 [file2 ...]]
```

* Drag a file from your file manager into Texxy to open it in a new tab
* Middle-click a tab to close it
* Detach a tab by dragging it out of the window

---

## Configuration ⚙️

Texxy uses Qt settings; most options are available from the UI. Power users can tune behavior (for example, visibility of the search bar, line numbers, side-pane mode) via the app’s settings dialogs 🧩

---

## Contributing 🤝

Bug reports and pull requests are welcome. If you plan a larger change:

* open an issue first to discuss the approach
* keep changes focused and include before/after behavior in the description
* follow the existing Qt/C++ style used in the codebase
* add or update tests if applicable

See `CONTRIBUTING.md` if present

---

## License 📄

See `LICENSE` for licensing terms

---

## Credits 🙏

Texxy is inspired by practical, minimal editors and uses Qt for its UI

* Original author: **Tsu Jan** [tsujan2000@gmail.com](mailto:tsujan2000@gmail.com)

