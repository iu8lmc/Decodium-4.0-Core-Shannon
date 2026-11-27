#!/usr/bin/env python3
"""Compare the original and extracted TX selection across all slots and fallbacks."""
import os, shlex, subprocess, tempfile
from pathlib import Path

root = Path(__file__).resolve().parents[2]
before = (root / 'tests/bridge_refactor_fixtures/txselection_before.inc').read_text()
after = (root / 'src/bridge/DecodiumBridgeTxSelection.cpp').read_text().split('QString DecodiumBridge::buildCurrentTxMessage() const', 1)[1]
after = 'QString DecodiumBridge::buildCurrentTxMessage() const' + after
if before != after:
    raise SystemExit('TX selection changed during extraction')

harness = r'''
#include <QString>
#include <iostream>
constexpr int kSpecialOpHound = 7;
class DecodiumBridge {
public:
 int m_currentTx=1, m_specialOperationActivity=0;
 QString m_tx1="ONE", m_tx2="TWO", m_tx3="THREE", m_tx4="FOUR", m_tx5="FIVE", m_tx6="CQ", m_dxCall="K1ABC";
 QString buildCurrentTxMessage() const;
};
#include "method.inc"
int main() {
 DecodiumBridge b;
 for (int i=-1;i<=7;i++) {b.m_currentTx=i;std::cout << b.buildCurrentTxMessage().toStdString() << '\n';}
 b.m_tx1="  "; b.m_dxCall=""; b.m_currentTx=1;
 std::cout << b.buildCurrentTxMessage().toStdString() << '\n';
 b.m_specialOperationActivity=7; std::cout << '[' << b.buildCurrentTxMessage().toStdString() << "]\n";
 b.m_specialOperationActivity=0; b.m_dxCall="K1ABC"; std::cout << '[' << b.buildCurrentTxMessage().toStdString() << "]\n";
 b.m_dxCall=""; b.m_tx6=" "; std::cout << '[' << b.buildCurrentTxMessage().toStdString() << "]\n";
}
'''
flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'Qt6Core'], text=True))
outputs=[]
with tempfile.TemporaryDirectory(prefix='decodium-txselection-') as tmp:
    p=Path(tmp); (p/'main.cpp').write_text(harness)
    for code in (before, after):
        (p/'method.inc').write_text(code)
        subprocess.run([*shlex.split(os.environ.get('CXX','c++')), '-std=c++17', str(p/'main.cpp'), *flags, '-o', str(p/'test')], check=True)
        outputs.append(subprocess.check_output([str(p/'test')], text=True))
expected='ONE\nONE\nONE\nTWO\nTHREE\nFOUR\nFIVE\nCQ\nONE\nCQ\n[  ]\n[  ]\n[  ]\n'
if outputs != [expected, expected]:
    raise SystemExit('TX selection behaviour mismatch: '+repr(outputs))
print('TX selection A/B OK: slots -1..7, CQ fallback, Hound, partner, empty CQ')
