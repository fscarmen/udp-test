# UDP Connectivity Testing Tool

English | [简体中文](README.md)

A UDP network connectivity testing tool. **Single binary, dual mode**, implemented in C with fully static compilation and zero dependencies.

**One command for both ends:** Server `udp-test -d`, Client `udp-test -m "ping" -s <IP>`

## Features

- **Single binary, dual mode**: One file acts as client or server via command-line flags
- **C implementation**: High performance, memory footprint < 1 MB
- **Zero dependency**: Statically compiled, no Python, Go, or glibc version required
- **IPv4/IPv6 smart selection**: Auto-detects local network support, prefers IPv6
- **Cross-platform**: Pre-built for Linux, macOS, Windows (amd64 / arm64 / i386)
- **Server mode**: Listens on UDP port, replies with "Hello UDP"
- **Client mode**: Sends custom messages, waits for response with timeout

## Quick Start

Download the appropriate binary for your platform from the Releases page.

### Server Mode

```bash
# Default port 12345
./udp-test -d

# Custom port
./udp-test -d -p 8080

# Install as systemd service (Linux)
sudo cp udp-test /usr/local/bin/
sudo cp udp-test.service /etc/systemd/system/
sudo systemctl enable --now udp-test
```

### Client Mode

```bash
# Default parameters
./udp-test

# Custom message, server, port and timeout
./udp-test -m "hello" -s example.com -p 8080 -w 3
```

## Options

| Flag | Long Flag       | Description                 | Default                      |
| ---- | --------------- | --------------------------- | ---------------------------- |
| `-d` | `--server`      | Server mode                 | Client mode                  |
| `-m` | `--message`     | Message to send             | `"ping"`                     |
| `-s` | `--server-addr` | Target server address       | `udp-test.cloudflare.now.cc` |
| `-p` | `--port`        | Port (works for both modes) | `12345`                      |
| `-w` | `--timeout`     | Timeout in seconds          | `2`                          |
| `-4` | `--ipv4`        | Force IPv4 only             | Auto-detect                  |
| `-6` | `--ipv6`        | Force IPv6 only             | Auto-detect                  |
| `-v` | `--version`     | Show version                | —                            |
| `-h` | `--help`        | Show help                   | —                            |

## Build from Source

```bash
# Linux amd64
gcc -Wall -O2 -s -static -o udp-test src/udp-test.c

# Linux arm64 (requires cross-compilation toolchain)
aarch64-linux-gnu-gcc -Wall -O2 -s -static -o udp-test src/udp-test.c

# macOS
gcc -Wall -O2 -s -o udp-test src/udp-test.c

# Windows (requires mingw-w64)
x86_64-w64-mingw32-gcc -Wall -O2 -s -static -o udp-test.exe src/udp-test.c -lws2_32
```

## Project Structure

```
udp-test/
├── src/
│   └── udp-test.c         # Source code (single C file for both server & client)
├── udp-test.service        # systemd service file
├── .github/workflows/      # CI/CD configuration
├── README.md / README_EN.md
└── LICENSE
```

## License

[MIT License](LICENSE)

## Contributing

Issues and Pull Requests are welcome!
