#include <windows.h>
#include <iostream>
#include <fstream>
#include <array>
#include <thread>
#include <chrono>
#include "../src/editor_gpu.hpp"
static HRESULT result=E_NOTIMPL;static unsigned calls=0;
static HRESULT STDMETHODCALLTYPE simulatedVblank(IDXGIOutput*){++calls;return result;}
int main(){
 std::array<void*,11> table{};table[10]=(void*)&simulatedVblank;
 struct Output{void** vtable;} output{table.data()};
 EditorGPU::installPacingHook(output.vtable,10,(void*)&EditorGPU::pacedVblank);
 auto start=std::chrono::steady_clock::now();
 if(EditorGPU::pacedVblank((IDXGIOutput*)&output)!=S_OK||calls!=1)return 1;
 if(std::chrono::steady_clock::now()-start<std::chrono::milliseconds(10))return 2;
 result=DXGI_ERROR_DEVICE_REMOVED;
 if(EditorGPU::pacedVblank((IDXGIOutput*)&output)!=DXGI_ERROR_DEVICE_REMOVED||calls!=2)return 3;
 result=S_OK;if(EditorGPU::pacedVblank((IDXGIOutput*)&output)!=S_OK||calls!=3)return 4;
 HANDLE alias=nullptr;std::thread waiter;DWORD waited=WAIT_FAILED;
 {
  EditorGPU::FrameGate gate;if(!gate.valid()||!gate.start())return 5;
  HANDLE first=gate.duplicate();alias=gate.duplicate();if(!first||!alias)return 6;
  CloseHandle(first);
  if(WaitForSingleObject(alias,2000)!=WAIT_OBJECT_0)return 7;
  if(WaitForSingleObject(alias,2000)!=WAIT_OBJECT_0)return 8;
  waiter=std::thread([&]{waited=WaitForSingleObject(alias,2000);});
 }
 waiter.join();CloseHandle(alias);
 if(waited!=WAIT_OBJECT_0||EditorGPU::liveTickers.load()!=0)return 9;
 std::cout<<"Unsupported refresh is paced and emulated; device errors preserved; frame-ready aliases and shutdown wakeup: PASS\n";
 return 0;
}
