# 思考：2、RTMP 完整交互流程详解

---

## 一、问题

用户在理解第 32 题（`avio_open2`）之后，提出两个追问：

1. **「模拟一下 RTMP 两端进行一次完整通信的流程，详细一点，我要回顾一下流程。」**
2. **「关于 RTMP 握手规则中，我看到一点：无论发送什么消息，必须先发送 C2？这是不是有什么误解？」**

---

## 二、用户回答

（该问题为用户主动追问，未提交原始回答草稿，以下为用户问题的背景说明）

- 用户已理解 `avio_open2` 对于 RTMP 推流会内部完成 TCP 连接 + RTMP 握手，但想深入了解 RTMP 协议交互的具体步骤和时序。
- 用户在自主学习 RTMP 协议时，看到资料中提到"C2"的概念，产生了疑问：是否每条消息之前都必须先发送 C2？

---

## 三、标准参考回答

### 3.1 完整通信流程 — 逐包级模拟

以下模拟客户端（推流端）与服务器（Nginx-RTMP / SRS）从建立连接到开始推流的完整过程。

---

#### 阶段一：TCP 三次握手（传输层）

```
客户端                              服务器
  │                                   │
  │────── SYN ──────────────────────→│  (1) 请求建立连接
  │                                   │
  │←───── SYN + ACK ────────────────│  (2) 确认并同步
  │                                   │
  │────── ACK ──────────────────────→│  (3) 最终确认
  │                                   │
  │      TCP 连接建立，进入 RTMP 握手   │
```

---

#### 阶段二：RTMP 握手（应用层，共 6 个包，1 个 RTT）

```
客户端                              服务器
  │                                   │
  │── C0 (1 字节) ─────────────────→│  协议版本号，如 0x03 = RTMP v3
  │── C1 (1536 字节) ──────────────→│  4B 时间戳 + 4B 零 + 1528B 随机数据
  │                                   │
  │←─ S0 (1 字节) ─────────────────│  服务器版本号
  │←─ S1 (1536 字节) ──────────────│  4B 时间戳 + 4B 零 + 1528B 随机数据
  │←─ S2 (1536 字节) ──────────────│  回显 C1 的时间戳 + C1 的随机数据
  │                                   │
  │── C2 (1536 字节) ──────────────→│  回显 S1 的时间戳 + S1 的随机数据
  │                                   │
  │      握手完成，进入 RTMP Chunk 通信  │
```

| 包名 | 方向 | 大小 | 内容 |
|------|------|------|------|
| C0 | 客户端 → 服务器 | 1 字节 | RTMP 协议版本（通常 0x03） |
| C1 | 客户端 → 服务器 | 1536 字节 | 4B 时间戳 + 4B 零值 + 1528B 随机填充 |
| S0 | 服务器 → 客户端 | 1 字节 | 服务器 RTMP 版本 |
| S1 | 服务器 → 客户端 | 1536 字节 | 4B 时间戳 + 4B 零值 + 1528B 随机填充 |
| S2 | 服务器 → 客户端 | 1536 字节 | 回显 C1 的时间戳 + 回显 C1 的全部 1528B 随机数据 |
| C2 | 客户端 → 服务器 | 1536 字节 | 回显 S1 的时间戳 + 回显 S1 的全部 1528B 随机数据 |

**握手设计目的**：
- 版本协商（C0/S0）
- 时间同步（C1/S1 中的时间戳）
- 身份验证/防伪造（C2/S2 中回显对方随机数据——证明"我看到过你的包"）

---

#### 阶段三：RTMP Chunk 命令交互

RTMP 握手完成后，所有后续通信走 **RTMP Chunk Stream** 协议：
- 每个 Chunk 头部包含 `chunk stream id`（cs_id），用于区分 64 个逻辑通道
- cs_id=3：命令消息通道
- cs_id=4：音频数据通道
- cs_id=6：视频数据通道

---

##### 3.3.1 `connect` — 连接到 RTMP 应用

```
客户端 ──────────────────────────────→ 服务器

AMF0 编码命令：
  "connect"                           ← 命令名
  1                                   ← 事务 ID
  {
    app: "live",                      ← RTMP 应用名（对应 URL 路径）
    tcUrl: "rtmp://server_ip/live",   ← 完整连接 URL
    type: "nonprivate",
    ...
  }

服务器 ←────────────────────────────── 客户端

  _result                             ← 响应命令
  1                                   ← 同一事务 ID
  {
    fmsVer: "...",
    capabilities: ...,
    level: "status",
    code: "NetConnection.Connect.Success"
  }
```

| 关键字段 | 含义 |
|---------|------|
| `app` | 对应 Nginx 配置中的 `application live { ... }` |
| `tcUrl` | 完整的推流 URL |
| `NetConnection.Connect.Success` | 返回值——必须为 Success 才表示连接成功 |

---

##### 3.3.2 `createStream` — 创建逻辑流

```
客户端 ──────────────────────────────→ 服务器
  "createStream"
  2                                   ← 事务 ID
  null

服务器 ←────────────────────────────── 客户端
  _result
  2
  null
  1                                   ← 返回的 stream ID（重要！）
```

- 返回的 `stream_id = 1` 后续所有音视频推流都要带上这个 ID。
- 一个 RTMP 连接可以创建多个流，但推流场景通常只用一个。

---

##### 3.3.3 `publish` — 发布/推流

```
客户端 ──────────────────────────────→ 服务器
  "publish"                           ← 流名（URL 中 rtmp://.../live/xxx 的 xxx 部分）
  3
  null
  "test"                              ← 发布类型（live / record / append）
  "live"

服务器 ←────────────────────────────── 客户端
  onStatus
  0
  null
  {
    level: "status",
    code: "NetStream.Publish.Start",  ← 推流已开始！
    description: "Stream is now published."
  }
```

