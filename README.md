# live555ProxyServerEx

This is improved version of [LIVE555 Proxy Server](http://www.live555.com/proxyServer/)
that supports configuration files.

The "LIVE555 Proxy Server" is a unicast RTSP server - built from the
["LIVE555 Streaming Media" software](http://www.live555.com/liveMedia/) - that
acts as a 'proxy' for one or more 'back-end' unicast or multicast RTSP/RTP
streams (i.e., served by other server(s)).

The key feature of a proxy server is that it reads each 'back-end' stream only
once, regardless of how many separate clients are streaming from the proxy
server. This makes the proxy server ideal, for example, for streaming from
a RTSP-enabled video camera (which might not be able to handle more than
one connection at a time).

                                                          --> [RTSP client1]
    [back-end RTSP/RTP stream] --> [LIVE555 Proxy Server] --> [RTSP client2]
                                                          ...
                                                          --> [RTSP clientN]

For more information see [the official documentation](http://www.live555.com/proxyServer/).


## Build instructions

(only `linux-64bit` is supported now)

- Install latest version of vanilla LIVE555 (`livemedia-utils` package
  in Debian/Ubuntu, `live-media` in Arch Linux; version `2018.01.29` was
  tested). Development files (e.g. `/usr/include/liveMedia` or
  `/usr/local/include/liveMedia`; `liblivemedia-dev` package in Debian/Ubuntu)
  should be also available

- Copy `config-example.linux-64bit` to `config.linux-64bit` and edit it. You
  may want to replace `LIBRARIES_DIR` and `INCLUDES_PREFIX` or change
  the compiler

- Generate Makefile using `./genMakefiles`

- `make`

- Then `live555ProxyServerEx` executable file will be available
