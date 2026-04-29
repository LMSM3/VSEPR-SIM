#!/usr/bin/env pwsh
# =========================================================================
# bin2var.ps1 - Wrap raw Z80 binary into TI-83+/84+ .8xp container
# =========================================================================
#
# Implements the TI variable file format (Commandments 5, 6, 15, 16).
#
# TI-83F .8xp structure:
#   [0..7]    8 bytes   "**TI83F*"  signature
#   [8..9]    2 bytes   0x1A 0x0A   (further signature)
#   [10..17]  8 bytes   0x00        (reserved / comment padding)
#   [18..52]  42 bytes  comment     (zero-padded ASCII)
#   [53..54]  2 bytes   data_length (LE, everything after this field to checksum)
#   --- data section ---
#   [55..56]  2 bytes   0x0D 0x00   (always, data sub-header)
#   [57..58]  2 bytes   payload_len + 2 (LE)
#   [59]      1 byte    type_id     (0x06 = protected program)
#   [60..67]  8 bytes   var name    (zero-padded, uppercase)
#   [68]      1 byte    version     (0x00)
#   [69]      1 byte    flag        (0x00 for RAM, 0x80 for archived)
#   [70..71]  2 bytes   payload_len + 2 (LE, again)
#   [72..73]  2 bytes   payload_len (LE, the raw size prefix inside var)
#   [74..N]   N bytes   payload     (raw Z80 binary)
#   [N+1..N+2] 2 bytes  checksum    (LE, sum of data section bytes)
#
# Usage:
#   bin2var.ps1 -BinFile build\HELLO.bin -OutFile out\HELLO.8xp -Name HELLO
#
# =========================================================================

param(
    [Parameter(Mandatory=$true)]
    [string]$BinFile,

    [Parameter(Mandatory=$true)]
    [string]$OutFile,

    [Parameter(Mandatory=$true)]
    [string]$Name,

    [switch]$Archived
)

$ErrorActionPreference = "Stop"

# --- Read payload ---
$payload = [System.IO.File]::ReadAllBytes($BinFile)
$payloadLen = $payload.Length

if ($payloadLen -eq 0) {
    Write-Host "ERR: Binary file is empty" -ForegroundColor Red
    exit 1
}
if ($payloadLen -gt 65000) {
    Write-Host "ERR: Binary too large ($payloadLen bytes). Max ~64KB for program variable." -ForegroundColor Red
    exit 1
}

# --- Name validation (Commandment 7) ---
$Name = $Name.ToUpper()
if ($Name.Length -gt 8) { $Name = $Name.Substring(0, 8) }
$nameBytes = [byte[]]::new(8)
$asciiName = [System.Text.Encoding]::ASCII.GetBytes($Name)
[Array]::Copy($asciiName, $nameBytes, [Math]::Min($asciiName.Length, 8))

# --- Build file (Commandments 5, 6, 15) ---

$ms = [System.IO.MemoryStream]::new()
$bw = [System.IO.BinaryWriter]::new($ms)

# Signature (10 bytes)
$bw.Write([System.Text.Encoding]::ASCII.GetBytes("**TI83F*"))
$bw.Write([byte]0x1A)
$bw.Write([byte]0x0A)

# Comment area (42 bytes, zero-padded)
$comment = "Built by tibuild"
$commentBytes = [byte[]]::new(42)
$commentAscii = [System.Text.Encoding]::ASCII.GetBytes($comment)
[Array]::Copy($commentAscii, $commentBytes, [Math]::Min($commentAscii.Length, 42))
$bw.Write($commentBytes)

# Padding (8 bytes, part of header in some interpretations)
# Actually the header is 53 bytes total before data_length
# We've written 10 + 42 = 52 bytes. Need 1 more for the 53-byte header.
# Then data_length (2 bytes).

# Correction: standard format is exactly:
# 8 (sig) + 3 (0x1A,0x0A,0x00) + 42 (comment) = 53 bytes before data_length
$bw.Write([byte]0x00)  # the extra 0x00 after 0x1A,0x0A

# Data length: from after this field to just before checksum
# Data section = 2 (sub-header 0x0D,0x00) + 2 (var_len) + 1 (type)
#              + 8 (name) + 1 (version) + 1 (flag) + 2 (var_len again)
#              + 2 (payload_size_prefix) + payloadLen
# = 19 + payloadLen
$dataLen = 19 + $payloadLen
$bw.Write([uint16]$dataLen)

# --- Data section (this is what gets checksummed) ---
$dataStart = $ms.Position

# Sub-header
$bw.Write([byte]0x0D)
$bw.Write([byte]0x00)

# Variable entry length (payload + 2 for size prefix)
$varLen = $payloadLen + 2
$bw.Write([uint16]$varLen)

# Type ID: 0x06 = protected program, 0x05 = program
$bw.Write([byte]0x06)

# Variable name (8 bytes)
$bw.Write($nameBytes)

# Version
$bw.Write([byte]0x00)

# Flag: 0x00 = RAM, 0x80 = archived
if ($Archived) { $bw.Write([byte]0x80) } else { $bw.Write([byte]0x00) }

# Variable entry length again
$bw.Write([uint16]$varLen)

# Payload size prefix (the TI variable's internal size field)
$bw.Write([uint16]$payloadLen)

# Payload (raw Z80 binary — Commandment 15: byte-exact, no conversions)
$bw.Write($payload)

$dataEnd = $ms.Position

# --- Checksum (Commandment 16) ---
$allBytes = $ms.ToArray()
$checksum = 0
for ($i = $dataStart; $i -lt $dataEnd; $i++) {
    $checksum = ($checksum + $allBytes[$i]) -band 0xFFFF
}
$bw.Write([uint16]$checksum)

# --- Write output ---
$bw.Flush()
$result = $ms.ToArray()
[System.IO.File]::WriteAllBytes($OutFile, $result)

$bw.Close()
$ms.Close()

Write-Host "  bin2var: $BinFile ($payloadLen bytes) -> $OutFile ($($result.Length) bytes)"
Write-Host "  Name: $Name  Type: ProtectedProgram  Checksum: 0x$($checksum.ToString('X4'))"
