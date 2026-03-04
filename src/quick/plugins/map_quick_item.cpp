// Copyright (C) 2023 MapLibre contributors

// SPDX-License-Identifier: BSD-2-Clause

#include "map_quick_item.hpp"

#include "texture_node_base_p.hpp"
#ifdef MLN_RENDER_BACKEND_OPENGL
#include "texture_node_opengl_p.hpp"
#endif
#ifdef MLN_RENDER_BACKEND_METAL
#include "texture_node_metal_p.hpp"
#endif
#ifdef MLN_RENDER_BACKEND_VULKAN
#include "texture_node_vulkan_p.hpp"
#endif

#include <QMapLibre/Map>

#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGRectangleNode>
#ifdef MLN_RENDER_BACKEND_OPENGL
#include <QtGui/QOpenGLContext>
#endif

#include <memory>

namespace {
constexpr int minSize{64};
constexpr int intervalTime{250};

constexpr double minZoomLevel{0.0};
constexpr double maxZoomLevel{20.0};

bool isJsonStyleString(const QString &style) {
    const QString trimmed = style.trimmed();
    return trimmed.startsWith('{') || trimmed.startsWith('[');
}
} // namespace

namespace QMapLibre {

MapQuickItem::MapQuickItem(QQuickItem *parent)
    : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    // TODO: make configurable
    m_settings.setCacheDatabasePath(QStringLiteral(":memory:"));
}

void MapQuickItem::initialize() {
    if (m_map != nullptr) {
        return;
    }

    const QSize viewportSize{static_cast<int>(width()), static_cast<int>(height())};
    const qreal pixelRatio = window() != nullptr ? window()->devicePixelRatio() : 1.0;
    m_map = std::make_unique<Map>(nullptr, m_settings, viewportSize, pixelRatio);
    m_map->setConnectionEstablished();

    // Set default style
    if (!m_style.isEmpty()) {
        if (isJsonStyleString(m_style)) {
            m_map->setStyleJson(m_style);
        } else {
            m_map->setStyleUrl(m_style);
        }
    } else if (!m_settings.styles().empty() && !m_settings.styles().front().url.isEmpty()) {
        m_map->setStyleUrl(m_settings.styles().front().url);
    } else if (!m_settings.providerStyles().empty() && !m_settings.providerStyles().front().url.isEmpty()) {
        m_map->setStyleUrl(m_settings.providerStyles().front().url);
    }

    update();
}

void MapQuickItem::setStyle(const QString &style) {
    if (m_style == style) {
        return;
    }
    m_style = style;

    // 如果地图已初始化，立即应用新样式
    if (m_map != nullptr && !m_style.isEmpty()) {
        if (isJsonStyleString(m_style)) {
            m_map->setStyleJson(m_style);
        } else {
            m_map->setStyleUrl(m_style);
        }
        update();
    }
}

void MapQuickItem::setZoomLevel(double zoomLevel) {
    if (zoomLevel < minZoomLevel) {
        zoomLevel = minZoomLevel;
    } else if (zoomLevel > maxZoomLevel) {
        zoomLevel = maxZoomLevel;
    }

    if (m_zoomLevel == zoomLevel) {
        return;
    }

    m_zoomLevel = zoomLevel;

    if (m_map != nullptr) {
        m_syncState |= CameraOptionsSync;
        update();
    }

    emit zoomLevelChanged();
}

void MapQuickItem::setBearing(double bearing) {
    if (m_bearing == bearing) {
        return;
    }

    m_bearing = bearing;

    if (m_map != nullptr) {
        m_syncState |= CameraOptionsSync;
        update();
    }

    emit bearingChanged();
}

void MapQuickItem::setPitch(double pitch) {
    if (m_pitch == pitch) {
        return;
    }

    m_pitch = pitch;

    if (m_map != nullptr) {
        m_syncState |= CameraOptionsSync;
        update();
    }

    emit pitchChanged();
}

void MapQuickItem::setCoordinate(const QVariantList &coordinate) {
    if (m_coordinate == coordinate || coordinate.size() != 2) {
        return;
    }

    m_coordinate = coordinate;

    if (m_map != nullptr) {
        m_syncState |= CameraOptionsSync;
        update();
    }

    emit coordinateChanged();
}

