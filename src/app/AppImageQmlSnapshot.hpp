// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QStringList>
#include <algorithm>

namespace decodium::appimage_qml {

// Only application-owned QML trees are copied. Qt's modules and native
// plugins remain in the AppImage and are resolved through the normal paths.
inline QStringList roots() { return {QStringLiteral("decodium"), QStringLiteral("dialogs"), QStringLiteral("panels")}; }

inline bool treeFiles(QString const& root, QStringList* files, bool rejectCompiled = false)
{
    QDir const base(root);
    if (!base.exists() || QFileInfo(root).isSymLink()) return false;
    if (rejectCompiled) {
        // The snapshot is also an import path: an unverified extra module at
        // its root could otherwise shadow a Qt or application import.
        auto const entries = base.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot
                                                | QDir::Hidden | QDir::System);
        for (QFileInfo const& entry : entries)
            if (entry.isSymLink() || !entry.isDir() || !roots().contains(entry.fileName())) return false;
    }
    files->clear();
    for (QString const& name : roots()) {
        QString const directory = base.filePath(name);
        if (!QFileInfo(directory).isDir() || QFileInfo(directory).isSymLink()) return false;
        QDirIterator it(directory, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString const path = it.next();
            QFileInfo const info = it.fileInfo();
            if (info.isSymLink()) return false;
            if (info.isDir()) continue;
            if (!info.isFile() || !info.isReadable()) return false;
            QString const lowerName=info.fileName().toLower();
            // Relocating a native plugin can break its relative library paths.
            // Such packages keep using the mounted tree instead.
            if (lowerName.endsWith(QStringLiteral(".so"))
                || lowerName.contains(QStringLiteral(".so."))
                || lowerName.endsWith(QStringLiteral(".dll"))
                || lowerName.endsWith(QStringLiteral(".dylib"))) return false;
            // Adjacent compiled units could have been produced by a different
            // build. Qt compiles its own cache under the versioned directory.
            if (path.endsWith(QStringLiteral(".qmlc")) || path.endsWith(QStringLiteral(".jsc"))) {
                if (rejectCompiled) return false;
                // A compiled-only application component cannot be safely
                // omitted: retain the mounted-tree fallback in that case.
                if (!QFileInfo::exists(path.left(path.size() - 1))) return false;
                continue;
            }
            files->append(base.relativeFilePath(path));
        }
    }
    std::sort(files->begin(), files->end());
    return !files->isEmpty()
        && QFileInfo(base.filePath(QStringLiteral("decodium/BootLoader.qml"))).isFile()
        && QFileInfo(base.filePath(QStringLiteral("decodium/Main.qml"))).isFile();
}

inline QByteArray treeDigest(QString const& root, QString const& salt, bool rejectCompiled = false)
{
    QStringList files;
    if (!treeFiles(root, &files, rejectCompiled)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    QByteArray const separator(1, '\0');
    hash.addData(salt.toUtf8());
    hash.addData(separator);
    QDir const base(root);
    for (QString const& relative : files) {
        QByteArray const name = relative.toUtf8();
        hash.addData(name);
        hash.addData(separator);
        QFile file(base.filePath(relative));
        if (!file.open(QIODevice::ReadOnly)) return {};
        // Explicit sizes distinguish content boundaries and empty files.
        hash.addData(QByteArray::number(file.size()));
        hash.addData(separator);
        if (!hash.addData(&file)) return {};
    }
    return hash.result().toHex();
}

// Returns an immutable, content-addressed QML directory or an empty string.
// Concurrent creators publish with a single directory rename. An existing
// snapshot is verified before use; a damaged snapshot is never repaired in
// place because another running instance may be using it.
inline QString prepare(QString const& source, QString const& cacheRoot, QString const& salt)
{
    QByteArray const digest=treeDigest(source,salt);
    if (digest.isEmpty() || cacheRoot.isEmpty()) return {};
    QDir base(cacheRoot);
    if (!base.mkpath(QStringLiteral("."))) return {};
    QString const target=base.filePath(QString::fromLatin1(digest));
    if (QFileInfo::exists(target))
        return treeDigest(target,salt,true)==digest ? target : QString();

    QTemporaryDir staging(base.filePath(QStringLiteral(".qml-stage-XXXXXX")));
    if (!staging.isValid()) return {};
    QStringList files;
    if (!treeFiles(source,&files)) return {};
    QDir const input(source), output(staging.path());
    for (QString const& name : roots())
        if (!QDir().mkpath(output.filePath(name))) return {};
    for (QString const& relative : files) {
        QString const dest=output.filePath(relative);
        if (!QDir().mkpath(QFileInfo(dest).path()) || !QFile::copy(input.filePath(relative),dest)) return {};
    }
    if (treeDigest(staging.path(),salt,true)!=digest) return {};
    if (!QDir().rename(staging.path(),target))
        return treeDigest(target,salt,true)==digest ? target : QString();
    staging.setAutoRemove(false);
    return target;
}
}
