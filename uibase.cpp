///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Apr 16 2008)
// http://www.wxformbuilder.org/
//
// PLEASE DO "NOT" EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "uibase.h"

#include "xpm/about.xpm"
#include "xpm/addressbook20.xpm"
#include "xpm/check.xpm"
#include "xpm/send20.xpm"
#include "xpm/mining20.xpm"
#include "xpm/settings20.xpm"

///////////////////////////////////////////////////////////////////////////

CMainFrameBase::CMainFrameBase( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxFrame( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );
	this->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );

	m_menubar = new wxMenuBar( 0 );
	m_menuFile = new wxMenu();
	wxMenuItem* m_menuFileExit;
	m_menuFileExit = new wxMenuItem( m_menuFile, wxID_EXIT, wxString( _("E&xit") ) , wxEmptyString, wxITEM_NORMAL );
	m_menuFile->Append( m_menuFileExit );

	m_menubar->Append( m_menuFile, _("&File") );

	m_menuOptions = new wxMenu();
	wxMenuItem* m_menuOptionsGenerateBitcoins;
	m_menuOptionsGenerateBitcoins = new wxMenuItem( m_menuOptions, wxID_OPTIONSGENERATEBITCOINS, wxString( _("&Generate Coins") ) , wxEmptyString, wxITEM_CHECK );
	m_menuOptions->Append( m_menuOptionsGenerateBitcoins );

	wxMenuItem* m_menuOptionsChangeYourAddress;
	m_menuOptionsChangeYourAddress = new wxMenuItem( m_menuOptions, wxID_ANY, wxString( _("&Your Receiving Addresses...") ) , wxEmptyString, wxITEM_NORMAL );
	m_menuOptions->Append( m_menuOptionsChangeYourAddress );

	wxMenuItem* m_menuOptionsOptions;
	m_menuOptionsOptions = new wxMenuItem( m_menuOptions, wxID_PREFERENCES, wxString( _("&Options...") ) , wxEmptyString, wxITEM_NORMAL );
	m_menuOptions->Append( m_menuOptionsOptions );

	m_menubar->Append( m_menuOptions, _("&Settings") );

	m_menuHelp = new wxMenu();
	wxMenuItem* m_menuHelpAbout;
	m_menuHelpAbout = new wxMenuItem( m_menuHelp, wxID_ABOUT, wxString( _("&About...") ) , wxEmptyString, wxITEM_NORMAL );
	m_menuHelp->Append( m_menuHelpAbout );

	m_menubar->Append( m_menuHelp, _("&Help") );

	this->SetMenuBar( m_menubar );

	m_toolBar = this->CreateToolBar( wxTB_FLAT|wxTB_HORZ_TEXT, wxID_ANY );
	m_toolBar->SetToolBitmapSize( wxSize( 24,24 ) );
	m_toolBar->SetToolSeparation( 8 );
	m_toolBar->SetFont( wxFont( 10, 70, 90, 92, false, wxEmptyString ) );
	m_toolBar->SetMargins( 8, 8 );

	m_toolBar->AddTool( wxID_BUTTONSEND, _("  Send Coins  "), wxBitmap( send20_xpm ), wxNullBitmap, wxITEM_NORMAL, _("Send coins to another address"), wxEmptyString );
	m_toolBar->AddTool( wxID_BUTTONRECEIVE, _("  Address Book  "), wxBitmap( addressbook20_xpm ), wxNullBitmap, wxITEM_NORMAL, _("Manage your address book"), wxEmptyString );
#ifdef __WXOSX__
	m_toolBar->AddSeparator();
	m_toolBar->AddTool( wxID_BUTTONGENERATECOINS, _("  Generate Coins  "), wxBitmap( mining20_xpm ), wxNullBitmap, wxITEM_CHECK, _("Start or stop mining coins"), wxEmptyString );
	m_toolBar->AddTool( wxID_BUTTONSETTINGS, _("  Settings  "), wxBitmap( settings20_xpm ), wxNullBitmap, wxITEM_NORMAL, _("Configure mining threads and other options"), wxEmptyString );
