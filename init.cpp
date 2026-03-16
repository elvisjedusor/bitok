// Copyright (c) 2009-2010 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "headers.h"
#include "crypto/sha256.h"
#include "yespower_dispatch.h"
#if wxUSE_GUI
#include <wx/progdlg.h>
#endif

extern void InitSHA256();

#if wxUSE_GUI
static bool AtomStartupProgressCallback(wxProgressDialog* dlg, int nScanned, int nTotal)
{
    int pct = (nTotal > 0) ? (nScanned * 100 / nTotal) : 0;
    if (pct > 100) pct = 100;
    dlg->Update(pct, wxString::Format(_("Scanned %d / %d blocks"), nScanned, nTotal));
    wxSafeYield(dlg);
    return true;
}
#endif


void ExitTimeout(void* parg)
{
    Sleep(5000);
#if defined(_WIN32) || defined(__MINGW32__) || defined(__WXMSW__)
    ExitProcess(0);
#else
    _exit(0);
#endif
}

void Shutdown(void* parg)
{
    static CCriticalSection cs_Shutdown;
    static bool fTaken;
    bool fFirstThread;
    CRITICAL_BLOCK(cs_Shutdown)
    {
        fFirstThread = !fTaken;
        fTaken = true;
    }
    static bool fExit;
    if (fFirstThread)
    {
        fShutdown = true;
        nTransactionsUpdated++;
        DBFlush(false);
        StopNode();
        DBFlush(true);
        CreateThread(ExitTimeout, NULL);
        Sleep(50);
        printf("Bitok exiting\n\n");
        fExit = true;
        exit(0);
    }
    else
    {
        while (!fExit)
            Sleep(500);
        Sleep(100);
        ExitThread(0);
    }
}






//////////////////////////////////////////////////////////////////////////////
//
// Startup folder
//

#if defined(_WIN32) || defined(__MINGW32__) || defined(__WXMSW__)
typedef BOOL (WINAPI *PSHGETSPECIALFOLDERPATHA)(HWND hwndOwner, LPSTR lpszPath, int nFolder, BOOL fCreate);

string MyGetSpecialFolderPath(int nFolder, bool fCreate)
{
    char pszPath[MAX_PATH+100] = "";

    // SHGetSpecialFolderPath is not usually available on NT 4.0
    HMODULE hShell32 = LoadLibraryA("shell32.dll");
    if (hShell32)
    {
        PSHGETSPECIALFOLDERPATHA pSHGetSpecialFolderPath =
            (PSHGETSPECIALFOLDERPATHA)GetProcAddress(hShell32, "SHGetSpecialFolderPathA");
        if (pSHGetSpecialFolderPath)
            (*pSHGetSpecialFolderPath)(NULL, pszPath, nFolder, fCreate);
        FreeModule(hShell32);
    }

    // Backup option
    if (pszPath[0] == '\0')
    {
        if (nFolder == CSIDL_STARTUP)
        {
            strcpy(pszPath, getenv("USERPROFILE"));
            strcat(pszPath, "\\Start Menu\\Programs\\Startup");
        }
        else if (nFolder == CSIDL_APPDATA)
        {
            strcpy(pszPath, getenv("APPDATA"));
        }
    }

    return pszPath;
}

string StartupShortcutPath()
{
    return MyGetSpecialFolderPath(CSIDL_STARTUP, true) + "\\Bitok.lnk";
}

bool GetStartOnSystemStartup()
{
#if wxUSE_GUI
    return wxFileExists(StartupShortcutPath());
#else
    return false;
#endif
}

void SetStartOnSystemStartup(bool fAutoStart)
{
    remove(StartupShortcutPath().c_str());
    if (fAutoStart)
    {
        printf("Note: Auto-start shortcut creation not available in this build.\n");
        printf("To start Bitok on login, manually create a shortcut in your Startup folder.\n");
    }
}
#else
bool GetStartOnSystemStartup() { return false; }
void SetStartOnSystemStartup(bool fAutoStart) { }
#endif







//////////////////////////////////////////////////////////////////////////////
//
// CMyApp
//

#if wxUSE_GUI
// Define a new application
class CMyApp: public wxApp
{
public:
    wxLocale m_locale;

    CMyApp(){};
    ~CMyApp(){};
    bool OnInit();
    bool OnInit2();
    int OnExit();

    // Hook Initialize so we can start without GUI
    virtual bool Initialize(int& argc, wxChar** argv);

    // 2nd-level exception handling: we get all the exceptions occurring in any
    // event handler here
    virtual bool OnExceptionInMainLoop();

    // 3rd, and final, level exception handling: whenever an unhandled
    // exception is caught, this function is called
    virtual void OnUnhandledException();

