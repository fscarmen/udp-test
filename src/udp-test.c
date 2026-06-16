// Copyright (c) 2025 fscarmen
//
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT
//
// udp-test — A UDP connectivity testing tool.
// Single binary, dual mode: server (-d) or client (default).
// Static compile with zero dependencies.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#ifdef _WIN32
    /* Windows needs winsock2.h before windows.h,
     * and must call WSAStartup() before using sockets.
     * POSIX headers (sys/socket.h, netinet/in.h, etc.)
     * are NOT available on Windows. */
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>

    #define CLOSE_SOCKET(s) closesocket(s)
    #define GET_ERRNO()     WSAGetLastError()
    #define SHUT_RDWR       SD_BOTH

    /* MinGW-w64 does provide unistd.h for getopt */
    #ifdef __MINGW32__
        #include <unistd.h>
    #endif
#else
    /* POSIX / Unix-like (Linux, macOS) */
    #include <unistd.h>
    #include <signal.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <errno.h>
    #include <sys/time.h>

    #define CLOSE_SOCKET(s) close(s)
    #define GET_ERRNO()     errno
#endif

/* ------------------------------------------------------------------ */
/*  Constants                                                         */
/* ------------------------------------------------------------------ */
#define VERSION         "v1.0.2"
#define DEFAULT_PORT    12345
#define DEFAULT_MSG     "ping"
#define DEFAULT_SERVER  "udp-test.cloudflare.now.cc"
#define DEFAULT_TIMEOUT 2
#define BUFFER_SIZE     1024
#define RESPONSE_STR    "Hello UDP"

/* ------------------------------------------------------------------ */
/*  Globals (needed by signal handler)                                 */
/* ------------------------------------------------------------------ */
static int g_server_fd = -1;

/* ------------------------------------------------------------------ */
/*  Signal handler (Unix only)                                         */
/* ------------------------------------------------------------------ */
#ifndef _WIN32
static void signal_handler(int sig) {
    const char *msg;
    if (sig == SIGINT)
        msg = "\nCaught SIGINT, exiting...\n";
    else
        msg = "\nCaught SIGTERM, exiting...\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    if (g_server_fd >= 0)
        CLOSE_SOCKET(g_server_fd);
    exit(0);
}

static void setup_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}
#endif

#ifdef _WIN32
static BOOL WINAPI win_ctrl_handler(DWORD dwCtrlType) {
    (void)dwCtrlType;
    fprintf(stderr, "\nCaught shutdown event, exiting...\n");
    if (g_server_fd >= 0)
        CLOSE_SOCKET(g_server_fd);
    exit(0);
    return TRUE;
}
#endif

/* ------------------------------------------------------------------ */
/*  Help / version                                                     */
/* ------------------------------------------------------------------ */
static void print_version(void) {
    printf("udp-test %s\n", VERSION);
}

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n", prog);
    printf("\n");
    printf("A UDP connectivity testing tool \xe2\x80\x94 client and server in one binary.\n");
    printf("Default mode is client; use -d / --server to run as server.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -d, --server            Run as server mode (default: client mode)\n");
    printf("  -m, --message <str>     Message to send (default: \"%s\")\n", DEFAULT_MSG);
    printf("  -s, --server-addr <host> Target server address (default: \"%s\")\n", DEFAULT_SERVER);
    printf("  -p, --port <int>        Port number (default: %d)\n", DEFAULT_PORT);
    printf("  -w, --timeout <sec>     Receive timeout in seconds (default: %d)\n", DEFAULT_TIMEOUT);
    printf("  -4, --ipv4              Force IPv4 only\n");
    printf("  -6, --ipv6              Force IPv6 only\n");
    printf("  -v, --version           Show version information (%s) and exit\n", VERSION);
    printf("  -h, --help              Show this help message and exit\n");
    printf("\n");
    printf("Examples:\n");
    printf("  # Start server on default port %d\n", DEFAULT_PORT);
    printf("  %s -d\n", prog);
    printf("\n");
    printf("  # Start server on custom port\n");
    printf("  %s -d -p 8080\n", prog);
    printf("\n");
    printf("  # Send a message to default server\n");
    printf("  %s -m \"ping\"\n", prog);
    printf("\n");
    printf("  # Force IPv4 / IPv6\n");
    printf("  %s -4 -m \"test\" -s example.com\n", prog);
    printf("  %s -6 -m \"test\" -s example.com\n", prog);
    printf("\n");
    printf("  # Send a message with custom server, port and timeout\n");
    printf("  %s -m \"hello\" -s example.com -p 8080 -w 3\n", prog);
    printf("\n");
    printf("  # Connect using IPv6 address\n");
    printf("  %s -m \"test\" -s 2001:db8::1 -p 12345\n", prog);
    printf("\n");
    printf("  # IPv6 address with brackets (also accepted)\n");
    printf("  %s -m \"test\" -s [2001:db8::1] -p 12345\n", prog);
    printf("\n");
    printf("  # Show version\n");
    printf("  %s -v\n", prog);
}

