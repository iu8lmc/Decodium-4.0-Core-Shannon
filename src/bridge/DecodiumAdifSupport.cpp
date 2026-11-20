#include "AdifDocumentInternal.h"
#include <QFile>
#include <QSet>

using namespace decodium::adif_document;

bool decodiumBridgeWriteAdifDocument(const QString& path, const ParsedAdifDocument& doc)
{
    return writeAdifDocument(path, doc);
}

void rebuildWorkedCallsFromDocument(QSet<QString>& workedCalls, QList<ParsedAdifRecord> const& records)
{
    workedCalls.clear();
    for (ParsedAdifRecord const& record : records) {
        QString const call = record.fields.value(QStringLiteral("CALL")).trimmed().toUpper();
        if (!call.isEmpty()) {
            workedCalls.insert(call);
        }
    }
}

static int countAdifRecordsLightweight(QString const& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }

    int count = 0;
    QByteArray carry;
    while (!file.atEnd()) {
        QByteArray chunk = carry + file.read(64 * 1024).toUpper();
        int cursor = 0;
        while (true) {
            int const pos = chunk.indexOf("<EOR", cursor);
            if (pos < 0) {
                break;
            }
            ++count;
            cursor = pos + 4;
        }
        carry = chunk.right(3);
    }
    return count;
}
int decodiumBridgeCountAdifRecords(const QString& path)
{
    return countAdifRecordsLightweight(path);
}


void setAdifModeFields(QMap<QString, QString>& fields, QString const& modeValue)
{
    QString const normalized = modeValue.trimmed().toUpper();
    if (normalized.isEmpty()) {
        return;
    }

    if (normalized == QStringLiteral("FT2")
        || normalized == QStringLiteral("FT4")
        || normalized == QStringLiteral("FST4")
        || normalized == QStringLiteral("Q65")
        || normalized.startsWith(QStringLiteral("Q65-"))) {
        fields.insert(QStringLiteral("MODE"), QStringLiteral("MFSK"));
        fields.insert(QStringLiteral("SUBMODE"), normalized);
        return;
    }

    fields.insert(QStringLiteral("MODE"), normalized);
    fields.remove(QStringLiteral("SUBMODE"));
}

