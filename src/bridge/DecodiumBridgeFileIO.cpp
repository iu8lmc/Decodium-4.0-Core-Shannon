#include "DecodiumBridge.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUrl>
#include <QVariantMap>

QVariantMap DecodiumBridge::readTextFile(const QString& path,
                                         int maxBytes) const
{
    QVariantMap result;
    QString resolved = path.trimmed();
    if (resolved.startsWith(QLatin1String("file:"))) {
        resolved = QUrl(resolved).toLocalFile();
    }
    int const limit = qBound(1, maxBytes, 65536);
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("path"), resolved);
    result.insert(QStringLiteral("text"), QString());
    result.insert(QStringLiteral("bytes"), 0);
    result.insert(QStringLiteral("fileSize"), 0);
    result.insert(QStringLiteral("truncated"), false);

    if (resolved.isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Missing file path"));
        return result;
    }

    QFile file(resolved);
    if (!file.open(QIODevice::ReadOnly)) {
        result.insert(QStringLiteral("error"), file.errorString());
        return result;
    }

    result.insert(QStringLiteral("fileSize"), QFileInfo(file).size());
    QByteArray bytes = file.read(limit + 1);
    bool const truncated = bytes.size() > limit || !file.atEnd();
    if (bytes.size() > limit) {
        bytes.truncate(limit);
    }

    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("text"), QString::fromUtf8(bytes));
    result.insert(QStringLiteral("bytes"), bytes.size());
    result.insert(QStringLiteral("truncated"), truncated);
    result.insert(QStringLiteral("error"), QString());
    return result;
}

QVariantMap DecodiumBridge::writeTextFile(const QString& path,
                                          const QString& text) const
{
    QVariantMap result;
    QString resolved = path.trimmed();
    if (resolved.startsWith(QLatin1String("file:"))) {
        resolved = QUrl(resolved).toLocalFile();
    }
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("path"), resolved);
    QByteArray const bytes = text.toUtf8();
    result.insert(QStringLiteral("bytes"), bytes.size());

    if (resolved.isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Missing file path"));
        return result;
    }

    QFileInfo const info(resolved);
    QDir const dir = info.absoluteDir();
    if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) {
        result.insert(QStringLiteral("error"),
                      QStringLiteral("Cannot create directory %1")
                          .arg(dir.absolutePath()));
        return result;
    }

    QSaveFile file(resolved);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        result.insert(QStringLiteral("error"), file.errorString());
        return result;
    }
    if (file.write(bytes) != bytes.size()) {
        result.insert(QStringLiteral("error"), file.errorString());
        return result;
    }
    if (!file.commit()) {
        result.insert(QStringLiteral("error"), file.errorString());
        return result;
    }

    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("error"), QString());
    return result;
}

QVariantMap DecodiumBridge::readFileBytes(const QString& path,
                                          int maxBytes) const
{
    QVariantMap result;
    QString resolved = path.trimmed();
    if (resolved.startsWith(QLatin1String("file:"))) {
        resolved = QUrl(resolved).toLocalFile();
    }
    int const limit = qBound(1, maxBytes, 65536);
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("path"), resolved);
    result.insert(QStringLiteral("base64"), QString());
    result.insert(QStringLiteral("bytes"), 0);
    result.insert(QStringLiteral("fileSize"), 0);
    result.insert(QStringLiteral("truncated"), false);

    if (resolved.isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Missing file path"));
        return result;
    }

    QFile file(resolved);
    if (!file.open(QIODevice::ReadOnly)) {
        result.insert(QStringLiteral("error"), file.errorString());
        return result;
    }

    result.insert(QStringLiteral("fileSize"), QFileInfo(file).size());
    QByteArray bytes = file.read(limit + 1);
    bool const truncated = bytes.size() > limit || !file.atEnd();
    if (bytes.size() > limit) {
        bytes.truncate(limit);
    }

    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("base64"), QString::fromLatin1(bytes.toBase64()));
    result.insert(QStringLiteral("bytes"), bytes.size());
    result.insert(QStringLiteral("truncated"), truncated);
    result.insert(QStringLiteral("error"), QString());
    return result;
}

QVariantMap DecodiumBridge::writeFileBytes(const QString& path,
                                           const QString& base64) const
{
    QVariantMap result;
    QString resolved = path.trimmed();
    if (resolved.startsWith(QLatin1String("file:"))) {
        resolved = QUrl(resolved).toLocalFile();
    }
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("path"), resolved);

    QByteArray const bytes = QByteArray::fromBase64(
        base64.trimmed().toLatin1(), QByteArray::Base64Encoding);
    result.insert(QStringLiteral("bytes"), bytes.size());

    if (resolved.isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Missing file path"));
        return result;
    }
    if (base64.trimmed().isEmpty()) {
        result.insert(QStringLiteral("error"), QStringLiteral("Missing file data"));
        return result;
    }

    QFileInfo const info(resolved);
    QDir const dir = info.absoluteDir();
    if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) {
        result.insert(QStringLiteral("error"),
                      QStringLiteral("Cannot create directory %1")
                          .arg(dir.absolutePath()));
        return result;
    }

    QSaveFile file(resolved);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        result.insert(QStringLiteral("error"), file.errorString());
        return result;
    }
    if (file.write(bytes) != bytes.size()) {
        result.insert(QStringLiteral("error"), file.errorString());
        return result;
    }
    if (!file.commit()) {
        result.insert(QStringLiteral("error"), file.errorString());
        return result;
    }

    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("error"), QString());
    return result;
}

