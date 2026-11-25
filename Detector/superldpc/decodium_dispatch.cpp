// decodium_dispatch.cpp — dispatcher portabile per il decoder superldpc.
//
// Questo file DEVE essere compilato per la CPU minima supportata. Non deve
// ricevere -mavx2, -mfma o /arch:AVX2: contiene il rilevamento delle capacita'
// della CPU, il controllo dello stato SIMD del sistema operativo e il fallback
// al decoder LDPC originale.
//
// Livelli, dal piu' veloce, scelti a runtime:
//   1. superldpc AVX2/FMA (decodium_bridge.cpp) su x86, NEON su ARM64;
//   2. superldpc SSE2 (decodium_bridge_sse2.cpp) sulle CPU x86-64 senza AVX2
//      o senza stato YMM salvato dal sistema: stesso algoritmo, stessi
//      risultati bit per bit (tests/test_superldpc_tiers.cpp);
//   3. il decoder LDPC originale (ftx_decode174_91_c).
// Ogni backend vive nella sua translation unit e nel suo namespace e viene
// chiamato solo dopo che i controlli del suo livello hanno avuto esito positivo.
// DECODIUM_SUPERLDPC_MAX_TIER=sse2|generic limita il livello (prove e
// confronti su una CPU moderna); DECODIUM_FT2_DISABLE_SUPERLDPC=1 resta
// l'interruttore di emergenza che porta sempre al decoder originale.

#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#if defined(__APPLE__)
#  include <sys/sysctl.h>
#endif

#if defined(_M_IX86) || defined(_M_X64)
#  include <intrin.h>
#elif defined(__i386__) || defined(__x86_64__)
#  include <cpuid.h>
#endif

#if defined(__linux__) && defined(__aarch64__)
#  include <sys/auxv.h>
#  if defined(__has_include)
#    if __has_include(<asm/hwcap.h>)
#      include <asm/hwcap.h>
#    endif
#  endif
#elif defined(_WIN32) && defined(_M_ARM64)
#  include <windows.h>
#endif

extern "C" void ftx_decode174_91_c (float const*, int, int, int, signed char const*,
                                    signed char*, signed char*, int*, int*, float*);

#if defined(DECODIUM_SUPERLDPC_AVX2_BUILT) || defined(DECODIUM_SUPERLDPC_NEON_BUILT)
extern "C" void superldpc_simd_set_ft8_mode_c (int);
extern "C" void superldpc_simd_set_ft2_max_hard_c (int);
extern "C" void superldpc_simd_decode174_91_c (float const*, int, int, int,
                                              signed char const*, signed char*,
                                              signed char*, int*, int*, float*);
extern "C" void superldpc_simd_decode174_91_batch_c (int, float const*,
                                                    signed char const*, int, int, int,
                                                    signed char*, signed char*, int*,
                                                    int*, float*);
extern "C" void superldpc_simd_gate_dump_open_c (char const*);
extern "C" void superldpc_simd_gate_dump_close_c ();
extern "C" void superldpc_simd_gate_truth_set_c (signed char const*);
extern "C" void superldpc_simd_gate_truth_clear_c ();
extern "C" int superldpc_simd_extrinsic174_91_c (float const*, int, float, float*);
extern "C" int superldpc_simd_extrinsic174_91_v2_c (float const*, int, int, int, float,
                                                  float*);
#endif

#if defined(DECODIUM_SUPERLDPC_SSE2_BUILT)
extern "C" void superldpc_sse2_set_ft8_mode_c (int);
extern "C" void superldpc_sse2_set_ft2_max_hard_c (int);
extern "C" void superldpc_sse2_decode174_91_c (float const*, int, int, int,
                                              signed char const*, signed char*,
                                              signed char*, int*, int*, float*);
extern "C" void superldpc_sse2_decode174_91_batch_c (int, float const*,
                                                    signed char const*, int, int, int,
                                                    signed char*, signed char*, int*,
                                                    int*, float*);
