// Copyright (C) 2023 MapLibre contributors

// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <mbgl/map/map.hpp>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtCore/QVector>

namespace QMapLibre {

struct SourceRecord final {
    QString id;
    QString type;
};

struct LayerRecord final {
    QString id;
    QString type;
    QString sourceId;
    QString groupId;
    bool requestedVisibility = true;
    bool actualVisibility = true;
    int order = -1;
    double minZoom = 0.0;
    double maxZoom = 0.0;
};

struct LayerBatchState final {
    QVector<QString> addedSources;
    QVector<QString> updatedSources;
    QVector<QString> removedSources;
    QVector<QString> addedLayers;
    QVector<QString> removedLayers;
    QVector<QString> visibilityChangedLayers;
    QVector<QString> groupChangedLayers;
    bool orderChanged = false;

    void clear();
};

class LayerManager final : public QObject {
    Q_OBJECT

public:
    explicit LayerManager(mbgl::Map &map, QObject *parent = nullptr);

    bool addSource(const QString &id, const QVariantMap &params);
    [[nodiscard]] bool sourceExists(const QString &id) const;
    bool updateSource(const QString &id, const QVariantMap &params);
    bool removeSource(const QString &id);

    bool addLayer(const QString &id, const QVariantMap &params, const QString &before);
    [[nodiscard]] bool layerExists(const QString &id) const;
    bool removeLayer(const QString &id);

    [[nodiscard]] QVector<QString> layerIds() const;
    bool setLayerVisible(const QString &id, bool visible);
    [[nodiscard]] bool isLayerVisible(const QString &id) const;
    [[nodiscard]] bool moveLayer(const QString &id, const QString &before);

    [[nodiscard]] bool setLayerGroup(const QString &id, const QString &groupId);
    [[nodiscard]] QVector<QString> groupLayerIds(const QString &groupId) const;
    [[nodiscard]] bool setGroupVisible(const QString &groupId, bool visible);
    bool removeGroup(const QString &groupId);
    
    [[nodiscard]] QVariantMap layerMetadata(const QString &id) const;
    [[nodiscard]] QVariantList layerMetadataSnapshot() const;
    void notifyCustomLayerAdded(const QString &id);
    void beginBatch();
    void commitBatch();
    [[nodiscard]] bool isBatchActive() const;

Q_SIGNALS:
    void sourceAdded(const QString &id);
    void sourceUpdated(const QString &id);
    void sourceRemoved(const QString &id);
    void layerAdded(const QString &id);
    void layerRemoved(const QString &id);
    void layerVisibilityChanged(const QString &id, bool visible);
    void layerOrderChanged();
    void layerGroupChanged(const QString &id, const QString &groupId);
    void layerBatchCommitted();

private:
    void emitSourceAdded(const QString &id);
    void emitSourceUpdated(const QString &id);
    void emitSourceRemoved(const QString &id);
    void emitLayerAdded(const QString &id);
    void emitLayerRemoved(const QString &id);
    void emitLayerVisibilityChanged(const QString &id);
    void emitLayerOrderChanged();
    void emitLayerGroupChanged(const QString &id);
    void flushBatchEvents();
    static void appendUnique(QVector<QString> &values, const QString &value);
    [[nodiscard]] LayerRecord makeLayerRecord(const mbgl::style::Layer &layer, int order) const;
    void syncLayerRecords() const;
    void syncSourceRecord(const QString &id, const QVariantMap &params);
    static void sanitizeLayerParams(QString &groupId, QVariantMap &params);
    [[nodiscard]] static QVariantMap toMetadataMap(const LayerRecord &record);
    void updateLayerActualVisibility(LayerRecord &record, mbgl::style::Layer *layer = nullptr);

    mbgl::Map &m_map;
    int m_batchDepth = 0;
    LayerBatchState m_batchState;
    mutable QHash<QString, SourceRecord> m_sources;
    mutable QHash<QString, LayerRecord> m_layers;
    mutable QHash<QString, bool> m_groupVisibility;
};

} // namespace QMapLibre
