// Copyright (C) 2023 MapLibre contributors

// SPDX-License-Identifier: BSD-2-Clause

#include "value_conversion_p.hpp"

#include <QtGui/QColor>

namespace QMapLibre {

QVariant variantFromValue(const mbgl::Value &value) {
    return value.match([](const mbgl::NullValue) { return QVariant(); },
                       [](const bool value_) { return QVariant(value_); },
                       [](const float value_) { return QVariant(value_); },
                       [](const int64_t value_) { return QVariant(static_cast<qlonglong>(value_)); },
                       [](const double value_) { return QVariant(value_); },
                       [](const std::string &value_) { return QVariant(value_.c_str()); },
                       [](const mbgl::Color &value_) {
                           return QColor(static_cast<int>(value_.r),
                                         static_cast<int>(value_.g),
                                         static_cast<int>(value_.b),
                                         static_cast<int>(value_.a));
                       },
                       [&](const std::vector<mbgl::Value> &vector) {
                           QVariantList list;
                           list.reserve(static_cast<int>(vector.size()));
                           for (const auto &value_ : vector) {
                               list.push_back(variantFromValue(value_));
                           }
                           return QVariant(list);
                       },
                       [&](const std::unordered_map<std::string, mbgl::Value> &map) {
                           QVariantMap varMap;
                           for (const auto &item : map) {
                               varMap.insert(item.first.c_str(), variantFromValue(item.second));
                           }
                           return QVariant(varMap);
                       },
                       [](const auto &) { return QVariant(); });
}

QString stylePropertyKindName(const mbgl::style::StyleProperty::Kind kind) {
    switch (kind) {
        case mbgl::style::StyleProperty::Kind::Constant:
            return QStringLiteral("constant");
        case mbgl::style::StyleProperty::Kind::Expression:
            return QStringLiteral("expression");
        case mbgl::style::StyleProperty::Kind::Transition:
            return QStringLiteral("transition");
        case mbgl::style::StyleProperty::Kind::Undefined:
        default:
            return QStringLiteral("undefined");
    }
}

QVariantMap variantMapFromStyleProperty(const mbgl::style::StyleProperty &property) {
    QVariantMap snapshot;
    snapshot.insert(QStringLiteral("kind"), stylePropertyKindName(property.getKind()));
    if (property.getValue()) {
        snapshot.insert(QStringLiteral("value"), variantFromValue(property.getValue()));
    }
    return snapshot;
}

} // namespace QMapLibre