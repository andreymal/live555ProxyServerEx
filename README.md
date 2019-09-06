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

- Install latest version of vanilla LIVE555:

    - Debian/Ubuntu: `sudo apt-get install livemedia-utils liblivemedia-dev`
    - Arch Linux: `sudo pacman -S live-media`
    - from source (`2018.09.10` was tested):
      - build using the official documentation:
        http://www.live555.com/liveMedia/#config-unix
      - install `*.a` libraries using `sudo make install` (you can put custom
        `DESTDIR` in your live555 config file (not proxy config file!) or use
        something like `checkinstall`)
      - put `LIBRARIES_DIR = /usr/local/lib` to your config file
      - use `LIB_SUFFIX = a` in your config file

  Make sure the development files (e.g. `/usr/include/liveMedia` or
  `/usr/local/include/liveMedia`) are available

- Copy `config-example.linux-64bit` to `config.linux-64bit` and edit it. You
  may want to replace `LIBRARIES_DIR` and `INCLUDES_PREFIX` or change
  the compiler (see also example for Ubuntu 18.04 with gcc:
  `config-example-ubuntu.linux-64bit`)

    - Use `LIB_SUFFIX = a` for static linking
    - Use `LIB_SUFFIX = so` for dynamic linking

- Generate Makefile using `./genMakefiles linux-64bit`

- `make`

- Then `live555ProxyServerEx` executable file will be available


## Configuration file

While you can use command-line arguments as with vanilla live555ProxyServer,
you also can create a configuration file and load it using `-c config.cfg`
option. It has INI format.

`[general]` section describes global options:

- `verbosity`: `0` is default, `1` is equivalent to `-v`, `2` is equivalent
  to `-V`;

- `stream_rtp_over_tcp` (boolean) is equivalent to `-t`;

- `try_standard_port_numbers` (boolean): if `1` (default), then proxy server
  tries to use port 554 or 8554 if `rtsp_server_port` failed;

- `server_tunneling_over_http` (boolean): if `1` (default), then proxy creates
  a HTTP server for RTSP-over-HTTP tunneling;

- `server_tunneling_over_http_port` (0..65535): if `0` (default), then proxy
  tries to use port 80, 8000 or 8080 for RTSP-over-HTTP tunneling; a non-zero
  value means the use of a specific port;

- `rtsp_server_port` (1..65535, default 554) is equivalent to `-p`;

- `register_requests` (boolean) is equivalent to `-R`;

- `username_for_register` and `password_for_register` are equivalent
  to `-U username password`;

- `single_stream_name`: name of the stream if there is only one proxied URL
  (default `proxyStream`);

- `multiple_stream_name`: names of the streams if there are multiple proxied
  URLs, must have `%d` for stream number (default `proxyStream-%d`);

- `out_packet_max_size` (unsigned integer): changes
  `OutPacketBuffer::maxSize` value. If you see something like
  `The total received frame size exceeds the client's buffer size` or
  `The input frame data was too large for our buffer size`, you can try
  to increase this value (default is 2000000 — 2 megabytes).

`[auth]` section is a list of usernames and passwords. If this is not empty,
then proxy server will require authentication. Example:

    [auth]
    username = admin
    password = 123456

    username = alice
    password = 777bob777

`[streams]` section is a list of proxied streams. Every stream must have URL
and stream name. Example:

    [streams]
    url = rtsp://192.168.69.100:554/onvif1
    name = office

    url = rtsp://10.0.2.66:38888/h264_ulaw.sdp
    name = android_ipwebcam

These streams will be proxied as `rtsp://[proxy_ip]:[proxy_port]/office` and
`rtsp://[proxy_ip]:[proxy_port]/android_ipwebcam`.

`[streamparams]` section has additional options for `[streams]` section. These
options does not affect other config files or command-line arguments:

- `username` and `password` are used for connection to proxied streams;

- `tunnel_over_http_port` is equivalent to `-T`.

`-u` and `-T` command-line options affects only command-line streams.

`[include]` allows you to use multiple config files. They will be loaded
in the specified order. Example:

    [include]
    path = proxyserver.d/auth.cfg
    path = proxyserver.d/office.cfg
    path = proxyserver.d/android_ipwebcam.cfg

Configuration in the files that were read later has higher priority. (Note
that `[auth]` sections will be merged and `[streamparams]` section affects
only current file.)

Command-line arguments have higher priority than any config files.
