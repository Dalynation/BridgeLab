#include "../src/vst2_abi.hpp"
#include <cmath>
static bool active=false;static double phase=0;
static intptr_t BCALL dispatch(Effect*,int32_t op,int32_t,intptr_t,void* ptr,float){if(op==25){auto* es=(Events*)ptr;active=es->items[0]->data[0]==0x90;}return 0;}
static void BCALL process(Effect*,float**,float** out,int32_t n){for(int i=0;i<n;i++){float v=active?float(std::sin(phase)*0.1):0;phase+=2*3.141592653589793*261.625565/48000;out[0][i]=out[1][i]=v;}}
extern "C" Effect* VSTPluginMain(Callback cb){static Effect e{};e.magic=0x56737450;e.dispatch=dispatch;e.outputs=2;e.programs=1;e.replacing=process;return cb(nullptr,1,0,0,nullptr,0)==2400?&e:nullptr;}
