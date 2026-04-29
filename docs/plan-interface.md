<!--
  author: Codex
  date: 2026-04-29
  note: 本文件仅记录接口规划，不表示相关代码已全部实施
-->

# Agora-Hex TCP Server/Client 接口规划

## 总体目标

总体目标是建立一个清晰可用的 **TCP server/client 模式** 实现，并以 **标准 C** 为核心组织协议库、接口层和示例程序，使调用方能够明确理解：

- 哪些接口属于协议库核心
- 哪些接口属于 server/client 集成层
- 每个接口的输入、输出、错误码和资源释放责任
- 成帧、拆帧、信封解析、序列化在 TCP 收发流程中的推荐使用方式
- 后续扩展消息类型、连接管理或跨平台适配时的边界与约束

本文件只描述规划，不涉及代码修改方案的直接落地。

## 标准 C 实现边界

这里需要明确一个现实边界：

- **协议核心层** 可以尽量保持为标准 C 风格：
  - 数据结构定义
  - JSON envelope 解析与序列化
  - 帧编码与流式拆帧
  - 错误码、生命周期和资源释放约定
- **TCP 网络层** 本身不属于 ISO C 标准库能力范围：
  - 在类 Unix 平台通常依赖 POSIX socket
  - 在 Windows 平台通常依赖 Winsock

因此，规划上的合理目标应是：

- 把 **协议与数据处理部分** 做成尽量纯粹、可移植的标准 C 接口
- 把 **socket 收发、监听、连接管理** 收敛在薄的一层平台适配代码中
- 对外暴露的 server/client 模式，优先通过清晰的模块边界实现可移植，而不是假设 TCP 本身能只靠 ISO C 标准库完成

## 当前接口现状

当前对外接口主要分布在 `include/agorahex/` 下：

- `framing.h`
  - 负责 TCP 帧编码、流式拆帧、decoder 生命周期管理
- `envelope.h`
  - 负责消息类型枚举、JSON envelope 解析、JSON envelope 序列化、消息资源释放
- `types.h`
  - 负责各消息体结构定义
- `result.h`
  - 负责统一错误码和错误字符串

当前仓库还已有以下配套内容：

- `examples/`
  - 提供 client/server 示例，能体现 TCP server/client 模式的最小可用集成方式
- `tests/`
  - 提供 framing、envelope、samples 三类回归测试
- `docs/json_msg/`
  - 提供消息样例，可作为接口联调基线

## 规划原则

1. **公开边界清晰**
   - 仅 `include/agorahex/` 视为公开接口面，`src/` 视为内部实现。

2. **所有权明确**
   - 凡是接口返回堆内存或在结构体中写入堆内存字段，必须明确由谁释放、何时释放、使用哪个释放函数。

3. **错误语义一致**
   - 所有失败路径都应映射到 `agorahex_result_t`，并能通过 `agorahex_strerror` 给出稳定说明。

4. **生命周期可预测**
   - `init`、`reset`、`fini` 的差异要明确，尤其是 `agorahex_frame_decoder_t` 的错误粘滞行为与复位方式。

5. **向后兼容优先**
   - 后续如果扩展消息类型、字段或辅助函数，优先追加，不轻易破坏已有调用方式。

6. **网络层与协议层解耦**
   - 协议库尽量不和具体 socket 细节耦合；server/client 模式通过适配层组合协议库能力。

## 拟定外部接口

结合当前目标，规划中的信令层外部接口收敛为四个入口：

```c
typedef void (*agorahex_signal_cb_t)(int fd, const void *json, int len, agorahex_message_t *msg_t);

int agorahex_signal_start(int server_mode, int tcp_port, agorahex_signal_cb_t cb);
int agorahex_signal_send(int fd, const void *buffer, int len);
int agorahex_signal_poll(int timeout_ms);
void agorahex_signal_close(void);
```

说明：

- 这里使用的是**标准 C 可接受的函数指针写法**
- 你给出的 `(void*)cb(void* buffer, int len)` 不是合法的 C 形参声明
- 如果回调只负责把收到的数据通知给上层，那么 `void` 返回值更合理
- 如果后续确实需要让回调影响底层流程，再考虑改成 `int` 返回值，而不是 `void *`
- 回调新增 `fd` 参数，用于标识数据来源连接
- 回调新增 `msg_t` 参数，用于直接暴露已解析完成的协议消息对象
- server 模式下，`fd` 表示发来该数据的 client fd
- client 模式下，`fd` 固定表示当前连接的 server fd
- `msg_t` 只在回调执行期间有效，由底层负责释放
- 如果上层需要在回调返回后继续使用其中字段，必须自行拷贝

