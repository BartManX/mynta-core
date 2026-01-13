# QT Wallet Rebranding Audit

**Date:** January 13, 2026  
**Status:** ❌ **FAIL - REQUIRES REMEDIATION**  
**Scope:** Visual assets, UI branding, source-level references, binary/installer presentation

---

## Executive Summary

The Mynta QT wallet contains **extensive residual Ravencoin branding** that must be remediated before launch. While some rebranding has occurred (app names partially updated), the majority of visual assets, UI strings, code symbols, and platform-specific metadata still reference "Raven" or "RVN".

**Critical Issues Found:** 47  
**Files Requiring Changes:** 150+  
**Assets Requiring Replacement:** 35+

---

## 1. Asset Replacement Requirements

### 1.1 Icon Files - ❌ FAIL

All icon files retain "raven" naming and likely contain Ravencoin imagery.

| Path | Dimensions | Status |
|------|------------|--------|
| `src/qt/res/icons/raven.png` | TBD | ❌ Must replace |
| `src/qt/res/icons/raven.ico` | Multi-res | ❌ Must replace |
| `src/qt/res/icons/raven.icns` | Multi-res | ❌ Must replace |
| `src/qt/res/icons/raven_testnet.ico` | Multi-res | ❌ Must replace |
| `src/qt/res/icons/ravencointext.png` | TBD | ❌ Must replace |
| `src/qt/res/icons/rvntext.png` | TBD | ❌ Must replace |
| `src/qt/res/src/raven.svg` | Vector | ❌ Must replace |

### 1.2 Installer Graphics (Windows) - ❌ FAIL

| Path | Dimensions | Status |
|------|------------|--------|
| `share/pixmaps/raven.ico` | Multi-res | ❌ Must replace |
| `share/pixmaps/raven.BMP` | TBD | ❌ Must replace |
| `share/pixmaps/raven16.png` | 16x16 | ❌ Must replace |
| `share/pixmaps/raven32.png` | 32x32 | ❌ Must replace |
| `share/pixmaps/raven64.png` | 64x64 | ❌ Must replace |
| `share/pixmaps/raven128.png` | 128x128 | ❌ Must replace |
| `share/pixmaps/raven256.png` | 256x256 | ❌ Must replace |
| `share/pixmaps/raven*.xpm` | Various | ❌ Must replace |
| `share/pixmaps/nsis-header.bmp` | 150x57 | ⚠️ Review content |
| `share/pixmaps/nsis-wizard.bmp` | 164x314 | ⚠️ Review content |

### 1.3 Resource Files - ❌ FAIL

| File | Issue |
|------|-------|
| `src/qt/raven.qrc` | References `raven` icon alias |
| `src/qt/raven_locale.qrc` | References `raven_*.qm` locale files |
| `src/qt/res/raven-qt-res.rc` | Windows metadata references "Raven" |

---

## 2. Upstream Branding Removal

### 2.1 Source Files with "Raven" References - ❌ FAIL

**70+ .cpp files** and **72+ .h files** contain "Raven" or "raven" references.

#### Critical UI-Facing References:

| File | Line | Issue |
|------|------|-------|
| `guiconstants.h:117` | `QAPP_ORG_NAME "Raven"` | ❌ Organization name |
| `guiconstants.h:118` | `QAPP_ORG_DOMAIN "raven.org"` | ❌ Domain |
| `guiconstants.h:47-92` | `/* Ravencoin dark orange */` etc. | ⚠️ Comments |
| `ravenunits.h:44-46` | `RVN, mRVN, uRVN` enum values | ❌ Unit symbols |
| `ravenunits.cpp:44-57` | `"RVN"`, `"Ravens"` strings | ❌ Display text |
| `utilitydialog.cpp:74` | `"raven-qt"` in usage text | ❌ Binary name |
| `utilitydialog.cpp:81` | `HMM_RAVEN_QT` constant | ❌ Constant name |
| `ravengui.cpp:706` | `"Ravencoin Market Price"` | ❌ UI label |
| `ravengui.cpp:725,899` | GitHub RavenProject URLs | ❌ Links |
| `ravengui.cpp:379` | `"Transfer assets to RVN addresses"` | ❌ Tooltip |
| `networkstyle.cpp:31` | `QPixmap(":/icons/raven")` | ❌ Icon path |

### 2.2 UI Form Files (.ui) - ❌ FAIL

**103+ occurrences** across UI forms:

