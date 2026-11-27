#include "DecodiumBridge.h"
#include <QFile>
#include <QTextStream>

extern QString decodiumBridgeExpandedLocalFilePath(QString);
extern bool decodiumBridgeEnsureParentDirectoryForFile(QString const&, QString*);

bool DecodiumBridge::exportCabrillo(const QString& filename)
{
    if (filename.isEmpty()) return false;

    // Leggi il log da all.txt (formato custom IU8LMC)
    QString logPath = logAllTxtPath();
    QFile logFile(logPath);
    if (!logFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorMessage("Log non trovato: " + logPath);
        return false;
    }

    QString const targetPath = decodiumBridgeExpandedLocalFilePath(filename);
    QString parentError;
    if (!decodiumBridgeEnsureParentDirectoryForFile(targetPath, &parentError)) {
        emit errorMessage(parentError);
        return false;
    }

    QFile out(targetPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit errorMessage("Impossibile scrivere: " + targetPath);
        return false;
    }

    QTextStream ts(&out);
    // Cabrillo header
    ts << "START-OF-LOG: 3.0\n";
    ts << "CALLSIGN: " << m_callsign << "\n";
    ts << "CONTEST: " << (m_contestExchange.isEmpty() ? "GENERIC-FT" : m_contestExchange) << "\n";
    ts << "CREATED-BY: Decodium 3.0\n";
    ts << "CATEGORY-OPERATOR: SINGLE-OP\n";
    ts << "CATEGORY-MODE: DIGI\n";
    ts << "OPERATORS: " << m_callsign << "\n";

    // Converti ogni riga log → QSO Cabrillo
    QTextStream in(&logFile);
    int qsoCount = 0;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        // formato: YYYYMMDD_HHMMSS MYCALL DXCALL DXGRID freq MODE
        QStringList f = line.split(' ', Qt::SkipEmptyParts);
        if (f.size() < 6) continue;
        QString dt = f[0];   // YYYYMMDD_HHMMSS
        QString dxCall = f[2];
        QString freq = f[4]; // e.g. "14.074000MHz"
        QString mode = f[5];
        // Parse date/time
        QString date = dt.left(8);     // YYYYMMDD
        QString time = dt.mid(9, 6);   // HHMMSS
        // Format: QSO: freq mode date time mycall rst dxcall rst
        QString freqKhz = freq.replace("MHz","").trimmed();
        bool ok;
        double fmhz = freqKhz.toDouble(&ok);
        if (ok) freqKhz = QString::number((int)(fmhz * 1000));
        ts << QString("QSO: %1 %2 %3 %4 %5 59 %6 59\n")
              .arg(freqKhz, -6)
              .arg(mode, -6)
              .arg(date.mid(0,4) + "-" + date.mid(4,2) + "-" + date.mid(6,2))
              .arg(time.left(4))
              .arg(m_callsign, -13)
              .arg(dxCall, -13);
        ++qsoCount;
    }

    ts << "END-OF-LOG:\n";
    emit statusMessage(QString("Cabrillo esportato: %1 QSO → %2").arg(qsoCount).arg(targetPath));
    return true;
}

