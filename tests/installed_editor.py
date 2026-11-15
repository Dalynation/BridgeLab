"""Exercise the installed runner's actual editor pixels, input, and state."""
from pathlib import Path
import subprocess,os,struct,mmap,select,time,tempfile,json,zlib
HOME=Path(os.environ.get('BRIDGELAB_HOME',str(Path.home()/'Library/Application Support/BridgeLab')))
MAP_BYTES=16+2048*2048*4
class Session:
 def __init__(self,row):
  arch=row['architecture'];runtime='Wine Crossover.app/Contents/Resources/wine/bin/wine64'
  env={**os.environ,'WINEPREFIX':str(HOME/'prefixes'/('crossover-'+arch)),'WINEDEBUG':'-all','MVK_CONFIG_LOG_LEVEL':'0'}
  runtime=os.environ.get('BRIDGELAB_TEST_RUNTIME_'+arch.upper(),str(HOME/'engine/runtime'/runtime))
  env['WINEPREFIX']=os.environ.get('BRIDGELAB_TEST_PREFIX_'+arch.upper(),env['WINEPREFIX'])
  runner=os.environ.get('BRIDGELAB_TEST_RUNNER_'+arch.upper(),str(HOME/'engine/bin'/('runner-'+arch+'.exe')))
  self.p=subprocess.Popen([runtime,runner,'Z:'+row['path'],'--serve','48000','realtime'],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,env=env)
  try:assert struct.unpack('<3I',self.read(12,90))[0]==0x424c3032
  except Exception:
   self.p.kill();self.p.wait();raise
 def read(self,n,timeout=10):
  result=b'';deadline=time.monotonic()+timeout
  while len(result)<n:
   remain=deadline-time.monotonic()
   if remain<=0 or not select.select([self.p.stdout],[],[],max(0,remain))[0]:raise TimeoutError('Runner response timed out')
   data=os.read(self.p.stdout.fileno(),n-len(result))
   if not data:raise RuntimeError('Runner exited')
   result+=data
  return result
 def request(self,cmd=0,payload=b'',program=0):
  self.p.stdin.write(struct.pack('<6I',0x424c3032,256 if cmd==0 else 0,0,program,cmd,len(payload))+bytes(1024)+payload);self.p.stdin.flush()
  raw=self.read(2064);head=struct.unpack_from('<4I',raw);data=self.read(head[3]) if head[3] else b'';assert head[0]==0x424c3032;return head,data
 def input(self,msg,x,y,flags):
  head,_=self.request(7,struct.pack('<IiiI',msg,x,y,flags));assert head[2]==1;time.sleep(.04)
 def pump(self,count=30):
  for _ in range(count):self.request();time.sleep(.005)
 def close(self):
  if self.p.stdin:self.p.stdin.close()
  try:self.p.wait(timeout=10)
  except subprocess.TimeoutExpired:self.p.terminate();self.p.wait(timeout=10)
def test(row,output_dir):
 result={'plugin':row['name']}
 overrides={k:v for k,v in os.environ.items() if k.startswith('BRIDGELAB_TEST_')}
 if overrides:result['test_overrides']=overrides
 try:s=Session(row)
 except Exception as error:
  result.update(passed=False,error=str(error));print(json.dumps(result),flush=True);return result
 fd,path=tempfile.mkstemp(prefix='BridgeLab-test-',suffix='.frame');os.ftruncate(fd,MAP_BYTES);mapping=mmap.mmap(fd,MAP_BYTES);os.close(fd)
 try:
  head,_=s.request(6,('Z:'+path).encode());assert head[2]==1;s.pump(100)
  magic,sequence,w,h=struct.unpack('<4I',mapping[:16]);assert magic==0x424c4631 and sequence>0 and 0<w<=2048 and 0<h<=2048
  _,initial_status=s.request(10);result['initial_window_status']=list(struct.unpack('<4i',initial_status))
  pixels=bytearray(mapping[16:16+w*h*4]);pixels[0::4],pixels[2::4]=pixels[2::4],pixels[0::4];pixels[3::4]=bytes([255])*(w*h)
  def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
  scan=b''.join(b'\0'+pixels[y*w*4:(y+1)*w*4] for y in range(h))
  png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(scan))+chunk(b'IEND',b'')
  (output_dir/('frame-'+row['id']+'.png')).write_bytes(png)
  colors=len(set(mapping[16+i:16+i+3] for i in range(0,w*h*4,4)));result.update(frame_size=[w,h],pixel_colors=colors);assert colors>50,'Editor frame is blank'
  if 'SQ8L' in row['name']:
   _,initial=s.request(3)
   s.input(0x201,295,46,1);s.input(0x202,295,46,0);s.pump();_,preset=s.request(3);assert preset!=initial
   for msg,x,y,flags in [(0x200,234,326,0),(0x201,234,326,1),(0x200,234,320,1),(0x200,234,310,1),(0x200,234,290,1),(0x202,234,290,0)]:s.input(msg,x,y,flags)
   s.pump();_,edited=s.request(3);assert edited!=preset
   _,status=s.request(10);left,top,visible,foreground=struct.unpack('<4i',status);result['after_input_window_status']=[left,top,visible,foreground];assert left<0 and top<0 ,'Windows backing editor moved onscreen'
   result.update(preset_click=True,enabled_knob_drag=True,backing_window_offscreen=True,wine_local_foreground=bool(foreground),mac_foreground_not_determined_by_win32=True)
  else:
   _,initial=s.request(3)
   for msg,x,y,flags in [(0x200,42,322,0),(0x201,42,322,1),(0x200,42,342,1),(0x200,42,362,1),(0x200,42,382,1),(0x202,42,382,0)]:s.input(msg,x,y,flags)
   s.pump();_,edited=s.request(3);assert edited!=initial,'Editor knob input did not change plugin state'
   result['enabled_knob_drag']=True
  _,status=s.request(10);left,top,visible,foreground=struct.unpack('<4i',status);assert left<0 and top<0,'Windows backing editor moved onscreen'
  result['backing_window_offscreen']=True
  s.close();s=Session(row)
  head,_=s.request(4,edited);assert head[2]==1,'Edited state restore rejected'
  _,restored=s.request(3);assert restored==edited,'GUI-edited state changed after runner recreation'
  result['gui_edited_state_recreated_exactly']=True
  result['passed']=True
 except Exception as error:result.update(passed=False,error=str(error))
 finally:
  mapping.close();os.unlink(path);s.close()
 (output_dir/('editor-'+row['id']+'.json')).write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result),flush=True);return result
if __name__=='__main__':
 out=Path(os.environ.get('BRIDGELAB_TEST_OUTPUT',str(Path.cwd()/'work/clean-start')));out.mkdir(parents=True,exist_ok=True);rows=json.loads((HOME/'plugins.json').read_text());results=[test(row,out) for row in rows]
 Path(os.environ.get('BRIDGELAB_TEST_REPORT',str(Path.cwd()/'outputs/BridgeLab/installed-editor-results.json'))).write_text(json.dumps(results,indent=2)+'\n')
 raise SystemExit(0 if all(r['passed'] for r in results) else 1)