#endif
	m_toolBar->Realize();

	m_statusBar = this->CreateStatusBar( 1, wxST_SIZEGRIP|wxSB_SUNKEN, wxID_ANY );
	m_statusBar->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_BTNFACE ) );

	wxBoxSizer* bSizer2;
	bSizer2 = new wxBoxSizer( wxVERTICAL );

	bSizer2->Add( 0, 8, 0, wxEXPAND, 5 );

	wxFlexGridSizer* fgSizerAddrs;
	fgSizerAddrs = new wxFlexGridSizer( 2, 4, 4, 0 );
	fgSizerAddrs->AddGrowableCol( 1 );
	fgSizerAddrs->SetFlexibleDirection( wxHORIZONTAL );
	fgSizerAddrs->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_NONE );

	m_staticTextAddrLabel = new wxStaticText( this, wxID_ANY, _("Bitok Address:"), wxDefaultPosition, wxSize( 100,-1 ), wxALIGN_RIGHT );
	m_staticTextAddrLabel->Wrap( -1 );
	m_staticTextAddrLabel->SetFont( wxFont( 9, 70, 90, 90, false, wxEmptyString ) );
	fgSizerAddrs->Add( m_staticTextAddrLabel, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT|wxLEFT, 10 );

	m_textCtrlAddress = new wxTextCtrl( this, wxID_TEXTCTRLADDRESS, wxEmptyString, wxDefaultPosition, wxSize( 340,26 ), wxTE_READONLY );
	m_textCtrlAddress->SetFont( wxFont( 9, 70, 90, 90, false, wxEmptyString ) );
	fgSizerAddrs->Add( m_textCtrlAddress, 1, wxALIGN_CENTER_VERTICAL|wxEXPAND|wxRIGHT|wxLEFT, 6 );

	m_buttonCopy = new wxButton( this, wxID_BUTTONCOPY, _("&Copy"), wxDefaultPosition, wxSize( 70,26 ), 0 );
	fgSizerAddrs->Add( m_buttonCopy, 0, wxALIGN_CENTER_VERTICAL, 0 );

	m_buttonNew = new wxButton( this, wxID_BUTTONNEW, _("&New..."), wxDefaultPosition, wxSize( 80,26 ), 0 );
	fgSizerAddrs->Add( m_buttonNew, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT|wxLEFT, 4 );

	m_staticTextOkLabel = new wxStaticText( this, wxID_ANY, _("ok-Address:"), wxDefaultPosition, wxSize( 100,-1 ), wxALIGN_RIGHT );
	m_staticTextOkLabel->Wrap( -1 );
	m_staticTextOkLabel->SetFont( wxFont( 9, 70, 90, 90, false, wxEmptyString ) );
	fgSizerAddrs->Add( m_staticTextOkLabel, 0, wxALIGN_CENTER_VERTICAL|wxALIGN_RIGHT|wxLEFT, 10 );

	m_textCtrlOkAddress = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( 340,26 ), wxTE_READONLY );
	m_textCtrlOkAddress->SetFont( wxFont( 9, 70, 90, 90, false, wxEmptyString ) );
	fgSizerAddrs->Add( m_textCtrlOkAddress, 1, wxALIGN_CENTER_VERTICAL|wxEXPAND|wxRIGHT|wxLEFT, 6 );

	m_buttonCopyOk = new wxButton( this, wxID_BUTTONCOPYOK, _("&Copy"), wxDefaultPosition, wxSize( 70,26 ), 0 );
	fgSizerAddrs->Add( m_buttonCopyOk, 0, wxALIGN_CENTER_VERTICAL, 0 );

	fgSizerAddrs->Add( 0, 0, 0, 0, 0 );

	bSizer2->Add( fgSizerAddrs, 0, wxEXPAND|wxRIGHT|wxLEFT, 5 );

	wxBoxSizer* bSizer3;
	bSizer3 = new wxBoxSizer( wxHORIZONTAL );

	wxBoxSizer* bSizer66;
	bSizer66 = new wxBoxSizer( wxHORIZONTAL );

	m_staticText41 = new wxStaticText( this, wxID_ANY, _("Balance:"), wxDefaultPosition, wxSize( -1,22 ), 0 );
	m_staticText41->Wrap( -1 );
	m_staticText41->SetFont( wxFont( 11, 70, 90, 92, false, wxEmptyString ) );
	bSizer66->Add( m_staticText41, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 6 );

	m_staticTextBalance = new wxStaticText( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( 180,24 ), wxALIGN_RIGHT|wxST_NO_AUTORESIZE );
	m_staticTextBalance->Wrap( -1 );
	m_staticTextBalance->SetFont( wxFont( 12, 70, 90, 92, false, wxEmptyString ) );
	m_staticTextBalance->SetBackgroundColour( wxSystemSettings::GetColour( wxSYS_COLOUR_WINDOW ) );

	bSizer66->Add( m_staticTextBalance, 0, wxALIGN_CENTER_VERTICAL|wxALL, 6 );

	bSizer3->Add( bSizer66, 1, wxEXPAND|wxALL, 3 );


	bSizer3->Add( 0, 0, 0, wxEXPAND, 5 );

	wxString m_choiceFilterChoices[] = { _(" All"), _(" Sent"), _(" Received"), _(" In Progress") };
	int m_choiceFilterNChoices = sizeof( m_choiceFilterChoices ) / sizeof( wxString );
	m_choiceFilter = new wxChoice( this, wxID_ANY, wxDefaultPosition, wxSize( 130,26 ), m_choiceFilterNChoices, m_choiceFilterChoices, 0 );
	m_choiceFilter->SetSelection( 0 );
	m_choiceFilter->Hide();

	bSizer3->Add( m_choiceFilter, 0, wxALIGN_BOTTOM|wxTOP|wxRIGHT|wxLEFT, 6 );

	bSizer2->Add( bSizer3, 0, wxEXPAND, 5 );

	m_notebook = new wxNotebook( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_notebook->SetFont( wxFont( 9, 70, 90, 90, false, wxEmptyString ) );
	m_panel9 = new wxPanel( m_notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer11;
	bSizer11 = new wxBoxSizer( wxVERTICAL );

	m_listCtrlAll = new wxListCtrl( m_panel9, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_NO_SORT_HEADER|wxLC_REPORT|wxLC_SORT_DESCENDING );
	bSizer11->Add( m_listCtrlAll, 1, wxEXPAND, 5 );

	m_panel9->SetSizer( bSizer11 );
	m_panel9->Layout();
	bSizer11->Fit( m_panel9 );
	m_notebook->AddPage( m_panel9, _("All Transactions"), true );
	m_panel92 = new wxPanel( m_notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer112;
	bSizer112 = new wxBoxSizer( wxVERTICAL );

	m_listCtrlSent = new wxListCtrl( m_panel92, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_NO_SORT_HEADER|wxLC_REPORT|wxLC_SORT_DESCENDING );
	bSizer112->Add( m_listCtrlSent, 1, wxEXPAND, 5 );

	m_panel92->SetSizer( bSizer112 );
	m_panel92->Layout();
	bSizer112->Fit( m_panel92 );
	m_notebook->AddPage( m_panel92, _("Sent"), false );
	m_panel93 = new wxPanel( m_notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer113;
	bSizer113 = new wxBoxSizer( wxVERTICAL );

	m_listCtrlReceived = new wxListCtrl( m_panel93, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_NO_SORT_HEADER|wxLC_REPORT|wxLC_SORT_DESCENDING );
	bSizer113->Add( m_listCtrlReceived, 1, wxEXPAND, 5 );

	m_panel93->SetSizer( bSizer113 );
	m_panel93->Layout();
	bSizer113->Fit( m_panel93 );
	m_notebook->AddPage( m_panel93, _("Received"), false );

	bSizer2->Add( m_notebook, 1, wxEXPAND|wxALL, 6 );

	bSizer2->Add( 0, 6, 0, wxEXPAND, 5 );

	this->SetSizer( bSizer2 );
	this->Layout();

	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( CMainFrameBase::OnClose ) );
	this->Connect( wxEVT_ICONIZE, wxIconizeEventHandler( CMainFrameBase::OnIconize ) );
	this->Connect( wxEVT_IDLE, wxIdleEventHandler( CMainFrameBase::OnIdle ) );
	this->Connect( wxEVT_LEFT_DOWN, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Connect( wxEVT_LEFT_UP, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Connect( wxEVT_MIDDLE_DOWN, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Connect( wxEVT_MIDDLE_UP, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Connect( wxEVT_RIGHT_UP, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Connect( wxEVT_MOTION, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Connect( wxEVT_LEFT_DCLICK, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Connect( wxEVT_MIDDLE_DCLICK, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Connect( wxEVT_RIGHT_DCLICK, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Connect( wxEVT_LEAVE_WINDOW, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Connect( wxEVT_ENTER_WINDOW, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Connect( wxEVT_MOUSEWHEEL, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Connect( wxEVT_PAINT, wxPaintEventHandler( CMainFrameBase::OnPaint ) );
	this->Connect( m_menuFileExit->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( CMainFrameBase::OnMenuFileExit ) );
	this->Connect( m_menuOptionsGenerateBitcoins->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( CMainFrameBase::OnMenuOptionsGenerate ) );
	this->Connect( m_menuOptionsGenerateBitcoins->GetId(), wxEVT_UPDATE_UI, wxUpdateUIEventHandler( CMainFrameBase::OnUpdateUIOptionsGenerate ) );
	this->Connect( m_menuOptionsChangeYourAddress->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( CMainFrameBase::OnMenuOptionsChangeYourAddress ) );
	this->Connect( m_menuOptionsOptions->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( CMainFrameBase::OnMenuOptionsOptions ) );
	this->Connect( m_menuHelpAbout->GetId(), wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( CMainFrameBase::OnMenuHelpAbout ) );
	this->Connect( wxID_BUTTONSEND, wxEVT_COMMAND_TOOL_CLICKED, wxCommandEventHandler( CMainFrameBase::OnButtonSend ) );
	this->Connect( wxID_BUTTONRECEIVE, wxEVT_COMMAND_TOOL_CLICKED, wxCommandEventHandler( CMainFrameBase::OnButtonAddressBook ) );
#ifdef __WXOSX__
	this->Connect( wxID_BUTTONGENERATECOINS, wxEVT_COMMAND_TOOL_CLICKED, wxCommandEventHandler( CMainFrameBase::OnButtonGenerateCoins ) );
	this->Connect( wxID_BUTTONGENERATECOINS, wxEVT_UPDATE_UI, wxUpdateUIEventHandler( CMainFrameBase::OnUpdateUIButtonGenerateCoins ) );
	this->Connect( wxID_BUTTONSETTINGS, wxEVT_COMMAND_TOOL_CLICKED, wxCommandEventHandler( CMainFrameBase::OnButtonSettings ) );
#endif
	m_textCtrlAddress->Connect( wxEVT_KEY_DOWN, wxKeyEventHandler( CMainFrameBase::OnKeyDown ), NULL, this );
	m_textCtrlAddress->Connect( wxEVT_LEFT_DOWN, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Connect( wxEVT_LEFT_UP, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Connect( wxEVT_MIDDLE_DOWN, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Connect( wxEVT_MIDDLE_UP, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Connect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Connect( wxEVT_RIGHT_UP, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Connect( wxEVT_MOTION, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Connect( wxEVT_LEFT_DCLICK, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Connect( wxEVT_MIDDLE_DCLICK, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Connect( wxEVT_RIGHT_DCLICK, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Connect( wxEVT_LEAVE_WINDOW, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Connect( wxEVT_ENTER_WINDOW, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Connect( wxEVT_MOUSEWHEEL, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Connect( wxEVT_SET_FOCUS, wxFocusEventHandler( CMainFrameBase::OnSetFocusAddress ), NULL, this );
	m_buttonNew->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CMainFrameBase::OnButtonNew ), NULL, this );
	m_buttonCopy->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CMainFrameBase::OnButtonCopy ), NULL, this );
	m_buttonCopyOk->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CMainFrameBase::OnButtonCopyOk ), NULL, this );
	m_notebook->Connect( wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGED, wxNotebookEventHandler( CMainFrameBase::OnNotebookPageChanged ), NULL, this );
	m_listCtrlAll->Connect( wxEVT_COMMAND_LIST_COL_BEGIN_DRAG, wxListEventHandler( CMainFrameBase::OnListColBeginDrag ), NULL, this );
	m_listCtrlAll->Connect( wxEVT_COMMAND_LIST_ITEM_ACTIVATED, wxListEventHandler( CMainFrameBase::OnListItemActivated ), NULL, this );
	m_listCtrlAll->Connect( wxEVT_PAINT, wxPaintEventHandler( CMainFrameBase::OnPaintListCtrl ), NULL, this );
	m_listCtrlSent->Connect( wxEVT_COMMAND_LIST_COL_BEGIN_DRAG, wxListEventHandler( CMainFrameBase::OnListColBeginDrag ), NULL, this );
	m_listCtrlSent->Connect( wxEVT_COMMAND_LIST_ITEM_ACTIVATED, wxListEventHandler( CMainFrameBase::OnListItemActivated ), NULL, this );
	m_listCtrlSent->Connect( wxEVT_PAINT, wxPaintEventHandler( CMainFrameBase::OnPaintListCtrl ), NULL, this );
	m_listCtrlReceived->Connect( wxEVT_COMMAND_LIST_COL_BEGIN_DRAG, wxListEventHandler( CMainFrameBase::OnListColBeginDrag ), NULL, this );
	m_listCtrlReceived->Connect( wxEVT_COMMAND_LIST_ITEM_ACTIVATED, wxListEventHandler( CMainFrameBase::OnListItemActivated ), NULL, this );
	m_listCtrlReceived->Connect( wxEVT_PAINT, wxPaintEventHandler( CMainFrameBase::OnPaintListCtrl ), NULL, this );
}

CMainFrameBase::~CMainFrameBase()
{
	// Disconnect Events
	this->Disconnect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( CMainFrameBase::OnClose ) );
	this->Disconnect( wxEVT_ICONIZE, wxIconizeEventHandler( CMainFrameBase::OnIconize ) );
	this->Disconnect( wxEVT_IDLE, wxIdleEventHandler( CMainFrameBase::OnIdle ) );
	this->Disconnect( wxEVT_LEFT_DOWN, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Disconnect( wxEVT_LEFT_UP, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Disconnect( wxEVT_MIDDLE_DOWN, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Disconnect( wxEVT_MIDDLE_UP, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Disconnect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Disconnect( wxEVT_RIGHT_UP, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Disconnect( wxEVT_MOTION, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Disconnect( wxEVT_LEFT_DCLICK, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Disconnect( wxEVT_MIDDLE_DCLICK, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Disconnect( wxEVT_RIGHT_DCLICK, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Disconnect( wxEVT_LEAVE_WINDOW, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Disconnect( wxEVT_ENTER_WINDOW, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Disconnect( wxEVT_MOUSEWHEEL, wxMouseEventHandler( CMainFrameBase::OnMouseEvents ) );
	this->Disconnect( wxEVT_PAINT, wxPaintEventHandler( CMainFrameBase::OnPaint ) );
	this->Disconnect( wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( CMainFrameBase::OnMenuFileExit ) );
	this->Disconnect( wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( CMainFrameBase::OnMenuOptionsGenerate ) );
	this->Disconnect( wxID_ANY, wxEVT_UPDATE_UI, wxUpdateUIEventHandler( CMainFrameBase::OnUpdateUIOptionsGenerate ) );
	this->Disconnect( wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( CMainFrameBase::OnMenuOptionsChangeYourAddress ) );
	this->Disconnect( wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( CMainFrameBase::OnMenuOptionsOptions ) );
	this->Disconnect( wxID_ANY, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler( CMainFrameBase::OnMenuHelpAbout ) );
	this->Disconnect( wxID_BUTTONSEND, wxEVT_COMMAND_TOOL_CLICKED, wxCommandEventHandler( CMainFrameBase::OnButtonSend ) );
	this->Disconnect( wxID_BUTTONRECEIVE, wxEVT_COMMAND_TOOL_CLICKED, wxCommandEventHandler( CMainFrameBase::OnButtonAddressBook ) );
#ifdef __WXOSX__
	this->Disconnect( wxID_BUTTONGENERATECOINS, wxEVT_COMMAND_TOOL_CLICKED, wxCommandEventHandler( CMainFrameBase::OnButtonGenerateCoins ) );
	this->Disconnect( wxID_BUTTONGENERATECOINS, wxEVT_UPDATE_UI, wxUpdateUIEventHandler( CMainFrameBase::OnUpdateUIButtonGenerateCoins ) );
	this->Disconnect( wxID_BUTTONSETTINGS, wxEVT_COMMAND_TOOL_CLICKED, wxCommandEventHandler( CMainFrameBase::OnButtonSettings ) );
#endif
	m_textCtrlAddress->Disconnect( wxEVT_KEY_DOWN, wxKeyEventHandler( CMainFrameBase::OnKeyDown ), NULL, this );
	m_textCtrlAddress->Disconnect( wxEVT_LEFT_DOWN, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Disconnect( wxEVT_LEFT_UP, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Disconnect( wxEVT_MIDDLE_DOWN, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Disconnect( wxEVT_MIDDLE_UP, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Disconnect( wxEVT_RIGHT_DOWN, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Disconnect( wxEVT_RIGHT_UP, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Disconnect( wxEVT_MOTION, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Disconnect( wxEVT_LEFT_DCLICK, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Disconnect( wxEVT_MIDDLE_DCLICK, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Disconnect( wxEVT_RIGHT_DCLICK, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Disconnect( wxEVT_LEAVE_WINDOW, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Disconnect( wxEVT_ENTER_WINDOW, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Disconnect( wxEVT_MOUSEWHEEL, wxMouseEventHandler( CMainFrameBase::OnMouseEventsAddress ), NULL, this );
	m_textCtrlAddress->Disconnect( wxEVT_SET_FOCUS, wxFocusEventHandler( CMainFrameBase::OnSetFocusAddress ), NULL, this );
	m_buttonNew->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CMainFrameBase::OnButtonNew ), NULL, this );
	m_buttonCopy->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CMainFrameBase::OnButtonCopy ), NULL, this );
	m_buttonCopyOk->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CMainFrameBase::OnButtonCopyOk ), NULL, this );
	m_notebook->Disconnect( wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGED, wxNotebookEventHandler( CMainFrameBase::OnNotebookPageChanged ), NULL, this );
	m_listCtrlAll->Disconnect( wxEVT_COMMAND_LIST_COL_BEGIN_DRAG, wxListEventHandler( CMainFrameBase::OnListColBeginDrag ), NULL, this );
	m_listCtrlAll->Disconnect( wxEVT_COMMAND_LIST_ITEM_ACTIVATED, wxListEventHandler( CMainFrameBase::OnListItemActivated ), NULL, this );
	m_listCtrlAll->Disconnect( wxEVT_PAINT, wxPaintEventHandler( CMainFrameBase::OnPaintListCtrl ), NULL, this );
	m_listCtrlSent->Disconnect( wxEVT_COMMAND_LIST_COL_BEGIN_DRAG, wxListEventHandler( CMainFrameBase::OnListColBeginDrag ), NULL, this );
	m_listCtrlSent->Disconnect( wxEVT_COMMAND_LIST_ITEM_ACTIVATED, wxListEventHandler( CMainFrameBase::OnListItemActivated ), NULL, this );
	m_listCtrlSent->Disconnect( wxEVT_PAINT, wxPaintEventHandler( CMainFrameBase::OnPaintListCtrl ), NULL, this );
	m_listCtrlReceived->Disconnect( wxEVT_COMMAND_LIST_COL_BEGIN_DRAG, wxListEventHandler( CMainFrameBase::OnListColBeginDrag ), NULL, this );
	m_listCtrlReceived->Disconnect( wxEVT_COMMAND_LIST_ITEM_ACTIVATED, wxListEventHandler( CMainFrameBase::OnListItemActivated ), NULL, this );
	m_listCtrlReceived->Disconnect( wxEVT_PAINT, wxPaintEventHandler( CMainFrameBase::OnPaintListCtrl ), NULL, this );
}

CTxDetailsDialogBase::CTxDetailsDialogBase( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( 500,350 ), wxDefaultSize );

	wxBoxSizer* bSizer64;
	bSizer64 = new wxBoxSizer( wxVERTICAL );

	m_htmlWin = new wxHtmlWindow( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHW_SCROLLBAR_AUTO );
	bSizer64->Add( m_htmlWin, 1, wxALL|wxEXPAND, 5 );

	wxBoxSizer* bSizer65;
	bSizer65 = new wxBoxSizer( wxHORIZONTAL );


	bSizer65->Add( 0, 0, 1, wxEXPAND, 5 );

	m_buttonOK = new wxButton( this, wxID_OK, _("OK"), wxDefaultPosition, wxSize( 100,32 ), 0 );
	bSizer65->Add( m_buttonOK, 0, wxALL, 5 );

	bSizer64->Add( bSizer65, 0, wxEXPAND, 5 );

	this->SetSizer( bSizer64 );
	this->Layout();

	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( CTxDetailsDialogBase::OnClose ) );
	m_buttonOK->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CTxDetailsDialogBase::OnButtonOK ), NULL, this );
}

CTxDetailsDialogBase::~CTxDetailsDialogBase()
{
	// Disconnect Events
	this->Disconnect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( CTxDetailsDialogBase::OnClose ) );
	m_buttonOK->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CTxDetailsDialogBase::OnButtonOK ), NULL, this );
}

COptionsDialogBase::COptionsDialogBase( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( 680,460 ), wxDefaultSize );

	wxBoxSizer* bSizer55;
	bSizer55 = new wxBoxSizer( wxHORIZONTAL );

	m_listBox = new wxListBox( this, wxID_ANY, wxDefaultPosition, wxSize( 120,-1 ), 0, NULL, 0 );
	bSizer55->Add( m_listBox, 0, wxEXPAND|wxTOP|wxBOTTOM|wxLEFT, 8 );

	m_scrolledWindow = new wxScrolledWindow( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_scrolledWindow->SetScrollRate( 5, 5 );
	wxBoxSizer* bSizer68;
	bSizer68 = new wxBoxSizer( wxVERTICAL );

	m_panelMain = new wxPanel( m_scrolledWindow, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer69;
	bSizer69 = new wxBoxSizer( wxVERTICAL );

	m_staticText32 = new wxStaticText( m_panelMain, wxID_ANY, _("Optional transaction fee per transaction that goes to the nodes that process transactions."), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText32->Wrap( 360 );
	m_staticText32->Hide();

	bSizer69->Add( m_staticText32, 0, wxALL, 5 );

	wxFlexGridSizer* fgSizer96;
	fgSizer96 = new wxFlexGridSizer( 0, 2, 0, 0 );

	m_staticText31 = new wxStaticText( m_panelMain, wxID_ANY, _("Transaction &fee:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText31->Wrap( -1 );
	m_staticText31->Hide();

	fgSizer96->Add( m_staticText31, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxBOTTOM|wxLEFT, 5 );

	m_textCtrlTransactionFee = new wxTextCtrl( m_panelMain, wxID_TRANSACTIONFEE, wxEmptyString, wxDefaultPosition, wxSize( 70,-1 ), 0 );
	m_textCtrlTransactionFee->Hide();

	fgSizer96->Add( m_textCtrlTransactionFee, 0, wxALL|wxALIGN_CENTER_VERTICAL, 5 );

	bSizer69->Add( fgSizer96, 0, wxLEFT, 5 );

	wxBoxSizer* bSizer71;
	bSizer71 = new wxBoxSizer( wxHORIZONTAL );

	m_checkBoxLimitProcessors = new wxCheckBox( m_panelMain, wxID_ANY, _("&Limit coin generation to"), wxDefaultPosition, wxDefaultSize, 0 );

	bSizer71->Add( m_checkBoxLimitProcessors, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	m_spinCtrlLimitProcessors = new wxSpinCtrl( m_panelMain, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize( 110,-1 ), wxSP_ARROW_KEYS, 1, 999, 1 );
	bSizer71->Add( m_spinCtrlLimitProcessors, 0, wxALIGN_CENTER_VERTICAL, 5 );

	m_staticText35 = new wxStaticText( m_panelMain, wxID_ANY, _("processors"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText35->Wrap( -1 );
	bSizer71->Add( m_staticText35, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	bSizer69->Add( bSizer71, 0, wxLEFT, 5 );

	m_checkBoxStartOnSystemStartup = new wxCheckBox( m_panelMain, wxID_ANY, _("&Start Bitok on system startup"), wxDefaultPosition, wxDefaultSize, 0 );

	bSizer69->Add( m_checkBoxStartOnSystemStartup, 0, wxALL|wxLEFT, 10 );

	m_checkBoxMinimizeToTray = new wxCheckBox( m_panelMain, wxID_ANY, _("&Minimize to the tray instead of the taskbar"), wxDefaultPosition, wxDefaultSize, 0 );

	bSizer69->Add( m_checkBoxMinimizeToTray, 0, wxALL|wxLEFT, 10 );

	m_checkBoxMinimizeOnClose = new wxCheckBox( m_panelMain, wxID_ANY, _("M&inimize to the tray on close"), wxDefaultPosition, wxDefaultSize, 0 );

	bSizer69->Add( m_checkBoxMinimizeOnClose, 0, wxALL|wxLEFT, 10 );

	m_checkBoxUseProxy = new wxCheckBox( m_panelMain, wxID_ANY, _("&Connect through socks4 proxy:"), wxDefaultPosition, wxDefaultSize, 0 );

	bSizer69->Add( m_checkBoxUseProxy, 0, wxALL|wxLEFT, 10 );

	wxBoxSizer* bSizer76;
	bSizer76 = new wxBoxSizer( wxHORIZONTAL );

	bSizer76->Add( 18, 0, 0, 0, 5 );

	m_staticTextProxyIP = new wxStaticText( m_panelMain, wxID_ANY, _("Proxy &IP:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextProxyIP->Wrap( -1 );
	bSizer76->Add( m_staticTextProxyIP, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	m_textCtrlProxyIP = new wxTextCtrl( m_panelMain, wxID_PROXYIP, wxEmptyString, wxDefaultPosition, wxSize( 160,-1 ), 0 );
	m_textCtrlProxyIP->SetMaxLength( 15 );
	bSizer76->Add( m_textCtrlProxyIP, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	m_staticTextProxyPort = new wxStaticText( m_panelMain, wxID_ANY, _("&Port:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextProxyPort->Wrap( -1 );
	bSizer76->Add( m_staticTextProxyPort, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	m_textCtrlProxyPort = new wxTextCtrl( m_panelMain, wxID_PROXYPORT, wxEmptyString, wxDefaultPosition, wxSize( 70,-1 ), 0 );
	m_textCtrlProxyPort->SetMaxLength( 5 );
	bSizer76->Add( m_textCtrlProxyPort, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5 );

	bSizer69->Add( bSizer76, 0, wxLEFT, 5 );

	m_panelMain->SetSizer( bSizer69 );
	m_panelMain->Layout();
	bSizer69->Fit( m_panelMain );
	bSizer68->Add( m_panelMain, 0, wxEXPAND|wxLEFT, 10 );

	m_panelTest2 = new wxPanel( m_scrolledWindow, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	m_panelTest2->Hide();

	wxBoxSizer* bSizer70;
	bSizer70 = new wxBoxSizer( wxVERTICAL );

	m_staticText321 = new wxStaticText( m_panelTest2, wxID_ANY, _("// [don't translate this] Test panel 2 for future expansion"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText321->Wrap( -1 );
	bSizer70->Add( m_staticText321, 0, wxALL, 5 );

	m_staticText69 = new wxStaticText( m_panelTest2, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText69->Wrap( -1 );
	bSizer70->Add( m_staticText69, 0, wxALL, 5 );

	m_panelTest2->SetSizer( bSizer70 );
	m_panelTest2->Layout();
	bSizer70->Fit( m_panelTest2 );
	bSizer68->Add( m_panelTest2, 0, wxEXPAND|wxLEFT, 10 );

	m_scrolledWindow->SetSizer( bSizer68 );
	m_scrolledWindow->Layout();
	bSizer68->Fit( m_scrolledWindow );
	bSizer55->Add( m_scrolledWindow, 1, wxEXPAND|wxALL, 5 );

	wxBoxSizer* bSizerOuter;
	bSizerOuter = new wxBoxSizer( wxVERTICAL );

	bSizerOuter->Add( bSizer55, 1, wxEXPAND, 0 );

	wxBoxSizer* bSizer56;
	bSizer56 = new wxBoxSizer( wxHORIZONTAL );

	bSizer56->Add( 0, 0, 1, wxEXPAND, 5 );

	m_buttonOK = new wxButton( this, wxID_OK, _("OK"), wxDefaultPosition, wxSize( 90,32 ), 0 );
	bSizer56->Add( m_buttonOK, 0, wxALL, 5 );

	m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( 90,32 ), 0 );
	bSizer56->Add( m_buttonCancel, 0, wxALL, 5 );

	m_buttonApply = new wxButton( this, wxID_APPLY, _("&Apply"), wxDefaultPosition, wxSize( 90,32 ), 0 );
	bSizer56->Add( m_buttonApply, 0, wxALL, 5 );

	bSizerOuter->Add( bSizer56, 0, wxEXPAND|wxBOTTOM, 4 );

	this->SetSizer( bSizerOuter );
	this->Layout();

	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( COptionsDialogBase::OnClose ) );
	m_listBox->Connect( wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler( COptionsDialogBase::OnListBox ), NULL, this );
	m_textCtrlTransactionFee->Connect( wxEVT_KILL_FOCUS, wxFocusEventHandler( COptionsDialogBase::OnKillFocusTransactionFee ), NULL, this );
	m_checkBoxLimitProcessors->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( COptionsDialogBase::OnCheckBoxLimitProcessors ), NULL, this );
	m_checkBoxMinimizeToTray->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( COptionsDialogBase::OnCheckBoxMinimizeToTray ), NULL, this );
	m_checkBoxUseProxy->Connect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( COptionsDialogBase::OnCheckBoxUseProxy ), NULL, this );
	m_textCtrlProxyIP->Connect( wxEVT_KILL_FOCUS, wxFocusEventHandler( COptionsDialogBase::OnKillFocusProxy ), NULL, this );
	m_textCtrlProxyPort->Connect( wxEVT_KILL_FOCUS, wxFocusEventHandler( COptionsDialogBase::OnKillFocusProxy ), NULL, this );
	m_buttonOK->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( COptionsDialogBase::OnButtonOK ), NULL, this );
	m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( COptionsDialogBase::OnButtonCancel ), NULL, this );
	m_buttonApply->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( COptionsDialogBase::OnButtonApply ), NULL, this );
}

COptionsDialogBase::~COptionsDialogBase()
{
	// Disconnect Events
	this->Disconnect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( COptionsDialogBase::OnClose ) );
	m_listBox->Disconnect( wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler( COptionsDialogBase::OnListBox ), NULL, this );
	m_textCtrlTransactionFee->Disconnect( wxEVT_KILL_FOCUS, wxFocusEventHandler( COptionsDialogBase::OnKillFocusTransactionFee ), NULL, this );
	m_checkBoxLimitProcessors->Disconnect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( COptionsDialogBase::OnCheckBoxLimitProcessors ), NULL, this );
	m_checkBoxMinimizeToTray->Disconnect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( COptionsDialogBase::OnCheckBoxMinimizeToTray ), NULL, this );
	m_checkBoxUseProxy->Disconnect( wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler( COptionsDialogBase::OnCheckBoxUseProxy ), NULL, this );
	m_textCtrlProxyIP->Disconnect( wxEVT_KILL_FOCUS, wxFocusEventHandler( COptionsDialogBase::OnKillFocusProxy ), NULL, this );
	m_textCtrlProxyPort->Disconnect( wxEVT_KILL_FOCUS, wxFocusEventHandler( COptionsDialogBase::OnKillFocusProxy ), NULL, this );
	m_buttonOK->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( COptionsDialogBase::OnButtonOK ), NULL, this );
	m_buttonCancel->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( COptionsDialogBase::OnButtonCancel ), NULL, this );
	m_buttonApply->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( COptionsDialogBase::OnButtonApply ), NULL, this );
}

CAboutDialogBase::CAboutDialogBase( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( 580,460 ), wxDefaultSize );

	wxBoxSizer* bSizerOuter;
	bSizerOuter = new wxBoxSizer( wxVERTICAL );

	bSizerOuter->Add( 0, 10, 0, wxEXPAND, 5 );

	wxBoxSizer* bSizerContent;
	bSizerContent = new wxBoxSizer( wxHORIZONTAL );

	wxBitmap aboutBmp( about_xpm );
	if ( aboutBmp.GetWidth() > 1 && aboutBmp.GetHeight() > 1 )
	{
		m_bitmap = new wxStaticBitmap( this, wxID_ANY, aboutBmp, wxDefaultPosition, wxDefaultSize, 0 );
		bSizerContent->Add( m_bitmap, 0, wxALL|wxALIGN_TOP, 15 );
	}
	else
	{
		m_bitmap = NULL;
	}

	wxBoxSizer* bSizerText;
	bSizerText = new wxBoxSizer( wxVERTICAL );

	m_staticText40 = new wxStaticText( this, wxID_ANY, _("\nBitok"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText40->Wrap( -1 );
	m_staticText40->SetFont( wxFont( 14, 70, 90, 92, false, wxEmptyString ) );

	bSizerText->Add( m_staticText40, 0, wxTOP|wxRIGHT|wxLEFT, 5 );

	m_staticTextVersion = new wxStaticText( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextVersion->Wrap( -1 );
	bSizerText->Add( m_staticTextVersion, 0, wxTOP|wxRIGHT|wxLEFT, 5 );

	bSizerText->Add( 0, 8, 0, wxEXPAND, 5 );

	m_staticTextMain = new wxStaticText( this, wxID_ANY, _("Copyright (c) 2009-2010 Satoshi Nakamoto\nCopyright (c) 2026-2026 Tom Elvis Jedusor\n\nThis is experimental software.\n\nDistributed under the MIT/X11 software license, see the accompanying file license.txt or http://www.opensource.org/licenses/mit-license.php.\n\nThis product includes software developed by the OpenSSL Project for use in \nthe OpenSSL Toolkit (http://www.openssl.org/) and cryptographic software \nwritten by Eric Young (eay@cryptsoft.com)."), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextMain->Wrap( 380 );
	bSizerText->Add( m_staticTextMain, 1, wxALL|wxEXPAND, 5 );

	bSizerContent->Add( bSizerText, 1, wxEXPAND, 5 );

	bSizerOuter->Add( bSizerContent, 1, wxEXPAND|wxRIGHT|wxLEFT, 5 );

	wxBoxSizer* bSizer61;
	bSizer61 = new wxBoxSizer( wxHORIZONTAL );

	bSizer61->Add( 0, 0, 1, wxEXPAND, 5 );

	m_buttonOK = new wxButton( this, wxID_OK, _("OK"), wxDefaultPosition, wxSize( 100,32 ), 0 );
	m_buttonOK->SetDefault();
	bSizer61->Add( m_buttonOK, 0, wxALL, 5 );

	bSizerOuter->Add( bSizer61, 0, wxEXPAND|wxBOTTOM, 5 );

	this->SetSizer( bSizerOuter );
	this->Layout();

	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( CAboutDialogBase::OnClose ) );
	m_buttonOK->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CAboutDialogBase::OnButtonOK ), NULL, this );
}

CAboutDialogBase::~CAboutDialogBase()
{
	// Disconnect Events
	this->Disconnect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( CAboutDialogBase::OnClose ) );
	m_buttonOK->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CAboutDialogBase::OnButtonOK ), NULL, this );
}

CSendDialogBase::CSendDialogBase( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( 500,220 ), wxDefaultSize );

	wxBoxSizer* bSizer21;
	bSizer21 = new wxBoxSizer( wxVERTICAL );

	bSizer21->Add( 0, 10, 0, wxEXPAND, 5 );

	m_staticTextInstructions = new wxStaticText( this, wxID_ANY, _("Enter a Bitok address or ok-address"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextInstructions->Wrap( -1 );
	m_staticTextInstructions->SetFont( wxFont( 9, 70, 90, 90, false, wxEmptyString ) );
	bSizer21->Add( m_staticTextInstructions, 0, wxLEFT|wxRIGHT, 14 );

	bSizer21->Add( 0, 8, 0, wxEXPAND, 5 );

	wxFlexGridSizer* fgSizer1;
	fgSizer1 = new wxFlexGridSizer( 0, 2, 0, 0 );
	fgSizer1->AddGrowableCol( 1 );
	fgSizer1->SetFlexibleDirection( wxHORIZONTAL );
	fgSizer1->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_NONE );

	wxBoxSizer* bSizerPayToLabel;
	bSizerPayToLabel = new wxBoxSizer( wxHORIZONTAL );

	m_bitmapCheckMark = new wxStaticBitmap( this, wxID_ANY, wxBitmap( check_xpm ), wxDefaultPosition, wxSize( 14,14 ), 0 );
	bSizerPayToLabel->Add( m_bitmapCheckMark, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 4 );

	m_staticText36 = new wxStaticText( this, wxID_ANY, _("Pay &To:"), wxDefaultPosition, wxSize( -1,-1 ), 0 );
	m_staticText36->Wrap( -1 );
	bSizerPayToLabel->Add( m_staticText36, 0, wxALIGN_CENTER_VERTICAL, 5 );

	fgSizer1->Add( bSizerPayToLabel, 0, wxALIGN_CENTER_VERTICAL|wxLEFT, 10 );

	wxBoxSizer* bSizerPayToField;
	bSizerPayToField = new wxBoxSizer( wxHORIZONTAL );

	m_textCtrlAddress = new wxTextCtrl( this, wxID_TEXTCTRLPAYTO, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	bSizerPayToField->Add( m_textCtrlAddress, 1, wxALIGN_CENTER_VERTICAL|wxLEFT|wxRIGHT, 4 );

	m_buttonPaste = new wxButton( this, wxID_BUTTONPASTE, _("&Paste"), wxDefaultPosition, wxSize( 70,26 ), 0 );
	bSizerPayToField->Add( m_buttonPaste, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 4 );

	m_buttonAddress = new wxButton( this, wxID_BUTTONADDRESSBOOK, _("Address &Book..."), wxDefaultPosition, wxSize( 120,26 ), 0 );
	bSizerPayToField->Add( m_buttonAddress, 0, wxALIGN_CENTER_VERTICAL|wxRIGHT, 10 );

	fgSizer1->Add( bSizerPayToField, 1, wxEXPAND|wxALIGN_CENTER_VERTICAL, 5 );

	m_staticText19 = new wxStaticText( this, wxID_ANY, _("&Amount:"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText19->Wrap( -1 );
	fgSizer1->Add( m_staticText19, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxLEFT, 10 );

	m_textCtrlAmount = new wxTextCtrl( this, wxID_TEXTCTRLAMOUNT, wxEmptyString, wxDefaultPosition, wxSize( 170,-1 ), 0 );
	m_textCtrlAmount->SetMaxLength( 20 );
	m_textCtrlAmount->SetFont( wxFont( 10, 70, 90, 90, false, wxEmptyString ) );
	fgSizer1->Add( m_textCtrlAmount, 0, wxALIGN_CENTER_VERTICAL|wxTOP|wxLEFT, 10 );

	bSizer21->Add( fgSizer1, 0, wxEXPAND, 5 );

	bSizer21->Add( 0, 0, 1, wxEXPAND, 5 );

	wxBoxSizer* bSizer23;
	bSizer23 = new wxBoxSizer( wxHORIZONTAL );

	bSizer23->Add( 0, 0, 1, wxEXPAND, 5 );

	m_buttonSend = new wxButton( this, wxID_ANY, _("&Send"), wxDefaultPosition, wxSize( 100,32 ), 0 );
	m_buttonSend->SetFont( wxFont( wxNORMAL_FONT->GetPointSize(), 70, 90, 92, false, wxEmptyString ) );
	bSizer23->Add( m_buttonSend, 0, wxALL, 5 );

	m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( 100,32 ), 0 );
	bSizer23->Add( m_buttonCancel, 0, wxALL, 5 );

	bSizer21->Add( bSizer23, 0, wxEXPAND|wxBOTTOM, 5 );

	this->SetSizer( bSizer21 );
	this->Layout();

	// Connect Events
	m_textCtrlAddress->Connect( wxEVT_KEY_DOWN, wxKeyEventHandler( CSendDialogBase::OnKeyDown ), NULL, this );
	m_textCtrlAddress->Connect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( CSendDialogBase::OnTextAddress ), NULL, this );
	m_buttonPaste->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CSendDialogBase::OnButtonPaste ), NULL, this );
	m_buttonAddress->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CSendDialogBase::OnButtonAddressBook ), NULL, this );
	m_textCtrlAmount->Connect( wxEVT_KILL_FOCUS, wxFocusEventHandler( CSendDialogBase::OnKillFocusAmount ), NULL, this );
	m_buttonSend->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CSendDialogBase::OnButtonSend ), NULL, this );
	m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CSendDialogBase::OnButtonCancel ), NULL, this );
}

CSendDialogBase::~CSendDialogBase()
{
	// Disconnect Events
	m_textCtrlAddress->Disconnect( wxEVT_KEY_DOWN, wxKeyEventHandler( CSendDialogBase::OnKeyDown ), NULL, this );
	m_textCtrlAddress->Disconnect( wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler( CSendDialogBase::OnTextAddress ), NULL, this );
	m_buttonPaste->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CSendDialogBase::OnButtonPaste ), NULL, this );
	m_buttonAddress->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CSendDialogBase::OnButtonAddressBook ), NULL, this );
	m_textCtrlAmount->Disconnect( wxEVT_KILL_FOCUS, wxFocusEventHandler( CSendDialogBase::OnKillFocusAmount ), NULL, this );
	m_buttonSend->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CSendDialogBase::OnButtonSend ), NULL, this );
	m_buttonCancel->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CSendDialogBase::OnButtonCancel ), NULL, this );
}

CSendingDialogBase::CSendingDialogBase( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( 400,150 ), wxDefaultSize );

	wxBoxSizer* bSizer77;
	bSizer77 = new wxBoxSizer( wxVERTICAL );

	m_staticTextSending = new wxStaticText( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE );
	m_staticTextSending->Wrap( -1 );
	bSizer77->Add( m_staticTextSending, 0, wxALIGN_CENTER_HORIZONTAL|wxTOP|wxRIGHT|wxLEFT, 8 );

	m_textCtrlStatus = new wxTextCtrl( this, wxID_ANY, _("\n\nConnecting..."), wxDefaultPosition, wxDefaultSize, wxTE_CENTRE|wxTE_MULTILINE|wxTE_NO_VSCROLL|wxTE_READONLY|wxNO_BORDER );
	bSizer77->Add( m_textCtrlStatus, 1, wxEXPAND|wxALL, 8 );

	wxBoxSizer* bSizer79;
	bSizer79 = new wxBoxSizer( wxHORIZONTAL );


	bSizer79->Add( 0, 0, 1, wxEXPAND, 5 );

	m_buttonOK = new wxButton( this, wxID_OK, _("OK"), wxDefaultPosition, wxSize( 90,32 ), 0 );
	m_buttonOK->Enable( false );

	bSizer79->Add( m_buttonOK, 0, wxALL, 5 );

	m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( 90,32 ), 0 );
	bSizer79->Add( m_buttonCancel, 0, wxALL, 5 );

	bSizer77->Add( bSizer79, 0, wxEXPAND, 5 );

	this->SetSizer( bSizer77 );
	this->Layout();

	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( CSendingDialogBase::OnClose ) );
	this->Connect( wxEVT_PAINT, wxPaintEventHandler( CSendingDialogBase::OnPaint ) );
	m_buttonOK->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CSendingDialogBase::OnButtonOK ), NULL, this );
	m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CSendingDialogBase::OnButtonCancel ), NULL, this );
}

CSendingDialogBase::~CSendingDialogBase()
{
	// Disconnect Events
	this->Disconnect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( CSendingDialogBase::OnClose ) );
	this->Disconnect( wxEVT_PAINT, wxPaintEventHandler( CSendingDialogBase::OnPaint ) );
	m_buttonOK->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CSendingDialogBase::OnButtonOK ), NULL, this );
	m_buttonCancel->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CSendingDialogBase::OnButtonCancel ), NULL, this );
}

CYourAddressDialogBase::CYourAddressDialogBase( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( 550,350 ), wxDefaultSize );

	wxBoxSizer* bSizer58;
	bSizer58 = new wxBoxSizer( wxVERTICAL );

	m_staticText45 = new wxStaticText( this, wxID_ANY, _("These are your Bitok addresses for receiving payments.  You may want to give a different one to each sender so you can keep track of who is paying you.  The highlighted address is displayed in the main window."), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText45->Wrap( 590 );
	bSizer58->Add( m_staticText45, 0, wxALL, 10 );

	m_listCtrl = new wxListCtrl( this, wxID_LISTCTRL, wxDefaultPosition, wxDefaultSize, wxLC_NO_SORT_HEADER|wxLC_REPORT|wxLC_SORT_ASCENDING );
	bSizer58->Add( m_listCtrl, 1, wxALL|wxEXPAND, 5 );

	wxBoxSizer* bSizer59;
	bSizer59 = new wxBoxSizer( wxHORIZONTAL );

	bSizer59->Add( 0, 0, 1, wxEXPAND, 5 );

	m_buttonRename = new wxButton( this, wxID_BUTTONRENAME, _("&Edit..."), wxDefaultPosition, wxSize( 90,32 ), 0 );
	bSizer59->Add( m_buttonRename, 0, wxALL, 5 );

	m_buttonNew = new wxButton( this, wxID_BUTTONNEW, _("&New Address..."), wxDefaultPosition, wxSize( 130,32 ), 0 );
	bSizer59->Add( m_buttonNew, 0, wxALL, 5 );

	m_buttonCopy = new wxButton( this, wxID_BUTTONCOPY, _("&Copy to Clipboard"), wxDefaultPosition, wxSize( 140,32 ), 0 );
	bSizer59->Add( m_buttonCopy, 0, wxALL, 5 );

	m_buttonOK = new wxButton( this, wxID_OK, _("OK"), wxDefaultPosition, wxSize( 90,32 ), 0 );
	bSizer59->Add( m_buttonOK, 0, wxALL, 5 );

	m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( 90,32 ), 0 );
	m_buttonCancel->Hide();

	bSizer59->Add( m_buttonCancel, 0, wxALL, 5 );

	bSizer58->Add( bSizer59, 0, wxEXPAND, 5 );

	this->SetSizer( bSizer58 );
	this->Layout();

	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( CYourAddressDialogBase::OnClose ) );
	m_listCtrl->Connect( wxEVT_COMMAND_LIST_END_LABEL_EDIT, wxListEventHandler( CYourAddressDialogBase::OnListEndLabelEdit ), NULL, this );
	m_listCtrl->Connect( wxEVT_COMMAND_LIST_ITEM_ACTIVATED, wxListEventHandler( CYourAddressDialogBase::OnListItemActivated ), NULL, this );
	m_listCtrl->Connect( wxEVT_COMMAND_LIST_ITEM_SELECTED, wxListEventHandler( CYourAddressDialogBase::OnListItemSelected ), NULL, this );
	m_buttonRename->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CYourAddressDialogBase::OnButtonRename ), NULL, this );
	m_buttonNew->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CYourAddressDialogBase::OnButtonNew ), NULL, this );
	m_buttonCopy->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CYourAddressDialogBase::OnButtonCopy ), NULL, this );
	m_buttonOK->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CYourAddressDialogBase::OnButtonOK ), NULL, this );
	m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CYourAddressDialogBase::OnButtonCancel ), NULL, this );
}

