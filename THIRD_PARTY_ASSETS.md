# UI Asset Attribution

## Classic UI raster set

- **Source package:** VSEPR-SIM original artwork
- **Source path:** `tools/generate_classic_icons.py`
- **Qt/PyQt dependency:** None; PNG files are generated with the Python standard library.
- **License:** MIT, consistent with this repository's project licensing.
- **Purpose:** Stable internal UI resource aliases under `:/classic/...`.

| File | Alias | Purpose |
| --- | --- | --- |
| `checkmark.png` | `:/classic/checkmark.png` | Successful or passed status |
| `warning.png` | `:/classic/warning.png` | Recoverable issue |
| `critical.png` | `:/classic/critical.png` | Failure or invalid state |
| `information.png` | `:/classic/information.png` | Metadata and provenance |
| `question.png` | `:/classic/question.png` | Unknown or unresolved state |
| `copy.png` | `:/classic/copy.png` | Copy actions |
| `crosshairs.png` | `:/classic/crosshairs.png` | Target/focus actions |
| `checkers.png` | `:/classic/checkers.png` | Transparency/background display |
| `window_border.png` | `:/classic/window_border.png` | Window/chrome display |

These are original project assets, not copied Qt, PyQt, operating-system, or third-party icon-package files. Regenerate only through the checked-in generator and do not rename resource aliases without a compatibility review.
