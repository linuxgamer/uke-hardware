// SPDX-License-Identifier: MIT
#include "counters.hpp"
#include "reader.hpp"
#include <KPluginFactory>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QVariant>
#include <systemstats/SensorContainer.h>
#include <systemstats/SensorObject.h>
#include <systemstats/SensorPlugin.h>
#include <systemstats/SensorProperty.h>
#include <limits>
#include <vector>

namespace {
constexpr double unavailable=std::numeric_limits<double>::quiet_NaN();
struct Device {
    QString path;
    KSysGuard::SensorProperty *usage,*frequency;
    uke::Activity rate;
};
}
class AdrenoPlugin final:public KSysGuard::SensorPlugin {
    Q_OBJECT
    std::vector<Device> devices_;
    QElapsedTimer clock_;
public:
    AdrenoPlugin(QObject* parent,const QVariantList& args):SensorPlugin(parent,args) {
        clock_.start();
        auto group=new KSysGuard::SensorContainer(QStringLiteral("adreno"),QStringLiteral("Adreno GPU"),this);
        QDir directory(QStringLiteral("/sys/class/devfreq"));
        for(const auto& entry:directory.entryList(QDir::Dirs|QDir::NoDotAndDotDot,QDir::Name)) {
            const auto path=directory.filePath(entry);
            if(QFileInfo(QFileInfo(path+QStringLiteral("/device/driver")).canonicalFilePath()).fileName()!=QStringLiteral("adreno")
                ||!QFileInfo::exists(path+QStringLiteral("/gpu_engine_ns")))continue;
            auto object=new KSysGuard::SensorObject(QStringLiteral("gpu%1").arg(devices_.size()),QStringLiteral("Adreno GPU"),group);
            auto usage=new KSysGuard::SensorProperty(QStringLiteral("usage"),QStringLiteral("GPU activity"),unavailable,object);
            usage->setUnit(KSysGuard::UnitPercent);usage->setMin(0);usage->setMax(100);
            usage->setDescription(QStringLiteral("Completed GPU execution time across all clients divided by monotonic elapsed time. Long jobs are counted when retired; the display is capped at 100%."));
            auto frequency=new KSysGuard::SensorProperty(QStringLiteral("coreFrequency"),QStringLiteral("GPU frequency"),unavailable,object);
            frequency->setUnit(KSysGuard::UnitMegaHertz);
            frequency->setDescription(QStringLiteral("Driver-reported devfreq frequency; this is not a power measurement."));
            devices_.push_back({path,usage,frequency,{}});
        }
    }
    QString providerName()const override{return QStringLiteral("uke-adreno");}
    void update()override {
        for(auto& device:devices_) {
            if(device.usage->isSubscribed()) {
                auto counter=uke::read_counter(device.path+QStringLiteral("/gpu_engine_ns"));
                if(counter){auto value=device.rate.sample(*counter,clock_.nsecsElapsed());device.usage->setValue(value.value_or(unavailable));}
                else {device.rate.reset();device.usage->setValue(unavailable);}
            } else device.rate.reset();
            if(device.frequency->isSubscribed()) {
                auto value=uke::read_counter(device.path+QStringLiteral("/cur_freq"));
                device.frequency->setValue(value?double(*value)/1000000.0:unavailable);
            }
        }
    }
};
K_PLUGIN_CLASS_WITH_JSON(AdrenoPlugin,"plugin.json")
#include "plugin.moc"