CYourAddressDialogBase::~CYourAddressDialogBase()
{
	// Disconnect Events
	this->Disconnect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( CYourAddressDialogBase::OnClose ) );
	m_listCtrl->Disconnect( wxEVT_COMMAND_LIST_END_LABEL_EDIT, wxListEventHandler( CYourAddressDialogBase::OnListEndLabelEdit ), NULL, this );
	m_listCtrl->Disconnect( wxEVT_COMMAND_LIST_ITEM_ACTIVATED, wxListEventHandler( CYourAddressDialogBase::OnListItemActivated ), NULL, this );
	m_listCtrl->Disconnect( wxEVT_COMMAND_LIST_ITEM_SELECTED, wxListEventHandler( CYourAddressDialogBase::OnListItemSelected ), NULL, this );
	m_buttonRename->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CYourAddressDialogBase::OnButtonRename ), NULL, this );
	m_buttonNew->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CYourAddressDialogBase::OnButtonNew ), NULL, this );
	m_buttonCopy->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CYourAddressDialogBase::OnButtonCopy ), NULL, this );
	m_buttonOK->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CYourAddressDialogBase::OnButtonOK ), NULL, this );
	m_buttonCancel->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CYourAddressDialogBase::OnButtonCancel ), NULL, this );
}

CAddressBookDialogBase::CAddressBookDialogBase( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxSize( 550,350 ), wxDefaultSize );

	wxBoxSizer* bSizer85;
	bSizer85 = new wxBoxSizer( wxVERTICAL );

	m_notebook = new wxNotebook( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	m_notebook->SetFont( wxFont( 9, 70, 90, 90, false, wxEmptyString ) );

	m_panelSending = new wxPanel( m_notebook, wxID_PANELSENDING, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer86;
	bSizer86 = new wxBoxSizer( wxVERTICAL );

	m_staticText55 = new wxStaticText( m_panelSending, wxID_ANY, _("Bitok Address"), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText55->Wrap( -1 );
	m_staticText55->Hide();

	bSizer86->Add( m_staticText55, 0, wxTOP|wxRIGHT|wxLEFT, 5 );

	m_listCtrlSending = new wxListCtrl( m_panelSending, wxID_LISTCTRLSENDING, wxDefaultPosition, wxDefaultSize, wxLC_NO_SORT_HEADER|wxLC_REPORT|wxLC_SORT_ASCENDING );
	bSizer86->Add( m_listCtrlSending, 1, wxALL|wxEXPAND, 4 );

	m_panelSending->SetSizer( bSizer86 );
	m_panelSending->Layout();
	bSizer86->Fit( m_panelSending );
	m_notebook->AddPage( m_panelSending, _("Sending"), true );

	m_panelReceiving = new wxPanel( m_notebook, wxID_PANELRECEIVING, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer87;
	bSizer87 = new wxBoxSizer( wxVERTICAL );

	m_staticText45 = new wxStaticText( m_panelReceiving, wxID_ANY, _("These are your Bitok addresses for receiving payments."), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticText45->Wrap( -1 );
	m_staticText45->SetFont( wxFont( 8, 70, 90, 90, false, wxEmptyString ) );
	bSizer87->Add( m_staticText45, 0, wxTOP|wxRIGHT|wxLEFT, 6 );

	m_listCtrlReceiving = new wxListCtrl( m_panelReceiving, wxID_LISTCTRLRECEIVING, wxDefaultPosition, wxDefaultSize, wxLC_NO_SORT_HEADER|wxLC_REPORT|wxLC_SORT_ASCENDING );
	bSizer87->Add( m_listCtrlReceiving, 1, wxALL|wxEXPAND, 4 );

	m_panelReceiving->SetSizer( bSizer87 );
	m_panelReceiving->Layout();
	bSizer87->Fit( m_panelReceiving );
	m_notebook->AddPage( m_panelReceiving, _("Receiving"), false );

	m_panelOkAddresses = new wxPanel( m_notebook, wxID_PANELOKADDRESSES, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* bSizer88;
	bSizer88 = new wxBoxSizer( wxVERTICAL );

	m_staticTextOkInfo = new wxStaticText( m_panelOkAddresses, wxID_ANY, _("ok-Addresses are private stealth addresses. Share them to receive payments without exposing your balance."), wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextOkInfo->Wrap( -1 );
	m_staticTextOkInfo->SetFont( wxFont( 8, 70, 90, 90, false, wxEmptyString ) );
	bSizer88->Add( m_staticTextOkInfo, 0, wxTOP|wxRIGHT|wxLEFT, 6 );

	m_listCtrlOkAddresses = new wxListCtrl( m_panelOkAddresses, wxID_LISTCTRLOKADDRESSES, wxDefaultPosition, wxDefaultSize, wxLC_NO_SORT_HEADER|wxLC_REPORT|wxLC_SORT_ASCENDING );
	bSizer88->Add( m_listCtrlOkAddresses, 1, wxALL|wxEXPAND, 4 );

	m_panelOkAddresses->SetSizer( bSizer88 );
	m_panelOkAddresses->Layout();
	bSizer88->Fit( m_panelOkAddresses );
	m_notebook->AddPage( m_panelOkAddresses, _("ok-Addresses"), false );

	bSizer85->Add( m_notebook, 1, wxEXPAND|wxALL, 4 );

	wxBoxSizer* bSizer89;
	bSizer89 = new wxBoxSizer( wxHORIZONTAL );

	m_buttonDelete = new wxButton( this, wxID_BUTTONDELETE, _("&Delete"), wxDefaultPosition, wxSize( 90,32 ), 0 );
	bSizer89->Add( m_buttonDelete, 0, wxALL, 5 );

	bSizer89->Add( 0, 0, 1, wxEXPAND, 5 );

	m_buttonCopy = new wxButton( this, wxID_BUTTONCOPY, _("&Copy to Clipboard"), wxDefaultPosition, wxSize( 140,32 ), 0 );
	bSizer89->Add( m_buttonCopy, 0, wxALL, 5 );

	m_buttonExport = new wxButton( this, wxID_BUTTONEXPORTKEY, _("Export &Key"), wxDefaultPosition, wxSize( 110,32 ), 0 );
	bSizer89->Add( m_buttonExport, 0, wxALL, 5 );

	m_buttonEdit = new wxButton( this, wxID_BUTTONEDIT, _("&Edit..."), wxDefaultPosition, wxSize( 90,32 ), 0 );
	bSizer89->Add( m_buttonEdit, 0, wxALL, 5 );

	m_buttonNew = new wxButton( this, wxID_BUTTONNEW, _("&New Address..."), wxDefaultPosition, wxSize( 130,32 ), 0 );
	bSizer89->Add( m_buttonNew, 0, wxALL, 5 );

	m_buttonOK = new wxButton( this, wxID_OK, _("OK"), wxDefaultPosition, wxSize( 90,32 ), 0 );
	bSizer89->Add( m_buttonOK, 0, wxALL, 5 );

	m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( 90,32 ), 0 );
	bSizer89->Add( m_buttonCancel, 0, wxALL, 5 );

	bSizer85->Add( bSizer89, 0, wxEXPAND, 5 );

	this->SetSizer( bSizer85 );
	this->Layout();

	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( CAddressBookDialogBase::OnClose ) );
	m_notebook->Connect( wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGED, wxNotebookEventHandler( CAddressBookDialogBase::OnNotebookPageChanged ), NULL, this );
	m_listCtrlSending->Connect( wxEVT_COMMAND_LIST_END_LABEL_EDIT, wxListEventHandler( CAddressBookDialogBase::OnListEndLabelEdit ), NULL, this );
	m_listCtrlSending->Connect( wxEVT_COMMAND_LIST_ITEM_ACTIVATED, wxListEventHandler( CAddressBookDialogBase::OnListItemActivated ), NULL, this );
	m_listCtrlSending->Connect( wxEVT_COMMAND_LIST_ITEM_SELECTED, wxListEventHandler( CAddressBookDialogBase::OnListItemSelected ), NULL, this );
	m_listCtrlReceiving->Connect( wxEVT_COMMAND_LIST_END_LABEL_EDIT, wxListEventHandler( CAddressBookDialogBase::OnListEndLabelEdit ), NULL, this );
	m_listCtrlReceiving->Connect( wxEVT_COMMAND_LIST_ITEM_ACTIVATED, wxListEventHandler( CAddressBookDialogBase::OnListItemActivated ), NULL, this );
	m_listCtrlReceiving->Connect( wxEVT_COMMAND_LIST_ITEM_SELECTED, wxListEventHandler( CAddressBookDialogBase::OnListItemSelected ), NULL, this );
	m_listCtrlOkAddresses->Connect( wxEVT_COMMAND_LIST_END_LABEL_EDIT, wxListEventHandler( CAddressBookDialogBase::OnListEndLabelEdit ), NULL, this );
	m_listCtrlOkAddresses->Connect( wxEVT_COMMAND_LIST_ITEM_ACTIVATED, wxListEventHandler( CAddressBookDialogBase::OnListItemActivated ), NULL, this );
	m_listCtrlOkAddresses->Connect( wxEVT_COMMAND_LIST_ITEM_SELECTED, wxListEventHandler( CAddressBookDialogBase::OnListItemSelected ), NULL, this );
	m_buttonDelete->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CAddressBookDialogBase::OnButtonDelete ), NULL, this );
	m_buttonCopy->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CAddressBookDialogBase::OnButtonCopy ), NULL, this );
	m_buttonExport->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CAddressBookDialogBase::OnButtonExport ), NULL, this );
	m_buttonEdit->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CAddressBookDialogBase::OnButtonEdit ), NULL, this );
	m_buttonNew->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CAddressBookDialogBase::OnButtonNew ), NULL, this );
	m_buttonOK->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CAddressBookDialogBase::OnButtonOK ), NULL, this );
	m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CAddressBookDialogBase::OnButtonCancel ), NULL, this );
}

