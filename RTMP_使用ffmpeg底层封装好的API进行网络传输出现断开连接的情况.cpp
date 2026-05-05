// 问题现象：
// 在调用 av_interleaved_write_frame(...) 时返回 -10054。
// 这个错误在 Windows 下对应 WSAECONNRESET，含义是“远程主机强迫关闭了一个现有的连接”。
// 也就是说，RTMP 服务器（这里是本地 Nginx-RTMP）先主动断开了连接，
// 客户端后续再写入数据时就会看到这个错误。

// 断开连接的直接原因：
// Nginx 日志中出现了 “codec: invalid video codec header size=5”。
// 这表示推送过去的视频编码头信息不合法，通常是 H.264 的 SPS/PPS（extradata / sequence header）
// 没有正确写入，或者写入格式不符合 RTMP/FLV 的要求。

// 为什么会发生：
// 1) 视频流的编码器没有正确输出全局头信息；
// 2) muxer 在写入 header 时没有拿到正确的 extradata；
// 3) 时间戳虽然也需要检查，但这次日志已经明确指向了“视频 codec header 无效”，
//    所以首要问题是视频头封装错误，而不是网络本身不通。

// 解决方法：
// 1) 在视频编码器初始化时开启全局头：
//    m_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
//    这样 SPS/PPS 会进入 extradata，供封装层写入 RTMP/FLV 头部。
// 2) 保证 avformat_write_header(...) 之后，输出流的 time_base 已正确设置。
// 3) 保证音视频 packet 的 pts / dts 单调递增，避免后续再次触发服务端拒绝。
// 4) 重新编译并推流，检查 Nginx error.log，确认不再出现 invalid video codec header。

// 补充说明：
// 如果后续仍然断开连接，再继续检查：
// - 编码器输出的 packet 是否有正确的 stream_index、pts、dts；
// - 音视频时间基是否与 muxer 侧保持一致；
// - Nginx-RTMP 的 app / live / gop_cache / hls 等配置是否合理。
