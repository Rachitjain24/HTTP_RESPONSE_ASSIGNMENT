//============================================================================
// Name        : gethttp_debug_proxy.cpp
// Author      : Bussysteme (modified with proxy support and debug prints)
// Version     : WiSe 24/25
// Description : HTTP GET with step-by-step output and optional proxy via
//               the http_proxy environment variable.
//============================================================================

#ifdef _WIN32
  #include <windows.h>
  #include <winsock.h>
#else
  #include <stddef.h>
  #include <stdio.h>
  #include <errno.h>
  #include <stdlib.h>
  #include <string.h>      // for strchr, strncmp, strlen, memcpy
  #include <unistd.h>      // for close()
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <netdb.h>
  #define SOCKADDR_IN struct sockaddr_in
  #define SOCKADDR    struct sockaddr
  #define HOSTENT     struct hostent
  #define SOCKET      int
  int WSAGetLastError() { return errno; }
  int closesocket(int s) { return close(s); }
#endif

#include <iostream>
#include <string>

void perr_exit(const char* msg, int ret_code)
{
    printf("ERROR: %s (code %d)\n", msg, ret_code);
    exit(ret_code);
}

int main(int argc, char** argv)
{
    const int bufsz = 4096;
    char url[1024];
    char send_buf[bufsz];
    char recv_buf[bufsz];
    SOCKET s;
    SOCKADDR_IN addr;
    HOSTENT* hent;
    char* site;
    char* host;
    long rc;

    // --- Step 1: WSAStartup (only on Windows) ---
#ifdef _WIN32
    printf("[Step 1] Initializing WinSock (WSAStartup)...\n");
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,0), &wsa))
        perr_exit("WSAStartup failed", WSAGetLastError());
#else
    printf("[Step 1] (Unix) No WSAStartup needed.\n");
#endif

    // --- Step 2: Prepare sockaddr structure ---
    printf("[Step 2] Preparing address structure...\n");
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(80);

    // --- Step 3: Prompt for URL ---
    printf("[Step 3] Asking for URL...\n");
    printf("URL: ");
    if (scanf("%1023s", url) != 1) {
        perr_exit("Failed to read URL", -1);
    }

    // --- Step 4: Parse the URL into host and site ---
    printf("[Step 4] Parsing URL...\n");
    if (strncmp("http://", url, 7) == 0) {
        host = url + 7;
    } else {
        host = url;
    }
    if ((site = strchr(host, '/')) != nullptr) {
        *site++ = '\0';
    } else {
        site = host + strlen(host);  // points at terminating '\0'
    }
    printf("        Parsed Host: %s\n", host);
    printf("        Parsed Site: %s\n", site);

    // --- Step 5: Check for http_proxy env variable ---
    const char* proxy_env = getenv("http_proxy");
    bool useProxy = false;
    std::string proxyHost;
    int proxyPort = 0;

    if (proxy_env && strncmp(proxy_env, "http://", 7) == 0) {
        const char* p = proxy_env + 7;               // after "http://"
        const char* colon = strchr(p, ':');
        if (colon) {
            proxyHost.assign(p, colon - p);
            proxyPort = atoi(colon + 1);
            if (proxyPort > 0) {
                useProxy = true;
                printf("[DEBUG] http_proxy detected → %s:%d\n",
                       proxyHost.c_str(), proxyPort);
            }
        }
    }
    if (!useProxy) {
        printf("[DEBUG] No valid http_proxy found → connecting directly\n");
    }

    // --- Step 6: Determine connect target (proxy or direct) ---
    std::string connectTo;
    int connectPort;
    if (useProxy) {
        connectTo   = proxyHost;
        connectPort = proxyPort;
    } else {
        connectTo   = host;    // original hostname from URL
        connectPort = 80;
    }

    // --- Step 7: Resolve connectTo hostname ---
    printf("[Step 5] Resolving '%s' ...\n", connectTo.c_str());
    addr.sin_port = htons(connectPort);
    if ((addr.sin_addr.s_addr = inet_addr(connectTo.c_str())) == INADDR_NONE) {
        printf("        inet_addr failed; calling gethostbyname for '%s'...\n",
               connectTo.c_str());
        if (!(hent = gethostbyname(connectTo.c_str())))
            perr_exit("Cannot resolve connectTo", WSAGetLastError());
        memcpy(&addr.sin_addr.s_addr, hent->h_addr, hent->h_length);
    }
    printf("        Resolved '%s' → %s:%d\n",
           connectTo.c_str(),
           inet_ntoa(addr.sin_addr),
           connectPort);

    // --- Step 8: Create socket ---
    printf("[Step 6] Creating socket...\n");
    s = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
    if (s == INVALID_SOCKET)
#else
    if (s < 0)
#endif
        perr_exit("Cannot create socket", WSAGetLastError());
    printf("        Socket created (fd=%d)\n", s);

    // --- Step 9: Connect to server (proxy or direct) ---
    printf("[Step 7] Connecting to %s:%d ...\n",
           connectTo.c_str(), connectPort);
    if (connect(s, (SOCKADDR*)&addr, sizeof(addr)) != 0)
        perr_exit("Cannot connect", WSAGetLastError());
    printf("        Connected to %s:%d\n",
           connectTo.c_str(), connectPort);

    // --- Step 10: Prepare and send HTTP GET request ---
    printf("[Step 8] Preparing HTTP GET request...\n");
    if (useProxy) {
        // When using a proxy, send full URL in GET line
        snprintf(
            send_buf, bufsz,
            "GET http://%s/%s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Connection: close\r\n"
            "\r\n",
            host, site, host
        );
    } else {
        // Direct connection: GET only the path
        snprintf(
            send_buf, bufsz,
            "GET /%s HTTP/1.1\r\n"
            "Host: %s:80\r\n"
            "Connection: close\r\n"
            "\r\n",
            site, host
        );
    }
    printf("        >>> Request >>>\n%s\n", send_buf);

    if ((send(s, send_buf, (int)strlen(send_buf), 0)) < (int)strlen(send_buf)) {
        perr_exit("Cannot send data", WSAGetLastError());
    }
    printf("        Request sent successfully.\n");

    // --- Step 11: Receive HTTP response and print it ---
    printf("[Step 9] Receiving HTTP response...\n");
    printf("---- Start of response ----\n");
    while ((rc = recv(s, recv_buf, bufsz - 2, 0)) > 0) {
        recv_buf[rc] = '\0';
        printf("%s", recv_buf);
    }
    if (rc < 0) {
        perr_exit("Error receiving data", WSAGetLastError());
    }
    printf("\n---- End of response ----\n");

    // --- Step 12: Cleanup and exit ---
    printf("[Step 10] Closing socket and cleaning up.\n");
    closesocket(s);
#ifdef _WIN32
    WSACleanup();
#endif
    printf("[Step 10] Done. Exiting.\n");
    return 0;
}