CAddressBookDialogBase::~CAddressBookDialogBase()
{
	// Disconnect Events
	this->Disconnect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( CAddressBookDialogBase::OnClose ) );
	m_notebook->Disconnect( wxEVT_COMMAND_NOTEBOOK_PAGE_CHANGED, wxNotebookEventHandler( CAddressBookDialogBase::OnNotebookPageChanged ), NULL, this );
	m_listCtrlSending->Disconnect( wxEVT_COMMAND_LIST_END_LABEL_EDIT, wxListEventHandler( CAddressBookDialogBase::OnListEndLabelEdit ), NULL, this );
	m_listCtrlSending->Disconnect( wxEVT_COMMAND_LIST_ITEM_ACTIVATED, wxListEventHandler( CAddressBookDialogBase::OnListItemActivated ), NULL, this );
	m_listCtrlSending->Disconnect( wxEVT_COMMAND_LIST_ITEM_SELECTED, wxListEventHandler( CAddressBookDialogBase::OnListItemSelected ), NULL, this );
	m_listCtrlReceiving->Disconnect( wxEVT_COMMAND_LIST_END_LABEL_EDIT, wxListEventHandler( CAddressBookDialogBase::OnListEndLabelEdit ), NULL, this );
	m_listCtrlReceiving->Disconnect( wxEVT_COMMAND_LIST_ITEM_ACTIVATED, wxListEventHandler( CAddressBookDialogBase::OnListItemActivated ), NULL, this );
	m_listCtrlReceiving->Disconnect( wxEVT_COMMAND_LIST_ITEM_SELECTED, wxListEventHandler( CAddressBookDialogBase::OnListItemSelected ), NULL, this );
	m_listCtrlOkAddresses->Disconnect( wxEVT_COMMAND_LIST_END_LABEL_EDIT, wxListEventHandler( CAddressBookDialogBase::OnListEndLabelEdit ), NULL, this );
	m_listCtrlOkAddresses->Disconnect( wxEVT_COMMAND_LIST_ITEM_ACTIVATED, wxListEventHandler( CAddressBookDialogBase::OnListItemActivated ), NULL, this );
	m_listCtrlOkAddresses->Disconnect( wxEVT_COMMAND_LIST_ITEM_SELECTED, wxListEventHandler( CAddressBookDialogBase::OnListItemSelected ), NULL, this );
	m_buttonDelete->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CAddressBookDialogBase::OnButtonDelete ), NULL, this );
	m_buttonCopy->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CAddressBookDialogBase::OnButtonCopy ), NULL, this );
	m_buttonExport->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CAddressBookDialogBase::OnButtonExport ), NULL, this );
	m_buttonEdit->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CAddressBookDialogBase::OnButtonEdit ), NULL, this );
	m_buttonNew->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CAddressBookDialogBase::OnButtonNew ), NULL, this );
	m_buttonOK->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CAddressBookDialogBase::OnButtonOK ), NULL, this );
	m_buttonCancel->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CAddressBookDialogBase::OnButtonCancel ), NULL, this );
}

