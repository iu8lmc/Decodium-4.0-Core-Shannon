#pragma once

#include <QStringList>

namespace decodium::cw {

// Porte seriali reali viste da Qt. Su macOS il percorso completo /dev/cu.* e'
// quello giusto da passare a hamlib; non si deve scansionare /dev perche' ttys*
// sono pseudo-terminali del sistema, non cavi CAT.
QStringList availableSerialPorts();

} // namespace decodium::cw
