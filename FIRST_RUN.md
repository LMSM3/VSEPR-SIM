# ⚡ VSEPR-Sim First Run Guide

## 30-Second Quickstart

### WSL / Linux / macOS
```bash
# 1. Build (one time)
./build.sh --clean

# 2. Activate (adds 'vsepr' command)
source activate.sh

# 3. Run something cool!
vsepr build random --watch
```

### Windows (PowerShell)
```powershell
# 1. Build (one time)
.\build.ps1 -Clean

# 2. Run something cool!
.\vsepr.bat build random --watch
```

---

## What Just Happened?

### `vsepr build random --watch`
- Generates a random molecule
- Opens **live 3D visualization**
- You can rotate, zoom, explore
- Watch geometry optimize in real-time

### `vsepr build discover --thermal`
- Builds **100 random molecules**
- Runs HGST (High-Gradient Structure Traversal)
- Computes thermal eigenvalues
- Saves all data for ML training

---

## More Cool Commands

```bash
# Classic molecules with visualization
vsepr build H2O --optimize --viz
vsepr build NH3 --optimize --viz
vsepr build CH4 --optimize --viz

# Batch processing
vsepr batch examples/molecules/*.xyz --output artifacts/

# Run tests
vsepr test all
```

---

## Need Help?

```bash
# General help
vsepr --help

# Command-specific help
vsepr build --help
vsepr optimize --help

# Run tests to verify installation
vsepr test --all
```

---

## Making 'vsepr' Permanent (Optional)

### WSL/Linux:
```bash
# Add to ~/.bashrc
echo "source $(pwd)/activate.sh" >> ~/.bashrc
source ~/.bashrc

# Now 'vsepr' works in every new terminal
```

### Windows PowerShell:
```powershell
# Create permanent alias (optional)
Set-Alias vsepr "C:\Users\Liam\Desktop\vsepr-sim\vsepr.bat"

# Or just use vsepr.bat directly
```

---

## What to Try Next

1. **Interactive Mode**: `vsepr build random --watch`
2. **Discovery Mode**: `vsepr build discover --thermal`
3. **Classic Molecules**: `vsepr build H2O --optimize --viz`
4. **Batch Processing**: `vsepr batch examples/molecules/*.xyz`
5. **Generate Training Data**: Run discovery mode overnight

---

## Documentation

- **[README.md](README.md)** - Full project overview
- **[QUICK_START.md](QUICK_START.md)** - Command reference
- **[docs/](docs/)** - Detailed technical docs
- **[examples/](examples/)** - Example molecules

---

**Ready to fill your hard drive with thermal eigen training data?** 🔥

Start with: `vsepr build discover --thermal`