| Form File | Issue Examples |
|-----------|----------------|
| `signverifymessagedialog.ui` | "Raven address", "Raven network" |
| `sendcoinsdialog.ui` | "raven transactions", `RavenAmountField` |
| `sendcoinsentry.ui` | "raven: URI", "Raven address", "Raven network" |
| `sendassetsentry.ui` | "Raven address", "Raven network" |
| `receivecoinsdialog.ui` | "Raven network" |
| `overviewpage.ui` | "Raven network" |
| `modaloverlay.ui` | "raven network", "ravens" |
| `optionsdialog.ui` | "Raven network", "Raven client" |
| `mnemonicdialog1.ui` | "Raven block chain" |
| `mnemonicdialog2.ui` | "Ravencoin and Assets" |
| `helpmessagedialog.ui` | `:/icons/raven` resource |

### 2.3 Transaction/Asset Code - ❌ FAIL

| File | Issue |
|------|-------|
| `transactionrecord.cpp` | `"RVN"` hardcoded 15+ times |
| `transactiondesc.cpp` | `"Net RVN amount"` |
| `transactiontablemodel.cpp` | `"The asset (or RVN)"` |
| `createassetdialog.cpp` | `"RVN"` in burn amount display |
| `reissueassetdialog.cpp` | `"RVN"` in burn amount |

### 2.4 Class/File Naming - ❌ FAIL

Files and classes that should be renamed:

| Current | Should Be |
|---------|-----------|
| `ravengui.cpp/.h` | `myntagui.cpp/.h` |
| `ravenunits.cpp/.h` | `myntaunits.cpp/.h` |
| `ravenamountfield.cpp/.h` | `myntaamountfield.cpp/.h` |
| `ravenaddressvalidator.cpp/.h` | `myntaaddressvalidator.cpp/.h` |
| `ravenstrings.cpp` | `myntastrings.cpp` |
| `raven.cpp` | `mynta.cpp` |
| `RavenGUI` class | `MyntaGUI` class |
| `RavenUnits` class | `MyntaUnits` class |
| `RavenAmountField` class | `MyntaAmountField` class |

---

## 3. Application Identity Consistency

### 3.1 Current State - ⚠️ PARTIAL

| Element | Current Value | Status |
|---------|---------------|--------|
| App Name (main) | `Mynta-Qt` | ✅ OK |
| App Name (testnet) | `Mynta-Qt-testnet` | ✅ OK |
| Organization Name | `Raven` | ❌ FAIL |
| Organization Domain | `raven.org` | ❌ FAIL |
| Ticker Symbol | `RVN` | ❌ FAIL |
| Unit Names | `Ravens`, `mRVN`, `uRVN` | ❌ FAIL |

### 3.2 Unit Symbol Requirements

| Current | Required |
|---------|----------|
| `RVN` | `MYNTA` |
| `mRVN` | `mMYNTA` |
| `uRVN` | `μMYNTA` |
| `Ravens` | `Mynta` |
| `Milli-Ravens` | `Milli-Mynta` |
| `Micro-Ravens` | `Micro-Mynta` |

---

## 4. Platform-Specific Checks

### 4.1 Windows - ❌ FAIL

| Element | Issue |
|---------|-------|
| `raven-qt-res.rc:22` | `CompanyName "Raven"` |
| `raven-qt-res.rc:23` | `"GUI node for Raven"` |
| `raven-qt-res.rc:25` | `InternalName "raven-qt"` |
| `raven-qt-res.rc:28` | `OriginalFilename "raven-qt.exe"` |
| `setup.nsi:12-13` | References `raven.ico` |
| `setup.nsi:53-56` | `InstallDir $PROGRAMFILES\Raven` |

### 4.2 macOS - ❌ FAIL

| Element | Issue |
|---------|-------|
| `res/icons/raven.icns` | File named with "raven" |
| Bundle identifier | Likely uses raven namespace |

### 4.3 Linux - ❌ FAIL

| Element | Issue |
|---------|-------|
| `raven-qt.desktop:3` | `Name=Raven Core` |
| `raven-qt.desktop:4-7` | Comments reference "Raven" |
| `raven-qt.desktop:8` | `Exec=raven-qt` |
| `raven-qt.desktop:11` | `Icon=raven128` |
| `raven-qt.desktop:12` | `MimeType=x-scheme-handler/raven` |

---

## 5. External Links/URLs - ❌ FAIL

| File | Line | URL |
|------|------|-----|
| `ravengui.cpp:725` | `github.com/RavenProject/Ravencoin/releases` |
| `ravengui.cpp:899` | `github.com/RavenProject/Ravencoin/releases` |
| `ravengui.cpp:1896` | `api.github.com/repos/RavenProject/Ravencoin/releases` |
| `ravengui.cpp:111-115` | `RVNBTC`, `RVNUSDT` trading pair symbols |