    // and now for something different: this function is called in case of a
    // crash (e.g. dereferencing null pointer, division by 0, ...)
    virtual void OnFatalException();
};

IMPLEMENT_APP(CMyApp)

bool CMyApp::Initialize(int& argc, wxChar** argv)
{
    if (argc > 1 && argv[1][0] != '-' && (!fWindows || argv[1][0] != '/') &&
        wxString(argv[1]) != "start")
    {
        fCommandLine = true;
    }
    else if (!fGUI)
    {
        fDaemon = true;
    }
    else
    {
        // wxApp::Initialize will remove environment-specific parameters,
        // so it's too early to call ParseParameters yet
        for (int i = 1; i < argc; i++)
        {
            wxString str = argv[i];
            #if defined(_WIN32) || defined(__MINGW32__) || defined(__WXMSW__)
            if (str.size() >= 1 && str[0] == '/')
                str[0] = '-';
            str = str.MakeLower();
            #endif
            // haven't decided which argument to use for this yet
            if (str == "-daemon" || str == "-d" || str == "start")
                fDaemon = true;
        }
    }

    // Handle daemonization for Unix/Linux systems
#if !defined(_WIN32) && !defined(__MINGW32__) && !defined(__WXMSW__)
    if (fDaemon)
    {
        // Fork process to background
        pid_t pid = fork();
        if (pid < 0)
        {
            fprintf(stderr, "Error: fork() returned %d errno %d\n", pid, errno);
            return false;
        }
        if (pid > 0)
        {
            // Parent process exits
            exit(0);
        }

        // Child process continues
        // Create new session and detach from terminal
        if (setsid() < 0)
        {
            fprintf(stderr, "Error: setsid() failed errno %d\n", errno);
            return false;
        }

        // Change working directory to root to avoid blocking unmount
        // (actually, let's keep the current directory for .bitok access)

        // Close standard file descriptors
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        // Redirect to /dev/null
        int fd = open("/dev/null", O_RDWR);
        if (fd != -1)
        {
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            if (fd > STDERR_FILENO)
                close(fd);
        }
    }
#endif

#ifdef __WXGTK__
    if (fDaemon || fCommandLine)
    {
        // Call the original Initialize while suppressing error messages
        // and ignoring failure.  If unable to initialize GTK, it fails
        // near the end so hopefully the last few things don't matter.
        {
            wxLogNull logNo;
            wxApp::Initialize(argc, argv);
        }

        return true;
    }
#endif

    return wxApp::Initialize(argc, argv);
}

bool CMyApp::OnInit()
{
    bool fRet = false;
    try
    {
        fRet = OnInit2();
    }
    catch (std::exception& e) {
        PrintException(&e, "OnInit()");
    } catch (...) {
        PrintException(NULL, "OnInit()");
    }
    if (!fRet)
        Shutdown(NULL);
    return fRet;
}

extern int g_isPainting;

bool CMyApp::OnInit2()
{
#ifdef _MSC_VER
    // Turn off microsoft heap dump noise
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, CreateFileA("NUL", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0));
#endif
#if _MSC_VER >= 1400
    // Disable confusing "helpful" text message on abort, ctrl-c
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
#if defined(__WXMSW__) && defined(__WXDEBUG__) && wxUSE_GUI
    // Disable malfunctioning wxWidgets debug assertion
    g_isPainting = 10000;
#endif
#if wxUSE_GUI
    wxImage::AddHandler(new wxPNGHandler);
#endif
// Bitok: Updated app name for data directory
#if defined(__WXMSW__ ) || defined(__WXMAC__)
    SetAppName("Bitok");
#else
    SetAppName("bitokd");
#endif
#if !defined(_WIN32) && !defined(__MINGW32__) && !defined(__WXMSW__)
    umask(077);
#endif
    InitSHA256();
    yespower_init_dispatch();

#if defined(_WIN32) || defined(__MINGW32__) || defined(__WXMSW__)
#if wxUSE_UNICODE
    // Hack to set wxConvLibc codepage to UTF-8 on Windows,
    // may break if wxMBConv_win32 implementation in strconv.cpp changes.
    class wxMBConv_win32 : public wxMBConv
    {
    public:
        long m_CodePage;
        size_t m_minMBCharWidth;
    };
    if (((wxMBConv_win32*)&wxConvLibc)->m_CodePage == CP_ACP)
        ((wxMBConv_win32*)&wxConvLibc)->m_CodePage = CP_UTF8;
