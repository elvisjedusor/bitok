// Copyright (c) 2009-2010 Satoshi Nakamoto
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#include "headers.h"
#include <wx/choicdlg.h>
#include <wx/progdlg.h>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

int g_isPainting = 0;

#if wxUSE_GUI

static bool RescanProgressCallback(wxProgressDialog* dlg, int nScanned, int nTotal, int nFound)
{
    int pct = (nTotal > 0) ? (nScanned * 100 / nTotal) : 0;
    if (pct > 100) pct = 100;
    dlg->Update(pct, wxString::Format(_("Scanned %d / %d blocks (%d found)"), nScanned, nTotal, nFound));
    wxSafeYield(dlg);
    return true;
}

static bool AtomRescanProgressCallback(wxProgressDialog* dlg, int nScanned, int nTotal)
{
    int pct = (nTotal > 0) ? (nScanned * 100 / nTotal) : 0;
    if (pct > 100) pct = 100;
    dlg->Update(pct, wxString::Format(_("ATOM rescan: %d / %d blocks"), nScanned, nTotal));
    wxSafeYield(dlg);
    return true;
}

#if wxCHECK_VERSION(3, 0, 0)
wxDEFINE_EVENT(wxEVT_UITHREADCALL, wxCommandEvent);
#else
DEFINE_EVENT_TYPE(wxEVT_UITHREADCALL)
#endif

CMainFrame* pframeMain = NULL;
CMyTaskBarIcon* ptaskbaricon = NULL;
bool fClosedToTray = false;









//////////////////////////////////////////////////////////////////////////////
//
// Util
//

void HandleCtrlA(wxKeyEvent& event)
{
    // Ctrl-a select all
    event.Skip();
    wxTextCtrl* textCtrl = (wxTextCtrl*)event.GetEventObject();
    if (event.GetModifiers() == wxMOD_CONTROL && event.GetKeyCode() == 'A')
        textCtrl->SetSelection(-1, -1);
}

bool Is24HourTime()
{
    //char pszHourFormat[256];
    //pszHourFormat[0] = '\0';
    //GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_ITIME, pszHourFormat, 256);
    //return (pszHourFormat[0] != '0');
    return true;
}

string DateStr(int64 nTime)
{
    // Can only be used safely here in the UI
    return (string)wxDateTime((time_t)nTime).FormatDate();
}

string DateTimeStr(int64 nTime)
{
    // Can only be used safely here in the UI
    wxDateTime datetime((time_t)nTime);
    if (Is24HourTime())
        return (string)datetime.Format("%x %H:%M");
    else
        return (string)datetime.Format("%x ") + itostr((datetime.GetHour() + 11) % 12 + 1) + (string)datetime.Format(":%M %p");
}

wxString GetItemText(wxListCtrl* listCtrl, int nIndex, int nColumn)
{
    // Helper to simplify access to listctrl
    wxListItem item;
    item.m_itemId = nIndex;
    item.m_col = nColumn;
    item.m_mask = wxLIST_MASK_TEXT;
    if (!listCtrl->GetItem(item))
        return "";
    return item.GetText();
}

int InsertLine(wxListCtrl* listCtrl, const wxString& str0, const wxString& str1)
{
    int nIndex = listCtrl->InsertItem(listCtrl->GetItemCount(), str0);
    listCtrl->SetItem(nIndex, 1, str1);
    return nIndex;
}

int InsertLine(wxListCtrl* listCtrl, const wxString& str0, const wxString& str1, const wxString& str2, const wxString& str3, const wxString& str4)
{
    int nIndex = listCtrl->InsertItem(listCtrl->GetItemCount(), str0);
    listCtrl->SetItem(nIndex, 1, str1);
    listCtrl->SetItem(nIndex, 2, str2);
    listCtrl->SetItem(nIndex, 3, str3);
    listCtrl->SetItem(nIndex, 4, str4);
    return nIndex;
}

int InsertLine(wxListCtrl* listCtrl, void* pdata, const wxString& str0, const wxString& str1, const wxString& str2, const wxString& str3, const wxString& str4)
{
    int nIndex = listCtrl->InsertItem(listCtrl->GetItemCount(), str0);
    listCtrl->SetItemPtrData(nIndex, (wxUIntPtr)pdata);
    listCtrl->SetItem(nIndex, 1, str1);
    listCtrl->SetItem(nIndex, 2, str2);
    listCtrl->SetItem(nIndex, 3, str3);
    listCtrl->SetItem(nIndex, 4, str4);
    return nIndex;
}

void SetSelection(wxListCtrl* listCtrl, int nIndex)
{
    int nSize = listCtrl->GetItemCount();
    long nState = (wxLIST_STATE_SELECTED|wxLIST_STATE_FOCUSED);
    for (int i = 0; i < nSize; i++)
        listCtrl->SetItemState(i, (i == nIndex ? nState : 0), nState);
}

int GetSelection(wxListCtrl* listCtrl)
{
    int nSize = listCtrl->GetItemCount();
    for (int i = 0; i < nSize; i++)
        if (listCtrl->GetItemState(i, wxLIST_STATE_FOCUSED))
            return i;
    return -1;
}

string HtmlEscape(const char* psz, bool fMultiLine=false)
{
    int len = 0;
    for (const char* p = psz; *p; p++)
    {
             if (*p == '<') len += 4;
        else if (*p == '>') len += 4;
        else if (*p == '&') len += 5;
        else if (*p == '"') len += 6;
        else if (*p == ' ' && p > psz && p[-1] == ' ' && p[1] == ' ') len += 6;
        else if (*p == '\n' && fMultiLine) len += 5;
        else
            len++;
    }
    string str;
    str.reserve(len);
    for (const char* p = psz; *p; p++)
    {
             if (*p == '<') str += "&lt;";
        else if (*p == '>') str += "&gt;";
        else if (*p == '&') str += "&amp;";
        else if (*p == '"') str += "&quot;";
        else if (*p == ' ' && p > psz && p[-1] == ' ' && p[1] == ' ') str += "&nbsp;";
        else if (*p == '\n' && fMultiLine) str += "<br>\n";
        else
            str += *p;
    }
    return str;
}

string HtmlEscape(const string& str, bool fMultiLine=false)
{
    return HtmlEscape(str.c_str(), fMultiLine);
}

void CalledMessageBox(const string& message, const string& caption, int style, wxWindow* parent, int x, int y, int* pnRet, bool* pfDone)
{
    *pnRet = wxMessageBox(message, caption, style, parent, x, y);
    *pfDone = true;
}

int ThreadSafeMessageBox(const string& message, const string& caption, int style, wxWindow* parent, int x, int y)
{
#ifdef __WXMSW__
    return wxMessageBox(message, caption, style, parent, x, y);
#else
    if (wxThread::IsMain() || fDaemon)
    {
        return wxMessageBox(message, caption, style, parent, x, y);
    }
    else
    {
        int nRet = 0;
        bool fDone = false;
        UIThreadCall(bind(CalledMessageBox, message, caption, style, parent, x, y, &nRet, &fDone));
        while (!fDone)
            Sleep(100);
        return nRet;
    }
#endif
}

bool ThreadSafeAskFee(int64 nFeeRequired, const string& strCaption, wxWindow* parent)
{
    if (nFeeRequired == 0 || fDaemon)
        return true;
    string strMessage = strprintf(
        _("This transaction requires a fee of %s.  "
          "The fee helps support the network and prevents spam.  "
          "Do you want to pay the fee?").mb_str(),
        FormatMoney(nFeeRequired).c_str());
    return (ThreadSafeMessageBox(strMessage, strCaption, wxYES_NO, parent) == wxYES);
}

void CalledSetStatusBar(const string& strText, int nField)
{
    if (pframeMain && pframeMain->m_statusBar)
        pframeMain->m_statusBar->SetStatusText(strText, nField);
}

void SetDefaultReceivingAddress(const string& strAddress)
{
    // Update main window address and database
    if (pframeMain == NULL)
        return;
    if (strAddress != pframeMain->m_textCtrlAddress->GetValue())
    {
        uint160 hash160;
        if (!AddressToHash160(strAddress, hash160))
            return;
        if (!mapPubKeys.count(hash160))
            return;
        CWalletDB().WriteDefaultKey(mapPubKeys[hash160]);
        pframeMain->m_textCtrlAddress->SetValue(strAddress);
    }
}










//////////////////////////////////////////////////////////////////////////////
//
// CMainFrame
//

CMainFrame::CMainFrame(wxWindow* parent) : CMainFrameBase(parent)
{
    Connect(wxEVT_UITHREADCALL, wxCommandEventHandler(CMainFrame::OnUIThreadCall), NULL, this);

    // Set initially selected page
    wxNotebookEvent event;
    event.SetSelection(0);
    OnNotebookPageChanged(event);
    m_notebook->ChangeSelection(0);

    // Init
    fRefreshListCtrl = false;
    fRefreshListCtrlRunning = false;
    fOnSetFocusAddress = false;
    fRefresh = false;
    m_choiceFilter->SetSelection(0);
    double dResize = 1.0;
#ifdef __WXMSW__
    SetIcon(wxICON(bitcoin));
#else
    SetIcon(bitcoin80_xpm);
    // Use system theme colors
    wxColour bgColour = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    wxColour fgColour = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
    wxColour panelBg = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE);
    wxColour textBg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    wxColour textFg = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);

    SetBackgroundColour(panelBg);
    SetForegroundColour(fgColour);
    if (m_toolBar) {
        m_toolBar->SetBackgroundColour(panelBg);
        m_toolBar->SetForegroundColour(fgColour);
    }
    if (m_notebook) {
        m_notebook->SetBackgroundColour(panelBg);
        m_notebook->SetForegroundColour(fgColour);
    }
    if (m_panel9) {
        m_panel9->SetBackgroundColour(panelBg);
        m_panel9->SetForegroundColour(fgColour);
    }
    if (m_panel92) {
        m_panel92->SetBackgroundColour(panelBg);
        m_panel92->SetForegroundColour(fgColour);
    }
    if (m_panel93) {
        m_panel93->SetBackgroundColour(panelBg);
        m_panel93->SetForegroundColour(fgColour);
    }
    if (m_staticText41) {
        m_staticText41->SetBackgroundColour(panelBg);
        m_staticText41->SetForegroundColour(fgColour);
    }
    wxFont fontTmp = m_staticText41->GetFont();
    fontTmp.SetFamily(wxFONTFAMILY_TELETYPE);
    m_staticTextBalance->SetFont(fontTmp);
    m_staticTextBalance->SetSize(140, 17);
    m_staticTextBalance->SetBackgroundColour(panelBg);
    m_staticTextBalance->SetForegroundColour(fgColour);
    if (m_textCtrlAddress) {
        m_textCtrlAddress->SetBackgroundColour(textBg);
        m_textCtrlAddress->SetForegroundColour(textFg);
    }
    if (m_choiceFilter) {
        m_choiceFilter->SetBackgroundColour(textBg);
        m_choiceFilter->SetForegroundColour(textFg);
    }
    // resize to fit ubuntu's huge default font
    dResize = 1.22;
    SetSize(dResize * GetSize().GetWidth(), 1.15 * GetSize().GetHeight());
#endif
    m_staticTextBalance->SetLabel(FormatMoney(GetBalance()) + "  ");
    m_listCtrl->SetFocus();
    ptaskbaricon = new CMyTaskBarIcon();
#ifdef __WXMAC__
    // On macOS, wxWidgets automatically moves certain menu items to the application menu:
    // - wxID_EXIT (File > Exit) becomes "Quit" in app menu
    // - wxID_PREFERENCES (Settings > Options) becomes "Preferences" in app menu
    // - wxID_ABOUT (Help > About) becomes "About" in app menu
    //
    // We keep all menus visible because the Settings menu contains important items that
    // don't auto-relocate: "Generate Coins" and "Your Receiving Addresses"
    // Users need clear access to these mining controls.
    //
    // Note: Some items will appear in both the app menu and original menu, which is
    // acceptable and follows standard macOS patterns for cross-platform apps.
#endif

    // Init column headers
    int nDateWidth = DateTimeStr(1229413914).size() * 6 + 8;
    if (!strstr(DateTimeStr(1229413914).c_str(), "2008"))
        nDateWidth += 12;
#ifdef __WXMAC__
    nDateWidth += 5;
    dResize -= 0.01;
#endif
    wxListCtrl* pplistCtrl[] = {m_listCtrlAll, m_listCtrlSent, m_listCtrlReceived};
    foreach(wxListCtrl* p, pplistCtrl)
    {
        p->InsertColumn(0, "",               wxLIST_FORMAT_LEFT,  dResize * 0);
        p->InsertColumn(1, "",               wxLIST_FORMAT_LEFT,  dResize * 0);
        p->InsertColumn(2, _("Status"),      wxLIST_FORMAT_LEFT,  dResize * 112);
        p->InsertColumn(3, _("Date"),        wxLIST_FORMAT_LEFT,  dResize * nDateWidth);
        p->InsertColumn(4, _("Description"), wxLIST_FORMAT_LEFT,  dResize * 409 - nDateWidth);
        p->InsertColumn(5, _("Debit"),       wxLIST_FORMAT_RIGHT, dResize * 79);
        p->InsertColumn(6, _("Credit"),      wxLIST_FORMAT_RIGHT, dResize * 79);
#ifndef __WXMSW__
        p->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX));
        p->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOXTEXT));
#endif
    }

    // Init status bar with 5 fields: each taking 20% of width
    int pnWidths[5] = { -20, -20, -20, -20, -20 };
    m_statusBar->SetFieldsCount(5, pnWidths);

    // Increase status bar height for better vertical padding
    m_statusBar->SetMinHeight(32);

#ifndef __WXMSW__
    if (m_statusBar) {
        m_statusBar->SetBackgroundColour(panelBg);
        m_statusBar->SetForegroundColour(fgColour);
    }
#endif

    // Style status bar fields with raised appearance for button-like look
    int styles[5];
    for (int i = 0; i < 5; i++) {
        styles[i] = wxSB_RAISED;
    }
    m_statusBar->SetStatusStyles(5, styles);

    // Fill your address text box
    vector<unsigned char> vchPubKey;
    if (CWalletDB("r").ReadDefaultKey(vchPubKey))
        m_textCtrlAddress->SetValue(PubKeyToAddress(vchPubKey));

    // Fill ok-address text box
    CRITICAL_BLOCK(cs_stealthAddresses)
    {
        if (!vStealthAddresses.empty())
        {
            for (unsigned int i = 0; i < vStealthAddresses.size(); i++)
            {
                if (vStealthAddresses[i].label == "default")
                {
                    m_textCtrlOkAddress->SetValue(vStealthAddresses[i].Encoded());
                    break;
                }
            }
            if (m_textCtrlOkAddress->GetValue().empty())
                m_textCtrlOkAddress->SetValue(vStealthAddresses[0].Encoded());
        }
    }

    // Fill listctrl with wallet transactions
    RefreshListCtrl();
}

