# 🚀 VSEPR Command Quick Setup

## Problem
After running `bash vseprW.sh` and seeing those awesome commands:
```bash
▶  vsepr build random --watch      ← Try this first!
▶  vsepr build discover --thermal  ← Advanced!
```

But then: `vsepr: command not found` 😢

## Solution: Activate the Environment

### Option 1: Quick Activation (This Session Only)
```bash
# In WSL or Git Bash:
source activate.sh

# Now you can run:
vsepr build random --watch
vsepr build discover --thermal
vsepr build H2O --optimize --viz
```

### Option 2: Permanent Activation (Recommended)
```bash
# Add to your ~/.bashrc (WSL/Linux)
echo "source ~/Desktop/vsepr-sim/activate.sh" >> ~/.bashrc
source ~/.bashrc

# Or for Git Bash (Windows)
echo "source /c/Users/Liam/Desktop/vsepr-sim/activate.sh" >> ~/.bashrc
source ~/.bashrc
```

### Option 3: Direct Execution (No Activation Needed)
```bash
# Use ./vsepr instead:
./vsepr build random --watch
./vsepr build discover --thermal
./vsepr build H2O --optimize --viz

# Or full path:
/mnt/c/Users/Liam/Desktop/vsepr-sim/vsepr build random --watch
```

### Option 4: Add to PATH
```bash
# Temporary (this session):
export PATH="$PATH:/mnt/c/Users/Liam/Desktop/vsepr-sim"
vsepr build random --watch

# Permanent (add to ~/.bashrc):
echo 'export PATH="$PATH:/mnt/c/Users/Liam/Desktop/vsepr-sim"' >> ~/.bashrc
```

## Windows Users (PowerShell/CMD)

```powershell
# PowerShell - use vsepr.bat:
vsepr.bat build random --watch
vsepr.bat build discover --thermal

# Or create alias:
Set-Alias vsepr "C:\Users\Liam\Desktop\vsepr-sim\vsepr.bat"
vsepr build random --watch
```

## Verification

After activation, test:
```bash
vsepr --help                    # Should show help
vsepr build random --watch       # Interactive mode
vsepr test --all                # Run tests
```

---

**Recommended for WSL/Linux**: Use **Option 1** (quick source) or **Option 2** (permanent)  
**Recommended for Windows**: Use **vsepr.bat** directly or create PowerShell alias
