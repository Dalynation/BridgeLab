#pragma once
// Single-surface DirectComposition adapter for original offscreen editors.
// The plugin still draws through its own Direct2D code. Unsupported composition
// surfaces/effects/transforms return errors rather than pretending to render.
// This is not a complete DirectComposition implementation.
#ifdef _WIN32
#include <d3d11.h>
#include <dxgi1_3.h>
#include <dcomp.h>
#include <map>
#include <array>
#include <memory>
#include <algorithm>
#include <vector>
#include <mutex>
#include <atomic>
#include <cstring>
#include <chrono>
#include <condition_variable>
#include <thread>
#include <new>
#include "wire.hpp"
namespace EditorGPU {
inline std::atomic<bool> enabled{false};
inline std::mutex pixelsMutex;
inline std::vector<uint8_t> pixels;
inline UINT width=0,height=0;
inline unsigned presents=0;
inline std::atomic<uint32_t> liveChains{0},liveTickers{0};
inline std::atomic<uint64_t> generation{0};
inline void reset(){std::lock_guard<std::mutex> lock(pixelsMutex);std::vector<uint8_t>{}.swap(pixels);width=height=presents=0;++generation;}
// The application owns one editor per runner. Publish immutable BGRA frames
// through the existing seqlock; the DAW never reads GPU resources or waits here.
inline bool publish(uint8_t* mapping,UINT expectedWidth,UINT expectedHeight){std::lock_guard<std::mutex> lock(pixelsMutex);if(pixels.empty()||width!=expectedWidth||height!=expectedHeight)return false;auto* header=(uint32_t*)mapping;InterlockedIncrement((volatile LONG*)&header[1]);MemoryBarrier();header[0]=editorFrameMagic;header[2]=width;header[3]=height;std::memcpy(mapping+16,pixels.data(),pixels.size());MemoryBarrier();InterlockedIncrement((volatile LONG*)&header[1]);return true;}
inline std::array<uint32_t,4> status(){std::lock_guard<std::mutex> lock(pixelsMutex);return {liveChains.load(),liveTickers.load(),uint32_t(pixels.size()),presents};}
template<class T> HRESULT unsupported(T** out){if(out)*out=nullptr;return E_NOTIMPL;}
// A latency handle is caller-owned. Keep a private event alive independently
// of Present, so an idle renderer and its shutdown path can both make progress.
class FrameGate {
 HANDLE event=CreateEventW(nullptr,FALSE,TRUE,nullptr);
 std::atomic<bool> stop{false};std::mutex mutex;std::condition_variable condition;std::thread thread;
public:
 bool valid()const{return event!=nullptr;}
 bool start()noexcept{if(!event)return false;if(thread.joinable())return true;try{thread=std::thread([this]{++liveTickers;std::unique_lock<std::mutex> lock(mutex);while(!condition.wait_for(lock,std::chrono::microseconds(16667),[this]{return stop.load();}))SetEvent(event);--liveTickers;});return true;}catch(...){return false;}}
 HANDLE duplicate()const noexcept{HANDLE result=nullptr;if(event)DuplicateHandle(GetCurrentProcess(),event,GetCurrentProcess(),&result,0,FALSE,DUPLICATE_SAME_ACCESS);return result;}
 ~FrameGate(){stop=true;condition.notify_all();if(event)SetEvent(event);if(thread.joinable())thread.join();if(event)CloseHandle(event);}
};
struct SwapChain final:IDXGISwapChain2 {
 std::atomic<ULONG> refs{1};IDXGIFactory2* parent=nullptr;ID3D11Device* device=nullptr;ID3D11DeviceContext* context=nullptr;ID3D11Texture2D* texture=nullptr;ID3D11Texture2D* staging=nullptr;DXGI_SWAP_CHAIN_DESC1 desc{};FrameGate ready;UINT count=0;bool counted=false;std::chrono::steady_clock::time_point lastCapture{};
 SwapChain(IDXGIFactory2* p,ID3D11Device* d,const DXGI_SWAP_CHAIN_DESC1& s):parent(p),device(d),desc(s){parent->AddRef();device->AddRef();device->GetImmediateContext(&context);}
 ~SwapChain(){if(counted)--liveChains;if(staging)staging->Release();if(texture)texture->Release();if(context)context->Release();device->Release();parent->Release();}
 HRESULT create(){if(!ready.valid())return E_OUTOFMEMORY;if(!desc.Width||!desc.Height||desc.Width>2048||desc.Height>2048||desc.SampleDesc.Count!=1)return DXGI_ERROR_UNSUPPORTED;
  if(desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM&&desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM_SRGB&&desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM&&desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)return DXGI_ERROR_UNSUPPORTED;
  D3D11_TEXTURE2D_DESC t{};t.Width=desc.Width;t.Height=desc.Height;t.MipLevels=t.ArraySize=1;t.Format=desc.Format;t.SampleDesc.Count=1;t.Usage=D3D11_USAGE_DEFAULT;t.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
  HRESULT hr=device->CreateTexture2D(&t,nullptr,&texture);if(FAILED(hr))return hr;t.Usage=D3D11_USAGE_STAGING;t.BindFlags=0;t.CPUAccessFlags=D3D11_CPU_ACCESS_READ;hr=device->CreateTexture2D(&t,nullptr,&staging);if(SUCCEEDED(hr)){if(!counted){counted=true;++liveChains;}if(!ready.start())return E_OUTOFMEMORY;}return hr;
 }
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out) noexcept override{if(!out)return E_POINTER;*out=nullptr;if(iid==__uuidof(IUnknown)||iid==__uuidof(IDXGIObject)||iid==__uuidof(IDXGIDeviceSubObject)||iid==__uuidof(IDXGISwapChain)||iid==__uuidof(IDXGISwapChain1)||iid==__uuidof(IDXGISwapChain2)){*out=static_cast<IDXGISwapChain2*>(this);AddRef();return S_OK;}return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef() noexcept override{return ++refs;}ULONG STDMETHODCALLTYPE Release() noexcept override{auto n=--refs;if(!n)delete this;return n;}
 HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID,UINT,const void*) noexcept override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID,const IUnknown*) noexcept override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID,UINT*,void*) noexcept override{return DXGI_ERROR_NOT_FOUND;}
 HRESULT STDMETHODCALLTYPE GetParent(REFIID iid,void** out) noexcept override{return parent->QueryInterface(iid,out);}
 HRESULT STDMETHODCALLTYPE GetDevice(REFIID iid,void** out) noexcept override{return device->QueryInterface(iid,out);}
 HRESULT present(UINT flags,const DXGI_PRESENT_PARAMETERS* changes) noexcept {
  if(flags&DXGI_PRESENT_TEST)return S_OK;if(!texture||!staging)return DXGI_ERROR_INVALID_CALL;++count;if(!enabled)return S_OK;
  // Present1 describes only modified pixels. A single headless surface needs
  // the same retained front image that a window compositor normally provides.
  if(changes&&(changes->pScrollRect||changes->pScrollOffset))return DXGI_ERROR_UNSUPPORTED;
  if(changes&&changes->DirtyRectsCount>4096)return DXGI_ERROR_INVALID_CALL;
  RECT full{0,0,LONG(desc.Width),LONG(desc.Height)};
  const RECT* rects=&full;UINT n=1;
  if(changes&&changes->DirtyRectsCount){if(!changes->pDirtyRects)return E_INVALIDARG;rects=changes->pDirtyRects;n=changes->DirtyRectsCount;}
  for(UINT r=0;r<n;++r){auto area=rects[r];if(area.left<0||area.top<0||area.right>LONG(desc.Width)||area.bottom>LONG(desc.Height)||area.right<=area.left||area.bottom<=area.top)return DXGI_ERROR_INVALID_CALL;}
  auto epoch=generation.load();
  for(UINT r=0;r<n;++r){auto area=rects[r];D3D11_BOX box{UINT(area.left),UINT(area.top),0,UINT(area.right),UINT(area.bottom),1};context->CopySubresourceRegion(staging,0,area.left,area.top,0,texture,0,&box);}
  D3D11_MAPPED_SUBRESOURCE mapped{};HRESULT hr=context->Map(staging,0,D3D11_MAP_READ,0,&mapped);if(FAILED(hr))return hr;
  try{std::lock_guard<std::mutex> lock(pixelsMutex);
   if(enabled&&generation.load()==epoch){
    if(width!=desc.Width||height!=desc.Height){width=desc.Width;height=desc.Height;pixels.assign(size_t(width)*height*4,0);}
    for(UINT r=0;r<n;++r){auto area=rects[r];for(LONG y=area.top;y<area.bottom;++y){auto* destination=pixels.data()+(size_t(y)*width+area.left)*4;auto* source=(const uint8_t*)mapped.pData+size_t(y)*mapped.RowPitch+area.left*4;std::memcpy(destination,source,size_t(area.right-area.left)*4);
     if(desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM||desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)for(LONG x=0;x<area.right-area.left;++x)std::swap(destination[x*4],destination[x*4+2]);}}
    ++presents;
   }
  }catch(...){context->Unmap(staging,0);return E_OUTOFMEMORY;}
  context->Unmap(staging,0);return S_OK;
 }
 HRESULT STDMETHODCALLTYPE Present(UINT,UINT flags) noexcept override{return present(flags,nullptr);}
 HRESULT STDMETHODCALLTYPE GetBuffer(UINT i,REFIID iid,void** out) noexcept override{if(!out)return E_POINTER;*out=nullptr;return i==0&&texture?texture->QueryInterface(iid,out):DXGI_ERROR_INVALID_CALL;}
 HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL fullscreen,IDXGIOutput*) noexcept override{return fullscreen?DXGI_ERROR_UNSUPPORTED:S_OK;}
 HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL* full,IDXGIOutput** output) noexcept override{if(full)*full=FALSE;if(output)*output=nullptr;return S_OK;}
 HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC* d) noexcept override{if(!d)return E_POINTER;*d={};d->BufferDesc.Width=desc.Width;d->BufferDesc.Height=desc.Height;d->BufferDesc.Format=desc.Format;d->SampleDesc=desc.SampleDesc;d->BufferUsage=desc.BufferUsage;d->BufferCount=desc.BufferCount;d->Windowed=TRUE;d->SwapEffect=desc.SwapEffect;d->Flags=desc.Flags;return S_OK;}
 HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT n,UINT w,UINT h,DXGI_FORMAT format,UINT flags) noexcept override{if(!w||!h)return DXGI_ERROR_INVALID_CALL;if(staging){staging->Release();staging=nullptr;}if(texture){texture->Release();texture=nullptr;}desc.Width=w;desc.Height=h;if(n)desc.BufferCount=n;if(format!=DXGI_FORMAT_UNKNOWN)desc.Format=format;desc.Flags=flags;return create();}
 HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC*) noexcept override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput** out) noexcept override{if(!out)return E_POINTER;*out=nullptr;IDXGIDevice* dxgi=nullptr;HRESULT hr=device->QueryInterface(__uuidof(IDXGIDevice),(void**)&dxgi);if(FAILED(hr))return hr;IDXGIAdapter* adapter=nullptr;hr=dxgi->GetAdapter(&adapter);dxgi->Release();if(FAILED(hr))return hr;hr=adapter->EnumOutputs(0,out);adapter->Release();return hr;}
 HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS*) noexcept override{return DXGI_ERROR_FRAME_STATISTICS_DISJOINT;}
 HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT* p) noexcept override{if(!p)return E_POINTER;*p=count;return S_OK;}
 HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_SWAP_CHAIN_DESC1* d) noexcept override{if(!d)return E_POINTER;*d=desc;return S_OK;}
 HRESULT STDMETHODCALLTYPE GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC* d) noexcept override{if(!d)return E_POINTER;*d={};d->Windowed=TRUE;return S_OK;}
 HRESULT STDMETHODCALLTYPE GetHwnd(HWND* out) noexcept override{if(out)*out=nullptr;return DXGI_ERROR_INVALID_CALL;}
 HRESULT STDMETHODCALLTYPE GetCoreWindow(REFIID,void** out) noexcept override{if(out)*out=nullptr;return E_NOINTERFACE;}
 HRESULT STDMETHODCALLTYPE Present1(UINT,UINT flags,const DXGI_PRESENT_PARAMETERS* changes) noexcept override{return present(flags,changes);}
 BOOL STDMETHODCALLTYPE IsTemporaryMonoSupported() noexcept override{return FALSE;}
 HRESULT STDMETHODCALLTYPE GetRestrictToOutput(IDXGIOutput** out) noexcept override{return unsupported(out);}
 HRESULT STDMETHODCALLTYPE SetBackgroundColor(const DXGI_RGBA*) noexcept override{return S_OK;}
 HRESULT STDMETHODCALLTYPE GetBackgroundColor(DXGI_RGBA* color) noexcept override{if(!color)return E_POINTER;*color={};return S_OK;}
 HRESULT STDMETHODCALLTYPE SetRotation(DXGI_MODE_ROTATION r) noexcept override{return r==DXGI_MODE_ROTATION_IDENTITY?S_OK:DXGI_ERROR_UNSUPPORTED;}
 HRESULT STDMETHODCALLTYPE GetRotation(DXGI_MODE_ROTATION* r) noexcept override{if(!r)return E_POINTER;*r=DXGI_MODE_ROTATION_IDENTITY;return S_OK;}
 HRESULT STDMETHODCALLTYPE SetSourceSize(UINT w,UINT h) noexcept override{return w==desc.Width&&h==desc.Height?S_OK:DXGI_ERROR_UNSUPPORTED;}
 HRESULT STDMETHODCALLTYPE GetSourceSize(UINT* w,UINT* h) noexcept override{if(!w||!h)return E_POINTER;*w=desc.Width;*h=desc.Height;return S_OK;}
 HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT n) noexcept override{return n==1?S_OK:DXGI_ERROR_INVALID_CALL;}
 HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT* n) noexcept override{if(!n)return E_POINTER;*n=1;return S_OK;}
 HANDLE STDMETHODCALLTYPE GetFrameLatencyWaitableObject() noexcept override{return ready.duplicate();}
 HRESULT STDMETHODCALLTYPE SetMatrixTransform(const DXGI_MATRIX_3X2_F* m) noexcept override{return m&&m->_11==1&&m->_22==1&&m->_12==0&&m->_21==0&&m->_31==0&&m->_32==0?S_OK:DXGI_ERROR_UNSUPPORTED;}
 HRESULT STDMETHODCALLTYPE GetMatrixTransform(DXGI_MATRIX_3X2_F* m) noexcept override{if(!m)return E_POINTER;*m={1,0,0,1,0,0};return S_OK;}
};
inline const GUID visualIID={0x4d93059d,0x097b,0x4651,{0x9a,0x60,0xf0,0xf2,0x51,0x16,0xe2,0xf3}};
inline const GUID targetIID={0xeacdd04c,0x117e,0x4e17,{0x88,0xf4,0xd1,0xb1,0x2b,0x0e,0x3d,0x89}};
struct Visual final:IDCompositionVisual{
 std::atomic<ULONG> refs{1};IUnknown* content=nullptr;~Visual(){if(content)content->Release();}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** o) noexcept override{if(!o)return E_POINTER;*o=nullptr;if(iid==__uuidof(IUnknown)||iid==visualIID){*o=this;AddRef();return S_OK;}return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef() noexcept override{return ++refs;}ULONG STDMETHODCALLTYPE Release() noexcept override{auto n=--refs;if(!n)delete this;return n;}
 HRESULT STDMETHODCALLTYPE SetOffsetX(float x) noexcept override{return x==0?S_OK:E_NOTIMPL;}HRESULT STDMETHODCALLTYPE SetOffsetX(IDCompositionAnimation*) noexcept override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE SetOffsetY(float y) noexcept override{return y==0?S_OK:E_NOTIMPL;}HRESULT STDMETHODCALLTYPE SetOffsetY(IDCompositionAnimation*) noexcept override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE SetTransform(const D2D_MATRIX_3X2_F&) noexcept override{return E_NOTIMPL;}HRESULT STDMETHODCALLTYPE SetTransform(IDCompositionTransform* t) noexcept override{return t?E_NOTIMPL:S_OK;}
 HRESULT STDMETHODCALLTYPE SetTransformParent(IDCompositionVisual* p) noexcept override{return p?E_NOTIMPL:S_OK;}HRESULT STDMETHODCALLTYPE SetEffect(IDCompositionEffect* e) noexcept override{return e?E_NOTIMPL:S_OK;}
 HRESULT STDMETHODCALLTYPE SetBitmapInterpolationMode(DCOMPOSITION_BITMAP_INTERPOLATION_MODE) noexcept override{return S_OK;}HRESULT STDMETHODCALLTYPE SetBorderMode(DCOMPOSITION_BORDER_MODE) noexcept override{return S_OK;}
 HRESULT STDMETHODCALLTYPE SetClip(const D2D_RECT_F&) noexcept override{return E_NOTIMPL;}HRESULT STDMETHODCALLTYPE SetClip(IDCompositionClip* c) noexcept override{return c?E_NOTIMPL:S_OK;}
 HRESULT STDMETHODCALLTYPE SetContent(IUnknown* c) noexcept override{if(c)c->AddRef();if(content)content->Release();content=c;return S_OK;}
 HRESULT STDMETHODCALLTYPE AddVisual(IDCompositionVisual*,BOOL,IDCompositionVisual*) noexcept override{return E_NOTIMPL;}HRESULT STDMETHODCALLTYPE RemoveVisual(IDCompositionVisual*) noexcept override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE RemoveAllVisuals() noexcept override{return S_OK;}HRESULT STDMETHODCALLTYPE SetCompositeMode(DCOMPOSITION_COMPOSITE_MODE m) noexcept override{return m==DCOMPOSITION_COMPOSITE_MODE_SOURCE_OVER||m==DCOMPOSITION_COMPOSITE_MODE_INHERIT?S_OK:E_NOTIMPL;}
};
struct Target final:IDCompositionTarget{
 std::atomic<ULONG> refs{1};IDCompositionVisual* root=nullptr;~Target(){if(root)root->Release();}
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** o) noexcept override{if(!o)return E_POINTER;*o=nullptr;if(iid==__uuidof(IUnknown)||iid==targetIID){*o=this;AddRef();return S_OK;}return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef() noexcept override{return ++refs;}ULONG STDMETHODCALLTYPE Release() noexcept override{auto n=--refs;if(!n)delete this;return n;}
 HRESULT STDMETHODCALLTYPE SetRoot(IDCompositionVisual* v) noexcept override{if(v)v->AddRef();if(root)root->Release();root=v;return S_OK;}
};
struct Device final:IDCompositionDevice{
 std::atomic<ULONG> refs{1};
 HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** o) noexcept override{if(!o)return E_POINTER;*o=nullptr;if(iid==__uuidof(IUnknown)||iid==__uuidof(IDCompositionDevice)){*o=this;AddRef();return S_OK;}return E_NOINTERFACE;}
 ULONG STDMETHODCALLTYPE AddRef() noexcept override{return ++refs;}ULONG STDMETHODCALLTYPE Release() noexcept override{auto n=--refs;if(!n)delete this;return n;}
 HRESULT STDMETHODCALLTYPE Commit() noexcept override{return S_OK;}HRESULT STDMETHODCALLTYPE WaitForCommitCompletion() noexcept override{return S_OK;}
 HRESULT STDMETHODCALLTYPE GetFrameStatistics(DCOMPOSITION_FRAME_STATISTICS*) noexcept override{return E_NOTIMPL;}
 HRESULT STDMETHODCALLTYPE CreateTargetForHwnd(HWND,BOOL,IDCompositionTarget** o) noexcept override{if(!o)return E_POINTER;*o=new(std::nothrow) Target;return *o?S_OK:E_OUTOFMEMORY;}
 HRESULT STDMETHODCALLTYPE CreateVisual(IDCompositionVisual** o) noexcept override{if(!o)return E_POINTER;*o=new(std::nothrow) Visual;return *o?S_OK:E_OUTOFMEMORY;}
 HRESULT STDMETHODCALLTYPE CreateSurface(UINT,UINT,DXGI_FORMAT,DXGI_ALPHA_MODE,IDCompositionSurface** o) noexcept override{return unsupported(o);}
 HRESULT STDMETHODCALLTYPE CreateVirtualSurface(UINT,UINT,DXGI_FORMAT,DXGI_ALPHA_MODE,IDCompositionVirtualSurface** o) noexcept override{return unsupported(o);}
 HRESULT STDMETHODCALLTYPE CreateSurfaceFromHandle(HANDLE,IUnknown** o) noexcept override{return unsupported(o);}HRESULT STDMETHODCALLTYPE CreateSurfaceFromHwnd(HWND,IUnknown** o) noexcept override{return unsupported(o);}
 HRESULT STDMETHODCALLTYPE CreateTranslateTransform(IDCompositionTranslateTransform** o) noexcept override{return unsupported(o);}HRESULT STDMETHODCALLTYPE CreateScaleTransform(IDCompositionScaleTransform** o) noexcept override{return unsupported(o);}
 HRESULT STDMETHODCALLTYPE CreateRotateTransform(IDCompositionRotateTransform** o) noexcept override{return unsupported(o);}HRESULT STDMETHODCALLTYPE CreateSkewTransform(IDCompositionSkewTransform** o) noexcept override{return unsupported(o);}
 HRESULT STDMETHODCALLTYPE CreateMatrixTransform(IDCompositionMatrixTransform** o) noexcept override{return unsupported(o);}HRESULT STDMETHODCALLTYPE CreateTransformGroup(IDCompositionTransform**,UINT,IDCompositionTransform** o) noexcept override{return unsupported(o);}
 HRESULT STDMETHODCALLTYPE CreateTranslateTransform3D(IDCompositionTranslateTransform3D** o) noexcept override{return unsupported(o);}HRESULT STDMETHODCALLTYPE CreateScaleTransform3D(IDCompositionScaleTransform3D** o) noexcept override{return unsupported(o);}
 HRESULT STDMETHODCALLTYPE CreateRotateTransform3D(IDCompositionRotateTransform3D** o) noexcept override{return unsupported(o);}HRESULT STDMETHODCALLTYPE CreateMatrixTransform3D(IDCompositionMatrixTransform3D** o) noexcept override{return unsupported(o);}
 HRESULT STDMETHODCALLTYPE CreateTransform3DGroup(IDCompositionTransform3D**,UINT,IDCompositionTransform3D** o) noexcept override{return unsupported(o);}HRESULT STDMETHODCALLTYPE CreateEffectGroup(IDCompositionEffectGroup** o) noexcept override{return unsupported(o);}
 HRESULT STDMETHODCALLTYPE CreateRectangleClip(IDCompositionRectangleClip** o) noexcept override{return unsupported(o);}HRESULT STDMETHODCALLTYPE CreateAnimation(IDCompositionAnimation** o) noexcept override{return unsupported(o);}
 HRESULT STDMETHODCALLTYPE CheckDeviceState(BOOL* o) noexcept override{if(!o)return E_POINTER;*o=TRUE;return S_OK;}
};
using EnumAdaptersFn=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory*,UINT,IDXGIAdapter**);
using EnumAdapters1Fn=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory1*,UINT,IDXGIAdapter1**);
using EnumOutputsFn=HRESULT(STDMETHODCALLTYPE*)(IDXGIAdapter*,UINT,IDXGIOutput**);
using VblankFn=HRESULT(STDMETHODCALLTYPE*)(IDXGIOutput*);
inline std::mutex pacingMutex;
inline std::map<std::pair<void**,UINT>,void*> pacingOriginals;
inline void installPacingHook(void** vt,UINT slot,void* fn){std::lock_guard<std::mutex> lock(pacingMutex);if(vt[slot]==fn)return;DWORD old;if(VirtualProtect(vt+slot,sizeof(void*),PAGE_READWRITE,&old)){pacingOriginals[{vt,slot}]=vt[slot];vt[slot]=fn;DWORD unused;VirtualProtect(vt+slot,sizeof(void*),old,&unused);}}
inline void* pacingOriginal(void* object,UINT slot){std::lock_guard<std::mutex> lock(pacingMutex);auto i=pacingOriginals.find({*(void***)object,slot});return i==pacingOriginals.end()?nullptr:i->second;}
inline HRESULT STDMETHODCALLTYPE pacedVblank(IDXGIOutput* output){auto fn=(VblankFn)pacingOriginal(output,10);if(!fn)return E_UNEXPECTED;auto start=std::chrono::steady_clock::now();HRESULT hr=fn(output);auto elapsed=std::chrono::steady_clock::now()-start;if(elapsed<std::chrono::milliseconds(2))std::this_thread::sleep_until(start+std::chrono::microseconds(16667));return hr==E_NOTIMPL?S_OK:hr;}
inline HRESULT STDMETHODCALLTYPE enumOutputs(IDXGIAdapter* adapter,UINT n,IDXGIOutput** output){auto fn=(EnumOutputsFn)pacingOriginal(adapter,7);if(!fn)return E_UNEXPECTED;HRESULT hr=fn(adapter,n,output);if(SUCCEEDED(hr)&&output&&*output)installPacingHook(*(void***)*output,10,(void*)&pacedVblank);return hr;}
inline HRESULT STDMETHODCALLTYPE enumAdapters(IDXGIFactory* factory,UINT n,IDXGIAdapter** adapter){auto fn=(EnumAdaptersFn)pacingOriginal(factory,7);if(!fn)return E_UNEXPECTED;HRESULT hr=fn(factory,n,adapter);if(SUCCEEDED(hr)&&adapter&&*adapter)installPacingHook(*(void***)*adapter,7,(void*)&enumOutputs);return hr;}
inline HRESULT STDMETHODCALLTYPE enumAdapters1(IDXGIFactory1* factory,UINT n,IDXGIAdapter1** adapter){auto fn=(EnumAdapters1Fn)pacingOriginal(factory,12);if(!fn)return E_UNEXPECTED;HRESULT hr=fn(factory,n,adapter);if(SUCCEEDED(hr)&&adapter&&*adapter)installPacingHook(*(void***)*adapter,7,(void*)&enumOutputs);return hr;}
using FactoryFn=HRESULT(WINAPI*)(UINT,REFIID,void**);
using LegacyFactoryFn=HRESULT(WINAPI*)(REFIID,void**);
using CompositionFn=HRESULT(WINAPI*)(IDXGIDevice*,REFIID,void**);
using ChainFn=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*,IUnknown*,const DXGI_SWAP_CHAIN_DESC1*,IDXGIOutput*,IDXGISwapChain1**);
inline FactoryFn factoryFn=nullptr;inline CompositionFn compositionFn=nullptr;
inline LegacyFactoryFn factory0Fn=nullptr,factory1Fn=nullptr;
inline std::mutex hooksMutex;inline std::map<void**,ChainFn> chainFns;
inline HRESULT STDMETHODCALLTYPE createChain(IDXGIFactory2* factory,IUnknown* d,const DXGI_SWAP_CHAIN_DESC1* desc,IDXGIOutput* output,IDXGISwapChain1** result){
 if(!enabled){ChainFn fn;{std::lock_guard<std::mutex> lock(hooksMutex);fn=chainFns[*(void***)factory];}return fn(factory,d,desc,output,result);}
 if(!result||!d||!desc)return E_INVALIDARG;*result=nullptr;ID3D11Device* device=nullptr;HRESULT hr=d->QueryInterface(__uuidof(ID3D11Device),(void**)&device);if(FAILED(hr))return hr;
 try{auto releaseDevice=[](ID3D11Device* p){if(p)p->Release();};std::unique_ptr<ID3D11Device,decltype(releaseDevice)> held(device,releaseDevice);auto releaseChain=[](SwapChain* p){if(p)p->Release();};std::unique_ptr<SwapChain,decltype(releaseChain)> chain(new SwapChain(factory,device,*desc),releaseChain);hr=chain->create();if(FAILED(hr))return hr;*result=chain.release();return S_OK;}catch(...){return E_OUTOFMEMORY;}
}
inline void instrumentFactory(IUnknown* object){
 IDXGIFactory* base=nullptr;if(SUCCEEDED(object->QueryInterface(__uuidof(IDXGIFactory),(void**)&base))){installPacingHook(*(void***)base,7,(void*)&enumAdapters);base->Release();}
 IDXGIFactory1* newer=nullptr;if(SUCCEEDED(object->QueryInterface(__uuidof(IDXGIFactory1),(void**)&newer))){installPacingHook(*(void***)newer,12,(void*)&enumAdapters1);newer->Release();}
 IDXGIFactory2* f=nullptr;
 if(SUCCEEDED(object->QueryInterface(__uuidof(IDXGIFactory2),(void**)&f))){auto** vt=*(void***)f;installPacingHook(vt,7,(void*)&enumAdapters);installPacingHook(vt,12,(void*)&enumAdapters1);std::lock_guard<std::mutex> lock(hooksMutex);if(vt[24]!=(void*)&createChain){DWORD old;if(VirtualProtect(vt+24,sizeof(void*),PAGE_READWRITE,&old)){chainFns[vt]=(ChainFn)vt[24];vt[24]=(void*)&createChain;DWORD unused;VirtualProtect(vt+24,sizeof(void*),old,&unused);}}f->Release();}
}
inline HRESULT WINAPI createFactory(UINT flags,REFIID iid,void** result){HRESULT hr=factoryFn(flags,iid,result);if(SUCCEEDED(hr)&&result&&*result)instrumentFactory((IUnknown*)*result);return hr;}
// Older software-rendered editors also schedule paints from DXGI vblank.
// Hook all factory generations, not only the DirectComposition entry point.
inline HRESULT WINAPI createFactory0(REFIID iid,void** result){HRESULT hr=factory0Fn(iid,result);if(SUCCEEDED(hr)&&result&&*result)instrumentFactory((IUnknown*)*result);return hr;}
inline HRESULT WINAPI createFactory1(REFIID iid,void** result){HRESULT hr=factory1Fn(iid,result);if(SUCCEEDED(hr)&&result&&*result)instrumentFactory((IUnknown*)*result);return hr;}
inline HRESULT WINAPI createComposition(IDXGIDevice* d,REFIID iid,void** result){if(!enabled)return compositionFn(d,iid,result);if(!result)return E_POINTER;*result=nullptr;auto* dev=new(std::nothrow) Device;if(!dev)return E_OUTOFMEMORY;HRESULT hr=dev->QueryInterface(iid,result);dev->Release();return hr;}
inline int patch(HMODULE module){int count=0;auto* base=(uint8_t*)module;auto* dos=(IMAGE_DOS_HEADER*)base;auto* nt=(IMAGE_NT_HEADERS*)(base+dos->e_lfanew);auto dir=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];if(!dir.VirtualAddress)return 0;
 for(auto* imp=(IMAGE_IMPORT_DESCRIPTOR*)(base+dir.VirtualAddress);imp->Name;++imp){if(!imp->OriginalFirstThunk)continue;auto* names=(IMAGE_THUNK_DATA*)(base+imp->OriginalFirstThunk);auto* addresses=(IMAGE_THUNK_DATA*)(base+imp->FirstThunk);
  for(size_t i=0;addresses[i].u1.Function;++i){if(IMAGE_SNAP_BY_ORDINAL(names[i].u1.Ordinal))continue;auto* name=(const char*)((IMAGE_IMPORT_BY_NAME*)(base+names[i].u1.AddressOfData))->Name;uintptr_t replacement=0;
   if(!std::strcmp(name,"CreateDXGIFactory2")){factoryFn=(FactoryFn)addresses[i].u1.Function;replacement=(uintptr_t)&createFactory;}
   if(!std::strcmp(name,"CreateDXGIFactory")){factory0Fn=(LegacyFactoryFn)addresses[i].u1.Function;replacement=(uintptr_t)&createFactory0;}
   if(!std::strcmp(name,"CreateDXGIFactory1")){factory1Fn=(LegacyFactoryFn)addresses[i].u1.Function;replacement=(uintptr_t)&createFactory1;}
   if(!std::strcmp(name,"DCompositionCreateDevice")){compositionFn=(CompositionFn)addresses[i].u1.Function;replacement=(uintptr_t)&createComposition;}
   if(replacement){DWORD old;if(VirtualProtect(&addresses[i].u1.Function,sizeof(uintptr_t),PAGE_READWRITE,&old)){addresses[i].u1.Function=replacement;DWORD unused;VirtualProtect(&addresses[i].u1.Function,sizeof(uintptr_t),old,&unused);++count;}}
  }
 }return count;
}
}
#endif
