# Cesium 离线地图需求与技术阅读清单

> 更新时间：2026-09-09
> 目标：UE 5.7 在在线/本地地图源之间切换。当前实现支持运行时读取 `[CesiumTileServer]`，但本仓库不包含离线地图数据集，也没有把视觉对齐当作已验收事实。

## 1. 当前代码和配置事实

权威入口：

- `Config/DefaultEngine.ini`
- `Source/UE5DroneControl/DroneOps/Control/DroneOpsGameMode.cpp`
- `Source/UE5DroneControl/DroneOps/Core/CesiumCoordinateService.cpp`

当前默认配置：

```ini
[CesiumTileServer]
UseLocalTileServer=false
SwitchTilesetsToLocal=false
LocalTileServerUrl=127.0.0.1:8870
CreateUrlTemplateRasterOverlay=false
```

注意：UE ini 中的 `//` 可能被当成注释，因此当前使用不带协议的地址；代码会补 `http://`。裸地址默认生成 raster `/{z}/{x}/{y}` 形式和 `/tileset.json`，也可以使用代码支持的 `$z/$x/$y` 模板配置 `RasterTemplateUrl`。实际模板、投影、tile 尺寸、级别由当前 ini 和 `ApplyCesiumTileServerConfig()` 决定。

当前行为：

- `UseLocalTileServer=false`：保留在线/原有 Cesium 源。
- `UseLocalTileServer=true`：切换可运行时配置的 URL raster overlay；只有 `SwitchTilesetsToLocal=true` 才切换 3D Tileset URL。
- 配置可选择离线 raster plane；默认本地端口是 `8870`，不是旧文档中的 `8070`。
- WGS84/GCJ02、tile bounds、zoom 和视觉对齐必须通过样例和运行日志验证。

## 2. P0 必读

| 优先级 | 材料 | 目的 |
|---|---|---|
| P0 | `Config/DefaultEngine.ini` | 当前开关、地址、模板、投影和级别 |
| P0 | `DroneOpsGameMode.cpp` 的 `ApplyCesiumTileServerConfig()` | URL 规范化、overlay 更新、离线平面和日志 |
| P0 | Cesium `ACesium3DTileset` API | `SetUrl()`/`RefreshTileset()` 的版本行为 |
| P0 | Cesium `UCesiumUrlTemplateRasterOverlay` API | URL template、投影、tile size 和刷新 |
| P1 | Cesium TMS/WMTS overlay API | 需要兼容 TMS/WMTS 数据集时使用 |
| P1 | GDAL `gdal2tiles` 官方文档 | GeoTIFF/COG 到 XYZ/TMS 瓦片 |
| P1 | 3D Tiles Specification | 仅在实际提供 `tileset.json` 时实现 3D 数据 |
| P1 | MBTiles Specification / Slippy Map Tilenames | 单文件离线包和 XYZ 坐标约定 |

## 3. 数据集必须先回答的问题

- 数据格式：GeoTIFF、COG、XYZ/TMS 目录或 MBTiles？
- 覆盖范围、min/max zoom、tile size 和数据版本？
- 坐标系：EPSG:4326、EPSG:3857、GCJ02 或其他？
- `{y}` 是 XYZ 还是 TMS？是否需要 `reverseY`？
- 目标区域外返回 404、透明瓦片还是占位图？
- 是否需要 attribution/copyright？
- 是否同时提供真实 3D Tiles；若没有，不要把空 `tileset.json` 当作 3D 功能完成。

建议项目侧提供 `offline-map-dataset.json`，至少包含 dataset、bounds、minZoom、maxZoom、tileSize、scheme、projection、attribution 和版本。

## 4. 最小本地服务契约

当前代码能消费的最小入口：

```text
GET /health
GET /{z}/{x}/{y}.png 或配置的 raster template
GET /tileset.json（只有启用本地 3D Tileset 时需要）
```

工程还未在 `tools/` 中固定一个离线 tile server。实现时应明确监听 `127.0.0.1:8870` 或把 UE 配置一起修改，并提供：启动日志、数据目录、范围、zoom、未命中瓦片和 metadata。

## 5. 推荐阅读/实现顺序

1. 先读 `DefaultEngine.ini` 和 `ApplyCesiumTileServerConfig()`，确认最终 URL。
2. 用浏览器/curl 请求少量瓦片，确认 MIME、大小、方向和 zoom。
3. 再用 Cesium overlay API 对齐一个已知经纬度点。
4. 开 `UseLocalTileServer=true`，进入 UE 5.7 Play Mode，记录地图日志和截图。
5. 只有真实离线数据具备时，才验证断网加载和 3D Tiles。

## 6. 验收清单

- [ ] 数据集授权、范围、投影、瓦片 scheme 和版本已记录。
- [ ] `GET /health` 返回 200；服务监听与 UE 配置一致。
- [ ] 抽样瓦片能加载，未上下翻转、跨层或偏移。
- [ ] `UseLocalTileServer=false` 的在线源未被破坏。
- [ ] `UseLocalTileServer=true` 的 URL raster overlay 可刷新；不支持的 overlay 有明确日志。
- [ ] GCJ02/WGS84 处理与数据集一致。
- [ ] 离线 raster plane 的高度、范围和相机覆盖经过 Play Mode 验证。
- [ ] 未启动本地服务时 UE 不崩溃，日志明确说明不可用。
- [ ] 3D Tiles 只有在真实合法 `tileset.json` 存在时才标记通过。
- [ ] 记录版本、命令、配置、日志和截图；不要用静态配置代替视觉验收。

## 7. 相关坐标规则

地图显示使用 WGS84/Cesium 世界坐标；后端控制仍使用每架无人机的上电原点相对偏移。离线地图对齐通过，不代表多机阵列的相对目标已经正确；另见 `Docs/坐标系转换说明.md`。