CMainFrame::~CMainFrame()
{
    pframeMain = NULL;
    delete ptaskbaricon;
    ptaskbaricon = NULL;
}

void CMainFrame::OnNotebookPageChanged(wxNotebookEvent& event)
{
    event.Skip();
    nPage = event.GetSelection();
    if (nPage == ALL)
    {
        m_listCtrl = m_listCtrlAll;
        fShowGenerated = true;
        fShowSent = true;
        fShowReceived = true;
    }
    else if (nPage == SENT)
    {
        m_listCtrl = m_listCtrlSent;
        fShowGenerated = false;
        fShowSent = true;
        fShowReceived = false;
    }
    else if (nPage == RECEIVED)
    {
        m_listCtrl = m_listCtrlReceived;
        fShowGenerated = false;
        fShowSent = false;
        fShowReceived = true;
    }
    RefreshListCtrl();
    m_listCtrl->SetFocus();
}

void CMainFrame::OnClose(wxCloseEvent& event)
{
    if (fMinimizeOnClose && event.CanVeto() && !IsIconized())
    {
        // Divert close to minimize
        event.Veto();
        fClosedToTray = true;
        Iconize(true);
    }
    else
    {
        Hide();
        Shutdown(NULL);
    }
}

void CMainFrame::OnIconize(wxIconizeEvent& event)
{
    event.Skip();
    // Hide the task bar button when minimized.
    // Event is sent when the frame is minimized or restored.
    if (!event.IsIconized())
        fClosedToTray = false;
//#ifdef __WXMSW__
    // The tray icon sometimes disappears on ubuntu karmic
    // Hiding the taskbar button doesn't work cleanly on ubuntu lucid
    if (fMinimizeToTray && event.IsIconized())
        fClosedToTray = true;
    Show(!fClosedToTray);
//#endif
    ptaskbaricon->Show(fMinimizeToTray || fClosedToTray);
}

void CMainFrame::OnMouseEvents(wxMouseEvent& event)
{
    event.Skip();
    // Skip entropy collection for mousewheel events to prevent scroll stuttering
    if (event.GetEventType() == wxEVT_MOUSEWHEEL)
        return;
    RandAddSeed();
    RAND_add(&event.m_x, sizeof(event.m_x), 0.25);
    RAND_add(&event.m_y, sizeof(event.m_y), 0.25);
}

void CMainFrame::OnListColBeginDrag(wxListEvent& event)
{
    // Hidden columns not resizeable
    if (event.GetColumn() <= 1 && !fDebug)
        event.Veto();
    else
        event.Skip();
}

int CMainFrame::GetSortIndex(const string& strSort)
{
#ifdef __WXMSW__
    return 0;
#else
    // The wx generic listctrl implementation used on GTK doesn't sort,
    // so we have to do it ourselves.  Remember, we sort in reverse order.
    // In the wx generic implementation, they store the list of items
    // in a vector, so indexed lookups are fast, but inserts are slower
    // the closer they are to the top.
    int low = 0;
    int high = m_listCtrl->GetItemCount();
    while (low < high)
    {
        int mid = low + ((high - low) / 2);
        if (strSort.compare(m_listCtrl->GetItemText(mid).c_str()) >= 0)
            high = mid;
        else
            low = mid + 1;
    }
    return low;
#endif
}

void CMainFrame::InsertLine(bool fNew, int nIndex, uint256 hashKey, string strSort, const wxString& str2, const wxString& str3, const wxString& str4, const wxString& str5, const wxString& str6)
{
    strSort = " " + strSort;       // leading space to workaround wx2.9.0 ubuntu 9.10 bug
    long nData = *(long*)&hashKey; //  where first char of hidden column is displayed

    // Find item
    if (!fNew && nIndex == -1)
    {
        string strHash = " " + hashKey.ToString();
        while ((nIndex = m_listCtrl->FindItem(nIndex, nData)) != -1)
            if (GetItemText(m_listCtrl, nIndex, 1) == strHash)
                break;
    }

    // fNew is for blind insert, only use if you're sure it's new
    if (fNew || nIndex == -1)
    {
        nIndex = m_listCtrl->InsertItem(GetSortIndex(strSort), strSort);
    }
    else
    {
        // If sort key changed, must delete and reinsert to make it relocate
        if (GetItemText(m_listCtrl, nIndex, 0) != strSort)
        {
            m_listCtrl->DeleteItem(nIndex);
            nIndex = m_listCtrl->InsertItem(GetSortIndex(strSort), strSort);
        }
    }

    m_listCtrl->SetItem(nIndex, 1, " " + hashKey.ToString());
    m_listCtrl->SetItem(nIndex, 2, str2);
    m_listCtrl->SetItem(nIndex, 3, str3);
    m_listCtrl->SetItem(nIndex, 4, str4);
    m_listCtrl->SetItem(nIndex, 5, str5);
    m_listCtrl->SetItem(nIndex, 6, str6);
    m_listCtrl->SetItemData(nIndex, nData);
}

bool CMainFrame::DeleteLine(uint256 hashKey)
{
    long nData = *(long*)&hashKey;

    // Find item
    int nIndex = -1;
    string strHash = " " + hashKey.ToString();
    while ((nIndex = m_listCtrl->FindItem(nIndex, nData)) != -1)
        if (GetItemText(m_listCtrl, nIndex, 1) == strHash)
            break;

    if (nIndex != -1)
        m_listCtrl->DeleteItem(nIndex);

    return nIndex != -1;
}

string FormatTxStatus(const CWalletTx& wtx)
{
    // Status
    if (!wtx.IsFinal())
    {
        if (wtx.nLockTime < 500000000)
            return strprintf(_STR("Open for %d blocks").c_str(), nBestHeight - wtx.nLockTime);
        else
            return strprintf(_STR("Open until %s").c_str(), DateTimeStr(wtx.nLockTime).c_str());
    }
    else
    {
        int nDepth = wtx.GetDepthInMainChain();
        if (GetAdjustedTime() - wtx.nTimeReceived > 2 * 60 && wtx.GetRequestCount() == 0)
            return strprintf(_STR("%d/offline?").c_str(), nDepth);
        else if (nDepth < 6)
            return strprintf(_STR("%d/unconfirmed").c_str(), nDepth);
        else
            return strprintf(_STR("%d confirmations").c_str(), nDepth);
    }
}

string SingleLine(const string& strIn)
{
    string strOut;
    bool fOneSpace = false;
    foreach(int c, strIn)
    {
        if (isspace(c))
        {
            fOneSpace = true;
        }
        else if (c > ' ')
        {
            if (fOneSpace && !strOut.empty())
                strOut += ' ';
            strOut += c;
            fOneSpace = false;
        }
    }
    return strOut;
}

bool CMainFrame::InsertTransaction(const CWalletTx& wtx, bool fNew, int nIndex)
{
    int64 nTime = wtx.nTimeDisplayed = wtx.GetTxTime();
    int64 nCredit = wtx.GetCredit(true);
    int64 nDebit = wtx.GetDebit();
    int64 nNet = nCredit - nDebit;
    uint256 hash = wtx.GetHash();
    string strStatus = FormatTxStatus(wtx);
    map<string, string> mapValue = wtx.mapValue;
    wtx.nLinesDisplayed = 1;
    nListViewUpdated++;

    // Filter
    if (wtx.IsCoinBase())
    {
        // Don't show generated coin until confirmed by at least one block after it
        // so we don't get the user's hopes up until it looks like it's probably accepted.
        //
        // It is not an error when generated blocks are not accepted.  By design,
        // some percentage of blocks, like 10% or more, will end up not accepted.
        // This is the normal mechanism by which the network copes with latency.
        //
        // We display regular transactions right away before any confirmation
        // because they can always get into some block eventually.  Generated coins
        // are special because if their block is not accepted, they are not valid.
        // In testmode, show coinbase with depth >= 1 for immediate feedback.
        //
        int nMinDepth = fTestMode ? 1 : 2;
        if (wtx.GetDepthInMainChain() < nMinDepth)
        {
            wtx.nLinesDisplayed = 0;
            return false;
        }

        if (!fShowGenerated)
            return false;
    }

    // Find the block the tx is in
    CBlockIndex* pindex = NULL;
    auto mi = mapBlockIndex.find(wtx.hashBlock);
    if (mi != mapBlockIndex.end())
        pindex = (*mi).second;

    // Sort order, unrecorded transactions sort to the top
    string strSort = strprintf("%010d-%01d-%010u",
        (pindex ? pindex->nHeight : INT_MAX),
        (wtx.IsCoinBase() ? 1 : 0),
        wtx.nTimeReceived);

    // Insert line
    if (nNet > 0 || wtx.IsCoinBase())
    {
        //
        // Credit
        //
        string strDescription;
        if (wtx.IsCoinBase())
        {
            // Generated
            strDescription = _STR("Generated");
            if (nCredit == 0)
            {
                int64 nUnmatured = 0;
                foreach(const CTxOut& txout, wtx.vout)
                    nUnmatured += txout.GetCredit();
                if (wtx.IsInMainChain())
                {
                    strDescription = strprintf(_STR("Generated (%s matures in %d more blocks)").c_str(), FormatMoney(nUnmatured).c_str(), wtx.GetBlocksToMaturity());

                    // Check if the block was requested by anyone
                    if (GetAdjustedTime() - wtx.nTimeReceived > 2 * 60 && wtx.GetRequestCount() == 0)
                        strDescription = _STR("Generated - Warning: This block was not received by any other nodes and will probably not be accepted!");
                }
                else
                {
                    strDescription = _STR("Generated (not accepted)");
                }
            }
        }
        else if (!mapValue["from"].empty() || !mapValue["message"].empty())
        {
            // Received by IP connection
            if (!fShowReceived)
                return false;
            if (!mapValue["from"].empty())
                strDescription += _STR("From: ") + mapValue["from"];
            if (!mapValue["message"].empty())
            {
                if (!strDescription.empty())
                    strDescription += " - ";
                strDescription += mapValue["message"];
            }
        }
        else
        {
            if (!fShowReceived)
                return false;
            if (mapValue.count("stealth_address") && !mapValue["stealth_address"].empty())
            {
                strDescription = _STR("Received via ok-address");
                string strSxLabel;
                CRITICAL_BLOCK(cs_stealthAddresses)
                {
                    for (unsigned int i = 0; i < vStealthAddresses.size(); i++)
                    {
                        if (vStealthAddresses[i].Encoded() == mapValue["stealth_address"])
                        {
                            strSxLabel = vStealthAddresses[i].label;
                            break;
                        }
                    }
                }
                if (!strSxLabel.empty())
                    strDescription += " (" + strSxLabel + ")";
            }
            else
            {
                foreach(const CTxOut& txout, wtx.vout)
                {
                    if (txout.IsMine())
                    {
                        vector<unsigned char> vchPubKey;
                        if (ExtractPubKey(txout.scriptPubKey, true, vchPubKey))
                        {
                            CRITICAL_BLOCK(cs_mapAddressBook)
                            {
                                strDescription += _STR("Received with: ");
                                string strAddress = PubKeyToAddress(vchPubKey);
                                map<string, string>::iterator mi = mapAddressBook.find(strAddress);
                                if (mi != mapAddressBook.end() && !(*mi).second.empty())
                                {
                                    string strLabel = (*mi).second;
                                    strDescription += strAddress.substr(0,12) + "... ";
                                    strDescription += "(" + strLabel + ")";
                                }
                                else
                                    strDescription += strAddress;
                            }
                        }
                        break;
                    }
                }
            }
        }

        InsertLine(fNew, nIndex, hash, strSort,
                   strStatus,
                   nTime ? DateTimeStr(nTime) : "",
                   SingleLine(strDescription),
                   "",
                   FormatMoney(nNet, true));
    }
    else
    {
        bool fAllFromMe = true;
        foreach(const CTxIn& txin, wtx.vin)
            fAllFromMe = fAllFromMe && txin.IsMine();

        bool fAllToMe = true;
        foreach(const CTxOut& txout, wtx.vout)
        {
            if (txout.nValue == 0 && txout.scriptPubKey.size() > 0 && txout.scriptPubKey[0] == OP_RETURN)
                continue;
            fAllToMe = fAllToMe && txout.IsMine();
        }

        if (fAllFromMe && fAllToMe)
        {
            // Payment to self
            int64 nValue = wtx.vout[0].nValue;
            InsertLine(fNew, nIndex, hash, strSort,
                       strStatus,
                       nTime ? DateTimeStr(nTime) : "",
                       _("Payment to yourself"),
                       "",
                       "");
            /// issue: can't tell which is the payment and which is the change anymore
            //           FormatMoney(nNet - nValue, true),
            //           FormatMoney(nValue, true));
        }
        else if (fAllFromMe)
        {
            //
            // Debit
            //
            if (!fShowSent)
                return false;

            int64 nTxFee = nDebit - wtx.GetValueOut();
            wtx.nLinesDisplayed = 0;
            for (int nOut = 0; nOut < wtx.vout.size(); nOut++)
            {
                const CTxOut& txout = wtx.vout[nOut];
                if (txout.IsMine())
                    continue;

                if (txout.nValue == 0 && txout.scriptPubKey.size() > 0 && txout.scriptPubKey[0] == OP_RETURN)
                    continue;

                string strAddress;
                if (!mapValue["to"].empty())
                {
                    // Sent to IP
                    strAddress = mapValue["to"];
                }
                else
                {
                    // Sent to Bitok Address
                    uint160 hash160;
                    if (ExtractHash160(txout.scriptPubKey, hash160))
                        strAddress = Hash160ToAddress(hash160);
                }

                string strDescription;
                if (mapValue.count("stealth_address") && !mapValue["stealth_address"].empty())
                {
                    strDescription = _STR("To ok-address: ");
                    string strSxLabel;
                    CRITICAL_BLOCK(cs_stealthAddresses)
                    {
                        for (unsigned int i = 0; i < vStealthAddresses.size(); i++)
                        {
                            if (vStealthAddresses[i].Encoded() == mapValue["stealth_address"])
                            {
                                strSxLabel = vStealthAddresses[i].label;
                                break;
                            }
                        }
                    }
                    if (!strSxLabel.empty())
                        strDescription += strSxLabel;
                    else
                        strDescription += mapValue["stealth_address"].substr(0, 24) + "...";
                }
                else
                {
                    strDescription = _STR("To: ");
                    CRITICAL_BLOCK(cs_mapAddressBook)
                        if (mapAddressBook.count(strAddress) && !mapAddressBook[strAddress].empty())
                            strDescription += mapAddressBook[strAddress] + " ";
                    strDescription += strAddress;
                }
                if (!mapValue["message"].empty())
                {
                    if (!strDescription.empty())
                        strDescription += " - ";
                    strDescription += mapValue["message"];
                }

                int64 nValue = txout.nValue;
                if (nTxFee > 0)
                {
                    nValue += nTxFee;
                    nTxFee = 0;
                }

                InsertLine(fNew, nIndex, hash, strprintf("%s-%d", strSort.c_str(), nOut),
                           strStatus,
                           nTime ? DateTimeStr(nTime) : "",
                           SingleLine(strDescription),
                           FormatMoney(-nValue, true),
                           "");
                wtx.nLinesDisplayed++;
            }
        }
        else
        {
            //
            // Mixed debit transaction, can't break down payees
            //
            bool fAllMine = true;
            foreach(const CTxOut& txout, wtx.vout)
                fAllMine = fAllMine && txout.IsMine();
            foreach(const CTxIn& txin, wtx.vin)
                fAllMine = fAllMine && txin.IsMine();

            InsertLine(fNew, nIndex, hash, strSort,
                       strStatus,
                       nTime ? DateTimeStr(nTime) : "",
                       "",
                       FormatMoney(nNet, true),
                       "");
        }
    }

    return true;
}

