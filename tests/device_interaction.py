"""Original editor input during Core Audio, plus four concurrent audio clients."""
from pathlib import Path
import concurrent.futures,json,os,re,subprocess
root=Path(__file__).resolve().parents[1];home=Path(os.environ['BRIDGELAB_HOME']).resolve();out=Path(os.environ['BRIDGELAB_QA_OUTPUT'])/'interaction';out.mkdir(parents=True,exist_ok=True)
if not (home/'.bridgelab-qa').is_file():raise SystemExit('Disposable .bridgelab-qa profile required')
rows=json.loads((home/'plugins.json').read_text());results=[];cleanup=[];log=home/'logs/host.log'
def run(row,arch,label,editor=False):
 path=out/(label+'.f32');env=dict(os.environ,BRIDGELAB_TEST_FRAMES='480000' if editor else '153600',BRIDGELAB_TEST_RATE='48000',BRIDGELAB_TEST_BLOCK='64' if editor else '256')
 if editor:env['BRIDGELAB_TEST_EDITOR']='SQ8L' if 'SQ8L' in row['name'] else 'TAL'
 p=subprocess.Popen(['/usr/bin/arch','-'+arch,str(root/'build/vst3/bridge-device'),row['wrapper'],str(path)],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
 try:stdout,stderr=p.communicate(timeout=120)
 except subprocess.TimeoutExpired:p.kill();p.communicate();raise
 metadata=log.read_text();rt=('transport=shared pid='+str(p.pid)+' scheduler=1') in metadata
 result=dict(test=label,arch=arch,plugin=row['name'],exit=p.returncode,native_realtime=rt,stdout=stdout.strip(),stderr=stderr)
 result['passed']=p.returncode==0 and rt and (('editor_changed=1' in stdout) if editor else path.read_bytes()==(out/(row['id']+'-reference.f32')).read_bytes())
 return result
try:
 for arch in ['arm64','x86_64']:
  for row in rows:
   offset=log.stat().st_size;r=run(row,arch,row['id']+'-'+arch+'-editor',True);quanta=re.findall(r'underrun quanta=(\d+)',log.read_text()[offset:]);r['underrun_quanta']=quanta;r['passed']=r['passed'] and quanta==['0'];results.append(r);print(json.dumps(r),flush=True)
 for row in rows:
  env=dict(os.environ,BRIDGELAB_TEST_FRAMES='153600',BRIDGELAB_TEST_RATE='48000',BRIDGELAB_TEST_BLOCK='256')
  subprocess.run([str(root/'build/vst3/bridge-smoke'),row['wrapper'],str(out/(row['id']+'-reference.f32'))],env=env,capture_output=True,check=True,timeout=120)
 offset=log.stat().st_size
 with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
  futures=[pool.submit(run,row,arch,'concurrent-'+str(i)) for i,(row,arch) in enumerate([(rows[0],'arm64'),(rows[-1],'arm64'),(rows[0],'x86_64'),(rows[-1],'x86_64')])]
  group=[f.result() for f in futures]
 quanta=re.findall(r'underrun quanta=(\d+)',log.read_text()[offset:]);results.extend(group);results.append(dict(test='four concurrent Core Audio clients: underruns',passed=quanta==['0']*4,underrun_quanta=quanta))
 for r in results[4:]:print(json.dumps(r),flush=True)
finally:
 server=home/'engine/runtime/Wine Crossover.app/Contents/Resources/wine/bin/wineserver'
 for arch in ['x86','x64']:
  env=dict(os.environ,WINEPREFIX=str(home/'prefixes'/('crossover-'+arch)),WINEDEBUG='-all')
  for option in ['-k','-w']:
   p=subprocess.run([str(server),option],env=env,capture_output=True,text=True,timeout=15);cleanup.append(dict(arch=arch,option=option,exit=p.returncode))
 (out/'results.json').write_text(json.dumps(dict(results=results,cleanup=cleanup),indent=2)+'\n')
raise SystemExit(0 if len(results)==9 and all(r['passed'] for r in results) else 1)
