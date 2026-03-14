# Documentation

This is the documentation of the public MapLibre Native bindings for Qt.
You would normally use this API in a Qt-based application.
If you are interested in the internals of MapLibre Native you can also look at
[Core C++ API](https://maplibre.org/maplibre-native/cpp/api/).

If not explicitly specified, all classes and functions are available
as of version 3.0 of the library.

The source code can be found [on GitHub](https://github.com/maplibre/maplibre-native-qt):

```shell
git clone https://github.com/maplibre/maplibre-native-qt.git
cd maplibre-native-qt
git submodule update --init --recursive
```

@note On some file systems, the submodule update may yield a `Filename too long`
error. It can be ignored as only some dependencies test data will be missing.

---

## Engine Overview

MapLibre Native Qt is a Qt wrapper around the [MapLibre Native](https://github.com/maplibre/maplibre-native)
rendering engine. It exposes the engine's functionality through Qt-friendly C++ types, deep
integration with the Qt event loop, and optional QML bindings.

### Key Features

- **Vector tile rendering** — displays Mapbox GL / MapLibre style specifications with full
  support for vector, raster, GeoJSON and image sources.
- **Multiple rendering backends** — OpenGL (Linux, Windows, Android), Vulkan (Linux, Windows,
  Android), Metal (macOS, iOS) and WebGL 2 via Emscripten (WASM).
- **Interactive camera** — pan, zoom, rotate and pitch the map; smooth animated transitions.
- **Annotation API** — add symbol, line and fill annotations at runtime.
- **Style layer management** — add, remove and modify style layers and their layout / paint
  properties at runtime.
- **Data source management** — add, update and remove GeoJSON and image sources at runtime.
- **Custom GL layers** — inject your own OpenGL / Metal / Vulkan rendering through a
  `CustomLayerHostInterface`.
- **Coordinate utilities** — convert between geographic coordinates, pixel positions and
  projected metres; query map scale.
- **Network mode** — toggle between online and offline operation.
- **Qt Widgets integration** — `QMapLibre::MapWidget` is a `QRhiWidget` subclass that provides
  mouse-event signals and handles the render loop automatically.
- **Qt Quick / QML integration** — `MapLibre` QML type exposes the map as a `QQuickItem`
  with bindable properties and additional lifecycle signals.
- **Qt Location plugin** — a `maplibre` geo-services plugin that works with Qt Location's
  `Map` QML type and its accompanying `Style`, `SourceParameter` and `LayerParameter` helpers.
- **Static rendering** — render the map to an image without a display (useful for tile
  generation and testing).

---

## Library Components

The library is split into three CMake / pkg-config components:

| Component              | Target                  | Description |
|------------------------|-------------------------|-------------|
| **Core**               | `QMapLibre::Core`       | `QMapLibre::Map`, `Settings`, types, utilities. Always required. |
| **Location**           | `QMapLibre::Location`   | Qt Location geo-services plugin (`maplibre`). Requires Qt Location. |
| **Widgets**            | `QMapLibre::Widgets`    | `QMapLibre::MapWidget` (`QRhiWidget`-based widget). Requires Qt Widgets. |

The `MapLibre` QML module (installed under `qml/MapLibre`) is bundled with the
**Location** component and is available when `QMapLibre::Location` is linked.

---

## Core API — `QMapLibre::Map`

`QMapLibre::Map` is the central class. It wraps the underlying MapLibre Native map and
exposes its full API through Qt types.

### Viewport / Camera

| Method | Description |
|--------|-------------|
| `latitude()` / `setLatitude()` | Geographic latitude of the map centre |
| `longitude()` / `setLongitude()` | Geographic longitude of the map centre |
| `coordinate()` / `setCoordinate()` | Centre as a `(latitude, longitude)` pair |
| `zoom()` / `setZoom()` | Current zoom level |
| `minimumZoom()` / `maximumZoom()` | Zoom level bounds |
| `scale()` / `setScale()` | Map scale relative to a given screen point |
| `bearing()` / `setBearing()` | Map rotation in degrees |
| `pitch()` / `setPitch()` | Camera tilt in degrees |
| `northOrientation()` / `setNorthOrientation()` | Which screen edge geographic north maps to |
| `margins()` / `setMargins()` | Inset margins; the effective viewport centre is shifted accordingly |
| `jumpTo(CameraOptions)` | Instantly move the camera (centre, zoom, bearing, pitch) |
| `setCoordinateZoom()` | Set centre and zoom in one call |
| `setTransitionOptions()` | Duration and delay for animated camera transitions |
| `setGestureInProgress()` | Notify the engine that a user gesture is in progress |
| `moveBy()` / `scaleBy()` / `rotateBy()` | Apply incremental camera changes |

### Style

| Method | Description |
|--------|-------------|
| `styleUrl()` / `setStyleUrl()` | Load a style from a URL (HTTP, `asset://`, `file://`) |
| `styleJson()` / `setStyleJson()` | Load a style from an inline JSON string |

### Sources

| Method | Description |
|--------|-------------|
| `addSource(id, params)` | Add a data source (GeoJSON, image, vector, …) |
| `sourceExists(id)` | Test whether a source with the given ID exists |
| `updateSource(id, params)` | Update the data or properties of an existing source |
| `removeSource(id)` | Remove a source |

### Layers

| Method | Description |
|--------|-------------|
| `addLayer(id, params, before)` | Add a style layer |
| `addCustomLayer(id, host, before)` | Add a custom rendering layer via `CustomLayerHostInterface` |
| `layerExists(id)` | Test whether a layer with the given ID exists |
| `removeLayer(id)` | Remove a layer |
| `layerIds()` | Return the ordered list of all layer IDs in the current style |
| `setLayoutProperty(layerId, name, value)` | Set a layout property on a layer |
| `setPaintProperty(layerId, name, value)` | Set a paint property on a layer |
| `setFilter(layerId, filter)` | Set a feature filter expression on a layer |
| `getFilter(layerId)` | Get the current filter expression of a layer |

### Images

| Method | Description |
|--------|-------------|
| `addImage(id, QImage)` | Register a sprite image for use in symbol layers |
| `removeImage(id)` | Remove a sprite image |

### Annotations

Annotations are lightweight geometry objects that are rendered on top of the style.

| Method | Description |
|--------|-------------|
| `addAnnotationIcon(name, QImage)` | Register an icon for use with symbol annotations |
| `addAnnotation(Annotation)` | Add an annotation; returns its `AnnotationID` |
| `updateAnnotation(id, Annotation)` | Update an existing annotation |
| `removeAnnotation(id)` | Remove an annotation |

The `Annotation` type is a `QVariant` holding one of:
- `QMapLibre::SymbolAnnotation` — a point with an icon
- `QMapLibre::LineAnnotation` — a polyline with colour, width and opacity
- `QMapLibre::FillAnnotation` — a polygon with fill colour, outline colour and opacity

### Coordinate Utilities

| Method | Description |
|--------|-------------|
| `pixelForCoordinate(coord)` | Map a geographic coordinate to a screen pixel |
| `coordinateForPixel(pixel)` | Map a screen pixel to a geographic coordinate |
| `coordinateZoomForBounds(sw, ne)` | Return the centre and zoom that fit a bounding box |

### Rendering Control

| Method | Description |
|--------|-------------|
| `resize(size, pixelRatio)` | Notify the engine of a viewport size change |
| `render()` (slot) | Advance one rendering frame |
| `triggerRepaint()` (slot) | Schedule a repaint |
| `isFullyLoaded()` | Return `true` when all tiles and resources for the current viewport are loaded |
| `startStaticRender()` (slot) | Begin a static (off-screen) render; emits `staticRenderFinished` when done |
| `needsRendering` (signal) | Emitted when the map state has changed and a new frame should be rendered |
| `mapChanged(MapChange)` (signal) | Emitted on each map state transition (loading, rendering, …) |
| `mapLoadingFailed(MapLoadingFailure, reason)` (signal) | Emitted when a style or tile fails to load |
| `copyrightsChanged(html)` (signal) | Emitted when the attribution string changes |

---

## Settings — `QMapLibre::Settings`

`QMapLibre::Settings` is passed to the `Map` constructor and controls how the engine is
initialised.

| Property | Description |
|----------|-------------|
| `contextMode` | `UniqueGLContext` (default) or `SharedGLContext` |
| `mapMode` | `Continuous` (interactive rendering) or `Static` (off-screen rendering) |
| `constrainMode` | Whether to constrain the map to valid latitude / longitude ranges |
| `viewportMode` | `DefaultViewport` or `FlippedYViewport` (for framebuffers with reversed Y) |
| `cacheDatabasePath` | Path to the tile cache SQLite database (`":memory:"` for no persistence) |
| `cacheDatabaseMaximumSize` | Maximum tile cache size in bytes |
| `assetPath` | Base directory for `asset://` URL resources |
| `apiKey` | Provider API key (used by MapLibre, MapTiler and Mapbox provider templates) |
| `apiBaseUrl` | Override the tile server base URL |
| `localFontFamily` | Font family used when a glyph is not found in the remote font stack |
| `clientName` / `clientVersion` | Reported in HTTP `User-Agent` headers |
| `resourceTransform` | Callback to rewrite resource URLs before they are fetched |
| `providerTemplate` | Convenience preset: `NoProvider`, `MapLibreProvider`, `MapTilerProvider`, `MapboxProvider` |
| `styles` | List of `QMapLibre::Style` objects shown to the user (e.g. by Qt Location) |
| `defaultCoordinate` / `defaultZoom` | Initial camera position |

---

## Types — `QMapLibre::Types`

### Coordinate types

| Type | Alias for | Description |
|------|-----------|-------------|
| `Coordinate` | `QPair<double, double>` | `(latitude, longitude)` |
| `CoordinateZoom` | `QPair<Coordinate, double>` | Centre + zoom level |
| `ProjectedMeters` | `QPair<double, double>` | Northing + easting in projected metres |
| `Coordinates` | `QVector<Coordinate>` | Sequence of coordinates |
| `CoordinatesCollection` | `QVector<Coordinates>` | E.g. a polygon ring list |
| `CoordinatesCollections` | `QVector<CoordinatesCollection>` | E.g. a multi-polygon |

### Style

`QMapLibre::Style` represents a named map style with a URL, an optional description, and a
`Type` that maps to the Qt Location style type enumeration.

### Feature

`QMapLibre::Feature` represents a GeoJSON-like geometry (point, linestring or polygon)
with arbitrary `QVariantMap` properties and an optional ID.

### Annotation types

- `QMapLibre::SymbolAnnotation` — a symbol at a single coordinate
- `QMapLibre::LineAnnotation` — a styled polyline
- `QMapLibre::FillAnnotation` — a styled polygon

### Camera

`QMapLibre::CameraOptions` groups the camera parameters (centre, anchor, zoom, bearing,
pitch) as optional `QVariant` values; unset fields leave the corresponding camera
parameter unchanged when passed to `Map::jumpTo()`.

### Custom layer

`QMapLibre::CustomLayerHostInterface` is a pure-virtual interface for injecting custom
rendering into the map. Implement `initialize()`, `render(CustomLayerRenderParameters)`,
and `deinitialize()`, then pass a `std::unique_ptr` to `Map::addCustomLayer()`.

---

## Utilities — `QMapLibre::Utils`

| Symbol | Description |
|--------|-------------|
| `supportedRendererType()` | Returns the `RendererType` (`OpenGL`, `Vulkan`, or `Metal`) the library was built with |
| `networkMode()` / `setNetworkMode()` | Get / set online vs offline operation globally |
| `metersPerPixelAtLatitude(lat, zoom)` | Ground resolution at a given latitude and zoom level |
| `projectedMetersForCoordinate(coord)` | Convert a geographic coordinate to projected metres |
| `coordinateForProjectedMeters(pm)` | Convert projected metres back to a geographic coordinate |

---

## Widgets — `QMapLibre::MapWidget`

`QMapLibre::MapWidget` is a `QRhiWidget` (Qt 6.7+) that embeds a fully interactive map.
It handles the OpenGL / Vulkan / Metal render loop and forwards mouse events to the
underlying `Map`.

```cpp
QMapLibre::Settings settings;
settings.setStyleUrl("https://demotiles.maplibre.org/style.json");

auto *widget = new QMapLibre::MapWidget(settings);
widget->show();
```

Signals emitted for user interaction:

| Signal | Description |
|--------|-------------|
| `onMousePressEvent(Coordinate)` | Left mouse button pressed |
| `onMouseReleaseEvent(Coordinate)` | Left mouse button released |
| `onMouseMoveEvent(Coordinate)` | Mouse cursor moved |
| `onMouseDoubleClickEvent(Coordinate)` | Left mouse button double-clicked |

Access the underlying `Map` with `widget->map()` to customise styles, sources, layers or
annotations programmatically.

---

## QML — `MapLibre` QML Type

The `MapLibre` QML type (`QML_NAMED_ELEMENT(MapLibre)`) is a `QQuickItem` subclass that
renders the map inside a Qt Quick scene.

### Properties

| Property | Type | Description |
|----------|------|-------------|
| `style` | `string` | Style URL or inline JSON style string |
| `coordinate` | `list<real>` | `[latitude, longitude]` of the map centre |
| `zoomLevel` | `real` | Current zoom level (0 – 20) |
| `bearing` | `real` | Map rotation in degrees |
| `pitch` | `real` | Camera tilt in degrees |

### Methods

| Method | Description |
|--------|-------------|
| `pan(offset: point)` | Pan the map by a screen-space pixel offset |
| `scale(scale: real, center: point)` | Scale the map around a screen-space anchor point |
| `setCoordinateFromPixel(pixel: point)` | Re-centre the map on the coordinate under a pixel |
| `coordinateForPixel(pixel: point) : list<real>` | Return the `[lat, lon]` under a pixel |
| `getNativeMap() : QObject*` | Return the underlying `QMapLibre::Map` for advanced use |

### Signals

| Signal | Description |
|--------|-------------|
| `coordinateChanged()` | Emitted when the map centre changes |
| `zoomLevelChanged()` | Emitted when the zoom level changes |
| `bearingChanged()` | Emitted when the bearing changes |
| `pitchChanged()` | Emitted when the pitch changes |
| `styleLoaded()` | Emitted once the style JSON has been downloaded and parsed |
| `mapFullyLoaded()` | Emitted when all tiles for the current viewport have finished loading |
| `firstFrameReady()` | Emitted after the first complete frame is composited by the Qt scene graph — suitable for removing loading overlays without visual flash |

### Minimal QML example

```qml
import QtQuick

MapLibre {
    anchors.fill: parent
    style: "https://demotiles.maplibre.org/style.json"
    coordinate: [48.8566, 2.3522]   // Paris
    zoomLevel: 10

    onMapFullyLoaded: console.log("Map ready")
}
```

---

## Qt Location Plugin

The `maplibre` Qt Location geo-services plugin integrates with Qt Location's `Map` QML
type. It is identified by the plugin ID `"maplibre"`.

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

Additional style customisation (sources and layers) can be attached using
`MapLibre.style`, `SourceParameter` and `LayerParameter` from
`import MapLibre.Location 3.0`.

---

<div class="section_buttons">

|                        Next |
|----------------------------:|
| [How to build](Building.md) |

</div>