void CMainFrame::RefreshListCtrl()
{
    fRefreshListCtrl = true;
    ::wxWakeUpIdle();
}

void CMainFrame::OnIdle(wxIdleEvent& event)
{
    if (fRefreshListCtrl)
    {
        // Collect list of wallet transactions and sort newest first
        bool fEntered = false;
        vector<pair<unsigned int, uint256> > vSorted;
        TRY_CRITICAL_BLOCK(cs_mapWallet)
        {
            printf("RefreshListCtrl starting\n");
            fEntered = true;
            fRefreshListCtrl = false;
            vWalletUpdated.clear();

            // Do the newest transactions first
            vSorted.reserve(mapWallet.size());
            for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
            {
                const CWalletTx& wtx = (*it).second;
                unsigned int nTime = UINT_MAX - wtx.GetTxTime();
                vSorted.push_back(make_pair(nTime, (*it).first));
            }
            m_listCtrl->DeleteAllItems();
        }
        if (!fEntered)
            return;

        sort(vSorted.begin(), vSorted.end());

        // Freeze list to prevent redraws during bulk insert (performance optimization)
        m_listCtrl->Freeze();

        // Fill list control
        for (int i = 0; i < vSorted.size();)
        {
            if (fShutdown)
            {
                m_listCtrl->Thaw();
                return;
            }
            bool fEntered = false;
            TRY_CRITICAL_BLOCK(cs_mapWallet)
            {
                fEntered = true;
                uint256& hash = vSorted[i++].second;
                map<uint256, CWalletTx>::iterator mi = mapWallet.find(hash);
                if (mi != mapWallet.end())
                    InsertTransaction((*mi).second, true);
            }
        }

        // Thaw to redraw all items at once
        m_listCtrl->Thaw();

        printf("RefreshListCtrl done\n");

        // Update transaction total display
        MainFrameRepaint();
    }
    else
    {
        // Check for time updates
        static int64 nLastTime;
        if (GetTime() > nLastTime + 30)
        {
            TRY_CRITICAL_BLOCK(cs_mapWallet)
            {
                nLastTime = GetTime();

                // Freeze list to prevent redraws during time updates
                m_listCtrl->Freeze();

                for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
                {
                    CWalletTx& wtx = (*it).second;
                    if (wtx.nTimeDisplayed && wtx.nTimeDisplayed != wtx.GetTxTime())
                        InsertTransaction(wtx, false);
                }

                // Thaw to redraw all updates at once
                m_listCtrl->Thaw();
            }
        }
    }
}

void CMainFrame::RefreshStatusColumn()
{
    static int nLastTop;
    static CBlockIndex* pindexLastBest;
    static unsigned int nLastRefreshed;

    int nTop = max((int)m_listCtrl->GetTopItem(), 0);
    if (nTop == nLastTop && pindexLastBest == pindexBest)
        return;

    TRY_CRITICAL_BLOCK(cs_mapWallet)
    {
        int nStart = nTop;
        int nEnd = min(nStart + 100, m_listCtrl->GetItemCount());

        if (pindexLastBest == pindexBest && nLastRefreshed == nListViewUpdated)
        {
            // If no updates, only need to do the part that moved onto the screen
            if (nStart >= nLastTop && nStart < nLastTop + 100)
                nStart = nLastTop + 100;
            if (nEnd >= nLastTop && nEnd < nLastTop + 100)
                nEnd = nLastTop;
        }
        nLastTop = nTop;
        pindexLastBest = pindexBest;
        nLastRefreshed = nListViewUpdated;

        // Freeze list to prevent redraws during updates (smooth scrolling)
        m_listCtrl->Freeze();

        for (int nIndex = nStart; nIndex < min(nEnd, m_listCtrl->GetItemCount()); nIndex++)
        {
            uint256 hash((string)GetItemText(m_listCtrl, nIndex, 1));
            map<uint256, CWalletTx>::iterator mi = mapWallet.find(hash);
            if (mi == mapWallet.end())
            {
                printf("CMainFrame::RefreshStatusColumn() : tx not found in mapWallet\n");
                continue;
            }
            CWalletTx& wtx = (*mi).second;
            if (wtx.IsCoinBase() || wtx.GetTxTime() != wtx.nTimeDisplayed)
            {
                if (!InsertTransaction(wtx, false, nIndex))
                    m_listCtrl->DeleteItem(nIndex--);
            }
            else
                m_listCtrl->SetItem(nIndex, 2, FormatTxStatus(wtx));
        }

        // Thaw to redraw updated items
        m_listCtrl->Thaw();
    }
}

void CMainFrame::OnPaint(wxPaintEvent& event)
{
    event.Skip();
    if (fRefresh)
    {
        fRefresh = false;
        Refresh();
    }
}


unsigned int nNeedRepaint = 0;
unsigned int nLastRepaint = 0;
int64 nLastRepaintTime = 0;
int64 nRepaintInterval = 500;

void ThreadDelayedRepaint(void* parg)
{
    while (!fShutdown)
    {
        if (nLastRepaint != nNeedRepaint && GetTimeMillis() - nLastRepaintTime >= nRepaintInterval)
        {
            nLastRepaint = nNeedRepaint;
            if (pframeMain)
            {
                pframeMain->fRefresh = true;
                pframeMain->Refresh();
            }
        }
        Sleep(nRepaintInterval);
    }
}

void MainFrameRepaint()
{
    // This is called by network code that shouldn't access pframeMain
    // directly because it could still be running after the UI is closed.
    if (pframeMain)
    {
        // Don't repaint too often
        static int64 nLastRepaintRequest;
        if (GetTimeMillis() - nLastRepaintRequest < 100)
        {
            nNeedRepaint++;
            return;
        }
        nLastRepaintRequest = GetTimeMillis();

        printf("MainFrameRepaint\n");
        pframeMain->fRefresh = true;
        pframeMain->Refresh();
    }
}

void CMainFrame::OnPaintListCtrl(wxPaintEvent& event)
{
    // Skip lets the listctrl do the paint, we're just hooking the message
    event.Skip();

    //
    // Throttle expensive updates to prevent lag during scrolling
    //
    static int nTransactionCount;
    bool fPaintedBalance = false;
    if (GetTimeMillis() - nLastRepaintTime >= nRepaintInterval)
    {
        nLastRepaint = nNeedRepaint;
        nLastRepaintTime = GetTimeMillis();

        // Update taskbar tooltip
        if (ptaskbaricon)
            ptaskbaricon->UpdateTooltip();

        // Update listctrl contents
        if (!vWalletUpdated.empty())
        {
            TRY_CRITICAL_BLOCK(cs_mapWallet)
            {
                string strTop;
                if (m_listCtrl->GetItemCount())
                    strTop = (string)m_listCtrl->GetItemText(0);

                // Freeze list to prevent redraws during batch updates
                m_listCtrl->Freeze();

                foreach(uint256 hash, vWalletUpdated)
                {
                    map<uint256, CWalletTx>::iterator mi = mapWallet.find(hash);
                    if (mi != mapWallet.end())
                        InsertTransaction((*mi).second, false);
                }
                vWalletUpdated.clear();

                // Thaw to redraw all updates at once
                m_listCtrl->Thaw();

                if (m_listCtrl->GetItemCount() && strTop != (string)m_listCtrl->GetItemText(0))
                    m_listCtrl->ScrollList(0, INT_MIN/2);
            }
        }

        // Balance total
        TRY_CRITICAL_BLOCK(cs_mapWallet)
        {
            fPaintedBalance = true;
            m_staticTextBalance->SetLabel(FormatMoney(GetBalance()) + "  ");

            // Count hidden and multi-line transactions
            nTransactionCount = 0;
            for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
            {
                CWalletTx& wtx = (*it).second;
                nTransactionCount += wtx.nLinesDisplayed;
            }
        }

        // Update status column of visible items only
        RefreshStatusColumn();

        // Update status bar - 5 separate fields
        // Field 0: Hashrate (set by mining thread, clear when not mining)
        if (!fGenerateBitcoins)
            m_statusBar->SetStatusText("", 0);

        // Field 1: Mining status (with leading space for padding)
        string strMining = "";
        if (fGenerateBitcoins)
        {
            int nMinerThreads = vnThreadsRunning[3];
            if (nMinerThreads > 0)
                strMining = strprintf(_STR(" Mining (%d)").c_str(), nMinerThreads);
            else
                strMining = _STR(" Mining");
        }
        if (fGenerateBitcoins && vNodes.empty())
            strMining = _STR(" (not connected)");
        m_statusBar->SetStatusText(strMining, 1);

        // Field 2: Connections
        int nNumConnections = 0;
        int nHighestPeerHeight = 0;
        CRITICAL_BLOCK(cs_vNodes)
        {
            nNumConnections = vNodes.size();
            foreach(CNode* pnode, vNodes)
                if (pnode->nStartingHeight > nHighestPeerHeight)
                    nHighestPeerHeight = pnode->nStartingHeight;
        }
        string strConnections = strprintf(_STR(" %d connection%s").c_str(), nNumConnections, nNumConnections == 1 ? "" : "s");
        m_statusBar->SetStatusText(strConnections, 2);

        // Field 3: Blocks
        string strBlocks;
        if (nHighestPeerHeight > 0 && nBestHeight < nHighestPeerHeight - 5)
            strBlocks = strprintf(_STR(" %d / %d blocks").c_str(), nBestHeight + 1, nHighestPeerHeight);
        else
            strBlocks = strprintf(_STR(" %d blocks").c_str(), nBestHeight + 1);
        m_statusBar->SetStatusText(strBlocks, 3);

        // Field 4: Transactions (only show if > 0)
        if (nTransactionCount > 0)
        {
            string strTransactions = strprintf(_STR(" %d transaction%s").c_str(), nTransactionCount, nTransactionCount == 1 ? "" : "s");
            m_statusBar->SetStatusText(strTransactions, 4);
        }
        else
        {
            m_statusBar->SetStatusText("", 4);
        }

        if (fDebug && GetTime() - nThreadSocketHandlerHeartbeat > 60)
            m_statusBar->SetStatusText("ERROR: ThreadSocketHandler has stopped", 0);

        // Update receiving address
        string strDefaultAddress = PubKeyToAddress(vchDefaultKey);
        if (m_textCtrlAddress->GetValue() != strDefaultAddress)
            m_textCtrlAddress->SetValue(strDefaultAddress);
    }
    if (!vWalletUpdated.empty() || !fPaintedBalance)
        nNeedRepaint++;
}


void UIThreadCall(boost::function0<void> fn)
{
    // Call this with a function object created with bind.
    // bind needs all parameters to match the function's expected types
    // and all default parameters specified.  Some examples:
    //  UIThreadCall(bind(wxBell));
    //  UIThreadCall(bind(wxMessageBox, wxT("Message"), wxT("Title"), wxOK, (wxWindow*)NULL, -1, -1));
    //  UIThreadCall(bind(&CMainFrame::OnMenuHelpAbout, pframeMain, event));
    if (pframeMain)
    {
        wxCommandEvent event(wxEVT_UITHREADCALL);
        event.SetClientData((void*)new boost::function0<void>(fn));
        pframeMain->GetEventHandler()->AddPendingEvent(event);
    }
}

void CMainFrame::OnUIThreadCall(wxCommandEvent& event)
{
    boost::function0<void>* pfn = (boost::function0<void>*)event.GetClientData();
    (*pfn)();
    delete pfn;
}

void CMainFrame::OnMenuFileExit(wxCommandEvent& event)
{
    // File->Exit
    Close(true);
}

void CMainFrame::OnMenuOptionsGenerate(wxCommandEvent& event)
{
    // Options->Generate Coins
    GenerateBitcoins(event.IsChecked());
}

void CMainFrame::OnUpdateUIOptionsGenerate(wxUpdateUIEvent& event)
{
    event.Check(fGenerateBitcoins);
}

void CMainFrame::OnMenuOptionsChangeYourAddress(wxCommandEvent& event)
{
    // Options->Your Receiving Addresses
    CAddressBookDialog dialog(this, "", CAddressBookDialog::RECEIVING, false);
    if (!dialog.ShowModal())
        return;
}

void CMainFrame::OnMenuOptionsOptions(wxCommandEvent& event)
{
    // Options->Options
    COptionsDialog dialog(this);
    dialog.ShowModal();
}

void CMainFrame::OnMenuHelpAbout(wxCommandEvent& event)
{
    // Help->About
    CAboutDialog dialog(this);
    dialog.ShowModal();
}

void CMainFrame::OnButtonSend(wxCommandEvent& event)
{
    // Toolbar: Send
    CSendDialog dialog(this);
    dialog.ShowModal();
}

void CMainFrame::OnButtonAddressBook(wxCommandEvent& event)
{
    // Toolbar: Address Book
    CAddressBookDialog dialogAddr(this, "", CAddressBookDialog::SENDING, false);
    if (dialogAddr.ShowModal() == 2)
    {
        // Send
        CSendDialog dialogSend(this, dialogAddr.GetSelectedAddress());
        dialogSend.ShowModal();
    }
}

void CMainFrame::OnButtonGenerateCoins(wxCommandEvent& event)
{
    // Toolbar: Generate Coins (same as menu)
    GenerateBitcoins(event.IsChecked());
}

void CMainFrame::OnUpdateUIButtonGenerateCoins(wxUpdateUIEvent& event)
{
    event.Check(fGenerateBitcoins);
}

void CMainFrame::OnButtonSettings(wxCommandEvent& event)
{
    // Toolbar: Settings (same as menu Options->Options)
    COptionsDialog dialog(this);
    dialog.ShowModal();
}

void CMainFrame::OnSetFocusAddress(wxFocusEvent& event)
{
    // Automatically select-all when entering window
    event.Skip();
    m_textCtrlAddress->SetSelection(-1, -1);
    fOnSetFocusAddress = true;
}

void CMainFrame::OnMouseEventsAddress(wxMouseEvent& event)
{
    event.Skip();
    if (fOnSetFocusAddress)
        m_textCtrlAddress->SetSelection(-1, -1);
    fOnSetFocusAddress = false;
}

