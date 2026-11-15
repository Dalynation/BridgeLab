"""Installed-app integration checks. Run after importing SQ8L and TAL.
The development tree is temporarily moved aside, then always restored.
"""
from pathlib import Path
import os,subprocess,json,shutil,time,hashlib
repo=Path(__file__).resolve().parents[3];source=repo/'outputs/BridgeLab';scratch=repo/'work/clean-start';home=Path.home()/'Library/Application Support/BridgeLab';app=Path.home()/'Desktop/BridgeLab.app/Contents/MacOS/BridgeLab'
rows=json.loads((home/'plugins.json').read_text());sq=next(x for x in rows if 'SQ8L' in x['name']);tal=next(x for x in rows if 'TAL-' in x['name'])
assert len({x['processor_uid'] for x in rows})==len(rows)
hosts=scratch/'hosts';hosts.mkdir(exist_ok=True)
for name,path in [('smoke',source/'build/vst3/bridge-smoke'),('multi',source/'build/vst3/bridge-multi'),('validator',source/'build/vst3/bin/Release/validator')]:shutil.copy2(path,hosts/name)
report={'version':'0.9.1','macos':subprocess.check_output(['sw_vers','-productVersion'],text=True).strip(),'hardware_arch':os.uname().machine,'plugins':[{k:r[k] for k in ['name','architecture','processor_uid','wrapper','sha256']} for r in rows],'checks':[]}
def run(label,args,timeout=150,env=None):
 begin=time.monotonic();p=subprocess.run(list(map(str,args)),stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,timeout=timeout,env=env)
 row={'test':label,'returncode':p.returncode,'seconds':round(time.monotonic()-begin,2),'stdout':p.stdout[-3000:],'stderr':p.stderr[-1500:]}
 report['checks'].append(row);print(label, p.returncode,flush=True)
 (scratch/'release-results-progress.json').write_text(json.dumps(report,indent=2))
 return p
hidden=scratch/('development-offline-'+str(int(time.time())))
os.rename(source,hidden)
try:
 report['development_tree_absent_during_tests']=not source.exists()
 run('installed app without development tree',[app,'doctor'])
 for plugin in [sq,tal]:
  for arch in ['arm64','x86_64']:
   for mode in ['offline','live']:
    out=scratch/(plugin['id']+'-'+arch+'-'+mode+'.f32');cmd=['/usr/bin/arch','-'+arch,hosts/'smoke',plugin['wrapper'],out]+(['realtime'] if mode=='live' else [])
    run(plugin['name']+' '+arch+' '+mode,cmd)
   off=(scratch/(plugin['id']+'-'+arch+'-offline.f32')).read_bytes();live=(scratch/(plugin['id']+'-'+arch+'-live.f32')).read_bytes()
   report['checks'].append({'test':plugin['name']+' '+arch+' exact live/offline audio','passed':off==live,'sha256_offline':hashlib.sha256(off).hexdigest(),'sha256_live':hashlib.sha256(live).hexdigest()})
 for arch in ['arm64','x86_64']:run('three instances and state recall '+arch,['/usr/bin/arch','-'+arch,hosts/'multi',sq['wrapper'],tal['wrapper']])
 for plugin in [sq,tal]:
  run('VST3 validation '+plugin['name'],[hosts/'validator',plugin['wrapper']],timeout=240)
  out=scratch/(plugin['id']+'-app-'+str(time.time_ns())+'.wav');run('packaged app render '+plugin['name'],[app,'render',plugin['path'],out])
 before=(home/'plugins.json').read_bytes();run('idempotent reimport',[app,'import',sq['original']]);after=json.loads((home/'plugins.json').read_text())
 report['idempotent_import_count_unchanged']=len(after)==len(rows)
 invalid=scratch/'invalid.dll';invalid.write_bytes(b'not a Windows DLL');p=run('invalid DLL rejected',[app,'import',invalid]);report['invalid_rejected_without_registry_change']=p.returncode!=0 and len(json.loads((home/'plugins.json').read_text()))==len(rows)
finally:
 os.rename(hidden,source)
 report['development_tree_restored']=source.exists()
 (source/'release-test-results.json').write_text(json.dumps(report,indent=2)+'\n')
print('Results saved to',source/'release-test-results.json')
