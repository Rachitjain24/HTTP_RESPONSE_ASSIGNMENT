
# HTTP Downloader with Debug & Proxy Support

## Features
- **Step-by-Step Debug Output**  
  Each major action (initialization, parsing, DNS lookup, socket creation, connection, request composition, send, receive, cleanup) is logged with `[Step N]` or `[DEBUG]` prefixes, making it easy to follow the program’s execution flow.

- **Transparent Proxy Detection**  
  If the `http_proxy` environment variable is set to a valid URL (e.g. `http://127.0.0.1:9999`), the program automatically routes its HTTP GET through that proxy. Otherwise, it falls back to a direct connection on port 80.

- **Full-URL GET When Proxying**  
  When using a proxy, the request line is formatted as  
```

GET http\://<real-host>/<path> HTTP/1.1

```
combined with a correct `Host:` header, ensuring proper proxy forwarding (e.g. for tools like mitmproxy).

- **Direct Fallback Mode**  
If no valid `http_proxy` is detected, the program connects directly to `<host>:80` and sends a standard  
```

GET /<path> HTTP/1.1
Host: <host>:80

````

- **Binary-Friendly Reception**  
The program prints the raw HTTP response (headers + body) to stdout. Redirect stdout into a file (e.g. `> image.png`) to capture binary data correctly.

- **Connection: Close**  
The `Connection: close` header ensures the server will close the socket after sending the full response, making it easy to detect EOF and terminate gracefully.

---

## Development Output (Sample Run)

Below is a typical console session showing compilation and two execution scenarios: direct mode (no proxy) and proxy mode (using mitmproxy).

### A. Compilation
```bash
$ g++ -std=c++11 gethttp_debug_proxy.cpp -o gethttp_debug_proxy
````

* Produces the executable `gethttp_debug_proxy` (no output if successful).

---

### B. Direct-Connect Run (No Proxy)

```bash
$ ./gethttp_debug_proxy
[Step 1] (Unix) No WSAStartup needed.
[Step 2] Preparing address structure...
[Step 3] Asking for URL...
URL: http://httpbin.org/get
[Step 4] Parsing URL...
        Parsed Host: httpbin.org
        Parsed Site: get
[DEBUG] No valid http_proxy found → connecting directly
[Step 5] Resolving 'httpbin.org' …
        inet_addr failed; calling gethostbyname for 'httpbin.org'...
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
Date: Mon, 02 Jun 2025 17:30:00 GMT
Content-Type: application/json
Content-Length: 265
Connection: close
Server: gunicorn/19.9.0
Access-Control-Allow-Origin: *
Access-Control-Allow-Credentials: true

{
  "args": {},
  "headers": {
    "Accept": "*/*",
    "Host": "httpbin.org",
    "User-Agent": "gethttp_debug_proxy/1.0"
  },
  "origin": "1.2.3.4",
  "url": "http://httpbin.org/get"
}
---- End of response ----
[Step 10] Closing socket and cleaning up.
[Step 10] Done. Exiting.
```

* Notice the `[DEBUG] No valid http_proxy` message and a direct DNS lookup for `httpbin.org`.
* The HTTP response (JSON) prints in full.

---

### C. Proxy-Connect Run (mitmproxy on Port 9999)

1. **Start mitmproxy** in another terminal:

   ```bash
   $ mitmproxy -p 9999
   Mitmproxy: listening on 0.0.0.0:9999, 0 clients connected
   ```

   (No further prompts; leave this window open to capture flows.)

2. **Export proxy variables** and run the program:

   ```bash
   $ export http_proxy="http://127.0.0.1:9999"
   $ export https_proxy="http://127.0.0.1:9999"
   $ ./gethttp_debug_proxy
   [Step 1] (Unix) No WSAStartup needed.
   [Step 2] Preparing address structure...
   [Step 3] Asking for URL...
   URL: http://httpbin.org/get
   [Step 4] Parsing URL...
           Parsed Host: httpbin.org
           Parsed Site: get
   [DEBUG] http_proxy detected → 127.0.0.1:9999
   [Step 5] Resolving '127.0.0.1' …
           Resolved '127.0.0.1' → 127.0.0.1:9999
   [Step 6] Creating socket...
           Socket created (fd=3)
   [Step 7] Connecting to 127.0.0.1:9999 …
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
   Date: Mon, 02 Jun 2025 17:31:00 GMT
   Content-Type: application/json
   Content-Length: 265
   Connection: close

   {
     "args": {},
     "headers": {
       "Accept": "*/*",
       "Host": "httpbin.org",
       "User-Agent": "gethttp_debug_proxy/1.0"
     },
     "origin": "1.2.3.4",
     "url": "http://httpbin.org/get"
   }
   ---- End of response ----
   [Step 10] Closing socket and cleaning up.
   [Step 10] Done. Exiting.
   ```