static bool CreateNewOkAddress(wxWindow* parent, const string& strLabel)
{
    CStealthAddress sxAddr;
    CKey scanKey, spendKey;
    if (!GenerateStealthAddress(sxAddr, scanKey, spendKey))
    {
        wxMessageBox(_("Failed to generate ok-address."),
            _("New ok-Address"), wxOK | wxICON_ERROR);
        return false;
    }

    sxAddr.label = strLabel;

    CWalletDB walletdb;
    if (!walletdb.WriteStealthAddress(sxAddr) ||
        !walletdb.WriteStealthScanKey(sxAddr.scan_pubkey, scanKey.GetPrivKey()) ||
        !walletdb.WriteStealthSpendKey(sxAddr.spend_pubkey, spendKey.GetPrivKey()))
    {
        wxMessageBox(_("Failed to save ok-address to wallet."),
            _("New ok-Address"), wxOK | wxICON_ERROR);
        return false;
    }

    CRITICAL_BLOCK(cs_stealthAddresses)
    {
        vStealthAddresses.push_back(sxAddr);
    }

    if (pframeMain)
        pframeMain->SetOkAddressIfEmpty(sxAddr.Encoded());

    return true;
}

static bool ImportOkAddressSecret(wxWindow* parent, const string& strSecret, const string& strLabel)
{
    vector<unsigned char> vchScanSecret, vchSpendSecret;
    if (!DecodeStealthSecret(strSecret, vchScanSecret, vchSpendSecret))
    {
        wxMessageBox(_("Invalid ok-address secret format."),
            _("Import ok-Address"), wxOK | wxICON_ERROR);
        return false;
    }

    CKey scanKey;
    if (!scanKey.SetSecret(vchScanSecret))
    {
        wxMessageBox(_("Invalid scan secret key."),
            _("Import ok-Address"), wxOK | wxICON_ERROR);
        return false;
    }

    CKey spendKey;
    if (!spendKey.SetSecret(vchSpendSecret))
    {
        wxMessageBox(_("Invalid spend secret key."),
            _("Import ok-Address"), wxOK | wxICON_ERROR);
        return false;
    }

    CStealthAddress sxAddr;
    sxAddr.scan_pubkey = scanKey.GetCompressedPubKey();
    sxAddr.spend_pubkey = spendKey.GetCompressedPubKey();
    sxAddr.label = strLabel;

    if (!sxAddr.IsValid())
    {
        wxMessageBox(_("Derived ok-address is invalid."),
            _("Import ok-Address"), wxOK | wxICON_ERROR);
        return false;
    }

    bool fExists = false;
    CRITICAL_BLOCK(cs_stealthAddresses)
    {
        foreach(const CStealthAddress& sx, vStealthAddresses)
        {
            if (sx.scan_pubkey == sxAddr.scan_pubkey && sx.spend_pubkey == sxAddr.spend_pubkey)
            {
                fExists = true;
                break;
            }
        }
    }

    if (fExists)
    {
        wxMessageBox(_("This ok-address already exists in your wallet."),
            _("Import ok-Address"), wxOK | wxICON_INFORMATION);
        return false;
    }

    CWalletDB walletdb;
    if (!walletdb.WriteStealthAddress(sxAddr) ||
        !walletdb.WriteStealthScanKey(sxAddr.scan_pubkey, scanKey.GetPrivKey()) ||
        !walletdb.WriteStealthSpendKey(sxAddr.spend_pubkey, spendKey.GetPrivKey()))
    {
        wxMessageBox(_("Failed to save ok-address to wallet."),
            _("Import ok-Address"), wxOK | wxICON_ERROR);
        return false;
    }

    CRITICAL_BLOCK(cs_stealthAddresses)
    {
        vStealthAddresses.push_back(sxAddr);
    }

    if (pframeMain)
        pframeMain->SetOkAddressIfEmpty(sxAddr.Encoded());

    printf("[STEALTH] Imported stealth address %s\n", sxAddr.Encoded().substr(0, 16).c_str());

    {
        wxProgressDialog progressDlg(
            _("Rescanning Blockchain"),
            _("Scanning for stealth payments..."),
            100,
            parent,
            wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_SMOOTH | wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME);
        set<uint160> setOnChainAddresses;
        int nFound = ScanWalletTransactions(pindexGenesisBlock,
            bind(RescanProgressCallback, &progressDlg, _1, _2, _3),
            &setOnChainAddresses);
        progressDlg.Update(100);
        if (nFound > 0)
            wxMessageBox(strprintf(_("Rescan complete. Found %d transaction(s).").mb_str(), nFound),
                _("Import ok-Address"), wxOK | wxICON_INFORMATION);
    }

    {
        wxProgressDialog atomDlg(
            _("Rebuilding ATOM Index"),
            _("Rescanning ATOM balances and transactions..."),
            100,
            parent,
            wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_SMOOTH | wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME);
        RescanAtom(bind(AtomRescanProgressCallback, &atomDlg, _1, _2));
        atomDlg.Update(100);
    }

    return true;
}

void CMainFrame::OnButtonNew(wxCommandEvent& event)
{
    wxArrayString choices;
    choices.Add(_("Create New Bitok Address"));
    choices.Add(_("Create New ok-Address"));
    choices.Add(_("Import Private Key (Bitok Address)"));
    choices.Add(_("Import ok-Address Secret"));
    wxSingleChoiceDialog choiceDlg(this,
        _("What would you like to do?"),
        _("New Address"),
        choices);
    choiceDlg.SetSelection(0);
    if (choiceDlg.ShowModal() != wxID_OK)
        return;

    int nChoice = choiceDlg.GetSelection();

    if (nChoice == 0)
    {
        CGetTextFromUserDialog dialog(this,
            _STR("New Bitok Address"),
            _STR("It's good policy to use a new address for each payment you receive.\n\nLabel"),
            "");
        if (!dialog.ShowModal())
            return;
        string strName = dialog.GetValue();
        string strAddress = PubKeyToAddress(GenerateNewKey());
        SetAddressBookName(strAddress, strName);
        SetDefaultReceivingAddress(strAddress);
    }
    else if (nChoice == 1)
    {
        CGetTextFromUserDialog dialog(this,
            _STR("New ok-Address"),
            _STR("Label"),
            "");
        if (!dialog.ShowModal())
            return;
        CreateNewOkAddress(this, dialog.GetValue());
    }
    else if (nChoice == 2)
    {
        CGetTextFromUserDialog dialog(this,
            _STR("Import Private Key"),
            _STR("Private Key (WIF format)"),
            "",
            _STR("Label (optional)"),
            "");
        if (!dialog.ShowModal())
            return;
        string strSecret = dialog.GetValue1();
        string strName = dialog.GetValue2();

        vector<unsigned char> vchWIF;
        if (!DecodeBase58Check(strSecret, vchWIF) || vchWIF.size() < 33 || vchWIF[0] != 128)
        {
            wxMessageBox(_("Invalid private key format. Please enter a valid WIF key."),
                _("Import Private Key"), wxOK | wxICON_ERROR);
            return;
        }
        vector<unsigned char> vchSecret(vchWIF.begin() + 1, vchWIF.begin() + 33);
        CKey key;
        if (!key.SetSecret(vchSecret))
        {
            wxMessageBox(_("Invalid private key."),
                _("Import Private Key"), wxOK | wxICON_ERROR);
            return;
        }
        if (!AddKey(key))
        {
            wxMessageBox(_("Error adding key to wallet."),
                _("Import Private Key"), wxOK | wxICON_ERROR);
            return;
        }
        string strAddress = PubKeyToAddress(key.GetPubKey());

        {
            printf("[WALLET] Rescanning blockchain for imported key %s\n", strAddress.c_str());
            wxProgressDialog progressDlg(
                _("Rescanning Blockchain"),
                _("Scanning for wallet transactions..."),
                100,
                this,
                wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_SMOOTH | wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME);
            int nFound = ScanWalletTransactions(pindexGenesisBlock,
                bind(RescanProgressCallback, &progressDlg, _1, _2, _3));
            progressDlg.Update(100);
            if (nFound > 0)
                wxMessageBox(strprintf(_("Rescan complete. Found %d transaction(s).").mb_str(), nFound),
                    _("Import Private Key"), wxOK | wxICON_INFORMATION);
        }

        {
            wxProgressDialog atomDlg(
                _("Rebuilding ATOM Index"),
                _("Rescanning ATOM balances and transactions..."),
                100,
                this,
                wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_SMOOTH | wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME);
            RescanAtom(bind(AtomRescanProgressCallback, &atomDlg, _1, _2));
            atomDlg.Update(100);
        }

        SetAddressBookName(strAddress, strName);
        SetDefaultReceivingAddress(strAddress);
    }
    else if (nChoice == 3)
    {
        CGetTextFromUserDialog dialog(this,
            _STR("Import ok-Address Secret"),
            _STR("ok-Address Secret (SK... format)"),
            "",
            _STR("Label (optional)"),
            "");
        if (!dialog.ShowModal())
            return;
        ImportOkAddressSecret(this, dialog.GetValue1(), dialog.GetValue2());
    }
}

void CMainFrame::OnButtonCopy(wxCommandEvent& event)
{
    // Copy address box to clipboard
    if (wxTheClipboard->Open())
    {
        wxTheClipboard->SetData(new wxTextDataObject(m_textCtrlAddress->GetValue()));
        wxTheClipboard->Close();
    }
}

void CMainFrame::OnButtonCopyOk(wxCommandEvent& event)
{
    if (wxTheClipboard->Open())
    {
        wxTheClipboard->SetData(new wxTextDataObject(m_textCtrlOkAddress->GetValue()));
        wxTheClipboard->Close();
    }
}

void CMainFrame::SetOkAddressIfEmpty(const string& strAddr)
{
    if (m_textCtrlOkAddress->GetValue().IsEmpty())
        m_textCtrlOkAddress->SetValue(strAddr);
}

void CMainFrame::OnListItemActivated(wxListEvent& event)
{
    uint256 hash((string)GetItemText(m_listCtrl, event.GetIndex(), 1));
    CWalletTx wtx;
    CRITICAL_BLOCK(cs_mapWallet)
    {
        map<uint256, CWalletTx>::iterator mi = mapWallet.find(hash);
        if (mi == mapWallet.end())
        {
            printf("CMainFrame::OnListItemActivated() : tx not found in mapWallet\n");
            return;
        }
        wtx = (*mi).second;
    }
    CTxDetailsDialog dialog(this, wtx);
    dialog.ShowModal();
    //CTxDetailsDialog* pdialog = new CTxDetailsDialog(this, wtx);
    //pdialog->Show();
}






//////////////////////////////////////////////////////////////////////////////
//
// CTxDetailsDialog
//

