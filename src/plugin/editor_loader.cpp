#include "editor.hpp"
#include "config.hpp"
#include <dlfcn.h>
Steinberg::IPlugView* makeBridgeEditor(Steinberg::Vst::EditController* controller){
 // All wrappers use the same image. Keep it loaded for the host process's
 // lifetime because AppKit caches Objective-C classes after an editor closes.
 static void* library=dlopen((bridgeHome()+"/engine/libBridgeLabUI.dylib").c_str(),RTLD_NOW|RTLD_LOCAL);
 using Factory=Steinberg::IPlugView*(*)(Steinberg::Vst::EditController*);
 auto create=library?reinterpret_cast<Factory>(dlsym(library,"BridgeLabMakeEditor")):nullptr;
 return create?create(controller):nullptr;
}