#endif
#endif

    // Load locale/<lang>/LC_MESSAGES/bitok.mo language file
    m_locale.Init(wxLANGUAGE_DEFAULT, 0);
    m_locale.AddCatalogLookupPathPrefix("locale");
    if (!fWindows)
    {
        m_locale.AddCatalogLookupPathPrefix("/usr/share/locale");
        m_locale.AddCatalogLookupPathPrefix("/usr/local/share/locale");
    }
    m_locale.AddCatalog("wxstd"); // wxWidgets standard translations, if any
    m_locale.AddCatalog("bitok");

    //
    // Parameters
    //
    if (fCommandLine)
    {
        int ret = CommandLineRPC(argc, argv);
        exit(ret);
    }

    ParseParameters(argc, argv);

    if (mapArgs.count("-datadir"))
        strlcpy(pszSetDataDir, mapArgs["-datadir"].c_str(), sizeof(pszSetDataDir));

    map<string, string> mapConfigSettings;
    ReadConfigFile(mapConfigSettings);

    for (map<string, string>::iterator mi = mapConfigSettings.begin(); mi != mapConfigSettings.end(); ++mi)
    {
        if (mapArgs.count((*mi).first) == 0)
            mapArgs[(*mi).first] = (*mi).second;
    }

    if (mapArgs.count("-?") || mapArgs.count("--help"))
    {
        wxString strUsage = string() +
          _("Usage:") + "\t\t\t\t\t\t\t\t\t\t\n" +
            "  bitok [options]       \t" + "\n" +
            "  bitok [command]       \t" + _("Send command to bitokd running with -server or -daemon\n") +
            "  bitok [command] --help\t" + _("Get help for a command\n") +
            "  bitok help            \t" + _("List commands\n") +
          _("Options:\n") +
            "  -gen            \t  " + _("Generate coins\n") +
            "  -gen=0          \t  " + _("Don't generate coins\n") +
            "  -genproclimit=<n>\t  " + _("Limit mining to n processors (-1 = all)\n") +
            "  -min            \t  " + _("Start minimized\n") +
            "  -datadir=<dir>  \t  " + _("Specify data directory\n") +
            "  -proxy=<ip:port>\t  " + _("Connect through socks4 proxy\n") +
            "  -addnode=<ip>   \t  " + _("Add a node to connect to\n") +
            "  -connect=<ip>   \t  " + _("Connect only to the specified node\n") +
            "  -server         \t  " + _("Accept command line and JSON-RPC commands\n") +
            "  -daemon         \t  " + _("Run in the background as a daemon and accept commands\n") +
            "  -irc            \t  " + _("Enable IRC peer discovery (disabled by default)\n") +
            "  -recover        \t  " + _("Recover database and extract keys from corrupted wallet\n") +
            "  --help          \t  " + _("This help message\n");


        if (fWindows && fGUI)
        {
            // Tabs make the columns line up in the message box
            wxMessageBox(strUsage, "Bitok", wxOK);
        }
        else
        {
            // Remove tabs
            strUsage.Replace("\t", "");
            fprintf(stderr, "%s", ((string)strUsage).c_str());
        }
        return false;
    }

    if (mapArgs.count("-datadir"))
        strlcpy(pszSetDataDir, mapArgs["-datadir"].c_str(), sizeof(pszSetDataDir));

    fDebug = GetBoolArg("-debug");
    if (fDebug)
        fPrintToConsole = true;

    fCORS = GetBoolArg("-cors");

    fPrintToDebugger = GetBoolArg("-printtodebugger");

    fUseIndexer = GetBoolArg("-indexer");
    if (fUseIndexer)
        printf("UTXO/address indexer: enabled\n");

    if (!fDebug && !pszSetDataDir[0])
        ShrinkDebugFile();
    printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
    printf("Bitok version %d.%d.%d%s\n", VERSION/10000, (VERSION/100)%100, VERSION%100, pszSubVer);
    printf("Debug mode: %s\n", fDebug ? "ON" : "OFF");
    if (fCORS)
        printf("CORS: enabled\n");
    printf("System default language is %d\n", m_locale.GetSystemLanguage());
    printf("Language file loading...\n");

    if (GetBoolArg("-loadblockindextest"))
    {
        CTxDB txdb("r");
        txdb.LoadBlockIndex();
        PrintBlockTree();
        return false;
    }

    if (GetBoolArg("-recover"))
    {
        printf("Running database recovery...\n");
        if (!RecoverDatabaseEnvironment())
        {
            wxMessageBox(_("Database recovery failed. You may need to delete database files manually."), "Bitok", wxOK | wxICON_ERROR);
            return false;
        }
        printf("Database recovery completed.\n");

        printf("Running wallet key recovery...\n");
        if (!RecoverWalletKeys())
        {
            wxMessageBox(_("Wallet key recovery failed. Check console for details."), "Bitok", wxOK | wxICON_ERROR);
            return false;
        }
        printf("Wallet key recovery completed. Continuing startup...\n");
    }

    //
    // Limit to single instance per user
    // Required to protect the database files if we're going to keep deleting log.*
    //
