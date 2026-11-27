#include <QCoreApplication>
#include <QString>
#include <QTextStream>
#ifdef SIMULATE_MAC
#define Q_OS_MAC 1
#else
#undef Q_OS_MAC
#endif
#define DECODIUM_HAS_SSTV 1
static bool isFt2LinkApplicationMode(QString m){return m.trimmed().toUpper()=="FT2-LINK";}
static bool isStreamingKeyboardMode(const QString&m){return QStringList{"RTTY","JTTY","SSB","CW"}.contains(m.trimmed().toUpper());}
static bool bridgeOwnsFst4Audio(const QString&m){return m.trimmed().toUpper().startsWith("FST4");}
static bool bridgeOwnsLegacyJtRxWhenCatSuppressed(const QString&m){return m.trimmed().toUpper()=="JT65";}
namespace decodium {namespace audio {bool useLegacyRxLevels(bool a,bool b){return a&&!b;}}}
class Legacy {public:bool value=true;bool available(){return value;}};
class DecodiumBridge {public:QString m_mode="FT8";bool rtl=false,requested=true,m_decoPortUseRemote=false,m_directVisualAudioCaptureUnsafe=false,m_tciAudioCaptureActive=false,m_sstvWorkspaceActive=false;Legacy*m_legacyBackend=nullptr;bool rtlSdrEnabled()const{return rtl;}bool legacyTxBackendRequested()const{return requested;}bool nativeSstvRxForcesDedicatedAudioCapture()const{return false;}bool usingLegacyBackendForTx()const;bool usingLegacyBackendForRx()const;bool legacyBackendRequestedForRx()const;bool useModernSpectrumFeedWithLegacy()const;bool useDedicatedModernAudioCaptureWithLegacy()const;bool usesLegacyRxLevels()const;bool legacyBackendAvailable()const;};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTextStream o(stdout);qunsetenv("DECODIUM_DUAL_AUDIO_PANADAPTER");qunsetenv("DECODIUM_FT2LINK_DEDICATED_MODERN_RX");Legacy legacy;DecodiumBridge b;b.m_legacyBackend=&legacy;auto p=[&](const char*n){o<<n<<'|'<<b.usingLegacyBackendForTx()<<'|'<<b.usingLegacyBackendForRx()<<'|'<<b.legacyBackendRequestedForRx()<<'|'<<b.useModernSpectrumFeedWithLegacy()<<'|'<<b.useDedicatedModernAudioCaptureWithLegacy()<<'|'<<b.usesLegacyRxLevels()<<'\n';};p("ft8");b.m_mode="Q65-60A";p("q65");b.m_mode="RTTY";p("rtty");b.m_mode="WSPR";p("wspr");b.m_mode="JT65";p("jt65");b.m_mode="FT2-LINK";p("ft2link");b.m_mode="FT8";b.m_decoPortUseRemote=true;p("remote");b.m_decoPortUseRemote=false;b.rtl=true;p("rtl");b.rtl=false;legacy.value=false;p("unavailable");legacy.value=true;b.m_directVisualAudioCaptureUnsafe=true;qputenv("DECODIUM_DUAL_AUDIO_PANADAPTER","1");p("dual");b.m_sstvWorkspaceActive=true;p("sstv");}
