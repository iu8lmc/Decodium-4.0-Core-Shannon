#include "DecodiumBridge.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMetaObject>
#include <QMutex>
#include <QPointer>
#include <QRegularExpression>
#include <QRunnable>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThreadPool>
#include <QUrl>
#include <QWindow>

static QString localDialogPath(QString path)
{
    path = path.trimmed();
    if (path.startsWith(QLatin1String("file:")))
        path = QUrl(path).toLocalFile();
    if (path.isEmpty())
        return QDir::homePath();
    return QDir::toNativeSeparators(path);
}

static QString safeFt2LinkReceivedDirectoryName(QString name)
{
    name = name.trimmed();
    name.replace(
        QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*\\x00-\\x1f]")),
        QStringLiteral("_"));
    while (name.endsWith(QLatin1Char(' '))
           || name.endsWith(QLatin1Char('.'))) {
        name.chop(1);
    }
    return name.isEmpty() ? QStringLiteral("Default") : name.left(80);
}

static QString ft2LinkReceivedText(char const* sourceText)
{
    return QCoreApplication::translate("FT2LinkPanel", sourceText);
}

static QString defaultFt2LinkReceivedFilesDirectoryPath()
{
    QString root = QStandardPaths::writableLocation(
        QStandardPaths::DownloadLocation);
    if (root.trimmed().isEmpty()) {
        root = QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation);
    }
    if (root.trimmed().isEmpty()) {
        root = QDir::homePath();
    }

    QString instanceName = QCoreApplication::applicationName().trimmed();
    qsizetype const separator = instanceName.lastIndexOf(
        QStringLiteral(" - "));
    if (separator >= 0) {
        instanceName = instanceName.mid(separator + 3).trimmed();
    } else {
        instanceName = QStringLiteral("Default");
    }
    instanceName = safeFt2LinkReceivedDirectoryName(instanceName);

    return QDir(root).absoluteFilePath(
        QStringLiteral("Decodium/FT2-Link Received/%1").arg(instanceName));
}