---

## 6. Verification Checklist

| Requirement | Status |
|-------------|--------|
| No Ravencoin branding visible anywhere in the UI | ❌ FAIL |
| All original images replaced with Mynta originals | ❌ FAIL |
| Wallet launches clean with Mynta identity from first frame | ❌ FAIL |
| No placeholder or upstream logos remain | ❌ FAIL |
| Installer and binary metadata reflect Mynta | ❌ FAIL |

---

## 7. Required Remediation Actions

### Priority 1: Critical (Blocks Launch)

1. **Replace all icon assets** (35+ files)
   - Create Mynta versions matching exact dimensions
   - Update file references in .qrc files

2. **Update guiconstants.h**
   - Change `QAPP_ORG_NAME` to `"Mynta"`
   - Change `QAPP_ORG_DOMAIN` to `"mynta.network"`

3. **Update unit symbols**
   - Rename `RVN` → `MYNTA` throughout
   - Update `RavenUnits` class and all references

4. **Fix UI form strings**
   - Replace "Raven" with "Mynta" in all .ui files
   - Replace "ravens" with "mynta" in lowercase contexts

5. **Update Windows metadata**
   - Fix `raven-qt-res.rc` company info
   - Update `setup.nsi` installer script

6. **Update Linux desktop entry**
   - Rename and update `raven-qt.desktop`

### Priority 2: High (Professional Quality)

1. **Rename source files**
   - `ravengui.*` → `myntagui.*`
   - `ravenunits.*` → `myntaunits.*`
   - etc.

2. **Rename classes**
   - `RavenGUI` → `MyntaGUI`
   - `RavenUnits` → `MyntaUnits`
   - etc.

3. **Update external URLs**
   - Remove RavenProject GitHub references
   - Update to Mynta project URLs

### Priority 3: Cleanup

1. **Update code comments**
   - Replace "Ravencoin" in comments with "Mynta"
   
2. **Update copyright headers**
   - Add Mynta copyright line

---

## 8. Asset Inventory (Required Replacements)

| Asset Path | Dimensions | Format |
|------------|------------|--------|
| `src/qt/res/icons/raven.png` | TBD | PNG |
| `src/qt/res/icons/raven.ico` | 16,32,48,256 | ICO |
| `src/qt/res/icons/raven.icns` | 16-1024 | ICNS |
| `src/qt/res/icons/raven_testnet.ico` | 16,32,48,256 | ICO |
| `src/qt/res/icons/ravencointext.png` | TBD | PNG |
| `src/qt/res/icons/rvntext.png` | TBD | PNG |
| `src/qt/res/src/raven.svg` | Vector | SVG |
| `share/pixmaps/raven.ico` | Multi-res | ICO |
| `share/pixmaps/raven.BMP` | TBD | BMP |
| `share/pixmaps/raven16.png` | 16x16 | PNG |
| `share/pixmaps/raven16.xpm` | 16x16 | XPM |
| `share/pixmaps/raven32.png` | 32x32 | PNG |
| `share/pixmaps/raven32.xpm` | 32x32 | XPM |
| `share/pixmaps/raven64.png` | 64x64 | PNG |
| `share/pixmaps/raven64.xpm` | 64x64 | XPM |
| `share/pixmaps/raven128.png` | 128x128 | PNG |
| `share/pixmaps/raven128.xpm` | 128x128 | XPM |
| `share/pixmaps/raven256.png` | 256x256 | PNG |
| `share/pixmaps/raven256.xpm` | 256x256 | XPM |
| `share/pixmaps/nsis-header.bmp` | 150x57 | BMP |
| `share/pixmaps/nsis-wizard.bmp` | 164x314 | BMP |

---

## 9. Sign-Off

**Audit Result:** ❌ **HARD STOP - DO NOT SHIP**

The QT wallet cannot be released in its current state. Extensive Ravencoin branding remains throughout the codebase, UI, and assets. Shipping this would:

1. Confuse users about the product identity
2. Potentially violate Ravencoin project trademarks
3. Undermine Mynta's professional legitimacy
4. Create support and documentation conflicts

**Estimated Remediation Effort:** 2-4 days for a complete rebrand

---

*This audit ensures the QT wallet stands on its own visually and professionally. Rebranding is not cosmetic—it's part of establishing trust, legitimacy, and long-term independence.*
