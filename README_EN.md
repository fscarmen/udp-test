# UDP Connectivity Testing Tool

English | [简体中文](README.md)

A specialized tool for testing UDP network connectivity, featuring both client and server components. The client is implemented in Go to ensure cross-platform compatibility and zero-dependency deployment, while the server is implemented in Python for simplified deployment and maintenance.

## Features

### Client (Go Implementation)
* Custom message sending
* Configurable target server and port
* Built-in timeout control
* Command-line parameter support
* Cross-platform support (Windows, Linux, macOS, Alpine Linux)
* Zero-dependency deployment: Single binary file, no additional dependencies required
* Static compilation: Ensures operation in any environment

### Server (Python Implementation)
* Simple UDP server
* Custom port support
* Graceful signal handling
* Process title customization
* Lightweight deployment: Only requires Python environment and one dependency
* Cross-platform compatibility: Single codebase for all platforms, no compilation needed

## Quick Start

### Client

Download the appropriate version for your system from the [Releases](https://github.com/yourusername/udp-test/releases) page.

Or build from source:
```bash
cd client
go build
```

### Server

Install dependencies and run:
```bash
pip3 install setproctitle
python3 server/udp-server.py
```

## Usage

### Client

Basic usage:
```bash
./udp-test
```

Parameters:
- `-m`: Message to send (default: "ping")
- `-s`: Target server address (default: "udp-test.cloudflare.now.cc")
- `-p`: Target port (default: 12345)
- `-w`: Timeout in seconds (default: 2)
- `-h`: Show help information

Example:
```bash
./udp-test -m "hello" -s example.com -p 8080 -w 3
```

Successfully displayed the server response:
```
Received response from <IP>:<PORT>: Hello UDP
```

### Server

Direct run (optional port specification):
```bash
python3 udp-server.py [port]
```

Default port is 12345.

## Supported Platforms

Client supports the following platforms:
- Windows (x86, x64, ARM64)
- macOS (x64, ARM64)
- Linux (x86, x64, ARM, ARM64)
- Alpine Linux (x64, ARM64)

## Build Notes

This project uses GitHub Actions for automated builds, generating binaries for all platforms with each new release.

## License

[MIT License](LICENSE)

## Contributing

Issues and Pull Requests are welcome!
        