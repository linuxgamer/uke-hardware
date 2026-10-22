// SPDX-License-Identifier: MIT
#include <QCoreApplication>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <systemstats/SensorPlugin.h>
#include <systemstats/SensorContainer.h>
#include <cassert>
#include <QFile>
#include <QTemporaryFile>
#include "reader.hpp"
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);assert(argc==2);
    const QString sysfs=QStringLiteral("/sys/class/net/lo/mtu");
    QFile file(sysfs);assert(file.open(QIODevice::ReadOnly));
    const auto text=file.read(129);const auto expected=uke::unsigned_value(std::string_view(text.constData(),text.size()));
    assert(expected&&uke::read_counter(sysfs)==expected);
    QTemporaryFile temporary;assert(temporary.open());
    assert(temporary.write("42\n")==3&&temporary.flush());
    assert(uke::read_counter(temporary.fileName())==42);
    assert(temporary.resize(0)&&temporary.seek(0));
    assert(temporary.write(QByteArray(129,'1'))==129&&temporary.flush());
    assert(!uke::read_counter(temporary.fileName()));
    assert(!uke::read_counter(temporary.fileName()+QStringLiteral(".missing")));
    KPluginMetaData metadata(QString::fromLocal8Bit(argv[1]));assert(metadata.isValid());
    auto loaded=KPluginFactory::instantiatePlugin<KSysGuard::SensorPlugin>(metadata);
    assert(loaded.plugin&&loaded.plugin->providerName()==QStringLiteral("uke-adreno"));
    auto containers=loaded.plugin->containers();
    assert(containers.size()==1&&containers[0]->id()==QStringLiteral("adreno"));
    loaded.plugin->update();app.processEvents();delete loaded.plugin;
}
