// SPDX-License-Identifier: GPL-3.0-or-later
// Minimal file adapter calling the archived author's unmodified encoder.
#include "common/datadecoding/DABMOT.h"
#include <cstdio>
#include <fstream>
#include <string>

int main(int argc, char** argv)
{
    if (argc != 3 || (std::string(argv[1]) != "a" && std::string(argv[1]) != "b")) return 2;
    CMOTObject object;
    object.strName = std::string(argv[1]) == "a" ? "de_TEST1-1-image-a.jpg" : "de_TEST2-1-image-b.jpg";
    object.strFormat = ""; // The reference's default image/JPEG subtype.
    object.vecbRawData.Init(16 * 8);
    object.vecbRawData.ResetBitAccess();
    for (int i = 0; i < 16; ++i) object.vecbRawData.Enqueue(i, 8);
    CMOTDABEnc encoder;
    encoder.Reset();
    encoder.SetMOTObject(object);
    CVector<_BINARY> group;
    encoder.GetDataGroup(group);
    if (group.Size() != 400) return 3;
    group.ResetBitAccess();
    std::ofstream out(argv[2], std::ios::binary);
    for (int i = 0; i < group.Size() / 8; ++i) out.put(static_cast<char>(group.Separate(8)));
    if (!out) return 4;
    out.close();
    encoder.GetDataGroup(group);
    if (group.Size() % 8 != 0 || group.Size() == 0) return 5;
    group.ResetBitAccess();
    std::ofstream body(std::string(argv[2]) + ".body", std::ios::binary);
    for (int i = 0; i < group.Size() / 8; ++i) body.put(static_cast<char>(group.Separate(8)));
    return body ? 0 : 6;
}
