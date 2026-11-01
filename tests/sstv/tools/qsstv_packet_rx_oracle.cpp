// SPDX-License-Identifier: GPL-3.0-or-later
// Developer probe: the two sourceDecoder method bodies are extracted verbatim
// from the separately checked QSSTV checkout at build time. This shim supplies
// only their fields, logging stubs and globals; it is not the QSSTV application.
#include <QByteArray>
#include <QFile>
#include <QImage>
#include <array>
#include <cmath>
#include <cfloat>
#include <iostream>
#include <string>
#include <vector>

enum edataGroupType {GENDATA,CAMESS,GENCA,MOTHEAD,MOTDATA,MOTDATACA};
struct dataPacket {
    QByteArray ba;
    bool extFlag{}, crcFlag{}, sessionFlag{}, userFlag{}, crcOK{};
    edataGroupType dataGroupType{};
    unsigned short segmentNumber{}, transportID{}, segmentSize{};
    bool lastSegment{};
    unsigned char userAccessField{};
    int lenght{};
    void log() {}
    unsigned int advance(int count) { ba.remove(0,count); return unsigned(ba.size()); }
    unsigned int chop(int count) { ba.chop(count); return unsigned(ba.size()); }
};
struct dataBlock {
    bool firstFlag{},lastFlag{},PPI{},crcOK{};
    short packetID{};
    unsigned char continuityIndex{};
    unsigned int length{};
    QByteArray ba;
    void log() {}
};
int msc_valid = 0;
int currentSegmentNumber = 0;
constexpr int INVALID = -1;
void crc16_bytewise(double*, unsigned char*, long);
class sourceDecoder {
public:
    bool setupDataBlock(unsigned char*,bool,int);
    bool setupDataPacket(QByteArray);
    dataBlock currentDataBlock;
    dataPacket currentDataPacket;
    QByteArray holdingBuffer;
    short lastContinuityIndex {-1};
};
#include "qsstv_extracted_packet_methods.inc"

int main(int argc, char** argv) {
    if (argc != 3) return 11;
    std::array<QByteArray,2> expected;
    for (int i=0;i<2;++i) {
        QFile file(QString::fromLocal8Bit(argv[i+1]));
        if (!file.open(QIODevice::ReadOnly)) return 12;
        expected[i]=file.readAll();
        const QImage decoded=QImage::fromData(expected[i], "JPEG");
        if (decoded.isNull() || decoded.size()!=QSize(16,16)) return 13;
    }

    int variant, kind, segment, last, transport, count, rows=0, corruptedRows=0, packets=0, objects=0, continuityErrors=0;
    std::string payloadHex;
    sourceDecoder decoder;
    std::array<QByteArray,2> bodies;
    std::array<int,2> nextSegment {};
    std::array<bool,2> sawContinuityFailure {};
    while (std::cin >> variant >> kind >> segment >> last >> transport >> payloadHex >> count) {
        for (int i=0;i<count;++i) {
            std::string wireHex;
            if (!(std::cin >> wireHex)) return 2;
            QByteArray wire=QByteArray::fromHex(QByteArray::fromStdString(wireHex));
            if (wire.size()<4) return 3;
            double checksum=1.0;
            crc16_bytewise(&checksum,reinterpret_cast<unsigned char*>(wire.data()),wire.size());
            if (std::fabs(checksum)>DBL_EPSILON) return 4;
            QByteArray corrupted=wire;
            corrupted[1] = char(corrupted[1] ^ 1);
            double bad=0.0;
            crc16_bytewise(&bad,reinterpret_cast<unsigned char*>(corrupted.data()),corrupted.size());
            if (std::fabs(bad)<=DBL_EPSILON) return 5;
            const bool complete=decoder.setupDataBlock(reinterpret_cast<unsigned char*>(wire.data()),true,wire.size());
            if (complete != (i==count-1 && variant<2)) return 6;
            if (variant>=4 && i>=1 && decoder.lastContinuityIndex!=-1) return 16;
            ++packets;
        }
        if (variant>=4) { ++continuityErrors; sawContinuityFailure[variant-4]=true; continue; }
        if (variant>=2) { ++corruptedRows; continue; }
        const auto& p=decoder.currentDataPacket;
        if (int(p.dataGroupType)!=kind || p.segmentNumber!=segment || int(p.lastSegment)!=last
            || p.transportID!=transport || p.ba.toHex()!=QByteArray::fromStdString(payloadHex)) return 7;
        if (kind==3) { bodies[variant].clear(); nextSegment[variant]=0; }
        if (kind==4) {
            if (segment!=nextSegment[variant]++) return 9;
            bodies[variant].append(p.ba);
            if (last) {
                if (bodies[variant]!=expected[variant]) return 10;
                const QImage decoded=QImage::fromData(bodies[variant], "JPEG");
                if (decoded.isNull() || decoded.size()!=QSize(16,16)) return 14;
                const QColor center=decoded.pixelColor(8,8);
                if (variant==0 ? center.red()<200 || center.blue()>40
                               : center.blue()<200 || center.red()>40) return 15;
                ++objects;
            }
        }
        ++rows;
    }
    if (rows!=24 || corruptedRows!=24 || objects!=4 || continuityErrors<2
        || !sawContinuityFailure[0] || !sawContinuityFailure[1]) return 8;
    std::cout << "PASS: " << rows << " native MOT groups, " << packets
              << " packets, " << objects << " A/B objects; " << corruptedRows << " corrupted MOT groups and " << continuityErrors << " continuity errors rejected by QSSTV RX functions\n";
}
