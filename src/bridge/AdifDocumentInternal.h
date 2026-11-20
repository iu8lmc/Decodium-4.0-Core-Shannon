#pragma once
#include <QByteArray>
#include <QVariantMap>
#include <functional>
#include <QList>
#include <QMap>
#include <QString>

struct ParsedAdifRecord
{
    QMap<QString, QString> fields;
};

struct ParsedAdifDocument
{
    QString header;
    QList<ParsedAdifRecord> records;
    bool loaded {false};
};


namespace decodium::adif_document {
using AdifImportProgressCallback = std::function<void(int, const QString&)>;
QVariantMap importAdifDocumentInBackground(QString const&, QString const&, AdifImportProgressCallback const&);
QVariantMap importConfirmedAdifDocumentInBackground(QString const&, QString const&, QString const&);
QString adifDigitsOnly(QString const& value);
QString normalizedAdifDateToken(QString const& value);
QString normalizedAdifTimeToken(QString const& value);
QString normalizeAdifModeForDisplay(QMap<QString, QString> const& fields);
QString normalizedAdifDateTime(QMap<QString, QString> const& fields);
quint64 adifFrequencyHzFromFields(QMap<QString, QString> const& fields);
QString adifBandFromFields(QMap<QString, QString> const& fields);
ParsedAdifRecord normalizeImportedAdifRecord(ParsedAdifRecord record);
QString adifRecordDedupeKey(QMap<QString, QString> const& fields);
QString freqHzToBandToken(quint64 freqHz);
QList<ParsedAdifRecord> parseAdifRecordsBytes(QByteArray const& bodyBytes);
ParsedAdifDocument loadAdifDocument(QString const& path);
bool writeAdifDocument(QString const& path, ParsedAdifDocument const& doc);
}
