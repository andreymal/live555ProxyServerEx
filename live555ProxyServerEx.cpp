/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 3 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// Copyright (c) 1996-2018, Live Networks, Inc.
// Copyright (c) 2018 andreymal
// All rights reserved

// LIVE555 Proxy Server Ex
// main program

#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <sys/stat.h>
#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "inih/INIReader.hh"

struct ProxyStream {
  std::string proxiedURL;
  std::string streamName;
  std::string username;
  std::string password;
  portNumBits tunnelOverHTTPPortNum;
};

char const* progName;
UsageEnvironment* env;
UserAuthenticationDatabase* authDB = NULL;
UserAuthenticationDatabase* authDBForREGISTER = NULL;

std::vector<ProxyStream> streams;
std::vector<std::string> streamNames; // for collision detection

// Global configuration
int verbosityLevel = 0;
Boolean streamRTPOverTCP = False;
Boolean tryStandardPortNumbers = True;
Boolean serverTunnelingOverHTTP = True;
portNumBits serverTunnelingOverHTTPPortNum = 0; // It tries 80, 8000 or 8080 by default
portNumBits rtspServerPortNum = 554;
Boolean proxyREGISTERRequests = False;
std::string usernameForREGISTER;
std::string passwordForREGISTER;
std::string singleStreamNameTemplate("proxyStream");
std::string multipleStreamNameTemplate("proxyStream-%d");

// Configuration for command-line streams
std::string username;
std::string password;
portNumBits tunnelOverHTTPPortNum = 0;


bool includeConfig(const char* path);
bool loadINIFile(const char* inifile);


static RTSPServer* createRTSPServer(Port port) {
  if (proxyREGISTERRequests) {
    return RTSPServerWithREGISTERProxying::createNew(
      *env, port, authDB, authDBForREGISTER, 65,
      streamRTPOverTCP, verbosityLevel,
      username.length() > 0 ? username.c_str() : NULL,
      password.length() > 0 ? password.c_str() : NULL);
  } else {
    return RTSPServer::createNew(*env, port, authDB);
  }
}

void usage() {
  *env << "Usage: " << progName
       << " [-c <config-file>]"
       << " [-v|-V]"
       << " [-t|-T <http-port>]"
       << " [-p <rtspServer-port>]"
       << " [-u <username> <password>]"
       << " [-R] [-U <username-for-REGISTER> <password-for-REGISTER>]"
       << " <rtsp-url-1> ... <rtsp-url-n>\n";
  exit(1);
}


char* findConfigPath(int argc, char** argv) {
  while (argc > 1) {
    // Process initial command-line options (beginning with "-"),
    // but find only path to configuration file
    char* const opt = argv[1];
    if (opt[0] != '-') break; // the remaining parameters are assumed to be "rtsp://" URLs

    switch (opt[1]) {
    case 'c': { // path to configuration file
      if (argc < 3) {
        usage();
        return NULL;
      }
      return argv[2];
    }

    default: {
      break;
    }
    }

    ++argv; --argc;
  }

  return NULL;
}


bool includeConfig(const char* path) {
  struct stat st;
  if (stat(path, &st) != 0) {
    *env << "Path \"" << path << "\" not found\n";
    return false;
  }

  if (S_ISDIR(st.st_mode)) {
    *env << "Config directories are not supported\n";
    return false;
  }

  return loadINIFile(path);
}


