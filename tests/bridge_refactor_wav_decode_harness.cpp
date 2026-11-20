#include <QtCore>
#include <QtEndian>
#include <cstdint>
#include <cstring>
#include <cstdio>
class DecodiumBridge:public QObject {public:
 QString m_mode="FT8";QMutex m_audioBufferMutex;QVector<short> m_audioBuffer;QStringList errors,statuses;QList<QVector<short>> decoded;
 void errorMessage(const QString&s){errors<<s;}void statusMessage(const QString&s){statuses<<s;}void feedAudioToDecoder(){decoded<<m_audioBuffer;}
 void openWavForDecode(const QString&);void openWavFolderDecode(const QString&);
};
#include "bridge_refactor_wav_decode.inc"
static void check(bool v,const char*s){if(!v){fprintf(stderr,"FAIL %s\n",s);exit(1);}puts(s);}
static void put16(QByteArray&b,int n,quint16 v){qToLittleEndian(v,reinterpret_cast<uchar*>(b.data()+n));}
static void put32(QByteArray&b,int n,quint32 v){qToLittleEndian(v,reinterpret_cast<uchar*>(b.data()+n));}
static void wav(const QString&p,int bits,const QByteArray&pcm){QByteArray b(44,'\0');memcpy(b.data(),"RIFF",4);put32(b,4,36+pcm.size());memcpy(b.data()+8,"WAVEfmt ",8);put32(b,16,16);put16(b,20,1);put16(b,22,1);put32(b,24,12000);put32(b,28,12000*bits/8);put16(b,32,bits/8);put16(b,34,bits);memcpy(b.data()+36,"data",4);put32(b,40,pcm.size());b+=pcm;QFile f(p);if(!f.open(QIODevice::WriteOnly)||f.write(b)!=b.size())exit(2);}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir d;check(d.isValid(),"isolated");DecodiumBridge b;
 b.openWavForDecode(d.filePath("missing.wav"));check(b.errors.size()==1,"missing");QFile t(d.filePath("short.wav"));t.open(QIODevice::WriteOnly);t.write("RIFF");t.close();b.openWavForDecode(t.fileName());check(b.errors.size()==2,"short");
 QByteArray pcm16(6,'\0');put16(pcm16,0,0);put16(pcm16,2,1000);put16(pcm16,4,static_cast<quint16>(-1000));wav(d.filePath("a.wav"),16,pcm16);b.openWavForDecode(d.filePath("a.wav"));check(b.m_audioBuffer==QVector<short>({0,1000,-1000}),"pcm16");QCoreApplication::processEvents();check(b.decoded.size()==1 && b.decoded.last()==b.m_audioBuffer,"dispatch16");
 QByteArray pcm8;pcm8.append(char(128));pcm8.append(char(255));pcm8.append(char(0));wav(d.filePath("b.WAV"),8,pcm8);b.openWavForDecode(d.filePath("b.WAV"));check(b.m_audioBuffer==QVector<short>({0,32512,-32768}),"pcm8");QCoreApplication::processEvents();check(b.decoded.size()==2 && b.decoded.last()==b.m_audioBuffer,"dispatch8");
 wav(d.filePath("bad.dat"),24,QByteArray(6,'x'));b.openWavForDecode(d.filePath("bad.dat"));check(b.errors.size()==3,"unsupported");b.openWavFolderDecode(d.filePath("missing-dir"));check(b.errors.size()==4,"missing-folder");
 QDir().mkdir(d.filePath("empty"));b.openWavFolderDecode(d.filePath("empty"));check(b.statuses.last().contains("No WAV"),"empty-folder");
 QDir().mkdir(d.filePath("batch"));wav(d.filePath("batch/a.wav"),16,pcm16);wav(d.filePath("batch/b.WAV"),8,pcm8);b.openWavFolderDecode(d.filePath("batch"));QEventLoop loop;QTimer::singleShot(1100,&loop,&QEventLoop::quit);loop.exec();check(b.decoded.size()==4 && b.decoded.at(2)==QVector<short>({0,1000,-1000}) && b.decoded.at(3)==QVector<short>({0,32512,-32768}),"batch");
}
