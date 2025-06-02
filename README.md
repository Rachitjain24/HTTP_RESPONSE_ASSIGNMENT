## 1. Introduction

The original `gethttp.cpp` example was a minimal C++ program that performed a plain‐HTTP GET request to a given URL and printed the entire response (headers + body) to standard output. We modified it to add two major capabilities:

1. **Step‐by‐step debug output**, so that each stage of initialization, DNS lookup, socket creation, connection, request sending, and response receiving is explicitly logged.
2. **Optional proxy support**, reading the standard `http_proxy` environment variable (e.g. `http://127.0.0.1:9999`) and, if set, automatically routing the GET through that proxy. When proxying, the request line uses the full URL (`GET http://host/path HTTP/1.1`); otherwise it uses the normal “GET /path HTTP/1.1” form.

This report summarizes the code changes, highlights the new features, and briefly explains how to test and verify correct behavior (including capturing traffic in mitmproxy).

---

## 2. Summary of Code Changes

1. **Debug (“step‐by‐step”) print statements**

   * At the beginning of each major block (WSAStartup on Windows, address struct preparation, URL parsing, DNS resolution, socket creation, connection, request composition, send, receive, cleanup), we added a `printf("[Step N] …")` line.
   * These statements make it easy to see exactly which part of the program is executing and in what order. For example:

     ```cpp
     printf("[Step 4] Parsing URL...\n");
     printf("        Parsed Host: %s\n", host);
     printf("        Parsed Site: %s\n", site);
     ```
   * Additional `[DEBUG]` prints show internal decisions—specifically, whether a proxy was detected and which host/port is being resolved and connected to.

2. **URL parsing & extraction**

   * We preserved the original logic that strips any leading `"http://"` and splits the remainder into `host` (everything before the first `/`) and `site` (everything after).
   * If no path is present (no `/` found), `site` defaults to the empty string (the request becomes `GET / HTTP/1.1`).

3. **Proxy detection logic**

   * At Step 5, the program calls `getenv("http_proxy")`. If the variable is set and begins with `"http://"`, it parses out the proxy’s hostname and port (looking for a `:` after the host).
   * If parsing succeeds (`HOST:PORT`), `useProxy = true` and a debug message prints:

     ```
     [DEBUG] http_proxy detected → 127.0.0.1:9999
     ```
   * If `http_proxy` is absent or malformed, the code prints:

     ```
     [DEBUG] No valid http_proxy found → connecting directly
     ```

4. **Conditional DNS lookup and connect**

   * If `useProxy` is true, the code sets `connectTo = proxyHost` and `connectPort = proxyPort`; otherwise `connectTo = host` and `connectPort = 80`.
   * It then performs `inet_addr` + `gethostbyname` on `connectTo` and sets up `addr.sin_addr` accordingly.
   * The debug output shows exactly what hostname/IP and port the program will connect to:

     ```cpp
     printf("[Step 5] Resolving '%s' …\n", connectTo.c_str());
     // … after DNS:
     printf("        Resolved '%s' → %s:%d\n",
            connectTo.c_str(),
            inet_ntoa(addr.sin_addr),
            connectPort);
     ```
   * Next, we create the socket and call `connect()` on `connectTo:connectPort`, printing:

     ```cpp
     printf("[Step 7] Connecting to %s:%d …\n",
            connectTo.c_str(), connectPort);
     // after successful connect:
     printf("        Connected to %s:%d\n",
            connectTo.c_str(), connectPort);
     ```

5. **Building the GET request**

   * If proxying, the request line must include the full URL. We do:

     ```cpp
     snprintf(
         send_buf, bufsz,
         "GET http://%s/%s HTTP/1.1\r\n"
         "Host: %s\r\n"
         "Connection: close\r\n"
         "\r\n",
         host, site, host
     );
     ```

     This ensures the proxy knows where to forward the request.
   * If not proxying, we revert to the standard:

     ```cpp
     snprintf(
         send_buf, bufsz,
         "GET /%s HTTP/1.1\r\n"
         "Host: %s:80\r\n"
         "Connection: close\r\n"
         "\r\n",
         site, host
     );
     ```

