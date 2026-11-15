from pathlib import Path
import subprocess,os,json,hashlib,wave,struct,time,signal
root=Path(__file__).resolve().parents[3];bridge=root/'outputs/BridgeLab';env={**os.environ,'WINEPREFIX':str(bridge/'runtime/prefix'),'WINEDEBUG':'-all','MVK_CONFIG_LOG_LEVEL':'0'}
plugins=[root/'outputs/SQ8L/SQ8L.dll',*sorted((root/'work/compatibility/fb3300').rglob('*.dll')),*sorted((root/'work/compatibility/tal').rglob('*.dll'))]
results=[]
for plugin in plugins:
 data=plugin.read_bytes();offset=struct.unpack_from('<I',data,60)[0];machine=struct.unpack_from('<H',data,offset+4)[0];arch='x86' if machine==0x14c else 'x64'
 out=root/'work/compatibility'/(plugin.stem+'-offline.wav');cmd=[str(bridge/'runtime/Wine Devel.app/Contents/Resources/wine/bin/wine'),str(bridge/'build'/('runner-'+arch+'.exe')),'Z:'+str(plugin),'Z:'+str(out)]
 row={'plugin':plugin.name,'path':str(plugin),'architecture':arch,'sha256':hashlib.sha256(data).hexdigest()};start=time.monotonic()
 process=subprocess.Popen(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=env,cwd=plugin.parent,start_new_session=True)
 try:
  stdout,stderr=process.communicate(timeout=60);row.update(returncode=process.returncode,stdout=stdout.decode(errors='replace').strip(),stderr=stderr.decode(errors='replace')[-1500:])
 except subprocess.TimeoutExpired:
  os.killpg(process.pid,signal.SIGKILL);process.communicate();row.update(returncode=-9,error='Timed out after 60 seconds')
 row['seconds']=round(time.monotonic()-start,2)
 if out.exists():
  with wave.open(str(out)) as f:
   row['frames']=f.getnframes();row['rate']=f.getframerate();pcm=struct.unpack('<%dh'%(f.getnframes()*f.getnchannels()),f.readframes(f.getnframes()));row['pcm_peak']=max(map(abs,pcm))
 results.append(row);Path('work/compatibility/offline-results.json').write_text(json.dumps(results,indent=2));print(json.dumps(row),flush=True)