当前这组接口的职责边界如下：

- `agorahex_signal_start(...)`
  - 负责初始化信令收发模式
  - 负责创建 socket、记录模式、保存回调、初始化连接状态和 client 列表
- `agorahex_signal_send(...)`
  - 负责将一条上层 signal 数据发送到目标连接
  - 负责按模式处理“指定发送”或“广播发送”
- `agorahex_signal_poll(...)`
  - 负责驱动一次事件循环
  - 负责 `poll`、`accept`、`recv`、拆帧、协议解析、回调分发以及异常连接清理
- `agorahex_signal_close(void)`
  - 负责关闭所有 fd、释放所有连接相关资源，并把内部状态恢复到未启动状态

## 平台范围

本规划以 **macOS + Linux** 为目标运行环境，因此网络层优先采用两端都具备的 POSIX 能力：

- `socket`
- `bind`
- `listen`
- `connect`
- `accept`
- `recv`
- `close`
- `poll`
- `fcntl(..., O_NONBLOCK, ...)`
- `setsockopt(..., SO_REUSEADDR, ...)`

这意味着：

- 协议核心仍按标准 C 风格组织
- TCP 网络适配层可以直接按 macOS 和 Linux 的公共 POSIX 子集设计
- 当前规划不以 Windows 为目标，不引入 Winsock 分支

## 拟定对象结构

下面这部分是**文档中的结构设计草案**，目的是把 server/client 模式需要维护的运行时状态先定义清楚，供你判断是否符合需求。

考虑到当前对外接口仍然是：

```c
int agorahex_signal_start(int server_mode, int tcp_port, agorahex_signal_cb_t cb);
int agorahex_signal_send(int fd, const void *buffer, int len);
int agorahex_signal_poll(int timeout_ms);
void agorahex_signal_close(void);
```

因此以下结构更适合作为**内部运行时对象**，由模块内部维护；对外暂时不直接暴露。

### 共用连接对象

```c
typedef struct agorahex_signal_conn {
    int fd;
    int state;
    agorahex_frame_decoder_t decoder;
} agorahex_signal_conn_t;
```

字段说明：

- `fd`
  - 对应一个已创建的 socket fd
  - 对 server 侧来说，表示某一个已 accept 的 client 连接
  - 对 client 侧来说，表示当前用于连接 server 的 socket
  - 约定 `fd == -1` 表示当前没有有效 fd，或该槽位为空

- `state`
  - 表示当前连接状态
  - 建议取值：
    - `0` = disconnected
    - `1` = connecting
    - `2` = connected
  - 这样可以把“尚未连接”“连接中”“已连接”三种状态明确区分开
  - 对 client 侧尤其重要，因为它需要在 `poll()` 中持续尝试连接

- `decoder`
  - 每条 TCP 连接都需要独立的拆帧状态
  - 用于处理半包、粘包和错误粘滞状态
  - 这样 server 端多个 client 可以并发维护各自的拆帧上下文

为什么需要单独的连接对象：

- server 端天然是“一监听 socket + 多个 client 连接”
- client 端虽然通常只有一个连接，但把它也统一抽象成连接对象，能减少分支复杂度
- 连接级资源清理也更明确：关闭 fd 时顺带释放 decoder

### Server 对象

```c
#define AGORAHEX_SIGNAL_MAX_CLIENTS 8

typedef struct agorahex_signal_server {
    int server_fd;
    int tcp_port;
    int running;
    agorahex_signal_cb_t cb;

    agorahex_signal_conn_t clients[AGORAHEX_SIGNAL_MAX_CLIENTS];
    int client_count;
} agorahex_signal_server_t;
```

字段说明：

- `server_fd`
  - 监听 socket 的 fd
  - 仅在 `server_mode == 1` 时有效

- `tcp_port`
  - 当前监听端口
  - 便于日志、重启和状态检查时使用

- `running`
  - `1` 表示 server 模式已经启动
  - `0` 表示未启动、关闭中、或已被关闭
  - 这个字段用于区分“模块未启动”和“只是当前没有 client 连入”

