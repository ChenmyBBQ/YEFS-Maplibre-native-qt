// Copyright (C) 2023 MapLibre contributors

// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <mbgl/style/style_property.hpp>
#include <mbgl/util/feature.hpp>
#include <mbgl/util/color.hpp>

#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QtCore/QVariantMap>

namespace QMapLibre {

QVariant variantFromValue(const mbgl::Value &value);
QString stylePropertyKindName(mbgl::style::StyleProperty::Kind kind);
QVariantMap variantMapFromStyleProperty(const mbgl::style::StyleProperty &property);

} // namespace QMapLibre