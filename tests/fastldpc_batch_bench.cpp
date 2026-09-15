// fastldpc_batch_bench.cpp — quanto costa una chiamata al decoder LDPC al
// variare di quante parole porta.
//
// Il min-sum vettorizzato lavora su 16 corsie per volta: una chiamata con 5
// parole costa quasi quanto una con 16, perche' il lavoro SIMD e' lo stesso.
// In aria Decodium chiama con 5,1 parole in media (una passata per ipotesi di
// un singolo candidato di sincronismo), quindi due terzi del lavoro vanno
// sprecati. Prima di riscrivere Stage4 per mettere nello stesso blocco
// candidati diversi, questo banco dice quanto varrebbe: e' il tetto.
//
// Uso: fastldpc_batch_bench [ripetizioni]

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

extern "C" {
void fastldpc_set_ft8_mode_c (int on);
void fastldpc_decode174_91_batch_c (int n, float const* llr, signed char const* apmask,
                                    int Keff, int maxosd, int norder,
                                    signed char* message91, signed char* cw,
                                    int* ntype, int* nharderror, float* dmin);
}

int main (int argc, char** argv)
{
    int const ripetizioni = argc > 1 ? std::atoi (argv[1]) : 400;
    constexpr int kN = 174;

    fastldpc_set_ft8_mode_c (1);

    std::printf ("%d ripetizioni per punto, LLR di solo rumore (caso peggiore: nessuna\n"
                 "parola chiude subito, quindi il min-sum itera fino in fondo)\n\n", ripetizioni);
    std::printf ("| parole per chiamata | ms per chiamata | us per parola | corsie usate |\n");
    std::printf ("|---:|---:|---:|---:|\n");

    double costo5 = 0.0, costo16 = 0.0;
    for (int n : {1, 5, 8, 16, 32, 64})
        {
            std::mt19937 rng (20260910u);
            std::normal_distribution<float> g (0.0f, 1.2f);
            std::vector<float> llr (static_cast<size_t> (n) * kN);
            for (auto& x : llr) x = g (rng);
            std::vector<signed char> apmask (static_cast<size_t> (n) * kN, 0);
            std::vector<signed char> msg (static_cast<size_t> (n) * 91);
            std::vector<signed char> cw (static_cast<size_t> (n) * kN);
            std::vector<int> ntype (static_cast<size_t> (n)), hard (static_cast<size_t> (n));
            std::vector<float> dmin (static_cast<size_t> (n));

            // un giro a vuoto: la prima chiamata costruisce il decoder del thread
            fastldpc_decode174_91_batch_c (n, llr.data (), apmask.data (), 91, 2, 2,
                                           msg.data (), cw.data (), ntype.data (),
                                           hard.data (), dmin.data ());

            auto const t0 = std::chrono::steady_clock::now ();
            for (int r = 0; r < ripetizioni; ++r)
                fastldpc_decode174_91_batch_c (n, llr.data (), apmask.data (), 91, 2, 2,
                                               msg.data (), cw.data (), ntype.data (),
                                               hard.data (), dmin.data ());
            auto const t1 = std::chrono::steady_clock::now ();

            double const ms = std::chrono::duration<double, std::milli> (t1 - t0).count () / ripetizioni;
            std::printf ("| %d | %.3f | %.1f | %d |\n", n, ms, 1000.0 * ms / n, ((n + 15) / 16) * 16);
            if (n == 5) costo5 = ms;
            if (n == 16) costo16 = ms;
        }

    if (costo5 > 0.0 && costo16 > 0.0)
        {
            double const oggi = costo5 / 5.0;             // ms per parola come chiama Decodium
            double const pieno = costo16 / 16.0;          // ms per parola a corsie piene
            std::printf ("\nOggi Decodium chiama con 5,1 parole: %.1f us per parola.\n", 1000.0 * oggi);
            std::printf ("A 16 corsie piene sarebbero %.1f us per parola, cioe' %.2fx.\n",
                         1000.0 * pieno, oggi / pieno);
            std::printf ("Su 9096 parole per slot: %.0f ms contro %.0f ms, %.0f ms risparmiati.\n",
                         9096.0 * oggi, 9096.0 * pieno, 9096.0 * (oggi - pieno));
        }
    return 0;
}