/* ------------------------------------------------------------------ */
/*  Utility helpers                                                    */
/* ------------------------------------------------------------------ */

/* Remove surrounding brackets from an IPv6 address if present */
static void normalize_ipv6(char *host, size_t len) {
    size_t slen = strlen(host);
    if (slen > 2 && host[0] == '[' && host[slen - 1] == ']') {
        memmove(host, host + 1, slen - 2);
        host[slen - 2] = '\0';
    }
    (void)len;
}

/* Check if string looks like an IPv6 address (contains ':') */
static bool looks_like_ipv6(const char *s) {
    return strchr(s, ':') != NULL;
}

/* Test local network support for IPv4 and IPv6 */
static void detect_network_support(bool *ipv4_ok, bool *ipv6_ok) {
    int fd;
    struct sockaddr_in  sa4;
    struct sockaddr_in6 sa6;

    *ipv4_ok = false;
    *ipv6_ok = false;

    /* Test IPv4 — try to create a UDP socket to 8.8.8.8:53 */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd >= 0) {
        memset(&sa4, 0, sizeof(sa4));
        sa4.sin_family = AF_INET;
        sa4.sin_port   = htons(53);
        inet_pton(AF_INET, "8.8.8.8", &sa4.sin_addr);
        if (connect(fd, (struct sockaddr *)&sa4, sizeof(sa4)) == 0)
            *ipv4_ok = true;
        CLOSE_SOCKET(fd);
    }

    /* Test IPv6 — try to create a UDP socket to 2001:4860:4860::8888:53 */
    fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (fd >= 0) {
        memset(&sa6, 0, sizeof(sa6));
        sa6.sin6_family = AF_INET6;
        sa6.sin6_port   = htons(53);
        inet_pton(AF_INET6, "2001:4860:4860::8888", &sa6.sin6_addr);
        if (connect(fd, (struct sockaddr *)&sa6, sizeof(sa6)) == 0)
            *ipv6_ok = true;
        CLOSE_SOCKET(fd);
    }
}

/*
 * Smart address selection: replicates Go client logic.
 * Steps:
 *   1. If force_family is AF_INET or AF_INET6, use it directly (skip detection).
 *   2. Detect local IPv4/IPv6 support.
 *   3. If target looks like IPv6, strip brackets.
 *   4. DNS-resolve the hostname.
 *   5. Separate resolved addresses into IPv4 and IPv6 lists.
 *   6. Prefer IPv6 if local supports it and we have IPv6 addrs.
 *   7. Fall back to IPv4.
 *   8. If all else fails, report error.
 */
