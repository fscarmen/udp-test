// Copyright (c) 2025 fscarmen
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

package main

import (
	"flag"
	"fmt"
	"net"
	"os"
	"strings"
	"time"
)

const Version = "v1.0.1"

// 检测 IPv6 地址格式
func isIPv6(address string) bool {
	return strings.Contains(address, ":")
}

// 规范化 IPv6 地址（移除方括号如果存在）
func normalizeIPv6(address string) string {
	if strings.HasPrefix(address, "[") && strings.HasSuffix(address, "]") {
		return address[1 : len(address)-1]
	}
	return address
}

// 检测本机网络环境支持情况
func detectNetworkSupport() (bool, bool) {
	ipv4Support := false
	ipv6Support := false

	// 测试 IPv4 支持
	if conn, err := net.Dial("udp4", "8.8.8.8:53"); err == nil {
		conn.Close()
		ipv4Support = true
	}

	// 测试 IPv6 支持
	if conn, err := net.Dial("udp6", "[2001:4860:4860::8888]:53"); err == nil {
		conn.Close()
		ipv6Support = true
	}

	return ipv4Support, ipv6Support
}

// 智能选择可用地址
func selectBestAddress(server string, port int) (string, string, error) {
	// 检测本机网络支持
	ipv4Support, ipv6Support := detectNetworkSupport()

	// 如果是 IPv6 地址，先规范化
	if isIPv6(server) {
		server = normalizeIPv6(server)
	}

	// 尝试解析地址
	addresses, err := net.LookupHost(server)
	if err != nil {
		// 如果解析失败，可能是 IP 地址，直接使用
		if net.ParseIP(server) != nil {
			addresses = []string{server}
		} else {
			return "", "", fmt.Errorf("failed to resolve hostname: %v", err)
		}
	}

	// 分离 IPv4 和 IPv6 地址
	var ipv4Addrs, ipv6Addrs []string
	for _, addr := range addresses {
		ip := net.ParseIP(addr)
		if ip != nil {
			if ip.To4() != nil {
				ipv4Addrs = append(ipv4Addrs, addr)
			} else {
				ipv6Addrs = append(ipv6Addrs, addr)
			}
		}
	}

	// 根据本机网络支持情况选择地址
	if ipv6Support && len(ipv6Addrs) > 0 {
		// 优先选择 IPv6（如果本机支持且有 IPv6 地址）
		addr := ipv6Addrs[0]
		return addr, "udp6", nil
	} else if ipv4Support && len(ipv4Addrs) > 0 {
		// 选择 IPv4
		addr := ipv4Addrs[0]
		return addr, "udp4", nil
	}

	// 如果都不支持，返回错误
	if !ipv4Support && !ipv6Support {
		return "", "", fmt.Errorf("no network connectivity available")
	}

	return "", "", fmt.Errorf("no compatible address found for available network")
}

func main() {
	// 定义命令行参数
	var (
		message = flag.String("m", "ping", "Message to send")
		server  = flag.String("s", "udp-test.cloudflare.now.cc", "Target server address")
		port    = flag.Int("p", 12345, "Target port")
		timeout = flag.Int("w", 2, "Timeout in seconds")
		version = flag.Bool("v", false, "Show version information")
		help    = flag.Bool("h", false, "Show help information")
	)

	// 自定义 Usage 输出
	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, "Usage of %s:\n", os.Args[0])
		fmt.Fprintf(os.Stderr, "A UDP client tool that sends messages to a specified server\n\n")
		fmt.Fprintf(os.Stderr, "Options:\n")
		flag.PrintDefaults()
		fmt.Fprintf(os.Stderr, "\nExample:\n")
		fmt.Fprintf(os.Stderr, "  %s -m \"hello\" -s example.com -p 8080 -w 3\n", os.Args[0])
		fmt.Fprintf(os.Stderr, "  %s -m \"test\" -s 2001:db8::1 -p 12345\n", os.Args[0])
		fmt.Fprintf(os.Stderr, "  %s -m \"test\" -s [2001:db8::1] -p 12345\n", os.Args[0])
	}

	flag.Parse()

	// 如果指定了 -v 参数，显示版本信息并退出
	if *version {
		fmt.Printf("udp-test %s\n", Version)
		os.Exit(0)
	}

	// 如果指定了 -h 参数，显示帮助信息并退出
	if *help {
		flag.Usage()
		os.Exit(0)
	}

	// 智能选择最佳地址
	bestAddr, network, err := selectBestAddress(*server, *port)
	if err != nil {
		fmt.Printf("Address selection failed: %v\n", err)
		os.Exit(1)
	}

	// 构建目标地址
	var address string
	if network == "udp6" {
		address = fmt.Sprintf("[%s]:%d", bestAddr, *port)
	} else {
		address = fmt.Sprintf("%s:%d", bestAddr, *port)
	}

	fmt.Printf("Using %s address: %s\n", strings.ToUpper(network[3:]), address)

	// 解析地址
	remoteAddr, err := net.ResolveUDPAddr(network, address)
	if err != nil {
		fmt.Printf("Failed to resolve address: %v\n", err)
		os.Exit(1)
	}

	// 创建 UDP 连接
	conn, err := net.DialUDP(network, nil, remoteAddr)
	if err != nil {
		fmt.Printf("Connection failed: %v\n", err)
		os.Exit(1)
	}
	defer conn.Close()

	// 设置读写超时
	timeoutDuration := time.Duration(*timeout) * time.Second
	deadline := time.Now().Add(timeoutDuration)
	err = conn.SetDeadline(deadline)
	if err != nil {
		fmt.Printf("Failed to set timeout: %v\n", err)
		os.Exit(1)
	}

	// 发送数据
	_, err = conn.Write([]byte(*message))
	if err != nil {
		fmt.Printf("Failed to send data: %v\n", err)
		os.Exit(1)
	}

	// 准备接收响应
	buffer := make([]byte, 1024)
	n, remoteAddr, err := conn.ReadFromUDP(buffer)
	
	if err != nil {
		if err, ok := err.(net.Error); ok && err.Timeout() {
			fmt.Printf("Message '%s' sent to %s, but no response received (UDP may be blocked)\n", *message, address)
			return
		}
		fmt.Printf("Failed to receive response: %v\n", err)
		return
	}

	// 如果收到响应，打印出来
	if n > 0 {
		fmt.Printf("Received response from %s: %s\n", remoteAddr.String(), string(buffer[:n]))
	}
}