/*
* 
* 1、通过在 encode_DXGI_GPU 中调试获得：
*	在调用 API 写入媒体头信息后，outStream->time_base 才会被初始化，一般为 {1, 90000};
*	即使如此，还是建议在使用该字段做 数据包时间戳的转化时，检查{0, 0}; 并且处理相关错误
*	对于 video, 将要用的 time_base 直接设置为 {1, fps};
*	对于 audio, 将要用的 time_base 直接设置为 {1, m_ctx->sample_rate};
* 
* 
* 
* 
* 
* 
* 
* 
*/