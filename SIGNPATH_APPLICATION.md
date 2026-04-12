# SignPath Foundation - Application Guide for Decodium

## Email da inviare

**A:** oss-support@signpath.org
**Oggetto:** Open Source Code Signing Application - Decodium 3.0 FT2

**Corpo:**

```
Dear SignPath Foundation Team,

I would like to apply for free code signing for my open source project
"Decodium 3.0 FT2 Raptor".

Decodium is an optimized weak-signal digital communication client for
amateur radio, focused on the FT2 mode. It is a GPL v3 fork of WSJT-X 3.0
with enhanced decoder sensitivity and extended frequency range.

Project details:
- Repository: https://github.com/iu8lmc/Decodium-3.0-Codename-Raptor
- License: GNU General Public License v3.0
- Code Signing Policy: https://github.com/iu8lmc/Decodium-3.0-Codename-Raptor/blob/master/CODE_SIGNING_POLICY.md
- Releases: https://github.com/iu8lmc/Decodium-3.0-Codename-Raptor/releases

Please find the completed OSSRequestForm-v4.xlsx attached.

Best regards,
IU8LMC
```

---

## Informazioni per compilare OSSRequestForm-v4.xlsx

### Section 1 - Basic Information
| Campo | Valore |
|-------|--------|
| Project Name | Decodium 3.0 FT2 Raptor |
| Project Short Name | decodium |
| Project Homepage | https://github.com/iu8lmc/Decodium-3.0-Codename-Raptor |
| Description (breve) | Optimized weak-signal digital communication client for amateur radio, focused on FT2 mode |
| Description (dettagliata) | Decodium 3.0 FT2 "Raptor" is a GPL v3 fork of WSJT-X 3.0, the widely-used amateur radio software by Joe Taylor K1JT. It focuses exclusively on the FT2 digital mode (3.75-second period, 4-GFSK modulation) with enhanced decoder sensitivity (syncmin 0.90, OSD depth 3, 4-pass subtraction), extended frequency range (200-4910 Hz), multi-period soft averaging for +4 dB gain, and real-time NTP/DT feedback. Used by radio amateurs worldwide for weak-signal digital communication on HF/VHF/UHF bands. |
| License | GPL-3.0-only |
| License URL | https://github.com/iu8lmc/Decodium-3.0-Codename-Raptor/blob/master/COPYING |
| Languages | Fortran, C++, C, CMake |

### Section 2 - Repository Information
| Campo | Valore |
|-------|--------|
| Repository Type | GitHub |
| Repository URL | https://github.com/iu8lmc/Decodium-3.0-Codename-Raptor |
| Contributors | 2 (iu8lmc, avantol) |
| Commits | ~50+ (fork from WSJT-X with 20+ years of development history) |
| Age of Project | Fork created February 2026; based on WSJT-X which has been in development since 2001 |
| Development Status | Active |

### Section 3 - Distribution & Downloads
| Campo | Valore |
|-------|--------|
| Download Page | https://github.com/iu8lmc/Decodium-3.0-Codename-Raptor/releases |
| Package Formats | EXE (Windows Inno Setup installer, x64 and x86) |
| Distribution Method | GitHub Releases |
| Total Downloads | (controlla su GitHub: gh api repos/iu8lmc/Decodium-3.0-Codename-Raptor/releases --jq '.[].assets[].download_count' ) |

### Section 4 - Privacy Policy
| Campo | Valore |
|-------|--------|
| Collects user data? | No |
| Privacy statement | This program will not transfer any information to other networked systems unless specifically requested by the user. Network connections are only made for user-initiated callsign lookups or manual CTY.DAT updates. No telemetry, analytics, or tracking. |

### Section 5 - Wikipedia Article
| Campo | Valore |
|-------|--------|
| Wikipedia URL | https://en.wikipedia.org/wiki/WSJT_(amateur_radio_software) (parent project WSJT-X) |

### Section 6 - Verification & Trust Evidence
| Campo | Valore |
|-------|--------|
| How to verify usage | WSJT-X is the most widely used amateur radio digital mode software worldwide, with hundreds of thousands of users. Decodium is a specialized fork focusing on the FT2 mode. |
| GitHub stats | 9 stars, 6 forks (growing) |
| Related resources | WSJT-X official: https://wsjt.sourceforge.io/ ; QRZ.com profile: https://www.qrz.com/db/IU8LMC |

### Section 7 - Technical Details
| Campo | Valore |
|-------|--------|
| Files to sign | decodium.exe, jt9.exe, wsprd.exe, message_aggregator.exe, udp_daemon.exe, ChronoGPS.exe, rigctl-decodium.exe, rigctld-decodium.exe, rigctlcom-decodium.exe |
| File types | EXE (PE/Authenticode) |
| Signing frequency | 2-4 releases per month |
| Build process | CMake + MinGW (GCC/gfortran) on MSYS2, Inno Setup for installers |
| CI/CD | GitHub Actions (workflow: build-windows-sign.yml) |

### Section 8 - Contact Information
| Campo | Valore |
|-------|--------|
| Primary Contact | IU8LMC |
| Email | (il tuo indirizzo email) |
| GitHub User | iu8lmc |

### Section 9 - Terms & Conditions
| Campo | Valore |
|-------|--------|
| OSI license confirmed | Yes (GPL v3) |
| No proprietary components | Yes |
| Accept terms | Yes |

---

## Passi successivi dopo l'approvazione

1. **Installa SignPath GitHub App** sul tuo repository da GitHub Marketplace
2. **Configura su signpath.io:**
   - Crea Organization
   - Crea Project "decodium"
   - Carica `artifact-configuration.xml` (gia creato in `.signpath/`)
   - Crea signing policies: `test-signing` e `release-signing`
3. **Aggiungi secrets al repo GitHub:**
   - `SIGNPATH_API_TOKEN` (ottenuto da SignPath dashboard)
   - `SIGNPATH_ORGANIZATION_ID` (variable, non secret)
4. **Abilita MFA** sul tuo account GitHub (requisito obbligatorio)
5. **Testa** con un push su master: il workflow dovrebbe fare build + test-signing
6. **Release**: crea un tag `v3.0.2603010018` per triggerare release-signing + installer

## Form scaricato in:
`C:\Users\IU8LMC\Downloads\OSSRequestForm-v4.xlsx`
