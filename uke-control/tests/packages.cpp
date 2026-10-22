// SPDX-License-Identifier: MIT
#define UKE_PACKAGES_TEST
#include "../src/packages.cpp"
#include <cassert>
#include <fnmatch.h>
using namespace uke::packages;
static Options options(std::initializer_list<const char*> input) {
    std::vector<char*> args;for(auto arg:input)args.push_back(const_cast<char*>(arg));
    return parse(int(args.size()),args.data());
}
int main() {
    assert(options({"brunch","stable"}).selected=="stable");assert(options({"branch","test","--dry-run"}).dry);
    assert(options({"brunch","unstable","--yes"}).yes);assert(options({"brunch","sync"}).sync);
    assert(options({"update"}).update);assert(options({"update","--check"}).dry);
    for(auto args:{std::initializer_list<const char*>{"brunch"},{"brunch","bad"},{"brunch","stable","--nodeps"},{"brunch","sync","--yes","--yes"},{"brunch","status","--yes"},{"update","--allowerasing"}}) {
        bool rejected=false;try{options(args);}catch(const std::exception&){rejected=true;}assert(rejected);
    }
    assert(project("unstable")=="uke-linux-test"&&project("stable")=="uke-linux");
    assert(private_package("uke-local-touch-data"));
    assert(!private_package("uke-local-sensor-data")&&!private_package("uke-local-touch-data-extra"));
    assert(managed_repo(prefix+"uke-linux-test:custom:experiment"));assert(!managed_repo(prefix+"unrelated"));
    assert(infer_channel({prefix+"uke-linux-test"})=="test");
    assert(infer_channel({"uke-linux"})=="stable");
    assert(infer_channel({}).empty());
    assert(infer_channel({prefix+"uke-linux",prefix+"uke-linux-test"}).empty());
    assert(infer_channel({prefix+"uke-linux-test:custom:experiment"}).empty());
    assert(infer_channel({"uke-linux",prefix+"uke-linux"}).empty());
    assert(managed_source("legacy-candidate",{"https://download.copr.fedorainfracloud.org/results/mcc45tr/uke-linux-test/custom/"}));
    for(const auto& arch:{"aarch64","noarch","x86_64"}) {
        const auto stable=kernel+"-core-7.2.11-1.fc46."+arch;
        const auto rc=kernel+"-core-7.4~rc7-1.fc46."+arch;
        assert(fnmatch(prerelease_excludes[0].c_str(),stable.c_str(),0)==FNM_NOMATCH);
        assert(fnmatch(prerelease_excludes[0].c_str(),rc.c_str(),0)==0);
    }
    bool denied=false;try{atomic_write("/definitely-missing-parent/record","test");}catch(const std::exception&){denied=true;}assert(denied);
    if(geteuid()==0) {
        char dir[]="/tmp/uke-channel-state-XXXXXX";assert(mkdtemp(dir));const fs::path root=dir;
        auto rejected=[](auto&& fn){bool caught=false;try{fn();}catch(const std::exception&){caught=true;}assert(caught);};
        const auto previous_mask=umask(0077);
        atomic_write(root/"channel","test\n");assert(saved(root/"channel")=="test");
        struct stat mode{};assert(stat((root/"channel").c_str(),&mode)==0&&(mode.st_mode&0777)==0644);
        umask(previous_mask);
        atomic_write(root/"channel","stable\n");assert(saved(root/"channel")=="stable");
        fs::create_symlink(root/"channel",root/"symlink");rejected([&]{atomic_write(root/"symlink","bad");});rejected([&]{saved(root/"symlink");});
        fs::create_hard_link(root/"channel",root/"hardlink");rejected([&]{atomic_write(root/"channel","bad");});rejected([&]{saved(root/"channel");});fs::remove(root/"hardlink");
        atomic_write(root/"pending","unstable\n",0600);assert(saved(root/"pending")=="unstable");
        std::ofstream(root/"channel.new")<<"interrupted";rejected([&]{atomic_write(root/"channel","bad");});
        std::ofstream(root/"invalid")<<"stable\nunstable\n";rejected([&]{saved(root/"invalid");});
        std::ofstream(root/"empty");rejected([&]{saved(root/"empty");});
        std::ofstream(root/"oversized")<<std::string(65,'x');rejected([&]{saved(root/"oversized");});
        assert(mkfifo((root/"fifo").c_str(),0600)==0);rejected([&]{saved(root/"fifo");});
        assert(saved(root/"absent").empty());
        {
            libdnf5::Base base;base.get_config().get_installroot_option().set((root/"repo-root").string());
            base.get_config().get_cachedir_option().set((root/"cache").string());base.get_config().get_plugins_option().set(false);base.setup();
            auto stable=base.get_repo_sack()->create_repo(prefix+"uke-linux"),test=base.get_repo_sack()->create_repo(prefix+"uke-linux-test");
            stable->get_config().get_enabled_option().set(false);test->get_config().get_enabled_option().set(true);
            assert(infer_channel(enabled_managed_repos(base))=="test");
            stable->get_config().get_enabled_option().set(true);assert(infer_channel(enabled_managed_repos(base)).empty());
            stable->get_config().get_enabled_option().set(false);test->get_config().get_enabled_option().set(false);
            auto alias=base.get_repo_sack()->create_repo("legacy-candidate");
            alias->get_config().get_baseurl_option().set(std::vector<std::string>{"https://download.copr.fedorainfracloud.org/results/mcc45tr/uke-linux-test/custom/"});
            alias->get_config().get_enabled_option().set(true);assert(infer_channel(enabled_managed_repos(base)).empty());
        }
        assert(read_file(root/"channel")=="stable\n");fs::remove_all(root);
    }
}