extern "C" void superldpc_sse2_gate_dump_open_c (char const*);
extern "C" void superldpc_sse2_gate_dump_close_c ();
extern "C" void superldpc_sse2_gate_truth_set_c (signed char const*);
extern "C" void superldpc_sse2_gate_truth_clear_c ();
extern "C" int superldpc_sse2_extrinsic174_91_c (float const*, int, float, float*);
extern "C" int superldpc_sse2_extrinsic174_91_v2_c (float const*, int, int, int, float,
                                                  float*);
#endif

#if defined(DECODIUM_SUPERLDPC_TESTING)
// Test-only CPUID snapshot injection. Never enabled in the application.
extern "C" unsigned superldpc_test_x86_features_c ();
#endif

namespace {

constexpr int kN = 174;

#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
#  define DECODIUM_SUPERLDPC_X86 1
#else
#  define DECODIUM_SUPERLDPC_X86 0
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
#  define DECODIUM_SUPERLDPC_ARM64 1
#else
#  define DECODIUM_SUPERLDPC_ARM64 0
#endif

struct CpuCapabilities {
    std::string model;
    bool x86 = false;
    bool arm64 = false;
    bool sse2 = false;
    bool avx = false;
    bool avx2 = false;
    bool fma = false;
    bool osxsave = false;
    bool osAvxState = false;
    bool neon = false;
    bool avx2BackendBuilt = false;
    bool neonBackendBuilt = false;
    bool sse2BackendBuilt = false;

    // Livello 1: AVX2/FMA su x86, NEON su ARM64.
    bool topTierUsable () const
    {
        if (x86)
            return avx2BackendBuilt && avx && avx2 && fma && osxsave && osAvxState;
        if (arm64)
            return neonBackendBuilt && neon;
        return false;
    }

