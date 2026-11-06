// SPDX-License-Identifier: GPL-3.0-or-later
#include "MacKeychainBackend.h"
#if defined(Q_OS_MACOS)
#include <CoreFoundation/CoreFoundation.h>
#include <QByteArray>
#include <algorithm>

namespace secure_settings {
namespace {
template<typename T> class Cf final {
public:
    explicit Cf(T value = nullptr) : value_(value) {}
    ~Cf() { if (value_) CFRelease(value_); }
    Cf(const Cf&) = delete;
    Cf& operator=(const Cf&) = delete;
    Cf(Cf&& other) noexcept : value_(other.value_) { other.value_ = nullptr; }
    T get() const { return value_; }
private: T value_;
};
Cf<CFStringRef> stringRef(const QString& text) {
    const QByteArray utf8 = text.toUtf8();
    return Cf<CFStringRef>(CFStringCreateWithBytes(nullptr,
        reinterpret_cast<const UInt8*>(utf8.constData()), utf8.size(), kCFStringEncodingUTF8, false));
}
QString statusError(OSStatus status) { return QStringLiteral("Keychain operation failed (%1)").arg(status); }
bool validInput(const QString& service, const QString& account) {
    return !service.isEmpty() && !account.isEmpty() && service.size() <= 256 && account.size() <= 256
        && !service.contains(QChar('\0')) && !account.contains(QChar('\0'));
}
Cf<CFMutableDictionaryRef> query(const QString& service, const QString& account, SecKeychainRef keychain, bool search) {
    Cf<CFMutableDictionaryRef> dict(CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    auto serviceRef = stringRef(service); auto accountRef = stringRef(account);
    if (!dict.get() || !serviceRef.get() || !accountRef.get() || !keychain) return Cf<CFMutableDictionaryRef>();
    CFDictionarySetValue(dict.get(), kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(dict.get(), kSecAttrService, serviceRef.get());
    CFDictionarySetValue(dict.get(), kSecAttrAccount, accountRef.get());
    if (search) {
        const void* items[] = {keychain};
        Cf<CFArrayRef> list(CFArrayCreate(nullptr, items, 1, &kCFTypeArrayCallBacks));
        if (!list.get()) return Cf<CFMutableDictionaryRef>();
        CFDictionarySetValue(dict.get(), kSecMatchSearchList, list.get());
    } else CFDictionarySetValue(dict.get(), kSecUseKeychain, keychain);
    return dict;
}
}
MacKeychainBackend::MacKeychainBackend(const QString& path) {
    // Explicit file-keychain selection keeps reads and atomic adds in the same
    // store. Apple deprecates these file-keychain selectors; SecItem remains
    // the API used for all actual secret operations.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    if (path.isEmpty()) SecKeychainCopyDefault(&keychain_);
    else { const QByteArray p = path.toUtf8(); SecKeychainOpen(p.constData(), &keychain_); }
#pragma clang diagnostic pop
}
MacKeychainBackend::~MacKeychainBackend() { if (keychain_) CFRelease(keychain_); }
bool MacKeychainBackend::available() const { return keychain_ != nullptr; }
bool MacKeychainBackend::supports_private_key_storage() const { return available(); }
LookupResult MacKeychainBackend::lookup(const QString& service, const QString& account) const {
    LookupResult result; result.backend_available = available();
    if (!available() || !validInput(service, account)) { result.error = QStringLiteral("Invalid or unavailable keychain"); return result; }
    auto attrs = query(service, account, keychain_, true);
    if (!attrs.get()) { result.error = QStringLiteral("Keychain query allocation failed"); return result; }
    CFDictionarySetValue(attrs.get(), kSecReturnData, kCFBooleanTrue);
    CFTypeRef raw = nullptr;
    const OSStatus status = SecItemCopyMatching(attrs.get(), &raw);
    Cf<CFTypeRef> data(raw);
    if (status == errSecItemNotFound) return result;
    if (status != errSecSuccess || !data.get() || CFGetTypeID(data.get()) != CFDataGetTypeID()) {
        result.error = statusError(status); return result;
    }
    const auto bytes = static_cast<CFDataRef>(data.get());
    const auto length = CFDataGetLength(bytes);
    if (length < 0 || length > 4096) { result.error = QStringLiteral("Invalid keychain item length"); return result; }
    result.value = QString::fromUtf8(reinterpret_cast<const char*>(CFDataGetBytePtr(bytes)), int(length));
    result.found = true;
    return result;
}
InsertResult MacKeychainBackend::store_if_absent(const QString& service, const QString& account, const QString& value) const {
    if (!available() || !validInput(service, account) || value.isEmpty() || value.size() > 4096)
        return InsertResult::Error;
    auto attrs = query(service, account, keychain_, false);
    if (!attrs.get()) return InsertResult::Error;
    QByteArray utf8 = value.toUtf8();
    Cf<CFDataRef> data(CFDataCreate(nullptr, reinterpret_cast<const UInt8*>(utf8.constData()), utf8.size()));
    std::fill(utf8.begin(), utf8.end(), '\0');
    if (!data.get()) return InsertResult::Error;
    CFDictionarySetValue(attrs.get(), kSecValueData, data.get());
    const OSStatus status = SecItemAdd(attrs.get(), nullptr);
    return status == errSecSuccess ? InsertResult::Created
         : status == errSecDuplicateItem ? InsertResult::AlreadyExists : InsertResult::Error;
}
bool MacKeychainBackend::store(const QString& service, const QString& account, const QString& value, QString* error) const {
    const auto result = store_if_absent(service, account, value);
    if (result == InsertResult::Created) return true;
    if (error) *error = QStringLiteral("Keychain item was not created");
    return false;
}
bool MacKeychainBackend::remove(const QString& service, const QString& account, QString* error) const {
    if (!available() || !validInput(service, account)) return false;
    auto attrs = query(service, account, keychain_, true);
    if (!attrs.get()) return false;
    const OSStatus status = SecItemDelete(attrs.get());
    if (status == errSecSuccess || status == errSecItemNotFound) return true;
    if (error) *error = statusError(status);
    return false;
}
} // namespace secure_settings
#endif
