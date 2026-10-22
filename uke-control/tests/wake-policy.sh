#!/usr/bin/env bash
# Actual mutation/rollback functions with synthetic state; no sysfs/device writes.
set -Eeuo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -r -- "$work"' EXIT
awk '/^void wake_set\(/{copy=1} /^uint64_t swap_mib\(/{copy=0} copy' "$root/src/backend.cpp" > "$work/policy.inc"
cat > "$work/test.cpp" <<'CPP'
#include "wake.hpp"
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs=std::filesystem;
static fs::path state;
static bool foreign=false, disk_failure=false;
static int fail_after=-1;
static std::vector<uke::wake::Source> current;
static std::vector<std::string> writes;
static void need(bool value,const std::string& reason){if(!value)throw std::runtime_error(reason);}
static void root(){}
struct Lock{Lock(){}};
static std::string read(const std::string& path,size_t){std::ifstream in(path);return {std::istreambuf_iterator<char>(in),{}};}
static void atomic_file(const fs::path& path,const std::string& value){if(disk_failure)throw std::runtime_error("disk failure");std::ofstream(path)<<value;}
namespace uke::wake {
bool applicable(const fs::path&){return !foreign;}
std::vector<Source> sources(const fs::path&){return current;}
bool recovery_key(const std::vector<Source>& list){return std::any_of(list.begin(),list.end(),[](const auto& s){return s.enabled&&!s.path.empty()&&(s.name=="power-key"||s.name=="volume-up"||s.name=="volume-down");});}
}
using namespace uke;
static void wake_write(const wake::Source& source,bool enabled){
 need(!source.path.empty(),"unsupported source");
 if(fail_after==0){fail_after=-1;throw std::runtime_error("write failure");}
 if(fail_after>0)--fail_after;
 writes.push_back(source.name+ (enabled?" on":" off"));
 for(auto& item:current)if(item.name==source.name)item.enabled=enabled;
}
#include "policy.inc"
template<class Call> void refuses(Call call){bool refused=false;try{call();}catch(const std::runtime_error&){refused=true;}assert(refused);}
int main(int argc,char** argv){
 assert(argc==2);state=argv[1];fs::create_directories(state);
 current={{"power-key","","/power",true},{"volume-up","","/volume",true},{"bluetooth-uart","","/uart",true},{"double-tap","unsupported",{},false}};
 wake_apply();assert(!current[2].enabled);assert(writes.back()=="bluetooth-uart off");
 writes.clear();foreign=true;refuses([]{wake_set("power-key",false);});assert(writes.empty());foreign=false;
 refuses([]{wake_set("unknown",true);});refuses([]{wake_set("double-tap",true);});
 wake_set("power-key",false);assert(!current[0].enabled);assert(read((state/"wake-power-key").string(),32)=="disabled\n");
 writes.clear();refuses([]{wake_set("volume-up",false);});assert(writes.empty());assert(current[1].enabled);
 disk_failure=true;refuses([]{wake_set("power-key",true);});assert(!current[0].enabled);disk_failure=false;
 atomic_file(state/"wake-power-key","enabled\n");atomic_file(state/"wake-volume-up","invalid\n");
 writes.clear();refuses([]{wake_apply();});assert(writes.empty());fs::remove(state/"wake-volume-up");
 current[2].enabled=true;const auto before=current;fail_after=1;refuses([]{wake_apply();});
 for(size_t i=0;i<before.size();++i)assert(current[i].enabled==before[i].enabled);
 atomic_file(state/"wake-double-tap","enabled\n");writes.clear();refuses([]{wake_apply();});assert(writes.empty());fs::remove(state/"wake-double-tap");
 atomic_file(state/"wake-volume-up","disabled\n");wake_apply();
 assert(current[0].enabled&&!current[1].enabled&&!current[2].enabled);
 atomic_file(state/"wake-power-key","disabled\n");writes.clear();refuses([]{wake_apply();});assert(writes.empty());
}
CPP
c++ -std=c++20 -O2 -Wall -Wextra -Werror -I "$root/src" "$work/test.cpp" -o "$work/test"
"$work/test" "$work/state"
printf '%s\n' 'Actual wake policy identity, persistence, recovery-key, enable ordering and rollback fixtures passed.'
