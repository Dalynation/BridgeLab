#pragma once
#include "vst2_abi.hpp"
#include "wire.hpp"
#include <vector>
#include <cstring>
#include <algorithm>
struct StateHeader {uint32_t magic=0x53543031,id=0,program=0,kind=0,count=0;};
inline std::vector<uint8_t> captureState(Effect* e){
 StateHeader h;h.id=uint32_t(e->id);h.program=uint32_t(e->dispatch(e,3,0,0,nullptr,0));
 std::vector<uint8_t> result(sizeof(h));void* chunk=nullptr;
 intptr_t size=(e->flags&(1<<5))?e->dispatch(e,23,0,0,&chunk,0):0;
 if(size>0&&uint64_t(size)<=maxStateBytes-sizeof(h)&&chunk){h.kind=1;h.count=uint32_t(size);result.resize(sizeof(h)+size);std::memcpy(result.data()+sizeof(h),chunk,size);}
 else {if(e->parameters<0||e->parameters>100000||(!e->getParameter&&e->parameters))return {};h.kind=2;h.count=e->parameters;result.resize(sizeof(h)+h.count*sizeof(float));for(uint32_t i=0;i<h.count;i++){float value=e->getParameter(e,i);std::memcpy(result.data()+sizeof(h)+i*4,&value,4);}}
 std::memcpy(result.data(),&h,sizeof(h));return result;
}
inline bool restoreState(Effect* e,const std::vector<uint8_t>& bytes){
 if(bytes.size()<sizeof(StateHeader))return false;StateHeader h;std::memcpy(&h,bytes.data(),sizeof(h));
 if(h.magic!=0x53543031||h.id!=uint32_t(e->id)||h.program>=uint32_t(std::max(1,e->programs)))return false;
 if(h.kind==1){if(h.count!=bytes.size()-sizeof(h))return false;e->dispatch(e,2,0,h.program,nullptr,0);e->dispatch(e,24,0,h.count,const_cast<uint8_t*>(bytes.data()+sizeof(h)),0);}
 else if(h.kind==2){if(h.count!=uint32_t(e->parameters)||bytes.size()!=sizeof(h)+h.count*4||(!e->setParameter&&h.count))return false;}
 else return false;
 // Banks may not include the selected program. Restore selection before parameters.
 if(h.kind==2)e->dispatch(e,2,0,h.program,nullptr,0);
 if(h.kind==2)for(uint32_t i=0;i<h.count;i++){float value;std::memcpy(&value,bytes.data()+sizeof(h)+i*4,4);e->setParameter(e,i,value);}
 return true;
}
