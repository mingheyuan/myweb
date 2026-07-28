# myweb

`myweb` 是一个用于学习 Linux 网络编程的 C++11 轻量 HTTP 服务器。项目使用非阻塞 Socket、epoll、工作线程池、定时器、日志组件和可选的 MySQL 连接池。

## 功能

- 支持 `GET` 和 `POST` 的 HTTP/1.0、HTTP/1.1 请求解析。
- 基于 epoll 的事件循环，支持 LT/ET 触发模式组合。
- 支持 Proactor 和 Reactor 两种执行模型。
- 工作线程池和连接超时管理。
- 提供健康检查和示例文本路由。
- 使用 MySQL 实现 `/register` 和 `/login`。
- 支持同步或异步日志。

## 路由

| 方法 | 路径 | 说明 |
| --- | --- | --- |
| `GET` | `/` | 返回服务器状态 |
| `GET` | `/hello` | 返回 Hello 文本 |
| `GET` | `/about` | 返回项目说明 |
| `POST` | `/echo` | 原样返回请求体 |
| `POST` | `/register` | 使用 `username` 和 `password` 注册用户 |
| `POST` | `/login` | 校验用户密码 |

数据库接口接收 `application/x-www-form-urlencoded` 数据，使用 `user(username, passwd)` 表，并以明文保存密码，因此只适合本地学习。

## 环境要求

项目依赖 Linux API，建议在 Linux 上构建：

- 支持 C++11 的编译器。
- GNU Make。
- pthread。
- MySQL 客户端开发头文件和库，例如 `libmysqlclient-dev`。
- 用于冒烟测试的 `curl`。
- 用于可选压测脚本的 `webbench`。

## 构建和运行

```bash
./build.sh
./server
```

默认 HTTP 端口为 `9006`。不启用 MySQL 连接池时可以运行：

```bash
./server -s 0
```

默认数据库配置为 `127.0.0.1:3306`、用户 `root`、密码 `123456`、数据库 `yourdb`。实际环境请显式传入数据库参数，不要依赖这些开发默认值。

示例数据库初始化：

```sql
CREATE DATABASE yourdb;
USE yourdb;
CREATE TABLE user (
    username VARCHAR(64) PRIMARY KEY,
    passwd VARCHAR(64) NOT NULL
);
```

冒烟测试：

```bash
curl -i http://127.0.0.1:9006/hello
curl -i -X POST http://127.0.0.1:9006/echo -d 'hello myweb'
curl -i -X POST http://127.0.0.1:9006/register -d 'username=alice&password=secret'
curl -i -X POST http://127.0.0.1:9006/login -d 'username=alice&password=secret'
```

## 命令行参数

| 参数 | 说明 | 默认值 |
| --- | --- | --- |
| `-p` | HTTP 监听端口 | `9006` |
| `-l` | 日志模式：`0` 同步，`1` 异步 | `0` |
| `-c` | 设置为 `1` 时关闭日志 | `0` |
| `-t` | 工作线程数 | `4` |
| `-T` | 连接超时时间，单位秒 | `15` |
| `-s` | MySQL 连接池大小，`0` 表示关闭 | `4` |
| `-m` | 触发模式：`0` LT/LT、`1` LT/ET、`2` ET/LT、`3` ET/ET | `0` |
| `-a` | 执行模型：`0` Proactor、`1` Reactor | `0` |
| `-H` | MySQL 主机 | `127.0.0.1` |
| `-U` | MySQL 用户 | `root` |
| `-W` | MySQL 密码 | `123456` |
| `-D` | MySQL 数据库 | `yourdb` |
| `-P` | MySQL 端口 | `3306` |

高并发配置示例：

```bash
./server -p 9006 -c 1 -s 0 -m 3 -a 0 -t 8
```

## 压测

`benchmark_5000.sh` 使用 `webbench` 测试 4 种触发模式和执行模型组合。先构建服务；如果 webbench 不在脚本默认路径，可以指定 `WB_BIN`：

```bash
WB_BIN=/path/to/webbench SERVER_BIN=./server bash benchmark_5000.sh
```

客户端数量、持续时间、线程数和结果文件可以通过 `CLIENTS`、`DURATION`、`THREADS`、`OUT_FILE` 覆盖。已有结果和注意事项见 [PERFORMANCE.md](PERFORMANCE.md)。

## 主要文件

- `webserver.cpp`：Socket、epoll、事件循环和模式配置。
- `http_conn.cpp`：HTTP 解析、路由和响应构造。
- `threadpool.cpp`：工作队列和任务执行。
- `timer.cpp`：连接超时管理。
- `sql_connection_pool.cpp`：MySQL 连接池和 RAII 封装。
- `logger.cpp`：同步和异步日志。

## 安全说明

项目尚未达到生产要求：密码以明文保存和比对，传输未加密，认证逻辑较简单，数据库默认凭据也仅用于开发环境。
