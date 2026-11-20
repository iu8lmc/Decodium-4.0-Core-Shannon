// Chi occupa davvero le porte del hub DecoPort, e con quale programma.
//
// Le prese TCP e UDP danno il processo proprietario (IP Helper). Le porte COM
// non hanno un proprietario esposto dal sistema: si cercano tra gli handle dei
// processi dello stesso utente quelli che puntano al dispositivo seriale. Un
// processo elevato, o un handle che non si lascia leggere, resta non visibile e
// viene dichiarato come tale. Nessuna porta seriale viene aperta.
#pragma once

#include <QFileInfo>
#include <QHash>
#include <QString>

#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <tcpmib.h>
#include <udpmib.h>
#include <chrono>
#include <future>
#include <vector>
#endif

namespace decoport_hub {

struct PortUse {
    bool    used {false};
    bool    known {true};   // false: non si e' riusciti a saperlo
    quint32 pid {0};
    QString program;
    QString state;
};

#ifdef Q_OS_WIN

inline QString programOf(DWORD pid)
{
    HANDLE const h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h)
        return QStringLiteral("processo %1 (non accessibile)").arg(pid);
    wchar_t buffer[MAX_PATH * 2];
    DWORD size = MAX_PATH * 2;
    QString name;
    if (QueryFullProcessImageNameW(h, 0, buffer, &size))
        name = QFileInfo(QString::fromWCharArray(buffer, int(size))).fileName();
    CloseHandle(h);
    return name.isEmpty() ? QStringLiteral("processo %1").arg(pid) : name;
}

inline QString tcpStateName(DWORD s)
{
    switch (s) {
    case MIB_TCP_STATE_LISTEN:     return QStringLiteral("LISTEN");
    case MIB_TCP_STATE_ESTAB:      return QStringLiteral("ESTABLISHED");
    case MIB_TCP_STATE_SYN_SENT:   return QStringLiteral("SYN_SENT");
    case MIB_TCP_STATE_TIME_WAIT:  return QStringLiteral("TIME_WAIT");
    case MIB_TCP_STATE_CLOSE_WAIT: return QStringLiteral("CLOSE_WAIT");
    default:                       return QStringLiteral("stato %1").arg(s);
    }
}

// Le tabelle IP Helper si caricano a runtime: nessun link nuovo.
inline PortUse tcpOwner(quint16 port)
{
    PortUse out;
    using GetExTcp = DWORD (WINAPI*)(PVOID, PDWORD, BOOL, ULONG, TCP_TABLE_CLASS, ULONG);
    HMODULE lib = LoadLibraryW(L"iphlpapi.dll");
    auto fn = lib ? reinterpret_cast<GetExTcp>(reinterpret_cast<void*>(GetProcAddress(lib, "GetExtendedTcpTable"))) : nullptr;
    if (!fn) { out.known = false; return out; }
    DWORD size = 0;
    fn(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    std::vector<unsigned char> buf(size);
    if (fn(buf.data(), &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) != NO_ERROR) {
        out.known = false;
        return out;
    }
    auto* table = reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(buf.data());
    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        MIB_TCPROW_OWNER_PID const& row = table->table[i];
        if (ntohs(static_cast<u_short>(row.dwLocalPort)) != port)
            continue;
        DWORD const addr = row.dwLocalAddr;
        if (addr != htonl(INADDR_LOOPBACK) && addr != 0)
            continue;
        if (row.dwState != MIB_TCP_STATE_LISTEN && row.dwState != MIB_TCP_STATE_ESTAB)
            continue;
        out.used = true;
        out.pid = row.dwOwningPid;
        out.program = programOf(row.dwOwningPid);
        out.state = tcpStateName(row.dwState);
        break;
    }
    return out;
}

inline PortUse udpOwner(quint16 port)
{
    PortUse out;
    using GetExUdp = DWORD (WINAPI*)(PVOID, PDWORD, BOOL, ULONG, UDP_TABLE_CLASS, ULONG);
    HMODULE lib = LoadLibraryW(L"iphlpapi.dll");
    auto fn = lib ? reinterpret_cast<GetExUdp>(reinterpret_cast<void*>(GetProcAddress(lib, "GetExtendedUdpTable"))) : nullptr;
    if (!fn) { out.known = false; return out; }
    DWORD size = 0;
    fn(nullptr, &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0);
    std::vector<unsigned char> buf(size);
    if (fn(buf.data(), &size, FALSE, AF_INET, UDP_TABLE_OWNER_PID, 0) != NO_ERROR) {
        out.known = false;
        return out;
    }
    auto* table = reinterpret_cast<MIB_UDPTABLE_OWNER_PID*>(buf.data());
    for (DWORD i = 0; i < table->dwNumEntries; ++i) {
        MIB_UDPROW_OWNER_PID const& row = table->table[i];
        if (ntohs(static_cast<u_short>(row.dwLocalPort)) != port)
            continue;
        out.used = true;
        out.pid = row.dwOwningPid;
        out.program = programOf(row.dwOwningPid);
        out.state = QStringLiteral("aperta");
        break;
    }
    return out;
}