3. **Inspect in mitmproxy’s window**:

   ```
   1. GET http://httpbin.org/get
   ```

   * Use arrow keys to select the flow, press **Enter** to view request and response details.

---

## Installation & Compilation

1. **Install build tools** (WSL/Ubuntu):

   ```bash
   sudo apt update
   sudo apt install -y build-essential
   ```

2. **Save** the source code into `gethttp_debug_proxy.cpp`:

   ```bash
   nano gethttp_debug_proxy.cpp
   # → Paste the full code, then Save (Ctrl+O) and Exit (Ctrl+X).
   ```

3. **Compile**:

   ```bash
   g++ -std=c++11 gethttp_debug_proxy.cpp -o gethttp_debug_proxy
   ```

* If compilation succeeds, you get an executable named `gethttp_debug_proxy`.
* If errors about missing headers occur, ensure you have `#include <string.h>`, `<stdio.h>`, `<stdlib.h>`, `<unistd.h>`, and the socket headers at the top of the file.

---

## Usage

### 1. Direct-Connect Mode (no proxy)

```bash
./gethttp_debug_proxy
URL: http://<hostname>/<path>
```

* The program prints step-by-step debug output.
* It connects directly to `<hostname>:80`.
* It then sends:

  ```
  GET /<path> HTTP/1.1
  Host: <hostname>:80
  Connection: close
  ```
* The full response (headers + body) is printed.

### 2. Proxy-Connect Mode (via `http_proxy`)

1. **Start a proxy** (e.g. mitmproxy) on a free port (e.g., 9999):

   ```bash
   mitmproxy -p 9999
   ```
2. **Export the environment variables** in the same shell where you’ll run the program:

   ```bash
   export http_proxy="http://127.0.0.1:9999"
   export https_proxy="http://127.0.0.1:9999"
   ```
3. **Run**:

   ```bash
   ./gethttp_debug_proxy
   URL: http://<hostname>/<path>
   ```

   * The program detects `http_proxy`.
   * It connects to `127.0.0.1:9999` instead of the real host.
   * It sends:

     ```
     GET http://<hostname>/<path> HTTP/1.1
     Host: <hostname>
     Connection: close
     ```
   * mitmproxy will intercept and forward the request.
   * The program prints the response after proxy forwarding.

---

## Environment Variables

* `http_proxy`

  * Format: `http://<proxy-host>:<proxy-port>` (e.g. `http://127.0.0.1:9999`).
  * If set to a valid address, the program routes the GET request through that proxy.
  * If not set or invalid, direct‐connect mode is used.

* `https_proxy`

  * Although this program does not support HTTPS (TLS), exporting it ensures other tools in the same shell use the same proxy.

---

## Examples

1. **Fetch a JSON endpoint directly**

   ```bash
   ./gethttp_debug_proxy
   URL: http://httpbin.org/get
   ```
2. **Download a PNG image to a file**

   ```bash
   ./gethttp_debug_proxy > test.png
   URL: http://httpbin.org/image/png
   file test.png       # Verify it’s a PNG
   ```
3. **Use mitmproxy (port 9999) to intercept**
   In one terminal:

   ```bash
   mitmproxy -p 9999
   ```

   In another terminal:

   ```bash
   export http_proxy="http://127.0.0.1:9999"
   export https_proxy="http://127.0.0.1:9999"
   ./gethttp_debug_proxy
   URL: http://httpbin.org/get
   ```

   Inspect the flow inside the mitmproxy window.

---

## Known Limitations

* **No HTTPS/TLS**: Only plain HTTP (port 80) is supported.
* **No chunked-encoding reassembly**: If the response uses `Transfer-Encoding: chunked`, the raw chunk markers appear in the output.
* **No proxy authentication**: Proxies requiring user/pass are not supported.
* **Binary output to terminal**: Capturing binary directly to the terminal can garble it—redirect stdout to a file for images or ZIPs.

---

## File List

* **gethttp\_debug\_proxy.cpp**

  * C++ source with step-by-step debug and optional proxy support.
* **README.md**

  * This documentation (features, sample output, installation, usage, examples, limitations).

---

## Author

**Author:** Rachit Jain 94260

