#include "CellMappingTable.h"
#include <cstdio>
#include <cstring>
int main(int argc, char** argv) {
 const bool pilots = argc == 2 && std::strcmp(argv[1], "pilots") == 0;
 const ERobMode modes[] = {RM_ROBUSTNESS_MODE_A, RM_ROBUSTNESS_MODE_B, RM_ROBUSTNESS_MODE_E};
 for (int m=0;m<3;++m) for(int b=0;b<2;++b) {
  CCellMappingTable t; t.MakeTable(modes[m], b ? SO_1 : SO_0);
  std::printf("%d %d %d %d %d %d %d\n",m,b,t.iNumSymbolsPerSuperframe,t.iCarrierKmin,t.iCarrierKmax,t.iNumUsefMSCCellsPerFrame,t.iNumCarrier);
  for(int s=0;s<t.iNumSymbolsPerSuperframe;++s) {
   for(int c=0;c<t.iNumCarrier;++c) {
    if (!pilots) std::printf("%02x",t.matiMapTab[s][c]);
    else if (_IsPilot(t.matiMapTab[s][c])) {
     auto value = t.matcPilotCells[s][c];
     std::printf("%.17g %.17g ", double(value.real()), double(value.imag()));
    }
   }
   std::putchar('\n');
  }
 }
}
