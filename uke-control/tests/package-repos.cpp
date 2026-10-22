// SPDX-License-Identifier: MIT
// Host-only integration against signed local fixture repositories and a disposable RPM root.
#define UKE_PACKAGES_TEST
#include "../src/packages.cpp"
#include <cassert>
#include <functional>
using namespace uke::packages;
static void session(const fs::path& root,const fs::path& repo,const std::string& selected,const std::function<void(libdnf5::Base&)>& fn,bool with_fedora=false) {
    libdnf5::Base base;auto& c=base.get_config();
    c.get_installroot_option().set(root.string());c.get_use_host_config_option().set(false);c.get_plugins_option().set(false);
    c.get_config_file_path_option().set((root/"empty.conf").string());
    c.get_installonly_limit_option().set(0);c.get_best_option().set(true);c.get_pkg_gpgcheck_option().set(true);
    c.get_allow_vendor_change_option().set(false);
    c.get_reposdir_option().set(std::vector<std::string>{});base.get_vars()->set("releasever","46");base.setup();
    auto r=base.get_repo_sack()->create_repo(prefix+project(selected));r->get_config().get_baseurl_option().set(std::vector<std::string>{"file://"+repo.string()});r->get_config().get_pkg_gpgcheck_option().set(true);r->get_config().get_metadata_expire_option().set(0);
    if(with_fedora) {
        auto original=base.get_repo_sack()->create_repo("rawhide");original->get_config().get_baseurl_option().set(std::vector<std::string>{"file://"+(repo.parent_path()/"fedora").string()});original->get_config().get_pkg_gpgcheck_option().set(true);
    }
    if(selected=="test")r->get_config().get_excludepkgs_option().set(std::vector<std::string>(prerelease_excludes.begin(),prerelease_excludes.end()));
    base.lock_system_repo(libdnf5::utils::LockAccess::WRITE,libdnf5::utils::LockBlocking::NON_BLOCKING);
    base.get_repo_sack()->load_repos();fn(base);
}
int main(int argc,char** argv) {
    assert(argc==5);const fs::path root=argv[1],stable=argv[2],test=argv[3];const std::string phase=argv[4];
    const auto selected=phase=="test-preference"?"test":phase=="upgrade"||phase=="rc"||phase=="fallback"?"unstable":"stable";
    const fs::path repo=std::string(selected)=="stable"?stable:test;
    session(root,repo,selected,[&](libdnf5::Base& base) {
        const std::set<std::string> ids{prefix+"uke-linux",prefix+"uke-linux-test"};
        if(phase=="rename-upper-bound"||phase=="ambiguous-rename"||phase=="provides-only"||phase=="missing-fedora-fastfetch"||phase=="private-obsolete") {
            bool refused=false;try{auto p=plan(base,selected,ids);resolve(base,p,selected,false);}catch(const std::exception& e){std::cout<<e.what()<<'\n';refused=true;}assert(refused);return;
        }
        if(phase=="missing"||phase=="shared-uname"||phase=="old-manager") {
            bool refused=false;try{plan(base,selected,ids);}catch(const std::exception& e){std::cout<<e.what()<<'\n';refused=true;}assert(refused);return;
        }
        auto p=plan(base,selected,ids);
        if(phase=="isolated-kvm") {assert(p.targets.empty()&&p.kernel_nevra.empty());return;}
        if(phase=="fedora-fastfetch")assert(p.targets.at("fastfetch.noarch").get_repo_id()=="rawhide");
        if(phase=="rename"||phase=="rename-update-vendor")assert(p.renames.size()==1);
        if(phase=="rc")assert(p.kernel_nevra.find("rc7")!=std::string::npos&&!p.fallback);
        if(phase=="fallback")assert(p.fallback&&p.kernel_nevra.find("~rc")==std::string::npos);
        if(phase=="downgrade"||phase=="downgrade-update"||phase=="test-preference")assert(p.kernel_nevra.find("~rc")==std::string::npos);
        auto tx=resolve(base,p,selected,phase=="upgrade"||phase=="downgrade-update"||phase=="rename-update-vendor");
        assert(!base.get_config().get_allow_vendor_change_option().get_value());
        unsigned downgrades=0,reinstalls=0;
        for(const auto& item:tx.get_transaction_packages()) {
            assert(!private_package(item.get_package().get_name()));
            if(phase=="rename-update-vendor")assert(item.get_package().get_name()!="outside-vendor");
            std::cout<<libdnf5::transaction::transaction_item_action_to_string(item.get_action())<<' '<<item.get_package().get_nevra()<<'\n';
            if(item.get_action()==Action::DOWNGRADE)downgrades++;
            if(item.get_action()==Action::REINSTALL)reinstalls++;
            assert(item.get_action()!=Action::REMOVE);
        }
        if(phase=="downgrade"||phase=="downgrade-update")assert(downgrades>=1&&reinstalls>=1);
        tx.download();
        if(phase=="unsigned"){assert(!tx.check_gpg_signatures());return;}
        assert(tx.check_gpg_signatures());
        assert(tx.run()==libdnf5::base::Transaction::TransactionRunResult::SUCCESS);
    },phase=="fedora-fastfetch"||phase=="rename-update-vendor");
}
