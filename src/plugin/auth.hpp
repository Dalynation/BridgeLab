#pragma once
#include <atomic>
#include <thread>
#include <chrono>
#include <fstream>
#include <iterator>
#include "config.hpp"

// Implemented in Objective-C++ to validate the installed hub bundle and run its
// Keychain/HTTPS helper. It is never called from process().
bool zoneSixOnlineCheck(const std::atomic<bool>* cancelled=nullptr);
namespace Steinberg { class IPlugView; }
Steinberg::IPlugView* makeZoneSixLockedView();
inline std::string zoneSixGeneration(){
 const char* home=std::getenv("HOME");
 std::ifstream file(std::string(home?home:"")+"/Library/Application Support/ZoneSixStudioHub/session-generation");
 std::string value;std::getline(file,value);return value;
}
class ZoneSixAuthorization {
 std::atomic<bool> cancelled{false},allowed{false};std::thread monitor;
public:
 ~ZoneSixAuthorization(){stop();}
 bool permits()const{return allowed.load(std::memory_order_relaxed);}
 void stop(){cancelled=true;allowed=false;if(monitor.joinable())monitor.join();}
 bool start(){
  stop();cancelled=false;auto generation=zoneSixGeneration();
  if(!zoneSixOnlineCheck(&cancelled)||generation!=zoneSixGeneration())return false;
  allowed=true;
  monitor=std::thread([this,generation]{
   auto renewal=std::chrono::steady_clock::now()+std::chrono::seconds(30);
   while(!cancelled){
    // Changes can only revoke a lease. A fresh server check is required to grant it.
    if(zoneSixGeneration()!=generation){allowed=false;return;}
    if(std::chrono::steady_clock::now()>=renewal){
     if(!zoneSixOnlineCheck(&cancelled)||zoneSixGeneration()!=generation){allowed=false;return;}
     renewal=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
   }
  });return true;
 }
};
