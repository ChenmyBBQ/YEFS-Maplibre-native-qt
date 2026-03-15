// Copyright (C) 2023 MapLibre contributors

// SPDX-License-Identifier: BSD-2-Clause

#ifndef QMAPLIBRE_STYLE_EXPRESSION_H
#define QMAPLIBRE_STYLE_EXPRESSION_H

#include <QMapLibre/Export>

#include <QtCore/QVariant>
#include <QtCore/QVariantList>

namespace QMapLibre::StyleExpression {

Q_MAPLIBRE_CORE_EXPORT QVariantList literal(const QVariant &value);
Q_MAPLIBRE_CORE_EXPORT QVariantList get(const QString &propertyName);
Q_MAPLIBRE_CORE_EXPORT QVariantList featureState(const QString &stateKey);
Q_MAPLIBRE_CORE_EXPORT QVariantList booleanFeatureState(const QString &stateKey, bool fallbackValue = false);
Q_MAPLIBRE_CORE_EXPORT QVariantList eq(const QVariant &left, const QVariant &right);
Q_MAPLIBRE_CORE_EXPORT QVariantList caseWhen(const QVariant &condition,
                                             const QVariant &trueValue,
                                             const QVariant &falseValue);

} // namespace QMapLibre::StyleExpression

#endif // QMAPLIBRE_STYLE_EXPRESSION_H