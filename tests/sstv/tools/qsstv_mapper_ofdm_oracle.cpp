// SPDX-License-Identifier: GPL-3.0-or-later
// Independent QSSTV mapper + modulator fixture; input cells are synthetic.
#include "ofdmcellmapping/OFDMCellMapping.h"
#include "OFDM.h"
#include "FAC/FAC.h"
#include "mlc/ConvEncoder.h"
#include "mlc/BitInterleaver.h"
#include "mlc/EnergyDispersal.h"
#include "mlc/QAMMapping.h"
#include "mlc/MLC.h"
#include "interleaver/SymbolInterleaver.h"
#include "datadecoding/DataDecoder.h"
#include <cstring>
#include "logging.h"
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <QFile>
uint txTransportID=0;
int numTxFrames=0;
QList<short unsigned int> fixBlockList;
void CMOTSlideShowEncoder::GetDataUnit(CVector<_BINARY>& data) { data.Init(0); }
logFile* logFilePtr = nullptr;
void logFile::add(const char*, const char*, int, QString, unsigned short) {}
class Mapper : public COFDMCellMapping {
public:
    void setup(CParameter& p) { InitInternal(p); }
    int msc() const { return iInputBlockSize; }
    int fac() const { return iInputBlockSize2; }
    int carriers() const { return iOutputBlockSize; }
    void run(CParameter& p, CVectorEx<_COMPLEX>& a, CVectorEx<_COMPLEX>& b, CVectorEx<_COMPLEX>& out) {
        pvecInputData=&a; pvecInputData2=&b; pvecOutputData=&out; ProcessDataInternal(p);
    }
};
class Modulator : public COFDMModulation {
public:
    void setup(CParameter& p) { SetCarOffset(350.0); InitInternal(p); }
    int samples() const { return iOutputBlockSize; }
    void run(CParameter& p, CVectorEx<_COMPLEX>& in, CVectorEx<_COMPLEX>& out) {
        pvecInputData=&in; pvecOutputData=&out; ProcessDataInternal(p);
    }
};
class MscEncoder : public CMSCMLCEncoder {
public:
    void setup(CParameter& p) { InitInternal(p); }
    int inputLength() const { return iInputBlockSize; }
    int outputLength() const { return iOutputBlockSize; }
    void run(CParameter& p, CVectorEx<_BINARY>& in, CVectorEx<_COMPLEX>& out) {
        pvecInputData=&in; pvecOutputData=&out; ProcessDataInternal(p);
    }
};
class Interleaver : public CSymbInterleaver {
public:
    void setup(CParameter& p) { InitInternal(p); }
    void run(CParameter& p, CVectorEx<_COMPLEX>& in, CVectorEx<_COMPLEX>& out) {
        pvecInputData=&in; pvecOutputData=&out; ProcessDataInternal(p);
    }
};
class Packet : public CDataEncoder {
public:
    void setup(const QByteArray& group, int packetBytes, int continuity) {
        iPacketLen=(packetBytes-3)*8; iTotalPacketSize=packetBytes*8;
        iPacketID=0; iContinInd=continuity; iCurDataPointer=0;
        vecbiCurDataUnit.Init(group.size()*8); vecbiCurDataUnit.ResetBitAccess();
        for (auto c:group) vecbiCurDataUnit.Enqueue(static_cast<unsigned char>(c),8);
        vecbiCurDataUnit.ResetBitAccess();
    }
};
class OracleParameter : public CParameter { public: void setBandwidth() { eSpectOccup=SO_1; } };
int main(int argc,char**argv) {
    const int variant=argc>=2 ? std::atoi(argv[1]) : -1;
    const bool imageMot=argc==4 && std::strcmp(argv[2], "image") == 0;
    const bool objectMot=argc==3 && std::strcmp(argv[2], "object") == 0;
    const bool validMot=imageMot || objectMot || (argc==3 && std::strcmp(argv[2], "mot") == 0);
    const bool validFac=validMot || (argc==3 && std::strcmp(argv[2], "valid") == 0);
    if (variant<0 || variant>1) return 1;
    OracleParameter p; p.CellMappingTable.MakeTable(RM_ROBUSTNESS_MODE_E, SO_1); p.eMSCCodingScheme=CS_2_SM;
    p.setBandwidth(); p.eSymbolInterlMode=CParameter::SI_SHORT; p.MSCPrLe.iPartB=0;
    p.Service[0].eAudDataFlag=CService::SF_DATA; p.Service[0].DataParam.iPacketID=0; p.Service[0].strLabel="DECODIUM";
    Mapper mapper; Modulator modulator; mapper.setup(p); modulator.setup(p);
    MscEncoder mscEncoder; Interleaver symbolInterleaver;
    if (validMot) { mscEncoder.setup(p); symbolInterleaver.setup(p); }
    const QByteArray groups[] = {
        QByteArray::fromHex("7300800012ee55001b000000400d840185000000004600cc0b0073616d706c652e6a706775b5"),
        QByteArray::fromHex("7400800012ee55000412345678ef53")};
    CMOTDABEnc motEncoder;
    if (imageMot) {
        QFile file(QString::fromUtf8(argv[3])); if (!file.open(QIODevice::ReadOnly)) return 2;
        const QByteArray image=file.readAll(); if(image.isEmpty() || image.size()>4096) return 3;
        CMOTObject object; object.strName="sample.jpg"; object.strFormat="";
        object.vecbRawData.Init(image.size()*8); object.vecbRawData.ResetBitAccess();
        for (auto value:image) object.vecbRawData.Enqueue(static_cast<unsigned char>(value),8);
        motEncoder.SetMOTObject(object,64); motEncoder.prepareSegmentList();
    }
    CVectorEx<_COMPLEX> interleaved;
    if (validMot) interleaved.Init(mscEncoder.outputLength());
    int mscOffset=0;
    std::uint32_t seed=0x5144524dU + static_cast<std::uint32_t>(variant);
    const auto sign=[&seed] { seed=seed*1664525U+1013904223U; return (seed&0x80000000U) ? 1.0 : -1.0; };
    CVector<_COMPLEX> facCells(45);
    int facOffset=0;
    const int totalSymbols = imageMot ? 9*p.CellMappingTable.iNumSymPerFrame : p.CellMappingTable.iNumSymbolsPerSuperframe;
    for(int symbol=0; symbol < totalSymbols; ++symbol) {
        if (validMot && symbol % p.CellMappingTable.iNumSymPerFrame == 0) {
            const int groupIndex = objectMot ? ((symbol / p.CellMappingTable.iNumSymPerFrame + variant) % 2) : variant;
            QByteArray group = groups[groupIndex];
            if (imageMot) {
                CVector<_BINARY> groupBits; motEncoder.GetDataGroup(groupBits); groupBits.ResetBitAccess();
                group.clear();
                for (int i=0;i<groupBits.Size()/8;++i) group.append(static_cast<char>(groupBits.Separate(8)));
            }
            // CDataEncoder owns an unused slideshow encoder whose constructor
            // resets QSSTV's global transport ID; preserve the active object.
            const uint activeTransportId = txTransportID;
            Packet packetEncoder; txTransportID = activeTransportId;
            packetEncoder.setup(group, mscEncoder.inputLength()/8,
                                imageMot ? (symbol / p.CellMappingTable.iNumSymPerFrame) % 8 : 0);
            CVector<_BINARY> packet; packetEncoder.GeneratePacket(packet);
            CVectorEx<_BINARY> input; input.Init(mscEncoder.inputLength()); input.Reset(0);
            for (int i=0;i<packet.Size();++i) input[i]=packet[i];
            CVectorEx<_COMPLEX> encoded; encoded.Init(mscEncoder.outputLength());
            mscEncoder.run(p,input,encoded); symbolInterleaver.run(p,encoded,interleaved);
            mscOffset=0;
        }
        if (validFac && symbol % p.CellMappingTable.iNumSymPerFrame == 0) {
            CVector<_BINARY> bits(48); bits.Reset(0); CFACTransmit fac; fac.FACParam(&bits, p);
            CEngergyDispersal disperser; disperser.Init(48,0); disperser.ProcessData(&bits);
            CVector<_DECISION> decisions(48), coded(90), unused(1);
            for(int i=0;i<48;++i) decisions[i]=BitToSoft(bits[i]);
            CConvEncoder encoder; encoder.Init(CS_1_SM,CT_FAC,0,45,0,48,0,6,0); encoder.Encode(decisions,coded);
            CBitInterleaver interleaver; interleaver.Init(0,90,21); interleaver.Interleave(coded);
            CQAMMapping mapping; mapping.Init(45,CS_1_SM); mapping.Map(coded,unused,unused,unused,unused,unused,&facCells);
            facOffset=0;
        }
        CVectorEx<_COMPLEX> a,b,c,out; a.Init(mapper.msc()); b.Init(mapper.fac()); c.Init(mapper.carriers()); out.Init(modulator.samples());
        for(int i=0;i<mapper.msc();++i) {
            if (validMot) a[i]=interleaved[mscOffset++];
            else { const double re=sign()*0.31622776601683794; const double im=sign()*0.31622776601683794; a[i]=_COMPLEX(re,im); }
        }
        for(int i=0;i<mapper.fac();++i) {
            if (validFac) b[i]=facCells[facOffset++];
            else { const double re=sign()*0.70710678118654752; const double im=sign()*0.70710678118654752; b[i]=_COMPLEX(re,im); }
        }
        mapper.run(p,a,b,c); modulator.run(p,c,out);
        for(int i=0;i<modulator.samples();++i) {
            const auto sample=static_cast<std::int16_t>(out[i].real()*1000.0);
            const auto value=static_cast<std::uint16_t>(sample);
            std::putchar(value & 255); std::putchar((value>>8)&255);
        }
    }
}