#ifdef __WXMSW__
    // todo: wxSingleInstanceChecker wasn't working on Linux, never deleted its lock file
    //  maybe should go by whether successfully bind port 8333 instead
    wxString strMutexName = wxString("bitok_running.") + getenv("HOMEPATH");
    for (int i = 0; i < strMutexName.size(); i++)
        if (!isalnum(strMutexName[i]))
            strMutexName[i] = '.';
    wxSingleInstanceChecker* psingleinstancechecker = new wxSingleInstanceChecker(strMutexName);
    if (psingleinstancechecker->IsAnotherRunning())
    {
        printf("Existing instance found\n");
        unsigned int nStart = GetTime();
        loop
        {
            // TODO: find out how to do this in Linux, or replace with wxWidgets commands
            // Show the previous instance and exit
            HWND hwndPrev = FindWindowA("wxWindowClassNR", "Bitok");
            if (hwndPrev)
            {
                if (IsIconic(hwndPrev))
                    ShowWindow(hwndPrev, SW_RESTORE);
                SetForegroundWindow(hwndPrev);
                return false;
            }

            if (GetTime() > nStart + 60)
                return false;

            // Resume this instance if the other exits
            delete psingleinstancechecker;
            Sleep(1000);
            psingleinstancechecker = new wxSingleInstanceChecker(strMutexName);
            if (!psingleinstancechecker->IsAnotherRunning())
                break;
        }
    }