- 收到 `NetStream.Publish.Start` 后，客户端就可以开始发送音视频数据了。

---

#### 阶段四：元数据 + 编码参数下发

##### 3.4.1 `@setDataFrame` / `onMetaData`

```
客户端 ──────────────────────────────→ 服务器
  @setDataFrame
  onMetaData
  {
    duration: 0,
    width: 1920,
    height: 1080,
    videodatarate: 0,
    framerate: 60,
    videocodecid: 7,                  ← AVC (H.264)
    audiodatarate: 0,
    audiosamplerate: 44100,
    audiosamplesize: 16,
    stereo: true,
    audiocodecid: 10,                 ← AAC
    ...
  }
```

- 服务器根据这些元数据初始化流信息，后续播放器拉流时也会拿到这份元数据。

---

##### 3.4.2 AVC Sequence Header（SPS + PPS）

```
客户端 ──────────────────────────────→ 服务器

RTMP Video Tag:
  Frame Type: 1 (关键帧)
  Codec ID: 7 (AVC / H.264)
  AVC Packet Type: 0x00 (Sequence Header)
  Composition Time: 0
  
  负载内容:
    SPS (Sequence Parameter Set)      ← H.264 序列参数集
    PPS (Picture Parameter Set)       ← H.264 图像参数集
```

- **必须**在第一个视频帧之前发送。
- 这就是项目中 `AV_CODEC_FLAG_GLOBAL_HEADER` 所控制的内容——不设该标志，SPS/PPS 不会被写入 FLV 的 extradata，Nginx 会报 `invalid video codec header size=5`。

---

#### 阶段五：音视频数据循环

```
客户端 ───────── 持续发送 ─────────→ 服务器

┌─────────────────────────────────────────┐
│  Video Data (cs_id=6)                   │
│  ├─ AVC Packet Type: 0x01 (NALU)       │
│  ├─ Frame Type: 1 (IDR) / 2 (inter)    │
│  ├─ Composition Time Offset            │
│  └─ H.264 NAL Units (多个 NALU 可聚合)  │
│                                         │
│  Audio Data (cs_id=4)                   │
│  ├─ Sound Format: 10 (AAC)             │
│  ├─ Sound Rate: 44100                  │
│  ├─ Sound Type: Stereo                 │
│  ├─ AAC Packet Type: 0x01 (Raw AAC)    │
│  └─ AAC Raw Data                        │
└─────────────────────────────────────────┘

时间戳使用 Chunk Header 中的增量时间戳（fmt=2 或 fmt=3），
大幅压缩重复字段，高效传输。
```

---

#### 阶段六：断开连接

```
客户端 ──────────────────────────────→ 服务器
  FCUnpublish                         ← 取消发布
  "test"

客户端 ──────────────────────────────→ 服务器
  deleteStream                        ← 删除流
  4
  null
  1

随后 TCP 四次挥手断开连接：
  客户端 ── FIN ──→ 服务器
  客户端 ←─ ACK ── 服务器
  客户端 ←─ FIN ── 服务器  (或合并为三次)
  客户端 ── ACK ──→ 服务器
```

---

### 3.2 关于 C2 的误解澄清

#### 误解来源

> **❌ 错误理解**：「无论发送什么消息，必须先发送 C2」
>
> 这容易被误解为：发送一条 connect 之前要发一个 C2，发送 createStream 之前又要发一个 C2……如同每条消息的"前缀暗号"。

#### 正确理解

> **✅ 正确说法**：「RTMP 握手完成后（即 C2 已经发送完毕），才能开始发送 RTMP Chunk 消息。」

| 对比维度 | 错误理解 | 正确理解 |
|---------|---------|---------|
| C2 发送次数 | 每条消息前都发 | **只发 1 次**，是握手第 6 个包 |
| C2 的角色 | 消息前缀/通行证 | 握手的最后一个环节 |
| C2 与消息的关系 | 每个消息前都要附加 C2 | C2 发送完之后，所有消息自由发送 |
| 类比 | 每次进门都要出示钥匙 | 进门时刷一次卡，进去后自由活动 |

#### 为什么容易产生误解

1. **C2 是握手的最后一个包**，它紧挨着第一条业务消息（`connect`），在 Wireshark 抓包中可能看到 C2 和 connect 被 TCP 粘在一个包中发出，看起来像「C2 + connect」是一体的。
2. 有些学习资料简化表述为"握手结束后才能发消息"，读者自行理解时把"结束后"错误等同于"每次都先发 C2"。
3. 真正的关系是**时序先后关系**，而非**包含/前缀关系**。

#### 一句话总结

> C2 是 RTMP 握手的第 6 个包，**仅发送一次**；握手完成后，所有 RTMP Chunk 消息（connect、createStream、publish、音视频数据）都可以自由发送，无需再发 C2。

---

## 四、面试要点速查

| 要点 | 一句话 |
|------|--------|
| **握手几个包** | 6 个包（C0+C1, S0+S1+S2, C2），1 个 RTT |
| **C2 发几次** | 仅 1 次，是握手最后一步，不是每条消息的前缀 |
| **握手核心目的** | 版本协商 + 时间同步 + 对端身份验证（回显随机数） |
| **命令走哪个通道** | cs_id=3（命令通道），cs_id=4 音频、cs_id=6 视频 |
| **推流前必须做的** | connect → createStream → publish → onMetaData → AVC sequence header |
| **最容易忘记的** | 在第一个视频帧之前发送 AVC Sequence Header（SPS/PPS），否则 Nginx 报 `invalid video codec header` |
| **断开流程** | FCUnpublish → deleteStream → TCP 挥手 |
