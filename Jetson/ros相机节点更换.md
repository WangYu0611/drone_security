建议优先“更换 ROS 相机节点”，用 `usb_cam` 解码 MJPG，再继续沿用现有的：

`/image_raw → jetson_video_stream.py → MediaMTX → UE`

这样视频元数据、后端自动写入 `video_url`、SfM 采集链路都不用推倒重来。

先停止旧的 `v4l2_camera_node`，确保只有一个程序占用 `/dev/video0`。然后在 Jetson 上安装并启动 `usb_cam`：

```bash
sudo apt update
sudo apt install -y ros-humble-usb-cam

source /opt/ros/humble/setup.bash

ros2 run usb_cam usb_cam_node_exe --ros-args \
  -p video_device:=/dev/video0 \
  -p image_width:=1920 \
  -p image_height:=1080 \
  -p framerate:=30 \
  -p pixel_format:=mjpeg2rgb \
  -p camera_frame_id:=camera \
  -r image_raw:=/image_raw \
  -r camera_info:=/camera_info
```

验证：

```bash
ros2 topic echo /image_raw --once
ros2 topic hz /image_raw
```

确认输出为 `1920×1080`、约 `30 FPS` 后，再启动原来的推流脚本：

```bash
python3 /home/jetson1/jetson1/jetson_video_stream.py \
  --drone-id 1 \
  --fps 30 \
  --bitrate 16000000 \
  --keyframe-interval 30
```

如果相机仅支持 `1080p@25fps`，把上面三处的 `30` 一起改为 `25`。

如果 `usb_cam` 在这台 Jetson 上仍不能解码 MJPG，再走“完全绕过 ROS”的 GStreamer 方案。它会替代 `v4l2_camera_node` 和 `jetson_video_stream.py`：

```bash
gst-launch-1.0 -e \
  v4l2src device=/dev/video0 io-mode=mmap do-timestamp=true \
  ! 'image/jpeg,width=1920,height=1080,framerate=30/1' \
  ! jpegparse \
  ! jpegdec \
  ! videoconvert \
  ! 'video/x-raw,format=I420' \
  ! nvvidconv \
  ! 'video/x-raw(memory:NVMM),format=NV12' \
  ! nvv4l2h264enc bitrate=16000000 control-rate=1 \
      iframeinterval=30 idrinterval=30 insert-sps-pps=true \
  ! h264parse config-interval=-1 \
  ! rtspclientsink \
      location=rtsp://192.168.30.111:8554/drone-1 \
      protocols=tcp
```

直接 GStreamer 的代价是：不再发布 `/image_raw`、`/camera_info`，当前的逐帧 GPS/姿态元数据和 SfM 链路会失效；而且原脚本负责的后端 `video_url` 自动更新也要手动补一次：

```bash
curl -X PUT http://192.168.30.111:8080/api/drones/1 \
  -H 'Content-Type: application/json' \
  -d '{"video_url":"http://192.168.30.111:8889/drone-1"}'
```

所以先试 `usb_cam`。它成功的话，对现有工程影响最小；现有文档也已确认 `v4l2_camera` 的 MJPG 兼容性问题仅在该节点本身。[故障记录](D:/RedAlert/UE5DroneControl/Jetson/视频流调试对话整理.md:104)