- `cb`
  - 收到合法完整数据包后的上行回调
  - server 下所有 client 收到的数据都复用这个回调出口

- `clients`
  - 固定长度的 client 连接对象数组
  - 当前固定最多保存 `8` 个已 accept 的连接
  - 每个槽位对应一个连接对象
  - 约定 `fd == -1` 表示该槽位为空，可直接复用

- `client_count`
  - 当前有效连接数量

这个结构的核心职责：

- 保存监听 socket
- 保存所有已接入 client 的连接状态
- 在 `poll()` 时统一完成 accept、recv、协议解析、回调和异常连接清理

### Client 对象

```c
typedef struct agorahex_signal_client {
    agorahex_signal_conn_t conn;
    int tcp_port;
    int running;
    agorahex_signal_cb_t cb;
} agorahex_signal_client_t;
```

字段说明：

- `conn`
  - client 模式下唯一的连接对象
  - 包含 fd、state 标记和 decoder

- `tcp_port`
  - 当前尝试连接的目标端口
  - 由于当前接口没有 `host` 参数，因此默认规划为本机地址上的该端口

- `running`
  - `1` 表示 client 模式已启动
  - `0` 表示未启动或已关闭
  - 该字段和 `conn.state` 需要区分：
    - `running == 1 && state == disconnected` 表示模块活着，但当前未连上 server
    - `running == 1 && state == connecting` 表示模块活着，且当前正在连 server
    - `running == 1 && state == connected` 表示模块活着，且当前已连上 server

- `cb`
  - 收到 server 发来的合法完整数据包后向上回调

这个结构的核心职责：

- 保存唯一 client 连接的生命周期
- 支撑“未连接时在 `poll()` 中重试 connect，连接后再判断可读”的行为

### 顶层运行时对象

如果继续保持当前全局接口形式，还需要一个顶层运行时对象来保存“当前究竟是 server 还是 client”：

```c
typedef struct agorahex_signal_runtime {
    int mode;
    agorahex_signal_server_t server;
    agorahex_signal_client_t client;
} agorahex_signal_runtime_t;
```

字段说明：

- `mode`
  - `1` 表示当前以 server 模式运行
  - `0` 表示当前以 client 模式运行
  - `-1` 表示尚未启动

- `server`
  - 仅当 `mode == 1` 时使用

- `client`
  - 仅当 `mode == 0` 时使用

这个顶层对象的意义：

- `agorahex_signal_start()` 根据 `server_mode` 初始化 `server` 或 `client`
- `agorahex_signal_poll()` 根据 `mode` 分发到不同逻辑
- `agorahex_signal_close()` 无论当前处于哪种模式，都可以从统一入口完成资源释放

## 结构设计上的判断点

为了让你更容易判断是否符合需求，下面列出这套结构设计的几个关键取舍：

1. **server 与 client 分成两个对象，而不是硬塞进一个大结构**
   - 优点是职责清晰
   - 缺点是顶层需要一个 `runtime` 做模式分发

2. **把 `fd`、`state`、`decoder` 收敛到连接对象**
   - 优点是 server/client 的连接管理逻辑更统一
   - 也更利于后续扩展发送接口或连接级状态

3. **`running` 与连接 `state` 分开**
   - 这是为了支持 client 的重连行为：
   - client 即使当前没连上 server，也不等于模块已经关闭
   - `running == 1 && state == disconnected` 是合法状态

4. **server 端 client 列表采用固定 8 个槽位**
   - 优点是结构简单、内存边界清晰、实现和排查都直接
   - 缺点是并发连接数上限被固定为 `8`
   - 空槽判断规则明确为 `fd == -1`
   - 新连接到来时优先复用 `fd == -1` 的槽位
   - 如果 8 个槽位都已占用，则拒绝连接并打印 error 信息

5. **当前结构没有加入发送缓冲、重连退避、对端地址信息**
   - 这是因为你目前给出的需求还集中在：
   - 启动
   - 发送
   - `poll`
   - 接收
   - 回调
   - 关闭
   - 如果后面要加发送能力、地址过滤、重连节流，再补这些字段更合适

6. **线程模型保持简单**
   - 本层默认非线程安全
   - 外部接口按单线程串行模型使用
   - 查询和管理线程放在 app 层，不在本层处理

## 拟定行为语义

### `agorahex_signal_start`