    // Livello 2: SSE2, presente su ogni CPU x86-64; lo stato XMM e' sempre
    // salvato dal sistema operativo a 64 bit.
    bool sse2TierUsable () const
    {
        return x86 && sse2BackendBuilt && sse2;
    }
};

#if DECODIUM_SUPERLDPC_X86
struct CpuidRegisters {
    std::uint32_t eax = 0;
    std::uint32_t ebx = 0;
    std::uint32_t ecx = 0;
    std::uint32_t edx = 0;
};

bool cpuid (std::uint32_t leaf, std::uint32_t subleaf, CpuidRegisters& out)
{
#  if defined(_MSC_VER)
    int registers[4] {};
    __cpuidex (registers, static_cast<int> (leaf), static_cast<int> (subleaf));
    out.eax = static_cast<std::uint32_t> (registers[0]);
    out.ebx = static_cast<std::uint32_t> (registers[1]);
    out.ecx = static_cast<std::uint32_t> (registers[2]);
    out.edx = static_cast<std::uint32_t> (registers[3]);
    return true;
#  else
    unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
    if (!__get_cpuid_count (leaf, subleaf, &eax, &ebx, &ecx, &edx)) return false;
    out.eax = eax;
    out.ebx = ebx;
    out.ecx = ecx;
    out.edx = edx;
    return true;
#  endif
}

std::uint64_t readXcr0 ()
{
#  if defined(_MSC_VER)
    return static_cast<std::uint64_t> (_xgetbv (0));
#  else
    std::uint32_t eax = 0, edx = 0;
    // Codifica esplicita di XGETBV: in questo modo il dispatcher resta
    // compilabile senza -mxsave e, soprattutto, senza abilitare AVX.
    __asm__ volatile (".byte 0x0f, 0x01, 0xd0" : "=a" (eax), "=d" (edx) : "c" (0));
    return (static_cast<std::uint64_t> (edx) << 32) | eax;
#  endif
}

std::string trimCpuModel (char const* raw)
{
    std::string result;
    bool pendingSpace = false;
    for (char const* p = raw; p && *p; ++p) {
        const unsigned char ch = static_cast<unsigned char> (*p);
        if (std::isspace (ch)) {
            pendingSpace = !result.empty ();
            continue;
        }
        if (pendingSpace) result.push_back (' ');
        result.push_back (static_cast<char> (ch));
        pendingSpace = false;
    }
    return result;
}

std::string detectX86CpuModel (std::uint32_t maxExtendedLeaf,
                               CpuidRegisters const& vendorLeaf)
{
    if (maxExtendedLeaf >= 0x80000004u) {
        std::array<std::uint32_t, 12> words {};
        for (std::uint32_t i = 0; i < 3; ++i) {
            CpuidRegisters r;
            if (!cpuid (0x80000002u + i, 0, r)) break;
            words[i * 4 + 0] = r.eax;
            words[i * 4 + 1] = r.ebx;
            words[i * 4 + 2] = r.ecx;
            words[i * 4 + 3] = r.edx;
        }
        char brand[49] {};
        std::memcpy (brand, words.data (), 48);
        std::string const trimmed = trimCpuModel (brand);
        if (!trimmed.empty ()) return trimmed;
    }

    char vendor[13] {};
    std::memcpy (vendor + 0, &vendorLeaf.ebx, 4);
    std::memcpy (vendor + 4, &vendorLeaf.edx, 4);
    std::memcpy (vendor + 8, &vendorLeaf.ecx, 4);
    return trimCpuModel (vendor);
}
#endif

#if DECODIUM_SUPERLDPC_ARM64
bool detectArmNeon ()
{
#if defined(__APPLE__)
    int supported = 0;
    std::size_t size = sizeof(supported);
    return sysctlbyname ("hw.optional.neon", &supported, &size, nullptr, 0) == 0
           && supported != 0;
#elif defined(__linux__) && defined(HWCAP_ASIMD)
    return (getauxval (AT_HWCAP) & HWCAP_ASIMD) != 0;
#elif defined(_WIN32) && defined(_M_ARM64)
#  ifndef PF_ARM_NEON_INSTRUCTIONS_AVAILABLE
#    define PF_ARM_NEON_INSTRUCTIONS_AVAILABLE 19
#  endif
    return IsProcessorFeaturePresent (PF_ARM_NEON_INSTRUCTIONS_AVAILABLE) != FALSE;
#else
    // Advanced SIMD e' obbligatorio nell'architettura AArch64. Questa via
    // copre i sistemi che non espongono una API di capability equivalente.
    return true;
#endif
}
#endif

#if !DECODIUM_SUPERLDPC_X86
std::string nonX86Model ()
{
#if defined(__APPLE__)
    std::size_t size = 0;
    if (sysctlbyname ("machdep.cpu.brand_string", nullptr, &size, nullptr, 0) == 0
        && size > 1) {
        std::string model (size, '\0');
        if (sysctlbyname ("machdep.cpu.brand_string", model.data (), &size, nullptr, 0) == 0) {
            while (!model.empty () && model.back () == '\0') model.pop_back ();
            if (!model.empty ()) return model;
        }
    }
#endif
#if defined(__aarch64__) || defined(_M_ARM64)
    return "ARM64";
#elif defined(__arm__) || defined(_M_ARM)
    return "ARM";
#else
    return "non-x86 CPU";
#endif
}
#endif

CpuCapabilities detectCpuCapabilities ()
{
    CpuCapabilities result;
#if defined(DECODIUM_SUPERLDPC_AVX2_BUILT)
    result.avx2BackendBuilt = true;
#endif
#if defined(DECODIUM_SUPERLDPC_NEON_BUILT)
    result.neonBackendBuilt = true;
#endif
#if defined(DECODIUM_SUPERLDPC_SSE2_BUILT)
    result.sse2BackendBuilt = true;
#endif

#if defined(DECODIUM_SUPERLDPC_TESTING)
    unsigned const features = superldpc_test_x86_features_c ();
    result.model = "test x86 CPU";
    result.x86 = true;
    result.avx = (features & 1u) != 0;
    result.avx2 = (features & 2u) != 0;
    result.fma = (features & 4u) != 0;
    result.osxsave = (features & 8u) != 0;
    result.osAvxState = (features & 16u) != 0;
    result.sse2 = (features & 32u) == 0;       // bit 32 = CPU senza SSE2
    return result;
#endif

#if DECODIUM_SUPERLDPC_X86
    result.x86 = true;
    CpuidRegisters leaf0;
    if (!cpuid (0, 0, leaf0)) {
        result.model = "unknown x86 CPU";
        return result;
    }
    std::uint32_t const maxBasicLeaf = leaf0.eax;

    CpuidRegisters extended0;
    std::uint32_t maxExtendedLeaf = 0;
    if (cpuid (0x80000000u, 0, extended0)) maxExtendedLeaf = extended0.eax;
    result.model = detectX86CpuModel (maxExtendedLeaf, leaf0);

    CpuidRegisters leaf1;
    if (maxBasicLeaf >= 1 && cpuid (1, 0, leaf1)) {
        result.sse2 = (leaf1.edx & (1u << 26)) != 0;
        result.fma = (leaf1.ecx & (1u << 12)) != 0;
        result.osxsave = (leaf1.ecx & (1u << 27)) != 0;
        result.avx = (leaf1.ecx & (1u << 28)) != 0;
        if (result.osxsave) {
            std::uint64_t const xcr0 = readXcr0 ();
            result.osAvxState = (xcr0 & 0x6u) == 0x6u;
        }
    }

    CpuidRegisters leaf7;
    if (maxBasicLeaf >= 7 && cpuid (7, 0, leaf7))
        result.avx2 = (leaf7.ebx & (1u << 5)) != 0;
#else
    result.model = nonX86Model ();
#if DECODIUM_SUPERLDPC_ARM64
    result.arm64 = true;
    result.neon = detectArmNeon ();
#endif
#endif
    return result;
}

CpuCapabilities const& cpuCapabilities ()
{
    static CpuCapabilities const capabilities = detectCpuCapabilities ();
    return capabilities;
}

std::atomic<int> g_enabledFromUi {-1};
std::atomic<bool> g_initialLogWritten {false};
thread_local bool g_ft8Mode = false;
// FT2 flipped-bit cap requested for the next decode calls, 0 = mode default.
thread_local int g_ft2MaxHard = 0;

bool disabledByEnvironment ()
{
    static bool const disabled = [] {
        char const* raw = std::getenv ("DECODIUM_FT2_DISABLE_SUPERLDPC");
        return raw && std::atoi (raw) != 0;
    }();
    return disabled;
}

bool superLdpcRequested ()
{
    // La variabile d'ambiente e' l'interruttore di emergenza usato quando si
    // deve avviare Decodium su una CPU problematica. Deve avere precedenza
    // assoluta: durante il caricamento delle impostazioni la GUI chiama
    // superldpc_set_enabled_c() con il valore salvato e, in precedenza, quel
    // valore poteva riattivare il backend nonostante DISABLE_SUPERLDPC=1.
    if (disabledByEnvironment ()) return false;
    int const ui = g_enabledFromUi.load (std::memory_order_relaxed);
    return ui >= 0 ? ui != 0 : true;
}

enum class Tier { Generic = 0, Sse2 = 1, Top = 2 };

// DECODIUM_SUPERLDPC_MAX_TIER: livello massimo ammesso. Serve a provare su una
// CPU moderna i percorsi delle CPU vecchie, con lo stesso eseguibile.
Tier maxTier ()
{
    static Tier const tier = [] {
        char const* raw = std::getenv ("DECODIUM_SUPERLDPC_MAX_TIER");
        if (!raw || !*raw) return Tier::Top;
        std::string value;
        for (char const* p = raw; *p; ++p)
            value.push_back (static_cast<char> (std::tolower (static_cast<unsigned char> (*p))));
        if (value == "generic" || value == "original" || value == "0") return Tier::Generic;
        if (value == "sse2" || value == "1") return Tier::Sse2;
        return Tier::Top;
    }();
    return tier;
}

struct Backend {
    Tier tier;
    char const* decoder;
    void (*setFt8Mode) (int);
    void (*setFt2MaxHard) (int);
    void (*decode) (float const*, int, int, int, signed char const*, signed char*,
                    signed char*, int*, int*, float*);
    void (*decodeBatch) (int, float const*, signed char const*, int, int, int,
                         signed char*, signed char*, int*, int*, float*);
    int (*extrinsic) (float const*, int, float, float*);
    int (*extrinsicV2) (float const*, int, int, int, float, float*);
    void (*gateDumpOpen) (char const*);
    void (*gateDumpClose) ();
    void (*gateTruthSet) (signed char const*);
    void (*gateTruthClear) ();
};

#if defined(DECODIUM_SUPERLDPC_AVX2_BUILT) || defined(DECODIUM_SUPERLDPC_NEON_BUILT)
Backend const kTopBackend {
    Tier::Top,
    DECODIUM_SUPERLDPC_ARM64 ? "superldpc-neon" : "superldpc-avx2-fma",
    superldpc_simd_set_ft8_mode_c, superldpc_simd_set_ft2_max_hard_c,
    superldpc_simd_decode174_91_c, superldpc_simd_decode174_91_batch_c,
    superldpc_simd_extrinsic174_91_c, superldpc_simd_extrinsic174_91_v2_c,
    superldpc_simd_gate_dump_open_c, superldpc_simd_gate_dump_close_c,
    superldpc_simd_gate_truth_set_c, superldpc_simd_gate_truth_clear_c
};
#endif

#if defined(DECODIUM_SUPERLDPC_SSE2_BUILT)
Backend const kSse2Backend {
    Tier::Sse2,
    "superldpc-sse2",
    superldpc_sse2_set_ft8_mode_c, superldpc_sse2_set_ft2_max_hard_c,
    superldpc_sse2_decode174_91_c, superldpc_sse2_decode174_91_batch_c,
    superldpc_sse2_extrinsic174_91_c, superldpc_sse2_extrinsic174_91_v2_c,
    superldpc_sse2_gate_dump_open_c, superldpc_sse2_gate_dump_close_c,
    superldpc_sse2_gate_truth_set_c, superldpc_sse2_gate_truth_clear_c
};
#endif

// Il backend che questa CPU e questo sistema possono eseguire, a prescindere
// dal pulsante LDPC e dall'interruttore di emergenza. nullptr = generico.
Backend const* hardwareBackend ()
{
    CpuCapabilities const& cpu = cpuCapabilities ();
    Tier const limit = maxTier ();
#if defined(DECODIUM_SUPERLDPC_AVX2_BUILT) || defined(DECODIUM_SUPERLDPC_NEON_BUILT)
    if (limit >= Tier::Top && cpu.topTierUsable ()) return &kTopBackend;
#endif
#if defined(DECODIUM_SUPERLDPC_SSE2_BUILT)
    if (limit >= Tier::Sse2 && cpu.sse2TierUsable ()) return &kSse2Backend;
#endif
    (void) cpu; (void) limit;
    return nullptr;
}

Backend const* activeBackend ()
{
    return superLdpcRequested () ? hardwareBackend () : nullptr;
}

bool useSuperLdpc ()
{
    return activeBackend () != nullptr;
}

// Perche' non si usa il livello 1 (AVX2/FMA o NEON).
char const* topTierReason ()
{
    CpuCapabilities const& cpu = cpuCapabilities ();
    if (cpu.x86) {
        if (!cpu.avx2BackendBuilt) return "AVX2 backend not built";
        if (!cpu.avx) return "CPU has no AVX";
        if (!cpu.avx2) return "CPU has no AVX2";
        if (!cpu.fma) return "CPU has no FMA";
        if (!cpu.osxsave) return "OSXSAVE is unavailable";
        if (!cpu.osAvxState) return "OS does not preserve AVX XMM/YMM state";
    } else if (cpu.arm64) {
        if (!cpu.neonBackendBuilt) return "NEON backend not built";
        if (!cpu.neon) return "ARM NEON/Advanced SIMD unavailable";
    } else {
        return "unsupported CPU architecture";
    }
    if (maxTier () < Tier::Top) return "limited by DECODIUM_SUPERLDPC_MAX_TIER";
    return "none";
}

char const* fallbackReason ()
{
    if (!superLdpcRequested ()) return "disabled by settings/environment";
    return topTierReason ();
}

char const* backendDescription (CpuCapabilities const& cpu)
{
    if (cpu.x86 && cpu.avx2BackendBuilt && cpu.sse2BackendBuilt) return "avx2-fma+sse2-compiled";
    if (cpu.x86 && cpu.avx2BackendBuilt) return "avx2-fma-compiled";
    if (cpu.x86 && cpu.sse2BackendBuilt) return "sse2-compiled";
    if (cpu.arm64 && cpu.neonBackendBuilt) return "neon-compiled";
    return "not-built";
}

char const* selectedDecoder (Backend const* backend)
{
    return backend ? backend->decoder : "original-generic";
}

// Motivo riportato nel log: "none" quando gira il livello 1, altrimenti
// perche' non gira (anche quando al suo posto lavora il livello SSE2).
char const* selectionReason (Backend const* backend)
{
    if (backend && backend->tier == Tier::Top) return "none";
    return fallbackReason ();
}

void logDecoderSelection (char const* trigger, bool force)
{
    if (!force) {
        bool expected = false;
        if (!g_initialLogWritten.compare_exchange_strong (expected, true,
                                                          std::memory_order_relaxed))
            return;
    } else {
        g_initialLogWritten.store (true, std::memory_order_relaxed);
    }

    CpuCapabilities const& cpu = cpuCapabilities ();
    Backend const* const backend = activeBackend ();
    std::fprintf (stderr,
                  "[superldpc] CPU=\"%s\" x86=%d ARM64=%d SSE2=%d AVX=%d AVX2=%d FMA=%d "
                  "OSXSAVE=%d OS_AVX_STATE=%d NEON=%d backend=%s decoder=%s "
                  "trigger=%s reason=\"%s\"\n",
                  cpu.model.c_str (), cpu.x86 ? 1 : 0, cpu.arm64 ? 1 : 0,
                  cpu.sse2 ? 1 : 0, cpu.avx ? 1 : 0,
                  cpu.avx2 ? 1 : 0, cpu.fma ? 1 : 0, cpu.osxsave ? 1 : 0,
                  cpu.osAvxState ? 1 : 0, cpu.neon ? 1 : 0,
                  backendDescription (cpu), selectedDecoder (backend),
                  trigger ? trigger : "runtime", selectionReason (backend));
}

void initialiseOutputs (signed char* message91, signed char* cw, int* ntype,
                        int* nharderror, float* dmin)
{
    if (message91) std::memset (message91, 0, 91);
    if (cw) std::memset (cw, 0, kN);
    if (ntype) *ntype = 0;
    if (nharderror) *nharderror = -1;
    if (dmin) *dmin = 0.0f;
}

} // namespace

