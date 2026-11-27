#if DECODIUM_HAS_SSTV
// The relay is deliberately not a QObject.  Direct producer callbacks only
// enter this small gate and the runtime's bounded ingress; no callback needs
// the bridge object or its affinity thread.  disableAndDrain() closes the gate
// under the same mutex used to count entrants, so a callback cannot slip
// between the final in-flight check and SstvRxRuntime::shutdown().
class DecodiumSstvAudioRelay final
{
public:
#if DECODIUM_HAS_HAMDRM
    void enable(
        decodium::sstv::SstvRxRuntime* runtime,
        std::shared_ptr<decodium::sstv::hamdrm::waveform::HamDrmNativeRxBackend>
            hamDrm)
#else
    void enable(decodium::sstv::SstvRxRuntime* runtime)
#endif
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_runtime = runtime;
        m_sourceClock.reset();
        m_reportedSourceClock = false;
#if DECODIUM_HAS_HAMDRM
        m_hamDrm = std::move(hamDrm);
        m_enabled = runtime != nullptr || m_hamDrm != nullptr;
#else
        m_enabled = runtime != nullptr;
#endif
    }

    void disableAndDrain()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_enabled = false;
        m_drained.wait(lock, [this]() { return m_inFlight == 0U; });
        m_runtime = nullptr;
#if DECODIUM_HAS_HAMDRM
        m_hamDrm.reset();
#endif
    }

    bool submit(QVector<short> samples,
                int sampleRate,
                decodium::sstv::SstvRxRouteToken token,
                qint64 sourceTimeNs = -1)
    {
        decodium::sstv::SstvRxRuntime* runtime = nullptr;
        std::optional<qint64> mappedTime;
        bool invalidSourceClock = false;
#if DECODIUM_HAS_HAMDRM
        std::shared_ptr<
            decodium::sstv::hamdrm::waveform::HamDrmNativeRxBackend> hamDrm;
#endif
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_enabled || samples.isEmpty()) {
                return false;
            }
            runtime = m_runtime;
            if (runtime && sourceTimeNs >= 0) {
                mappedTime = m_sourceClock.map(sourceTimeNs,
                    decodium::sstv::SstvRxRuntime::localMonotonicNowNs());
                if (mappedTime && !m_reportedSourceClock
                    && qEnvironmentVariableIsSet("DECODIUM_SSTV_TRACE_TIMELINE")) {
                    qInfo() << "[SSTV][TIMELINE] capture sample clock active";
                    m_reportedSourceClock = true;
                }
                if (!mappedTime) {
                    invalidSourceClock = true;
                    m_sourceClock.reset();
                }
            } else if (sourceTimeNs < 0) {
                m_sourceClock.reset();
            }
#if DECODIUM_HAS_HAMDRM
            hamDrm = m_hamDrm;
            if (!runtime && !hamDrm) {
                return false;
            }
#else
            if (!runtime) {
                return false;
            }
#endif
            ++m_inFlight;
        }

        bool accepted = false;
#if DECODIUM_HAS_HAMDRM
        static_assert(sizeof(short) == sizeof(std::int16_t),
                      "Decodium PCM16 bridge requires 16-bit short");
        if (hamDrm) {
            accepted = hamDrm->submitPcm16(
                reinterpret_cast<const std::int16_t*>(samples.constData()),
                static_cast<std::size_t>(samples.size()),
                static_cast<std::uint32_t>(sampleRate));
        }
#endif
        if (runtime && token.valid()) {
            if (invalidSourceClock)
                qWarning() << "[SSTV] capture sample clock regressed; restarting the RX signal path";
            accepted = (mappedTime
                ? runtime->enqueuePcm16At(std::move(samples), sampleRate, token, *mappedTime, true)
                : runtime->enqueuePcm16At(std::move(samples), sampleRate, token,
                    decodium::sstv::SstvRxRuntime::localMonotonicNowNs(), false,
                    invalidSourceClock)) || accepted;
        }

        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            --m_inFlight;
            if (m_inFlight == 0U) {
                m_drained.notify_all();
            }
        }
        return accepted;
    }

private:
    std::mutex m_mutex;
    decodium::sstv::SstvSourceClockMapper m_sourceClock;
    bool m_reportedSourceClock {false};
    std::condition_variable m_drained;
    decodium::sstv::SstvRxRuntime* m_runtime {nullptr};
#if DECODIUM_HAS_HAMDRM
    std::shared_ptr<
        decodium::sstv::hamdrm::waveform::HamDrmNativeRxBackend> m_hamDrm;
#endif
    std::size_t m_inFlight {0U};
    bool m_enabled {false};
};
#endif