void MapQuickItem::setCoordinateFromPixel(const QPointF &pixel) {
    if (m_map == nullptr) {
        return;
    }

    const Coordinate coordinate = m_map->coordinateForPixel(pixel);
    setCoordinate({coordinate.first, coordinate.second});
}

QVariantList MapQuickItem::coordinateForPixel(const QPointF &pixel) const {
    if (m_map == nullptr) {
        return {};
    }
    const Coordinate coordinate = m_map->coordinateForPixel(pixel);
    return {coordinate.first, coordinate.second};
}

void MapQuickItem::pan(const QPointF &offset) {
    if (m_map == nullptr) {
        return;
    }

    m_map->moveBy(offset);
    const Coordinate coordinate = m_map->coordinate();
    m_coordinate = {coordinate.first, coordinate.second};
    update();
    emit coordinateChanged();
}

void MapQuickItem::scale(double scale, const QPointF &center) {
    if (m_map == nullptr) {
        return;
    }

    m_map->scaleBy(scale, center);
    const Coordinate coordinate = m_map->coordinate();
    m_coordinate = {coordinate.first, coordinate.second};
    m_zoomLevel = m_map->zoom();
    update();
    emit coordinateChanged();
    emit zoomLevelChanged();
}

void MapQuickItem::componentComplete() {
    QQuickItem::componentComplete();

    QTimer::singleShot(intervalTime, this, &MapQuickItem::initialize);
}

void MapQuickItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);

    if (m_map == nullptr) {
        return;
    }

    if (newGeometry.size() != oldGeometry.size()) {
        const QSize viewportSize{static_cast<int>(newGeometry.width()), static_cast<int>(newGeometry.height())};
        const qreal pixelRatio = window() != nullptr ? window()->devicePixelRatio() : 1.0;
        m_map->resize(viewportSize.expandedTo({minSize, minSize}), pixelRatio);
        m_syncState |= ViewportSync;
        update();
    }
}

QSGNode *MapQuickItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) {
    Q_UNUSED(data);

    if (!m_map) {
        delete oldNode; // NOLINT(cppcoreguidelines-owning-memory)
        return nullptr;
    }

    auto *root = static_cast<QSGRectangleNode *>(oldNode);
    if (root == nullptr) {
        root = window()->createRectangleNode();
    }

    root->setRect(boundingRect());

    QSGNode *content = root->childCount() > 0 ? root->firstChild() : nullptr;
    content = updateMapNode(content);
    if (content != nullptr && root->childCount() == 0) {
        root->appendChildNode(content);
    }

    return root;
}