extern "C" void superldpc_set_enabled_c (int on)
{
    g_enabledFromUi.store (on ? 1 : 0, std::memory_order_relaxed);
    logDecoderSelection ("settings", true);
}

// superldpc_descrivi_c: la stessa riga che logDecoderSelection manda su
// stderr, ma restituita al chiamante. Serve al log diagnostico: nei log
// degli utenti mancava proprio l'informazione decisiva, cioe' quali
// estensioni ha la CPU e quale decodificatore e' stato scelto.
extern "C" void superldpc_descrivi_c (char* out, int n)
{
    if (!out || n <= 0) return;
    CpuCapabilities const& cpu = cpuCapabilities ();
    Backend const* const backend = activeBackend ();
    std::snprintf (out, static_cast<std::size_t> (n),
                   "CPU=\"%s\" x86=%d ARM64=%d SSE2=%d AVX=%d AVX2=%d FMA=%d OSXSAVE=%d "
                   "OS_AVX_STATE=%d NEON=%d backend=%s decoder=%s reason=\"%s\"",
                   cpu.model.c_str (), cpu.x86 ? 1 : 0, cpu.arm64 ? 1 : 0,
                   cpu.sse2 ? 1 : 0, cpu.avx ? 1 : 0, cpu.avx2 ? 1 : 0, cpu.fma ? 1 : 0,
                   cpu.osxsave ? 1 : 0, cpu.osAvxState ? 1 : 0, cpu.neon ? 1 : 0,
                   backendDescription (cpu), selectedDecoder (backend),
                   selectionReason (backend));
}