CTxDetailsDialog::CTxDetailsDialog(wxWindow* parent, CWalletTx wtx) : CTxDetailsDialogBase(parent)
{
#ifndef __WXMSW__
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
    SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
#endif
    CRITICAL_BLOCK(cs_mapAddressBook)
    {
        string strHTML;
        strHTML.reserve(4000);
        strHTML += "<html><font face='verdana, arial, helvetica, sans-serif'>";

        int64 nTime = wtx.GetTxTime();
        int64 nCredit = wtx.GetCredit();
        int64 nDebit = wtx.GetDebit();
        int64 nNet = nCredit - nDebit;



        strHTML += _STR("<b>Status:</b> ") + FormatTxStatus(wtx);
        int nRequests = wtx.GetRequestCount();
        if (nRequests != -1)
        {
            if (nRequests == 0)
                strHTML += _STR(", has not been successfully broadcast yet");
            else if (nRequests == 1)
                strHTML += strprintf(_STR(", broadcast through %d node").c_str(), nRequests);
            else
                strHTML += strprintf(_STR(", broadcast through %d nodes").c_str(), nRequests);
        }
        strHTML += "<br>";

        strHTML += _STR("<b>Date:</b> ") + (nTime ? DateTimeStr(nTime) : "") + "<br>";

        if (wtx.mapValue.count("stealth_address") && !wtx.mapValue["stealth_address"].empty())
        {
            strHTML += _STR("<b>ok-Address:</b> ") + HtmlEscape(wtx.mapValue["stealth_address"]) + "<br>";
        }

        //
        // From
        //
        if (wtx.IsCoinBase())
        {
            strHTML += _STR("<b>Source:</b> Generated<br>");
        }
        else if (!wtx.mapValue["from"].empty())
        {
            // Online transaction
            if (!wtx.mapValue["from"].empty())
                strHTML += _STR("<b>From:</b> ") + HtmlEscape(wtx.mapValue["from"]) + "<br>";
        }
        else
        {
            // Offline transaction
            if (nNet > 0)
            {
                // Credit
                foreach(const CTxOut& txout, wtx.vout)
                {
                    if (txout.IsMine())
                    {
                        vector<unsigned char> vchPubKey;
                        if (ExtractPubKey(txout.scriptPubKey, true, vchPubKey))
                        {
                            string strAddress = PubKeyToAddress(vchPubKey);
                            if (mapAddressBook.count(strAddress))
                            {
                                strHTML += _STR("<b>To:</b> ");
                                strHTML += HtmlEscape(strAddress);
                                if (!mapAddressBook[strAddress].empty())
                                    strHTML += _STR(" (yours, label: ") + mapAddressBook[strAddress] + ")";
                                else
                                    strHTML += _STR(" (yours)");
                                strHTML += "<br>";
                            }
                        }
                        break;
                    }
                }
            }
        }


        //
        // To
        //
        string strAddress;
        if (!wtx.mapValue["to"].empty())
        {
            // Online transaction
            strAddress = wtx.mapValue["to"];
            strHTML += _STR("<b>To:</b> ");
            if (mapAddressBook.count(strAddress) && !mapAddressBook[strAddress].empty())
                strHTML += mapAddressBook[strAddress] + " ";
            strHTML += HtmlEscape(strAddress) + "<br>";
        }


        //
        // Amount
        //
        if (wtx.IsCoinBase() && nCredit == 0)
        {
            //
            // Coinbase
            //
            int64 nUnmatured = 0;
            foreach(const CTxOut& txout, wtx.vout)
                nUnmatured += txout.GetCredit();
            strHTML += _STR("<b>Credit:</b> ");
            if (wtx.IsInMainChain())
                strHTML += strprintf(_STR("(%s matures in %d more blocks)").c_str(), FormatMoney(nUnmatured).c_str(), wtx.GetBlocksToMaturity());
            else
                strHTML += _STR("(not accepted)");
            strHTML += "<br>";
        }
        else if (nNet > 0)
        {
            //
            // Credit
            //
            strHTML += _STR("<b>Credit:</b> ") + FormatMoney(nNet) + "<br>";
        }
        else
        {
            bool fAllFromMe = true;
            foreach(const CTxIn& txin, wtx.vin)
                fAllFromMe = fAllFromMe && txin.IsMine();

            bool fAllToMe = true;
            foreach(const CTxOut& txout, wtx.vout)
            {
                if (txout.nValue == 0 && txout.scriptPubKey.size() > 0 && txout.scriptPubKey[0] == OP_RETURN)
                    continue;
                fAllToMe = fAllToMe && txout.IsMine();
            }

            if (fAllFromMe)
            {
                //
                // Debit
                //
                foreach(const CTxOut& txout, wtx.vout)
                {
                    if (txout.IsMine())
                        continue;

                    if (txout.nValue == 0 && txout.scriptPubKey.size() > 0 && txout.scriptPubKey[0] == OP_RETURN)
                        continue;

                    if (wtx.mapValue["to"].empty())
                    {
                        if (wtx.mapValue.count("stealth_address") && !wtx.mapValue["stealth_address"].empty())
                        {
                            strHTML += _STR("<b>To ok-address:</b> ") + HtmlEscape(wtx.mapValue["stealth_address"]) + "<br>";
                        }
                        else
                        {
                            uint160 hash160;
                            if (ExtractHash160(txout.scriptPubKey, hash160))
                            {
                                string strAddress = Hash160ToAddress(hash160);
                                strHTML += _STR("<b>To:</b> ");
                                if (mapAddressBook.count(strAddress) && !mapAddressBook[strAddress].empty())
                                    strHTML += mapAddressBook[strAddress] + " ";
                                strHTML += strAddress;
                                strHTML += "<br>";
                            }
                        }
                    }

                    strHTML += _STR("<b>Debit:</b> ") + FormatMoney(-txout.nValue) + "<br>";
                }

                if (fAllToMe)
                {
                    // Payment to self
                    /// issue: can't tell which is the payment and which is the change anymore
                    //int64 nValue = wtx.vout[0].nValue;
                    //strHTML += _("<b>Debit:</b> ") + FormatMoney(-nValue) + "<br>";
                    //strHTML += _("<b>Credit:</b> ") + FormatMoney(nValue) + "<br>";
                }

                int64 nTxFee = nDebit - wtx.GetValueOut();
                if (nTxFee > 0)
                    strHTML += _STR("<b>Transaction fee:</b> ") + FormatMoney(-nTxFee) + "<br>";
            }
            else
            {
                //
                // Mixed debit transaction
                //
                foreach(const CTxIn& txin, wtx.vin)
                    if (txin.IsMine())
                        strHTML += _STR("<b>Debit:</b> ") + FormatMoney(-txin.GetDebit()) + "<br>";
                foreach(const CTxOut& txout, wtx.vout)
                    if (txout.IsMine())
                        strHTML += _STR("<b>Credit:</b> ") + FormatMoney(txout.GetCredit()) + "<br>";
            }
        }

        strHTML += _STR("<b>Net amount:</b> ") + FormatMoney(nNet, true) + "<br>";


        //
        // Message
        //
        if (!wtx.mapValue["message"].empty())
            strHTML += string() + "<br><b>" + _STR("Message:") + "</b><br>" + HtmlEscape(wtx.mapValue["message"], true) + "<br>";

        if (wtx.IsCoinBase())
            strHTML += string() + "<br>" + _STR("Generated coins must wait 120 blocks before they can be spent.  When you generated this block, it was broadcast to the network to be added to the block chain.  If it fails to get into the chain, it will change to \"not accepted\" and not be spendable.  This may occasionally happen if another node generates a block within a few seconds of yours.") + "<br>";


        //
        // Debug view
        //
        if (fDebug)
        {
            strHTML += "<hr><br>debug print<br><br>";
            foreach(const CTxIn& txin, wtx.vin)
                if (txin.IsMine())
                    strHTML += "<b>Debit:</b> " + FormatMoney(-txin.GetDebit()) + "<br>";
            foreach(const CTxOut& txout, wtx.vout)
                if (txout.IsMine())
                    strHTML += "<b>Credit:</b> " + FormatMoney(txout.GetCredit()) + "<br>";

            strHTML += "<b>Inputs:</b><br>";
            CRITICAL_BLOCK(cs_mapWallet)
            {
                foreach(const CTxIn& txin, wtx.vin)
                {
                    COutPoint prevout = txin.prevout;
                    map<uint256, CWalletTx>::iterator mi = mapWallet.find(prevout.hash);
                    if (mi != mapWallet.end())
                    {
                        const CWalletTx& prev = (*mi).second;
                        if (prevout.n < prev.vout.size())
                        {
                            strHTML += HtmlEscape(prev.ToString(), true);
                            strHTML += " &nbsp;&nbsp; " + FormatTxStatus(prev) + ", ";
                            strHTML = strHTML + "IsMine=" + (prev.vout[prevout.n].IsMine() ? "true" : "false") + "<br>";
                        }
                    }
                }
            }

            strHTML += "<br><hr><br><b>Transaction:</b><br>";
            strHTML += HtmlEscape(wtx.ToString(), true);
        }



        strHTML += "</font></html>";
        string(strHTML.begin(), strHTML.end()).swap(strHTML);
        m_htmlWin->SetPage(strHTML);
        m_buttonOK->SetFocus();
    }
}

void CTxDetailsDialog::OnClose(wxCloseEvent& event)
{
    EndModal(false);
}

void CTxDetailsDialog::OnButtonOK(wxCommandEvent& event)
{
    EndModal(true);
}





//////////////////////////////////////////////////////////////////////////////
//
// COptionsDialog
//

COptionsDialog::COptionsDialog(wxWindow* parent) : COptionsDialogBase(parent)
{
#ifndef __WXMSW__
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
    SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    if (m_listBox) {
        m_listBox->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX));
        m_listBox->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOXTEXT));
    }
    if (m_textCtrlTransactionFee) {
        m_textCtrlTransactionFee->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
        m_textCtrlTransactionFee->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    }
    if (m_textCtrlProxyIP) {
        m_textCtrlProxyIP->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
        m_textCtrlProxyIP->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    }
    if (m_textCtrlProxyPort) {
        m_textCtrlProxyPort->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
        m_textCtrlProxyPort->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    }
#endif
    // Set up list box of page choices
    m_listBox->Append(_("Main"));
    //m_listBox->Append(_("Test 2"));
    m_listBox->SetSelection(0);
    SelectPage(0);
#ifndef __WXMSW__
    m_checkBoxMinimizeOnClose->SetLabel(_("&Minimize on close"));
    m_checkBoxStartOnSystemStartup->Enable(false); // not implemented yet
#endif

    // Init values
    m_textCtrlTransactionFee->SetValue(FormatMoney(nTransactionFee));

    // Detect CPU count and setup processor limit controls
    int nProcessors = wxThread::GetCPUCount();
    if (nProcessors < 1)
        nProcessors = 999;

    m_checkBoxLimitProcessors->SetValue(fLimitProcessors);
    m_spinCtrlLimitProcessors->SetRange(1, nProcessors);
    m_spinCtrlLimitProcessors->SetValue(nLimitProcessors);
    m_spinCtrlLimitProcessors->Enable(fLimitProcessors);

    // Update label to show detected CPU count
    m_staticText35->SetLabel(wxString::Format(_("processors (max: %d)"), nProcessors));
    m_checkBoxStartOnSystemStartup->SetValue(fTmpStartOnSystemStartup = GetStartOnSystemStartup());
    m_checkBoxMinimizeToTray->SetValue(fMinimizeToTray);
    m_checkBoxMinimizeOnClose->SetValue(fMinimizeOnClose);
    m_checkBoxUseProxy->SetValue(fUseProxy);
    m_textCtrlProxyIP->Enable(fUseProxy);
    m_textCtrlProxyPort->Enable(fUseProxy);
    m_staticTextProxyIP->Enable(fUseProxy);
    m_staticTextProxyPort->Enable(fUseProxy);
    m_textCtrlProxyIP->SetValue(addrProxy.ToStringIP());
    m_textCtrlProxyPort->SetValue(addrProxy.ToStringPort());

    m_buttonOK->SetFocus();
}

void COptionsDialog::SelectPage(int nPage)
{
    m_panelMain->Show(nPage == 0);
    m_panelTest2->Show(nPage == 1);

    m_scrolledWindow->Layout();
    m_scrolledWindow->SetScrollbars(0, 0, 0, 0, 0, 0);
}

void COptionsDialog::OnListBox(wxCommandEvent& event)
{
    SelectPage(event.GetSelection());
}

void COptionsDialog::OnKillFocusTransactionFee(wxFocusEvent& event)
{
    event.Skip();
    int64 nTmp = nTransactionFee;
    ParseMoney(m_textCtrlTransactionFee->GetValue(), nTmp);
    m_textCtrlTransactionFee->SetValue(FormatMoney(nTmp));
}

void COptionsDialog::OnCheckBoxLimitProcessors(wxCommandEvent& event)
{
    m_spinCtrlLimitProcessors->Enable(event.IsChecked());
}

void COptionsDialog::OnCheckBoxUseProxy(wxCommandEvent& event)
{
    m_textCtrlProxyIP->Enable(event.IsChecked());
    m_textCtrlProxyPort->Enable(event.IsChecked());
    m_staticTextProxyIP->Enable(event.IsChecked());
    m_staticTextProxyPort->Enable(event.IsChecked());
}

CAddress COptionsDialog::GetProxyAddr()
{
    // Be careful about byte order, addr.ip and addr.port are big endian
    CAddress addr(m_textCtrlProxyIP->GetValue() + ":" + m_textCtrlProxyPort->GetValue());
    if (addr.ip == INADDR_NONE)
        addr.ip = addrProxy.ip;
    int nPort = atoi(m_textCtrlProxyPort->GetValue());
    addr.port = htons(nPort);
    if (nPort <= 0 || nPort > USHRT_MAX)
        addr.port = addrProxy.port;
    return addr;
}

void COptionsDialog::OnKillFocusProxy(wxFocusEvent& event)
{
    event.Skip();
    m_textCtrlProxyIP->SetValue(GetProxyAddr().ToStringIP());
    m_textCtrlProxyPort->SetValue(GetProxyAddr().ToStringPort());
}


void COptionsDialog::OnClose(wxCloseEvent& event)
{
    EndModal(false);
}

void COptionsDialog::OnButtonOK(wxCommandEvent& event)
{
    OnButtonApply(event);
    EndModal(true);
}

void COptionsDialog::OnButtonCancel(wxCommandEvent& event)
{
    EndModal(false);
}

void COptionsDialog::OnButtonApply(wxCommandEvent& event)
{
    CWalletDB walletdb;

    int64 nPrevTransactionFee = nTransactionFee;
    if (ParseMoney(m_textCtrlTransactionFee->GetValue(), nTransactionFee) && nTransactionFee != nPrevTransactionFee)
        walletdb.WriteSetting("nTransactionFee", nTransactionFee);

    int nPrevMaxProc = (fLimitProcessors ? nLimitProcessors : INT_MAX);
    if (fLimitProcessors != m_checkBoxLimitProcessors->GetValue())
    {
        fLimitProcessors = m_checkBoxLimitProcessors->GetValue();
        walletdb.WriteSetting("fLimitProcessors", fLimitProcessors);
    }
    if (nLimitProcessors != m_spinCtrlLimitProcessors->GetValue())
    {
        nLimitProcessors = m_spinCtrlLimitProcessors->GetValue();
        walletdb.WriteSetting("nLimitProcessors", nLimitProcessors);
    }
    if (fGenerateBitcoins && (fLimitProcessors ? nLimitProcessors : INT_MAX) > nPrevMaxProc)
        GenerateBitcoins(fGenerateBitcoins);

    if (fTmpStartOnSystemStartup != m_checkBoxStartOnSystemStartup->GetValue())
    {
        fTmpStartOnSystemStartup = m_checkBoxStartOnSystemStartup->GetValue();
        SetStartOnSystemStartup(fTmpStartOnSystemStartup);
    }

    if (fMinimizeToTray != m_checkBoxMinimizeToTray->GetValue())
    {
        fMinimizeToTray = m_checkBoxMinimizeToTray->GetValue();
        walletdb.WriteSetting("fMinimizeToTray", fMinimizeToTray);
        ptaskbaricon->Show(fMinimizeToTray || fClosedToTray);
    }

    if (fMinimizeOnClose != m_checkBoxMinimizeOnClose->GetValue())
    {
        fMinimizeOnClose = m_checkBoxMinimizeOnClose->GetValue();
        walletdb.WriteSetting("fMinimizeOnClose", fMinimizeOnClose);
    }

    fUseProxy = m_checkBoxUseProxy->GetValue();
    walletdb.WriteSetting("fUseProxy", fUseProxy);

    addrProxy = GetProxyAddr();
    walletdb.WriteSetting("addrProxy", addrProxy);
}





//////////////////////////////////////////////////////////////////////////////
//
// CAboutDialog
//

CAboutDialog::CAboutDialog(wxWindow* parent) : CAboutDialogBase(parent)
{
#ifndef __WXMSW__
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
    SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    wxColour textColour = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
    m_staticText40->SetForegroundColour(textColour);
    m_staticTextVersion->SetForegroundColour(textColour);
    m_staticTextMain->SetForegroundColour(textColour);
#endif
    m_staticTextVersion->SetLabel(strprintf(_STR("version %d.%d.%d beta").c_str(), VERSION/10000, (VERSION/100)%100, VERSION%100));

    // Change (c) into UTF-8 or ANSI copyright symbol
    wxString str = m_staticTextMain->GetLabel();
#if wxUSE_UNICODE
    str.Replace("(c)", wxString::FromUTF8("\xC2\xA9"));
#else
    str.Replace("(c)", "\xA9");
#endif
    m_staticTextMain->SetLabel(str);
    m_staticTextMain->Wrap( 380 );
#ifndef __WXMSW__
    wxFont fontTmp = m_staticTextMain->GetFont();
    if (fontTmp.GetPointSize() > 8)
        fontTmp.SetPointSize(8);
    m_staticTextMain->SetFont(fontTmp);
    m_staticTextMain->Wrap( 380 );
    SetSize(GetSize().GetWidth() + 44, GetSize().GetHeight() + 10);
#endif
    GetSizer()->Layout();
}

void CAboutDialog::OnClose(wxCloseEvent& event)
{
    EndModal(false);
}

void CAboutDialog::OnButtonOK(wxCommandEvent& event)
{
    EndModal(true);
}






//////////////////////////////////////////////////////////////////////////////
//
// CSendDialog
//

CSendDialog::CSendDialog(wxWindow* parent, const wxString& strAddress) : CSendDialogBase(parent)
{
#ifndef __WXMSW__
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
    SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    if (m_textCtrlAddress) {
        m_textCtrlAddress->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
        m_textCtrlAddress->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    }
    if (m_textCtrlAmount) {
        m_textCtrlAmount->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
        m_textCtrlAmount->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    }
#endif
    m_textCtrlAddress->SetValue(strAddress);
    m_bitmapCheckMark->Show(false);
    m_textCtrlAddress->SetFocus();
#ifndef __WXMSW__
    wxFont fontTmp = m_staticTextInstructions->GetFont();
    if (fontTmp.GetPointSize() > 9);
        fontTmp.SetPointSize(9);
    m_staticTextInstructions->SetFont(fontTmp);
    SetSize(725, 300);
#endif

    wxIcon iconSend;
    iconSend.CopyFromBitmap(wxBitmap(send16noshadow_xpm));
    SetIcon(iconSend);

    wxCommandEvent event;
    OnTextAddress(event);

    m_buttonPaste->MoveAfterInTabOrder(m_buttonCancel);
    m_buttonAddress->MoveAfterInTabOrder(m_buttonPaste);
    this->Layout();
}

