# UDP 连通性测试工具

[English](README_EN.md) | 简体中文

一个 UDP 网络连通性测试工具。**单二进制、双模式**，使用 C 语言实现，全静态编译零依赖。

**一个命令搞定两端：** 服务端 `udp-test -d`、客户端 `udp-test -m "ping" -s <IP>`

## 功能特点

- **单二进制，双模式**：同一文件通过参数切换服务端/客户端
- **C 语言实现**：性能极高，内存占用 < 1 MB
- **零依赖部署**：静态编译，不依赖 Python、Go、glibc 版本
- **IPv4/IPv6 智能选择**：自动检测本机网络，优先 IPv6
- **跨平台**：预编译 Linux、macOS、Windows（amd64 / arm64 / i386）
- **服务端**：监听 UDP 端口，接收消息并回复 "Hello UDP"
- **客户端**：发送自定义消息，等待响应，支持超时

## 快速开始

从 Releases 页面下载对应系统的版本。

### 服务端模式

```bash
# 默认端口 12345
./udp-test -d

# 自定义端口
./udp-test -d -p 8080

# 注册为 systemd 服务（Linux）
sudo cp udp-test /usr/local/bin/
sudo cp udp-test.service /etc/systemd/system/
sudo systemctl enable --now udp-test
```

### 客户端模式

```bash
# 默认参数
./udp-test

# 自定义消息、服务器、端口和超时
./udp-test -m "hello" -s example.com -p 8080 -w 3
```

## 参数说明

| 参数 | 长参数          | 说明             | 默认值                       |
| ---- | --------------- | ---------------- | ---------------------------- |
| `-d` | `--server`      | 服务端模式       | 客户端模式                   |
| `-m` | `--message`     | 发送的消息       | `"ping"`                     |
| `-s` | `--server-addr` | 目标服务器地址   | `udp-test.cloudflare.now.cc` |
| `-p` | `--port`        | 端口（两端通用） | `12345`                      |
| `-w` | `--timeout`     | 超时秒数         | `2`                          |
| `-4` | `--ipv4`        | 强制使用 IPv4    | 系统自动选择                 |
| `-6` | `--ipv6`        | 强制使用 IPv6    | 系统自动选择                 |
| `-v` | `--version`     | 显示版本号       | —                            |
| `-h` | `--help`        | 显示帮助信息     | —                            |

## 从源码编译

```bash
# Linux amd64
gcc -Wall -O2 -s -static -o udp-test src/udp-test.c

# Linux arm64（需要交叉编译工具链）
aarch64-linux-gnu-gcc -Wall -O2 -s -static -o udp-test src/udp-test.c

# macOS
gcc -Wall -O2 -s -o udp-test src/udp-test.c

# Windows（需要 mingw-w64）
x86_64-w64-mingw32-gcc -Wall -O2 -s -static -o udp-test.exe src/udp-test.c -lws2_32
```

## 项目结构

```
udp-test/
├── src/
│   └── udp-test.c         # 源代码（服务端+客户端合一的 C 实现）
├── udp-test.service        # systemd 服务文件
├── .github/workflows/      # CI/CD 配置
├── README.md / README_EN.md
└── LICENSE
```

## 许可证

[MIT License](LICENSE)

## 贡献

欢迎提交 Issue 和 Pull Request！
