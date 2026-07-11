"""Fix PowerShell script encoding: strip non-ASCII, add UTF-8 BOM."""
import sys

path = sys.argv[1] if len(sys.argv) > 1 else "scripts/cfe_load.ps1"

with open(path, "rb") as f:
    raw = f.read()

# Decode as UTF-8 (the file was created UTF-8)
text = raw.decode("utf-8", errors="replace")

# Replace Unicode punctuation / box-drawing with ASCII equivalents
replacements = [
    ("\u2014", "--"),   # em dash
    ("\u2013", "-"),    # en dash
    ("\u2019", "'"),    # right single quote
    ("\u2018", "'"),    # left single quote
    ("\u201c", '"'),    # left double quote
    ("\u201d", '"'),    # right double quote
    ("\u25ba", ">"),    # right-pointing triangle
    # Box-drawing (light)
    ("\u2500", "-"), ("\u2502", "|"), ("\u250c", "+"), ("\u2510", "+"),
    ("\u2514", "+"), ("\u2518", "+"), ("\u251c", "+"), ("\u2524", "+"),
    ("\u252c", "+"), ("\u2534", "+"), ("\u253c", "+"),
    # Box-drawing (double)
    ("\u2550", "="), ("\u2551", "|"), ("\u2554", "+"), ("\u2557", "+"),
    ("\u255a", "+"), ("\u255d", "+"), ("\u2560", "+"), ("\u2563", "+"),
    ("\u2566", "+"), ("\u2569", "+"), ("\u256c", "+"),
    # Block elements
    ("\u2588", "#"), ("\u2584", "#"), ("\u2580", "#"), ("\u25a0", "#"),
    # Braille / spinner chars
    ("\u280b", "*"), ("\u2819", "*"), ("\u2839", "*"), ("\u2838", "*"),
    ("\u283c", "*"), ("\u2834", "*"), ("\u2826", "*"), ("\u2827", "*"),
    ("\u2807", "*"), ("\u280f", "*"),
]
for src, dst in replacements:
    text = text.replace(src, dst)

# Write with UTF-8 BOM so PowerShell reads it as UTF-8 on any locale
with open(path, "wb") as f:
    f.write(b"\xef\xbb\xbf")
    f.write(text.encode("utf-8"))

print(f"Fixed: {path}")
