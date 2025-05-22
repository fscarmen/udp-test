# UDP 连通性测试工具

[English](README_EN.md) | 简体中文

这是一个专门用于测试 UDP 网络通信的工具，包含客户端和服务器端组件。客户端使用 Go 语言实现以确保跨平台兼容性和零依赖部署，服务器端则采用 Python 实现以简化部署和维护。

## 功能特点

### 客户端（Go 语言实现）
* 支持自定义消息发送
* 可配置目标服务器地址和端口
* 内置超时控制
* 支持命令行参数配置
* 跨平台支持（Windows、Linux、macOS、Alpine Linux）
* 零依赖部署：单一二进制文件，无需安装任何依赖
* 静态编译：确保在任何环境下都能运行

### 服务器端（Python 实现）
* 简单的 UDP 服务器
* 支持自定义端口
* 优雅的信号处理
* 进程标题自定义
* 轻量级部署：仅需 Python 环境和一个依赖包
* 跨平台兼容：一份代码适用所有平台，无需编译

## 快速开始
### 客户端
从 Releases 页面下载适合你系统的版本。

或者从源码编译：

```
cd client
go build
```
### 服务器端
安装依赖并运行：

```
pip3 install setproctitle
python3 server/udp-server.py
```
## 使用方法
### 客户端
基本用法：

```
./udp-test
```
参数说明：

- -m : 要发送的消息（默认: "ping"）
- -s : 目标服务器地址（默认: "udp-test.cloudflare.now.cc"）
- -p : 目标端口（默认: 12345）
- -w : 超时时间（秒）（默认: 2）
- -h : 显示帮助信息

示例：

```
./udp-test -m "hello" -s example.
com -p 8080 -w 3
```

成功显示服务器响应：
```
Received response from <IP>:<PORT>: Hello UDP
```

### 服务器端
直接运行（可选指定端口）：

```
python3 udp-server.py [端口]
```
默认端口为 12345。

## 构建说明
本项目支持多平台交叉编译：

- Windows (x86, x64, ARM64)
- Linux (x86, x64, ARM, ARM64, Alpine)
- macOS (x64, ARM64)
## 许可证
[MIT License](LICENSE)

## 贡献
欢迎提交 Issue 和 Pull Request！