输入：

- `server_mode`
  - `1` 表示 server 模式
  - `0` 表示 client 模式
- `tcp_port`
  - server 监听端口，或 client 连接端口
- `cb`
  - 收到完整且合法的数据包后向上回调
  - 回调会同时收到原始 `json/len` 和已解析完成的 `agorahex_message_t *msg_t`

行为：

- 当 `server_mode == 1`
  - 创建 server socket
  - 绑定到指定 `tcp_port`
  - 设置监听
  - 将 server fd 设为 non-blocking
  - 初始化 client fd 列表

- 当 `server_mode == 0`
  - 创建 client socket
  - 尝试连接到目标端口
  - 如果首次连接失败，记录内部状态为 `disconnected`
  - 如果首次连接进入进行中状态，记录内部状态为 `connecting`
  - 如果首次连接成功，记录内部状态为 `connected`
  - 首次连接失败不视为整体生命周期结束，后续由 `agorahex_signal_poll()` 继续驱动重连尝试

当前接口限制：

- 该接口只有 `tcp_port`，没有 `host` 参数
- 因此按当前接口定义，client 侧只能规划为连接**本机地址**
- 文档建议默认目标为 `127.0.0.1:tcp_port`
- 如果后续需要跨机器连接，接口层应扩展为 `host + port`，否则能力边界不完整

返回语义规划：

- 返回 `0` 表示初始化成功
- 返回负值表示初始化失败
- 失败原因应能映射到内部错误码或日志

### `agorahex_signal_send`

输入：

- `fd`
  - server 模式下：
    - `fd == -1` 表示广播给所有已连接 client
    - `fd >= 0` 表示仅发送给指定 client fd
  - client 模式下：
    - 忽略 `fd` 参数
    - 固定发送给当前连接的 server
- `buffer`
  - 待发送的 signal 数据
- `len`
  - 待发送数据长度

行为：

- 先对 `buffer` 和 `len` 做基本校验
- 将上层 signal 数据编码为协议帧
- 当处于 `server_mode`
  - 若 `fd == -1`，则遍历所有已连接 client，逐个发送
  - 若 `fd >= 0`，则查找匹配 fd 的 client，并仅向该连接发送
  - 若指定 fd 不存在，则返回错误
- 当处于 `client_mode`
  - 忽略传入 `fd`
  - 若当前已连接，则发送给固定 server
  - 若当前未连接，则返回错误

返回语义规划：

- 返回 `0` 表示发送成功
- 返回负值表示发送失败
- server 广播时，如果需要更精细的部分成功语义，后续再补；当前规划先按“任一失败即整体失败或记录错误”处理

### `agorahex_signal_poll`

输入：

- `timeout_ms`
  - 传给 `poll` 的超时时间，单位毫秒

行为：

- 当处于 `server_mode`
  - 先对 server fd 做 `poll`
  - 如果有新连接，则执行 `accept`
  - 在 `clients[8]` 中查找 `fd == -1` 的空槽位
  - 如果找到空槽位，则将新 client fd 设为 non-blocking，并加入该槽位
  - 如果没有找到空槽位，则拒绝该连接，并打印 error 信息
  - 然后对 client list 中所有 fd 做 `poll`
  - 若某个 fd 可读，则执行 `recv`
  - 将收到的字节流送入帧解码与协议解析流程
  - 如果形成完整且合法的数据包，则通过 `cb` 回调出去
  - 如果连接异常或对方关闭，则关闭该 fd，并将对应槽位重置为 `fd == -1`
  - 如果 server fd 本身异常或关闭，则清理所有 client fd

- 当处于 `client_mode`
  - 先检查当前连接状态
  - 如果当前 `state == disconnected`
    - 重新创建 client socket
    - 尝试执行 `connect`
    - 如果进入连接进行中，则将状态置为 `connecting`
    - 如果立即成功，则将状态置为 `connected`
    - 如果失败，则关闭本次 socket，并保持 `disconnected`，等待下一次 `poll` 再尝试
  - 如果当前 `state == connecting`
    - 对 client fd 做 `poll`
    - 根据可写/错误状态判断连接是否建立成功
    - 若连接成功，则将状态置为 `connected`
    - 若连接失败，则关闭 client fd，重置为 `fd == -1`，并将状态置为 `disconnected`
  - 如果当前 `state == connected`
    - 对 client fd 做 `poll`
    - 若 fd 可读，则执行 `recv`
    - 将收到的字节流送入帧解码与协议解析流程
    - 如果形成完整且合法的数据包，则通过 `cb` 回调出去
    - 如果连接异常或对方关闭，则关闭 client fd，重置为 `fd == -1`，并将状态置为 `disconnected`

