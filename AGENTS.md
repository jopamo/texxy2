# Repository Guidelines

## Project Structure

* **`texxy/`** — Core Qt/C++ sources, widgets, and UI forms (`*.ui`).
  Includes subfolders such as:

  * `highlighter/` for syntax definitions
  * `data/` for bundled resources (icons, translations, etc.)
* **`cmake/Modules/`** — CMake helper scripts used by the top-level `CMakeLists.txt`.
* **`build/`** — Generated build output (safe to delete when switching toolchains).
* **`.github/workflows/`** — Continuous integration definitions; review these when modifying CI.

---

## Build, Test, and Run

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
./build/texxy/texxy    # run from the build tree
cmake --install build  # optional: install system-wide
```

* Use `-DCMAKE_BUILD_TYPE=Release` for production builds.
* Re-run CMake when adding new dependencies (e.g. Hunspell).

---

## Coding Style

* Format code using the repository’s `.clang-format`
  *(Chromium base, 4-space indent, 120-column limit, no include reordering)*.
* **Naming conventions**

  * **Classes / QObject types:** UpperCamelCase (`TexxyApplication`)
  * **Methods:** lowerCamelCase
  * **Free helpers / Qt-style APIs:** snake_case
* Braces stay on the same line as control statements.
* Keep `connect()` calls on a single line where readability allows.

---

## Testing

* No automated tests yet — perform **manual smoke tests** for major flows:

  * Launching, open/save, search, spell-check toggles, etc.
* For new features:

  * Include a brief manual test plan in the PR.
  * Optionally add Qt Test cases under `tests/` using `add_test`.
* Always verify CI still configures and builds cleanly before submission.

---

Here’s an improved section for **Commit & Pull Request Workflow** that provides a strong commit-style guideline tailored to your project. You can swap this into your repository guidelines.

---

Here’s a refined commit-message guideline based on your preferred format **(file-changed or one-word description): (more detailed description ...)**. Feel free to tweak for your project’s voice.

---

### Commit Message Style

```
<scope>: <short summary>

<optional body with more detail>
```

* **Scope** (file changed / one-word category)

  * Use the name of the file, module, or a single word that conveys the area of change (e.g., `highlighter`, `spellchecker`, `ui`, `build`, `ci`)
* **Short summary**

  * Describe the change in one sentence; use the imperative mood (e.g., `Update regex for HTML tag parsing`)
  * Keep it concise (≈ 50-72 characters) and **don’t** end with a period
* **Body (optional)**

  * Provide context: *why* you made the change, any side-effects, caveats
  * Wrap lines at ~72 characters
  * Skip describing *how* (the diff shows that) unless it clarifies non-obvious logic
* Example:

  ```
  highlighter: add XML CDATA section support

  Extend syntax definition so CDATA sections are recognised,
  and ensure the colour theme applies correctly. Closes #42.
  ```
* When you touch multiple files but the change is still logically one unit, choose an overarching scope (e.g., `ui`, `core`, `data`) rather than listing every file
* If the commit is trivial (e.g., formatting, renaming), keep the body minimal or skip it
* For breaking changes or new runtime requirements, mention in body or add a footer like `BREAKING: …`