extern "C" int superldpc_is_enabled_c ()
{
    logDecoderSelection ("status", false);
    return useSuperLdpc () ? 1 : 0;
}

// Hardware/OS availability of the TOP tier (AVX2/FMA or NEON), independent of
// the LDPC button and emergency override. FT2 compatibility limits must never
// affect such a CPU merely because the user has chosen the original decoder.
// A CPU that only reaches the SSE2 tier keeps them: its 2 s safety deadline
// still applies, and maxosd (0 there) does not change the superldpc OSD,
// which only distinguishes maxosd < 0 (off) from >= 0.
extern "C" int superldpc_is_available_c ()
{
    return (maxTier () >= Tier::Top && cpuCapabilities ().topTierUsable ()) ? 1 : 0;
}

// 0 = original generic decoder, 1 = superldpc SSE2, 2 = superldpc AVX2/FMA
// or NEON: the decoder the next call will use (button and override included).
extern "C" int superldpc_active_tier_c ()
{
    Backend const* const backend = activeBackend ();
    return backend ? static_cast<int> (backend->tier) : 0;
}

extern "C" void superldpc_set_ft8_mode_c (int on)
{
    g_ft8Mode = on != 0;
}

// FT2 only: per-candidate flipped-bit cap (0 = mode default). Thread-local
// like the mode; it has no effect in FT8 mode or on the generic decoder.
extern "C" void superldpc_set_ft2_max_hard_c (int n)
{
    g_ft2MaxHard = n > 0 ? n : 0;
}