static QString localDirectoryPathOrDefault(QString path)
{
    path = path.trimmed();
    if (path.startsWith(QLatin1String("file:"))) {
        path = QUrl(path).toLocalFile();
    }
    if (path.isEmpty()) {
        path = defaultFt2LinkReceivedFilesDirectoryPath();
    }
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

static QString safeFt2LinkReceivedFileName(QString fileName)
{
    fileName = fileName.trimmed();
    fileName.replace(QLatin1Char('\\'), QLatin1Char('/'));
    fileName = fileName.section(QLatin1Char('/'), -1);
    fileName.replace(
        QRegularExpression(QStringLiteral("[<>:\"/\\\\|?*\\x00-\\x1f]")),
        QStringLiteral("_"));
    while (fileName.endsWith(QLatin1Char(' '))
           || fileName.endsWith(QLatin1Char('.'))) {
        fileName.chop(1);
    }
    if (fileName.isEmpty() || fileName == QLatin1String(".")
        || fileName == QLatin1String("..")) {
        fileName = QStringLiteral("ft2link_received.bin");
    }

    QString const baseName = QFileInfo(fileName).completeBaseName().toUpper();
    static QRegularExpression const windowsReserved(
        QStringLiteral("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$"),
        QRegularExpression::CaseInsensitiveOption);
    if (windowsReserved.match(baseName).hasMatch()) {
        fileName.prepend(QLatin1Char('_'));
    }

    constexpr qsizetype kMaxFileNameLength = 180;
    if (fileName.size() > kMaxFileNameLength) {
        QFileInfo const info(fileName);
        QString const suffix = info.suffix();
        QString stem = info.completeBaseName();
        qsizetype const suffixSpace = suffix.isEmpty() ? 0 : suffix.size() + 1;
        stem.truncate(qMax<qsizetype>(1,
                                     kMaxFileNameLength - suffixSpace));
        fileName = suffix.isEmpty()
            ? stem
            : stem + QLatin1Char('.') + suffix;
    }
    return fileName;
}

static QString uniqueFt2LinkReceivedFilePath(QDir const& directory,
                                             QString const& safeFileName)
{
    QString candidate = directory.absoluteFilePath(safeFileName);
    if (!QFileInfo::exists(candidate)) {
        return candidate;
    }

    QFileInfo const info(safeFileName);
    QString const stem = info.completeBaseName().isEmpty()
        ? QStringLiteral("ft2link_received")
        : info.completeBaseName();
    QString const suffix = info.suffix();
    for (int index = 2; index < 100000; ++index) {
        QString const numbered = suffix.isEmpty()
            ? QStringLiteral("%1 (%2)").arg(stem).arg(index)
            : QStringLiteral("%1 (%2).%3").arg(stem).arg(index).arg(suffix);
        candidate = directory.absoluteFilePath(numbered);
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }

    return directory.absoluteFilePath(
        QStringLiteral("%1-%2%3")
            .arg(stem)
            .arg(QDateTime::currentDateTimeUtc().toString(
                QStringLiteral("yyyyMMdd-HHmmss-zzz")))
            .arg(suffix.isEmpty() ? QString() : QLatin1Char('.') + suffix));
}

static QVariantMap writeFt2LinkReceivedFile(QString const& requestedDirectory,
                                            QString const& requestedFileName,
                                            QString const& text,
                                            QString const& base64,
                                            bool binary,
                                            bool preserveExisting)
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);
    result.insert(QStringLiteral("path"), QString());
    result.insert(QStringLiteral("directory"), QString());
    result.insert(QStringLiteral("fileName"), QString());
    result.insert(QStringLiteral("bytes"), 0);

    QString const directoryPath =
        localDirectoryPathOrDefault(requestedDirectory);
    QDir directory(directoryPath);
    if (!directory.exists() && !QDir().mkpath(directoryPath)) {
        result.insert(QStringLiteral("error"),
                      ft2LinkReceivedText(
                          "Cannot create receive directory: %1")
                          .arg(directoryPath));
        return result;
    }

    QString const safeFileName =
        safeFt2LinkReceivedFileName(requestedFileName);
    // Multiple automatic saves may complete in parallel.  Serialize only the
    // short destination-selection/commit section on the worker pool so two
    // files with the same radio-provided name cannot overwrite one another.
    static QMutex receivedFileWriteMutex;
    QMutexLocker const writeLock(&receivedFileWriteMutex);
    QString const targetPath = preserveExisting
        ? uniqueFt2LinkReceivedFilePath(directory, safeFileName)
        : directory.absoluteFilePath(safeFileName);

    QByteArray bytes;
    if (binary) {
        QByteArray const encoded = base64.trimmed().toLatin1();
        if (encoded.isEmpty()) {
            result.insert(QStringLiteral("error"),
                          ft2LinkReceivedText(
                              "Received binary file has no data"));
            return result;
        }
        bytes = QByteArray::fromBase64(
            encoded, QByteArray::AbortOnBase64DecodingErrors);
        if (bytes.isEmpty()) {
            result.insert(QStringLiteral("error"),
                          ft2LinkReceivedText(
                              "Received binary file has invalid Base64 data"));
            return result;
        }
    } else {
        bytes = text.toUtf8();
    }

    QSaveFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        result.insert(QStringLiteral("error"), file.errorString());
        result.insert(QStringLiteral("path"), targetPath);
        return result;
    }
    if (file.write(bytes) != bytes.size()) {
        result.insert(QStringLiteral("error"), file.errorString());
        result.insert(QStringLiteral("path"), targetPath);
        file.cancelWriting();
        return result;
    }
    if (!file.commit()) {
        result.insert(QStringLiteral("error"), file.errorString());
        result.insert(QStringLiteral("path"), targetPath);
        return result;
    }

    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("path"), QDir::toNativeSeparators(targetPath));
    result.insert(QStringLiteral("directory"),
                  QDir::toNativeSeparators(directoryPath));
    result.insert(QStringLiteral("fileName"), QFileInfo(targetPath).fileName());
    result.insert(QStringLiteral("bytes"), bytes.size());
    result.insert(QStringLiteral("error"), QString());
    return result;
}

