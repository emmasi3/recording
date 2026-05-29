# recording
此项目采用DXGI高性能录屏技术实现从windows端录制音视频并推流到RTMP服务器部分
推流部分采用 ffmpeg 内置的 librtmp 模块，等写完 rtmp 服务器之后，再行完善推流部分、其次是拉流端 -- 在线播放器(本地)

此项目需要 SDL2、ffmpeg 这俩第三方库支持，请使用 vs2026 打开此项目，应该就能够使用了