extern "C" void superldpc_decode174_91_c (float const* llrIn, int Keff, int maxosd,
                                         int norder, signed char const* apmaskIn,
                                         signed char* message91Out, signed char* cwOut,
                                         int* ntypeOut, int* nharderrorOut, float* dminOut)
{
    initialiseOutputs (message91Out, cwOut, ntypeOut, nharderrorOut, dminOut);
    if (!llrIn || !apmaskIn) return;

    logDecoderSelection ("decode", false);
    Backend const* const backend = Keff == 91 ? activeBackend () : nullptr;
    if (backend) {
        backend->setFt8Mode (g_ft8Mode ? 1 : 0);
        if (!g_ft8Mode) backend->setFt2MaxHard (g_ft2MaxHard);
        backend->decode (llrIn, Keff, maxosd, norder, apmaskIn, message91Out, cwOut,
                         ntypeOut, nharderrorOut, dminOut);
        return;
    }
    ftx_decode174_91_c (llrIn, Keff, maxosd, norder, apmaskIn, message91Out,
                        cwOut, ntypeOut, nharderrorOut, dminOut);
}

extern "C" void superldpc_decode174_91_batch_c (int n, float const* llrIn,
                                                signed char const* apmaskIn,
                                                int Keff, int maxosd, int norder,
                                                signed char* message91Out,
                                                signed char* cwOut, int* ntypeOut,
                                                int* nharderrorOut, float* dminOut)
{
    if (n <= 0 || !llrIn || !apmaskIn) return;

    logDecoderSelection ("batch-decode", false);
    Backend const* const backend = Keff == 91 ? activeBackend () : nullptr;
    if (backend) {
        backend->setFt8Mode (g_ft8Mode ? 1 : 0);
        if (!g_ft8Mode) backend->setFt2MaxHard (g_ft2MaxHard);
        backend->decodeBatch (n, llrIn, apmaskIn, Keff, maxosd, norder, message91Out,
                              cwOut, ntypeOut, nharderrorOut, dminOut);
        return;
    }

    for (int word = 0; word < n; ++word) {
        superldpc_decode174_91_c (llrIn + static_cast<std::size_t> (word) * kN,
                                 Keff, maxosd, norder,
                                 apmaskIn + static_cast<std::size_t> (word) * kN,
                                 message91Out
                                     ? message91Out + static_cast<std::size_t> (word) * 91
                                     : nullptr,
                                 cwOut ? cwOut + static_cast<std::size_t> (word) * kN : nullptr,
                                 ntypeOut ? ntypeOut + word : nullptr,
                                 nharderrorOut ? nharderrorOut + word : nullptr,
                                 dminOut ? dminOut + word : nullptr);
    }
}

