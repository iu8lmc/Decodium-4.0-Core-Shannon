// SPDX-License-Identifier: GPL-3.0-or-later
// Developer-only harness around QSSTV CDataEncoder; never linked into the app.
#include "DataDecoder.h"
#include <cstdio>
uint txTransportID = 0;
int numTxFrames = 0;
QList<short unsigned int> fixBlockList;
// QSSTV asks for the next object after producing the final packet. The stub
// ends this one-object probe and does not alter the packet under examination.
void CMOTSlideShowEncoder::GetDataUnit(CVector<_BINARY>& data) { data.Init(0); }
class PacketProbe : public CDataEncoder {
public:
    void setup(unsigned count) {
        iPacketLen = 16 * 8;
        iTotalPacketSize = iPacketLen + 24;
        iPacketID = 1;
        iContinInd = 0;
        iCurDataPointer = 0;
        vecbiCurDataUnit.Init(count * 8);
        vecbiCurDataUnit.ResetBitAccess();
        for (unsigned i = 0; i < count; ++i) vecbiCurDataUnit.Enqueue(i, 8);
        vecbiCurDataUnit.ResetBitAccess();
    }
};
int main() {
    PacketProbe encoder; encoder.setup(35);
    for (int n = 0; n < 3; ++n) {
        CVector<_BINARY> packet;
        encoder.GeneratePacket(packet);
        packet.ResetBitAccess();
        for (int i = 0; i < packet.Size() / 8; ++i) std::printf("%02x", packet.Separate(8));
        std::puts("");
    }
}