6. **Receiving and printing the response**

   * Exactly as in the original example, we loop on `recv()` until it returns 0 (connection closed) or < 0 (error).
   * Each chunk of up to `bufsz−2` bytes is null‐terminated and printed:

     ```cpp
     while ((rc = recv(s, recv_buf, bufsz - 2, 0)) > 0) {
         recv_buf[rc] = '\0';
         printf("%s", recv_buf);
     }
     ```
   * This prints both headers and body (binary data will garble the terminal; to test image downloads, capture to a file or view the headers only until the empty line).

7. **Cleanup**

   * We explicitly call `closesocket(s)`, then on Windows also `WSACleanup()`.
   * Finally, print:

     ```
     [Step 10] Done. Exiting.
     ```

---

## 3. New Features and Benefits

1. **Step‐by‐step traceability**

   * The numbered `[Step N]` and `[DEBUG]` outputs make it trivial to follow exactly what the program is doing, in which sequence.
   * This is invaluable for teaching, debugging socket errors, or understanding DNS vs. proxy resolution.

2. **Proxy support via `http_proxy`**

   * Many environments require HTTP traffic to go through a local proxy (e.g. mitmproxy, Fiddler, corporate proxy). By reading `http_proxy` and automatically switching into “proxy mode,” users no longer need to rewrite their code manually.
   * When proxying, the GET‐line uses the full URL, and the `Host:` header remains correct, allowing transparent interception by a tool like mitmproxy.

3. **Unchanged direct‐connect fallback**

   * If `http_proxy` is not set (or malformed), the program behaves exactly like the original: a direct connection to `host:80` and a simple `GET /path HTTP/1.1` request.

4. **Connection: close header**

   * We explicitly request `Connection: close` so the remote server will close the socket once the response is fully delivered. That makes it easy to detect EOF (recv = 0) and exit cleanly.

5. **Binary‐safe printing**

   * Although printing binary directly to stdout can garble the terminal, it does show that header‐body separation is correct (the boundary is at `\r\n\r\n`).
   * For image or ZIP downloads, users can redirect `stdout` into a file:

     ```
     ./gethttp_debug_proxy > out.bin
     ```

     Then inspect the first part in a text editor to see HTTP headers, and open the remainder as a binary file.

---

## 4. Testing & Verification

1. **Compile & run without proxy**

   ```bash
   g++ -std=c++11 gethttp_debug_proxy.cpp -o gethttp_debug_proxy
   ./gethttp_debug_proxy
   URL: http://httpbin.org/get
   ```

   * You should see:

     ```
     [Step 1] (Unix) No WSAStartup needed.
     [Step 2] Preparing address structure...
     [Step 3] Asking for URL...
     [Step 4] Parsing URL...
             Parsed Host: httpbin.org
             Parsed Site: get
     [DEBUG] No valid http_proxy found → connecting directly
     [Step 5] Resolving 'httpbin.org' …
             Resolved 'httpbin.org' → 3.215.128.34:80
     [Step 6] Creating socket...
             Socket created (fd=3)
     [Step 7] Connecting to httpbin.org:80 …
             Connected to httpbin.org:80
     [Step 8] Preparing HTTP GET request...
             >>> Request >>>
     GET /get HTTP/1.1
     Host: httpbin.org:80
     Connection: close

             Request sent successfully.
     [Step 9] Receiving HTTP response...
     ---- Start of response ----
     HTTP/1.1 200 OK
     Date: …
     Content-Type: application/json
     …
     { … JSON … }
     ---- End of response ----
     [Step 10] Closing socket and cleaning up.
     [Step 10] Done. Exiting.
     ```

2. **Run mitmproxy on port 9999**
   In a separate shell:

   ```bash
   mitmproxy -p 9999
   ```

