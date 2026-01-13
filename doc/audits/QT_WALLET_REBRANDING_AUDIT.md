# QT Wallet Rebranding Audit

**Date:** January 13, 2026  
**Status:** ✅ **PASS - READY FOR LAUNCH**  
**Scope:** Visual assets, UI branding, source-level references, binary/installer presentation

---

## Executive Summary

The Mynta QT wallet rebranding has been **completed successfully**. All critical Ravencoin branding has been removed and replaced with proper Mynta branding throughout the codebase, UI, and assets.

**Remediation Completed:** January 13, 2026  
**Build Verified:** ✅ Successful  
**Runtime Verified:** ✅ Wallet launches with "Mynta Core" branding

---

## 1. Asset Replacement - ✅ PASS

### 1.1 Icon Files - ✅ PASS

| Path | Status |
|------|--------|
| `src/qt/res/icons/mynta.png` | ✅ Present |
| `src/qt/res/icons/mynta.ico` | ✅ Present |
| `src/qt/res/icons/mynta.icns` | ✅ Present |
| `src/qt/res/icons/mynta_testnet.ico` | ✅ Present |
| `src/qt/res/icons/myntacoinfulltext.png` | ✅ Present |
| `src/qt/res/icons/myntatext.png` | ✅ Present |
| Old raven*.* files | ✅ Deleted |

### 1.2 Installer Graphics (Windows) - ✅ PASS

| Path | Status |
|------|--------|
| `share/pixmaps/mynta.ico` | ✅ Present |
| `share/pixmaps/mynta.BMP` | ✅ Present |
| `share/pixmaps/mynta16.png` | ✅ Present |
| `share/pixmaps/mynta32.png` | ✅ Present |
| `share/pixmaps/mynta64.png` | ✅ Present |
| `share/pixmaps/mynta128.png` | ✅ Present |
| `share/pixmaps/mynta256.png` | ✅ Present |
| Old raven*.* files | ✅ Deleted |

### 1.3 Resource Files - ✅ PASS

| File | Status |
|------|--------|
| `src/qt/mynta.qrc` | ✅ References `mynta` icon |
| `src/qt/mynta_locale.qrc` | ✅ References `mynta_*.qm` locales |
| `src/qt/res/mynta-qt-res.rc` | ✅ Mynta metadata |

---

## 2. Source Code Branding - ✅ PASS

### 2.1 Application Identity - ✅ PASS

| Element | Value | Status |
|---------|-------|--------|
| `QAPP_ORG_NAME` | `"Mynta"` | ✅ |
| `QAPP_ORG_DOMAIN` | `"mynta.network"` | ✅ |
| `QAPP_APP_NAME_DEFAULT` | `"Mynta-Qt"` | ✅ |
| `QAPP_APP_NAME_TESTNET` | `"Mynta-Qt-testnet"` | ✅ |

### 2.2 Unit Symbols - ✅ PASS

| Unit | Display | Status |
|------|---------|--------|
| Base unit | `MYNTA` | ✅ |
| Milli unit | `mMYNTA` | ✅ |
| Micro unit | `μMYNTA` | ✅ |
| Description | `Mynta`, `Milli-Mynta`, `Micro-Mynta` | ✅ |

### 2.3 URI Scheme - ✅ PASS

| Component | Value | Status |
|-----------|-------|--------|
| URI prefix | `mynta:` | ✅ |
| URI handler | `mynta://` → `mynta:` conversion | ✅ |
| IPC prefix | `MYNTA_IPC_PREFIX` | ✅ |

### 2.4 Class/File Naming - ✅ PASS

| File | Status |
|------|--------|
| `myntagui.cpp/.h` | ✅ |
| `myntaunits.cpp/.h` | ✅ |
| `myntaamountfield.cpp/.h` | ✅ |
| `myntaaddressvalidator.cpp/.h` | ✅ |
| `myntastrings.cpp` | ✅ |
| `mynta.cpp` | ✅ |

