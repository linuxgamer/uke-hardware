# Split RPM-expanded recipes, preserving every reviewed build/check/scriptlet.
# Called by compose.sh; not installed on the target.
function fail(message) {print message >"/dev/stderr"; exit 1}
function renamed(name) {
    if(name in rename)return rename[name]
    if(name ~ /^uke-/)return name
    if(name ~ /-uke-/){sub(/-uke-/,"-",name);return "uke-" name}
    return "uke-" name
}
function mapped_dependency(line,  old,head,tail,pattern) {
    for(old in package_new) {
        pattern="(^|[[:space:]])" old "(\\([^)]*\\))?([[:space:]]|$)"
        # Literal package token replacement; '+' is legal in RPM names.
        head=line; line=""
        while(match(head,/[A-Za-z0-9_+.-]+(\([^)]*\))?/)) {
            line=line substr(head,1,RSTART-1);tail=substr(head,RSTART,RLENGTH)
            if(tail==old || index(tail,old "(")==1)tail=package_new[old] substr(tail,length(old)+1)
            line=line tail;head=substr(head,RSTART+RLENGTH)
        }
        line=line head
    }
    return line
}
function output(file,line) {print line >> (out "/" file)}
function section_package(line, prefix,  a,n,pkg,options,i) {
    n=split(line,a,/[[:space:]]+/);pkg=original;options=""
    if(n>=3 && a[2]=="-n"){pkg=a[3];i=4}
    else if(n>=2 && a[2] !~ /^-/){pkg=original "-" a[2];i=3}
    else i=2
    for(;i<=n;i++)options=options " " a[i]
    return prefix " -n " renamed(pkg) options
}
BEGIN {
    while((getline < mapping)>0){split($0,a,"\t");rename[a[1]]=a[2]}
    close(mapping)
    phase="header";package=0
}
/^Name:/ {original=$2; next}
/^Version:/ && phase=="header" {version=$2;next}
/^Release:/ && phase=="header" {release=$2;next}
/^Epoch:/ && phase=="header" {epoch=$2;next}
/^License:/ && phase=="header" {license=substr($0,index($0,":")+1);next}
/^License:/ && phase=="package" {licenses[package]=substr($0,index($0,":")+1);next}
/^BuildArch:/ && phase=="header" {arch=$2;next}
/^ExclusiveArch:/ && phase=="header" {next}
/^Source[0-9]*:/ || /^Patch[0-9]*:/ || /^URL:/ {if(phase=="header")next}
/^BuildRequires:/ || /^BuildConflicts:/ {output("buildrequires",$0);next}
/^%package([[:space:]]|$)/ {phase="package";package++;headers[package]=$0;next}
/^%description([[:space:]]|$)/ {phase="description";descriptions[package]=$0;next}
/^%(prep|build|install|check|clean|generate_buildrequires|conf|changelog)([[:space:]]|$)/ {
    phase=substr($1,2);if(phase=="clean" || phase=="conf")fail("Unreviewed build section: " phase)
    next
}
/^%(files|pre|post|preun|postun|pretrans|posttrans|trigger.*|filetrigger.*|transfiletrigger.*)([[:space:]]|$)/ {
    phase="tail";tail[++tails]=$0;next
}
/^: __UKE_CWD__=/ {cwd=substr($0,index($0,"=")+1);next}
/^: __UKE_BUILDROOT__=/ {buildroot=substr($0,index($0,"=")+1);next}
/^: __UKE_BUILDDIR__=/ {builddir=substr($0,index($0,"=")+1);next}
{
    if(phase=="header" || phase=="package") header[package]=header[package] $0 "\n"
    else if(phase=="description") description[package]=description[package] $0 "\n"
    else if(phase=="tail") tail[tails]=tail[tails] "\n" $0
    else if(phase!="changelog") body[phase]=body[phase] $0 "\n"
}
END {
    if(original=="" || version=="" || release=="")fail("Missing package identity")
    if(cwd=="" || cwd ~ /%|^\/|(^|\/)\.\.(\/|$)/)fail("Missing or unsafe build subdirectory")
    if(buildroot=="" || builddir=="")fail("Missing build paths")
    print original "\t" version "\t" release "\t" cwd "\t" builddir "\t" buildroot >out "/identity"
    newversion=native_version!=""?native_version:version
    newrelease=native_version!="" && native_version!=version?"1.fc46":release
    if(native_version=="" && original!="uke-plymonth" && original!="uke-desktop-metas" && original!="uke-firmware") {
        split(release,r,".");newrelease=(r[1]+1) ".fc46"
    }
    if(release_override!="")newrelease=release_override
    for(i=0;i<=package;i++) {
        pkg=original
        if(i>0) {
            split(headers[i],a,/[[:space:]]+/)
            pkg=a[2]=="-n"?a[3]:original "-" a[2]
        }
        package_new[pkg]=renamed(pkg)
    }
    for(i=0;i<=package;i++) {
        pkg=original
        if(i>0){split(headers[i],a,/[[:space:]]+/);pkg=a[2]=="-n"?a[3]:original "-" a[2]}
        output("preamble","%package -n " renamed(pkg))
        output("preamble","Version: " newversion "\nRelease: " newrelease "\nLicense:" (i in licenses?licenses[i]:license))
        if(epoch!="")output("preamble","Epoch: " epoch)
        if(arch!="")output("preamble","BuildArch: " arch)
        h=mapped_dependency(header[i]);oldevr=version "-" release;newevr=newversion "-" newrelease
        if(oldevr!=newevr)while((pos=index(h,oldevr)))h=substr(h,1,pos-1) newevr substr(h,pos+length(oldevr))
        output("preamble",h)
        # External consumers retain the original feature-capability names.
        count=split(header[i],original_lines,"\n")
        for(k=1;k<=count;k++) {
            alias=original_lines[k]
            if(alias ~ /^Provides:/ && mapped_dependency(alias)!=alias) {
                if(oldevr!=newevr)while((pos=index(alias,oldevr)))alias=substr(alias,1,pos-1) newevr substr(alias,pos+length(oldevr))
                output("preamble",alias)
            }
        }
        if(renamed(pkg)!=pkg) {
            evr=(epoch!=""?epoch ":":"") newevr
            output("preamble","Provides: " pkg " = " evr)
            if(arch!="noarch")output("preamble","Provides: " pkg "%{?_isa} = " evr)
            output("preamble","Obsoletes: " pkg " < " evr)
            output("preamble","Conflicts: " pkg)
        }
        output("preamble","%description -n " renamed(pkg) "\n" description[i])
    }
    for(p in body)output(p,body[p])
    for(i=1;i<=tails;i++) {
        split(tail[i],lines,"\n");marker=lines[1];sub(/[[:space:]].*/,"",marker)
        print section_package(lines[1],marker) >>out "/tail"
        for(j=2;j<=length(lines);j++)print lines[j] >>out "/tail"
    }
}
