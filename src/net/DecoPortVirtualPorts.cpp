// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecoPortVirtualPorts.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QSettings>
#include <QSysInfo>
#include <QThread>

#include <algorithm>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cfgmgr32.h>
#include <setupapi.h>
#include <shellapi.h>
#endif

namespace DecoPortVirtualPorts {

namespace {

#if defined(Q_OS_WIN)
constexpr int kInstallTimeoutS = 300;
constexpr int kCreateTimeoutS = 60;
#endif

// Gli installatori che DecoPort accetta di eseguire come amministratore: quelli
// di com0com 3.0.0.0 firmati, come pubblicati su SourceForge. Un file diverso
// con lo stesso nome non parte (third_party/com0com/NOTICE.md).
struct Installer {
    const char* arch;
    const char* file;
    const char* sha256;
};
constexpr Installer kInstallers[] = {
    {"x86_64", "Setup_com0com_v3.0.0.0_W7_x64_signed.exe",
     "26486b28604b49a9008c54feb11b9ece0008a8287ee5caf0bcf2a62f4317128f"},
    {"i386",   "Setup_com0com_v3.0.0.0_W7_x86_signed.exe",
     "de917425e022839f6374b509e3d34f3f9e9773915a44996194442337eadf4a5a"},
};

QString sha256Of(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash h(QCryptographicHash::Sha256);
    h.addData(&f);
    return QString::fromLatin1(h.result().toHex());
}

#if defined(Q_OS_WIN)
// Esegue un programma con l'elevazione e ne aspetta la fine. -1 se non parte;
// `cancelled` dice se e' stato l'utente a dire di no.
int runElevated(const QString& exe, const QString& params, const QString& dir,
                int timeoutS, bool* cancelled)
{
    *cancelled = false;
    const std::wstring file = QDir::toNativeSeparators(exe).toStdWString();
    const std::wstring wdir = QDir::toNativeSeparators(dir).toStdWString();
    const std::wstring wparams = params.toStdWString();
    SHELLEXECUTEINFOW sei;
    ZeroMemory(&sei, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = file.c_str();
    sei.lpParameters = wparams.c_str();
    sei.lpDirectory = wdir.c_str();
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei)) {
        *cancelled = GetLastError() == ERROR_CANCELLED;
        return -1;
    }
    DWORD code = 0;
    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, static_cast<DWORD>(timeoutS) * 1000);
        if (!GetExitCodeProcess(sei.hProcess, &code))
            code = static_cast<DWORD>(-1);
        CloseHandle(sei.hProcess);
    }
    return static_cast<int>(code);
}

QString deviceRegText(HDEVINFO set, SP_DEVINFO_DATA& info, const wchar_t* value)
{
    HKEY key = SetupDiOpenDevRegKey(set, &info, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
    if (key == INVALID_HANDLE_VALUE)
        return {};
    wchar_t buf[128] = {};
    DWORD size = sizeof(buf) - sizeof(wchar_t);
    DWORD type = 0;
    const LONG rc = RegQueryValueExW(key, value, nullptr, &type, reinterpret_cast<LPBYTE>(buf), &size);
    RegCloseKey(key);
    if (rc != ERROR_SUCCESS || type != REG_SZ)
        return {};
    return QString::fromWCharArray(buf).trimmed();
}
#endif

} // namespace

