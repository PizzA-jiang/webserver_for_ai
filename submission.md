# Submission

## 1. 已完成的功能或模块列表

### 核心服务模块
- `HttpServer` 类：封装 HTTP 服务端逻辑。
- TCP 监听：绑定端口并监听客户端连接。
- 连接接受：通过 `accept()` 接收用户请求。
- 请求读取：读取完整的 HTTP 报文头，直到 `\r\n\r\n`。
- 请求行解析：解析 `METHOD PATH VERSION`。
- 响应构建：根据请求结果返回不同 HTTP 响应。

### 已支持的 HTTP 行为
- `GET` 请求：返回 `200 OK`。
- 非 `GET` 请求：返回 `405 Method Not Allowed`。
- 非法请求行：返回 `400 Bad Request`。
- 连接关闭策略：响应后主动关闭连接，保持实现极简。

### 入口与配置
- `main.cpp`：作为程序入口，负责启动服务。
- 支持命令行参数设置端口，默认端口为 `8080`。
- `CMakeLists.txt`：已将服务端实现文件纳入构建目标。

### 文档与验证
- `README.md`：补充了构建、运行和快速测试命令。
- 已通过本地编译与 `curl` 测试验证基本功能。

---

## 2. 使用 Vibe Coding 进行项目开发的提示词日志

> 说明：以下为本项目开发过程中使用的核心提示词，按任务阶段整理。

### 阶段 1：需求拆解
- “构建一个 tinywebserver，仅包含主要功能接受和处理用户发来的 http 请求。”
- “请给出一个极简 C++ HTTP WebServer 的实现计划，要求只保留监听、accept、request line 解析和返回 response 的主要功能。”

### 阶段 2：功能实现
- “在现有 C++ 项目中新增 `http_server.h` 和 `http_server.cpp`，实现监听端口、接收连接、读取请求并返回响应。”
- “让入口程序支持启动 `HttpServer::run()`，并允许通过命令行传入端口号。”

### 阶段 3：构建与验证
- “把新的服务器实现加入构建配置，并补充最小测试说明。”
- “使用本地编译和 `curl` 验证 GET / POST 请求的响应行为。”

### 阶段 4：提交材料整理
- “添加 `submission.md`，包含已完成的功能或模块列表、Vibe Coding 提示词日志、GitHub repo 连接以及 commit history。”

---

## 3. GitHub 公共仓库连接

- GitHub Repo: `https://github.com/PizzA-jiang/webserver_for_ai.git`
- 远程名称: `origin`
- 说明: 请确认该仓库已设置为 **public**。

---

## 4. Commit History

当前仓库最近提交记录如下：

| Commit Hash | Message | Branch/Ref |
|---|---|---|
| `7f722a5` | `init` | `HEAD -> main` |
| `77fcbcb` | `Initial commit` | `origin/main` |

> 备注：当前仓库历史较短，仅包含以上 2 次提交。

---

## 5. 补充说明

- 本项目目标是一个极简 tiny webserver，重点是“能接受并处理 HTTP 请求”。
- 当前实现适合演示与教学用途；后续如果需要，可以继续扩展静态文件服务、多线程并发、HTTP header/body 解析等能力。