static int select_best_address(const char *server_in,
                               char *out_addr, size_t out_addr_len,
                               int *out_family,
                               int force_family)
{
    char server[256];
    bool ipv4_support, ipv6_support;
    struct addrinfo  hints, *res, *rp;
    char addr_str[INET6_ADDRSTRLEN];

    /* Make a mutable copy */
    strncpy(server, server_in, sizeof(server) - 1);
    server[sizeof(server) - 1] = '\0';

    /* Normalize IPv6 brackets */
    if (looks_like_ipv6(server))
        normalize_ipv6(server, sizeof(server));

    /* Detect local network support */
    detect_network_support(&ipv4_support, &ipv6_support);

    if (!ipv4_support && !ipv6_support) {
        fprintf(stderr, "Address selection failed: no network connectivity available\n");
        return -1;
    }

    /* If it's a bare IP address, skip DNS */
    {
        struct in6_addr  test6;
        struct in_addr   test4;
        if (inet_pton(AF_INET6, server, &test6) == 1) {
            /* Pure IPv6 address */
            if (force_family == AF_INET) {
                fprintf(stderr, "Address selection failed: target is an IPv6 address but IPv4 is forced\n");
                return -1;
            }
            if (!ipv6_support) {
                fprintf(stderr, "Address selection failed: IPv6 not supported on this host\n");
                return -1;
            }
            strncpy(out_addr, server, out_addr_len);
            out_addr[out_addr_len - 1] = '\0';
            *out_family = AF_INET6;
            return 0;
        }
        if (inet_pton(AF_INET, server, &test4) == 1) {
            /* Pure IPv4 address */
            if (force_family == AF_INET6) {
                fprintf(stderr, "Address selection failed: target is an IPv4 address but IPv6 is forced\n");
                return -1;
            }
            if (!ipv4_support) {
                fprintf(stderr, "Address selection failed: IPv4 not supported on this host\n");
                return -1;
            }
            strncpy(out_addr, server, out_addr_len);
            out_addr[out_addr_len - 1] = '\0';
            *out_family = AF_INET;
            return 0;
        }
    }

    /* DNS resolution — restrict to forced family if specified */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = (force_family != AF_UNSPEC) ? force_family : AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    int ret = getaddrinfo(server, NULL, &hints, &res);
    if (ret != 0) {
#ifdef _WIN32
        fprintf(stderr, "Address selection failed: failed to resolve hostname (error %d)\n", ret);
#else
        fprintf(stderr, "Address selection failed: failed to resolve hostname: %s\n",
                gai_strerror(ret));
#endif
        return -1;
    }

    /* Separate into IPv4 / IPv6 */
    char *ipv4_list[64], *ipv6_list[64];
    int   ipv4_count = 0, ipv6_count = 0;

    for (rp = res; rp != NULL && ipv4_count < 64 && ipv6_count < 64; rp = rp->ai_next) {
        memset(addr_str, 0, sizeof(addr_str));
        if (rp->ai_family == AF_INET) {
            struct sockaddr_in *sin = (struct sockaddr_in *)rp->ai_addr;
            inet_ntop(AF_INET, &sin->sin_addr, addr_str, sizeof(addr_str));
            ipv4_list[ipv4_count] = strdup(addr_str);
            if (ipv4_list[ipv4_count]) ipv4_count++;
        } else if (rp->ai_family == AF_INET6) {
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)rp->ai_addr;
            inet_ntop(AF_INET6, &sin6->sin6_addr, addr_str, sizeof(addr_str));
            ipv6_list[ipv6_count] = strdup(addr_str);
            if (ipv6_list[ipv6_count]) ipv6_count++;
        }
    }
    freeaddrinfo(res);

    /*
     * Smart selection:
     * - If force_family is set, only use that family.
     * - Otherwise prefer IPv6 if supported locally.
     */
    if ((force_family == AF_UNSPEC || force_family == AF_INET6) &&
         ipv6_support && ipv6_count > 0) {
        strncpy(out_addr, ipv6_list[0], out_addr_len);
        out_addr[out_addr_len - 1] = '\0';
        *out_family = AF_INET6;
    } else if ((force_family == AF_UNSPEC || force_family == AF_INET) &&
                ipv4_support && ipv4_count > 0) {
        strncpy(out_addr, ipv4_list[0], out_addr_len);
        out_addr[out_addr_len - 1] = '\0';
        *out_family = AF_INET;
    } else {
        fprintf(stderr, "Address selection failed: no compatible address found "
                        "for available network\n");
        ret = -1;
        goto cleanup;
    }
    ret = 0;

cleanup:
    for (int i = 0; i < ipv4_count; i++) free(ipv4_list[i]);
    for (int i = 0; i < ipv6_count; i++) free(ipv6_list[i]);
    return ret;
}

/* Set socket timeout (Unix) */
static int set_socket_timeout(int fd, int seconds) {
    struct timeval tv;
    tv.tv_sec  = seconds;
    tv.tv_usec = 0;
#ifdef _WIN32
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv)) < 0)
#else
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
#endif
        return -1;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Server mode                                                        */
