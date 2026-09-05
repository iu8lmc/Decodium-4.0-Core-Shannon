// decodium_dispatch.cpp — dispatcher portabile per il decoder fastldpc.
//
// Questo file DEVE essere compilato per la CPU minima supportata. Non deve
// ricevere -mavx2, -mfma o /arch:AVX2: contiene il rilevamento delle capacita'
// della CPU, il controllo dello stato SIMD del sistema operativo e il fallback
// al decoder LDPC originale. Il solo backend in decodium_bridge.cpp viene
// compilato con AVX2/FMA su x86 o NEON su ARM64 e viene chiamato esclusivamente
// dopo che tutti i controlli hanno avuto esito positivo.

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

#if defined(DECODIUM_FASTLDPC_AVX2_BUILT) || defined(DECODIUM_FASTLDPC_NEON_BUILT)
extern "C" void fastldpc_simd_set_ft8_mode_c (int);
extern "C" void fastldpc_simd_decode174_91_c (float const*, int, int, int,
                                              signed char const*, signed char*,
                                              signed char*, int*, int*, float*);
extern "C" void fastldpc_simd_decode174_91_batch_c (int, float const*,
                                                    signed char const*, int, int, int,
                                                    signed char*, signed char*, int*,
                                                    int*, float*);
extern "C" void fastldpc_simd_gate_dump_open_c (char const*);
extern "C" void fastldpc_simd_gate_dump_close_c ();
extern "C" void fastldpc_simd_gate_truth_set_c (signed char const*);
extern "C" void fastldpc_simd_gate_truth_clear_c ();
#endif

namespace {

constexpr int kN = 174;

#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__)
#  define DECODIUM_FASTLDPC_X86 1
#else
#  define DECODIUM_FASTLDPC_X86 0
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
#  define DECODIUM_FASTLDPC_ARM64 1
#else
#  define DECODIUM_FASTLDPC_ARM64 0
#endif

struct CpuCapabilities {
    std::string model;
    bool x86 = false;
    bool arm64 = false;
    bool avx = false;
    bool avx2 = false;
    bool fma = false;
    bool osxsave = false;
    bool osAvxState = false;
    bool neon = false;
    bool avx2BackendBuilt = false;
    bool neonBackendBuilt = false;

