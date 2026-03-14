# 文档

本文档介绍 MapLibre Native Qt 公开绑定层的 API。
通常情况下，您会在 Qt 应用中使用此 API。
如果您对 MapLibre Native 内部实现感兴趣，也可以查阅
[核心 C++ API](https://maplibre.org/maplibre-native/cpp/api/)。

除非特别说明，所有类和函数均在库的 3.0 版本起可用。

源代码托管于 [GitHub](https://github.com/maplibre/maplibre-native-qt)：

```shell
git clone https://github.com/maplibre/maplibre-native-qt.git
cd maplibre-native-qt
git submodule update --init --recursive
```

@note 在某些文件系统上，子模块更新可能会出现 `Filename too long`（文件名过长）错误，
可以忽略，仅少量依赖的测试数据会缺失。

---

## 引擎概述

MapLibre Native Qt 是对 [MapLibre Native](https://github.com/maplibre/maplibre-native)
渲染引擎的 Qt 封装层。它通过 Qt 友好的 C++ 类型、与 Qt 事件循环的深度集成以及可选的 QML 绑定，
将引擎的全部功能暴露给上层应用。

### 主要功能

- **矢量瓦片渲染** — 完整支持 Mapbox GL / MapLibre 样式规范，支持矢量、栅格、GeoJSON 和图像数据源。
- **多渲染后端** — OpenGL（Linux、Windows、Android），Vulkan（Linux、Windows、Android），
  Metal（macOS、iOS），以及通过 Emscripten 的 WebGL 2（WASM）。
- **交互式相机** — 平移、缩放、旋转、倾斜地图，支持平滑动画过渡。
- **标注 API** — 运行时添加点标注、线标注和面标注。
- **样式图层管理** — 运行时添加、删除和修改样式图层及其布局/绘制属性。
- **数据源管理** — 运行时添加、更新和删除 GeoJSON 及图像数据源。
- **自定义 GL 图层** — 通过 `CustomLayerHostInterface` 将您自己的 OpenGL / Metal / Vulkan
  渲染注入地图。
- **坐标工具** — 在地理坐标、屏幕像素坐标和投影米坐标之间互相转换，并查询地图比例尺。
- **网络模式** — 在线/离线模式切换。
- **Qt Widgets 集成** — `QMapLibre::MapWidget` 是 `QRhiWidget` 子类，自动处理渲染循环并提供鼠标事件信号。
- **Qt Quick / QML 集成** — `MapLibre` QML 类型将地图作为 `QQuickItem` 公开，
  包含可绑定属性和额外的生命周期信号。
- **Qt Location 插件** — ID 为 `maplibre` 的地理服务插件，可与 Qt Location 的 `Map` QML 类型
  及配套的 `Style`、`SourceParameter`、`LayerParameter` 辅助类协同使用。
- **静态渲染** — 在无显示设备的情况下将地图渲染为图片（适用于瓦片生成和测试）。

---

## 库组件

本库拆分为三个 CMake / pkg-config 组件：

| 组件                   | 目标                    | 说明 |
|------------------------|-------------------------|------|
| **Core**               | `QMapLibre::Core`       | 包含 `QMapLibre::Map`、`Settings`、数据类型及工具函数，始终必需。 |
| **Location**           | `QMapLibre::Location`   | Qt Location 地理服务插件（`maplibre`），需要 Qt Location 模块。 |
| **Widgets**            | `QMapLibre::Widgets`    | 基于 `QRhiWidget` 的 `QMapLibre::MapWidget`，需要 Qt Widgets 模块。 |

`MapLibre` QML 模块（安装在 `qml/MapLibre` 目录下）随 **Location** 组件一同提供，
链接 `QMapLibre::Location` 后即可使用。

---

## 核心 API — `QMapLibre::Map`

`QMapLibre::Map` 是核心类，封装了底层 MapLibre Native 地图，并通过 Qt 类型公开其完整 API。

### 视口 / 相机

| 方法 | 说明 |
|------|------|
| `latitude()` / `setLatitude()` | 地图中心的地理纬度 |
| `longitude()` / `setLongitude()` | 地图中心的地理经度 |
| `coordinate()` / `setCoordinate()` | 以 `(纬度, 经度)` 对表示的地图中心 |
| `zoom()` / `setZoom()` | 当前缩放级别 |
| `minimumZoom()` / `maximumZoom()` | 缩放级别边界 |
| `scale()` / `setScale()` | 相对于指定屏幕点的地图比例 |
| `bearing()` / `setBearing()` | 地图旋转角度（度） |
| `pitch()` / `setPitch()` | 相机俯仰角（度） |
| `northOrientation()` / `setNorthOrientation()` | 地理北方对应的屏幕方向 |
| `margins()` / `setMargins()` | 内边距；有效视口中心会相应偏移 |
| `jumpTo(CameraOptions)` | 立即移动相机（中心、缩放、旋转、俯仰） |
| `setCoordinateZoom()` | 一次性设置中心和缩放级别 |
| `setTransitionOptions()` | 相机动画过渡的持续时间和延迟 |
| `setGestureInProgress()` | 通知引擎正在进行用户手势操作 |
| `moveBy()` / `scaleBy()` / `rotateBy()` | 应用增量式相机变换 |

### 样式

| 方法 | 说明 |
|------|------|
| `styleUrl()` / `setStyleUrl()` | 从 URL 加载样式（HTTP、`asset://`、`file://`） |
| `styleJson()` / `setStyleJson()` | 从内联 JSON 字符串加载样式 |

### 数据源

| 方法 | 说明 |
|------|------|
| `addSource(id, params)` | 添加数据源（GeoJSON、图像、矢量瓦片等） |
| `sourceExists(id)` | 检查指定 ID 的数据源是否存在 |
| `updateSource(id, params)` | 更新已有数据源的数据或属性 |
| `removeSource(id)` | 删除数据源 |

### 图层

| 方法 | 说明 |
|------|------|
| `addLayer(id, params, before)` | 添加样式图层 |
| `addCustomLayer(id, host, before)` | 通过 `CustomLayerHostInterface` 添加自定义渲染图层 |
| `layerExists(id)` | 检查指定 ID 的图层是否存在 |
| `removeLayer(id)` | 删除图层 |
| `layerIds()` | 返回当前样式中所有图层 ID 的有序列表 |
| `setLayoutProperty(layerId, name, value)` | 设置图层的布局属性 |
| `setPaintProperty(layerId, name, value)` | 设置图层的绘制属性 |
| `setFilter(layerId, filter)` | 为图层设置要素过滤表达式 |
| `getFilter(layerId)` | 获取图层当前的过滤表达式 |

### 图像

| 方法 | 说明 |
|------|------|
| `addImage(id, QImage)` | 注册一张雪碧图，供符号图层使用 |
| `removeImage(id)` | 删除已注册的雪碧图 |

### 标注

标注是渲染在样式之上的轻量几何对象。

| 方法 | 说明 |
|------|------|
| `addAnnotationIcon(name, QImage)` | 注册用于点标注的图标 |
| `addAnnotation(Annotation)` | 添加标注，返回其 `AnnotationID` |
| `updateAnnotation(id, Annotation)` | 更新已有标注 |
| `removeAnnotation(id)` | 删除标注 |

`Annotation` 类型是一个 `QVariant`，内容为以下之一：
- `QMapLibre::SymbolAnnotation` — 带图标的点标注
- `QMapLibre::LineAnnotation` — 带颜色、宽度和透明度的折线标注
- `QMapLibre::FillAnnotation` — 带填充色、轮廓色和透明度的面标注

### 坐标工具

| 方法 | 说明 |
|------|------|
| `pixelForCoordinate(coord)` | 将地理坐标映射为屏幕像素坐标 |
| `coordinateForPixel(pixel)` | 将屏幕像素坐标映射为地理坐标 |
| `coordinateZoomForBounds(sw, ne)` | 返回适合给定边界框的中心点和缩放级别 |

### 渲染控制

| 方法 | 说明 |
|------|------|
| `resize(size, pixelRatio)` | 通知引擎视口尺寸已变化 |
| `render()` （槽） | 推进一帧渲染 |
| `triggerRepaint()` （槽） | 调度重绘 |
| `isFullyLoaded()` | 当前视口内所有瓦片和资源加载完毕时返回 `true` |
| `startStaticRender()` （槽） | 开始静态（离屏）渲染，完成后发出 `staticRenderFinished` 信号 |
| `needsRendering` （信号） | 地图状态变化、需要渲染新帧时发出 |
| `mapChanged(MapChange)` （信号） | 每次地图状态转换（加载、渲染等）时发出 |
| `mapLoadingFailed(MapLoadingFailure, reason)` （信号） | 样式或瓦片加载失败时发出 |
| `copyrightsChanged(html)` （信号） | 版权归属字符串变化时发出 |

---

## 配置类 — `QMapLibre::Settings`

`QMapLibre::Settings` 传入 `Map` 构造函数，控制引擎的初始化方式。

| 属性 | 说明 |
|------|------|
| `contextMode` | `UniqueGLContext`（默认）或 `SharedGLContext` |
| `mapMode` | `Continuous`（交互式渲染）或 `Static`（离屏渲染） |
| `constrainMode` | 是否将地图约束在有效纬经度范围内 |
| `viewportMode` | `DefaultViewport` 或 `FlippedYViewport`（用于 Y 轴翻转的帧缓冲） |
| `cacheDatabasePath` | 瓦片缓存 SQLite 数据库路径（`":memory:"` 表示不持久化） |
| `cacheDatabaseMaximumSize` | 瓦片缓存最大字节数 |
| `assetPath` | `asset://` URL 资源的根目录 |
| `apiKey` | 服务商 API 密钥（用于 MapLibre、MapTiler、Mapbox 提供商模板） |
| `apiBaseUrl` | 覆盖瓦片服务器基础 URL |
| `localFontFamily` | 当远程字体堆栈中找不到字形时使用的本地字体族 |
| `clientName` / `clientVersion` | 在 HTTP `User-Agent` 请求头中上报 |
| `resourceTransform` | 在请求资源前对 URL 进行重写的回调函数 |
| `providerTemplate` | 内置服务商预设：`NoProvider`、`MapLibreProvider`、`MapTilerProvider`、`MapboxProvider` |
| `styles` | 向用户展示的 `QMapLibre::Style` 列表（例如由 Qt Location 使用） |
| `defaultCoordinate` / `defaultZoom` | 初始相机位置 |

---

## 数据类型 — `QMapLibre::Types`

### 坐标类型

| 类型 | 别名 | 说明 |
|------|------|------|
| `Coordinate` | `QPair<double, double>` | `(纬度, 经度)` |
| `CoordinateZoom` | `QPair<Coordinate, double>` | 中心点 + 缩放级别 |
| `ProjectedMeters` | `QPair<double, double>` | 投影坐标系中的北向距 + 东向距（单位：米） |
| `Coordinates` | `QVector<Coordinate>` | 坐标序列 |
| `CoordinatesCollection` | `QVector<Coordinates>` | 例如多边形的环列表 |
| `CoordinatesCollections` | `QVector<CoordinatesCollection>` | 例如多多边形 |

### 样式（Style）

`QMapLibre::Style` 表示一个具名地图样式，包含 URL、可选描述以及与 Qt Location 样式类型枚举
对应的 `Type` 字段。

### 地物（Feature）

`QMapLibre::Feature` 表示类 GeoJSON 几何对象（点、线串或多边形），
带有任意 `QVariantMap` 属性和可选 ID。

### 标注类型

- `QMapLibre::SymbolAnnotation` — 单点处的符号标注
- `QMapLibre::LineAnnotation` — 带样式的折线标注
- `QMapLibre::FillAnnotation` — 带样式的面标注

### 相机选项（CameraOptions）

`QMapLibre::CameraOptions` 将相机参数（中心点、锚点、缩放、旋转、俯仰）
以可选 `QVariant` 的形式组合在一起；未设置的字段在传入 `Map::jumpTo()` 时不会改变对应的相机参数。

### 自定义图层

`QMapLibre::CustomLayerHostInterface` 是向地图注入自定义渲染的纯虚接口。
实现 `initialize()`、`render(CustomLayerRenderParameters)` 和 `deinitialize()` 方法，
然后将 `std::unique_ptr` 传入 `Map::addCustomLayer()`。

---

## 工具函数 — `QMapLibre::Utils`

| 符号 | 说明 |
|------|------|
| `supportedRendererType()` | 返回库编译时使用的 `RendererType`（`OpenGL`、`Vulkan` 或 `Metal`） |
| `networkMode()` / `setNetworkMode()` | 全局获取/设置在线与离线模式 |
| `metersPerPixelAtLatitude(lat, zoom)` | 在给定纬度和缩放级别下每像素对应的地面距离（米） |
| `projectedMetersForCoordinate(coord)` | 将地理坐标转换为投影米坐标 |
| `coordinateForProjectedMeters(pm)` | 将投影米坐标转换回地理坐标 |

---

## 小部件 — `QMapLibre::MapWidget`

`QMapLibre::MapWidget` 是基于 `QRhiWidget`（Qt 6.7+）的全交互地图控件。
它自动处理 OpenGL / Vulkan / Metal 渲染循环，并将鼠标事件转发给底层 `Map`。

```cpp
QMapLibre::Settings settings;
settings.setStyleUrl("https://demotiles.maplibre.org/style.json");

auto *widget = new QMapLibre::MapWidget(settings);
widget->show();
```

交互信号：

| 信号 | 说明 |
|------|------|
| `onMousePressEvent(Coordinate)` | 鼠标左键按下 |
| `onMouseReleaseEvent(Coordinate)` | 鼠标左键释放 |
| `onMouseMoveEvent(Coordinate)` | 鼠标光标移动 |
| `onMouseDoubleClickEvent(Coordinate)` | 鼠标左键双击 |

通过 `widget->map()` 访问底层 `Map`，可以以编程方式自定义样式、数据源、图层或标注。

---

## QML — `MapLibre` QML 类型

`MapLibre` QML 类型（`QML_NAMED_ELEMENT(MapLibre)`）是 `QQuickItem` 的子类，
可在 Qt Quick 场景中渲染地图。

### 属性

| 属性 | 类型 | 说明 |
|------|------|------|
| `style` | `string` | 样式 URL 或内联 JSON 样式字符串 |
| `coordinate` | `list<real>` | 地图中心的 `[纬度, 经度]` |
| `zoomLevel` | `real` | 当前缩放级别（0 – 20） |
| `bearing` | `real` | 地图旋转角度（度） |
| `pitch` | `real` | 相机俯仰角（度） |

### 方法

| 方法 | 说明 |
|------|------|
| `pan(offset: point)` | 按屏幕像素偏移量平移地图 |
| `scale(scale: real, center: point)` | 围绕屏幕锚点缩放地图 |
| `setCoordinateFromPixel(pixel: point)` | 将地图中心移动到指定像素下方的地理坐标 |
| `coordinateForPixel(pixel: point) : list<real>` | 返回指定像素下方的 `[纬度, 经度]` |
| `getNativeMap() : QObject*` | 返回底层 `QMapLibre::Map` 对象，供高级用途使用 |

### 信号

| 信号 | 说明 |
|------|------|
| `coordinateChanged()` | 地图中心变化时发出 |
| `zoomLevelChanged()` | 缩放级别变化时发出 |
| `bearingChanged()` | 旋转角度变化时发出 |
| `pitchChanged()` | 俯仰角变化时发出 |
| `styleLoaded()` | 样式 JSON 下载并解析完毕后发出 |
| `mapFullyLoaded()` | 当前视口内所有瓦片加载完成后发出 |
| `firstFrameReady()` | Qt 场景图合成出第一帧完整画面后发出——适合在此时机安全移除加载遮罩，避免闪屏 |

### 最小 QML 示例

```qml
import QtQuick

MapLibre {
    anchors.fill: parent
    style: "https://demotiles.maplibre.org/style.json"
    coordinate: [48.8566, 2.3522]   // 巴黎
    zoomLevel: 10

    onMapFullyLoaded: console.log("地图已就绪")
}
```

---

## Qt Location 插件

`maplibre` Qt Location 地理服务插件可与 Qt Location 的 `Map` QML 类型集成，
插件 ID 为 `"maplibre"`。

```qml
Plugin {
    id: mapPlugin
    name: "maplibre"
    PluginParameter { name: "maplibre.map.styles"; value: "https://demotiles.maplibre.org/style.json" }
}

Map {
    plugin: mapPlugin
    center: QtPositioning.coordinate(48.8566, 2.3522)
    zoomLevel: 10
}
```

还可以通过 `import MapLibre.Location 3.0` 中的 `MapLibre.style`、`SourceParameter` 和
`LayerParameter` 对样式进行进一步定制（添加数据源和图层）。

---

<div class="section_buttons">

|                        Next |
|----------------------------:|
| [How to build](Building.md) |

</div>