/* ------------------------------------------------------------------ */
static int run_server(int port) {
    int fd;
    struct sockaddr_in6 addr6;
    char buffer[BUFFER_SIZE];

    /* Create IPv6 socket (dual-stack: accepts both IPv4 and IPv6) */
    fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (fd < 0) {
        /* Fallback to IPv4-only if IPv6 socket fails */
        struct sockaddr_in addr4;
        fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0) {
            perror("socket");
            return 1;
        }
        g_server_fd = fd;

        /* SO_REUSEADDR */
        {
#ifdef _WIN32
            const char opt = 1;
#else
            int opt = 1;
#endif
            setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        }

        /* Bind IPv4 */
        memset(&addr4, 0, sizeof(addr4));
        addr4.sin_family      = AF_INET;
        addr4.sin_addr.s_addr = htonl(INADDR_ANY);
        addr4.sin_port        = htons((unsigned short)port);

        if (bind(fd, (struct sockaddr *)&addr4, sizeof(addr4)) < 0) {
            perror("bind");
            CLOSE_SOCKET(fd);
            return 1;
        }

        printf("UDP server running on port %d (IPv4 only)...\n", port);

        /* Signal handlers */
#ifndef _WIN32
        setup_signal_handlers();
#else
        SetConsoleCtrlHandler(win_ctrl_handler, TRUE);
#endif

        struct sockaddr_in cli;
        socklen_t          cli_len;

        while (1) {
            memset(buffer, 0, BUFFER_SIZE);
            cli_len = sizeof(cli);
            ssize_t n = recvfrom(fd, buffer, BUFFER_SIZE - 1, 0,
                                 (struct sockaddr *)&cli, &cli_len);
            if (n < 0) {
#ifdef _WIN32
                if (WSAGetLastError() == WSAEINTR) break;
#endif
                perror("recvfrom");
                continue;
            }
            buffer[n] = '\0';

            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof(ip));
            int client_port = ntohs(cli.sin_port);
            printf("Received: %s from %s:%d\n", buffer, ip, client_port);

            ssize_t sent = sendto(fd, RESPONSE_STR, strlen(RESPONSE_STR), 0,
                                  (struct sockaddr *)&cli, cli_len);
            if (sent < 0)
                perror("sendto");
        }

        CLOSE_SOCKET(fd);
        return 0;
    }
    g_server_fd = fd;

    /* SO_REUSEADDR */
    {
#ifdef _WIN32
        const char opt = 1;
#else
        int opt = 1;
#endif
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }

    /* Disable IPV6_V6ONLY so the socket accepts both IPv4 and IPv6 (dual-stack) */
    {
#ifdef _WIN32
        const char v6only = 0;
#else
        int v6only = 0;
#endif
        setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));
    }

    /* Bind IPv6 (also accepts IPv4 via mapped addresses) */
    memset(&addr6, 0, sizeof(addr6));
    addr6.sin6_family = AF_INET6;
    addr6.sin6_addr   = in6addr_any;
    addr6.sin6_port   = htons((unsigned short)port);

    if (bind(fd, (struct sockaddr *)&addr6, sizeof(addr6)) < 0) {
        perror("bind");
        CLOSE_SOCKET(fd);
        return 1;
    }

    printf("UDP server running on port %d (IPv4/IPv6 dual-stack)...\n", port);
    printf("Press Ctrl+C to exit server\n");

    /* Signal handlers */
#ifndef _WIN32
    setup_signal_handlers();
#else
    SetConsoleCtrlHandler(win_ctrl_handler, TRUE);
#endif

    struct sockaddr_storage cli;
    socklen_t               cli_len;

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        cli_len = sizeof(cli);
        ssize_t n = recvfrom(fd, buffer, BUFFER_SIZE - 1, 0,
                             (struct sockaddr *)&cli, &cli_len);
        if (n < 0) {
#ifdef _WIN32
            if (WSAGetLastError() == WSAEINTR) break;
#endif
            perror("recvfrom");
            continue;
        }
        buffer[n] = '\0';

        char ip[INET6_ADDRSTRLEN];
        int  client_port;

        if (cli.ss_family == AF_INET) {
            struct sockaddr_in *sin = (struct sockaddr_in *)&cli;
            inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip));
            client_port = ntohs(sin->sin_port);
        } else {
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&cli;
            inet_ntop(AF_INET6, &sin6->sin6_addr, ip, sizeof(ip));
            client_port = ntohs(sin6->sin6_port);

            /* Unmap IPv4-mapped IPv6 addresses (::ffff:x.x.x.x) */
            if (strncmp(ip, "::ffff:", 7) == 0) {
                char *v4 = ip + 7;
                memmove(ip, v4, strlen(v4) + 1);
            }
        }
        printf("Received: %s from %s:%d\n", buffer, ip, client_port);

        ssize_t sent = sendto(fd, RESPONSE_STR, strlen(RESPONSE_STR), 0,
                              (struct sockaddr *)&cli, cli_len);
        if (sent < 0)
            perror("sendto");
    }

    CLOSE_SOCKET(fd);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Client mode                                                        */