static QString dialogFilterString(const QStringList& nameFilters)
{
    return nameFilters.isEmpty() ? QString() : nameFilters.join(QStringLiteral(";;"));
}

static QWidget* dialogParentWidget()
{
    if (auto* modal = QApplication::activeModalWidget()) {
        return modal;
    }
    if (auto* active = QApplication::activeWindow()) {
        return active;
    }
    for (auto* widget : QApplication::topLevelWidgets()) {
        if (widget && widget->isVisible()) {
            return widget;
        }
    }
    return nullptr;
}

static QWindow* dialogOwnerWindow(QWidget* widgetParent)
{
    if (widgetParent) {
        if (auto* window = widgetParent->windowHandle()) {
            return window;
        }
    }

    // Detached QML windows (including the QSO log) are QQuickWindow objects,
    // not QWidget instances.  QApplication::activeWindow() therefore does
    // not find them, leaving a QFileDialog without an owner on macOS and
    // allowing it to open behind the application.
    if (auto* focused = QGuiApplication::focusWindow()) {
        if (focused->isVisible()
            && focused->visibility() != QWindow::Minimized) {
            return focused;
        }
    }

    for (auto* window : QGuiApplication::allWindows()) {
        if (window && window->isVisible()
            && window->visibility() != QWindow::Minimized) {
            return window;
        }
    }
    return nullptr;
}

static void prepareFileDialogForActivation(QFileDialog& dialog,
                                           QWindow* ownerWindow)
{
    dialog.setWindowModality(Qt::ApplicationModal);
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    // A detached logbook can be always-on-top. Its dialog must not be
    // hidden behind it, otherwise the modal operation appears frozen.
    if (ownerWindow && ownerWindow->flags().testFlag(Qt::WindowStaysOnTopHint))
        dialog.setWindowFlag(Qt::WindowStaysOnTopHint, true);

    // Create the native window handle before showing it so the transient
    // relationship is established before the window enters the desktop.
    dialog.winId();
    if (auto* dialogWindow = dialog.windowHandle()) {
        if (ownerWindow && ownerWindow != dialogWindow) {
            dialogWindow->setTransientParent(ownerWindow);
        }
    }

    dialog.show();
    dialog.raise();
    dialog.activateWindow();
    if (auto* dialogWindow = dialog.windowHandle()) {
        dialogWindow->raise();
        dialogWindow->requestActivate();
    }
}

QString DecodiumBridge::openFileDialog(const QString& title,
                                       const QString& initialPath,
                                       const QStringList& nameFilters) const
{
    QWidget* const widgetParent = dialogParentWidget();
    QWindow* const ownerWindow = dialogOwnerWindow(widgetParent);
    QFileDialog dialog(widgetParent,
                       title,
                       localDialogPath(initialPath),
                       dialogFilterString(nameFilters));
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    prepareFileDialogForActivation(dialog, ownerWindow);
    if (dialog.exec() != QDialog::Accepted) {
        return QString();
    }
    QStringList const files = dialog.selectedFiles();
    return files.isEmpty() ? QString() : files.first();
}

QString DecodiumBridge::openFileDialogForWindow(const QString& title,
                                                const QString& initialPath,
                                                const QStringList& nameFilters,
                                                QObject* owner) const
{
    auto* ownerWindow = qobject_cast<QWindow*>(owner);
    if (!ownerWindow || !ownerWindow->isVisible())
        return openFileDialog(title, initialPath, nameFilters);

    QFileDialog dialog(nullptr, title, localDialogPath(initialPath),
                       dialogFilterString(nameFilters));
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    prepareFileDialogForActivation(dialog, ownerWindow);
    if (dialog.exec() != QDialog::Accepted)
        return QString();
    const QStringList files = dialog.selectedFiles();
    return files.isEmpty() ? QString() : files.first();
}

