// Copyright (C) 2023 MapLibre contributors

// SPDX-License-Identifier: BSD-2-Clause

#include "layer_manager_p.hpp"

#include "conversion_p.hpp"

#include <mbgl/util/geojson.hpp>
#include <mbgl/style/conversion/coordinate.hpp>
#include <mbgl/style/conversion/geojson.hpp>
#include <mbgl/style/conversion/layer.hpp>
#include <mbgl/style/conversion/source.hpp>
#include <mbgl/style/conversion_impl.hpp>
#include <mbgl/style/layer.hpp>
#include <mbgl/style/source.hpp>
#include <mbgl/style/sources/geojson_source.hpp>
#include <mbgl/style/sources/image_source.hpp>
#include <mbgl/style/style.hpp>

#include <QtCore/QDebug>
#include <QtCore/QList>

#include <algorithm>
#include <array>
#include <optional>

namespace {

QString sourceTypeFromParams(const QVariantMap &params) {
    return params.value(QStringLiteral("type")).toString();
}

std::optional<std::string> beforeLayerId(const QString &before) {
    if (before.isEmpty()) {
        return std::nullopt;
    }

    return before.toStdString();
}

} // namespace

namespace QMapLibre {

void LayerBatchState::clear() {
    addedSources.clear();
    updatedSources.clear();
    removedSources.clear();
    addedLayers.clear();
    removedLayers.clear();
    visibilityChangedLayers.clear();
    groupChangedLayers.clear();
    orderChanged = false;
}

LayerManager::LayerManager(mbgl::Map &map, QObject *parent)
    : QObject(parent),
      m_map(map) {
}

bool LayerManager::addSource(const QString &id, const QVariantMap &params) {
    mbgl::style::conversion::Error error;
    auto source = mbgl::style::conversion::convert<std::unique_ptr<mbgl::style::Source>>(
        QVariant(params), error, id.toStdString());
    if (!source) {
        qWarning() << "Unable to add source with id" << id << ":" << error.message.c_str();
        return false;
    }

    m_map.getStyle().addSource(std::move(*source));
    syncSourceRecord(id, params);
    emitSourceAdded(id);
    return true;
}

bool LayerManager::sourceExists(const QString &id) const {
    return m_map.getStyle().getSource(id.toStdString()) != nullptr;
}

bool LayerManager::updateSource(const QString &id, const QVariantMap &params) {
    auto *source = m_map.getStyle().getSource(id.toStdString());
    if (source == nullptr) {
        return addSource(id, params);
    }

    auto *sourceGeoJson = source->as<mbgl::style::GeoJSONSource>();
    auto *sourceImage = source->as<mbgl::style::ImageSource>();
    if (sourceGeoJson == nullptr && sourceImage == nullptr) {
        qWarning() << "Unable to update source: only GeoJSON and Image sources are mutable.";
        return false;
    }

    if (sourceImage != nullptr) {
        if (params.contains(QStringLiteral("url"))) {
            sourceImage->setURL(params.value(QStringLiteral("url")).toString().toStdString());
        }
        if (params.contains(QStringLiteral("coordinates")) &&
            params.value(QStringLiteral("coordinates")).toList().size() == 4) {
            mbgl::style::conversion::Error error;
            std::array<mbgl::LatLng, 4> coordinates;
            const auto coordinateList = params.value(QStringLiteral("coordinates")).toList();
            for (std::size_t index = 0; index < coordinates.size(); ++index) {
                auto latLng = mbgl::style::conversion::convert<mbgl::LatLng>(
                    coordinateList.at(static_cast<int>(index)), error);
                if (latLng) {
                    coordinates[index] = *latLng;
                }
            }
            sourceImage->setCoordinates(coordinates);
        }
    } else if (params.contains(QStringLiteral("data"))) {
        mbgl::style::conversion::Error error;
        auto result = mbgl::style::conversion::convert<mbgl::GeoJSON>(params.value(QStringLiteral("data")), error);
        if (result) {
            sourceGeoJson->setGeoJSON(*result);
        }
    }

    syncSourceRecord(id, params);
    emitSourceUpdated(id);
    return true;
}

bool LayerManager::removeSource(const QString &id) {
    if (!sourceExists(id)) {
        return false;
    }

    m_map.getStyle().removeSource(id.toStdString());
    m_sources.remove(id);
    emitSourceRemoved(id);
    return true;
}

bool LayerManager::addLayer(const QString &id, const QVariantMap &params, const QString &before) {
    QVariantMap layerParams = params;
    QString groupId;
    sanitizeLayerParams(groupId, layerParams);
    layerParams.insert(QStringLiteral("id"), id);

    mbgl::style::conversion::Error error;
    auto layer = mbgl::style::conversion::convert<std::unique_ptr<mbgl::style::Layer>>(QVariant(layerParams), error);
    if (!layer) {
        qWarning() << "Unable to add layer with id" << id << ":" << error.message.c_str();
        return false;
    }

    m_map.getStyle().addLayer(std::move(*layer), beforeLayerId(before));
    syncLayerRecords();
    if (!groupId.isEmpty()) {
        setLayerGroup(id, groupId);
    }
    emitLayerAdded(id);
    emitLayerOrderChanged();
    return true;
}

bool LayerManager::layerExists(const QString &id) const {
    return m_map.getStyle().getLayer(id.toStdString()) != nullptr;
}

bool LayerManager::removeLayer(const QString &id) {
    if (!layerExists(id)) {
        return false;
    }

    m_map.getStyle().removeLayer(id.toStdString());
    m_layers.remove(id);
    syncLayerRecords();
    emitLayerRemoved(id);
    emitLayerOrderChanged();
    return true;
}

QVector<QString> LayerManager::layerIds() const {
    syncLayerRecords();

    QVector<QString> ids;
    ids.reserve(m_layers.size());
    for (const auto &record : m_layers) {
        ids.push_back(record.id);
    }

    std::sort(ids.begin(), ids.end(), [this](const QString &left, const QString &right) {
        return m_layers.value(left).order < m_layers.value(right).order;
    });
    return ids;
}

void LayerManager::updateLayerActualVisibility(LayerRecord &record, mbgl::style::Layer *layer) {
    bool groupVisible = true;
    if (!record.groupId.isEmpty() && m_groupVisibility.contains(record.groupId)) {
        groupVisible = m_groupVisibility.value(record.groupId);
    }
    
    bool targetVisibility = record.requestedVisibility && groupVisible;
    if (record.actualVisibility != targetVisibility || !layer) {
        record.actualVisibility = targetVisibility;
        if (!layer) {
            layer = m_map.getStyle().getLayer(record.id.toStdString());
        }
        if (layer) {
            layer->setVisibility(targetVisibility ? mbgl::style::VisibilityType::Visible : mbgl::style::VisibilityType::None);
        }
    }
}

bool LayerManager::setLayerVisible(const QString &id, bool visible) {
    auto *layer = m_map.getStyle().getLayer(id.toStdString());
    if (layer == nullptr) {
        qWarning() << "Layer not found:" << id;
        return false;
    }

    syncLayerRecords();
    auto record = m_layers.value(id);
    const bool oldActualVisibility = record.actualVisibility;
    record.requestedVisibility = visible;
    updateLayerActualVisibility(record, layer);
    
    m_layers.insert(id, record);
    if (oldActualVisibility != record.actualVisibility) {
        emitLayerVisibilityChanged(id);
    }
    return true;
}

bool LayerManager::isLayerVisible(const QString &id) const {
    syncLayerRecords();
    return m_layers.contains(id) && m_layers.value(id).actualVisibility;
}

bool LayerManager::moveLayer(const QString &id, const QString &before) {
    if (!layerExists(id)) {
        qWarning() << "Layer not found:" << id;
        return false;
    }
    if (!before.isEmpty() && !layerExists(before)) {
        qWarning() << "Target layer not found:" << before;
        return false;
    }
    if (id == before) {
        return false;
    }

    auto layer = m_map.getStyle().removeLayer(id.toStdString());
    if (!layer) {
        return false;
    }

    m_map.getStyle().addLayer(std::move(layer), beforeLayerId(before));
    syncLayerRecords();
    emitLayerOrderChanged();
    return true;
}

bool LayerManager::setLayerGroup(const QString &id, const QString &groupId) {
    syncLayerRecords();
    if (!m_layers.contains(id)) {
        qWarning() << "Layer not found:" << id;
        return false;
    }

    auto record = m_layers.value(id);
    record.groupId = groupId;
    
    bool oldActual = record.actualVisibility;
    updateLayerActualVisibility(record, nullptr);
    
    m_layers.insert(id, record);
    emitLayerGroupChanged(id);
    
    if (oldActual != record.actualVisibility) {
        emitLayerVisibilityChanged(id);
    }
    
    return true;
}

QVector<QString> LayerManager::groupLayerIds(const QString &groupId) const {
    syncLayerRecords();

    QVector<QString> ids;
    for (const auto &record : m_layers) {
        if (record.groupId == groupId) {
            ids.push_back(record.id);
        }
    }

    std::sort(ids.begin(), ids.end(), [this](const QString &left, const QString &right) {
        return m_layers.value(left).order < m_layers.value(right).order;
    });
    return ids;
}

bool LayerManager::setGroupVisible(const QString &groupId, bool visible) {
    if (groupId.isEmpty()) {
        return false;
    }

    syncLayerRecords();
    
    // Only process if creating new entry or changing existing
    if (m_groupVisibility.contains(groupId) && m_groupVisibility.value(groupId) == visible) {
        return true;
    }
    
    m_groupVisibility.insert(groupId, visible);
    
    beginBatch();
    for (auto it = m_layers.begin(); it != m_layers.end(); ++it) {
        if (it->groupId == groupId) {
            bool oldActual = it->actualVisibility;
            updateLayerActualVisibility(*it, nullptr);
            if (oldActual != it->actualVisibility) {
                emitLayerVisibilityChanged(it->id);
            }
        }
    }
    commitBatch();
    return true;
}

bool LayerManager::removeGroup(const QString &groupId) {
    if (groupId.isEmpty()) {
        return false;
    }

    const auto ids = groupLayerIds(groupId);
    if (ids.isEmpty()) {
        return false;
    }

    m_groupVisibility.remove(groupId);

    beginBatch();
    for (const auto &id : ids) {
        setLayerGroup(id, QString());
    }
    commitBatch();

    return true;
}

QVariantMap LayerManager::layerMetadata(const QString &id) const {
    syncLayerRecords();
    return m_layers.contains(id) ? toMetadataMap(m_layers.value(id)) : QVariantMap();
}

QVariantList LayerManager::layerMetadataSnapshot() const {
    syncLayerRecords();

    QVariantList snapshot;
    const auto ids = layerIds();
    snapshot.reserve(ids.size());
    for (const auto &id : ids) {
        snapshot.push_back(layerMetadata(id));
    }

    return snapshot;
}

void LayerManager::notifyCustomLayerAdded(const QString &id) {
    syncLayerRecords();
    emitLayerAdded(id);
    emitLayerOrderChanged();
}

void LayerManager::beginBatch() {
    ++m_batchDepth;
}

void LayerManager::commitBatch() {
    if (m_batchDepth == 0) {
        return;
    }

    --m_batchDepth;
    if (m_batchDepth == 0) {
        flushBatchEvents();
        emit layerBatchCommitted();
    }
}

bool LayerManager::isBatchActive() const {
    return m_batchDepth > 0;
}

void LayerManager::emitSourceAdded(const QString &id) {
    if (!isBatchActive()) {
        emit sourceAdded(id);
        return;
    }

    appendUnique(m_batchState.addedSources, id);
}

void LayerManager::emitSourceUpdated(const QString &id) {
    if (!isBatchActive()) {
        emit sourceUpdated(id);
        return;
    }

    appendUnique(m_batchState.updatedSources, id);
}

void LayerManager::emitSourceRemoved(const QString &id) {
    if (!isBatchActive()) {
        emit sourceRemoved(id);
        return;
    }

    appendUnique(m_batchState.removedSources, id);
}

void LayerManager::emitLayerAdded(const QString &id) {
    if (!isBatchActive()) {
        emit layerAdded(id);
        return;
    }

    appendUnique(m_batchState.addedLayers, id);
}

void LayerManager::emitLayerRemoved(const QString &id) {
    if (!isBatchActive()) {
        emit layerRemoved(id);
        return;
    }

    appendUnique(m_batchState.removedLayers, id);
}

void LayerManager::emitLayerVisibilityChanged(const QString &id) {
    if (!isBatchActive()) {
        emit layerVisibilityChanged(id, isLayerVisible(id));
        return;
    }

    appendUnique(m_batchState.visibilityChangedLayers, id);
}

void LayerManager::emitLayerOrderChanged() {
    if (!isBatchActive()) {
        emit layerOrderChanged();
        return;
    }

    m_batchState.orderChanged = true;
}

void LayerManager::emitLayerGroupChanged(const QString &id) {
    if (!isBatchActive()) {
        emit layerGroupChanged(id, m_layers.value(id).groupId);
        return;
    }

    appendUnique(m_batchState.groupChangedLayers, id);
}

void LayerManager::flushBatchEvents() {
    for (const auto &id : m_batchState.addedSources) {
        emit sourceAdded(id);
    }
    for (const auto &id : m_batchState.updatedSources) {
        emit sourceUpdated(id);
    }
    for (const auto &id : m_batchState.removedSources) {
        emit sourceRemoved(id);
    }
    for (const auto &id : m_batchState.addedLayers) {
        emit layerAdded(id);
    }
    for (const auto &id : m_batchState.removedLayers) {
        emit layerRemoved(id);
    }
    for (const auto &id : m_batchState.visibilityChangedLayers) {
        emit layerVisibilityChanged(id, isLayerVisible(id));
    }
    if (m_batchState.orderChanged) {
        emit layerOrderChanged();
    }
    for (const auto &id : m_batchState.groupChangedLayers) {
        emit layerGroupChanged(id, m_layers.value(id).groupId);
    }

    m_batchState.clear();
}

void LayerManager::appendUnique(QVector<QString> &values, const QString &value) {
    if (!values.contains(value)) {
        values.push_back(value);
    }
}

LayerRecord LayerManager::makeLayerRecord(const mbgl::style::Layer &layer, int order) const {
    LayerRecord record;
    const auto id = QString::fromStdString(layer.getID());
    record.id = id;
    record.order = order;
    record.type = QString::fromUtf8(layer.getTypeInfo()->type);
    record.sourceId = QString::fromStdString(layer.getSourceID());
    
    bool isVisible = layer.getVisibility() != mbgl::style::VisibilityType::None;
    record.actualVisibility = isVisible;
    record.requestedVisibility = isVisible; // Default to actual, override below if known
    
    record.minZoom = layer.getMinZoom();
    record.maxZoom = layer.getMaxZoom();
    if (m_layers.contains(id)) {
        const auto &existing = m_layers.value(id);
        record.groupId = existing.groupId;
        record.requestedVisibility = existing.requestedVisibility;
    }
    return record;
}

void LayerManager::syncLayerRecords() const {
    const auto layers = m_map.getStyle().getLayers();

    QHash<QString, LayerRecord> synced;
    synced.reserve(static_cast<qsizetype>(layers.size()));
    for (std::size_t index = 0; index < layers.size(); ++index) {
        const auto *layer = layers[index];
        const auto record = makeLayerRecord(*layer, static_cast<int>(index));
        synced.insert(record.id, record);
    }

    m_layers = synced;
}

void LayerManager::syncSourceRecord(const QString &id, const QVariantMap &params) {
    SourceRecord record;
    record.id = id;
    record.type = sourceTypeFromParams(params);
    m_sources.insert(id, record);
}

void LayerManager::sanitizeLayerParams(QString &groupId, QVariantMap &params) {
    groupId = params.value(QStringLiteral("groupId")).toString();
    params.remove(QStringLiteral("groupId"));
}

QVariantMap LayerManager::toMetadataMap(const LayerRecord &record) {
    QVariantMap metadata;
    metadata.insert(QStringLiteral("id"), record.id);
    metadata.insert(QStringLiteral("type"), record.type);
    metadata.insert(QStringLiteral("sourceId"), record.sourceId);
    metadata.insert(QStringLiteral("groupId"), record.groupId);
    metadata.insert(QStringLiteral("visible"), record.actualVisibility);
    metadata.insert(QStringLiteral("requestedVisibility"), record.requestedVisibility);
    metadata.insert(QStringLiteral("order"), record.order);
    metadata.insert(QStringLiteral("minZoom"), record.minZoom);
    metadata.insert(QStringLiteral("maxZoom"), record.maxZoom);
    return metadata;
}

} // namespace QMapLibre
