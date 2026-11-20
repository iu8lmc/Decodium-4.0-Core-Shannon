#!/usr/bin/env python3
"""Compare retired LDPC row arithmetic against live intermediate values."""
from pathlib import Path
import subprocess
from ldpc_minsum_fixture import previous_minsum

root=Path(__file__).resolve().parents[2]
source=root/'Detector/FtxLdpc.cpp'
original=source.read_text()
reference=previous_minsum(original,root)
anchor='''                  tov[static_cast<size_t> (bit)][static_cast<size_t> (edge)] = -kAlphaMs * sign_prod * min_abs;
                }
            }
        }
    }

  for (int i = 0; i < nosd; ++i)'''
if reference.count(anchor)!=1:raise RuntimeError('unexpected LDPC source')
probe='''
          for (int check=0; check<kLdpc17491M; ++check) {
            float signs=1.0f, first=1.0e30f, second=1.0e30f;
            int count=tables.nrw[static_cast<size_t>(check)];
            for(int row=0;row<count;++row) {
              float v=toc[static_cast<size_t>(check)][static_cast<size_t>(row)];
              float a=std::fabs(v); signs*=sign_one(-v);
              if(a<first){second=first;first=a;}else if(a<second)second=a;
            }
            for(int row=0;row<count;++row) {
              float v=toc[static_cast<size_t>(check)][static_cast<size_t>(row)];
              int bit=nm_at(tables,row,check)-1;
              int edge=kLdpc17491ReverseEdges[static_cast<size_t>(row+kLdpc17491MaxRowWeight*check)];
              float actual=tov[static_cast<size_t>(bit)][static_cast<size_t>(edge)];
              float trial=-kAlphaMs*(signs*sign_one(-v))*(std::fabs(v)==first?second:first);
              ++ldpcLabCount;
              if(std::memcmp(&actual,&trial,sizeof(float))!=0) {
                if(++ldpcLabMismatch<=8)std::fprintf(stderr,"[LDPC-LAB-MISMATCH] iter=%d check=%d row=%d actual=%a trial=%a v=%a first=%a second=%a\\n",iter,check,row,actual,trial,v,first,second);
              }
            }
          }
'''
modified=reference.replace('namespace\n{\n','''namespace
{
thread_local unsigned long long ldpcLabCount=0,ldpcLabMismatch=0;
struct LdpcLabReport { ~LdpcLabReport() { std::fprintf(stderr,"[LDPC-LAB-TOTAL] count=%llu mismatch=%llu\\n",ldpcLabCount,ldpcLabMismatch); } };
thread_local LdpcLabReport ldpcLabReport;
''',1)
modified=modified.replace('  clear_outputs (message91_out, cw_out, ntype_out, nharderror_out, dmin_out);','  (void) ldpcLabReport;\n  clear_outputs (message91_out, cw_out, ntype_out, nharderror_out, dmin_out);',1)
needle='''                  tov[static_cast<size_t> (bit)][static_cast<size_t> (edge)] = -kAlphaMs * sign_prod * min_abs;
                }
            }
'''
if modified.count(needle)!=1:raise RuntimeError('unexpected minsum loop')
modified=modified.replace(needle,needle+probe,1)
try:
 source.write_text(modified)
 with (root/'build-baseline/performance/results/ldpc-minsum-intermediate-build.log').open('w') as log:
  subprocess.run(['cmake','--build','build-baseline','--target','test_ft2_legacy_cpu','-j2'],cwd=root,stdout=log,stderr=subprocess.STDOUT,check=True)
 result=subprocess.run([str(root/'build-baseline/tests/test_ft2_legacy_cpu')],cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=100)
 (root/'build-baseline/performance/results/ldpc-minsum-intermediate.log').write_text(result.stdout)
 print('test exit:',result.returncode,flush=True)
 for line in result.stdout.splitlines():
  if '[LDPC-LAB-' in line or 'depth=' in line:print(line,flush=True)
 if '[LDPC-LAB-TOTAL]' not in result.stdout:raise RuntimeError('missing lab report')
finally:
 source.write_text(original)
 subprocess.run(['cmake','--build','build-baseline','--target','test_ft2_legacy_cpu','decodium_qml','-j2'],cwd=root,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)