bool loadINIFile(const char* inifile) {
  INIReader reader(inifile);
  if (reader.ParseError() != 0) {
    return false;
  }

  // Global configuration variables
  verbosityLevel = reader.GetInteger("general", "verbosity", verbosityLevel);
  streamRTPOverTCP = reader.GetBoolean("general", "stream_rtp_over_tcp", streamRTPOverTCP);
  tryStandardPortNumbers = reader.GetBoolean("general", "try_standard_port_numbers", tryStandardPortNumbers);
  serverTunnelingOverHTTP = reader.GetBoolean("general", "server_tunneling_over_http", serverTunnelingOverHTTP);
  serverTunnelingOverHTTPPortNum = reader.GetInteger("general", "server_tunneling_over_http_port", serverTunnelingOverHTTPPortNum);
  rtspServerPortNum = reader.GetInteger("general", "rtsp_server_port", rtspServerPortNum);
  proxyREGISTERRequests = reader.GetBoolean("general", "register_requests", proxyREGISTERRequests);
  usernameForREGISTER = reader.Get("general", "username_for_register", usernameForREGISTER);
  passwordForREGISTER = reader.Get("general", "password_for_register", passwordForREGISTER);
  singleStreamNameTemplate = reader.Get("general", "single_stream_name", singleStreamNameTemplate);
  // TODO: %d should be validated here
  multipleStreamNameTemplate = reader.Get("general", "multiple_stream_name", multipleStreamNameTemplate);

  // Variables that affects only current streams
  std::string streamUsername = reader.Get("streamparams", "username", "");
  std::string streamPassword = reader.Get("streamparams", "password", "");
  portNumBits streamTunnelOverHTTPPortNum =
    (portNumBits) reader.GetInteger("streamparams", "tunnel_over_http_port", 0);

  // Parse proxied streams from "streams" section
  std::string urls = reader.Get("streams", "url", "");
  std::string names = reader.Get("streams", "name", "");
  std::stringstream ss1(urls);
  std::stringstream ss2(names);
  std::string url;
  std::string streamName;

  while (std::getline(ss1, url, '\n') && std::getline(ss2, streamName, '\n')) {
    // Ensure that there are no collision with previous streams
    if (std::find(streamNames.begin(), streamNames.end(), streamName) != streamNames.end()) {
      *env << "Conflict: stream name " << streamName.c_str() << " is already used\n";
      return false;
    }

    ProxyStream stream;
    stream.proxiedURL = url;
    stream.streamName = streamName;
    stream.username = streamUsername;
    stream.password = streamPassword;
    stream.tunnelOverHTTPPortNum = streamTunnelOverHTTPPortNum;
    streams.push_back(stream);
    streamNames.push_back(streamName);
  }

  // Client access control to the RTSP server
  std::string usernames = reader.Get("auth", "username", "");
  std::string passwords = reader.Get("auth", "password", "");
  std::stringstream ss3(usernames);
  std::stringstream ss4(passwords);
  std::string authUsername;
  std::string authPassword;

  while (std::getline(ss3, authUsername, '\n') && std::getline(ss4, authPassword, '\n')) {
    if (authUsername.length() > 0 && authPassword.length() > 0) {
      if (authDB == NULL) {
        authDB = new UserAuthenticationDatabase;
      }
      authDB->addUserRecord(authUsername.c_str(), authPassword.c_str());
    }
  }

  // Get list of config files for recursive loading
  std::string includes = reader.Get("include", "path", "");

  std::stringstream ss(includes);
  std::string nextinifile;

  // TODO: prevent infinite recursion
  while(std::getline(ss, nextinifile, '\n')){
    if (nextinifile.length() > 0) {
      if (!includeConfig(nextinifile.c_str())) {
        return false;
      }
    }
  }

  return true;
}