// Proprietari delle porte COM, indicizzati per nome DOS (COM5).
inline QHash<QString, PortUse> serialOwners()
{
    struct Entry { void* Object; ULONG_PTR Pid; ULONG_PTR Handle; ULONG Access;
                   USHORT Creator; USHORT TypeIndex; ULONG Attr; ULONG Reserved; };
    struct InfoEx { ULONG_PTR Count; ULONG_PTR Reserved; Entry Handles[1]; };
    struct UStr { USHORT Length; USHORT Max; wchar_t* Buffer; };
    using NtQsi = LONG (NTAPI*)(ULONG, PVOID, ULONG, PULONG);
    using NtQo  = LONG (NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);

    QHash<QString, PortUse> owners;
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    auto qsi = ntdll ? reinterpret_cast<NtQsi>(reinterpret_cast<void*>(GetProcAddress(ntdll, "NtQuerySystemInformation"))) : nullptr;
    auto qo  = ntdll ? reinterpret_cast<NtQo>(reinterpret_cast<void*>(GetProcAddress(ntdll, "NtQueryObject"))) : nullptr;
    if (!qsi || !qo)
        return owners;

    // Nome NT del dispositivo (\Device\Serial0) -> nome DOS (COM5).
    QHash<QString, QString> devices;
    for (int n = 1; n <= 256; ++n) {
        wchar_t target[1024];
        QString const dos = QStringLiteral("COM%1").arg(n);
        if (QueryDosDeviceW(dos.toStdWString().c_str(), target, 1024))
            devices.insert(QString::fromWCharArray(target).toLower(), dos);
    }

    ULONG size = 1u << 20;
    std::vector<unsigned char> buf;
    LONG status = -1;
    for (int attempt = 0; attempt < 6; ++attempt) {
        buf.assign(size, 0);
        status = qsi(64 /* SystemExtendedHandleInformation */, buf.data(), size, &size);
        if (status >= 0 || status != static_cast<LONG>(0xC0000004))
            break;
    }
    if (status < 0)
        return owners;

    auto* info = reinterpret_cast<InfoEx*>(buf.data());
    DWORD const self = GetCurrentProcessId();
    for (ULONG_PTR i = 0; i < info->Count; ++i) {
        Entry const& e = info->Handles[i];
        DWORD const pid = static_cast<DWORD>(e.Pid);
        if (pid == 0 || pid == 4 || pid == self)
            continue;
        HANDLE proc = OpenProcess(PROCESS_DUP_HANDLE, FALSE, pid);
        if (!proc)
            continue;
        HANDLE dup = nullptr;
        bool const ok = DuplicateHandle(proc, reinterpret_cast<HANDLE>(e.Handle),
                                        GetCurrentProcess(), &dup, 0, FALSE, DUPLICATE_SAME_ACCESS);
        CloseHandle(proc);
        if (!ok || !dup)
            continue;
        // I dispositivi seriali risultano "char": pipe e file non servono.
        if (GetFileType(dup) != FILE_TYPE_CHAR) {
            CloseHandle(dup);
            continue;
        }
        // NtQueryObject puo' bloccarsi su certi handle: si aspetta con un limite.
        auto future = std::async(std::launch::async, [qo, dup]() {
            std::vector<unsigned char> nb(2048);
            ULONG len = 0;
            LONG const rc = qo(dup, 1 /* ObjectNameInformation */, nb.data(), ULONG(nb.size()), &len);
            if (rc < 0)
                return QString();
            auto* us = reinterpret_cast<UStr*>(nb.data());
            return QString::fromWCharArray(us->Buffer, us->Length / 2);
        });
        QString name;
        if (future.wait_for(std::chrono::milliseconds(200)) == std::future_status::ready)
            name = future.get();
        CloseHandle(dup);
        if (name.isEmpty())
            continue;
        auto const dev = devices.constFind(name.toLower());
        if (dev == devices.constEnd())
            continue;
        PortUse& use = owners[dev.value()];
        if (use.used)
            continue;
        use.used = true;
        use.pid = pid;
        use.program = programOf(pid);
        use.state = QStringLiteral("handle aperto");
    }
    return owners;
}

#else

inline PortUse tcpOwner(quint16) { PortUse u; u.known = false; return u; }
inline PortUse udpOwner(quint16) { PortUse u; u.known = false; return u; }
inline QHash<QString, PortUse> serialOwners() { return {}; }

#endif

} // namespace decoport_hub
