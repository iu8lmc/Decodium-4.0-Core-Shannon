// plausible.hpp — il messaggio da 77 bit ha senso?
//
// PERCHE'. Il solo test che decide se un candidato dell'OSD e' valido e' la
// CRC-14, che lascia passare un candidato sbagliato ogni 16384. Con la ricerca
// stretta si provano ~600 candidati per parola (0,04 falsi positivi attesi),
// con quella larga ~21400 (1,3 attesi). E' per questo che allargare la ricerca
// non paga: compra candidati giusti e falsi nella stessa proporzione.
//
// Ma la CRC non e' l'unica informazione disponibile. I 77 bit del payload non
// sono un numero qualunque: sono un messaggio con una struttura. Un payload
// sorteggiato a caso quasi sempre NON descrive nominativi possibili. Usare
// quella struttura dentro il ciclo di accettazione, invece che dopo, aggiunge
// bit di filtro alla CRC-14 e sposta il punto di lavoro.
//
// COSA CONTROLLA. Solo vincoli strutturali certi, mai statistici:
//
//   - il tipo di messaggio i3 deve essere fra quelli AMMESSI. I tipi 6 e 7 non
//     esistono e vanno sempre rifiutati; gli altri si possono restringere ai
//     soli che il modo usa davvero, ed e' li' che stanno i bit. Attenzione:
//     escludere un tipo significa che un messaggio di quel tipo non verra' MAI
//     decodificato. Per i3=0 vale lo stesso per il sottotipo n3, dove pero'
//     solo 2 e 7 sono indefiniti -- il 6 e' WSPR ed e' valido;
//   - nei messaggi con nominativi, i due campi da 28 bit devono decodificare a
//     nominativi con struttura possibile: suffisso allineato a sinistra, almeno
//     una lettera di suffisso, il carattere che precede la cifra deve essere
//     una lettera;
//   - un token (CQ, DE, QRZ) nella SECONDA posizione non e' un messaggio
//     sensato;
//   - i campi a lunghezza limitata devono stare nel loro intervallo: il testo
//     libero e' 42^13 dentro 71 bit (53%), il nominativo non standard e' 38^11
//     dentro 58 bit (83%), lo scambio ARRL e' 1..8000 oppure un moltiplicatore.
//
// Nessun controllo sul locatore, sul rapporto o sulla plausibilita' geografica:
// quelli rifiuterebbero collegamenti veri. Il filtro deve essere tale che una
// parola giusta non venga MAI scartata -- il banco lo verifica.
//
// Formato dei 77 bit, come in WSJT-X (indici a base zero):
//   i3 = bit 74,75,76        tipo di messaggio
//   n3 = bit 71,72,73        sottotipo, solo quando i3 = 0
//   i3 = 1 o 2:  c28 [0..27] r [28] c28 [29..56] r [57] R [58] g15 [59..73]
#pragma once
#include <cstdint>

