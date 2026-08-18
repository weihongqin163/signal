# 版本记录

本文档记录仓库中值得追踪的版本变更，重点描述对使用者和维护者有意义的交付内容。

## [1.0.8] - 2026-08-18

扩展 signal 连接状态回调，支持客户端 TCP 连接成功通知。

### 变更

- 将 `agorahex_signal_disconnect_cb_t` 重命名为 `agorahex_signal_connect_status_cb_t`，并将 `agorahex_signal_disconnect_reason_t` 重命名为 `agorahex_signal_connect_status_t`；这是源代码不兼容的公开 API 变更。
- 新增 `AGORAHEX_SIGNAL_CLIENT_CONNECTED` 状态。客户端每次 TCP 连接或重连完成后，`agorahex_signal_poll()` 会同步调用状态回调，并传入仍有效的连接 socket fd。
- 保留原有服务端客户端断开通知及三个断开状态的数值和语义；服务端回调中的 fd 仍已关闭，仅用于应用映射。
- 显式调用 `agorahex_signal_close()` 不产生连接状态通知。
- 更新 signal server 示例和回归测试，覆盖客户端连接成功状态、回调 fd 有效性以及关闭后不重复通知。

## [1.0.7] - 2026-08-17

增加 AVC 容量上报协议和 signal server 客户端断开通知。

### 变更

- 新增 `AVCCapacityIndication` 协议及公开 C 数据结构，支持 `maxCapacity`、`curCapacity`、`identifier` 以及可选的 `addr`、`port` 字段。
- 容量值仅校验为 C `int` 范围内的 JSON 整数，不执行默认容量约束；`identifier` 校验为 1 至 64 个 UTF-8 字节，容量和标识唯一性由业务侧管理。
- signal server 收到容量上报后使用 `getpeername` 获取真实 TCP 客户端地址与端口，并覆盖回调消息结构体中的不可信上报值，同时保持原始 JSON 不变。
- 为 `agorahex_signal_start` 增加可空的服务端断开回调参数，区分对端正常关闭、I/O 错误和协议错误；连接在回调前已关闭并移除，且每条连接最多通知一次。
- 服务端发送失败会触发 I/O 断开通知；显式调用 `agorahex_signal_close`、客户端超限拒绝、监听 socket 错误和 client 模式断线不触发该回调。
- 增加容量协议解析、序列化、真实对端覆盖和服务端断开原因的回归测试，并收录容量上报协议样例。

## [1.0.6] - 2026-08-17

提升 signal server 的并发连接容量，并修正 AVC 内容启动回复相关命名。

### 变更

- 将 `AGORAHEX_SIGNAL_MAX_CLIENTS` 从 8 提升至 128，扩大 signal server 可同时维护的客户端连接数量及监听队列容量。
- 将 AVC 内容启动回复的公开 C 类型由 `agorahex_avc_start_content_replay_t` 更正为 `agorahex_avc_start_content_reply_t`，枚举常量更正为 `AGORAHEX_KIND_AVC_START_CONTENT_REPLY`，并同步将消息联合体成员更名为 `avc_start_content_reply`。
- 同步更新 AVC 内容启动回复的解析、序列化、资源释放、示例程序和回归测试中的字段引用。
- 将协议样例中的消息名由 `AVCStartContentReplay` 修正为 `AVCStartContentReply`。
- 将 TCP 回归测试的服务端地址调整为标准回环地址 `127.0.0.1`，提升测试在受管网络环境中的可移植性。

## [1.0.5] - 2026-08-12

增加 Agora Dial-Out JSON 协议支持。

### 变更

- 新增 `AgoraDialOutRequest` 和 `AgoraDialOutReply` 消息类型及公开 C 数据结构。
- 增加两个协议的 JSON 解析、序列化和消息内存释放逻辑。
- 支持 Dial-Out 请求与回复中的 AVC endpoint、音视频属性、媒体能力和最大分辨率字段。
- 增加协议样例解析、framing 和 JSON round-trip 回归测试。

## [1.0.1] - 2026-04-30

聚焦 TCP signal 接口命名一致性和头文件可用性增强的小版本更新。

### 变更

- 将 `agorahex_signal_send` 的参数名从 `buffer` 统一调整为 `json`，使公开 API 命名与实际 JSON 信封语义保持一致。
- 同步更新 `src/signal_tcp.c` 内部校验与发送路径中的对应命名，减少接口阅读和维护时的语义歧义。
- 为 `include/agorahex/signal_tcp.h` 的公开回调和函数补充接口注释，覆盖参数说明、调用方式和返回值语义，便于直接集成和查阅。

## [1.0.0] - 2026-04-29

首次发布，完成 Agora-Hex TCP JSON 协议的 C11 版本基础交付。

### 新增

- 提供静态库 `libagorahex.a`，覆盖 8 字节帧头编码、流式拆帧、JSON 信封解析、JSON 信封序列化和统一错误码描述。
- 实现 TCP signal 层接口 `agorahex_signal_start`、`agorahex_signal_send`、`agorahex_signal_poll`、`agorahex_signal_close`，支持本机 client/server 模式和回调式收发。
- 支持当前协议样例中的消息类型，包括 `AgoraDialInIndication`、`AVCDialInRequest`、`AVCDialInReply`、`HangupIndication`、`MutedIndication`、`AVCNameChangedIndication`、`AgoraStartContentIndication`、`AVCStartContentRequest`、`AVCStartContentReplay` 和 `StopContentIndication`。
- 补齐对外头文件与数据结构定义，覆盖呼叫、媒体能力、信令腿和内容控制等主要字段形状，便于业务层直接接入。
- 提供 `hexagora-server` 和 `hexagora-client` 两个示例程序，可用于本机 TCP 联调、样例消息发送和回环验证。
- 提供 `make`、`make test` 和 `make clean` 构建入口，统一将库、示例程序、测试程序和中间产物输出到 `build/`。
- 增加回归测试，覆盖帧编解码、半包/粘包、错误输入、样例消息解析，以及本机 TCP client/server 交互流程。
- 收录协议说明文档、PDF 和 `docs/json_msg/` 示例消息，作为实现和测试的参考基线。

### 说明

- `1.0.0` 为初始版本，没有更早的对比版本。
- 当前仓库的 `release/1.0.0` 分支和 `1.0.0` tag 都指向这次首发内容。