    bool fastLdpcUsable () const
    {
        if (x86)
            return avx2BackendBuilt && avx && avx2 && fma && osxsave && osAvxState;
        if (arm64)
            return neonBackendBuilt && neon;
        return false;
    }
};

#if DECODIUM_FASTLDPC_X86
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

#if DECODIUM_FASTLDPC_ARM64
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

#if !DECODIUM_FASTLDPC_X86
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
#if defined(DECODIUM_FASTLDPC_AVX2_BUILT)
    result.avx2BackendBuilt = true;
#endif
#if defined(DECODIUM_FASTLDPC_NEON_BUILT)
    result.neonBackendBuilt = true;
#endif

#if DECODIUM_FASTLDPC_X86
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
#if DECODIUM_FASTLDPC_ARM64
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

bool disabledByEnvironment ()
{
    static bool const disabled = [] {
        char const* raw = std::getenv ("DECODIUM_FT2_DISABLE_FASTLDPC");
        return raw && std::atoi (raw) != 0;
    }();
    return disabled;
}

bool fastLdpcRequested ()
{
    // La variabile d'ambiente e' l'interruttore di emergenza usato quando si
    // deve avviare Decodium su una CPU problematica. Deve avere precedenza
    // assoluta: durante il caricamento delle impostazioni la GUI chiama
    // fastldpc_set_enabled_c() con il valore salvato e, in precedenza, quel
    // valore poteva riattivare il backend nonostante DISABLE_FASTLDPC=1.
    if (disabledByEnvironment ()) return false;
    int const ui = g_enabledFromUi.load (std::memory_order_relaxed);
    return ui >= 0 ? ui != 0 : true;
}

bool useFastLdpc ()
{
    return fastLdpcRequested () && cpuCapabilities ().fastLdpcUsable ();
}

char const* fallbackReason ()
{
    CpuCapabilities const& cpu = cpuCapabilities ();
    if (!fastLdpcRequested ()) return "disabled by settings/environment";
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
    return "none";
}

char const* backendDescription (CpuCapabilities const& cpu)
{
    if (cpu.x86 && cpu.avx2BackendBuilt) return "avx2-fma-compiled";
    if (cpu.arm64 && cpu.neonBackendBuilt) return "neon-compiled";
    return "not-built";
}

char const* selectedDecoder (CpuCapabilities const& cpu, bool selected)
{
    if (!selected) return "original-generic";
    return cpu.arm64 ? "fastldpc-neon" : "fastldpc-avx2-fma";
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
    bool const selected = useFastLdpc ();
    std::fprintf (stderr,
                  "[fastldpc] CPU=\"%s\" x86=%d ARM64=%d AVX=%d AVX2=%d FMA=%d "
                  "OSXSAVE=%d OS_AVX_STATE=%d NEON=%d backend=%s decoder=%s "
                  "trigger=%s reason=\"%s\"\n",
                  cpu.model.c_str (), cpu.x86 ? 1 : 0, cpu.arm64 ? 1 : 0,
                  cpu.avx ? 1 : 0,
                  cpu.avx2 ? 1 : 0, cpu.fma ? 1 : 0, cpu.osxsave ? 1 : 0,
                  cpu.osAvxState ? 1 : 0, cpu.neon ? 1 : 0,
                  backendDescription (cpu), selectedDecoder (cpu, selected),
                  trigger ? trigger : "runtime", selected ? "none" : fallbackReason ());
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

extern "C" void fastldpc_set_enabled_c (int on)
{
    g_enabledFromUi.store (on ? 1 : 0, std::memory_order_relaxed);
    logDecoderSelection ("settings", true);
}

extern "C" int fastldpc_is_enabled_c ()
{
    logDecoderSelection ("status", false);
    return useFastLdpc () ? 1 : 0;
}

extern "C" void fastldpc_set_ft8_mode_c (int on)
{
    g_ft8Mode = on != 0;
}

extern "C" void fastldpc_decode174_91_c (float const* llrIn, int Keff, int maxosd,
                                         int norder, signed char const* apmaskIn,
                                         signed char* message91Out, signed char* cwOut,
                                         int* ntypeOut, int* nharderrorOut, float* dminOut)
{
    initialiseOutputs (message91Out, cwOut, ntypeOut, nharderrorOut, dminOut);
    if (!llrIn || !apmaskIn) return;

    logDecoderSelection ("decode", false);
#if defined(DECODIUM_FASTLDPC_AVX2_BUILT) || defined(DECODIUM_FASTLDPC_NEON_BUILT)
    if (Keff == 91 && useFastLdpc ()) {
        fastldpc_simd_set_ft8_mode_c (g_ft8Mode ? 1 : 0);
        fastldpc_simd_decode174_91_c (llrIn, Keff, maxosd, norder, apmaskIn,
                                    message91Out, cwOut, ntypeOut,
                                    nharderrorOut, dminOut);
        return;
    }
#endif
    ftx_decode174_91_c (llrIn, Keff, maxosd, norder, apmaskIn, message91Out,
                        cwOut, ntypeOut, nharderrorOut, dminOut);
}

extern "C" void fastldpc_decode174_91_batch_c (int n, float const* llrIn,
                                                signed char const* apmaskIn,
                                                int Keff, int maxosd, int norder,
                                                signed char* message91Out,
                                                signed char* cwOut, int* ntypeOut,
                                                int* nharderrorOut, float* dminOut)
{
    if (n <= 0 || !llrIn || !apmaskIn) return;

    logDecoderSelection ("batch-decode", false);
#if defined(DECODIUM_FASTLDPC_AVX2_BUILT) || defined(DECODIUM_FASTLDPC_NEON_BUILT)
    if (Keff == 91 && useFastLdpc ()) {
        fastldpc_simd_set_ft8_mode_c (g_ft8Mode ? 1 : 0);
        fastldpc_simd_decode174_91_batch_c (n, llrIn, apmaskIn, Keff, maxosd,
                                          norder, message91Out, cwOut, ntypeOut,
                                          nharderrorOut, dminOut);
        return;
    }
#endif

    for (int word = 0; word < n; ++word) {
        fastldpc_decode174_91_c (llrIn + static_cast<std::size_t> (word) * kN,
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

// Raccolta dati per il riaddestramento del gate (tests/ft2_gate_dump.cpp):
// niente da fare senza il backend SIMD, non esiste un gate nel decoder
// originale a cui agganciarsi.
extern "C" void fastldpc_gate_dump_open_c (char const* path)
{
#if defined(DECODIUM_FASTLDPC_AVX2_BUILT) || defined(DECODIUM_FASTLDPC_NEON_BUILT)
    fastldpc_simd_gate_dump_open_c (path);
#else
    (void) path;
#endif
}

extern "C" void fastldpc_gate_dump_close_c ()
{
#if defined(DECODIUM_FASTLDPC_AVX2_BUILT) || defined(DECODIUM_FASTLDPC_NEON_BUILT)
    fastldpc_simd_gate_dump_close_c ();
#endif
}

extern "C" void fastldpc_gate_truth_set_c (signed char const* cw174)
{
#if defined(DECODIUM_FASTLDPC_AVX2_BUILT) || defined(DECODIUM_FASTLDPC_NEON_BUILT)
    fastldpc_simd_gate_truth_set_c (cw174);
#else
    (void) cw174;
#endif
}

extern "C" void fastldpc_gate_truth_clear_c ()
{
#if defined(DECODIUM_FASTLDPC_AVX2_BUILT) || defined(DECODIUM_FASTLDPC_NEON_BUILT)
    fastldpc_simd_gate_truth_clear_c ();
#endif
}
