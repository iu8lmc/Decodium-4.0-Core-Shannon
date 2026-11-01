// SPDX-License-Identifier: GPL-3.0-or-later
// Developer-only oracle linked to pinned QSSTV sources; not app code.
#include "FAC.h"
#include "ConvEncoder.h"
#include "BitInterleaver.h"
#include "EnergyDispersal.h"
#include "QAMMapping.h"
#include <cstdio>
class OracleParameter : public CParameter {
public:
    void setup(int frame, int bandwidth, int depth, int constellation, int protection, int packet) {
        eSpectOccup = bandwidth ? SO_1 : SO_0;
        iFrameIDTransm = frame;
        eSymbolInterlMode = depth ? SI_SHORT : SI_LONG;
        eMSCCodingScheme = constellation == 0 ? CS_1_SM : (constellation == 1 ? CS_2_SM : CS_3_SM);
        MSCPrLe.iPartB = protection;
        Service[0].eAudDataFlag = CService::SF_DATA;
        Service[0].DataParam.iPacketID = packet;
        Service[0].strLabel = "TEST123";
    }
};
int main() {
    OracleParameter parameters;
    for (int frame = 0; frame < 3; ++frame)
    for (int bandwidth = 0; bandwidth < 2; ++bandwidth)
    for (int depth = 0; depth < 2; ++depth)
    for (int constellation = 0; constellation < 3; ++constellation)
    for (int protection = 0; protection < 2; ++protection)
    for (int packet = 0; packet < 4; ++packet) {
    parameters.setup(frame, bandwidth, depth, constellation, protection, packet);
    std::printf("%d %d %d %d %d %d ", frame, bandwidth, depth, constellation, protection, packet);
    CVector<_BINARY> input(48); input.Reset(0);
    CFACTransmit fac; fac.FACParam(&input, parameters);
    for (int i=0; i<48; ++i) std::putchar(input[i] ? '1' : '0');
    std::putchar(' ');
    CEngergyDispersal disperser; disperser.Init(48, 0); disperser.ProcessData(&input);
    CVector<_DECISION> decisions(48), coded(90), unused(1);
    for (int i=0; i<48; ++i) decisions[i] = BitToSoft(input[i]);
    CConvEncoder encoder; encoder.Init(CS_1_SM, CT_FAC, 0, 45, 0, 48, 0, 6, 0);
    encoder.Encode(decisions, coded);
    CBitInterleaver interleaver; interleaver.Init(0, 90, 21); interleaver.Interleave(coded);
    CQAMMapping mapping; mapping.Init(45, CS_1_SM);
    CVector<_COMPLEX> cells(45);
    mapping.Map(coded, unused, unused, unused, unused, unused, &cells);
    for (int i=0; i<45; ++i) {
        std::putchar(cells[i].real() < 0 ? '1' : '0');
        std::putchar(cells[i].imag() < 0 ? '1' : '0');
    }
    std::puts("");
    }
}
