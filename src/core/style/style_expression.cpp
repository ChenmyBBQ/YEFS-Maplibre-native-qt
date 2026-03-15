// Copyright (C) 2023 MapLibre contributors

// SPDX-License-Identifier: BSD-2-Clause

#include "style_expression.hpp"

namespace QMapLibre::StyleExpression {

QVariantList literal(const QVariant &value) {
    return {QStringLiteral("literal"), value};
}

QVariantList get(const QString &propertyName) {
    return {QStringLiteral("get"), propertyName};
}

QVariantList featureState(const QString &stateKey) {
    return {QStringLiteral("feature-state"), stateKey};
}

QVariantList booleanFeatureState(const QString &stateKey, bool fallbackValue) {
    return {QStringLiteral("boolean"), QVariant::fromValue(featureState(stateKey)), fallbackValue};
}

QVariantList eq(const QVariant &left, const QVariant &right) {
    return {QStringLiteral("=="), left, right};
}

QVariantList caseWhen(const QVariant &condition, const QVariant &trueValue, const QVariant &falseValue) {
    return {QStringLiteral("case"), condition, trueValue, falseValue};
}

} // namespace QMapLibre::StyleExpression