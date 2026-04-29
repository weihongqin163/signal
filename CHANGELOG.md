# 版本记录

本文档记录仓库中值得追踪的版本变更，重点描述对使用者和维护者有意义的交付内容。

## [Unreleased]

- 暂无未发布记录。

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
