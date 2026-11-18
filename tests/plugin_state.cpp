#include "../src/plugin_state.hpp"
#include <iostream>
static intptr_t selected=1,lastSet=-1;static unsigned char chunk[4]={1,2,3,4};
static intptr_t BCALL dispatch(Effect*,int32_t op,int32_t,intptr_t value,void* ptr,float){if(op==3)return selected;if(op==2){lastSet=value;return 0;}if(op==23){*static_cast<void**>(ptr)=chunk;return 4;}return 0;}
int main(){Effect effect{};effect.id=123;effect.programs=1;effect.flags=1<<5;effect.dispatch=dispatch;auto state=captureState(&effect);StateHeader header;std::memcpy(&header,state.data(),sizeof(header));if(header.program!=0||!restoreState(&effect,state)||lastSet!=0)return 1;
 header.program=1;std::memcpy(state.data(),&header,sizeof(header));if(!restoreState(&effect,state)||lastSet!=0)return 1;
 header.program=500;std::memcpy(state.data(),&header,sizeof(header));if(restoreState(&effect,state))return 1;
 selected=-1;state=captureState(&effect);std::memcpy(&header,state.data(),sizeof(header));if(header.program!=0||!restoreState(&effect,state))return 1;
 std::cout<<"Nonstandard current-program reports: normalized capture, legacy state recall, bounded selection and invalid-header rejection: PASS\n";return 0;}