void CSendDialog::OnTextAddress(wxCommandEvent& event)
{
    event.Skip();
    string strAddr = (string)m_textCtrlAddress->GetValue();
    bool fBitcoinAddress = IsValidBitcoinAddress(strAddr);
    bool fStealthAddress = (strAddr.size() > 2 && strAddr.substr(0, 2) == "ok");
    m_bitmapCheckMark->Show(fBitcoinAddress || fStealthAddress);
}

void CSendDialog::OnKillFocusAmount(wxFocusEvent& event)
{
    // Reformat the amount
    event.Skip();
    if (m_textCtrlAmount->GetValue().Trim().empty())
        return;
    int64 nTmp;
    if (ParseMoney(m_textCtrlAmount->GetValue(), nTmp))
        m_textCtrlAmount->SetValue(FormatMoney(nTmp));
}

void CSendDialog::OnButtonAddressBook(wxCommandEvent& event)
{
    // Open address book
    CAddressBookDialog dialog(this, m_textCtrlAddress->GetValue(), CAddressBookDialog::SENDING, true);
    if (dialog.ShowModal())
        m_textCtrlAddress->SetValue(dialog.GetSelectedAddress());
}

void CSendDialog::OnButtonPaste(wxCommandEvent& event)
{
    // Copy clipboard to address box
    if (wxTheClipboard->Open())
    {
        if (wxTheClipboard->IsSupported(wxDF_TEXT))
        {
            wxTextDataObject data;
            wxTheClipboard->GetData(data);
            m_textCtrlAddress->SetValue(data.GetText());
        }
        wxTheClipboard->Close();
    }
}

void CSendDialog::OnButtonSend(wxCommandEvent& event)
{
    CWalletTx wtx;
    string strAddress = (string)m_textCtrlAddress->GetValue();

    int64 nValue = 0;
    if (!ParseMoney(m_textCtrlAmount->GetValue(), nValue) || nValue <= 0)
    {
        wxMessageBox(_("Error in amount  "), _("Send Coins"));
        return;
    }
    if (nValue > GetBalance())
    {
        wxMessageBox(_("Amount exceeds your balance  "), _("Send Coins"));
        return;
    }
    if (nValue + nTransactionFee > GetBalance())
    {
        wxMessageBox(_STR("Total exceeds your balance when the ") + FormatMoney(nTransactionFee) + _STR(" transaction fee is included  "), _("Send Coins"));
        return;
    }

    bool fStealthAddress = (strAddress.size() > 2 && strAddress.substr(0, 2) == "ok");

    if (fStealthAddress)
    {
        CStealthAddress sxAddr;
        if (!sxAddr.SetEncoded(strAddress))
        {
            wxMessageBox(_("Invalid ok-address  "), _("Send Coins"));
            return;
        }

        vector<unsigned char> vchEphemPub, vchDestPubKey, vchSharedSecret;
        if (!StealthEphemeral(sxAddr, vchEphemPub, vchDestPubKey, vchSharedSecret))
        {
            wxMessageBox(_("Failed to derive stealth destination  "), _("Send Coins"));
            return;
        }

        CScript scriptDestPubKey;
        scriptDestPubKey.SetBitcoinAddress(vchDestPubKey);

        vector<unsigned char> vchOpReturnData = BuildStealthOpReturn(vchEphemPub);
        CScript scriptOpReturn;
        scriptOpReturn << OP_RETURN << vchOpReturnData;

        wtx.mapValue["stealth_address"] = strAddress;

        CKey changeKey;
        int64 nFeeRequired;

        CRITICAL_BLOCK(cs_main)
        {
            if (!CreateStealthTransaction(scriptDestPubKey, scriptOpReturn, nValue, wtx, changeKey, nFeeRequired))
            {
                string strError;
                if (nValue + nFeeRequired > GetBalance())
                    strError = strprintf(_STR("This transaction requires a fee of %s").c_str(), FormatMoney(nFeeRequired).c_str());
                else
                    strError = _STR("Transaction creation failed");
                wxMessageBox(strError + "  ", _("Send Coins"));
                return;
            }
        }

        if (!ThreadSafeAskFee(nFeeRequired, _STR("Sending..."), this))
            return;

        CRITICAL_BLOCK(cs_main)
        {
            if (!CommitTransaction(wtx, changeKey))
            {
                wxMessageBox(_("The transaction was rejected.  This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here."), _("Send Coins"));
                return;
            }
        }
        wxMessageBox(_("Payment sent  "), _("Sending..."));
    }
    else
    {
        uint160 hash160;
        bool fBitcoinAddress = AddressToHash160(strAddress, hash160);

        if (fBitcoinAddress)
        {
            CScript scriptPubKey;
            scriptPubKey << OP_DUP << OP_HASH160 << hash160 << OP_EQUALVERIFY << OP_CHECKSIG;

            string strError = SendMoney(scriptPubKey, nValue, wtx, true);
            if (strError == "")
                wxMessageBox(_("Payment sent  "), _("Sending..."));
            else if (strError != "ABORTED")
                wxMessageBox(strError + "  ", _("Sending..."));
        }
        else
        {
            wxMessageBox(_("Invalid address  "), _("Send Coins"));
            return;
        }
    }

    CRITICAL_BLOCK(cs_mapAddressBook)
        if (!mapAddressBook.count(strAddress))
            SetAddressBookName(strAddress, "");

    EndModal(true);
}

void CSendDialog::OnButtonCancel(wxCommandEvent& event)
{
    // Cancel
    EndModal(false);
}






//////////////////////////////////////////////////////////////////////////////
//
// CSendingDialog
//

CSendingDialog::CSendingDialog(wxWindow* parent, const CAddress& addrIn, int64 nPriceIn, const CWalletTx& wtxIn) : CSendingDialogBase(NULL) // we have to give null so parent can't destroy us
{
#ifndef __WXMSW__
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
    SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    if (m_textCtrlStatus) {
        m_textCtrlStatus->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
        m_textCtrlStatus->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
    }
#endif
    addr = addrIn;
    nPrice = nPriceIn;
    wtx = wtxIn;
    start = wxDateTime::UNow();
    memset(pszStatus, 0, sizeof(pszStatus));
    fCanCancel = true;
    fAbort = false;
    fSuccess = false;
    fUIDone = false;
    fWorkDone = false;
#ifndef __WXMSW__
    SetSize(1.2 * GetSize().GetWidth(), 1.08 * GetSize().GetHeight());
#endif

    SetTitle(strprintf(_STR("Sending %s to %s").c_str(), FormatMoney(nPrice).c_str(), wtx.mapValue["to"].c_str()));
    m_textCtrlStatus->SetValue("");

    CreateThread(SendingDialogStartTransfer, this);
}

CSendingDialog::~CSendingDialog()
{
    printf("~CSendingDialog()\n");
}

void CSendingDialog::Close()
{
    // Last one out turn out the lights.
    // fWorkDone signals that work side is done and UI thread should call destroy.
    // fUIDone signals that UI window has closed and work thread should call destroy.
    // This allows the window to disappear and end modality when cancelled
    // without making the user wait for ConnectNode to return.  The dialog object
    // hangs around in the background until the work thread exits.
    if (IsModal())
        EndModal(fSuccess);
    else
        Show(false);
    if (fWorkDone)
        Destroy();
    else
        fUIDone = true;
}

void CSendingDialog::OnClose(wxCloseEvent& event)
{
    if (!event.CanVeto() || fWorkDone || fAbort || !fCanCancel)
    {
        EndModal(fWorkDone);
    }
    else
    {
        event.Veto();
        wxCommandEvent cmdevent;
        OnButtonCancel(cmdevent);
    }
}

void CSendingDialog::OnButtonOK(wxCommandEvent& event)
{
    if (fWorkDone)
        EndModal(true);
}

void CSendingDialog::OnButtonCancel(wxCommandEvent& event)
{
    if (fCanCancel)
        fAbort = true;
}

void CSendingDialog::OnPaint(wxPaintEvent& event)
{
    event.Skip();
    if (strlen(pszStatus) > 130)
        m_textCtrlStatus->SetValue(string("\n") + pszStatus);
    else
        m_textCtrlStatus->SetValue(string("\n\n") + pszStatus);
    m_staticTextSending->SetFocus();
    if (!fCanCancel)
        m_buttonCancel->Enable(false);
    if (fWorkDone)
    {
        m_buttonOK->Enable(true);
        m_buttonOK->SetFocus();
        m_buttonCancel->Enable(false);
    }
    if (fAbort && fCanCancel && IsShown())
    {
        strcpy(pszStatus, (const char*)_("CANCELLED").mb_str());
        m_buttonOK->Enable(true);
        m_buttonOK->SetFocus();
        m_buttonCancel->Enable(false);
        m_buttonCancel->SetLabel(_("Cancelled"));
        Close();
        wxMessageBox(_("Transfer cancelled  "), _("Sending..."), wxOK, this);
    }
}


//
// Everything from here on is not in the UI thread and must only communicate
// with the rest of the dialog through variables and calling repaint.
//

void CSendingDialog::Repaint()
{
    Refresh();
}

bool CSendingDialog::Status()
{
    if (fUIDone)
    {
        Destroy();
        return false;
    }
    if (fAbort && fCanCancel)
    {
        memset(pszStatus, 0, 10);
        strcpy(pszStatus, (const char*)_("CANCELLED").mb_str());
        Repaint();
        fWorkDone = true;
        return false;
    }
    return true;
}

bool CSendingDialog::Status(const string& str)
{
    if (!Status())
        return false;

    // This can be read by the UI thread at any time,
    // so copy in a way that can be read cleanly at all times.
    memset(pszStatus, 0, min(str.size()+1, sizeof(pszStatus)));
    strlcpy(pszStatus, str.c_str(), sizeof(pszStatus));

    Repaint();
    return true;
}

bool CSendingDialog::Error(const string& str)
{
    fCanCancel = false;
    fWorkDone = true;
    Status(_STR("Error: ") + str);
    return false;
}

void SendingDialogStartTransfer(void* parg)
{
    ((CSendingDialog*)parg)->StartTransfer();
}

void CSendingDialog::StartTransfer()
{
    // Make sure we have enough money
    if (nPrice + nTransactionFee > GetBalance())
    {
        Error(_STR("Insufficient funds"));
        return;
    }

    // We may have connected already for product details
    if (!Status(_STR("Connecting...")))
        return;
    CNode* pnode = ConnectNode(addr, 15 * 60);
    if (!pnode)
    {
        Error(_STR("Unable to connect"));
        return;
    }

    // Send order to seller, with response going to OnReply2 via event handler
    if (!Status(_STR("Requesting public key...")))
        return;
    pnode->PushRequest("checkorder", wtx, SendingDialogOnReply2, this);
}

void SendingDialogOnReply2(void* parg, CDataStream& vRecv)
{
    ((CSendingDialog*)parg)->OnReply2(vRecv);
}

void CSendingDialog::OnReply2(CDataStream& vRecv)
{
    if (!Status(_STR("Received public key...")))
        return;

    CScript scriptPubKey;
    int nRet;
    try
    {
        vRecv >> nRet;
        if (nRet > 0)
        {
            string strMessage;
            vRecv >> strMessage;
            Error(_STR("Transfer was not accepted"));
            //// todo: enlarge the window and enable a hidden white box to put seller's message
            return;
        }
        vRecv >> scriptPubKey;
    }
    catch (...)
    {
        //// what do we want to do about this?
        Error(_STR("Invalid response received"));
        return;
    }

    // Pause to give the user a chance to cancel
    while (wxDateTime::UNow() < start + wxTimeSpan(0, 0, 0, 2 * 1000))
    {
        Sleep(200);
        if (!Status())
            return;
    }

    CRITICAL_BLOCK(cs_main)
    {
        // Pay
        if (!Status(_STR("Creating transaction...")))
            return;
        if (nPrice + nTransactionFee > GetBalance())
        {
            Error(_STR("Insufficient funds"));
            return;
        }
        CKey key;
        int64 nFeeRequired;
        if (!CreateTransaction(scriptPubKey, nPrice, wtx, key, nFeeRequired))
        {
            if (nPrice + nFeeRequired > GetBalance())
                Error(strprintf(_STR("This is an oversized transaction that requires a transaction fee of %s").c_str(), FormatMoney(nFeeRequired).c_str()));
            else
                Error(_STR("Transaction creation failed"));
            return;
        }

        // Transaction fee
        if (!ThreadSafeAskFee(nFeeRequired, _STR("Sending..."), this))
        {
            Error(_STR("Transaction aborted"));
            return;
        }

        // Make sure we're still connected
        CNode* pnode = ConnectNode(addr, 2 * 60 * 60);
        if (!pnode)
        {
            Error(_STR("Lost connection, transaction cancelled"));
            return;
        }

        // Last chance to cancel
        Sleep(50);
        if (!Status())
            return;
        fCanCancel = false;
        if (fAbort)
        {
            fCanCancel = true;
            if (!Status())
                return;
            fCanCancel = false;
        }
        if (!Status(_STR("Sending payment...")))
            return;

        // Commit
        if (!CommitTransaction(wtx, key))
        {
            Error(_STR("The transaction was rejected.  This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here."));
            return;
        }

        // Send payment tx to seller, with response going to OnReply3 via event handler
        CWalletTx wtxSend = wtx;
        wtxSend.fFromMe = false;
        pnode->PushRequest("submitorder", wtxSend, SendingDialogOnReply3, this);

        Status(_STR("Waiting for confirmation..."));
        MainFrameRepaint();
    }
}

void SendingDialogOnReply3(void* parg, CDataStream& vRecv)
{
    ((CSendingDialog*)parg)->OnReply3(vRecv);
}

void CSendingDialog::OnReply3(CDataStream& vRecv)
{
    int nRet;
    try
    {
        vRecv >> nRet;
        if (nRet > 0)
        {
            Error(_STR("The payment was sent, but the recipient was unable to verify it.\n"
                    "The transaction is recorded and will credit to the recipient,\n"
                    "but the comment information will be blank."));
            return;
        }
    }
    catch (...)
    {
        //// what do we want to do about this?
        Error(_STR("Payment was sent, but an invalid response was received"));
        return;
    }

    fSuccess = true;
    fWorkDone = true;
    Status(_STR("Payment completed"));
}






//////////////////////////////////////////////////////////////////////////////
//
// CAddressBookDialog
//

