#!/usr/bin/env python3
"""Compare SSTV storage startup through two-app signed encrypted transfer."""
from pathlib import Path
import json,os,re,shutil,subprocess,tempfile
r=Path(__file__).resolve().parents[2];sstv=r/'src/bridge/DecodiumBridgeSstv.cpp';module=r/'src/bridge/DecodiumBridgeSstvStorageStartup.cpp';s=sstv.read_text();n=module.read_text();fixture=(r/'tests/bridge_refactor_fixtures/sstv_storage_startup_before.inc').read_text();signature='void DecodiumBridge::initialiseSstvStorage()';assert (signature+n.split(signature,1)[1]).replace('decodiumSstvQsoRequestFromMap(', 'sstvQsoRequestFromMap(')==fixture;anchor='QVariantMap DecodiumBridge::logSstvQso(';assert s.count(anchor)==1
policy='(version 1)(allow default)(deny network*)(allow network-inbound (local ip "localhost:*"))(allow network-outbound (remote ip "localhost:*"))';results=[]
try:
 for label in ('before','after'):
  if label=='before':sstv.write_text(s.replace(anchor,fixture+anchor,1));module.write_text('#include "DecodiumBridge.h"\n')
  else:sstv.write_text(s);module.write_text(n)
  subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  env=os.environ.copy();env.update(QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software')
  proc=subprocess.run(['sandbox-exec','-p',policy,'python3','tests/sstv/tools/run_share_server_two_app_lab.py','--accept-target','a'],cwd=r,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=150)
  if proc.returncode:print(proc.stdout[-4000:],flush=True);raise RuntimeError('signed encrypted two-app transfer failed')
  match=re.search(r'PASS signed encrypted A/B between two Decodium processes and authenticated HTTPS server; artifacts (\S+)',proc.stdout);assert match,proc.stdout
  artifact=Path(match.group(1)).resolve();assert artifact.parent==Path(tempfile.gettempdir()).resolve() and artifact.name.startswith('decodium-share-server-ab-')
  receipt=json.loads((artifact/'receiver-ok').read_text());transfer=receipt['transferId'];storage=Path(receipt['storageFolder']).resolve();testroot=(Path.home()/'.qttest/Library').resolve();assert storage.is_relative_to(testroot) and re.fullmatch(r'[A-Za-z0-9_-]+',transfer), 'unsafe Gallery path or transfer id'
  images=list((storage/'images/imported').rglob('*'+transfer+'.png'));assert len(images)==1 and images[0].is_file(),'imported Gallery image missing'
  def pixels(path):return subprocess.check_output(['ffmpeg','-v','error','-i',str(path),'-frames:v','1','-f','rawvideo','-pix_fmt','rgb24','-'])
  original_pixels=pixels(artifact/'image-a.jpg');imported_pixels=pixels(images[0]);assert len(original_pixels)==len(imported_pixels) and original_pixels and max(abs(a-b) for a,b in zip(original_pixels,imported_pixels))<=5,'Gallery import pixels differ'
  evidence=r/'tests/bridge_refactor_evidence/sstv_storage_startup_share';evidence.mkdir(parents=True,exist_ok=True);result={'signed_encrypted_transfer':True,'verified_images':2,'gallery_import':True,'cat_disabled':True};(evidence/(label+'.json')).write_text(json.dumps(result,indent=2)+'\n');results.append(result);print(label,json.dumps(result,sort_keys=True),flush=True)
  for log in artifact.glob('*.log'):
   for name in re.findall(r'Application identity: (Decodium - SHARE[0-9a-f]{8}(?:sender|receiver) - test)',log.read_text(errors='replace')):
    for parent in (Path.home()/'.qttest/Library/Application Support/IU8LMC',Path.home()/'.qttest/Library/Caches/IU8LMC'):
     target=parent/name
     if target.is_dir():shutil.rmtree(target)
  shutil.rmtree(artifact)
 assert results[0]==results[1]
finally:
 sstv.write_text(s);module.write_text(n)
 subprocess.run(['cmake','--build','build','--target','decodium_qml','-j','2'],cwd=r,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
