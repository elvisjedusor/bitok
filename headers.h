// Copyright (c) 2009-2010 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifdef _MSC_VER
#pragma warning(disable:4786)
#pragma warning(disable:4804)
#pragma warning(disable:4805)
#pragma warning(disable:4717)
#endif
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0601
#ifdef _WIN32_IE
#undef _WIN32_IE
#endif
#define _WIN32_IE 0x0601
#define __STDC_LIMIT_MACROS // to enable UINT64_MAX from stdint.h

#if defined(_WIN32) || defined(__MINGW32__) || defined(__WXMSW__)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#if defined(_WIN32) || defined(__MINGW32__) || defined(__WXMSW__)
#ifndef CSIDL_APPDATA
#define CSIDL_APPDATA 0x001a
#endif
#ifndef CSIDL_STARTUP
#define CSIDL_STARTUP 0x0007
#endif
#endif

#include <string>

#if wxUSE_GUI
#include <wx/wx.h>
#include <wx/stdpaths.h>
#include <wx/snglinst.h>
#include <wx/utils.h>
#include <wx/clipbrd.h>
#include <wx/taskbar.h>
#else
#define _(x) (x)
#define wxOK 0
#define wxICON_EXCLAMATION 0
#define wxICON_ERROR 0
#define wxYES 0
#define wxNO 0
#define wxYES_NO 0
#define wxCANCEL 0
#define wxID_YES 0
#define wxID_OK 0
#define wxID_NO 0
#define wxID_CANCEL 0
#endif

#define _STR(x) std::string(x)

// Include Berkeley DB after wxWidgets to let wxWidgets define ssize_t first
#include "db_cxx_compat.h"

#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <assert.h>
#include <memory>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <list>
#include <deque>
#include <map>
#include <set>
#include <algorithm>
#include <numeric>

// Suppress Boost bind placeholder deprecation warning
#ifndef BOOST_BIND_GLOBAL_PLACEHOLDERS
#define BOOST_BIND_GLOBAL_PLACEHOLDERS
#endif

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/tuple/tuple_comparison.hpp>
#include <boost/tuple/tuple_io.hpp>
#include <boost/array.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

#if defined(_WIN32) || defined(__MINGW32__) || defined(__WXMSW__)
#include <winsock2.h>
#include <windows.h>
#include <mswsock.h>
#include <io.h>
#include <direct.h>
#include <process.h>
#include <malloc.h>
inline BOOL PathRemoveFileSpecA(LPSTR pszPath) {
    if (!pszPath) return FALSE;
    char* lastSlash = strrchr(pszPath, '\\');
    char* lastFwdSlash = strrchr(pszPath, '/');
    if (lastFwdSlash > lastSlash) lastSlash = lastFwdSlash;
    if (lastSlash) { *lastSlash = '\0'; return TRUE; }
    return FALSE;
}
#define PathRemoveFileSpec PathRemoveFileSpecA
#else
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#endif
#ifdef __BSD__
#include <netinet/in.h>
#endif


#pragma hdrstop
using namespace std;
using namespace boost;

#include "strlcpy.h"
#include "serialize.h"
#include "uint256.h"
#include "util.h"
#include "key.h"
#include "bignum.h"
#include "base58.h"
#include "script.h"
#include "stealth.h"
#include "bitcoin_db.h"
#include "atom.h"
#include "main.h"
#include "bloom.h"
#include "merkleblock.h"
#include "net.h"
#include "irc.h"
#include "rpc.h"
#if wxUSE_GUI
#include "uibase.h"
#endif
#include "ui.h"
#include "init.h"

#if wxUSE_GUI
#include "xpm/addressbook16.xpm"
#include "xpm/addressbook20.xpm"
#include "xpm/bitcoin16.xpm"
#include "xpm/bitcoin20.xpm"
#include "xpm/bitcoin32.xpm"
#include "xpm/bitcoin48.xpm"
#include "xpm/bitcoin80.xpm"
#include "xpm/check.xpm"
#include "xpm/send16.xpm"
#include "xpm/send16noshadow.xpm"
#include "xpm/send20.xpm"
#include "xpm/about.xpm"
#endif