namespace plaus {

constexpr uint32_t kNTokens = 2063592u;
constexpr uint32_t kMax22   = 4194304u;

// Maschera dei tipi i3 ammessi, un bit per tipo. Il default tiene tutti i tipi
// definiti: non scarta nulla di valido, ma filtra meno.
constexpr uint32_t kTuttiDefiniti = 0x3Fu;               // i3 = 0..5
// Quello che un QSO FT2 usa davvero: standard e testo libero. I tipi 2, 3 e 5
// sono formati da contest che in FT2 non si vedono; il 4 serve ai nominativi
// non standard e va tenuto se se ne lavorano.
constexpr uint32_t kSoloUsati    = (1u << 0) | (1u << 1) | (1u << 4);

// 42^13 e 38^11: gli spazi validi del testo libero e del nominativo non
// standard, piu' piccoli dei campi che li contengono.
//
// Il primo non entra in 64 bit, e nemmeno il campo da 71 che lo ospita. Invece
// di ricorrere a __int128, che e' un'estensione e legherebbe l'header a GCC e
// Clang, si tiene il valore spezzato in due meta' e si confronta in ordine
// lessicografico: 42^13 = (68 << 64) + 11059121426617114624.
constexpr uint64_t k42p13_hi = 68ull;
constexpr uint64_t k42p13_lo = 11059121426617114624ull;
constexpr uint64_t k38p11    = 238572050223552512ull;

inline uint32_t bits_to_u32 (const uint8_t* b, int from, int n) {
    uint32_t v = 0;
    for (int i = 0; i < n; ++i) v = (v << 1) | (uint32_t)(b[from + i] & 1);
    return v;
}

inline uint64_t bits_to_u64 (const uint8_t* b, int from, int n) {
    uint64_t v = 0;
    for (int i = 0; i < n; ++i) v = (v << 1) | (uint64_t)(b[from + i] & 1);
    return v;
}

// Il campo da 71 bit letto in due meta': i 7 bit alti e i 64 bassi.
inline void bits_to_hi_lo (const uint8_t* b, int from, int n, uint64_t& hi, uint64_t& lo) {
    hi = 0; lo = 0;
    const int nhi = n - 64;
    for (int i = 0; i < nhi; ++i) hi = (hi << 1) | (uint64_t)(b[from + i] & 1);
    for (int i = nhi; i < n; ++i) lo = (lo << 1) | (uint64_t)(b[from + i] & 1);
}

// Decodifica un campo da 28 bit nei sei caratteri del nominativo standard.
// Ritorna false se il valore cade nelle zone token o hash, che si trattano a
// parte. L'alfabeto per posizione e' quello di WSJT-X.
inline bool call28_to_chars (uint32_t n28, char out[6]) {
    static const char A37[] = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static const char A36[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static const char A10[] = "0123456789";
    static const char A27[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    if (n28 < kNTokens + kMax22) return false;
    uint32_t n = n28 - kNTokens - kMax22;
    out[5] = A27[n % 27]; n /= 27;
    out[4] = A27[n % 27]; n /= 27;
    out[3] = A27[n % 27]; n /= 27;
    out[2] = A10[n % 10]; n /= 10;
    out[1] = A36[n % 36]; n /= 36;
    out[0] = A37[n % 37];
    return true;
}

// Struttura possibile per un nominativo: prefisso, cifra, suffisso di 1-3
// lettere allineato a sinistra.
//
// ATTENZIONE al prefisso. La prima stesura pretendeva una LETTERA prima della
// cifra, ed era sbagliata: rifiutava 12 nominativi veri su 404 presi dai log
// ADIF, cioe' i prefissi lettera+cifra S5 (Slovenia), A6 (Emirati), Z3
// (Macedonia), E7 (Bosnia), Z6 (Kosovo), N2, G5. Quelle nazioni non sarebbero
// piu' state decodificate. Il vincolo vero e' piu' debole: un prefisso puo'
// essere lettera, lettera+lettera, lettera+cifra o cifra+lettera, ma mai
// cifra+cifra ne' una sola cifra.
inline bool call_chars_ok (const char c[6]) {
    if (c[3] == ' ') return false;                       // serve almeno una lettera di suffisso
    if (c[4] == ' ' && c[5] != ' ') return false;        // suffisso con un buco dentro
    const bool c1cifra = (c[1] >= '0' && c[1] <= '9');
    const bool c0lettera = (c[0] >= 'A' && c[0] <= 'Z');
    if (c1cifra && !c0lettera) return false;             // "12..." o " 5...": nessun prefisso cosi'
    return true;
}

inline bool call28_ok (uint32_t n28, bool prima_posizione) {
    if (n28 < kNTokens) return prima_posizione;          // CQ/DE/QRZ solo come primo campo
    if (n28 < kNTokens + kMax22) return true;            // nominativo con hash: accettabile
    char c[6];
    call28_to_chars (n28, c);
    return call_chars_ok (c);
}

// Il payload da 77 bit descrive un messaggio possibile?
inline bool message77_ok (const uint8_t* b, uint32_t tipi = kTuttiDefiniti) {
    const uint32_t i3 = bits_to_u32 (b, 74, 3);
    if (i3 > 5) return false;                            // 6 e 7 non esistono
    if (!((tipi >> i3) & 1u)) return false;              // tipo non ammesso

    if (i3 == 0) {
        const uint32_t n3 = bits_to_u32 (b, 71, 3);
        if (n3 == 2 || n3 == 7) return false;            // sottotipi indefiniti
        if (n3 == 0) {                                   // testo libero: 42^13 in 71 bit
            uint64_t hi, lo;
            bits_to_hi_lo (b, 0, 71, hi, lo);
            return hi < k42p13_hi || (hi == k42p13_hi && lo < k42p13_lo);
        }
        if (n3 == 1 || n3 == 3 || n3 == 4)               // DXpedition e field day
            return call28_ok (bits_to_u32 (b, 0, 28), true)
                && call28_ok (bits_to_u32 (b, 28, 28), false);
        return true;                                     // 5 telemetria, 6 WSPR
    }
    if (i3 == 1 || i3 == 2) {                            // standard, EU VHF
        return call28_ok (bits_to_u32 (b, 0, 28), true)
            && call28_ok (bits_to_u32 (b, 29, 28), false);
    }
    if (i3 == 3) {                                       // ARRL RTTY Roundup
        if (!call28_ok (bits_to_u32 (b, 1, 28), true)) return false;
        if (!call28_ok (bits_to_u32 (b, 29, 28), false)) return false;
        const uint32_t nexch = bits_to_u32 (b, 61, 13);
        if (nexch == 0) return false;                    // ne' seriale ne' moltiplicatore
        if (nexch > 8000 && nexch - 8000 > 65) return false;
        return true;
    }
    if (i3 == 4) {                                       // nominativo non standard
        return bits_to_u64 (b, 12, 58) < k38p11;
    }
    return true;                                         // 5: EU VHF, nessun vincolo sicuro
}

}  // namespace plaus
