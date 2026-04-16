# SignPath Foundation - Application Guide for Decodium 4.0

## Email da inviare

**A:** oss-support@signpath.org
**Oggetto:** Open Source Code Signing Application - Decodium 4.0 Core Shannon

**Corpo:**

```
Dear SignPath Foundation Team,

I would like to apply for free code signing for my open source project
"Decodium 4.0 Core Shannon".

Decodium is an optimized weak-signal digital communication client for
amateur radio, supporting FT8, FT4 and FT2 modes. It is a GPL v3 fork
of WSJT-X with a modern Qt6 QML interface, enhanced decoder sensitivity
and extended frequency range.

Project details:
- Repository: https://github.com/iu8lmc/Decodium-4.0-Core-Shannon
- License: GNU General Public License v3.0
- Code Signing Policy: https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/blob/main/CODE_SIGNING_POLICY.md
- Releases: https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases

Please find the completed OSSRequestForm-v4.xlsx attached.

Best regards,
IU8LMC
```

---

## Informazioni per compilare OSSRequestForm-v4.xlsx

### Section 1 - Basic Information
| Campo | Valore |
|-------|--------|
| Project Name | Decodium 4.0 Core Shannon |
| Project Short Name | decodium |
| Project Homepage | https://github.com/iu8lmc/Decodium-4.0-Core-Shannon |
| Description (breve) | Optimized weak-signal digital communication client for amateur radio (FT8/FT4/FT2) |
| Description (dettagliata) | Decodium 4.0 Core Shannon is a GPL v3 fork of WSJT-X, the widely-used amateur radio software by Joe Taylor K1JT. It features a modern Qt6 QML interface, multi-mode support (FT8, FT4, FT2), enhanced decoder sensitivity with multi-period soft averaging, extended frequency range (200-4910 Hz), DX Cluster telnet integration, PSK Reporter, and real-time NTP/DT feedback. Used by radio amateurs worldwide for weak-signal digital communication on HF/VHF/UHF bands. |
| License | GPL-3.0-only |
| License URL | https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/blob/main/COPYING |
| Languages | C++, QML, Fortran, CMake |

### Section 2 - Repository Information
| Campo | Valore |
|-------|--------|
| Repository Type | GitHub |
| Repository URL | https://github.com/iu8lmc/Decodium-4.0-Core-Shannon |
| Contributors | 2+ (iu8lmc, elisir80) |
| Commits | 20+ (fork from WSJT-X with 20+ years of development history) |
| Age of Project | Fork created 2026; based on WSJT-X (active since 2001) |
| Development Status | Active |

### Section 3 - Distribution & Downloads
| Campo | Valore |
|-------|--------|
| Download Page | https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases |
| Package Formats | EXE (Windows Inno Setup installer, x64) |
| Distribution Method | GitHub Releases |

### Section 4 - Privacy Policy
| Campo | Valore |
|-------|--------|
| Collects user data? | No |
| Privacy statement | No telemetry, analytics, or tracking. Network connections only for user-initiated PSK Reporter spots and DX Cluster. |

### Section 5 - Wikipedia Article
| Campo | Valore |
|-------|--------|
| Wikipedia URL | https://en.wikipedia.org/wiki/WSJT_(amateur_radio_software) (parent project WSJT-X) |

### Section 6 - Verification & Trust Evidence
| Campo | Valore |
|-------|--------|
| How to verify usage | WSJT-X is the most widely used amateur radio digital mode software worldwide. Decodium is a specialized fork with modern UI. |
| Related resources | WSJT-X official: https://wsjt.sourceforge.io/ ; QRZ.com: https://www.qrz.com/db/IU8LMC |

### Section 7 - Technical Details
| Campo | Valore |
|-------|--------|
| Files to sign | decodium.exe |
| File types | EXE (PE/Authenticode) |
| Signing frequency | 2-4 releases per month |
| Build process | CMake + MinGW (GCC/gfortran) on MSYS2, Qt6, Inno Setup for installers |
| CI/CD | GitHub Actions (workflow: build-windows-sign.yml) |

### Section 8 - Contact Information
| Campo | Valore |
|-------|--------|
| Primary Contact | IU8LMC |
| Email | iu8lmc@gmail.com |
| GitHub User | iu8lmc |

### Section 9 - Terms & Conditions
| Campo | Valore |
|-------|--------|
| OSI license confirmed | Yes (GPL v3) |
| No proprietary components | Yes |
| Accept terms | Yes |

---

## Passi dopo l'approvazione

1. **Installa SignPath GitHub App** sul repo da GitHub Marketplace
2. **Configura su signpath.io:**
   - Crea Organization
   - Crea Project "decodium"
   - Carica `artifact-configuration.xml` (in `.signpath/`)
   - Crea signing policies: `test-signing` e `release-signing`
3. **Aggiungi al repo GitHub:**
   - Secret: `SIGNPATH_API_TOKEN` (da SignPath dashboard)
   - Variable: `SIGNPATH_ORGANIZATION_ID`
4. **Abilita MFA** sul tuo account GitHub
5. **Testa**: vai su Actions → "Build Windows + SignPath" → Run workflow
6. **Release**: crea tag `1.0.18` per triggerare build + signing + installer automatico