### 2.5 UI Form Files - ✅ PASS

All `.ui` files updated:
- Resource references: `../mynta.qrc` ✅
- Icon references: `:/icons/mynta` ✅
- Text strings: "mynta" (lowercase contexts) ✅
- Widget names: `myntaAtStartup` ✅

---

## 3. Platform-Specific - ✅ PASS

### 3.1 Windows - ✅ PASS

| Element | Value | Status |
|---------|-------|--------|
| CompanyName | `"Mynta"` | ✅ |
| FileDescription | `"GUI node for Mynta"` | ✅ |
| InternalName | `"mynta-qt"` | ✅ |
| OriginalFilename | `"mynta-qt.exe"` | ✅ |

### 3.2 macOS - ✅ PASS

| Element | Value | Status |
|---------|-------|--------|
| CFBundleIconFile | `mynta.icns` | ✅ |
| CFBundleExecutable | `Mynta-Qt` | ✅ |
| CFBundleName | `Mynta-Qt` | ✅ |
| CFBundleIdentifier | `network.mynta.Mynta-Qt` | ✅ |
| CFBundleURLSchemes | `mynta` | ✅ |

### 3.3 Linux - ✅ PASS

| Element | Value | Status |
|---------|-------|--------|
| Name | `Mynta Core` | ✅ |
| Exec | `mynta-qt` | ✅ |
| Icon | `mynta128` | ✅ |
| MimeType | `x-scheme-handler/mynta` | ✅ |

---

## 4. Locale Files - ✅ PASS

- All 86 locale files renamed from `raven_*.ts` to `mynta_*.ts` ✅
- `Makefile.qt.include` updated to reference `mynta_*.ts` ✅
- `mynta_locale.qrc` updated to reference `mynta_*.qm` ✅

---

## 5. Build Scripts - ✅ PASS

| File | Update | Status |
|------|--------|--------|
| `extract_strings_qt.py` | `myntastrings.cpp`, `mynta-core` | ✅ |
| `rpcuser.py` | `mynta.conf` | ✅ |
| `myntastrings.cpp` | `mynta_strings[]`, `mynta-core` | ✅ |

---

## 6. External URLs - ✅ PASS

| Check | Status |
|-------|--------|
| No RavenProject GitHub URLs | ✅ |
| No RVNBTC/RVNUSDT trading pairs | ✅ |
| Market label: "Mynta Market Price" | ✅ |

---

## 7. Build Verification - ✅ PASS

```
Build Date: January 13, 2026
Binaries Produced:
  - myntad (212 MB)
  - mynta-cli (9.4 MB)  
  - mynta-qt (288 MB)
Build Status: SUCCESS
```

---

## 8. Runtime Verification - ✅ PASS

| Check | Result |
|-------|--------|
| Window title displays "Mynta Core" | ✅ PASS |
| Application launches without branding errors | ✅ PASS |
| No Ravencoin references visible in UI | ✅ PASS |

---

## 9. Sign-Off

**Audit Result:** ✅ **READY FOR LAUNCH**

The QT wallet has been fully rebranded from Ravencoin to Mynta. All critical, moderate, and minor issues identified in the initial audit have been remediated:

### Remediation Summary

| Category | Items Fixed |
|----------|-------------|
| macOS Info.plist | 2 files, 15+ references |
| UI Forms (.ui) | 15+ files, 60+ references |
| URI Scheme | Changed to `mynta:` throughout |
| Icon/Pixmap Files | 12 old files deleted |
| Locale Files | 86 files renamed |
| Source Code | 20+ files updated |
| Build Config | 3 files updated |

### Verified Clean

- No `raven:` URI schemes in source
- No `:/icons/raven` references
- No `raven.qrc` references
- No leftover raven*.png/ico/icns files
- All UI text uses "Mynta" or "mynta"

---

*This audit confirms the QT wallet is ready for production release with complete Mynta branding.*