QString setupcPath()
{
    const QString forced = qEnvironmentVariable("DECOPORT_SETUPC");
    if (!forced.isEmpty())
        return QFileInfo::exists(forced) ? forced : QString();
#if defined(Q_OS_WIN)
    // L'installatore scrive dove si e' messo: HKLM\Software\com0com\Install_Dir.
    // E' un installatore a 32 bit, quindi la chiave sta di solito nella vista
    // WOW6432Node.
    for (const char* view : {"HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\com0com",
                             "HKEY_LOCAL_MACHINE\\SOFTWARE\\com0com"}) {
        const QSettings reg(QString::fromLatin1(view), QSettings::NativeFormat);
        const QString dir = reg.value(QStringLiteral("Install_Dir")).toString();
        if (!dir.isEmpty()) {
            const QString exe = QDir(dir).filePath(QStringLiteral("setupc.exe"));
            if (QFileInfo::exists(exe))
                return QDir::toNativeSeparators(exe);
        }
    }
    const QStringList roots = {qEnvironmentVariable("ProgramFiles(x86)"),
                               qEnvironmentVariable("ProgramFiles"),
                               qEnvironmentVariable("ProgramW6432")};
    for (const QString& root : roots) {
        if (root.isEmpty())
            continue;
        const QString exe = QDir(root).filePath(QStringLiteral("com0com/setupc.exe"));
        if (QFileInfo::exists(exe))
            return QDir::toNativeSeparators(exe);
    }
#endif
    return {};
}

bool available()
{
    return !setupcPath().isEmpty();
}

QString bundledInstaller(QString* why)
{
    const QString forced = qEnvironmentVariable("DECOPORT_COM0COM_SETUP");
    const QString arch = QSysInfo::currentCpuArchitecture();
    for (const Installer& i : kInstallers) {
        if (arch != QLatin1String(i.arch))
            continue;
        const QString path = !forced.isEmpty()
            ? forced
            : QDir(QCoreApplication::applicationDirPath())
                  .filePath(QStringLiteral("com0com/") + QLatin1String(i.file));
        if (!QFileInfo::exists(path)) {
            if (why)
                *why = QStringLiteral("l'installatore di com0com non e' accanto al programma (%1)")
                           .arg(QDir::toNativeSeparators(path));
            return {};
        }
        if (sha256Of(path) != QLatin1String(i.sha256)) {
            if (why)
                *why = QStringLiteral("%1 non e' l'installatore di com0com 3.0.0.0 firmato: "
                                      "l'impronta non corrisponde, non lo eseguo")
                           .arg(QDir::toNativeSeparators(path));
            return {};
        }
        return path;
    }
    if (why)
        *why = QStringLiteral("com0com non esiste per questa architettura (%1)").arg(arch);
    return {};
}

bool install(QString* error)
{
    if (available())
        return true;
    QString why;
    const QString setup = bundledInstaller(&why);
    if (setup.isEmpty()) {
        if (error)
            *error = why;
        return false;
    }
#if defined(Q_OS_WIN)
    bool cancelled = false;
    // /S: l'installazione silenziosa di com0com non crea coppie da sola.
    const int rc = runElevated(setup, QStringLiteral("/S"), QFileInfo(setup).absolutePath(),
                               kInstallTimeoutS, &cancelled);
    if (rc == -1 && cancelled) {
        if (error)
            *error = QStringLiteral("installazione di com0com annullata");
        return false;
    }
    // Il registro e i file arrivano un attimo dopo la fine dell'installatore.
    QElapsedTimer t;
    t.start();
    while (!available() && t.elapsed() < 10000)
        QThread::msleep(250);
    if (!available()) {
        if (error)
            *error = QStringLiteral("l'installatore di com0com e' finito (codice %1) ma setupc.exe "
                                    "non c'e'").arg(rc);
        return false;
    }
    return true;
#else
    if (error)
        *error = QStringLiteral("com0com esiste solo per Windows");
    return false;
#endif
}