QString DecodiumBridge::saveFileDialog(const QString& title,
                                       const QString& initialPath,
                                       const QStringList& nameFilters) const
{
    return QFileDialog::getSaveFileName(QApplication::activeWindow(),
                                        title,
                                        localDialogPath(initialPath),
                                        dialogFilterString(nameFilters));
}

QString DecodiumBridge::openDirectoryDialog(const QString& title,
                                            const QString& initialPath) const
{
    return QFileDialog::getExistingDirectory(QApplication::activeWindow(),
                                             title,
                                             localDialogPath(initialPath));
}

QString DecodiumBridge::defaultFt2LinkReceivedFilesDirectory() const
{
    return QDir::toNativeSeparators(
        defaultFt2LinkReceivedFilesDirectoryPath());
}

quint64 DecodiumBridge::saveFt2LinkReceivedFileAsync(
    const QString& directory,
    const QString& fileName,
    const QString& text,
    const QString& base64,
    bool binary,
    bool preserveExisting)
{
    quint64 const requestId =
        m_ft2LinkReceivedFileIoSerial.fetch_add(
            1u, std::memory_order_relaxed) + 1u;
    QPointer<DecodiumBridge> guard(this);
    auto* task = QRunnable::create([
        guard,
        requestId,
        directory,
        fileName,
        text,
        base64,
        binary,
        preserveExisting
    ] {
        QVariantMap const result = writeFt2LinkReceivedFile(
            directory, fileName, text, base64, binary, preserveExisting);
        if (!guard) {
            return;
        }
        QMetaObject::invokeMethod(guard, [guard, requestId, result] {
            if (guard) {
                emit guard->ft2LinkReceivedFileSaveFinished(requestId, result);
            }
        }, Qt::QueuedConnection);
    });
    QThreadPool::globalInstance()->start(task, -1);
    return requestId;
}

quint64 DecodiumBridge::openFt2LinkReceivedFilesDirectoryAsync(
    const QString& directory)
{
    quint64 const requestId =
        m_ft2LinkReceivedFileIoSerial.fetch_add(
            1u, std::memory_order_relaxed) + 1u;
    QString const path = localDirectoryPathOrDefault(directory);
    QPointer<DecodiumBridge> guard(this);
    auto* task = QRunnable::create([guard, requestId, path] {
        QVariantMap result;
        bool const ok = QDir(path).exists() || QDir().mkpath(path);
        result.insert(QStringLiteral("ok"), ok);
        result.insert(QStringLiteral("path"),
                      QDir::toNativeSeparators(path));
        result.insert(QStringLiteral("error"),
                      ok ? QString()
                         : ft2LinkReceivedText(
                               "Cannot create receive directory: %1")
                               .arg(path));
        if (!guard) {
            return;
        }
        QMetaObject::invokeMethod(guard, [guard, requestId, result] () mutable {
            if (!guard) {
                return;
            }
            if (result.value(QStringLiteral("ok")).toBool()) {
                bool const opened = QDesktopServices::openUrl(
                    QUrl::fromLocalFile(
                        result.value(QStringLiteral("path")).toString()));
                if (!opened) {
                    result.insert(QStringLiteral("ok"), false);
                    result.insert(
                        QStringLiteral("error"),
                        ft2LinkReceivedText(
                            "The system file manager could not open the receive directory"));
                }
            }
            emit guard->ft2LinkReceivedFilesDirectoryOpenFinished(
                requestId, result);
        }, Qt::QueuedConnection);
    });
    QThreadPool::globalInstance()->start(task, -1);
    return requestId;
}