int parseArgs(int argc, char** argv) {
  int pos = 1;

  while (pos < argc) {
    // Process initial command-line options (beginning with "-"):
    char* const opt = argv[pos];
    if (opt[0] != '-') break; // the remaining parameters are assumed to be "rtsp://" URLs

    switch (opt[1]) {
    case 'c': { // already parsed by findConfigPath function; just skip it
      ++pos;
      break;
    }

    case 'v': { // verbose output
      verbosityLevel = 1;
      break;
    }

    case 'V': { // more verbose output
      verbosityLevel = 2;
      break;
    }

    case 't': {
      // Stream RTP and RTCP over the TCP 'control' connection.
      // (This is for the 'back end' (i.e., proxied) stream only.)
      streamRTPOverTCP = True;
      break;
    }

    case 'T': {
      // stream RTP and RTCP over a HTTP connection
      if (pos + 1 < argc && argv[pos + 1][0] != '-') {
        // The next argument is the HTTP server port number:
        if (sscanf(argv[pos + 1], "%hu", &tunnelOverHTTPPortNum) == 1
            && tunnelOverHTTPPortNum > 0) {
          ++pos;
          break;
        }
      }

      // If we get here, the option was specified incorrectly:
      return -1;
    }

    case 'p': {
      // specify a rtsp server port number 
      if (pos + 1 < argc && argv[pos + 1][0] != '-') {
        // The next argument is the rtsp server port number:
        if (sscanf(argv[pos + 1], "%hu", &rtspServerPortNum) == 1
            && rtspServerPortNum > 0) {
          ++pos;
          break;
        }
      }

      // If we get here, the option was specified incorrectly:
      usage();
      break;
    }
    
    case 'u': { // specify a username and password (to be used if the 'back end' (i.e., proxied) stream requires authentication)
      if (pos + 2 >= argc) return -1; // there's no argv[pos + 2] (for the "password")
      username = std::string(argv[pos + 1]);
      password = std::string(argv[pos + 2]);
      pos += 2;
      break;
    }

    case 'U': { // specify a username and password to use to authenticate incoming "REGISTER" commands
      if (pos + 2 >= argc) return -1; // there's no argv[pos + 2] (for the "password")
      usernameForREGISTER = std::string(argv[pos + 1]);
      passwordForREGISTER = std::string(argv[pos + 2]);

      if (authDBForREGISTER == NULL) authDBForREGISTER = new UserAuthenticationDatabase;
      authDBForREGISTER->addUserRecord(usernameForREGISTER.c_str(), passwordForREGISTER.c_str());
      pos += 2;
      break;
    }

    case 'R': { // Handle incoming "REGISTER" requests by proxying the specified stream:
      proxyREGISTERRequests = True;
      break;
    }

    default: {
      return -1;
    }
    }

    ++pos;
  }

  return pos;
}


bool parseURLs(int argc, char** argv, int urlsStartPos) {
  int i = 1;

  while (urlsStartPos < argc) {
    char streamName[127];

    if (i == 1 && urlsStartPos == argc - 1) {
      sprintf(streamName, "%s", singleStreamNameTemplate.c_str()); // there's just one stream; give it this name
    } else {
      sprintf(streamName, multipleStreamNameTemplate.c_str(), i); // there's more than one stream; distinguish them by name
    }

    std::string cppStreamName(streamName);

    // Ensure that there are no collisions with streams from config files
    if (std::find(streamNames.begin(), streamNames.end(), cppStreamName) != streamNames.end()) {
      *env << "Conflict: stream name " << streamName << " is already used\n";
      return false;
    }

    ProxyStream stream;
    stream.proxiedURL = std::string(argv[urlsStartPos]);
    stream.streamName = cppStreamName;
    stream.username = username;
    stream.password = password;
    stream.tunnelOverHTTPPortNum = tunnelOverHTTPPortNum;
    streams.push_back(stream);
    streamNames.push_back(cppStreamName);

    ++urlsStartPos;
    ++i;
  }

  return true;
}