QVector<Pair> list(QString* error)
{
    QVector<Pair> out;
#if defined(Q_OS_WIN)
    // Su Windows non c'e' errore da riportare: nessun dispositivo con
    // quell'enumeratore vuol dire nessuna coppia.
    Q_UNUSED(error);
    // Le porte di com0com sono figlie del suo enumeratore: COM0COM\PORT\CNCA0.
    HDEVINFO set = SetupDiGetClassDevsW(nullptr, L"COM0COM", nullptr,
                                        DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (set == INVALID_HANDLE_VALUE) {
        // Nessun dispositivo con quell'enumeratore: com0com non ha coppie.
        return out;
    }
    QHash<int, Pair> pairs;
    SP_DEVINFO_DATA info;
    info.cbSize = sizeof(info);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &info); ++i) {
        wchar_t instance[256] = {};
        if (!SetupDiGetDeviceInstanceIdW(set, &info, instance, 255, nullptr))
            continue;
        const QString id = QString::fromWCharArray(instance);
        const QString ident = id.section(QLatin1Char('\\'), 2, 2).toUpper();   // CNCA0
        if (!id.startsWith(QLatin1String("COM0COM\\PORT\\"), Qt::CaseInsensitive)
            || ident.size() < 5 || !ident.startsWith(QLatin1String("CNC")))
            continue;
        bool ok = false;
        const int index = ident.mid(4).toInt(&ok);
        if (!ok)
            continue;
        QString name = deviceRegText(set, info, L"PortName");
        if (name.isEmpty() || name == QLatin1String("-") || name == QLatin1String("COM#"))
            name = ident;

        ULONG status = 0;
        ULONG problem = 0;
        if (CM_Get_DevNode_Status(&status, &problem, info.DevInst, 0) != CR_SUCCESS
            || !(status & DN_HAS_PROBLEM))
            problem = 0;

        Pair& p = pairs[index];
        p.index = index;
        if (ident.at(3) == QLatin1Char('A'))
            p.visible = name;
        else
            p.hidden = name;
        p.problem = std::max(p.problem, static_cast<int>(problem));
    }
    SetupDiDestroyDeviceInfoList(set);
    QList<int> keys = pairs.keys();
    std::sort(keys.begin(), keys.end());
    for (int k : keys)
        if (!pairs.value(k).visible.isEmpty() && !pairs.value(k).hidden.isEmpty())
            out.append(pairs.value(k));
#else
    if (error)
        *error = QStringLiteral("le porte virtuali con com0com esistono solo su Windows");
#endif
    return out;
}

Pair create(QString* error)
{
    const QString exe = setupcPath();
    if (exe.isEmpty()) {
        if (error)
            *error = QStringLiteral("com0com non e' installato");
        return {};
    }
    QSet<int> before;
    for (const Pair& p : list())
        before.insert(p.index);

#if defined(Q_OS_WIN)
    // setupc cerca com0com.inf nella cartella corrente: va lanciato dalla sua.
    // La parte A prende il primo numero COM libero; sulla B, che resta CNCB<n>,
    // il DCD segue l'apertura della A: e' cosi' che il multiplexer sa che un
    // programma ha chiuso la porta anche se non usa DTR.
    bool cancelled = false;
    const int rc = runElevated(exe, QStringLiteral("--silent --wait %1 install PortName=COM# dcd=ropen")
                                        .arg(kCreateTimeoutS),
                               QFileInfo(exe).absolutePath(), kCreateTimeoutS + 10, &cancelled);
    if (rc == -1) {
        if (error)
            *error = cancelled ? QStringLiteral("creazione della porta virtuale annullata")
                               : QStringLiteral("setupc non parte");
        return {};
    }
#endif

    // La coppia compare quando Windows ha finito di installarla.
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < 15000) {
        for (const Pair& p : list())
            if (!before.contains(p.index))
                return p;
        QThread::msleep(500);
    }
    if (error)
        *error = QStringLiteral("la coppia nuova non e' comparsa");
    return {};
}

QString problemText(int problem)
{
    switch (problem) {
    case 0:
        return {};
    case 52:
        return QStringLiteral("Windows ha rifiutato il driver di com0com per la firma (codice 52): "
                              "succede con Secure Boot attivo, vedi third_party/com0com/NOTICE.md");
    case 22:
        return QStringLiteral("la porta virtuale e' disattivata in Gestione dispositivi (codice 22)");
    case 28:
        return QStringLiteral("il driver di com0com non e' installato (codice 28)");
    default:
        return QStringLiteral("la porta virtuale non funziona (codice %1 in Gestione dispositivi)")
            .arg(problem);
    }
}

} // namespace DecoPortVirtualPorts