#endif

    // Bind to the port early so we can tell if another instance is already running.
    // This is a backup to wxSingleInstanceChecker, which doesn't work on Linux.
    string strErrors;
    if (!BindListenPort(strErrors))
    {
        wxMessageBox(strErrors, "Bitok");
        return false;
    }

    //
    // Load data files
    //
    if (fDaemon)
        fprintf(stdout, "Bitok server starting\n");
    strErrors = "";
    int64 nStart;

    printf("Loading addresses...\n");
    nStart = GetTimeMillis();
    if (!LoadAddresses())
        strErrors += _STR("Error loading addr.dat      \n");
    printf(" addresses   %15" PRI64d "ms\n", GetTimeMillis() - nStart);

    printf("Loading block index...\n");
    nStart = GetTimeMillis();
    if (!LoadBlockIndex())
        strErrors += _STR("Error loading blkindex.dat      \n");
    printf(" block index %15" PRI64d "ms\n", GetTimeMillis() - nStart);

    if (fUseIndexer && strErrors.empty())
    {
        bool fNeedReindex = GetBoolArg("-reindex");
        if (!fNeedReindex)
        {
            CTxDB txdb("r");
            int nIndexerHeight = -1;
            txdb.ReadIndexerHeight(nIndexerHeight);
            txdb.Close();
            if (nIndexerHeight != nBestHeight)
            {
                printf("Indexer height %d != best height %d, triggering reindex\n", nIndexerHeight, nBestHeight);
                fNeedReindex = true;
            }
        }
        if (fNeedReindex)
        {
            printf("Building UTXO and address indexes...\n");
            nStart = GetTimeMillis();
            fIndexerRebuilding = true;
            if (!ReindexUTXOs())
                strErrors += _STR("Error building UTXO indexes      \n");
            printf(" reindex     %15" PRI64d "ms\n", GetTimeMillis() - nStart);
        }
        else
        {
            printf("Indexer up to date at height %d\n", nBestHeight);
        }
    }

    printf("Loading wallet...\n");
    nStart = GetTimeMillis();
    bool fFirstRun;
    if (!LoadWallet(fFirstRun))
        strErrors += _STR("Error loading wallet.dat      \n");
    printf(" wallet      %15" PRI64d "ms\n", GetTimeMillis() - nStart);

    // Initialize nLimitProcessors to CPU count - 1 if not set (leave 1 core for system)
    if (nLimitProcessors == 1 && fFirstRun)
    {
#if wxUSE_GUI
        int nProcessors = wxThread::GetCPUCount();
#elif defined(_WIN32) || defined(__MINGW32__)
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        int nProcessors = sysinfo.dwNumberOfProcessors;
#else
        int nProcessors = sysconf(_SC_NPROCESSORS_ONLN);
#endif
        if (nProcessors > 1)
        {
            nLimitProcessors = nProcessors - 1;
            CWalletDB().WriteSetting("nLimitProcessors", nLimitProcessors);
            printf("Initialized nLimitProcessors to %d (leaving 1 core for system)\n", nLimitProcessors);
        }
    }

    {
        CTxDB txdb("r");
        int64 nAtomMinted = 0;
        bool fHasAtomData = txdb.ReadAtomTotalMinted(nAtomMinted);
        txdb.Close();
        if (!fHasAtomData && nBestHeight > 0)
        {
            printf("[ATOM] No ATOM index found in database, running automatic ATOM rescan...\n");
            nStart = GetTimeMillis();
#if wxUSE_GUI
            wxProgressDialog atomDlg(
                _("Building ATOM Index"),
                _("Scanning blockchain for ATOM data..."),
                100,
                NULL,
                wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_SMOOTH | wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME);
            RescanAtom(boost::bind(AtomStartupProgressCallback, &atomDlg, _1, _2));
            atomDlg.Update(100);
#else
            RescanAtom();
#endif
            printf(" atom rescan %15" PRI64d "ms\n", GetTimeMillis() - nStart);
        }
    }

    printf("Done loading\n");

        //// debug print
        printf("mapBlockIndex.size() = %d\n",   mapBlockIndex.size());
        printf("nBestHeight = %d\n",            nBestHeight);
        printf("mapKeys.size() = %d\n",         mapKeys.size());
        printf("mapPubKeys.size() = %d\n",      mapPubKeys.size());
        printf("mapWallet.size() = %d\n",       mapWallet.size());
        printf("mapAddressBook.size() = %d\n",  mapAddressBook.size());

    if (!strErrors.empty())
    {
        wxMessageBox(strErrors, "Bitcoin");
        return false;
    }

    // Add wallet transactions that aren't already in a block to mapTransactions
    ReacceptWalletTransactions();

    //
    // Parameters
    //
    if (GetBoolArg("-printblockindex") || GetBoolArg("-printblocktree"))
    {
        PrintBlockTree();
        return false;
    }

    if (mapArgs.count("-printblock"))
    {
        string strMatch = mapArgs["-printblock"];
        int nFound = 0;
        for (auto mi = mapBlockIndex.begin(); mi != mapBlockIndex.end(); ++mi)
        {
            uint256 hash = (*mi).first;
            if (strncmp(hash.ToString().c_str(), strMatch.c_str(), strMatch.size()) == 0)
            {
                CBlockIndex* pindex = (*mi).second;
                CBlock block;
                block.ReadFromDisk(pindex);
                block.BuildMerkleTree();
                block.print();
                printf("\n");
                nFound++;
            }
        }
        if (nFound == 0)
            printf("No blocks matching %s were found\n", strMatch.c_str());
        return false;
    }

    if (mapArgs.count("-gen"))
    {
        if (mapArgs["-gen"].empty())
            fGenerateBitcoins = true;
        else
            fGenerateBitcoins = (atoi(mapArgs["-gen"].c_str()) != 0);
    }

    fTestMode = GetBoolArg("-testmode");
    if (fTestMode)
        printf("TEST MODE enabled - no peer connections, minimum difficulty\n");

    fUseIRC = GetBoolArg("-irc");
    if (fUseIRC)
        printf("IRC peer discovery enabled\n");

    if (mapArgs.count("-genproclimit"))
    {
        int nLimit = atoi(mapArgs["-genproclimit"].c_str());
        if (nLimit == -1)
        {
            fLimitProcessors = false;
            printf("Mining processor limit disabled - using all processors\n");
        }
        else if (nLimit > 0)
        {
            fLimitProcessors = true;
            nLimitProcessors = nLimit;
            printf("Mining limited to %d processors\n", nLimitProcessors);
        }
    }

    if (mapArgs.count("-proxy"))
    {
        fUseProxy = true;
        addrProxy.SetAddress(mapArgs["-proxy"]);
        if (!addrProxy.IsValid())
        {
            wxMessageBox(_("Invalid -proxy address"), "Bitok");
            return false;
        }
    }

    if (mapArgs.count("-addnode"))
    {
        foreach(string strAddr, mapMultiArgs["-addnode"])
        {
            CAddress addr(strAddr, NODE_NETWORK);
            addr.nTime = 0; // so it won't relay unless successfully connected
            if (addr.IsValid())
                AddAddress(addr);
        }
    }

    //
    // Create the main window and start the node
    //
    if (!fDaemon)
        CreateMainWindow();

    if (!CheckDiskSpace())
        return false;

    RandAddSeedPerfmon();

    if (!CreateThread(StartNode, NULL))
        wxMessageBox("Error: CreateThread(StartNode) failed", "Bitok");

    if (GetBoolArg("-server") || fDaemon)
        CreateThread(ThreadRPCServer, NULL);

    if (fFirstRun)
        SetStartOnSystemStartup(true);

    return true;
}