QSGNode *MapQuickItem::updateMapNode(QSGNode *node) {
    const QSize viewportSize{static_cast<int>(width()), static_cast<int>(height())};

    if (node == nullptr) {
#ifdef MLN_RENDERER_DEBUGGING
        qDebug() << "MapQuickItem::updatePaintNode() - Creating new node for size" << viewportSize;
#endif

#ifdef MLN_RENDER_BACKEND_OPENGL
        // OpenGL context check
        const QOpenGLContext *glContext = QOpenGLContext::currentContext();
        if (glContext == nullptr) {
            qWarning("QOpenGLContext is NULL!");
            // qWarning() << "You are running on QSG backend " << QSGContext::backend();
            qWarning("The MapLibre plugin works with both Desktop and ES 2.0+ OpenGL versions.");
            qWarning("Verify that your Qt is built with OpenGL, and what kind of OpenGL.");
            qWarning(
                "To force using a specific OpenGL version, check QSurfaceFormat::setRenderableType and "
                "QSurfaceFormat::setDefaultFormat");

            return node;
        }

        std::unique_ptr<TextureNodeBase> mbglNode = std::make_unique<TextureNodeOpenGL>(
            m_map, viewportSize, window()->devicePixelRatio());
#elif defined(MLN_RENDER_BACKEND_METAL)
        std::unique_ptr<TextureNodeBase> mbglNode = std::make_unique<TextureNodeMetal>(
            m_map, viewportSize, window()->devicePixelRatio());
#elif defined(MLN_RENDER_BACKEND_VULKAN)
        std::unique_ptr<TextureNodeBase> mbglNode = std::make_unique<TextureNodeVulkan>(
            m_map, viewportSize, window()->devicePixelRatio());
#endif
        QObject::connect(m_map.get(), &Map::needsRendering, this, &QQuickItem::update);
        QObject::connect(m_map.get(), &Map::mapChanged, this, &MapQuickItem::onMapChanged);
        QObject::connect(m_map.get(), &Map::mapLoadingFailed, this, [this](Map::MapLoadingFailure failure, const QString &reason) {
            qWarning() << "[MapQuickItem] mapLoadingFailed:" << failure << reason;
            QTimer::singleShot(intervalTime, this, &QQuickItem::update);
        });

        // 注入首帧回调：只有 mapFullyLoaded 打开开关后的下一次 render()，才发出 firstFrameReady
        // exchange(false) 保证每次全量吐只触发一次
        mbglNode->setFirstFrameCallback([this]() {
            const bool wasArmed = m_awaitFirstFrameAfterLoad.exchange(false);
            qDebug() << QDateTime::currentDateTime().toString("hh:mm:ss.zzz")
                     << "[MapQuickItem] firstFrameCallback: wasArmed=" << wasArmed;
            if (wasArmed) {
                QMetaObject::invokeMethod(this, &MapQuickItem::firstFrameReady, Qt::QueuedConnection);
            }
        });

        m_syncState = ViewportSync | CameraOptionsSync;

        node = mbglNode.release();

#ifdef MLN_RENDERER_DEBUGGING
        qDebug() << "MapQuickItem::updatePaintNode() - Created new node" << node;
#endif
    }

    if ((m_syncState & CameraOptionsSync) != 0) {
        m_map->setCoordinateZoom({m_coordinate.size() == 2 ? m_coordinate[0].toDouble() : 0.0,
                                  m_coordinate.size() == 2 ? m_coordinate[1].toDouble() : 0.0},
                                 m_zoomLevel);
        m_map->setBearing(m_bearing);
        m_map->setPitch(m_pitch);
    }

    if ((m_syncState & ViewportSync) != 0) {
        static_cast<TextureNodeBase *>(node)->resize(viewportSize, window()->devicePixelRatio(), window());
    }

    static_cast<TextureNodeBase *>(node)->render(window());

    m_syncState = NoSync;

    return node;
}

void MapQuickItem::onMapChanged(Map::MapChange change) {
    qDebug() << QDateTime::currentDateTime().toString("hh:mm:ss.zzz")
             << "[MapQuickItem] mapChanged:" << static_cast<int>(change)
             << "isFullyLoaded=" << (m_map ? m_map->isFullyLoaded() : false)
             << "expectingFirst=" << m_expectingFirstFrame
             << "awaitArmed=" << m_awaitFirstFrameAfterLoad.load();

    if (change == Map::MapChangeDidFinishLoadingStyle) {
        emit styleLoaded();
    } else if (change == Map::MapChangeWillStartLoadingMap) {
        // 新样式开始加载：重置"允许 arm 一次"标志，准备接受下一次首帧信号
        m_expectingFirstFrame = true;
        qDebug() << QDateTime::currentDateTime().toString("hh:mm:ss.zzz")
                 << "[MapQuickItem] WillStartLoadingMap -> expectingFirstFrame=true";
    } else if (change == Map::MapChangeDidFinishRenderingMapFullyRendered) {
        // 只有 isFullyLoaded=true 且本次样式变化尚未 arm 过门控时才开闸，
        // 防止快速底图多批次瓦片触发多次 arm → 多次 firstFrameReady → 淡出期间粉色透出
        if (m_map && m_map->isFullyLoaded() && m_expectingFirstFrame) {
            m_expectingFirstFrame = false;
            m_awaitFirstFrameAfterLoad = true;
            qDebug() << QDateTime::currentDateTime().toString("hh:mm:ss.zzz")
                     << "[MapQuickItem] GATE ARMED (isFullyLoaded=true, expectingFirst was true)";
            emit mapFullyLoaded();
        }
    }

    if (change == Map::MapChangeDidFinishLoadingMap || change == Map::MapChangeDidFinishLoadingStyle || change == Map::MapChangeDidFinishRenderingMapFullyRendered) {
        // TODO: make it more elegant
        QTimer::singleShot(intervalTime, this, &QQuickItem::update);
    } else if (change == Map::MapChangeDidFailLoadingMap) {
        qWarning() << "[MapQuickItem] MapChangeDidFailLoadingMap - Network or parse error.";
        QTimer::singleShot(intervalTime, this, &QQuickItem::update);
    }
}

} // namespace QMapLibre