CGetTextFromUserDialogBase::CGetTextFromUserDialogBase( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxDialog( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );

	wxBoxSizer* bSizer79;
	bSizer79 = new wxBoxSizer( wxVERTICAL );

	bSizer79->Add( 0, 1, 0, wxEXPAND, 5 );

	m_staticTextMessage1 = new wxStaticText( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextMessage1->Wrap( -1 );
	bSizer79->Add( m_staticTextMessage1, 0, wxTOP|wxRIGHT|wxLEFT, 10 );

	m_textCtrl1 = new wxTextCtrl( this, wxID_TEXTCTRL, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER );
	bSizer79->Add( m_textCtrl1, 0, wxEXPAND|wxALL, 10 );

	m_staticTextMessage2 = new wxStaticText( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_staticTextMessage2->Wrap( -1 );
	m_staticTextMessage2->Hide();

	bSizer79->Add( m_staticTextMessage2, 0, wxRIGHT|wxLEFT, 10 );

	m_textCtrl2 = new wxTextCtrl( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER );
	m_textCtrl2->Hide();

	bSizer79->Add( m_textCtrl2, 0, wxEXPAND|wxALL, 10 );

	wxBoxSizer* bSizer80;
	bSizer80 = new wxBoxSizer( wxHORIZONTAL );


	bSizer80->Add( 0, 0, 1, wxEXPAND, 5 );

	m_buttonOK = new wxButton( this, wxID_OK, _("OK"), wxDefaultPosition, wxSize( 90,32 ), 0 );
	bSizer80->Add( m_buttonOK, 0, wxALL, 5 );

	m_buttonCancel = new wxButton( this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxSize( 90,32 ), 0 );
	bSizer80->Add( m_buttonCancel, 0, wxALL, 5 );

	bSizer79->Add( bSizer80, 0, wxEXPAND, 5 );

	this->SetSizer( bSizer79 );
	this->Layout();

	// Connect Events
	this->Connect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( CGetTextFromUserDialogBase::OnClose ) );
	m_textCtrl1->Connect( wxEVT_KEY_DOWN, wxKeyEventHandler( CGetTextFromUserDialogBase::OnKeyDown ), NULL, this );
	m_textCtrl2->Connect( wxEVT_KEY_DOWN, wxKeyEventHandler( CGetTextFromUserDialogBase::OnKeyDown ), NULL, this );
	m_buttonOK->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CGetTextFromUserDialogBase::OnButtonOK ), NULL, this );
	m_buttonCancel->Connect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CGetTextFromUserDialogBase::OnButtonCancel ), NULL, this );
}

CGetTextFromUserDialogBase::~CGetTextFromUserDialogBase()
{
	// Disconnect Events
	this->Disconnect( wxEVT_CLOSE_WINDOW, wxCloseEventHandler( CGetTextFromUserDialogBase::OnClose ) );
	m_textCtrl1->Disconnect( wxEVT_KEY_DOWN, wxKeyEventHandler( CGetTextFromUserDialogBase::OnKeyDown ), NULL, this );
	m_textCtrl2->Disconnect( wxEVT_KEY_DOWN, wxKeyEventHandler( CGetTextFromUserDialogBase::OnKeyDown ), NULL, this );
	m_buttonOK->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CGetTextFromUserDialogBase::OnButtonOK ), NULL, this );
	m_buttonCancel->Disconnect( wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( CGetTextFromUserDialogBase::OnButtonCancel ), NULL, this );
}