int CMyApp::OnExit()
{
    Shutdown(NULL);
    return wxApp::OnExit();
}

bool CMyApp::OnExceptionInMainLoop()
{
    try
    {
        throw;
    }
    catch (std::exception& e)
    {
        PrintException(&e, "CMyApp::OnExceptionInMainLoop()");
        wxLogWarning("Exception %s %s", typeid(e).name(), e.what());
        Sleep(1000);
        throw;
    }
    catch (...)
    {
        PrintException(NULL, "CMyApp::OnExceptionInMainLoop()");
        wxLogWarning("Unknown exception");
        Sleep(1000);
        throw;
    }
    return true;
}

void CMyApp::OnUnhandledException()
{
    // this shows how we may let some exception propagate uncaught
    try
    {
        throw;
    }
    catch (std::exception& e)
    {
        PrintException(&e, "CMyApp::OnUnhandledException()");
        wxLogWarning("Exception %s %s", typeid(e).name(), e.what());
        Sleep(1000);
        throw;
    }
    catch (...)
    {
        PrintException(NULL, "CMyApp::OnUnhandledException()");
        wxLogWarning("Unknown exception");
        Sleep(1000);
        throw;
    }
}

void CMyApp::OnFatalException()
{
    wxMessageBox(_("Program has crashed and will terminate.  "), "Bitok", wxOK | wxICON_ERROR);
}

#else

bool AppInit(int argc, char* argv[])
{
#ifdef _MSC_VER
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, CreateFileA("NUL", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0));
#endif
#if _MSC_VER >= 1400
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
#if !defined(_WIN32) && !defined(__MINGW32__) && !defined(__WXMSW__)
    umask(077);
#endif

    // Check if any argument is an RPC command (not starting with -)
    // RPC commands can appear after options like -datadir
    int rpcArgIndex = -1;
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] != '-')
        {
            rpcArgIndex = i;
            fCommandLine = true;
            break;
        }
    }

    if (!fCommandLine)
    {
        fDaemon = true;

#if !defined(_WIN32) && !defined(__MINGW32__)
        bool fBackground = false;
        for (int i = 1; i < argc; i++)
        {
            if (strcmp(argv[i], "-daemon") == 0 || strcmp(argv[i], "--daemon") == 0)
            {
                fBackground = true;
                break;
            }
        }

        if (fBackground)
        {
            pid_t pid = fork();
            if (pid < 0)
            {
                fprintf(stderr, "Error: fork() returned %d errno %d\n", pid, errno);
                return false;
            }
            if (pid > 0)
                exit(0);

            setsid();

            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO);

            int fd = open("/dev/null", O_RDWR);
            if (fd >= 0)
            {
                dup2(fd, STDIN_FILENO);
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                if (fd > STDERR_FILENO)
                    close(fd);
            }
        }