/* ------------------------------------------------------------------ */
static int run_client(const char *message, const char *server, int port, int timeout,
                      int force_family) {
    char    best_addr[INET6_ADDRSTRLEN];
    int     family;
    int     fd;
    char    addr_str[256];
    char    buffer[BUFFER_SIZE];

    /* Smart address selection (pass force_family) */
    if (select_best_address(server, best_addr, sizeof(best_addr), &family, force_family) != 0)
        return 1;

    /* Build address string and print */
    if (family == AF_INET6)
        snprintf(addr_str, sizeof(addr_str), "[%s]:%d", best_addr, port);
    else
        snprintf(addr_str, sizeof(addr_str), "%s:%d", best_addr, port);

    printf("Using %s address: %s\n",
           (family == AF_INET6) ? "IPv6" : "IPv4", addr_str);

    /* Create socket */
    fd = socket(family, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    /* Set timeout */
    if (set_socket_timeout(fd, timeout) < 0) {
        perror("setsockopt timeout");
        CLOSE_SOCKET(fd);
        return 1;
    }

    /* Resolve destination */
    struct addrinfo  hints, *res;
    char             port_str[16];
    struct addrinfo *rp;

    snprintf(port_str, sizeof(port_str), "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = family;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(best_addr, port_str, &hints, &res) != 0) {
        fprintf(stderr, "Failed to resolve address: %s\n", addr_str);
        CLOSE_SOCKET(fd);
        return 1;
    }

    int sent = -1;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sent = sendto(fd, message, strlen(message), 0,
                      rp->ai_addr, (socklen_t)rp->ai_addrlen);
        if (sent >= 0) break;
    }

    if (sent < 0) {
        perror("sendto");
        freeaddrinfo(res);
        CLOSE_SOCKET(fd);
        return 1;
    }

    /* Wait for response */
    memset(buffer, 0, BUFFER_SIZE);
    struct sockaddr_storage from;
    socklen_t from_len = sizeof(from);

    ssize_t n = recvfrom(fd, buffer, BUFFER_SIZE - 1, 0,
                         (struct sockaddr *)&from, &from_len);

    if (n < 0) {
        /* Timeout */
#ifdef _WIN32
        int wsa_err = WSAGetLastError();
        if (wsa_err == WSAETIMEDOUT || wsa_err == WSAEWOULDBLOCK) {
#else
        if (GET_ERRNO() == EAGAIN || GET_ERRNO() == EWOULDBLOCK) {
#endif
            printf("Message '%s' sent to %s, but no response received "
                   "(UDP may be blocked)\n", message, addr_str);
        } else {
            perror("recvfrom");
        }
        freeaddrinfo(res);
        CLOSE_SOCKET(fd);
        return 1;
    }

    buffer[n] = '\0';

    /* Format remote address for display */
    char from_ip[INET6_ADDRSTRLEN];
    int  from_port = 0;

    if (from.ss_family == AF_INET) {
        struct sockaddr_in *sin = (struct sockaddr_in *)&from;
        inet_ntop(AF_INET, &sin->sin_addr, from_ip, sizeof(from_ip));
        from_port = ntohs(sin->sin_port);
    } else {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&from;
        inet_ntop(AF_INET6, &sin6->sin6_addr, from_ip, sizeof(from_ip));
        from_port = ntohs(sin6->sin6_port);
    }

    printf("Received response from %s:%d: %s\n", from_ip, from_port, buffer);

    freeaddrinfo(res);
    CLOSE_SOCKET(fd);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Entry point                                                        */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[]) {
#ifdef _WIN32
    /* Windows requires WSAStartup before any Winsock calls */
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
#endif

    /* Defaults */
    int    port        = DEFAULT_PORT;
    char  *message     = NULL;  /* NULL means "use default" */
    char  *server      = NULL;  /* NULL means "use default" */
    int    timeout     = DEFAULT_TIMEOUT;
    bool   server_mode = false;
    bool   show_help   = false;
    bool   show_version= false;
    bool   force_v4    = false;
    bool   force_v6    = false;
    int    force_family= AF_UNSPEC;  /* AF_UNSPEC = system default, AF_INET = IPv4 only, AF_INET6 = IPv6 only */

    /*
     * Unified manual argument parser for all platforms.
     * Supports short options (-d -v -h -m -s -p -w -4 -6) and
     * long options (--server --version --help --message --server-addr
     * --port --timeout --ipv4 --ipv6).
     */
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (a[0] != '-' || a[1] == '\0') continue;

        /* Long options (--xxx) */
        if (a[1] == '-') {
            if (a[2] == '\0') continue; /* bare "--" */
            if (strcmp(a, "--server") == 0)      { server_mode = true; continue; }
            if (strcmp(a, "--help") == 0)         { show_help = true; continue; }
            if (strcmp(a, "--version") == 0)      { show_version = true; continue; }
            if (strcmp(a, "--ipv4") == 0)         { force_v4 = true; force_family = AF_INET; continue; }
            if (strcmp(a, "--ipv6") == 0)         { force_v6 = true; force_family = AF_INET6; continue; }
            if (strcmp(a, "--message") == 0)      { if (i+1 < argc) { message = argv[++i]; } continue; }
            if (strcmp(a, "--server-addr") == 0)  { if (i+1 < argc) { server = argv[++i]; } continue; }
            if (strcmp(a, "--port") == 0)         { if (i+1 < argc) { port = atoi(argv[++i]); } continue; }
            if (strcmp(a, "--timeout") == 0)      { if (i+1 < argc) { timeout = atoi(argv[++i]); } continue; }
            fprintf(stderr, "Unknown option: %s\n", a);
            print_usage(argv[0]);
            return 1;
        }

        /* Short options (-x, -x value, -xyz) */
        for (int j = 1; a[j] != '\0'; j++) {
            switch (a[j]) {
                case 'd': server_mode  = true; break;
                case 'v': show_version = true; break;
                case 'h': show_help    = true; break;
                case '4': force_v4     = true; force_family = AF_INET; break;
                case '6': force_v6     = true; force_family = AF_INET6; break;
                case 'm':
                    if (a[j+1] != '\0') { message = (char *)&a[j+1]; j = (int)strlen(a) - 1; }
                    else if (i+1 < argc) { message = argv[++i]; }
                    break;
                case 's':
                    if (a[j+1] != '\0') { server = (char *)&a[j+1]; j = (int)strlen(a) - 1; }
                    else if (i+1 < argc) { server = argv[++i]; }
                    break;
                case 'p':
                    if (a[j+1] != '\0') { port = atoi(&a[j+1]); j = (int)strlen(a) - 1; }
                    else if (i+1 < argc) { port = atoi(argv[++i]); }
                    break;
                case 'w':
                    if (a[j+1] != '\0') { timeout = atoi(&a[j+1]); j = (int)strlen(a) - 1; }
                    else if (i+1 < argc) { timeout = atoi(argv[++i]); }
                    break;
                default:
                    fprintf(stderr, "Unknown option: -%c\n", a[j]);
                    print_usage(argv[0]);
                    return 1;
            }
        }
    }

    /* Validation: -4 and -6 cannot be used together */
    if (force_v4 && force_v6) {
        fprintf(stderr, "Error: -4 / --ipv4 and -6 / --ipv6 cannot be used together\n");
        return 1;
    }

    /* Validate port */
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number: %d (must be 1-65535)\n", port);
        return 1;
    }

    /* Validate timeout */
    if (timeout <= 0) timeout = DEFAULT_TIMEOUT;

    /* Handle version / help */
    if (show_version) {
        print_version();
        return 0;
    }
    if (show_help) {
        print_usage(argv[0]);
        return 0;
    }

    /* Apply defaults */
    if (message == NULL) message = (char *)DEFAULT_MSG;
    if (server  == NULL) server  = (char *)DEFAULT_SERVER;

    int result;

    /* Run */
    if (server_mode) {
        result = run_server(port);
    } else {
        result = run_client(message, server, port, timeout, force_family);
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return result;
}
