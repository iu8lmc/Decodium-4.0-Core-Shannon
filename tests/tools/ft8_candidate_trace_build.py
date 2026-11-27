#!/usr/bin/env python3
"""Create a separate FT8 candidate trace executable, then restore the build."""
from pathlib import Path
import shutil
import subprocess

root=Path(__file__).resolve().parents[2]
source=root/'Detector/FtxFt8Stage4.cpp'
original=source.read_bytes()
text=original.decode()
anchor='''      append_ft8_hint_candidates (&state.a7[static_cast<size_t> (jseq)], request,
'''
trace='''      if (std::getenv ("DECODIUM_LAB_FT8_CANDIDATES"))
        for (int index = 0; index < ncand; ++index)
          {
            float const freq = candidate[static_cast<size_t> (index * 4)];
            if (freq >= 735.0f && freq <= 775.0f)
              std::fprintf (stderr, "[CAND-TRACE] pass=%d index=%d freq=%.3f dt=%.4f sync=%.5f flag=%.0f\\n",
                            ipass, index, static_cast<double> (freq),
                            static_cast<double> (candidate[static_cast<size_t> (index * 4 + 1)]),
                            static_cast<double> (candidate[static_cast<size_t> (index * 4 + 2)]),
                            static_cast<double> (candidate[static_cast<size_t> (index * 4 + 3)]));
          }
'''
if text.count(anchor)!=1:raise RuntimeError('unexpected candidate insertion point')
result_anchor='''          xdt -= 0.5f;

          int nsnr = 0;
'''
result_trace='''          if (std::getenv ("DECODIUM_LAB_FT8_CANDIDATES")
              && candidate[static_cast<size_t> (icand * 4)] >= 735.0f
              && candidate[static_cast<size_t> (icand * 4)] <= 775.0f)
            std::fprintf (stderr, "[CAND-RESULT] pass=%d index=%d freq=%.3f crc=%d hard=%d msg=%s\\n",
                          ipass, icand, static_cast<double> (candidate[static_cast<size_t> (icand * 4)]),
                          nbadcrc, nharderrors, trim_fixed (msg37).c_str ());
'''
if text.count(result_anchor)!=1:raise RuntimeError('unexpected candidate result point')
false_anchor='''\t          if (checked_badcrc != 0)
\t            {
'''
false_trace='''                  if (std::getenv ("DECODIUM_LAB_FT8_CANDIDATES"))
                    std::fprintf (stderr, "[FALSE8-RESULT] i3=%d n3=%d ap=%d hard=%d dmin=%.5f msg=%s\\n",
                                  candidate_i3, candidate_n3, pass_iaptype, pass_nharderrors,
                                  static_cast<double> (pass_dmin), trim_fixed (candidate_msg).c_str ());
'''
if text.count(false_anchor)!=1:raise RuntimeError('unexpected false8 insertion point')
try:
    source.write_bytes(text.replace(anchor,trace+anchor,1).replace(result_anchor,result_trace+result_anchor,1).replace(false_anchor,false_anchor+false_trace,1).encode())
    subprocess.run(['cmake','--build','build-baseline','--target','decodium_qml','-j2'],cwd=root,check=True,stdout=subprocess.DEVNULL)
    shutil.copy2(root/'build-baseline/decodium',root/'build-baseline/decodium-ft8-candidate-trace')
finally:
    source.write_bytes(original)
    subprocess.run(['cmake','--build','build-baseline','--target','decodium_qml','-j2'],cwd=root,check=True,stdout=subprocess.DEVNULL)
