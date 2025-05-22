// Copyright (c) 2023 fscarmen
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT

package main

import (
	"flag"
	"fmt"
	"net"
	"os"
	"time"
)

func main() {
	// 定义命令行参数
	var (
		message = flag.String("m", "ping", "Message to send")
		server  = flag.String("s", "udp-test.cloudflare.now.cc", "Target server address")
		port    = flag.Int("p", 12345, "Target port")
		timeout = flag.Int("w", 2, "Timeout in seconds")
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
	}

	flag.Parse()

	// 如果指定了 -h 参数，显示帮助信息并退出
	if *help {
		flag.Usage()
		os.Exit(0)
	}

	// 构建目标地址
	address := fmt.Sprintf("%s:%d", *server, *port)
	
	// 解析地址
	remoteAddr, err := net.ResolveUDPAddr("udp", address)
	if err != nil {
		fmt.Printf("Failed to resolve address: %v\n", err)
		os.Exit(1)
	}

	// 创建 UDP 连接
	conn, err := net.DialUDP("udp", nil, remoteAddr)
	if err != nil {
		fmt.Printf("Connection failed: %v\n", err)
		os.Exit(1)
	}
	defer conn.Close()

	// 设置读写超时
	deadline := time.Now().Add(time.Duration(*timeout) * time.Second)
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