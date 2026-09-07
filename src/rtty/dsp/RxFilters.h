// DecoRTTY — filtri di ingresso contro il QRM.
//
// Il demodulatore a filtri adattati è già selettivo: integra su un bit e vede
// una banda di una quarantina di hertz per tono. Ma lavora sul *rapporto* fra i
// due toni, e una portante forte dentro uno dei due lo falsa comunque. Questi
// filtri stanno prima, e tolgono di mezzo ciò che il rivelatore non dovrebbe
// nemmeno vedere.
//
// Tre stadi, tutti disinseribili:
//   passa-banda  — scarta tutto ciò che sta fuori dalla coppia di toni
//   notch        — cancella una frequenza che l'operatore indica sul waterfall
//   LMS          — cancella da sé le portanti stazionarie
#pragma once

#include <vector>

namespace decortty::dsp {

// Passa-banda a fase lineare attorno alla coppia di toni. Fase lineare perché
// una risposta di fase storta sposta le transizioni dei bit in modo diverso a
// seconda della frequenza, e il recupero del clock la paga.
class ToneBandpass {
public:
    void configure(float centreHz, float widthHz, int sampleRate, int taps = 129);
    void reset();
    bool isConfigured() const { return !m_taps.empty(); }

    void process(const float* in, int count, float* out);

private:
    std::vector<float> m_taps;
    std::vector<float> m_history;
    int                m_pos{0};
};

// Elimina-banda su una frequenza scelta. Serve contro una portante ferma, che è
// il disturbo più comune e più dannoso: cade dentro un filtro di tono e lo tiene
// alto per sempre.
class NotchFilter {
public:
    void configure(float hz, float widthHz, int sampleRate, int taps = 129);
    void reset();
    bool isConfigured() const { return !m_taps.empty(); }
    float frequency() const { return m_hz; }

    void process(const float* in, int count, float* out);

private:
    std::vector<float> m_taps;
    std::vector<float> m_history;
    int                m_pos{0};
    float              m_hz{0.0f};
};

// Cancellatore adattivo (Leaky LMS). Predice il segnale a partire dal proprio
// passato e sottrae la predizione: ciò che è prevedibile — una portante, un tono
// fermo — sparisce, ciò che non lo è resta.
//
// Il ritardo di decorrelazione decide cosa considerare prevedibile. Deve essere
// abbastanza lungo da non riuscire a predire il RTTY stesso, che i suoi toni li
// cambia in continuazione: un ritardo troppo corto e il filtro cancella proprio
// il segnale che si vuole leggere. È il motivo per cui questo stadio è spento di
// default e va acceso solo quando serve.
class LmsCanceller {
public:
    void configure(int taps, float mu, int delay);
    void reset();
    bool isConfigured() const { return !m_weights.empty(); }

    void process(const float* in, int count, float* out);

private:
    std::vector<float> m_weights;
    std::vector<float> m_delayLine;
    int                m_pos{0};
    int                m_delay{8};
    float              m_mu{0.003f};
    // Il termine di perdita: senza, i pesi vagano finché il filtro non diventa
    // instabile su un ingresso che resta a lungo silenzioso.
    float              m_leak{0.9999f};
};

} // namespace decortty::dsp
