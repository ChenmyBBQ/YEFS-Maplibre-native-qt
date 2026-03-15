// Copyright (C) 2023 MapLibre contributors

// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include "map.hpp"

#include <mbgl/map/map.hpp>
#include <mbgl/style/style_property.hpp>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtCore/QVector>

namespace QMapLibre {

struct LayerStateTrackedProperty final {
    bool layoutProperty = false;
    QVariantMap snapshot;
};

struct StyleBatchState final {
    QHash<QString, QVector<QString>> layoutChanges;
    QHash<QString, QVector<QString>> paintChanges;
    QVector<QString> patchedLayers;
    QHash<QString, QVariantMap> originalSnapshots;
    bool hasError = false;

    void clear();
};

class StyleManager final : public QObject {
    Q_OBJECT

public:
    explicit StyleManager(mbgl::Map &map, QObject *parent = nullptr);

    bool setLayoutProperty(const QString &layerId, const QString &propertyName, const QVariant &value);
    bool setPaintProperty(const QString &layerId, const QString &propertyName, const QVariant &value);
    bool setLayoutExpression(const QString &layerId, const QString &propertyName, const QVariantList &expression);
    bool setPaintExpression(const QString &layerId, const QString &propertyName, const QVariantList &expression);
    bool applyStylePatch(const QString &layerId, const QVariantMap &patch);
    void setFilter(const QString &layerId, const QVariant &filter);
    [[nodiscard]] QVariant getFilter(const QString &layerId) const;
    bool registerLayerStateStyle(const QString &layerId, const QString &stateId, const QVariantMap &patch);
    bool setLayerStateStyleActive(const QString &layerId, const QString &stateId, bool active);
    bool unregisterLayerStateStyle(const QString &layerId, const QString &stateId);
    [[nodiscard]] QStringList activeLayerStateStyles(const QString &layerId) const;
    void beginBatch();
    void commitBatch();
    [[nodiscard]] bool isBatchActive() const;
    [[nodiscard]] Map::StyleErrorCode lastErrorCode() const;
    [[nodiscard]] QString lastErrorMessage() const;
    [[nodiscard]] QVariantMap styleSnapshot(const QString &layerId) const;
    [[nodiscard]] QVariantMap stylePropertySnapshot(const QString &layerId, const QStringList &propertyNames) const;

public slots:
    void onLayerRemoved(const QString &layerId);

Q_SIGNALS:
    void layoutPropertyChanged(const QString &layerId, const QString &propertyName);
    void paintPropertyChanged(const QString &layerId, const QString &propertyName);
    void stylePatchApplied(const QString &layerId);
    void styleBatchCommitted();

private:
    using PropertySetter = std::optional<mbgl::style::conversion::Error> (mbgl::style::Layer::*)(
        const std::string &, const mbgl::style::conversion::Convertible &);

    bool setProperty(const PropertySetter &setter,
                     const QString &layerId,
                     const QString &name,
                     const QVariant &value,
                     bool layoutProperty);
    [[nodiscard]] mbgl::style::Layer *layerById(const QString &layerId) const;
    [[nodiscard]] bool validateLayer(const QString &layerId,
                                     const QString &propertyName,
                                     bool layoutProperty,
                                     mbgl::style::Layer *&layer) const;
    bool applyPropertyMap(const QString &layerId, const QVariantMap &properties, bool layoutProperties);
    [[nodiscard]] bool validatePatch(const QString &layerId, const QVariantMap &patch) const;
    [[nodiscard]] bool validateStateStyleRegistration(const QString &layerId, const QVariantMap &patch) const;
    [[nodiscard]] bool isSupportedLayerType(const QString &layerType) const;
    [[nodiscard]] bool isPropertyAllowed(const QString &layerType,
                                         const QString &propertyName,
                                         bool layoutProperty) const;
    [[nodiscard]] static QString layerType(const mbgl::style::Layer &layer);
    [[nodiscard]] static bool hasAllowedPrefix(const QString &propertyName, const QStringList &prefixes);
    [[nodiscard]] static QHash<QString, bool> patchPropertyKinds(const QVariantMap &patch);
    [[nodiscard]] QVariantMap effectiveStatePatch(const QString &layerId) const;
    void captureTrackedProperties(const QString &layerId, const QVariantMap &patch);
    bool applyActiveStateStyles(const QString &layerId);
    bool restoreLayerSnapshot(const QString &layerId, const QVariantMap &snapshot);
    bool applyTrackedPropertyValue(const QString &layerId,
                                   const QString &propertyName,
                                   bool layoutProperty,
                                   const QVariant &value);
    void setError(Map::StyleErrorCode code, const QString &message) const;
    void clearError() const;
    void emitLayoutPropertyChanged(const QString &layerId, const QString &propertyName);
    void emitPaintPropertyChanged(const QString &layerId, const QString &propertyName);
    void emitStylePatchApplied(const QString &layerId);
    void flushBatchEvents();
    static void appendUnique(QVector<QString> &values, const QString &value);

    mbgl::Map &m_map;
    int m_batchDepth = 0;
    StyleBatchState m_batchState;
    mutable Map::StyleErrorCode m_lastErrorCode = Map::NoStyleError;
    mutable QString m_lastErrorMessage;
    QHash<QString, QHash<QString, QVariantMap>> m_registeredStateStyles;
    QHash<QString, QStringList> m_activeStateStyles;
    QHash<QString, QHash<QString, LayerStateTrackedProperty>> m_trackedStateProperties;
};

} // namespace QMapLibre