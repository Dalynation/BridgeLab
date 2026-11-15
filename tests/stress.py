from pathlib import Path
import subprocess,os,json,hashlib,concurrent.futures
root=Path(__file__).resolve().parents[3];bridge=root/'outputs/BridgeLab';rows=json.loads((root/'work/compatibility/offline-results.json').read_text());jobs=[rows[0],rows[-1],rows[-1],rows[-1]]
def run(index,row):
 out=root/'work/compatibility'/f'concurrent-{index}.f32'
 result=subprocess.run([str(bridge/'build/vst3/bridge-smoke'),str(bridge/'BridgeLabInstrument.vst3'),str(out),'realtime'],env={**os.environ,'BRIDGELAB_TEST_DLL':row['path']},capture_output=True,text=True,timeout=45)
 return dict(plugin=row['plugin'],returncode=result.returncode,stdout=result.stdout.strip(),stderr=result.stderr[-1000:],audio_sha256=hashlib.sha256(out.read_bytes()).hexdigest() if out.exists() else None)
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:results=list(pool.map(lambda pair:run(*pair),enumerate(jobs)))
(root/'work/compatibility/stress-results.json').write_text(json.dumps(results,indent=2))
for row in results:print(json.dumps(row))