CAddressBookDialog::CAddressBookDialog(wxWindow* parent, const wxString& strInitSelected, int nPageIn, bool fDuringSendIn) : CAddressBookDialogBase(parent)
{
#ifndef __WXMSW__
    SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
    SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
#endif
    // Set initially selected page
    wxNotebookEvent event;
    event.SetSelection(nPageIn);
    OnNotebookPageChanged(event);
    m_notebook->ChangeSelection(nPageIn);

    fDuringSend = fDuringSendIn;
    if (!fDuringSend)
        m_buttonCancel->Show(false);

    // Set Icon
    wxIcon iconAddressBook;
    iconAddressBook.CopyFromBitmap(wxBitmap(addressbook16_xpm));
    SetIcon(iconAddressBook);

    // Init column headers
    m_listCtrlSending->InsertColumn(0, _("Name"), wxLIST_FORMAT_LEFT, 200);
    m_listCtrlSending->InsertColumn(1, _("Address"), wxLIST_FORMAT_LEFT, 350);
#ifndef __WXMSW__
    m_listCtrlSending->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX));
    m_listCtrlSending->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOXTEXT));
#endif
    m_listCtrlSending->SetFocus();
    m_listCtrlReceiving->InsertColumn(0, _("Label"), wxLIST_FORMAT_LEFT, 200);
    m_listCtrlReceiving->InsertColumn(1, _("Bitok Address"), wxLIST_FORMAT_LEFT, 350);
#ifndef __WXMSW__
    m_listCtrlReceiving->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX));
    m_listCtrlReceiving->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOXTEXT));
#endif
    m_listCtrlReceiving->SetFocus();

    m_listCtrlOkAddresses->InsertColumn(0, _("Label"), wxLIST_FORMAT_LEFT, 200);
    m_listCtrlOkAddresses->InsertColumn(1, _("ok-Address"), wxLIST_FORMAT_LEFT, 350);
#ifndef __WXMSW__
    m_listCtrlOkAddresses->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX));
    m_listCtrlOkAddresses->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOXTEXT));
#endif

    // Fill listctrl with address book data
    CRITICAL_BLOCK(cs_mapKeys)
    CRITICAL_BLOCK(cs_mapAddressBook)
    {
        string strDefaultReceiving = (string)pframeMain->m_textCtrlAddress->GetValue();
        foreach(const PAIRTYPE(string, string)& item, mapAddressBook)
        {
            string strAddress = item.first;
            string strName = item.second;
            uint160 hash160;
            bool fMine = (AddressToHash160(strAddress, hash160) && mapPubKeys.count(hash160));
            wxListCtrl* plistCtrl = fMine ? m_listCtrlReceiving : m_listCtrlSending;
            int nIndex = InsertLine(plistCtrl, strName, strAddress);
            if (strAddress == (fMine ? strDefaultReceiving : string(strInitSelected)))
                plistCtrl->SetItemState(nIndex, wxLIST_STATE_SELECTED|wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED|wxLIST_STATE_FOCUSED);
        }
    }

    CRITICAL_BLOCK(cs_stealthAddresses)
    {
        for (unsigned int i = 0; i < vStealthAddresses.size(); i++)
        {
            string strLabel = vStealthAddresses[i].label;
            string strEncoded = vStealthAddresses[i].Encoded();
            InsertLine(m_listCtrlOkAddresses, strLabel, strEncoded);
        }
    }
}

wxString CAddressBookDialog::GetSelectedAddress()
{
    if (nPage == OKADDRESSES)
        return GetSelectedOkAddress();
    int nIndex = GetSelection(m_listCtrl);
    if (nIndex == -1)
        return "";
    return GetItemText(m_listCtrl, nIndex, 1);
}

wxString CAddressBookDialog::GetSelectedSendingAddress()
{
    int nIndex = GetSelection(m_listCtrlSending);
    if (nIndex == -1)
        return "";
    return GetItemText(m_listCtrlSending, nIndex, 1);
}

wxString CAddressBookDialog::GetSelectedReceivingAddress()
{
    int nIndex = GetSelection(m_listCtrlReceiving);
    if (nIndex == -1)
        return "";
    return GetItemText(m_listCtrlReceiving, nIndex, 1);
}

wxString CAddressBookDialog::GetSelectedOkAddress()
{
    int nIndex = GetSelection(m_listCtrlOkAddresses);
    if (nIndex == -1)
        return "";
    return GetItemText(m_listCtrlOkAddresses, nIndex, 1);
}

void CAddressBookDialog::OnNotebookPageChanged(wxNotebookEvent& event)
{
    event.Skip();
    nPage = event.GetSelection();
    if (nPage == SENDING)
        m_listCtrl = m_listCtrlSending;
    else if (nPage == RECEIVING)
        m_listCtrl = m_listCtrlReceiving;
    else if (nPage == OKADDRESSES)
        m_listCtrl = m_listCtrlOkAddresses;
    m_buttonDelete->Show(nPage == SENDING);
    m_buttonCopy->Show(nPage == RECEIVING || nPage == OKADDRESSES);
    m_buttonExport->Show(nPage == RECEIVING || nPage == OKADDRESSES);
    m_buttonNew->Show(true);
    this->Layout();
    m_listCtrl->SetFocus();
}

void CAddressBookDialog::OnListEndLabelEdit(wxListEvent& event)
{
    event.Skip();
    if (event.IsEditCancelled())
        return;
    string strAddress = (string)GetItemText(m_listCtrl, event.GetIndex(), 1);
    string strNewName = (string)event.GetText();
    if (nPage == OKADDRESSES)
    {
        CRITICAL_BLOCK(cs_stealthAddresses)
        {
            for (unsigned int i = 0; i < vStealthAddresses.size(); i++)
            {
                if (vStealthAddresses[i].Encoded() == strAddress)
                {
                    vStealthAddresses[i].label = strNewName;
                    CWalletDB().WriteStealthAddress(vStealthAddresses[i]);
                    break;
                }
            }
        }
    }
    else
    {
        SetAddressBookName(strAddress, strNewName);
        pframeMain->RefreshListCtrl();
    }
}

void CAddressBookDialog::OnListItemSelected(wxListEvent& event)
{
    event.Skip();
}

void CAddressBookDialog::OnListItemActivated(wxListEvent& event)
{
    event.Skip();
    if (fDuringSend)
    {
        // Doubleclick returns selection
        EndModal(GetSelectedAddress() != "" ? 2 : 0);
        return;
    }

    // Doubleclick edits item
    wxCommandEvent event2;
    OnButtonEdit(event2);
}

void CAddressBookDialog::OnButtonDelete(wxCommandEvent& event)
{
    if (nPage != SENDING)
        return;
    for (int nIndex = m_listCtrl->GetItemCount()-1; nIndex >= 0; nIndex--)
    {
        if (m_listCtrl->GetItemState(nIndex, wxLIST_STATE_SELECTED))
        {
            string strAddress = (string)GetItemText(m_listCtrl, nIndex, 1);
            CWalletDB().EraseName(strAddress);
            m_listCtrl->DeleteItem(nIndex);
        }
    }
    pframeMain->RefreshListCtrl();
}

void CAddressBookDialog::OnButtonCopy(wxCommandEvent& event)
{
    wxString strAddr;
    if (nPage == OKADDRESSES)
        strAddr = GetSelectedOkAddress();
    else
        strAddr = GetSelectedAddress();
    if (wxTheClipboard->Open())
    {
        wxTheClipboard->SetData(new wxTextDataObject(strAddr));
        wxTheClipboard->Close();
    }
}

void CAddressBookDialog::OnButtonExport(wxCommandEvent& event)
{
    if (nPage == OKADDRESSES)
    {
        string strOkAddr = (string)GetSelectedOkAddress();
        if (strOkAddr.empty())
            return;

        int nResult = wxMessageBox(
            _("WARNING: Your ok-address secret controls access to all funds received via this ok-address.\n\n"
              "Anyone who has this secret can spend your funds.\n"
              "Never share it with anyone you do not trust.\n\n"
              "Do you want to export the secret for this ok-address?"),
            _("Export ok-Address Secret"),
            wxYES_NO | wxICON_WARNING);
        if (nResult != wxYES)
            return;

        vector<unsigned char> vchScanSecret;
        vector<unsigned char> vchSpendSecret;
        CRITICAL_BLOCK(cs_stealthAddresses)
        {
            for (unsigned int i = 0; i < vStealthAddresses.size(); i++)
            {
                if (vStealthAddresses[i].Encoded() == strOkAddr)
                {
                    CWalletDB walletdb("r");
                    CPrivKey scanPriv, spendPriv;
                    if (walletdb.ReadStealthScanKey(vStealthAddresses[i].scan_pubkey, scanPriv) &&
                        walletdb.ReadStealthSpendKey(vStealthAddresses[i].spend_pubkey, spendPriv))
                    {
                        CKey scanKey, spendKey;
                        scanKey.SetPrivKey(scanPriv);
                        spendKey.SetPrivKey(spendPriv);
                        vchScanSecret = scanKey.GetSecret();
                        vchSpendSecret = spendKey.GetSecret();
                    }
                    break;
                }
            }
        }

        if (vchScanSecret.empty() || vchSpendSecret.empty())
        {
            wxMessageBox(_("Failed to read secret keys for this ok-address."),
                _("Export ok-Address Secret"), wxOK | wxICON_ERROR);
            return;
        }

        string strSecret = EncodeStealthSecret(vchScanSecret, vchSpendSecret);

        enum { ID_COPY_SECRET = 1002 };
        wxDialog dlg(this, wxID_ANY, _("ok-Address Secret Exported"), wxDefaultPosition, wxDefaultSize);
#ifndef __WXMSW__
        dlg.SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
        dlg.SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
#endif
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(new wxStaticText(&dlg, wxID_ANY,
            _("Secret for ok-address:") + wxString(" ") + strOkAddr.substr(0, 24) + "..."),
            0, wxALL, 10);
        wxTextCtrl* txtKey = new wxTextCtrl(&dlg, wxID_ANY, strSecret,
            wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
#ifndef __WXMSW__
        txtKey->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
        txtKey->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
#endif
        sizer->Add(txtKey, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
        sizer->Add(new wxStaticText(&dlg, wxID_ANY,
            _("Store this secret safely. Anyone with this secret can spend coins received via this ok-address.")),
            0, wxALL, 10);

        wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
        wxButton* btnCopy = new wxButton(&dlg, ID_COPY_SECRET, _("&Copy to Clipboard"));
        wxButton* btnOK = new wxButton(&dlg, wxID_OK, _("OK"));
        btnSizer->Add(btnCopy, 0, wxALL | wxALIGN_CENTER_VERTICAL, 6);
        btnSizer->Add(btnOK, 0, wxALL | wxALIGN_CENTER_VERTICAL, 6);
        sizer->Add(btnSizer, 0, wxALIGN_CENTER | wxALL, 10);

        dlg.SetSizer(sizer);
        dlg.SetMinSize(wxSize(520, -1));
        sizer->Fit(&dlg);
        dlg.Centre();

        dlg.Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent& evt) {
            if (evt.GetId() == ID_COPY_SECRET)
            {
                if (wxTheClipboard->Open())
                {
                    wxTheClipboard->SetData(new wxTextDataObject(strSecret));
                    wxTheClipboard->Close();
                }
                btnCopy->SetLabel(_("Copied!"));
                btnCopy->Disable();
            }
            else
            {
                evt.Skip();
            }
        });

        dlg.ShowModal();
        return;
    }

    if (nPage != RECEIVING)
        return;
    string strAddress = (string)GetSelectedReceivingAddress();
    if (strAddress.empty())
        return;

    uint160 hash160;
    if (!AddressToHash160(strAddress, hash160))
    {
        wxMessageBox(_("Invalid address."), _("Export Private Key"), wxOK | wxICON_ERROR);
        return;
    }

    int nResult = wxMessageBox(
        _("WARNING: Your private key controls access to your coins.\n\n"
          "Anyone who has this key can spend your funds.\n"
          "Never share it with anyone you do not trust.\n\n"
          "Do you want to export the private key for this address?"),
        _("Export Private Key"),
        wxYES_NO | wxICON_WARNING);
    if (nResult != wxYES)
        return;

    string strWIF;
    CRITICAL_BLOCK(cs_mapKeys)
    {
        if (!mapPubKeys.count(hash160))
        {
            wxMessageBox(_("Private key not found for this address."), _("Export Private Key"), wxOK | wxICON_ERROR);
            return;
        }
        vector<unsigned char> vchPubKey = mapPubKeys[hash160];
        if (!mapKeys.count(vchPubKey))
        {
            wxMessageBox(_("Private key not found for this address."), _("Export Private Key"), wxOK | wxICON_ERROR);
            return;
        }
        CKey key;
        if (!key.SetPrivKey(mapKeys[vchPubKey]))
        {
            wxMessageBox(_("Failed to read private key."), _("Export Private Key"), wxOK | wxICON_ERROR);
            return;
        }
        vector<unsigned char> vchSecret = key.GetSecret();
        vector<unsigned char> vchWIF;
        vchWIF.push_back(128);
        vchWIF.insert(vchWIF.end(), vchSecret.begin(), vchSecret.end());
        strWIF = EncodeBase58Check(vchWIF);
    }

    enum { ID_COPY_KEY = 1001 };

    wxDialog dlg(this, wxID_ANY, _("Private Key Exported"), wxDefaultPosition, wxDefaultSize);
#ifndef __WXMSW__
    dlg.SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
    dlg.SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
#endif
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(new wxStaticText(&dlg, wxID_ANY,
        _("Private key for ") + strAddress + _(":")),
        0, wxALL, 10);
    wxTextCtrl* txtKey = new wxTextCtrl(&dlg, wxID_ANY, strWIF,
        wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
#ifndef __WXMSW__
    txtKey->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));
    txtKey->SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
#endif
    sizer->Add(txtKey, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
    sizer->Add(new wxStaticText(&dlg, wxID_ANY,
        _("Store this key safely. Anyone with this key can spend your coins.")),
        0, wxALL, 10);

    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* btnCopy = new wxButton(&dlg, ID_COPY_KEY, _("&Copy to Clipboard"));
    wxButton* btnOK = new wxButton(&dlg, wxID_OK, _("OK"));
    btnSizer->Add(btnCopy, 0, wxALL | wxALIGN_CENTER_VERTICAL, 6);
    btnSizer->Add(btnOK, 0, wxALL | wxALIGN_CENTER_VERTICAL, 6);
    sizer->Add(btnSizer, 0, wxALIGN_CENTER | wxALL, 10);

    dlg.SetSizer(sizer);
    dlg.SetMinSize(wxSize(520, -1));
    sizer->Fit(&dlg);
    dlg.Centre();

    dlg.Bind(wxEVT_COMMAND_BUTTON_CLICKED, [&](wxCommandEvent& evt) {
        if (evt.GetId() == ID_COPY_KEY)
        {
            if (wxTheClipboard->Open())
            {
                wxTheClipboard->SetData(new wxTextDataObject(strWIF));
                wxTheClipboard->Close();
            }
            btnCopy->SetLabel(_("Copied!"));
            btnCopy->Disable();
        }
        else
        {
            evt.Skip();
        }
    });

    dlg.ShowModal();
}