// BICM-ID is optional: the generic LDPC decoder has no min-sum posterior.
// Return neutral extrinsic information without entering the SIMD translation
// unit on unsupported CPUs, or when disabled by the UI/emergency switch.
extern "C" int superldpc_extrinsic174_91_c (float const* llrIn, int norder,
                                          float clamp, float* extrinsicOut)
{
    if (extrinsicOut) std::memset (extrinsicOut, 0, kN * sizeof (float));
    if (!llrIn || !extrinsicOut) return 0;
    logDecoderSelection ("extrinsic", false);
    if (Backend const* const backend = activeBackend ()) {
        backend->setFt8Mode (g_ft8Mode ? 1 : 0);
        return backend->extrinsic (llrIn, norder, clamp, extrinsicOut);
    }
    return 0;
}

// Second version of the extrinsic output (see decodium_bridge.cpp): word >= 0
// reuses the posteriors of the last batch, word < 0 decodes. -1 = not
// available (generic decoder, unsupported CPU, disabled, or no such word).
extern "C" int superldpc_extrinsic174_91_v2_c (float const* llrIn, int norder, int word,
                                             int flags, float clamp, float* extrinsicOut)
{
    if (extrinsicOut) std::memset (extrinsicOut, 0, kN * sizeof (float));
    if (!llrIn || !extrinsicOut) return -1;
    if (Backend const* const backend = activeBackend ()) {
        backend->setFt8Mode (g_ft8Mode ? 1 : 0);
        return backend->extrinsicV2 (llrIn, norder, word, flags, clamp, extrinsicOut);
    }
    return -1;
}

// Raccolta dati per il riaddestramento del gate (tests/ft2_gate_dump.cpp):
// niente da fare senza un backend superldpc, non esiste un gate nel decoder
// originale a cui agganciarsi. Va al backend che la CPU puo' eseguire.
extern "C" void superldpc_gate_dump_open_c (char const* path)
{
    if (Backend const* const backend = hardwareBackend ()) backend->gateDumpOpen (path);
}

extern "C" void superldpc_gate_dump_close_c ()
{
    if (Backend const* const backend = hardwareBackend ()) backend->gateDumpClose ();
}

extern "C" void superldpc_gate_truth_set_c (signed char const* cw174)
{
    if (Backend const* const backend = hardwareBackend ()) backend->gateTruthSet (cw174);
}

extern "C" void superldpc_gate_truth_clear_c ()
{
    if (Backend const* const backend = hardwareBackend ()) backend->gateTruthClear ();
}
