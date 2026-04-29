<!--
  author: Yan Zhennan
  date: 2026-04-29
-->

# Agora-Hex TCP 协议 C 实现

当前仓库根目录直接承载 Agora-Hex TCP 协议的 C11 实现，包含帧编解码、JSON 信封解析/序列化、示例程序和测试。协议文档、PDF 和消息样例放在 `docs/` 下。

## 目录结构

| 路径 | 说明 |
|------|------|
| `Makefile` | 根目录构建入口 |
| `include/agorahex/` | 对外头文件：`framing.h`、`envelope.h`、`types.h`、`result.h`、`signal_tcp.h` |
| `src/` | 协议库实现：`framing.c`、`envelope.c`、`result.c`、`signal_tcp.c` |
| `examples/` | 示例程序：`hexagora_server.c`、`hexagora_client.c` |
| `tests/` | 测试程序：`test_framing.c`、`test_envelope.c`、`test_samples.c`、`test_signal_tcp.c` |
| `third_party/cJSON/` | 内嵌 `cJSON` 源码 |
| `docs/` | 协议说明、PDF 和 `json_msg/*.txt` 样例消息 |
| `build/` | `make` 生成的库、可执行文件和中间产物 |

## 依赖

- 支持 C11 的编译器：`cc`、`clang` 或 `gcc`
- `ar`
- 仓库内置的 `third_party/cJSON`

## 构建

在仓库根目录执行：

```bash
make
```

或：

```bash
make all
```

主要产物：

- `build/libagorahex.a`
- `build/hexagora-server`
- `build/hexagora-client`

对象文件会统一输出到 `build/obj/`，不会再写回 `src/`、`examples/`、`tests/` 目录。

## 测试

```bash
make test
```

测试项包括：

- `test_framing`：帧编码/解码回归
- `test_envelope`：信封解析与序列化回归
- `test_samples`：校验 `docs/json_msg/*.txt` 中的协议样例

其中 `test_samples` 需要以 `docs/` 作为工作目录运行，`Makefile` 已自动处理。

## 运行示例

启动本机 signal server，默认监听 `127.0.0.1:9876`：

```bash
./build/hexagora-server -port 9876
```

发送内置样例消息，client 会每隔 1 秒重复发送一次：

```bash
./build/hexagora-client -port 9876 -sample hangup
```

`-sample` 当前支持：

- `hangup`
- `muted`
- `start_content_request`

也可以直接发送样例文件：

```bash
./build/hexagora-client -port 9876 -file docs/json_msg/HangupIndication.txt
```

server 收到消息后会原样回传，client 会在标准输出打印收到的 JSON。
以上命令默认都在仓库根目录执行，且示例程序只面向本机进程间通信。

## 清理

```bash
make clean
```

该命令会删除 `build/` 下的构建产物，并兼容清理历史遗留在源码目录中的 `.o` 文件。

## API 概要

- 成帧：`agorahex_frame_encoded_size`、`agorahex_frame_encode`
- 流式拆帧：`agorahex_frame_decoder_t`、`agorahex_frame_decoder_append`、`agorahex_frame_decoder_reset`
- 信封解析/序列化：`agorahex_parse_envelope`、`agorahex_marshal_envelope`
- 本机 TCP signal 层：`agorahex_signal_start`、`agorahex_signal_send`、`agorahex_signal_poll`、`agorahex_signal_close`
  - `agorahex_signal_cb_t` 回调会同时提供来源 `fd`、原始 `json/len` 以及已解析的 `agorahex_message_t *`
- 资源释放：`agorahex_message_free`
- 错误描述：`agorahex_strerror`

具体接口声明见 `include/agorahex/` 下头文件。
