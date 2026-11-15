from pathlib import Path
import subprocess,os,json,time,hashlib,signal,struct
root=Path(__file__).resolve().parents[3];bridge=root/'outputs/BridgeLab';base=json.loads((root/'work/compatibility/offline-results.json').read_text())
host=bridge/'build/vst3/bridge-smoke';bundle=bridge/'BridgeLabInstrument.vst3';cases=[]
for p in base:
 for live in [False,True]:cases.append((p['plugin'],p['path'],'arm64',48000,256,live))
for p in [base[0],base[-1]]:cases.append((p['plugin'],p['path'],'x86_64',48000,256,False))
for block in [64,257,1024]:cases.append(('SQ8L.dll',base[0]['path'],'arm64',48000,block,False))
for rate in [44100,96000]:cases.append(('SQ8L.dll',base[0]['path'],'arm64',rate,256,False))
results=[]
for name,path,arch,rate,block,live in cases:
 label=f'{name}-{arch}-{rate}-{block}-'+('live' if live else 'offline');out=root/'work/compatibility'/(label+'.f32')
 env={**os.environ,'BRIDGELAB_TEST_DLL':path,'BRIDGELAB_TEST_RATE':str(rate),'BRIDGELAB_TEST_BLOCK':str(block)}
 cmd=['/usr/bin/arch','-'+arch,str(host),str(bundle),str(out)]+(['realtime'] if live else [])
 row=dict(plugin=name,host_arch=arch,rate=rate,block=block,realtime=live);start=time.monotonic()
 process=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=env,start_new_session=True)
 try:
  stdout,stderr=process.communicate(timeout=45);row.update(returncode=process.returncode,stdout=stdout.decode(errors='replace').strip(),stderr=stderr.decode(errors='replace')[-1500:])
 except subprocess.TimeoutExpired:
  os.killpg(process.pid,signal.SIGKILL);process.communicate();row.update(returncode=-9,error='45s timeout')
 row['seconds']=round(time.monotonic()-start,2)
 if out.exists():row['audio_sha256']=hashlib.sha256(out.read_bytes()).hexdigest();row['bytes']=out.stat().st_size
 results.append(row);(root/'work/compatibility/matrix-results.json').write_text(json.dumps(results,indent=2));print(json.dumps(row),flush=True)