bool CAddressBookDialog::CheckIfMine(const string& strAddress, const string& strTitle)
{
    uint160 hash160;
    bool fMine = (AddressToHash160(strAddress, hash160) && mapPubKeys.count(hash160));
    if (fMine)
        wxMessageBox(_("This is one of your own addresses for receiving payments and cannot be entered in the address book.  "), strTitle);
    return fMine;
}

void CAddressBookDialog::OnButtonEdit(wxCommandEvent& event)
{
    int nIndex = GetSelection(m_listCtrl);
    if (nIndex == -1)
        return;
    string strName = (string)m_listCtrl->GetItemText(nIndex);
    string strAddress = (string)GetItemText(m_listCtrl, nIndex, 1);
    string strAddressOrg = strAddress;

    if (nPage == SENDING)
    {
        do
        {
            CGetTextFromUserDialog dialog(this, _STR("Edit Address"), _STR("Name"), strName, _STR("Address"), strAddress);
            if (!dialog.ShowModal())
                return;
            strName = dialog.GetValue1();
            strAddress = dialog.GetValue2();
        }
        while (CheckIfMine(strAddress, _STR("Edit Address")));

        if (strAddress != strAddressOrg)
            CWalletDB().EraseName(strAddressOrg);
        SetAddressBookName(strAddress, strName);
        m_listCtrl->SetItem(nIndex, 1, strAddress);
        m_listCtrl->SetItemText(nIndex, strName);
        pframeMain->RefreshListCtrl();
    }
    else if (nPage == RECEIVING)
    {
        CGetTextFromUserDialog dialog(this, _STR("Edit Address Label"), _STR("Label"), strName);
        if (!dialog.ShowModal())
            return;
        strName = dialog.GetValue();
        SetAddressBookName(strAddress, strName);
        m_listCtrl->SetItem(nIndex, 1, strAddress);
        m_listCtrl->SetItemText(nIndex, strName);
        pframeMain->RefreshListCtrl();
    }
    else if (nPage == OKADDRESSES)
    {
        CGetTextFromUserDialog dialog(this, _STR("Edit ok-Address Label"), _STR("Label"), strName);
        if (!dialog.ShowModal())
            return;
        strName = dialog.GetValue();
        CRITICAL_BLOCK(cs_stealthAddresses)
        {
            for (unsigned int i = 0; i < vStealthAddresses.size(); i++)
            {
                if (vStealthAddresses[i].Encoded() == strAddress)
                {
                    vStealthAddresses[i].label = strName;
                    CWalletDB().WriteStealthAddress(vStealthAddresses[i]);
                    break;
                }
            }
        }
        m_listCtrl->SetItemText(nIndex, strName);
    }
}

void CAddressBookDialog::OnButtonNew(wxCommandEvent& event)
{
    string strName;
    string strAddress;

    if (nPage == SENDING)
    {
        // Ask name and address
        do
        {
            CGetTextFromUserDialog dialog(this, _STR("Add Address"), _STR("Name"), strName, _STR("Address"), strAddress);
            if (!dialog.ShowModal())
                return;
            strName = dialog.GetValue1();
            strAddress = dialog.GetValue2();
        }
        while (CheckIfMine(strAddress, _STR("Add Address")));
    }
    else if (nPage == RECEIVING)
    {
        wxArrayString choices;
        choices.Add(_("Create New Address"));
        choices.Add(_("Import Private Key"));
        wxSingleChoiceDialog choiceDlg(this,
            _("What would you like to do?"),
            _("New Receiving Address"),
            choices);
        choiceDlg.SetSelection(0);
        if (choiceDlg.ShowModal() != wxID_OK)
            return;

        if (choiceDlg.GetSelection() == 0)
        {
            CGetTextFromUserDialog dialog(this,
                _STR("New Receiving Address"),
                _STR("It's good policy to use a new address for each payment you receive.\n\nLabel"),
                "");
            if (!dialog.ShowModal())
                return;
            strName = dialog.GetValue();
            strAddress = PubKeyToAddress(GenerateNewKey());
        }
        else
        {
            CGetTextFromUserDialog dialog(this,
                _STR("Import Private Key"),
                _STR("Private Key (WIF format)"),
                "",
                _STR("Label (optional)"),
                "");
            if (!dialog.ShowModal())
                return;
            string strSecret = dialog.GetValue1();
            strName = dialog.GetValue2();

            vector<unsigned char> vchWIF;
            if (!DecodeBase58Check(strSecret, vchWIF) || vchWIF.size() < 33 || vchWIF[0] != 128)
            {
                wxMessageBox(_("Invalid private key format. Please enter a valid WIF key."),
                    _("Import Private Key"), wxOK | wxICON_ERROR);
                return;
            }
            vector<unsigned char> vchSecret(vchWIF.begin() + 1, vchWIF.begin() + 33);
            CKey key;
            if (!key.SetSecret(vchSecret))
            {
                wxMessageBox(_("Invalid private key."),
                    _("Import Private Key"), wxOK | wxICON_ERROR);
                return;
            }
            if (!AddKey(key))
            {
                wxMessageBox(_("Error adding key to wallet."),
                    _("Import Private Key"), wxOK | wxICON_ERROR);
                return;
            }
            strAddress = PubKeyToAddress(key.GetPubKey());

            {
                printf("[WALLET] Rescanning blockchain for imported key %s\n", strAddress.c_str());
                wxProgressDialog progressDlg(
                    _("Rescanning Blockchain"),
                    _("Scanning for wallet transactions..."),
                    100,
                    this,
                    wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_SMOOTH | wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME);
                int nFound = ScanWalletTransactions(pindexGenesisBlock,
                    bind(RescanProgressCallback, &progressDlg, _1, _2, _3));
                progressDlg.Update(100);
                if (nFound > 0)
                    wxMessageBox(strprintf(_("Rescan complete. Found %d transaction(s).").mb_str(), nFound),
                        _("Import Private Key"), wxOK | wxICON_INFORMATION);
            }

            {
                wxProgressDialog atomDlg(
                    _("Rebuilding ATOM Index"),
                    _("Rescanning ATOM balances and transactions..."),
                    100,
                    this,
                    wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_SMOOTH | wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME);
                RescanAtom(bind(AtomRescanProgressCallback, &atomDlg, _1, _2));
                atomDlg.Update(100);
            }
        }
    }
    else if (nPage == OKADDRESSES)
    {
        wxArrayString choices;
        choices.Add(_("Create New ok-Address"));
        choices.Add(_("Import ok-Address Secret"));
        wxSingleChoiceDialog choiceDlg(this,
            _("What would you like to do?"),
            _("New ok-Address"),
            choices);
        choiceDlg.SetSelection(0);
        if (choiceDlg.ShowModal() != wxID_OK)
            return;

        if (choiceDlg.GetSelection() == 0)
        {
            CGetTextFromUserDialog dialog(this,
                _STR("New ok-Address"),
                _STR("Label"),
                "");
            if (!dialog.ShowModal())
                return;
            strName = dialog.GetValue();
            if (!CreateNewOkAddress(this, strName))
                return;
            CRITICAL_BLOCK(cs_stealthAddresses)
            {
                if (!vStealthAddresses.empty())
                    strAddress = vStealthAddresses.back().Encoded();
            }
        }
        else
        {
            CGetTextFromUserDialog dialog(this,
                _STR("Import ok-Address Secret"),
                _STR("ok-Address Secret (SK... format)"),
                "",
                _STR("Label (optional)"),
                "");
            if (!dialog.ShowModal())
                return;
            strName = dialog.GetValue2();
            if (!ImportOkAddressSecret(this, dialog.GetValue1(), strName))
                return;
            CRITICAL_BLOCK(cs_stealthAddresses)
            {
                if (!vStealthAddresses.empty())
                    strAddress = vStealthAddresses.back().Encoded();
            }
        }

        int nIndex = InsertLine(m_listCtrl, strName, strAddress);
        SetSelection(m_listCtrl, nIndex);
        m_listCtrl->SetFocus();
        return;
    }

    // Add to list and select it
    SetAddressBookName(strAddress, strName);
    int nIndex = InsertLine(m_listCtrl, strName, strAddress);
    SetSelection(m_listCtrl, nIndex);
    m_listCtrl->SetFocus();
    if (nPage == SENDING)
        pframeMain->RefreshListCtrl();
}

void CAddressBookDialog::OnButtonOK(wxCommandEvent& event)
{
    if (nPage == RECEIVING && !fDuringSend)
        SetDefaultReceivingAddress((string)GetSelectedReceivingAddress());
    if (nPage == OKADDRESSES)
    {
        wxString strOk = GetSelectedOkAddress();
        if (!strOk.empty() && fDuringSend)
        {
            EndModal(2);
            return;
        }
    }
    EndModal(GetSelectedAddress() != "" ? 1 : 0);
}

void CAddressBookDialog::OnButtonCancel(wxCommandEvent& event)
{
    // Cancel
    EndModal(0);
}

void CAddressBookDialog::OnClose(wxCloseEvent& event)
{
    // Close
    EndModal(0);
}






//////////////////////////////////////////////////////////////////////////////
//
// CMyTaskBarIcon
//

enum
{
    ID_TASKBAR_RESTORE = 10001,
    ID_TASKBAR_OPTIONS,
    ID_TASKBAR_GENERATE,
    ID_TASKBAR_EXIT,
};

BEGIN_EVENT_TABLE(CMyTaskBarIcon, wxTaskBarIcon)
    EVT_TASKBAR_LEFT_DCLICK(CMyTaskBarIcon::OnLeftButtonDClick)
    EVT_MENU(ID_TASKBAR_RESTORE, CMyTaskBarIcon::OnMenuRestore)
    EVT_MENU(ID_TASKBAR_OPTIONS, CMyTaskBarIcon::OnMenuOptions)
    EVT_MENU(ID_TASKBAR_GENERATE, CMyTaskBarIcon::OnMenuGenerate)
    EVT_UPDATE_UI(ID_TASKBAR_GENERATE, CMyTaskBarIcon::OnUpdateUIGenerate)
    EVT_MENU(ID_TASKBAR_EXIT, CMyTaskBarIcon::OnMenuExit)
END_EVENT_TABLE()

void CMyTaskBarIcon::Show(bool fShow)
{
    static char pszPrevTip[200];
    if (fShow)
    {
        int nNumConnections = 0;
        int nHighestPeerHeight = 0;
        CRITICAL_BLOCK(cs_vNodes)
        {
            nNumConnections = vNodes.size();
            foreach(CNode* pnode, vNodes)
                if (pnode->nStartingHeight > nHighestPeerHeight)
                    nHighestPeerHeight = pnode->nStartingHeight;
        }

        string strTooltip;
        if (nNumConnections == 0)
            strTooltip = strprintf(_STR("Bitok - Offline (%d blocks)").c_str(), nBestHeight + 1);
        else if (nHighestPeerHeight > 0 && nBestHeight < nHighestPeerHeight - 5)
            strTooltip = strprintf(_STR("Bitok - Syncing %d/%d").c_str(), nBestHeight + 1, nHighestPeerHeight);
        else
            strTooltip = strprintf(_STR("Bitok - %d blocks").c_str(), nBestHeight + 1);

        if (fGenerateBitcoins)
        {
            int nMinerThreads = vnThreadsRunning[3];
            if (nMinerThreads > 0)
                strTooltip += strprintf(_STR(" [Mining %d]").c_str(), nMinerThreads);
            else
                strTooltip += _STR(" [Mining]");
        }

        if (strncmp(pszPrevTip, strTooltip.c_str(), sizeof(pszPrevTip)-1) != 0)
        {
            strlcpy(pszPrevTip, strTooltip.c_str(), sizeof(pszPrevTip));
#ifdef __WXMSW__
            SetIcon(wxICON(favicon), strTooltip);
#else
            SetIcon(bitcoin80_xpm, strTooltip);
#endif
        }
    }
    else
    {
        strlcpy(pszPrevTip, "", sizeof(pszPrevTip));
        RemoveIcon();
    }
}

void CMyTaskBarIcon::Hide()
{
    Show(false);
}

void CMyTaskBarIcon::OnLeftButtonDClick(wxTaskBarIconEvent& event)
{
    Restore();
}

void CMyTaskBarIcon::OnMenuRestore(wxCommandEvent& event)
{
    Restore();
}

void CMyTaskBarIcon::OnMenuOptions(wxCommandEvent& event)
{
    // Since it's modal, get the main window to do it
    wxCommandEvent event2(wxEVT_COMMAND_MENU_SELECTED, wxID_PREFERENCES);
    pframeMain->GetEventHandler()->AddPendingEvent(event2);
}

void CMyTaskBarIcon::Restore()
{
    pframeMain->Show();
    wxIconizeEvent event(0, false);
    pframeMain->GetEventHandler()->AddPendingEvent(event);
    pframeMain->Iconize(false);
    pframeMain->Raise();
}

void CMyTaskBarIcon::OnMenuGenerate(wxCommandEvent& event)
{
    GenerateBitcoins(event.IsChecked());
}

void CMyTaskBarIcon::OnUpdateUIGenerate(wxUpdateUIEvent& event)
{
    event.Check(fGenerateBitcoins);
}

void CMyTaskBarIcon::OnMenuExit(wxCommandEvent& event)
{
    pframeMain->Close(true);
}

void CMyTaskBarIcon::UpdateTooltip()
{
    if (IsIconInstalled())
        Show(true);
}

wxMenu* CMyTaskBarIcon::CreatePopupMenu()
{
    wxMenu* pmenu = new wxMenu;
    pmenu->Append(ID_TASKBAR_RESTORE, _("&Open Bitok"));
    pmenu->Append(ID_TASKBAR_OPTIONS, _("O&ptions..."));
    pmenu->AppendCheckItem(ID_TASKBAR_GENERATE, _("&Generate Coins"))->Check(fGenerateBitcoins);
#ifndef __WXMAC_OSX__ // Mac has built-in quit menu
    pmenu->AppendSeparator();
    pmenu->Append(ID_TASKBAR_EXIT, _("E&xit"));
#endif
    return pmenu;
}













void CreateMainWindow()
{
    pframeMain = new CMainFrame(NULL);
    if (GetBoolArg("-min"))
        pframeMain->Iconize(true);
    pframeMain->Show(true);  // have to show first to get taskbar button to hide
    if (fMinimizeToTray && pframeMain->IsIconized())
        fClosedToTray = true;
    pframeMain->Show(!fClosedToTray);
    ptaskbaricon->Show(fMinimizeToTray || fClosedToTray);
    CreateThread(ThreadDelayedRepaint, NULL);
}

#endif // wxUSE_GUI
