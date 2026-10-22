#!/usr/bin/env bash
# Compile the actual allocator with bounded syscall fault fixtures; no device opens.
set -Eeuo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d);trap 'rm -rf -- "$work"' EXIT
cat > "$work/test.cpp" <<'CPP'
#include <array>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cerrno>
#include <linux/f2fs.h>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>
static std::vector<std::string> calls;
static int failure=0;
static void need(bool ok,const std::string &why){if(!ok)throw std::runtime_error(why);}
static int fixture_ioctl(int fd,unsigned long request,uint32_t *pin) {
 assert(fd==42 && request==F2FS_IOC_SET_PIN_FILE && *pin==1);calls.push_back("pin");return failure==1?-1:0;
}
static int fixture_fallocate(int fd,int mode,off_t start,off_t size) {
 assert(fd==42 && mode==0 && start==0 && size==1048579);calls.push_back("allocate");return failure==2?-1:0;
}
static ssize_t fixture_write(int fd,const void *data,size_t size) {
 assert(fd==42 && size<=1048576 && size>0);const auto bytes=static_cast<const char*>(data);
 assert(std::all_of(bytes,bytes+size,[](char v){return v==0;}));calls.push_back("zero");
 if(failure==3)return 0;
 if(failure==4){failure=0;errno=EINTR;return -1;}
 return size;
}
static int fixture_fsync(int fd){assert(fd==42);calls.push_back("sync");return failure==5?-1:0;}
#define ioctl fixture_ioctl
#define fallocate fixture_fallocate
#define write fixture_write
#define fsync fixture_fsync
CPP
awk '/^static void allocate_swap\(/{copy=1} copy&&/^static void apply_locked\(/{exit} copy{print}' "$root/src/backend.cpp" >> "$work/test.cpp"
cat >> "$work/test.cpp" <<'CPP'
int main() {
 allocate_swap(42,1048579,true);assert((calls==std::vector<std::string>{"pin","allocate","zero","zero","sync"}));
 calls.clear();allocate_swap(42,1048579,false);assert((calls==std::vector<std::string>{"zero","zero","sync"}));
 for(failure=1;failure<=5;++failure) {
  const int injected=failure;calls.clear();bool refused=false;try{allocate_swap(42,1048579,true);}catch(const std::exception&){refused=true;}
  if(injected==4){assert(!refused && calls.size()==6);failure=injected;}else assert(refused);
  if(injected==1)assert((calls==std::vector<std::string>{"pin"}));
  if(injected==2)assert((calls==std::vector<std::string>{"pin","allocate"}));
 }
}
CPP
"${CXX:-c++}" -std=c++20 -Wall -Wextra -Werror -O1 -g "$work/test.cpp" -o "$work/test"
"$work/test"
echo 'Actual swap allocator: pin before preallocation, full zeroing, EINTR, pin/allocation/write/sync failure refusal passed.'