3. **Set the proxy environment and run again**
   In the same (or a new) shell:

   ```bash
   export http_proxy="http://127.0.0.1:9999"
   export https_proxy="http://127.0.0.1:9999"
   ./gethttp_debug_proxy
   URL: http://httpbin.org/get
   ```

   * Now you’ll see:

     ```
     [DEBUG] http_proxy detected → 127.0.0.1:9999
     [Step 5] Resolving '127.0.0.1' …
             Resolved '127.0.0.1' → 127.0.0.1:9999
     [Step 6] Creating socket...
     …
     [Step 7] Connecting to 127.0.0.1:9999 …
     …  
     [Step 8] Preparing HTTP GET request...
             >>> Request >>>
     GET http://httpbin.org/get HTTP/1.1
     Host: httpbin.org
     Connection: close

     …
     ```
   * In the mitmproxy console (running on port 9999), you’ll see a new flow:

     ```
     1. GET http://httpbin.org/get
     ```

     Selecting it shows the full request headers and the JSON response.

4. **Download a binary (image/png)**

   ```bash
   ./gethttp_debug_proxy > image.png
   ```

   * The first few kilobytes of `image.png` (until the boundary `\r\n\r\n`) are HTTP headers. The rest is raw PNG data.
   * Confirm via `file image.png` or open in an image viewer.

5. **Wireshark (optional)**

   * If capturing on `eth0` or the WSL virtual interface, filter on `tcp port 80` (or wherever mitmproxy forwarded). You’ll see the same GET and response in packet form. The debug prints in Step 9 confirm precisely where the header/body boundary lies.

---

## 5. Conclusion

The modified program now provides clear, numbered debug output at each stage of execution, making it an excellent teaching or diagnostic tool. By detecting the `http_proxy` environment variable and automatically switching to a proxy‐style full‐URL GET, it can integrate seamlessly with tools like mitmproxy (or any HTTP proxy). When no proxy is set, it behaves exactly like the original, connecting directly to the target server.

---

# README

# HTTP Downloader with Debug & Proxy Support

## Overview
This C++ program performs a simple HTTP GET request on a user-provided URL, prints a step-by-step trace of each major action, and supports HTTP proxying via the standard `http_proxy` environment variable. It works only over plain HTTP (port 80) and does not support HTTPS or proxies requiring authentication.

---

## Features
- **Step-by-step debug output**: Each stage (parsing, DNS lookup, socket creation, connection, request composition, sending, receiving, cleanup) is labeled `[Step N]` or `[DEBUG]` for easy traceability.
- **Proxy detection**: If `http_proxy` is set (format: `http://<host>:<port>`), the program routes its request through that proxy.
- **Direct fallback**: If no valid `http_proxy` is found, it connects directly to the target host on port 80.
- **Full-URL requests when proxying**: Automatically adjusts the GET line to `GET http://<host>/<path> HTTP/1.1` so that proxies can forward the request correctly.
- **Binary‐friendly**: Prints the entire response (headers + body), including binary data. Use output redirection (`> file.bin`) to capture raw data into a file.

---

## Requirements
- **WSL-Ubuntu** (or any Linux with a POSIX sockets API)
- **g++** (tested with GCC 7+; requires C++11)
- **Optional**: mitmproxy (for testing proxy behavior)
  ```bash
  sudo apt update
  sudo apt install mitmproxy
````

* No external libraries beyond the standard C++/C sockets headers.

---

## Source File

```
gethttp_debug_proxy.cpp
```

This file contains the entire implementation.

---

## Compilation

1. Open your WSL/Ubuntu shell.
2. Install build tools (if not already installed):

   ```bash
   sudo apt update
   sudo apt install -y build-essential
   ```
3. Change into the directory containing `gethttp_debug_proxy.cpp`.
4. Compile:

   ```bash
   g++ -std=c++11 gethttp_debug_proxy.cpp -o gethttp_debug_proxy
   ```

   If successful, you will have an executable named:

   ```
   gethttp_debug_proxy
   ```

---

## Usage

### 1. Direct (no proxy)

```bash
./gethttp_debug_proxy
```

You will see:

```
[Step 1] (Unix) No WSAStartup needed.
[Step 2] Preparing address structure...
[Step 3] Asking for URL...
URL: http://httpbin.org/get
...
```

1. When prompted `URL:`, enter any plain‐HTTP URL (starting with `http://`).
2. The program will print a labeled sequence of actions, then display the full HTTP response (headers + body).