错误与关闭语义规划：

- `recv == 0`
  - 视为对端关闭连接
- `poll` 返回异常事件
  - 视为该 fd 已不可用，应关闭并移除
- 收到非法协议帧
  - 视为连接级错误，清理对应连接
- server fd 失效
  - 视为 server 整体不可用，清理全部 client 连接
- client 侧连接失败
  - 不立即终止模块生命周期，而是保留未连接状态，由后续 `poll` 继续尝试连接

回调语义规划：

- 只有在收到**完整且符合协议**的数据包后才触发 `cb`
- `fd` 表示该数据来源的连接 fd
- `json` 指向回调时可读的原始 JSON 数据
- `len` 为有效字节长度
- `msg_t` 指向当前包对应的已解析消息对象
- `msg_t` 的释放责任属于底层
- `msg_t` 仅在回调执行期间有效
- 回调是否允许长期持有该 `json`，需要在后续接口清单中明确
- 当前规划更倾向于：
  - 回调期间可读
  - 回调返回后缓冲区所有权仍归底层
  - 若上层要长期保存，必须自行拷贝

### `agorahex_signal_close`

输入：

- 无参数

行为：

- 关闭并清理当前信令层持有的所有资源
- 在 `server_mode` 下：
  - 关闭 server fd
  - 关闭所有 client list 中的 fd
  - 释放每个连接关联的拆帧状态
- 在 `client_mode` 下：
  - 关闭 client fd
  - 释放 client 侧关联的拆帧状态
- 清理公共状态：
  - 清空回调指针
  - 清空连接状态
  - 清空 `server_mode`、端口、fd、连接计数等运行时状态
  - 将模块恢复为“未启动”状态

关闭语义规划：

- 允许在任意时刻调用
- 如果当前尚未成功 `start`，调用应安全返回
- 如果某些 fd 已经失效，`close` 仍应继续清理其余资源，而不是中途停止
- `close` 调用后，再次调用 `poll` 应直接返回“未启动”或等价错误
- `close` 调用后，允许再次调用 `start` 重新初始化

## 线程模型

线程约束按当前场景明确如下：

- 本层默认**非线程安全**
- 外部接口调用采用**同一个线程串行调用**模型
- 按当前接口集合，建议以下四个接口都由同一线程调用：
  - `agorahex_signal_start`
  - `agorahex_signal_send`
  - `agorahex_signal_poll`
  - `agorahex_signal_close`
- 查询、调度、管理线程放在 app 层，本层不负责线程同步

## 分阶段规划

### 阶段 1：接口盘点与基线冻结

目标：

- 明确当前已暴露的全部函数、类型、常量和宏
- 形成一份“当前接口基线”，作为后续 TCP server/client 模式演进依据

计划项：

- 为 `framing.h`、`envelope.h`、`types.h`、`result.h` 建立接口清单
- 区分两类边界：
  - 协议核心接口
  - server/client 运行时依赖的网络封装接口
- 逐项标记：
  - 输入参数是否允许为 `NULL`
  - 调用前置条件
  - 返回值语义
  - 是否分配堆内存
  - 是否需要调用方补充清理
- 明确哪些行为是“当前实现约定”，哪些行为属于“稳定对外承诺”

验收标准：

- 能形成一张完整接口表
- 新接手的调用方无需阅读 `src/` 即可完成基础接入

### 阶段 2：资源管理与错误处理约定补齐

目标：

- 降低接口误用概率，尤其是内存释放、decoder 状态机误用，以及 TCP 收发异常后的恢复行为不明确问题

计划项：

- 补齐以下接口约定说明：
  - `agorahex_message_free` 的适用范围
  - `agorahex_parse_envelope` 成功/失败后的对象状态
  - `agorahex_marshal_envelope` 返回缓冲区的释放责任
  - `agorahex_frame_decoder_init/reset/fini` 的调用顺序
  - `agorahex_frame_decoder_append` 在遇到坏 magic、超长帧、回调返回错误后的恢复方式
  - server/client 模式下，读到半包、对端关闭、协议错误、连续异常时的处理策略
