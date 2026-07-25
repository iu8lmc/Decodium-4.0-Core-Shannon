#ifndef DECODIUM_QSO_SEQUENCER_STATE_HPP
#define DECODIUM_QSO_SEQUENCER_STATE_HPP

// Fase 1 port mobile — step B strangler (doc/mobile-port-plan.md): lo stato
// mutabile del sequencer QSO, raggruppato in un POD portabile che desktop e
// decodium-core mobile condividono. Modello: MamQsoSlot (DecodiumBridge.h), che
// già replica lo stato mono-QSO in un POD. In DecodiumBridge i vecchi membri
// m_* diventano riferimenti-alias a questi campi (stessi nomi/default) → zero
// modifiche ai call-site; allo step D il QsoSequencer estratto prenderà
// QsoSequencerState& e i riferimenti-alias spariranno.

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QtGlobal>

namespace decodium
{
namespace seq
{

struct QsoSequencerState
{
    // --- Progressione QSO (step B2) ---
    int     currentTx {1};     // 1..6 step FT8 corrente
    QString dxCall;            // callsign partner corrente
    QString dxGrid;            // grid partner (se noto)
    int     qsoProgress {0};   // 0=IDLE 1=CQ 2=REPLY 3=REPORT 4=ROGER 5=SIGNOFF
    QString reportSent;        // report che invio (da SNR ricevuto)
    QString reportReceived {QStringLiteral("-10")};  // report ricevuto (default -10 dB)
    bool    sendRR73 {true};   // true=RR73, false=RRR

    // --- Deferred / pending sequencing (step B3) ---
    QDateTime qsoStartedOn;               // epoch apertura QSO (per log On)
    bool    logAfterOwn73 {false};        // logga dopo il nostro 73
    bool    ft2DeferredLogPending {false};// attesa ack finale prima del log
    int     cqAutoReplyArmSecond {-1};    // secondo armato per auto-reply a CQ
    qint64  cqAutoReplyArmWallMs {0};
    int     pendingAutoSeqTxAfterActiveTx {0};  // TX da eseguire dopo il TX attivo
    QString pendingAutoSeqPartnerBase;
    QString pendingAutoSeqMessage;
    QString pendingAutoSeqMode;
    bool    quickPeerSignaled {false};
    bool    qsoLogged {false};            // anti-doppio log per QSO corrente

    // --- Blocco "TxController clone" (1° increment step B) ---
    int  nTx73         {0};    // 73/RR73 completati nel QSO corrente
    int  txRetryCount  {0};    // invii di lastNtx senza risposta
    int  lastNtx       {-1};   // ultimo TX number inviato
    int  lastCqPidx    {-1};   // period index dell'ultimo CQ (evita CQ consecutivi)
    QString lastAutoSeqKey;    // deduplicazione autoSequenceStep
    qint64  lastAutoSeqMs {0}; // timestamp ultima deduplicazione
    QHash<QString, qint64>  recentDirectedReportDecodeMs;
    QHash<QString, QString> recentDirectedReportDecodeMessage;
    QString lastTransmittedMessage;
    QString autoSeqRogerReportBase;
    int     activeTxNumber {0};
    QString activeTxMessage;

    // --- Step B4 (port autoSequenceStep): latch AutoCQ + identità autoseq ---
    int     autoCQPeriodsMissed {0};   // periodi CQ senza risposta (watchdog count-based)
    QString autoCqLockedCall;          // call agganciato in AutoCQ FT2
    QString autoCqLockedGrid;
    int     autoCqLockedNtx {6};
    int     autoCqLockedProgress {0};
    QString lastAutoSeqDecodeIdentity; // identità dell'ultimo decode processato
};

}
}

#endif