### 2. Via Proxy (e.g., mitmproxy)

1. **Launch mitmproxy** on a free port (e.g., 9999):

   ```bash
   mitmproxy -p 9999
   ```

   * You should see:

     ```
     Mitmproxy: listening on 0.0.0.0:9999, 0 clients connected
     ```

2. **Set the environment variable** in the same shell (or a new one) where you’ll run the C++ program:

   ```bash
   export http_proxy="http://127.0.0.1:9999"
   export https_proxy="http://127.0.0.1:9999"
   ```

3. **Run the program**:

   ```bash
   ./gethttp_debug_proxy
   ```

4. **At the `URL:` prompt**, enter your target HTTP URL:

   ```
   URL: http://httpbin.org/get
   ```

5. Observe output:

   ```
   [DEBUG] http_proxy detected → 127.0.0.1:9999
   [Step 5] Resolving '127.0.0.1' ...
           Resolved '127.0.0.1' → 127.0.0.1:9999
   [Step 6] Creating socket...
           Socket created (fd=3)
   [Step 7] Connecting to 127.0.0.1:9999 ...
           Connected to 127.0.0.1:9999
   [Step 8] Preparing HTTP GET request...
           >>> Request >>>
   GET http://httpbin.org/get HTTP/1.1
   Host: httpbin.org
   Connection: close

           Request sent successfully.
   [Step 9] Receiving HTTP response...
   ---- Start of response ----
   HTTP/1.1 200 OK
   Date: …
   Content-Type: application/json
   …
   { … }
   ---- End of response ----
   [Step 10] Closing socket and cleaning up.
   [Step 10] Done. Exiting.
   ```

6. **Check mitmproxy’s window**:

   * You will see a new flow:

     ```
     1. GET http://httpbin.org/get
     ```
   * Press **Enter** on that flow to view full request and response details.

---

## Example Commands

1. **Direct, text response**

   ```bash
   ./gethttp_debug_proxy
   URL: http://httpbin.org/get
   ```

2. **Direct, binary response (PNG)**

   ```bash
   ./gethttp_debug_proxy > image.png
   URL: http://httpbin.org/image/png
   ```

   * Inspect headers in the terminal; the rest of the file is raw PNG.
   * Confirm with:

     ```bash
     file image.png
     ```

3. **Using mitmproxy (port 9999)**

   ```bash
   # In one terminal:
   mitmproxy -p 9999
   # In another terminal:
   export http_proxy="http://127.0.0.1:9999"
   export https_proxy="http://127.0.0.1:9999"
   ./gethttp_debug_proxy
   URL: http://httpbin.org/get
   ```

---

## Environment Variables

* `http_proxy`

  * Format must be `http://<hostname>:<port>`, e.g. `http://127.0.0.1:9999`.
  * If set and valid, the program will route all requests through that proxy.
  * If not set or invalid, the program connects directly to the target on port 80.

* `https_proxy`

  * Exporting `https_proxy` does not affect this program because it only supports plain HTTP (port 80). However, we export it for completeness if you use other tools in the same shell.

---

## Known Limitations

* **No HTTPS support**: This code does not implement TLS. Only plain HTTP (port 80) is supported.
* **No chunked‐encoding parsing**: If the HTTP response uses `Transfer-Encoding: chunked`, the program will still print the raw chunk markers plus data. It does **not** reassemble or strip the chunk‐size lines.
* **No proxy authentication**: If your proxy requires a username/password, this program will not handle it.
* **Binary data on stdout**: Printing binary directly can garble your terminal. To save images, ZIPs, or other binary content, redirect stdout to a file:

  ```bash
  ./gethttp_debug_proxy > out.bin
  ```

---

## File List

* `gethttp_debug_proxy.cpp`

  * The complete C++ source with debug and proxy support.
* `README.md`

  * This file (providing compilation instructions, usage examples, and notes).

---

## Author

**Author:** Rachit Jain



