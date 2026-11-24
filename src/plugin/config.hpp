#pragma once
#include <dlfcn.h>
#include <cstdlib>
#include <string>
#include <fstream>
#include <array>
#include <vector>
#include <filesystem>
#include "pluginterfaces/base/funknown.h"
static inline std::string bridgeHome(){if(const char* overrideRoot=std::getenv("BRIDGELAB_HOME"))return overrideRoot;const char* home=std::getenv("HOME");return std::string(home?home:"")+"/Library/Application Support/BridgeLab";}
__attribute__((noinline)) static const void* bridgeImageAnchor(){return (const void*)&bridgeImageAnchor;}
static inline const std::array<std::string,5>& bridgeConfig(){
 static const auto value=[] {std::array<std::string,5> result{};Dl_info info{};if(dladdr(bridgeImageAnchor(),&info)&&info.dli_fname){auto resources=std::filesystem::path(info.dli_fname).parent_path().parent_path()/"Resources";std::ifstream file(resources/"bridgelab.conf");for(auto& line:result)std::getline(file,line);}return result;}();return value;
}
static inline Steinberg::FUID bridgeClassID(int index){auto text=bridgeConfig()[index];if(text.size()!=32)text=index==0?"BA34C810D6A442A09FDD90A151C35719":"615DA39B90C546CFA774F663732AEA90";std::array<uint32_t,4> words{};for(int i=0;i<4;i++)words[i]=uint32_t(std::stoul(text.substr(i*8,8),nullptr,16));return Steinberg::FUID(words[0],words[1],words[2],words[3]);}
static inline const char* bridgeDisplayName(){static std::string name=bridgeConfig()[2].empty()?"BridgeLab Instrument":bridgeConfig()[2];return name.c_str();}
static inline std::string bridgePluginPath(){return bridgeConfig()[3];}
static inline std::string bridgeRuntimeProfile(int bits){auto name=bridgeConfig()[4];if(name=="crossover-shared"||name=="crossover-x86"||name=="crossover-x64")return name;return bits==32?"crossover-x86":"crossover-x64";}

// Cached import metadata lets host scans enumerate presets without starting Wine.
static inline const std::vector<std::string>& bridgePrograms(){
 static const auto names=[] {
  std::vector<std::string> result;Dl_info info{};
  if(dladdr(bridgeImageAnchor(),&info)&&info.dli_fname){
   auto resources=std::filesystem::path(info.dli_fname).parent_path().parent_path()/"Resources";
   std::ifstream file(resources/"programs.txt");std::string line;
   while(result.size()<16384&&std::getline(file,line)){
    if(line.empty()||line.size()>2040||line.size()%2){result.clear();break;}
    std::string name;bool valid=true;
    auto digit=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
    for(size_t i=0;i<line.size();i+=2){int a=digit(line[i]),b=digit(line[i+1]);if(a<0||b<0||!(a||b)){valid=false;break;}name.push_back(char(a*16+b));}
    if(!valid){result.clear();break;}result.push_back(name);
   }
  }
  return result;
 }();return names;
}
static inline uint32_t bridgeProgramSteps(){return bridgePrograms().empty()?511:uint32_t(bridgePrograms().size()-1);}
