# MediaMTX 本地视频演示

> 更新时间：2026-09-09
> 本文只描述仓库内 `tools/MediaMTX/` 的本地演示。真实 Jetson 推流、摄像头占用和 SfM 采集属于外部系统，不能用本地循环视频代替。

## 1. 当前链路

```text
本地 MP4 ── FFmpeg RTSP publish ──► MediaMTX
                                      ├─ RTSP :8554
                                      └─ WebRTC browser page :8889/drone-N
UE DroneVideoWindowWidget ───────────► 读取 video_url 并加载浏览器页面
```

当前 `tools/MediaMTX/mediamtx.yml` 配置了 `drone-1`、`drone-2`、`drone-3`，路径默认不再次录制。`tools/MediaMTX/start_demo_streams.ps1` 会从 `D:\DroneData\recordings\drone-4` 读取脚本内列出的 MP4，并循环推送到这三条路径；这只是当前机器的演示目录约定，换机器要改脚本。

## 2. 启动

要求：MediaMTX、FFmpeg/FFprobe 和脚本中列出的录像文件存在。

```powershell
cd D:\RedAlert\UE5DroneControl
.\tools\MediaMTX\start_demo_streams.cmd
```

PowerShell 主脚本会：

1. 检查或启动后端 `Backend/build/Release/DroneBackend.exe`；
2. 检查或启动 `tools/MediaMTX/mediamtx.exe`；
3. 循环推送 `drone-1`、`drone-2`、`drone-3`；
4. 用 FFprobe 检查 RTSP 可读性并打印 WebRTC 页面地址。

预览地址：

```text
http://127.0.0.1:8889/drone-1
http://127.0.0.1:8889/drone-2
http://127.0.0.1:8889/drone-3
```

UE 的 `video_url` 应填浏览器播放页，不要填 RTSP 或 `/drone-N/whep` 原生接口。UE 关闭窗口时会导航到 `about:blank`，不会停止 MediaMTX 进程。

## 3. 端口和跨机

| 用途 | 默认 |
|---|---:|
| RTSP TCP | 8554 |
| WebRTC HTTP | 8889 |
| WebRTC ICE UDP | 8189 |

本机演示使用 `127.0.0.1`。跨机时：

- `webrtcAdditionalHosts` 必须包含 UE 客户端可访问的 MediaMTX LAN 地址；
- Windows 防火墙允许 TCP 8554/8889 和 UDP 8189；
- 后端注册的 `video_url` 指向 MediaMTX 主机，而不是 Jetson 或后端的猜测地址；
- 浏览器先能播放，再在 UE 中验证 WebBrowser；不要只看 `video_url` 字符串。

## 4. 验收边界

- [ ] FFprobe 能读到每条 RTSP 路径。
- [ ] 浏览器能连续播放 WebRTC 页面。
- [ ] UE DroneInfo/video window 能打开和关闭页面。
- [ ] 通过 `GET /api/drones` 返回的 `video_url` 与实际路径一致。
- [ ] 如果验证录像，确认 `mediamtx.yml` 的 `pathDefaults.record`、录制目录和磁盘空间；当前演示配置默认 `record=false`，循环播放已有录像不会再录制。
- [ ] 视频成功不代表后端控制、GPS/SfM 或 PX4 执行成功。

逐帧元数据如果参与测试，使用后端 `POST /api/video-metadata/batch` 和 `Backend/config.yaml` 的 `storage.video_metadata_path`，不要假设遥测 WebSocket 会承载视频元数据。

## 5. 外部依赖

仓库当前未包含 Jetson 摄像头采集脚本和完整 PX4/ROS2 环境。需要做真实视频/SfM 时，应在 Jetson 项目中确认：摄像头话题、RTSP publisher、后端 metadata endpoint、网络地址和版本；完成后把实际命令与日志路径补进单独外场记录。