int main(int argc, char** argv) {
  // Increase the maximum size of video frames that we can 'proxy' without truncation.
  // (Such frames are unreasonably large; the back-end servers should really not be sending frames this large!)
  OutPacketBuffer::maxSize = 100000; // bytes

  // Begin by setting up our usage environment:
  TaskScheduler* scheduler = BasicTaskScheduler::createNew();
  env = BasicUsageEnvironment::createNew(*scheduler);

  *env << "LIVE555 Proxy Server Ex\n"
       << "\t(LIVE555 Streaming Media library version "
       << LIVEMEDIA_LIBRARY_VERSION_STRING
       << "; licensed under the GNU LGPL)\n\n";

  // Check command-line arguments: optional parameters, then one or more rtsp:// URLs (of streams to be proxied):
  progName = argv[0];
  if (argc < 2) usage();

  // Config file has low priority, so it should be loaded first
  char *inifile = findConfigPath(argc, argv);
  if (inifile != NULL) {
    if (!includeConfig(inifile)) {
      *env << "Cannot parse config files\n";
      return 1;
    }
  }

  // Command line parameters have high priority, parse it after config file
  int urlsStartPos = parseArgs(argc, argv);
  if (urlsStartPos < 1) {
    usage();
  }

  // Parse URLs from command line arguments
  if (!parseURLs(argc, argv, urlsStartPos)) {
    return 1;
  }

  if (streams.size() < 1 && !proxyREGISTERRequests) {
    usage();
  }

  std::vector<ProxyStream>::iterator it;

  // Make sure that the remaining arguments appear to be "rtsp://" URLs:
  for(it = streams.begin(); it != streams.end(); ++it) {
    if ((*it).proxiedURL.find("rtsp://") != 0) {
      *env << "Invalid URL " << (*it).proxiedURL.c_str();
      return 1;
    }

    if (streamRTPOverTCP) {
      if ((*it).tunnelOverHTTPPortNum > 0) {
        *env << "Invalid configuration for " << (*it).proxiedURL.c_str() << ": the -t and -T options cannot both be used!\n";
        return 1;
      } else {
        (*it).tunnelOverHTTPPortNum = (portNumBits)(~0); // hack to tell "ProxyServerMediaSession" to stream over TCP, but not using HTTP
      }
    }
  }

  // Do some additional checking for invalid command-line argument combinations:
  if (authDBForREGISTER != NULL && !proxyREGISTERRequests) {
    *env << "The '-U <username> <password>' option can be used only with -R\n";
    usage();
  }

  // Create the RTSP server. Try first with the configured port number,
  // and then with the default port number (554) if different,
  // and then with the alternative port number (8554):
  RTSPServer* rtspServer;
  rtspServer = createRTSPServer(rtspServerPortNum);
  if (rtspServer == NULL && tryStandardPortNumbers && rtspServerPortNum != 554) {
    *env << "Unable to create a RTSP server with port number " << rtspServerPortNum << ": " << env->getResultMsg() << "\n";
    *env << "Trying instead with the standard port numbers (554 and 8554)...\n";

    rtspServerPortNum = 554;
    rtspServer = createRTSPServer(rtspServerPortNum);
  }
  if (rtspServer == NULL && tryStandardPortNumbers) {
    rtspServerPortNum = 8554;
    rtspServer = createRTSPServer(rtspServerPortNum);
  }
  if (rtspServer == NULL) {
    *env << "Failed to create RTSP server: " << env->getResultMsg() << "\n";
    exit(1);
  }

  // Create a proxy for each "rtsp://" URL specified on the command line:
  for(it = streams.begin(); it != streams.end(); ++it) {
    ServerMediaSession* sms
      = ProxyServerMediaSession::createNew(*env, rtspServer,
          (*it).proxiedURL.c_str(),
          (*it).streamName.c_str(),
          (*it).username.length() > 0 ? (*it).username.c_str() : NULL,
          (*it).password.length() > 0 ? (*it).password.c_str() : NULL,
          (*it).tunnelOverHTTPPortNum, verbosityLevel);
    rtspServer->addServerMediaSession(sms);

    char* proxyStreamURL = rtspServer->rtspURL(sms);
    *env << "RTSP stream, proxying the stream \"" << (*it).proxiedURL.c_str() << "\"\n";
    *env << "\tPlay this stream using the URL: " << proxyStreamURL << "\n";
    delete[] proxyStreamURL;
  }

  if (proxyREGISTERRequests) {
    *env << "(We handle incoming \"REGISTER\" requests on port " << rtspServerPortNum << ")\n";
  }

  // Also, attempt to create a HTTP server for RTSP-over-HTTP tunneling.
  // Try first with the default HTTP port (80), and then with the alternative HTTP
  // port numbers (8000 and 8080).

  if (serverTunnelingOverHTTP) {
    Boolean success;

    if (serverTunnelingOverHTTPPortNum == 0) {
      success = (rtspServer->setUpTunnelingOverHTTP(80) || rtspServer->setUpTunnelingOverHTTP(8000) || rtspServer->setUpTunnelingOverHTTP(8080));
    } else {
      success = rtspServer->setUpTunnelingOverHTTP(serverTunnelingOverHTTPPortNum);
    }

    if (success) {
      *env << "\n(We use port " << rtspServer->httpServerPortNum() << " for optional RTSP-over-HTTP tunneling.)\n";
    } else {
      *env << "\n(RTSP-over-HTTP tunneling is not available.)\n";
    }
  } else {
    *env << "\n(RTSP-over-HTTP tunneling is disabled.)\n";
  }

  // Now, enter the event loop:
  env->taskScheduler().doEventLoop(); // does not return

  return 0; // only to prevent compiler warning
}
