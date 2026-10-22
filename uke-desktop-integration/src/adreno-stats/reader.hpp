// SPDX-License-Identifier: MIT
#pragma once
#include "counters.hpp"
#include <QFile>
namespace uke {
inline std::optional<std::uint64_t> read_counter(const QString& path) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))return std::nullopt;
    // Sysfs reports PAGE_SIZE even for a short value; atEnd() is not a length check.
    const auto value=file.read(129);
    if(value.size()>128||file.error()!=QFileDevice::NoError)return std::nullopt;
    return unsigned_value(std::string_view(value.constData(),value.size()));
}
}