- 建立“常见误用示例”清单，例如：
  - 忘记释放 `agorahex_message_t` 内部字符串
  - decoder 出错后未 `reset` 就继续 `append`
  - 对非 NUL 结尾 JSON 的调用方式理解错误

验收标准：

- 关键接口都有所有权与错误恢复说明
- 示例程序和测试中的调用路径能够覆盖这些约定

### 阶段 3：接口文档与使用示例标准化

目标：

- 让公开头文件、README、协议文档、示例程序之间的描述一致，并能直接支撑 TCP server/client 模式接入

计划项：

- 统一以下文档视角：
  - README 面向使用者，强调如何构建、测试、运行
  - 协议计划文档面向实现和维护，强调协议语义和结构边界
  - 接口规划文档面向后续接口演进，强调契约和阶段任务
- 为以下典型场景补齐使用示例：
  - 单条 JSON 成帧发送
  - 流式读取 TCP 字节并拆帧
  - 解析 envelope 后读取 `kind` 和 `callId`
  - 构造消息并序列化为 JSON
  - 建立一个最小 server 循环
  - 建立一个最小 client 发送流程

验收标准：

- 同一接口在不同文档中的说法不冲突
- 示例程序能作为接口使用参考，而不只是 demo

### 阶段 4：接口可扩展性设计

目标：

- 为后续增加消息类型、增强 server/client 能力或扩展跨平台支持做好接口层准备

计划项：

- 评估新增消息类型时需要同步修改的层面：
  - `agorahex_kind_t`
  - `agorahex_message_t`
  - 解析与序列化分发逻辑
  - 测试样例与回归测试
- 评估是否需要补充辅助接口，例如：
  - 更细粒度的 kind 判断辅助函数
  - 更显式的初始化辅助函数
  - 面向嵌入式/低分配场景的接口变体
  - 面向不同 socket 后端的薄适配接口
- 明确版本演进原则：
  - 新增优先，避免破坏已有字段或函数签名

验收标准：

- 新增消息类型的改动范围可预估、可复用
- 对外接口演进有明确约束，不依赖临时判断

## 交付物规划

建议最终形成以下几类产物：

- 一份 TCP server/client 总体接口说明
- 一份公开接口清单文档
- 一份资源所有权与错误处理约定文档
- 一组最小可运行的 server/client 示例
- 一组覆盖主要误用路径的测试用例
- 一份接口演进规则，约束后续新增消息或新增辅助函数的方式

## 风险与关注点

1. **文档与实现漂移**
   - 如果只补文档、不建立检查机制，后续实现修改后仍可能偏离文档。

2. **结构体字段过于暴露**
   - 当前 `types.h` 直接暴露了较多消息结构字段，后续演进时需要谨慎处理兼容性。

3. **错误恢复语义被误解**
   - `agorahex_frame_decoder_t` 的 `last_error` 是粘滞的，这一点如果文档不明确，调用方很容易误用。

4. **样例驱动的隐性约束**
   - 当前嵌套字段解析较依赖 `docs/json_msg/` 样例形状，后续若样例变化，需要同步确认接口承诺边界。

5. **标准 C 与网络平台能力的边界不清**
   - 如果目标表述成“全部只用标准 C”，很容易忽略 socket API 的平台差异，最终导致实现方案失焦。

## 建议执行顺序

1. 先明确“协议核心层”和“TCP server/client 网络层”的边界。
2. 再冻结当前公开接口清单。
3. 然后补资源所有权、错误码和生命周期文档。
4. 再统一 README、协议文档和示例程序描述。
5. 最后讨论接口扩展策略和版本演进规则。

## 非目标

以下内容不属于本规划直接处理范围：

- 协议业务状态机本身的实现
- 复杂的高可用 server 框架能力
- 非 C 语言绑定
- ABI 稳定性承诺的正式发布流程

## 结论

当前仓库已经具备 TCP 协议处理和基础 client/server 示例的雏形。下一步重点是围绕“**标准 C 协议核心 + 薄网络适配层 + 清晰的 TCP server/client 模式**”这条主线，把公开接口契约讲清楚、固定住，并让示例、测试、文档三者对齐。`plan-interface.md` 的作用，就是为这件事提供一个明确、可执行的阶段路线图。
