// Copyright (C) 2023 MapLibre contributors

// SPDX-License-Identifier: BSD-2-Clause

#include "style_manager_p.hpp"

#include "value_conversion_p.hpp"

#include <mbgl/style/conversion/layer.hpp>
#include <mbgl/style/conversion_impl.hpp>
#include <mbgl/style/conversion/filter.hpp>
#include <mbgl/style/filter.hpp>
#include <mbgl/style/layer.hpp>
#include <mbgl/style/style.hpp>
#include <mbgl/util/rapidjson.hpp>

#include <QtCore/QDebug>

#include <optional>

namespace QMapLibre {

namespace {

QStringList supportedPrefixes(const QString &layerType) {
    if (layerType == QStringLiteral("fill")) {
        return {QStringLiteral("fill-")};
    }
    if (layerType == QStringLiteral("line")) {
        return {QStringLiteral("line-")};
    }
    if (layerType == QStringLiteral("circle")) {
        return {QStringLiteral("circle-")};
    }
    if (layerType == QStringLiteral("symbol")) {
        return {QStringLiteral("icon-"), QStringLiteral("text-"), QStringLiteral("symbol-")};
    }
    return {};
}

bool isExpressionValue(const QVariantList &expression) {
    return !expression.isEmpty();
}

} // namespace

void StyleBatchState::clear() {
    layoutChanges.clear();
    paintChanges.clear();
    patchedLayers.clear();
    originalSnapshots.clear();
    hasError = false;
}

StyleManager::StyleManager(mbgl::Map &map, QObject *parent)
    : QObject(parent),
      m_map(map) {
}

bool StyleManager::setLayoutProperty(const QString &layerId, const QString &propertyName, const QVariant &value) {
    return setProperty(&mbgl::style::Layer::setProperty, layerId, propertyName, value, true);
}

bool StyleManager::setPaintProperty(const QString &layerId, const QString &propertyName, const QVariant &value) {
    return setProperty(&mbgl::style::Layer::setProperty, layerId, propertyName, value, false);
}

bool StyleManager::setLayoutExpression(const QString &layerId,
                                       const QString &propertyName,
                                       const QVariantList &expression) {
    if (!isExpressionValue(expression)) {
        setError(Map::StyleInvalidExpressionError, QStringLiteral("Expression must not be empty."));
        return false;
    }

    return setLayoutProperty(layerId, propertyName, expression);
}

bool StyleManager::setPaintExpression(const QString &layerId,
                                      const QString &propertyName,
                                      const QVariantList &expression) {
    if (!isExpressionValue(expression)) {
        setError(Map::StyleInvalidExpressionError, QStringLiteral("Expression must not be empty."));
        return false;
    }

    return setPaintProperty(layerId, propertyName, expression);
}

bool StyleManager::applyStylePatch(const QString &layerId, const QVariantMap &patch) {
    if (!validatePatch(layerId, patch)) {
        return false;
    }

    bool success = true;
    if (patch.contains(QStringLiteral("layout"))) {
        success = applyPropertyMap(layerId, patch.value(QStringLiteral("layout")).toMap(), true) && success;
    }
    if (patch.contains(QStringLiteral("paint"))) {
        success = applyPropertyMap(layerId, patch.value(QStringLiteral("paint")).toMap(), false) && success;
    }
    if (success) {
        emitStylePatchApplied(layerId);
    }
    return success;
}

void StyleManager::setFilter(const QString &layerId, const QVariant &filter) {
    auto *layer = layerById(layerId);
    if (layer == nullptr) {
        setError(Map::StyleLayerNotFoundError, QStringLiteral("Layer not found: %1").arg(layerId));
        if (isBatchActive()) {
            m_batchState.hasError = true;
        }
        qWarning() << m_lastErrorMessage;
        return;
    }

    if (isBatchActive() && !m_batchState.originalSnapshots.contains(layerId)) {
        m_batchState.originalSnapshots.insert(layerId, styleSnapshot(layerId));
    }

    clearError();
    if (filter.isNull() || filter.toList().isEmpty()) {
        layer->setFilter(mbgl::style::Filter());
        return;
    }

    mbgl::style::conversion::Error error;
    std::optional<mbgl::style::Filter> converted = mbgl::style::conversion::convert<mbgl::style::Filter>(filter, error);
    if (!converted) {
        setError(Map::StylePropertyApplyError,
                 QStringLiteral("Error parsing filter on layer %1: %2").arg(layerId, QString::fromStdString(error.message)));
        if (isBatchActive()) {
            m_batchState.hasError = true;
        }
        qWarning() << m_lastErrorMessage;
        return;
    }

    layer->setFilter(*converted);
}

QVariant StyleManager::getFilter(const QString &layerId) const {
    const auto *layer = layerById(layerId);
    if (layer == nullptr) {
        setError(Map::StyleLayerNotFoundError, QStringLiteral("Layer not found: %1").arg(layerId));
        return {};
    }

    clearError();
    auto serialized = layer->getFilter().serialize();
    return variantFromValue(serialized);
}

bool StyleManager::registerLayerStateStyle(const QString &layerId, const QString &stateId, const QVariantMap &patch) {
    if (stateId.isEmpty()) {
        setError(Map::StyleInvalidPatchError, QStringLiteral("State style id must not be empty."));
        return false;
    }
    if (!validatePatch(layerId, patch) || !validateStateStyleRegistration(layerId, patch)) {
        return false;
    }

    clearError();
    m_registeredStateStyles[layerId].insert(stateId, patch);
    return true;
}

bool StyleManager::setLayerStateStyleActive(const QString &layerId, const QString &stateId, bool active) {
    if (!m_registeredStateStyles.value(layerId).contains(stateId)) {
        setError(Map::StyleStateStyleNotFoundError,
                 QStringLiteral("Layer state style %1 is not registered on %2.").arg(stateId, layerId));
        return false;
    }

    auto &activeStates = m_activeStateStyles[layerId];
    const bool alreadyActive = activeStates.contains(stateId);
    if (active == alreadyActive) {
        clearError();
        return true;
    }

    if (active) {
        captureTrackedProperties(layerId, m_registeredStateStyles.value(layerId).value(stateId));
        activeStates.push_back(stateId);
    } else {
        activeStates.removeAll(stateId);
    }

    return applyActiveStateStyles(layerId);
}

bool StyleManager::unregisterLayerStateStyle(const QString &layerId, const QString &stateId) {
    if (!m_registeredStateStyles.value(layerId).contains(stateId)) {
        setError(Map::StyleStateStyleNotFoundError,
                 QStringLiteral("Layer state style %1 is not registered on %2.").arg(stateId, layerId));
        return false;
    }

    if (m_activeStateStyles.value(layerId).contains(stateId) && !setLayerStateStyleActive(layerId, stateId, false)) {
        return false;
    }

    m_registeredStateStyles[layerId].remove(stateId);
    if (m_registeredStateStyles.value(layerId).isEmpty()) {
        m_registeredStateStyles.remove(layerId);
    }
    clearError();
    return true;
}

QStringList StyleManager::activeLayerStateStyles(const QString &layerId) const {
    return m_activeStateStyles.value(layerId);
}

void StyleManager::beginBatch() {
    ++m_batchDepth;
}

void StyleManager::commitBatch() {
    if (m_batchDepth == 0) {
        return;
    }

    if (m_batchDepth == 1 && m_batchState.hasError) {
        // Restore the original layer objects when any change in the batch fails.
        for (auto it = m_batchState.originalSnapshots.cbegin(); it != m_batchState.originalSnapshots.cend(); ++it) {
            if (!restoreLayerSnapshot(it.key(), it.value())) {
                qWarning() << "Failed to restore style snapshot for layer" << it.key();
            }
        }
        m_batchState.clear();
        --m_batchDepth;
        // We intentionally do not clear m_lastErrorCode here so the caller can check it
        qWarning() << "StyleBatch aborted and rolled back due to preceding errors.";
        return;
    }

    --m_batchDepth;
    if (m_batchDepth == 0) {
        flushBatchEvents();
        emit styleBatchCommitted();
    }
}

bool StyleManager::isBatchActive() const {
    return m_batchDepth > 0;
}

Map::StyleErrorCode StyleManager::lastErrorCode() const {
    return m_lastErrorCode;
}

QString StyleManager::lastErrorMessage() const {
    return m_lastErrorMessage;
}

QVariantMap StyleManager::styleSnapshot(const QString &layerId) const {
    auto *layer = layerById(layerId);
    if (layer == nullptr) {
        setError(Map::StyleLayerNotFoundError, QStringLiteral("Layer not found: %1").arg(layerId));
        return {};
    }

    clearError();
    return variantFromValue(layer->serialize()).toMap();
}

QVariantMap StyleManager::stylePropertySnapshot(const QString &layerId, const QStringList &propertyNames) const {
    auto *layer = layerById(layerId);
    if (layer == nullptr) {
        setError(Map::StyleLayerNotFoundError, QStringLiteral("Layer not found: %1").arg(layerId));
        return {};
    }

    clearError();
    QVariantMap snapshot;
    for (const auto &propertyName : propertyNames) {
        const auto property = layer->getProperty(propertyName.toStdString());
        if (property.getKind() != mbgl::style::StyleProperty::Kind::Undefined) {
            snapshot.insert(propertyName, variantMapFromStyleProperty(property));
        }
    }
    return snapshot;
}

bool StyleManager::setProperty(const PropertySetter &setter,
                               const QString &layerId,
                               const QString &name,
                               const QVariant &value,
                               bool layoutProperty) {
    mbgl::style::Layer *layer = nullptr;
    if (!validateLayer(layerId, name, layoutProperty, layer)) {
        if (isBatchActive()) {
            m_batchState.hasError = true;
        }
        qWarning() << m_lastErrorMessage;
        return false;
    }

    if (isBatchActive() && !m_batchState.originalSnapshots.contains(layerId)) {
        m_batchState.originalSnapshots.insert(layerId, styleSnapshot(layerId));
    }

    clearError();
    const auto propertyName = name.toStdString();
    std::optional<mbgl::style::conversion::Error> result;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (value.typeId() == QMetaType::QString) {
#else
    if (value.type() == QVariant::String) {
#endif
        mbgl::JSDocument document;
        document.Parse<0>(value.toString().toStdString());
        if (!document.HasParseError()) {
            const mbgl::JSValue *jsonValue = &document;
            result = (layer->*setter)(propertyName, jsonValue);
        } else {
            result = (layer->*setter)(propertyName, value);
        }
    } else {
        result = (layer->*setter)(propertyName, value);
    }

    if (result) {
        setError(Map::StylePropertyApplyError,
                 QStringLiteral("Error setting property %1 on layer %2: %3")
                     .arg(name, layerId, QString::fromStdString(result->message)));
        if (isBatchActive()) {
            m_batchState.hasError = true;
        }
        qWarning() << m_lastErrorMessage;
        return false;
    }

    if (layoutProperty) {
        emitLayoutPropertyChanged(layerId, name);
    } else {
        emitPaintPropertyChanged(layerId, name);
    }
    return true;
}

mbgl::style::Layer *StyleManager::layerById(const QString &layerId) const {
    return m_map.getStyle().getLayer(layerId.toStdString());
}

bool StyleManager::validateLayer(const QString &layerId,
                                 const QString &propertyName,
                                 bool layoutProperty,
                                 mbgl::style::Layer *&layer) const {
    layer = layerById(layerId);
    if (layer == nullptr) {
        setError(Map::StyleLayerNotFoundError, QStringLiteral("Layer not found: %1").arg(layerId));
        return false;
    }

    const auto type = layerType(*layer);
    if (!isSupportedLayerType(type)) {
        setError(Map::StyleUnsupportedLayerTypeError,
                 QStringLiteral("Layer type %1 is not enabled in current StyleRuntime scope.").arg(type));
        return false;
    }
    if (!isPropertyAllowed(type, propertyName, layoutProperty)) {
        setError(Map::StyleUnsupportedPropertyError,
                 QStringLiteral("Property %1 is not allowed for %2 layer.").arg(propertyName, type));
        return false;
    }

    return true;
}

bool StyleManager::applyPropertyMap(const QString &layerId, const QVariantMap &properties, bool layoutProperties) {
    bool success = true;
    for (auto it = properties.cbegin(); it != properties.cend(); ++it) {
        const bool applied = layoutProperties ? setLayoutProperty(layerId, it.key(), it.value())
                                              : setPaintProperty(layerId, it.key(), it.value());
        success = applied && success;
    }
    return success;
}

bool StyleManager::validatePatch(const QString &layerId, const QVariantMap &patch) const {
    auto *layer = layerById(layerId);
    if (layer == nullptr) {
        setError(Map::StyleLayerNotFoundError, QStringLiteral("Layer not found: %1").arg(layerId));
        return false;
    }

    const auto type = layerType(*layer);
    if (!isSupportedLayerType(type)) {
        setError(Map::StyleUnsupportedLayerTypeError,
                 QStringLiteral("Layer type %1 is not enabled in current StyleRuntime scope.").arg(type));
        return false;
    }

    for (auto it = patch.cbegin(); it != patch.cend(); ++it) {
        const bool isLayoutPatch = it.key() == QStringLiteral("layout");
        const bool isPaintPatch = it.key() == QStringLiteral("paint");
        if (!isLayoutPatch && !isPaintPatch) {
            setError(Map::StyleInvalidPatchError,
                     QStringLiteral("Style patch only supports layout and paint sections."));
            return false;
        }

        const auto propertyMap = it.value().toMap();
        if (propertyMap.isEmpty() && !it.value().canConvert<QVariantMap>()) {
            setError(Map::StyleInvalidPatchError,
                     QStringLiteral("Style patch section %1 must be an object.").arg(it.key()));
            return false;
        }

        for (auto propertyIt = propertyMap.cbegin(); propertyIt != propertyMap.cend(); ++propertyIt) {
            if (!isPropertyAllowed(type, propertyIt.key(), isLayoutPatch)) {
                setError(Map::StyleUnsupportedPropertyError,
                         QStringLiteral("Property %1 is not allowed for %2 layer.").arg(propertyIt.key(), type));
                return false;
            }
        }
    }

    return true;
}

bool StyleManager::validateStateStyleRegistration(const QString &layerId, const QVariantMap &patch) const {
    const auto propertyKinds = patchPropertyKinds(patch);
    const auto snapshots = stylePropertySnapshot(layerId, QStringList(propertyKinds.keyBegin(), propertyKinds.keyEnd()));
    for (auto it = propertyKinds.cbegin(); it != propertyKinds.cend(); ++it) {
        if (!snapshots.contains(it.key())) {
            setError(Map::StyleInvalidPatchError,
                     QStringLiteral("State style requires an existing baseline property: %1").arg(it.key()));
            return false;
        }

        const auto propertySnapshot = snapshots.value(it.key()).toMap();
        if (propertySnapshot.value(QStringLiteral("kind")).toString() != QStringLiteral("constant") ||
            !propertySnapshot.contains(QStringLiteral("value"))) {
            setError(Map::StyleInvalidPatchError,
                     QStringLiteral("State style currently requires a constant baseline property: %1").arg(it.key()));
            return false;
        }
    }
    return true;
}

bool StyleManager::isSupportedLayerType(const QString &layerType) const {
    return layerType == QStringLiteral("fill") || layerType == QStringLiteral("line") ||
           layerType == QStringLiteral("symbol") || layerType == QStringLiteral("circle");
}

bool StyleManager::isPropertyAllowed(const QString &layerType,
                                     const QString &propertyName,
                                     bool layoutProperty) const {
    if (layoutProperty && propertyName == QStringLiteral("visibility")) {
        return false;
    }

    return hasAllowedPrefix(propertyName, supportedPrefixes(layerType));
}

QString StyleManager::layerType(const mbgl::style::Layer &layer) {
    return QString::fromUtf8(layer.getTypeInfo()->type);
}

bool StyleManager::hasAllowedPrefix(const QString &propertyName, const QStringList &prefixes) {
    for (const auto &prefix : prefixes) {
        if (propertyName.startsWith(prefix)) {
            return true;
        }
    }
    return false;
}

QHash<QString, bool> StyleManager::patchPropertyKinds(const QVariantMap &patch) {
    QHash<QString, bool> properties;
    const auto layout = patch.value(QStringLiteral("layout")).toMap();
    for (auto it = layout.cbegin(); it != layout.cend(); ++it) {
        properties.insert(it.key(), true);
    }
    const auto paint = patch.value(QStringLiteral("paint")).toMap();
    for (auto it = paint.cbegin(); it != paint.cend(); ++it) {
        properties.insert(it.key(), false);
    }
    return properties;
}

QVariantMap StyleManager::effectiveStatePatch(const QString &layerId) const {
    QVariantMap patch;
    QVariantMap layout;
    QVariantMap paint;
    const auto registered = m_registeredStateStyles.value(layerId);
    for (const auto &stateId : m_activeStateStyles.value(layerId)) {
        const auto statePatch = registered.value(stateId);
        const auto stateLayout = statePatch.value(QStringLiteral("layout")).toMap();
        for (auto it = stateLayout.cbegin(); it != stateLayout.cend(); ++it) {
            layout.insert(it.key(), it.value());
        }
        const auto statePaint = statePatch.value(QStringLiteral("paint")).toMap();
        for (auto it = statePaint.cbegin(); it != statePaint.cend(); ++it) {
            paint.insert(it.key(), it.value());
        }
    }
    if (!layout.isEmpty()) {
        patch.insert(QStringLiteral("layout"), layout);
    }
    if (!paint.isEmpty()) {
        patch.insert(QStringLiteral("paint"), paint);
    }
    return patch;
}

void StyleManager::captureTrackedProperties(const QString &layerId, const QVariantMap &patch) {
    const auto propertyKinds = patchPropertyKinds(patch);
    const auto snapshots = stylePropertySnapshot(layerId, QStringList(propertyKinds.keyBegin(), propertyKinds.keyEnd()));
    auto &tracked = m_trackedStateProperties[layerId];
    for (auto it = propertyKinds.cbegin(); it != propertyKinds.cend(); ++it) {
        if (tracked.contains(it.key())) {
            continue;
        }
        tracked.insert(it.key(), LayerStateTrackedProperty{it.value(), snapshots.value(it.key()).toMap()});
    }
}

bool StyleManager::applyActiveStateStyles(const QString &layerId) {
    const auto patch = effectiveStatePatch(layerId);
    const auto tracked = m_trackedStateProperties.value(layerId);
    bool success = true;
    for (auto it = tracked.cbegin(); it != tracked.cend(); ++it) {
        const QString section = it.value().layoutProperty ? QStringLiteral("layout") : QStringLiteral("paint");
        const auto sectionMap = patch.value(section).toMap();
        if (sectionMap.contains(it.key())) {
            success = applyTrackedPropertyValue(layerId, it.key(), it.value().layoutProperty, sectionMap.value(it.key())) && success;
            continue;
        }
        if (it.value().snapshot.contains(QStringLiteral("value"))) {
            success = applyTrackedPropertyValue(layerId,
                                                it.key(),
                                                it.value().layoutProperty,
                                                it.value().snapshot.value(QStringLiteral("value"))) && success;
        }
    }

    if (m_activeStateStyles.value(layerId).isEmpty()) {
        m_activeStateStyles.remove(layerId);
        m_trackedStateProperties.remove(layerId);
    }
    return success;
}

bool StyleManager::restoreLayerSnapshot(const QString &layerId, const QVariantMap &snapshot) {
    auto &style = m_map.getStyle();
    const auto layers = style.getLayers();

    std::optional<std::string> before;
    bool found = false;
    for (std::size_t index = 0; index < layers.size(); ++index) {
        if (QString::fromStdString(layers[index]->getID()) != layerId) {
            continue;
        }

        found = true;
        if (index + 1 < layers.size()) {
            before = layers[index + 1]->getID();
        }
        break;
    }

    if (!found) {
        setError(Map::StyleLayerNotFoundError, QStringLiteral("Layer not found: %1").arg(layerId));
        return false;
    }

    auto removedLayer = style.removeLayer(layerId.toStdString());
    if (!removedLayer) {
        setError(Map::StylePropertyApplyError,
                 QStringLiteral("Unable to remove layer %1 while restoring batch state.").arg(layerId));
        return false;
    }

    mbgl::style::conversion::Error error;
    auto restoredLayer = mbgl::style::conversion::convert<std::unique_ptr<mbgl::style::Layer>>(QVariant(snapshot), error);
    if (!restoredLayer) {
        style.addLayer(std::move(removedLayer), before);
        setError(Map::StylePropertyApplyError,
                 QStringLiteral("Unable to restore snapshot for layer %1: %2")
                     .arg(layerId, QString::fromStdString(error.message)));
        return false;
    }

    style.addLayer(std::move(*restoredLayer), before);
    return true;
}

bool StyleManager::applyTrackedPropertyValue(const QString &layerId,
                                             const QString &propertyName,
                                             bool layoutProperty,
                                             const QVariant &value) {
    return layoutProperty ? setLayoutProperty(layerId, propertyName, value)
                          : setPaintProperty(layerId, propertyName, value);
}

void StyleManager::onLayerRemoved(const QString &layerId) {
    m_activeStateStyles.remove(layerId);
    m_trackedStateProperties.remove(layerId);
    m_registeredStateStyles.remove(layerId);
}

void StyleManager::setError(Map::StyleErrorCode code, const QString &message) const {
    m_lastErrorCode = code;
    m_lastErrorMessage = message;
}

void StyleManager::clearError() const {
    m_lastErrorCode = Map::NoStyleError;
    m_lastErrorMessage.clear();
}

void StyleManager::emitLayoutPropertyChanged(const QString &layerId, const QString &propertyName) {
    if (!isBatchActive()) {
        emit layoutPropertyChanged(layerId, propertyName);
        return;
    }

    appendUnique(m_batchState.layoutChanges[layerId], propertyName);
}

void StyleManager::emitPaintPropertyChanged(const QString &layerId, const QString &propertyName) {
    if (!isBatchActive()) {
        emit paintPropertyChanged(layerId, propertyName);
        return;
    }

    appendUnique(m_batchState.paintChanges[layerId], propertyName);
}

void StyleManager::emitStylePatchApplied(const QString &layerId) {
    if (!isBatchActive()) {
        emit stylePatchApplied(layerId);
        return;
    }

    appendUnique(m_batchState.patchedLayers, layerId);
}

void StyleManager::flushBatchEvents() {
    for (auto it = m_batchState.layoutChanges.cbegin(); it != m_batchState.layoutChanges.cend(); ++it) {
        for (const auto &propertyName : it.value()) {
            emit layoutPropertyChanged(it.key(), propertyName);
        }
    }
    for (auto it = m_batchState.paintChanges.cbegin(); it != m_batchState.paintChanges.cend(); ++it) {
        for (const auto &propertyName : it.value()) {
            emit paintPropertyChanged(it.key(), propertyName);
        }
    }
    for (const auto &layerId : m_batchState.patchedLayers) {
        emit stylePatchApplied(layerId);
    }

    m_batchState.clear();
}

void StyleManager::appendUnique(QVector<QString> &values, const QString &value) {
    if (!values.contains(value)) {
        values.push_back(value);
    }
}

} // namespace QMapLibre