#endif

        InitSHA256();
        yespower_init_dispatch();
    }

    if (fCommandLine)
    {
        ParseParameters(argc, argv);

        if (mapArgs.count("-datadir"))
            strlcpy(pszSetDataDir, mapArgs["-datadir"].c_str(), sizeof(pszSetDataDir));

        map<string, string> mapConfigSettings;
        ReadConfigFile(mapConfigSettings);
        for (map<string, string>::iterator mi = mapConfigSettings.begin(); mi != mapConfigSettings.end(); ++mi)
        {
            if (mapArgs.count((*mi).first) == 0)
                mapArgs[(*mi).first] = (*mi).second;
        }

        int rpcArgc = 1 + (argc - rpcArgIndex);
        char** rpcArgv = new char*[rpcArgc];
        rpcArgv[0] = argv[0];
        for (int i = rpcArgIndex; i < argc; i++)
            rpcArgv[1 + i - rpcArgIndex] = argv[i];

        int ret = CommandLineRPC(rpcArgc, rpcArgv);
        delete[] rpcArgv;
        exit(ret);
    }

    ParseParameters(argc, argv);

    if (mapArgs.count("-datadir"))
        strlcpy(pszSetDataDir, mapArgs["-datadir"].c_str(), sizeof(pszSetDataDir));

    map<string, string> mapConfigSettings;
    ReadConfigFile(mapConfigSettings);

    for (map<string, string>::iterator mi = mapConfigSettings.begin(); mi != mapConfigSettings.end(); ++mi)
    {
        if (mapArgs.count((*mi).first) == 0)
            mapArgs[(*mi).first] = (*mi).second;
    }

    if (mapArgs.count("-?") || mapArgs.count("--help"))
    {
        string strUsage = string() +
          _("Usage:") + "\n" +
            "  bitokd [options]         \n" +
            "  bitokd [command]         " + _("Send command to bitokd running with -server or -daemon\n") +
            "  bitokd [command] --help  " + _("Get help for a command\n") +
            "  bitokd help              " + _("List commands\n") +
          _("Options:\n") +
            "  -gen              " + _("Generate coins\n") +
            "  -gen=0            " + _("Don't generate coins\n") +
            "  -genproclimit=<n> " + _("Limit mining to n processors (-1 = all)\n") +
            "  -datadir=<dir>    " + _("Specify data directory\n") +
            "  -proxy=<ip:port>  " + _("Connect through socks4 proxy\n") +
            "  -addnode=<ip>     " + _("Add a node to connect to\n") +
            "  -connect=<ip>     " + _("Connect only to the specified node\n") +
            "  -server           " + _("Accept command line and JSON-RPC commands\n") +
            "  -daemon           " + _("Run in the background as a daemon and accept commands\n") +
            "  -irc              " + _("Enable IRC peer discovery (disabled by default)\n") +
            "  -recover          " + _("Recover database and extract keys from corrupted wallet\n") +
            "  -indexer          " + _("Enable UTXO and address indexing (getaddressbalance, getaddressutxos, etc.)\n") +
            "  -reindex          " + _("Force rebuild of UTXO indexes on startup (use with -indexer)\n") +
            "  --help            " + _("This help message\n");
        fprintf(stderr, "%s", strUsage.c_str());
        return false;
    }

    if (mapArgs.count("-datadir"))
        strlcpy(pszSetDataDir, mapArgs["-datadir"].c_str(), sizeof(pszSetDataDir));

    fDebug = GetBoolArg("-debug");
    if (fDebug)
        fPrintToConsole = true;

    fCORS = GetBoolArg("-cors");

    fPrintToDebugger = GetBoolArg("-printtodebugger");

    fUseIndexer = GetBoolArg("-indexer");
    if (fUseIndexer)
        printf("UTXO/address indexer: enabled\n");

    if (!fDebug && !pszSetDataDir[0])
        ShrinkDebugFile();

    printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
    printf("Bitok version %d.%d.%d%s\n", VERSION/10000, (VERSION/100)%100, VERSION%100, pszSubVer);
    printf("Debug mode: %s\n", fDebug ? "ON" : "OFF");
    if (fCORS)
        printf("CORS: enabled\n");

    if (GetBoolArg("-loadblockindextest"))
    {
        CTxDB txdb("r");
        txdb.LoadBlockIndex();
        PrintBlockTree();
        return false;
    }

    if (GetBoolArg("-recover"))
    {
        printf("Running database recovery...\n");
        if (!RecoverDatabaseEnvironment())
        {
            fprintf(stderr, "Database recovery failed. You may need to delete database files manually.\n");
            return false;
        }
        printf("Database recovery completed.\n");

        printf("Running wallet key recovery...\n");
        if (!RecoverWalletKeys())
        {
            fprintf(stderr, "Wallet key recovery failed. Check output for details.\n");
            return false;
        }
        printf("Wallet key recovery completed. Continuing startup...\n");
    }

    string strErrors;
    if (!BindListenPort(strErrors))
    {
        fprintf(stderr, "%s\n", strErrors.c_str());
        return false;
    }

    if (mapArgs.count("-gen"))
    {
        if (mapArgs["-gen"].empty())
            fGenerateBitcoins = true;
        else
            fGenerateBitcoins = (atoi(mapArgs["-gen"].c_str()) != 0);
    }

    fTestMode = GetBoolArg("-testmode");
    if (fTestMode)
        printf("TEST MODE enabled - no peer connections, minimum difficulty\n");

    fUseIRC = GetBoolArg("-irc");
    if (fUseIRC)
        printf("IRC peer discovery enabled\n");

    if (mapArgs.count("-genproclimit"))
    {
        int nLimit = atoi(mapArgs["-genproclimit"].c_str());
        if (nLimit == -1)
        {
            fLimitProcessors = false;
            printf("Mining processor limit disabled - using all processors\n");
        }
        else if (nLimit > 0)
        {
            fLimitProcessors = true;
            nLimitProcessors = nLimit;
            printf("Mining limited to %d processors\n", nLimitProcessors);
        }
    }

    printf("Loading addresses...\n");
    if (!LoadAddresses())
        fprintf(stderr, "Warning: Error loading addresses\n");

    printf("Loading block index...\n");
    if (!LoadBlockIndex())
    {
        fprintf(stderr, "Error loading block index\n");
        return false;
    }

    if (fUseIndexer)
    {
        bool fNeedReindex = GetBoolArg("-reindex");
        if (!fNeedReindex)
        {
            CTxDB txdb("r");
            int nIndexerHeight = -1;
            txdb.ReadIndexerHeight(nIndexerHeight);
            txdb.Close();
            if (nIndexerHeight != nBestHeight)
            {
                printf("Indexer height %d != best height %d, triggering reindex\n", nIndexerHeight, nBestHeight);
                fNeedReindex = true;
            }
        }
        if (fNeedReindex)
        {
            printf("Building UTXO and address indexes...\n");
            fIndexerRebuilding = true;
            if (!ReindexUTXOs())
            {
                fprintf(stderr, "Error building UTXO indexes\n");
                return false;
            }
            printf("UTXO indexes built successfully\n");
        }
        else
        {
            printf("Indexer up to date at height %d\n", nBestHeight);
        }
    }

    bool fFirstRun;
    if (!LoadWallet(fFirstRun))
    {
        fprintf(stderr, "Error loading wallet\n");
        return false;
    }

    // Initialize nLimitProcessors to CPU count - 1 if not set (leave 1 core for system)
    if (nLimitProcessors == 1 && fFirstRun)
    {
#if wxUSE_GUI
        int nProcessors = wxThread::GetCPUCount();
#elif defined(_WIN32) || defined(__MINGW32__)
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        int nProcessors = sysinfo.dwNumberOfProcessors;
#else
        int nProcessors = sysconf(_SC_NPROCESSORS_ONLN);
#endif
        if (nProcessors > 1)
        {
            nLimitProcessors = nProcessors - 1;
            CWalletDB().WriteSetting("nLimitProcessors", nLimitProcessors);
            printf("Initialized nLimitProcessors to %d (leaving 1 core for system)\n", nLimitProcessors);
        }
    }

    {
        CTxDB txdb("r");
        int64 nAtomMinted = 0;
        bool fHasAtomData = txdb.ReadAtomTotalMinted(nAtomMinted);
        txdb.Close();
        if (!fHasAtomData && nBestHeight > 0)
        {
            printf("[ATOM] No ATOM index found in database, running automatic ATOM rescan...\n");
            if (!RescanAtom())
            {
                fprintf(stderr, "Error building ATOM indexes\n");
                return false;
            }
            printf("[ATOM] Automatic ATOM rescan completed\n");
        }
    }

    printf("Done loading\n");

    if (GetBoolArg("-printblockindex") || GetBoolArg("-printblocktree"))
    {
        PrintBlockTree();
        return false;
    }

    if (mapArgs.count("-printblock"))
    {
        string strMatch = mapArgs["-printblock"];
        int nFound = 0;
        for (auto mi = mapBlockIndex.begin(); mi != mapBlockIndex.end(); ++mi)
        {
            uint256 hash = (*mi).first;
            if (strncmp(hash.ToString().c_str(), strMatch.c_str(), strMatch.size()) == 0)
            {
                CBlockIndex* pindex = (*mi).second;
                CBlock block;
                block.ReadFromDisk(pindex);
                block.BuildMerkleTree();
                block.print();
                printf("\n");
                nFound++;
            }
        }
        if (nFound == 0)
            printf("No blocks matching %s were found\n", strMatch.c_str());
        return false;
    }

    if (mapArgs.count("-proxy"))
    {
        fUseProxy = true;
        addrProxy.SetAddress(mapArgs["-proxy"]);
        if (!addrProxy.IsValid())
        {
            fprintf(stderr, "Invalid -proxy address\n");
            return false;
        }
    }

    if (mapArgs.count("-paytxfee"))
    {
        if (!ParseMoney(mapArgs["-paytxfee"], nTransactionFee))
        {
            fprintf(stderr, "Invalid -paytxfee amount\n");
            return false;
        }
        if (nTransactionFee > 0.25 * COIN)
            fprintf(stderr, "Warning: -paytxfee is set very high\n");
    }

    if (!CheckDiskSpace())
        return false;

    RandAddSeedPerfmon();

    if (!CreateThread(StartNode, NULL))
    {
        fprintf(stderr, "Error: CreateThread(StartNode) failed\n");
        return false;
    }

    if (GetBoolArg("-server") || fDaemon)
        CreateThread(ThreadRPCServer, NULL);

    return true;
}

int main(int argc, char* argv[])
{
    bool fRet = false;
    try
    {
        fRet = AppInit(argc, argv);
    }
    catch (std::exception& e)
    {
        PrintException(&e, "AppInit()");
    }
    catch (...)
    {
        PrintException(NULL, "AppInit()");
    }

    if (fRet)
    {
        while (!fShutdown)
            Sleep(5000);
    }

    Shutdown(NULL);
    return 0;
}

#endif
