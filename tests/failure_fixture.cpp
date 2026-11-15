#include "../src/vst2_abi.hpp"
static intptr_t BCALL dispatch(Effect*,int32_t,int32_t,intptr_t,void*,float){return 0;}
static void BCALL silence(Effect*,float**,float** out,int32_t n){for(int i=0;i<n;i++)out[0][i]=out[1][i]=0;}
extern "C" __declspec(dllexport) Effect* VSTPluginMain(Callback){static Effect e{};
#ifdef INVALID_EFFECT
 e.magic=0;
#else
 e.magic=0x56737450;
#endif
 e.dispatch=dispatch;e.outputs=2;e.programs=1;e.replacing=silence;return &e;}
