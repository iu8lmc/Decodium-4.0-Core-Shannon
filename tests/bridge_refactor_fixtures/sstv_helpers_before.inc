namespace
{

#if DECODIUM_HAS_SSTV
using decodium::sstv::SstvAudioSourceKind;
using decodium::sstv::SstvRxAfcMode;
using decodium::sstv::SstvRxControlSettings;
using decodium::sstv::SstvRxModeControl;
using decodium::sstv::SstvRxRedecodeParameters;
using decodium::sstv::SstvRxRuntime;
using decodium::sstv::SstvRxSlantMode;

QString sstvRxModeControlName(SstvRxModeControl mode)
{
    return mode == SstvRxModeControl::Manual
        ? QStringLiteral("manual") : QStringLiteral("auto");
}

QString sstvRxAfcModeName(SstvRxAfcMode mode)
{
    switch (mode) {
    case SstvRxAfcMode::Off: return QStringLiteral("off");
    case SstvRxAfcMode::Automatic: return QStringLiteral("auto");
    case SstvRxAfcMode::Manual: return QStringLiteral("manual");
    }
    return QStringLiteral("auto");
}

QString sstvRxSlantModeName(SstvRxSlantMode mode)
{
    switch (mode) {
    case SstvRxSlantMode::Off: return QStringLiteral("off");
    case SstvRxSlantMode::Automatic: return QStringLiteral("auto");
    case SstvRxSlantMode::Manual: return QStringLiteral("manual");
    }
    return QStringLiteral("auto");
}

bool parseSstvRxModeControl(const QVariant& value,
                            SstvRxModeControl* parsed)
{
    const QString text = value.toString().trimmed().toLower();
    if (text == QLatin1String("auto")
        || text == QLatin1String("automatic")) {
        *parsed = SstvRxModeControl::Automatic;
        return true;
    }
    if (text == QLatin1String("manual")) {
        *parsed = SstvRxModeControl::Manual;
        return true;
    }
    return false;
}

bool parseSstvRxAfcMode(const QVariant& value, SstvRxAfcMode* parsed)
{
    const QString text = value.toString().trimmed().toLower();
    if (text == QLatin1String("off")) {
        *parsed = SstvRxAfcMode::Off;
        return true;
    }
    if (text == QLatin1String("auto")
        || text == QLatin1String("automatic")) {
        *parsed = SstvRxAfcMode::Automatic;
        return true;
    }
    if (text == QLatin1String("manual")) {
        *parsed = SstvRxAfcMode::Manual;
        return true;
    }
    return false;
}

bool parseSstvRxSlantMode(const QVariant& value,
                          SstvRxSlantMode* parsed)
{
    const QString text = value.toString().trimmed().toLower();
    if (text == QLatin1String("off")) {
        *parsed = SstvRxSlantMode::Off;
        return true;
    }
    if (text == QLatin1String("auto")
        || text == QLatin1String("automatic")) {
        *parsed = SstvRxSlantMode::Automatic;
        return true;
    }
    if (text == QLatin1String("manual")) {
        *parsed = SstvRxSlantMode::Manual;
        return true;
    }
    return false;
}

bool selectableSstvRxMode(const std::string& id)
{
    if (id.empty()) {
        return false;
    }
    const decodium::sstv::SstvModeRegistry registry =
        decodium::sstv::SstvModeRegistry::canonical();
    const decodium::sstv::SstvModeSpec* mode = registry.findById(id);
    return mode && mode->classification
            == decodium::sstv::ModeClassification::AnalogSstv
        && mode->claimsRxSupport();
}

bool sstvRxSettingsFromMap(const QVariantMap& values,
                           SstvRxControlSettings base,
                           SstvRxControlSettings* parsed,
                           QString* error)
{
    auto fail = [error](const QString& detail) {
        if (error) {
            *error = detail;
        }
        return false;
    };
    if (values.contains(QStringLiteral("modeControl"))
        && !parseSstvRxModeControl(
            values.value(QStringLiteral("modeControl")),
            &base.modeControl)) {
        return fail(QObject::tr("Invalid SSTV RX mode control"));
    }
    if (values.contains(QStringLiteral("manualMode"))) {
        base.manualMode = values.value(QStringLiteral("manualMode"))
                              .toString().trimmed().toStdString();
    }
    if (values.contains(QStringLiteral("modeLockEnabled"))) {
        base.modeLockEnabled = values.value(
            QStringLiteral("modeLockEnabled")).toBool();
    }
    if (values.contains(QStringLiteral("lockedMode"))) {
        base.lockedMode = values.value(QStringLiteral("lockedMode"))
                              .toString().trimmed().toStdString();
    }
    if (values.contains(QStringLiteral("receiveWithoutVis"))) {
        base.receiveWithoutVis = values.value(
            QStringLiteral("receiveWithoutVis")).toBool();
    }
    if (values.contains(QStringLiteral("timingFallbackEnabled"))) {
        base.timingFallbackEnabled = values.value(
            QStringLiteral("timingFallbackEnabled")).toBool();
    }
    if (values.contains(QStringLiteral("afcMode"))
        && !parseSstvRxAfcMode(values.value(QStringLiteral("afcMode")),
                               &base.afcMode)) {
        return fail(QObject::tr("Invalid SSTV AFC mode"));
    }
    if (values.contains(QStringLiteral("manualFrequencyCorrectionHz"))) {
        bool ok = false;
        const double value = values.value(
            QStringLiteral("manualFrequencyCorrectionHz")).toDouble(&ok);
        if (!ok || !std::isfinite(value)) {
            return fail(QObject::tr("Invalid manual SSTV AFC correction"));
        }
        base.manualFrequencyCorrectionHz = value;
    }
    if (values.contains(QStringLiteral("slantMode"))
        && !parseSstvRxSlantMode(
            values.value(QStringLiteral("slantMode")), &base.slantMode)) {
        return fail(QObject::tr("Invalid SSTV slant mode"));
    }
    if (values.contains(QStringLiteral("manualClockErrorPpm"))) {
        bool ok = false;
        const double value = values.value(
            QStringLiteral("manualClockErrorPpm")).toDouble(&ok);
        if (!ok || !std::isfinite(value)) {
            return fail(QObject::tr("Invalid manual SSTV slant correction"));
        }
        base.manualClockErrorPpm = value;
    }
    if (values.contains(QStringLiteral("replayRetentionSeconds"))) {
        bool ok = false;
        const uint value = values.value(
            QStringLiteral("replayRetentionSeconds")).toUInt(&ok);
        if (!ok) {
            return fail(QObject::tr("Invalid SSTV replay retention"));
        }
        base.replayRetentionSeconds = value;
    }
    if (values.contains(QStringLiteral("retainRawAudio"))) {
        base.retainRawAudio = values.value(
            QStringLiteral("retainRawAudio")).toBool();
    }
    if (values.contains(QStringLiteral("diagnosticScopeEnabled"))) {
        base.diagnosticScopeEnabled = values.value(
            QStringLiteral("diagnosticScopeEnabled")).toBool();
    }
    if ((base.modeControl == SstvRxModeControl::Manual
         && !selectableSstvRxMode(base.manualMode))
        || (base.modeLockEnabled && !selectableSstvRxMode(base.lockedMode))
        || !decodium::sstv::SstvRxControlPolicy::settingsAreValid(base)) {
        return fail(QObject::tr("SSTV RX controls are outside their safe bounds"));
    }
    *parsed = std::move(base);
    return true;
}

void persistSstvRxControls(const SstvRxControlSettings& settings)
{
    // Keep the whole SSTV RX control snapshot in one QSettings transaction.
    // Calling DecodiumBridge::setSetting() once per field forces a synchronous
    // sync() for every field, which can pause the GUI while FT8/panadapter are
    // active (Lock used to trigger that path twice in succession).
    QSettings store(QSettings::IniFormat,
                    QSettings::UserScope,
                    QStringLiteral("Decodium"),
                    QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(store);
    store.setValue(QStringLiteral("SSTV/RxModeControl"),
                   sstvRxModeControlName(settings.modeControl));
    store.setValue(QStringLiteral("SSTV/RxManualMode"),
                   QString::fromStdString(settings.manualMode));
    store.setValue(QStringLiteral("SSTV/RxModeLockEnabled"),
                   settings.modeLockEnabled);
    store.setValue(QStringLiteral("SSTV/RxLockedMode"),
                   QString::fromStdString(settings.lockedMode));
    store.setValue(QStringLiteral("SSTV/RxWithoutVis"),
                   settings.receiveWithoutVis);
    store.setValue(QStringLiteral("SSTV/RxTimingFallbackEnabled"),
                   settings.timingFallbackEnabled);
    store.setValue(QStringLiteral("SSTV/RxAfcMode"),
                   sstvRxAfcModeName(settings.afcMode));
    store.setValue(QStringLiteral("SSTV/RxManualFrequencyCorrectionHz"),
                   settings.manualFrequencyCorrectionHz);
    store.setValue(QStringLiteral("SSTV/RxSlantMode"),
                   sstvRxSlantModeName(settings.slantMode));
    store.setValue(QStringLiteral("SSTV/RxManualClockErrorPpm"),
                   settings.manualClockErrorPpm);
    store.setValue(QStringLiteral("SSTV/RxReplayRetentionSeconds"),
                   settings.replayRetentionSeconds);
    store.setValue(QStringLiteral("SSTV/RxRetainRawAudio"),
                   settings.retainRawAudio);
    store.setValue(QStringLiteral("SSTV/RxDiagnosticScopeEnabled"),
                   settings.diagnosticScopeEnabled);
    store.sync();
}

QString sstvSourceName(SstvAudioSourceKind kind)
{
    switch (kind) {
    case SstvAudioSourceKind::LocalSoundCard:
        return QObject::tr("Local sound card");
    case SstvAudioSourceKind::DecoPort:
        return QStringLiteral("DecoPort");
    case SstvAudioSourceKind::Tci:
        return QStringLiteral("TCI");
    case SstvAudioSourceKind::WebSdr:
        return QObject::tr("WebSDR/KiwiSDR monitor");
    case SstvAudioSourceKind::RtlSdr:
        return QStringLiteral("RTL-SDR");
    case SstvAudioSourceKind::LegacyBackend:
        return QObject::tr("Decodium legacy backend");
    case SstvAudioSourceKind::Replay:
        return QObject::tr("Replay/WAV");
    case SstvAudioSourceKind::Unknown:
        break;
    }
    return QObject::tr("None");
}

QString sstvDiagnosticSourceToken(SstvAudioSourceKind kind)
{
    // Diagnostics must remain locale-independent and pass the central scalar
    // allowlist.  Do not reuse the translated UI labels (some contain '/').
    switch (kind) {
    case SstvAudioSourceKind::LocalSoundCard:
        return QStringLiteral("local-sound-card");
    case SstvAudioSourceKind::DecoPort:
        return QStringLiteral("decoport");
    case SstvAudioSourceKind::Tci:
        return QStringLiteral("tci");
    case SstvAudioSourceKind::WebSdr:
        return QStringLiteral("websdr-kiwisdr");
    case SstvAudioSourceKind::RtlSdr:
        return QStringLiteral("rtl-sdr");
    case SstvAudioSourceKind::LegacyBackend:
        return QStringLiteral("legacy-backend");
    case SstvAudioSourceKind::Replay:
        return QStringLiteral("replay-wav");
    case SstvAudioSourceKind::Unknown:
        break;
    }
    return QStringLiteral("unknown");
}

QString sstvRuntimeStateName(SstvRxRuntime::State state)
{
    switch (state) {
    case SstvRxRuntime::State::Inactive:
        return QStringLiteral("Inactive");
    case SstvRxRuntime::State::Running:
        return QStringLiteral("Running");
    case SstvRxRuntime::State::Cancelled:
        return QStringLiteral("Cancelled");
    case SstvRxRuntime::State::Stopping:
        return QStringLiteral("Stopping");
    case SstvRxRuntime::State::Error:
        return QStringLiteral("Error");
    case SstvRxRuntime::State::Shutdown:
        return QStringLiteral("Shutdown");
    }
    return QStringLiteral("Unknown");
}

qulonglong boundedUnsigned(std::uint64_t value)
{
    return static_cast<qulonglong>(value);
}

bool terminalSstvTxState(decodium::sstv::SstvTxState state) noexcept
{
    return decodium::sstv::SstvTxStateMachine::isTerminal(state)
        || state == decodium::sstv::SstvTxState::Disabled;
}

#if defined(Q_OS_MAC)
[[maybe_unused]] constexpr int kSstvOutputBufferFrames = 16'384;
#else
[[maybe_unused]] constexpr int kSstvOutputBufferFrames = 49'152;
#endif

bool sstvOutputMatchesConfiguredSelection(
    const QString& configuredName,
    const QString& configuredId,
    bool requestedDeviceFound,
    const QAudioDevice& output)
{
    if (output.isNull()) {
        return false;
    }
    const QString name = configuredName.trimmed();
    const QString id = configuredId.trimmed();
    if (name.isEmpty() && id.isEmpty()) {
        return true;
    }
    if (!requestedDeviceFound) {
        return false;
    }

    // findOutputDevice() historically reports "found" for an ID-only
    // selection when its name is empty, even if it returned the default
    // fallback. Close that hole locally without changing legacy routing.
    if (name.isEmpty()) {
        const QString selectedId = QString::fromLatin1(output.id().toHex());
        return !selectedId.isEmpty()
            && selectedId.compare(id, Qt::CaseInsensitive) == 0;
    }
    return true;
}

QImage sstvSnapshotToImage(
    const decodium::sstv::SstvImageSnapshot& snapshot)
{
    const std::size_t width = snapshot.width;
    const std::size_t height = snapshot.height;
    if (width == 0U || height == 0U
        || width > decodium::sstv::SstvImageFrame::kMaximumDimension
        || height > decodium::sstv::SstvImageFrame::kMaximumDimension
        || width > decodium::sstv::SstvImageFrame::kMaximumPixels / height
        || snapshot.pixels.size() != width * height
        || width > static_cast<std::size_t>(
               std::numeric_limits<int>::max())
        || height > static_cast<std::size_t>(
               std::numeric_limits<int>::max())) {
        return {};
    }

    QImage image(static_cast<int>(width), static_cast<int>(height),
                 QImage::Format_RGB888);
    if (image.isNull()) {
        return {};
    }
    image.fill(Qt::black);
    for (std::size_t y = 0U; y < height; ++y) {
        uchar* const destination = image.scanLine(static_cast<int>(y));
        if (!destination) {
            return {};
        }
        const decodium::sstv::SstvRgbPixel* const source =
            snapshot.pixels.data() + y * width;
        for (std::size_t x = 0U; x < width; ++x) {
            destination[x * 3U] = source[x].red;
            destination[x * 3U + 1U] = source[x].green;
            destination[x * 3U + 2U] = source[x].blue;
        }
    }
    return image;
}

bool sstvQsoRequestFromMap(
    const QVariantMap& values,
    decodium::sstv::SstvQsoLogRequest* request,
    QString* error)
{
    return decodium::sstv::SstvQsoLog::requestFromVariantMap(
        values, request, error);
}
#endif

} // namespace

#if DECODIUM_HAS_SSTV
bool decodiumSstvQsoRequestFromMap(const QVariantMap& values, decodium::sstv::SstvQsoLogRequest* request, QString* error) { return sstvQsoRequestFromMap(values, request, error); }
bool decodiumSstvTerminalTxState(decodium::sstv::SstvTxState state) noexcept { return terminalSstvTxState(state); }
bool decodiumSstvOutputMatchesConfiguredSelection(const QString& name, const QString& id, bool found, const QAudioDevice& output) { return sstvOutputMatchesConfiguredSelection(name, id, found, output); }
QImage decodiumSstvSnapshotToImage(const decodium::sstv::SstvImageSnapshot& snapshot) { return sstvSnapshotToImage(snapshot); }
QString decodiumSstvSourceName(decodium::sstv::SstvAudioSourceKind kind) { return sstvSourceName(kind); }
QString decodiumSstvRuntimeStateName(decodium::sstv::SstvRxRuntime::State state) { return sstvRuntimeStateName(state); }
QString decodiumSstvDiagnosticSourceToken(decodium::sstv::SstvAudioSourceKind kind) { return sstvDiagnosticSourceToken(kind); }
bool decodiumSstvParseAfc(const QVariant& value, decodium::sstv::SstvRxAfcMode* parsed) { return parseSstvRxAfcMode(value, parsed); }
bool decodiumSstvParseSlant(const QVariant& value, decodium::sstv::SstvRxSlantMode* parsed) { return parseSstvRxSlantMode(value, parsed); }
bool decodiumSstvSelectableMode(const std::string& id) { return selectableSstvRxMode(id); }
bool decodiumSstvRxSettingsFromMap(const QVariantMap& values, decodium::sstv::SstvRxControlSettings base, decodium::sstv::SstvRxControlSettings* parsed, QString* error) { return sstvRxSettingsFromMap(values, std::move(base), parsed, error); }
void decodiumPersistSstvRxControls(const decodium::sstv::SstvRxControlSettings& settings) { persistSstvRxControls(settings); }
QString decodiumSstvRxModeControlName(decodium::sstv::SstvRxModeControl mode) { return sstvRxModeControlName(mode); }
QString decodiumSstvRxAfcModeName(decodium::sstv::SstvRxAfcMode mode) { return sstvRxAfcModeName(mode); }
QString decodiumSstvRxSlantModeName(decodium::sstv::SstvRxSlantMode mode) { return sstvRxSlantModeName(mode); }
qulonglong decodiumSstvBoundedUnsigned(std::uint64_t value) { return boundedUnsigned(value); }
#endif

