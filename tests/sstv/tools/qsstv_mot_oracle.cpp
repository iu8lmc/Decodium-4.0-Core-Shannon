// SPDX-License-Identifier: GPL-3.0-or-later
// Developer-only harness: compile against the pinned, separate QSSTV tree as
// described in docs/sstv/HAMDRM_QSSTV_ORACLE.md. Not linked into Decodium.
#include "DABMOT.h"
#include <cstdio>
uint txTransportID = 0;
int numTxFrames = 0;
QList<short unsigned int> fixBlockList;
int main(int argc, char**) {
    CMOTObject object;
    object.strName = "sample.jpg";
    // The pinned QSSTV implementation transforms into an empty destination
    // string for nonempty formats; an empty format avoids that undefined
    // behaviour and selects QSSTV's default JPEG subtype.
    object.strFormat = "";
    const bool segmented = argc > 1;
    object.vecbRawData.Init(segmented ? 103 * 8 : 32);
    object.vecbRawData.ResetBitAccess();
    if (segmented) {
        for (unsigned i = 0; i < 103U; ++i) object.vecbRawData.Enqueue(i, 8);
    } else {
        for (auto b : {0x12U, 0x34U, 0x56U, 0x78U}) object.vecbRawData.Enqueue(b, 8);
    }
    CMOTDABEnc encoder;
    encoder.SetMOTObject(object, 64);
    encoder.prepareSegmentList();
    for (int n = 0; n < (segmented ? 5 : 2); ++n) {
        CVector<_BINARY> group;
        encoder.GetDataGroup(group);
        group.ResetBitAccess();
        for (int i = 0; i < group.Size() / 8; ++i) std::printf("%02x", group.Separate(8));
        std::puts("");
    }
}
