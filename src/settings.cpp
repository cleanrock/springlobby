/* Copyright (C) 2007, 2008 The SpringLobby Team. All rights reserved. */
//
// Class: Settings
//

#ifdef __WXMSW__
#include <wx/fileconf.h>
#include <wx/filename.h>
#include <wx/wfstream.h>
#else
#include <wx/config.h>
#endif
#include <wx/filefn.h>
#include <wx/intl.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/colour.h>
#include <wx/cmndata.h>
#include <wx/font.h>

#include "nonportable.h"
#include "settings.h"
#include "utils.h"
#include "uiutils.h"
#include "battlelistfiltervalues.h"
#include "iunitsync.h"
#include "ibattle.h"

const wxColor defaultHLcolor (255,0,0);


Settings& sett()
{
    static Settings m_sett;
    return m_sett;
}

Settings::Settings()
{
  #if defined(__WXMSW__) && !defined(HAVE_WX26)
  wxString userfilepath = wxStandardPaths::Get().GetUserDataDir() + wxFileName::GetPathSeparator() + _T("springlobby.conf");
  wxString globalfilepath =  wxStandardPathsBase::Get().GetExecutablePath().BeforeLast( wxFileName::GetPathSeparator() ) + wxFileName::GetPathSeparator() + _T("springlobby.conf");

  if (  wxFileName::FileExists( userfilepath ) || !wxFileName::FileExists( globalfilepath ) || !wxFileName::IsFileWritable( globalfilepath ) )
  {
     m_chosed_path = userfilepath;
     m_portable_mode = false;
  }
  else
  {
     m_chosed_path = globalfilepath; /// portable mode, use only current app paths
     m_portable_mode = true;
  }

  // if it doesn't exist, try to create it
  if ( !wxFileName::FileExists( m_chosed_path ) )
  {
     // if directory doesn't exist, try to create it
     if ( !m_portable_mode && !wxFileName::DirExists( wxStandardPaths::Get().GetUserDataDir() ) )
         wxFileName::Mkdir( wxStandardPaths::Get().GetUserDataDir() );

     wxFileOutputStream outstream( m_chosed_path );

     if ( !outstream.IsOk() )
     {
         // TODO: error handling
     }
  }

  wxFileInputStream instream( m_chosed_path );

  if ( !instream.IsOk() )
  {
      // TODO: error handling
  }

  m_config = new myconf( instream );

  #else
  //removed temporarily because it's suspected to cause a bug with userdir creation
 // m_config = new wxConfig( _T("SpringLobby"), wxEmptyString, _T(".springlobby/springlobby.conf"), _T("springlobby.global.conf"), wxCONFIG_USE_LOCAL_FILE | wxCONFIG_USE_GLOBAL_FILE  );
  m_config = new wxConfig( _T("SpringLobby"), wxEmptyString, _T(".springlobby/springlobby.conf"), _T("springlobby.global.conf") );
  m_portable_mode = false;
  #endif
  if ( !m_config->Exists( _T("/Server") ) ) SetDefaultSettings();

  if ( !m_config->Exists( _T("/Groups") ) ) AddGroup( _("Default") );
}

Settings::~Settings()
{
    m_config->Write( _T("/General/firstrun"), false );
    #if defined(__WXMSW__) && !defined(HAVE_WX26)
    SaveSettings();
    #endif
    SetCacheVersion();
    delete m_config;
}

//! @brief Saves the settings to file
void Settings::SaveSettings()
{
  m_config->Write( _T("/General/firstrun"), false );
  SetCacheVersion();
  m_config->Flush();
  #if defined(__WXMSW__) && !defined(HAVE_WX26)
  wxFileOutputStream outstream( m_chosed_path );

  if ( !outstream.IsOk() )
  {
      // TODO: error handling
  }

  m_config->Save( outstream );
  #endif
  if (usync()->IsLoaded()) usync()->SetSpringDataPath( GetSpringDir() );
}


bool Settings::IsPortableMode()
{
  return m_portable_mode;
}


bool Settings::IsFirstRun()
{
  return m_config->Read( _T("/General/firstrun"), true );
}


void Settings::SetSettingsVersion()
{
   m_config->Write( _T("/General/SettingsVersion"), SETTINGS_VERSION );
}


unsigned int  Settings::GetSettingsVersion()
{
   return m_config->Read( _T("/General/SettingsVersion"), 0l );
}


wxString Settings::GetLobbyWriteDir()
{
  wxString sep = wxFileName::GetPathSeparator();
  wxString path = GetSpringDir() + sep + _T("lobby");
  if ( !wxFileName::DirExists( path ) )
  {
    if ( !wxFileName::Mkdir(  path  ) ) return wxEmptyString;
  }
  path += sep + _T("SpringLobby") + sep;
  if ( !wxFileName::DirExists( path ) )
  {
    if ( !wxFileName::Mkdir(  path  ) ) return wxEmptyString;
  }
  return path;
}


bool Settings::UseOldSpringLaunchMethod()
{
    return m_config->Read( _T("/Spring/UseOldLaunchMethod"), 0l );
}

bool Settings::GetNoUDP()
{
    return m_config->Read( _T("/General/NoUDP"), 0l );
}

void Settings::SetNoUDP(bool value)
{
    m_config->Write( _T("/General/NoUDP"), value );
}

int Settings::GetClientPort()
{
    return m_config->Read( _T("/General/ClientPort"), 0l );
}

void Settings::SetClientPort(int value)
{
    m_config->Write( _T("/General/ClientPort"), value );
}

bool Settings::GetShowIPAddresses()
{
    return m_config->Read( _T("/General/ShowIP"), 0l );
}

void Settings::SetShowIPAddresses(bool value)
{
    m_config->Write( _T("/General/ShowIP"), value );
}

void Settings::SetOldSpringLaunchMethod( bool value )
{
    m_config->Write( _T("/Spring/UseOldLaunchMethod"), value );
}


bool Settings::GetWebBrowserUseDefault()
{
  // See note on ambiguities, in wx/confbase.h near line 180.
  bool useDefault;
  m_config->Read(_T("/General/WebBrowserUseDefault"), &useDefault, DEFSETT_WEB_BROWSER_USE_DEFAULT);
  return useDefault;
}

void Settings::SetWebBrowserUseDefault(bool useDefault)
{
  m_config->Write(_T("/General/WebBrowserUseDefault"), useDefault);
}

wxString Settings::GetWebBrowserPath()
{
  return m_config->Read( _T("/General/WebBrowserPath"), wxEmptyString);
}


void Settings::SetWebBrowserPath( const wxString path )
{
    m_config->Write( _T("/General/WebBrowserPath"), path );
}


wxString Settings::GetCachePath()
{
  wxString path = GetLobbyWriteDir() + _T("cache") + wxFileName::GetPathSeparator();
  if ( !wxFileName::DirExists( path ) )
  {
    if ( !wxFileName::Mkdir(  path  ) ) return wxEmptyString;
  }
  return path;
}


//! @brief sets version number for the cache, needed to nuke it in case it becomes obsolete & incompatible with new versions
void Settings::SetCacheVersion()
{
    m_config->Write( _T("/General/CacheVersion"), CACHE_VERSION );
}


//! @brief returns the cache versioning number, do decide whenever to delete if becomes obsolete & incompatible with new versions
int Settings::GetCacheVersion()
{
    return m_config->Read( _T("/General/CacheVersion"), 0l );
}


//! @brief Restores default settings
void Settings::SetDefaultSettings()
{
    AddServer( WX_STRINGC(DEFSETT_DEFAULT_SERVER) );
    SetServerHost( WX_STRINGC(DEFSETT_DEFAULT_SERVER), WX_STRINGC(DEFSETT_DEFAULT_SERVER_HOST) );
    SetServerPort( WX_STRINGC(DEFSETT_DEFAULT_SERVER), DEFSETT_DEFAULT_SERVER_PORT );
    SetDefaultServer( WX_STRINGC(DEFSETT_DEFAULT_SERVER) );
}


//! @brief Checks if the server name/alias exists in the settings
bool Settings::ServerExists( const wxString& server_name )
{
    return m_config->Exists( _T("/Server/")+ server_name );
}


//! @brief Get the name/alias of the default server.
//!
//! @note Normally this will be the previously selected server. But at first run it will be a server that is set as the default.
wxString Settings::GetDefaultServer()
{
    wxString serv = WX_STRINGC(DEFSETT_DEFAULT_SERVER);
    return m_config->Read( _T("/Servers/Default"), serv );
}

void Settings::SetAutoConnect( bool do_autoconnect )
{
    m_config->Write( _T("/Server/Autoconnect"),  do_autoconnect );
}

bool Settings::GetAutoConnect( )
{
    return m_config->Read( _T("/Server/Autoconnect"), 0l );
}


//! @brief Set the name/alias of the default server.
//!
//! @param server_name the server name/alias
//! @see GetDefaultServer()
void   Settings::SetDefaultServer( const wxString& server_name )
{
    m_config->Write( _T("/Servers/Default"),  server_name );
}


//! @brief Get hostname of a server.
//!
//! @param server_name the server name/alias
wxString Settings::GetServerHost( const wxString& server_name )
{
    wxString host = WX_STRINGC(DEFSETT_DEFAULT_SERVER_HOST);
    return m_config->Read( _T("/Server/")+ server_name +_T("/host"), host );
}


//! @brief Set hostname of a server.
//!
//! @param server_name the server name/alias
//! @param value the vaule to be set
void   Settings::SetServerHost( const wxString& server_name, const wxString& value )
{
    m_config->Write( _T("/Server/")+ server_name +_T("/host"), value );
}


//! @brief Get port number of a server.
//!
//! @param server_name the server name/alias
int    Settings::GetServerPort( const wxString& server_name )
{
    return m_config->Read( _T("/Server/")+ server_name +_T("/port"), DEFSETT_DEFAULT_SERVER_PORT );
}


//! @brief Set port number of a server.
//!
//! @param server_name the server name/alias
//! @param value the vaule to be set
void   Settings::SetServerPort( const wxString& server_name, const int value )
{
    m_config->Write( _T("/Server/")+ server_name +_T("/port"), value );
}


void Settings::AddServer( const wxString& server_name )
{
    int index = GetServerIndex( server_name );
    if ( index != -1 ) return;

    index = GetNumServers();
    SetNumServers( index + 1 );

    m_config->Write( _T("/Servers/Server")+ wxString::Format( _T("%d"), index ), server_name );
}

int Settings::GetNumServers()
{
    return m_config->Read( _T("/Servers/Count"), (long)0 );
}



void Settings::SetNumServers( int num )
{
    m_config->Write( _T("/Servers/Count"), num );
}


int Settings::GetServerIndex( const wxString& server_name )
{
    int num = GetNumServers();
    for ( int i= 0; i < num; i++ )
    {
        if ( GetServerName( i ) == server_name ) return i;
    }
    return -1;
}


//! @brief Get name/alias of a server.
//!
//! @param index the server index
wxString Settings::GetServerName( int index )
{
    return m_config->Read( wxString::Format( _T("/Servers/Server%d"), index ), _T("") );
}


//! @brief Get nickname of the default account for a server.
//!
//! @param server_name the server name/alias
wxString Settings::GetServerAccountNick( const wxString& server_name )
{
    return m_config->Read( _T("/Server/")+ server_name +_T("/nick"), _T("") ) ;
}


//! @brief Set nickname of the default account for a server.
//!
//! @param server_name the server name/alias
//! @param value the vaule to be set
void Settings::SetServerAccountNick( const wxString& server_name, const wxString& value )
{
    m_config->Write( _T("/Server/")+ server_name +_T("/nick"), value );
}


//! @brief Get password of the default account for a server.
//!
//! @param server_name the server name/alias
//! @todo Implement
wxString Settings::GetServerAccountPass( const wxString& server_name )
{
    return m_config->Read( _T("/Server/")+ server_name +_T("/pass"), _T("") );
}


//! @brief Set password of the default account for a server.
//!
//! @param server_name the server name/alias
//! @param value the vaule to be set
//! @todo Implement
void   Settings::SetServerAccountPass( const wxString& server_name, const wxString& value )
{
    m_config->Write( _T("/Server/")+ server_name +_T("/pass"), value );
}


//! @brief Get if the password should be saved for a default server account.
//!
//! @param server_name the server name/alias
//! @todo Implement
bool   Settings::GetServerAccountSavePass( const wxString& server_name )
{
    return m_config->Read( _T("/Server/")+ server_name +_T("/savepass"), (long int)false );
}


//! @brief Set if the password should be saved for a default server account.
//!
//! @param server_name the server name/alias
//! @param value the vaule to be set
//! @todo Implement
void Settings::SetServerAccountSavePass( const wxString& server_name, const bool value )
{
    m_config->Write( _T("/Server/")+ server_name +_T("/savepass"), (long int)value );
}


int Settings::GetNumChannelsJoin()
{
    return m_config->Read( _T("/Channels/Count"), (long)0 );
}

void Settings::SetNumChannelsJoin( int num )
{
    m_config->Write( _T("/Channels/Count"), num );
}

void Settings::AddChannelJoin( const wxString& channel , const wxString& key )
{
    int index = GetChannelJoinIndex( channel );
    if ( index != -1 ) return;

    index = GetNumChannelsJoin();
    SetNumChannelsJoin( index + 1 );

    m_config->Write( wxString::Format( _T("/Channels/Channel%d"), index ), channel + _T(" ") + key );
}


void Settings::RemoveChannelJoin( const wxString& channel )
{
    int index = GetChannelJoinIndex( channel );
    if ( index == -1 ) return;
    int total = GetNumChannelsJoin();
    wxString LastEntry;
    m_config->Read( _T("/Channels/Channel") +  wxString::Format( _T("%d"), total - 1 ), &LastEntry);
    m_config->Write( _T("/Channels/Channel") + wxString::Format( _T("%d"), index ), LastEntry );
    m_config->DeleteEntry( _T("/Channels/Channel") + wxString::Format( _T("%d"), total - 1 ) );
    SetNumChannelsJoin( total -1 );
}


int Settings::GetChannelJoinIndex( const wxString& server_name )
{
    int num = GetNumChannelsJoin();
    for ( int i= 0; i < num; i++ )
    {
        wxString name = GetChannelJoinName( i );
        name = name.BeforeFirst( ' ' );
        if ( name == server_name ) return i;
    }
    return -1;
}

wxString Settings::GetChannelJoinName( int index )
{
    return m_config->Read( wxString::Format( _T("/Channels/Channel%d"), index ), _T("") );
}
/************* SPRINGLOBBY WINDOW POS/SIZE   ******************/
//! @brief Get width of MainWindow.
int Settings::GetMainWindowWidth()
{
    return m_config->Read( _T("/Mainwin/width"), DEFSETT_MW_WIDTH );
}


//! @brief Set width position of MainWindow
void Settings::SetMainWindowWidth( const int value )
{
    m_config->Write( _T("/Mainwin/width"), value );
}


//! @brief Get height of MainWindow.
int Settings::GetMainWindowHeight()
{
    return m_config->Read( _T("/Mainwin/height"), DEFSETT_MW_HEIGHT );
}


//! @brief Set height position of MainWindow
void Settings::SetMainWindowHeight( const int value )
{
    m_config->Write( _T("/Mainwin/height"), value );
}


//! @brief Get top position of MainWindow.
int Settings::GetMainWindowTop()
{
    return m_config->Read( _T("/Mainwin/top"), DEFSETT_MW_TOP );
}


//! @brief Set top position of MainWindow
void Settings::SetMainWindowTop( const int value )
{
    m_config->Write( _T("/Mainwin/top"), value );
}


//! @brief Get left position of MainWindow.
int Settings::GetMainWindowLeft()
{
    return m_config->Read( _T("/Mainwin/left"), DEFSETT_MW_LEFT );
}

//! @brief Set left position of MainWindow
void Settings::SetMainWindowLeft( const int value )
{
    m_config->Write( _T("/Mainwin/left"), value );
}

wxString Settings::GetSpringDir()
{
    if ( !IsPortableMode() ) return m_config->Read( _T("/Spring/dir"), WX_STRINGC(DEFSETT_SPRING_DIR) );
    else return wxStandardPathsBase::Get().GetExecutablePath().BeforeLast( wxFileName::GetPathSeparator() );
}


void Settings::SetSpringDir( const wxString& spring_dir )
{
    m_config->Write( _T("/Spring/dir"), spring_dir );
}


bool Settings::GetUnitSyncUseDefLoc()
{
    return m_config->Read( _T("/Spring/use_unitsync_def_loc"), true );
}


void Settings::SetUnitSyncUseDefLoc( const bool usedefloc )
{
    m_config->Write( _T("/Spring/use_unitsync_def_loc"), usedefloc );
}



wxString Settings::GetUnitSyncLoc()
{
  return m_config->Read( _T("/Spring/unitsync_loc"), _T("") );
}



void Settings::SetUnitSyncLoc( const wxString& loc )
{
    m_config->Write( _T("/Spring/unitsync_loc"), loc );
}



bool Settings::GetSpringUseDefLoc()
{
    return m_config->Read( _T("/Spring/use_spring_def_loc"), true );
}



void Settings::SetSpringUseDefLoc( const bool usedefloc )
{
    m_config->Write( _T("/Spring/use_spring_def_loc"), usedefloc );
}



wxString Settings::GetSpringLoc()
{
    return m_config->Read( _T("/Spring/exec_loc"), _T("") );
}



void Settings::SetSpringLoc( const wxString& loc )
{
    m_config->Write( _T("/Spring/exec_loc"), loc );
}


wxString Settings::GetSpringUsedLoc( bool force, bool defloc )
{
    if ( IsPortableMode() ) return GetSpringDir() + wxFileName::GetPathSeparator() + _T("spring.exe");
    bool df;
    if ( force ) df = defloc;
    else df = GetSpringUseDefLoc();

    if ( df )
    {
        wxString tmp = GetSpringDir();
        if ( tmp.Last() != wxFILE_SEP_PATH ) tmp += wxFILE_SEP_PATH;
        tmp += SPRING_BIN;
        return tmp;
    }
    else
    {
        return GetSpringLoc();
    }
}

wxString Settings::GetUnitSyncUsedLoc( bool force, bool defloc )
{
    if ( IsPortableMode() ) return GetSpringDir() + wxFileName::GetPathSeparator() + _T("unitsync.dll");
    bool df;
    if ( force ) df = defloc;
    else df = GetUnitSyncUseDefLoc();

    if ( df )
    {
        wxString tmp = sett().GetSpringDir();
        if ( tmp.Last() != wxFILE_SEP_PATH ) tmp += wxFILE_SEP_PATH;
        tmp += _T("unitsync") + GetLibExtension();
        return tmp;
    }
    else
    {
        return sett().GetUnitSyncLoc();
    }
}

bool Settings::GetChatLogEnable()
{
    if (!m_config->Exists(_T("/ChatLog/chatlog_enable"))) SetChatLogEnable( false );
    return m_config->Read( _T("/ChatLog/chatlog_enable"), true );
}

void Settings::SetChatLogEnable( const bool value )
{
    m_config->Write( _T("/ChatLog/chatlog_enable"), value );
}

wxString Settings::GetChatLogLoc()
{
    wxString path;
    if ( !IsPortableMode() ) path = m_config->Read( _T("/ChatLog/chatlog_loc"), GetLobbyWriteDir() + _T("chatlog") );
    else path = GetLobbyWriteDir() + _T("chatlog");
    if ( !wxFileName::DirExists( path ) )
    {
      if ( !wxFileName::Mkdir(  path  ) ) return wxEmptyString;
    }
    return path;
}

void Settings::SetChatLogLoc( const wxString& loc )
{
    m_config->Write( _T("/ChatLog/chatlog_loc"), loc );
}

wxString Settings::GetLastHostDescription()
{
    return m_config->Read( _T("/Hosting/LastDescription"), _T("") );
}


wxString Settings::GetLastHostMod()
{
    return m_config->Read( _T("/Hosting/LastMod"), _T("") );
}


wxString Settings::GetLastHostPassword()
{
    return m_config->Read( _T("/Hosting/LastPassword"), _T("") );
}


int Settings::GetLastHostPort()
{
    return m_config->Read( _T("/Hosting/LastPort"), DEFSETT_SPRING_PORT );
}


int Settings::GetLastHostPlayerNum()
{
    return m_config->Read( _T("/Hosting/LastPlayerNum"), 4 );
}


int Settings::GetLastHostNATSetting()
{
    return m_config->Read( _T("/Hosting/LastNATSetting"), (long)0 );
}


wxString Settings::GetLastHostMap()
{
    return m_config->Read( _T("/Hosting/LastMap"), _T("") );
}

int Settings::GetLastRankLimit()
{
    return m_config->Read( _T("/Hosting/LastRank"), 0l );
}

bool Settings::GetTestHostPort()
{
    return m_config->Read( _T("/Hosting/TestHostPort"), 0l );
}

wxColour Settings::GetBattleLastColour()
{
   return  GetColorFromStrng( m_config->Read( _T("/Hosting/MyLastColour"), _T("1 1 0") ) );
}


void Settings::SetBattleLastColour( const wxColour& col )
{
  m_config->Write( _T("/Hosting/MyLastColour"), GetColorString( col ) );
}

void Settings::SetLastHostDescription( const wxString& value )
{
    m_config->Write( _T("/Hosting/LastDescription"), value );
}


void Settings::SetLastHostMod( const wxString& value )
{
    m_config->Write( _T("/Hosting/LastMod"), value );
}


void Settings::SetLastHostPassword( const wxString& value )
{
    m_config->Write( _T("/Hosting/LastPassword"), value );
}


void Settings::SetLastHostPort( int value )
{
    m_config->Write( _T("/Hosting/LastPort"), value );
}


void Settings::SetLastHostPlayerNum( int value )
{
    m_config->Write( _T("/Hosting/LastPlayerNum"), value );
}


void Settings::SetLastHostNATSetting( int value )
{
    m_config->Write( _T("/Hosting/LastNATSetting"), value );
}


void Settings::SetLastHostMap( const wxString& value )
{
    m_config->Write( _T("/Hosting/LastMap"), value );
}

void Settings::SetLastRankLimit( int rank )
{
    m_config->Write( _T("/Hosting/LastRank"), rank );
}

void Settings::SetLastAI( const wxString& ai )
{
    m_config->Write( _T("/SinglePlayer/LastAI"), ai );
}

void Settings::SetTestHostPort( bool value )
{
    m_config->Write( _T("/Hosting/TestHostPort"), value );
}




void Settings::SetBalanceMethod(int value)
{
    m_config->Write( _T("/Hosting/BalanceMethod"), value );
}
int Settings::GetBalanceMethod()
{
    return m_config->Read( _T("/Hosting/BalanceMethod"), 1l);
}

void Settings::SetBalanceClans(bool value)
{
    m_config->Write( _T("/Hosting/BalanceClans"), value );
}
bool Settings::GetBalanceClans()
{
    return m_config->Read( _T("/Hosting/BalanceClans"), true);
}

void Settings::SetBalanceStrongClans(bool value)
{
    m_config->Write( _T("/Hosting/BalanceStrongClans"), value );
}

bool Settings::GetBalanceStrongClans()
{
    return m_config->Read( _T("/Hosting/BalanceStrongClans"), 0l);
}



wxString Settings::GetLastAI()
{
    return m_config->Read( _T("/SinglePlayer/LastAI"), wxEmptyString );
}

void Settings::SetDisplayJoinLeave( bool display, const wxString& channel  )
{
    m_config->Write( _T("/Channels/DisplayJoinLeave/")  + channel, display);
}

bool Settings::GetDisplayJoinLeave( const wxString& channel  )
{
    return m_config->Read( _T("/Channels/DisplayJoinLeave/") +  channel, true);
}


void Settings::SetChatHistoryLenght( unsigned int historylines )
{
    m_config->Write( _T("/Chat/HistoryLinesLenght/"), (int)historylines);
}


unsigned int Settings::GetChatHistoryLenght()
{
    return (unsigned int)m_config->Read( _T("/Chat/HistoryLinesLenght/"), 1000);
}


void Settings::SetChatPMSoundNotificationEnabled( bool enabled )
{
  m_config->Write( _T("/Chat/PMSound"), enabled);
}


bool Settings::GetChatPMSoundNotificationEnabled()
{
  return m_config->Read( _T("/Chat/PMSound"), 1l);
}


wxColour Settings::GetChatColorNormal()
{
    return wxColour( GetColorFromStrng( m_config->Read( _T("/Chat/Colour/Normal"), _T( "0 0 0" ) ) ) );
}

void Settings::SetChatColorNormal( wxColour value )
{
    m_config->Write( _T("/Chat/Colour/Normal"), GetColorString(value) );
}

wxColour Settings::GetChatColorBackground()
{
    return wxColour( GetColorFromStrng( m_config->Read( _T("/Chat/Colour/Background"), _T( "255 255 255" ) ) ) );
}

void Settings::SetChatColorBackground( wxColour value )
{
    m_config->Write( _T("/Chat/Colour/Background"), GetColorString(value) );
}

wxColour Settings::GetChatColorHighlight()
{
    return wxColour( GetColorFromStrng( m_config->Read( _T("/Chat/Colour/Highlight"), _T( "255 0 0" ) ) ) );
}

void Settings::SetChatColorHighlight( wxColour value )
{
    m_config->Write( _T("/Chat/Colour/Highlight"), GetColorString(value) );
}

wxColour Settings::GetChatColorMine()
{
    return wxColour( GetColorFromStrng( m_config->Read( _T("/Chat/Colour/Mine"), _T( "100 100 140" ) ) ) );
}

void Settings::SetChatColorMine( wxColour value )
{
    m_config->Write( _T("/Chat/Colour/Mine"), GetColorString(value) );
}

wxColour Settings::GetChatColorNotification()
{
    return wxColour( GetColorFromStrng( m_config->Read( _T("/Chat/Colour/Notification"), _T( "255 40 40" ) ) ) );
}

void Settings::SetChatColorNotification( wxColour value )
{
    m_config->Write( _T("/Chat/Colour/Notification"), GetColorString(value) );
}

wxColour Settings::GetChatColorAction()
{
    return wxColour( GetColorFromStrng( m_config->Read( _T("/Chat/Colour/Action"), _T( "230 0 255" ) ) ) );
}

void Settings::SetChatColorAction( wxColour value )
{
    m_config->Write( _T("/Chat/Colour/Action"), GetColorString(value) );
}

wxColour Settings::GetChatColorServer()
{
    return wxColour( GetColorFromStrng( m_config->Read( _T("/Chat/Colour/Server"), _T( "0 80 128" ) ) ) );
}

void Settings::SetChatColorServer( wxColour value )
{
    m_config->Write( _T("/Chat/Colour/Server"), GetColorString(value) );
}

wxColour Settings::GetChatColorClient()
{
    return wxColour( GetColorFromStrng( m_config->Read( _T("/Chat/Colour/Client"), _T( "20 200 25" ) ) ) );
}

void Settings::SetChatColorClient( wxColour value )
{
    m_config->Write( _T("/Chat/Colour/Client"), GetColorString(value) );
}

wxColour Settings::GetChatColorJoinPart()
{
    return wxColour( GetColorFromStrng( m_config->Read( _T("/Chat/Colour/JoinPart"), _T( "0 80 0" ) ) ) );
}

void Settings::SetChatColorJoinPart( wxColour value )
{
    m_config->Write( _T("/Chat/Colour/JoinPart"), GetColorString(value) );
}

wxColour Settings::GetChatColorError()
{
    return wxColour( GetColorFromStrng( m_config->Read( _T("/Chat/Colour/Error"), _T( "128 0 0" ) ) ) );
}

void Settings::SetChatColorError( wxColour value )
{
    m_config->Write( _T("/Chat/Colour/Error"), GetColorString(value) );
}

wxColour Settings::GetChatColorTime()
{
    return wxColour( GetColorFromStrng( m_config->Read( _T("/Chat/Colour/Time"), _T( "100 100 140" ) ) ) );
}

void Settings::SetChatColorTime( wxColour value )
{
    m_config->Write( _T("/Chat/Colour/Time"), GetColorString(value) );
}

wxFont Settings::GetChatFont()
{
    wxString info = m_config->Read( _T("/Chat/Font"), wxEmptyString );
    if (info != wxEmptyString) {
        wxFont f;
        f.SetNativeFontInfo( info );
        return f;
    }
    else {
        wxFont f(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
        return f;
    }
}

void Settings::SetChatFont( wxFont value )
{
    m_config->Write( _T("/Chat/Font"), value.GetNativeFontInfoDesc() );
}


bool Settings::GetSmartScrollEnabled()
{
    return m_config->Read( _T("/Chat/SmartScrollEnabled"), true);
}

void Settings::SetSmartScrollEnabled(bool value){
  m_config->Write( _T("/Chat/SmartScrollEnabled"), value);
}

bool Settings::GetAlwaysAutoScrollOnFocusLost()
{
  return m_config->Read( _T("/Chat/AlwaysAutoScrollOnFocusLost"), true );
}

void Settings::SetAlwaysAutoScrollOnFocusLost(bool value)
{
  m_config->Write( _T("/Chat/AlwaysAutoScrollOnFocusLost"), value);
}

void Settings::SetHighlightedWords( const wxString& words )
{
  m_config->Write( _T("/Chat/HighlightedWords"), words );
}

wxString Settings::GetHighlightedWords( )
{
  return m_config->Read( _T("/Chat/HighlightedWords"), wxEmptyString );
}

void Settings::SetRequestAttOnHighlight( const bool req )
{
  m_config->Write( _T("/Chat/ReqAttOnHighlight"), req);
}

bool Settings::GetRequestAttOnHighlight( )
{
  return m_config->Read( _T("/Chat/ReqAttOnHighlight"), 0l);
}

BattleListFilterValues Settings::GetBattleFilterValues(const wxString& profile_name)
{
    BattleListFilterValues filtervalues;
    filtervalues.description =      m_config->Read( _T("/BattleFilter/")+profile_name + _T("/description"), _T("") );
    filtervalues.host =             m_config->Read( _T("/BattleFilter/")+profile_name + _T("/host"), _T("") );
    filtervalues.map=               m_config->Read( _T("/BattleFilter/")+profile_name + _T("/map"), _T("") );
    filtervalues.map_show =         m_config->Read( _T("/BattleFilter/")+profile_name + _T("/map_show"), 0L );
    filtervalues.maxplayer =        m_config->Read( _T("/BattleFilter/")+profile_name + _T("/maxplayer"), _T("All") );
    filtervalues.maxplayer_mode =   m_config->Read( _T("/BattleFilter/")+profile_name + _T("/maxplayer_mode"), _T("=") );
    filtervalues.mod =              m_config->Read( _T("/BattleFilter/")+profile_name + _T("/mod"), _T("") );
    filtervalues.mod_show =         m_config->Read( _T("/BattleFilter/")+profile_name + _T("/mod_show"), 0L );
    filtervalues.player_mode =      m_config->Read( _T("/BattleFilter/")+profile_name + _T("/player_mode"), _T("=") );
    filtervalues.player_num  =      m_config->Read( _T("/BattleFilter/")+profile_name + _T("/player_num"), _T("All") );
    filtervalues.rank =             m_config->Read( _T("/BattleFilter/")+profile_name + _T("/rank"), _T("All") );
    filtervalues.rank_mode =        m_config->Read( _T("/BattleFilter/")+profile_name + _T("/rank_mode"), _T("<") );
    filtervalues.spectator =        m_config->Read( _T("/BattleFilter/")+profile_name + _T("/spectator"), _T("All") );
    filtervalues.spectator_mode =   m_config->Read( _T("/BattleFilter/")+profile_name + _T("/spectator_mode"), _T("=") );
    filtervalues.status_full =      m_config->Read( _T("/BattleFilter/")+profile_name + _T("/status_full"), true );
    filtervalues.status_locked =    m_config->Read( _T("/BattleFilter/")+profile_name + _T("/status_locked"),true );
    filtervalues.status_open =      m_config->Read( _T("/BattleFilter/")+profile_name + _T("/status_open"), true );
    filtervalues.status_passworded= m_config->Read( _T("/BattleFilter/")+profile_name + _T("/status_passworded"), true );
    filtervalues.status_start =     m_config->Read( _T("/BattleFilter/")+profile_name + _T("/status_start"), true );
    filtervalues.highlighted_only = m_config->Read( _T("/BattleFilter/")+profile_name + _T("/highlighted_only"), 0l);
    return filtervalues;
}

void Settings::SetBattleFilterValues(const BattleListFilterValues& filtervalues, const wxString& profile_name)
{
    m_config->Write( _T("/BattleFilter/")+profile_name + _T("/description"),filtervalues.description);
    m_config->Write( _T("/BattleFilter/")+profile_name + _T("/host"),filtervalues.host );
    m_config->Write( _T("/BattleFilter/")+profile_name + _T("/map"),filtervalues.map );
    m_config->Write( _T("/BattleFilter/")+profile_name + _T("/map_show"),filtervalues.map_show );
    m_config->Write( _T("/BattleFilter/")+profile_name + _T("/maxplayer"),filtervalues.maxplayer );
    m_config->Write( _T("/BattleFilter/")+profile_name + _T("/maxplayer_mode"),filtervalues.maxplayer_mode);
    m_config->Write( _T("/BattleFilter/")+profile_name + _T("/mod"),filtervalues.mod );
    m_config->Write( _T("/BattleFilter/")+profile_name + _T("/mod_show"),filtervalues.mod_show );
    m_config->Write( _T("/BattleFilter/")+profile_name + _T("/player_mode"),filtervalues.player_mode );
    m_config->Write( _T("/BattleFilter/")+profile_name + _T("/player_num"),filtervalues.player_num );
    m_config->Write( _T("/BattleFilter/")+profile_name + _T("/rank"),filtervalues.rank );
    m_config->Write( _T("/BattleFilter/")+profile_name + _T("/rank_mode"),filtervalues.rank_mode );
    m_config->Write( _T("/BattleFilter/")+profile_name + _T("/spectator"),filtervalues.spectator );
    m_config->Write( _T("/BattleFilter/")+profile_name + _T("/spectator_mode"),filtervalues.spectator_mode );
    m_config->Write( _T("/BattleFilter/")+profile_name + _T("/status_full"),filtervalues.status_full );
    m_config->Write( _T("/BattleFilter/")+profile_name + _T("/status_locked"),filtervalues.status_locked );
    m_config->Write( _T("/BattleFilter/")+profile_name + _T("/status_open"),filtervalues.status_open );
    m_config->Write( _T("/BattleFilter/")+profile_name + _T("/status_passworded"),filtervalues.status_passworded );
    m_config->Write( _T("/BattleFilter/")+profile_name + _T("/status_start"),filtervalues.status_start );
    m_config->Write( _T("/BattleFilter/")+profile_name + _T("/highlighted_only"),filtervalues.highlighted_only );
    m_config->Write( _T("/BattleFilter/lastprofile"),profile_name);
}

bool Settings::GetFilterActivState() const
{
    return m_config->Read( _T("/BattleFilter/Active") , 0l );
}

void Settings::SetFilterActivState(const bool state)
{
    m_config->Write( _T("/BattleFilter/Active") , state );
}

bool Settings::GetDisableSpringVersionCheck()
{
    bool ret;
    m_config->Read( _T("/Spring/DisableVersionCheck"), &ret, false );
    return ret;
}

wxString Settings::GetLastFilterProfileName()
{
    return  m_config->Read( _T("/BattleFilter/lastprofile"), _T("default") );
}


unsigned int Settings::GetTorrentPort()
{
    return  (unsigned int)m_config->Read( _T("/Torrent/Port"), GetLastHostPort() );
}


void Settings::SetTorrentPort( unsigned int port )
{
  m_config->Write( _T("/Torrent/port"), (int)port );
}


int Settings::GetTorrentUploadRate()
{
    return  m_config->Read( _T("/Torrent/UploadRate"), -1 );
}


void Settings::SetTorrentUploadRate( int speed )
{
  m_config->Write( _T("/Torrent/UploadRate"), speed );
}


int Settings::GetTorrentDownloadRate()
{
    return  m_config->Read( _T("/Torrent/DownloadRate"), -1 );
}



void Settings::SetTorrentDownloadRate( int speed )
{
  m_config->Write( _T("/Torrent/DownloadRate"), speed );
}


int Settings::GetTorrentSystemSuspendMode()
{
    return  m_config->Read( _T("/Torrent/SuspendMode"), 0l );
}



void Settings::SetTorrentSystemSuspendMode( int mode )
{
  m_config->Write( _T("/Torrent/SuspendMode"), mode );
}


int Settings::GetTorrentThrottledUploadRate()
{
    return  m_config->Read( _T("/Torrent/ThrottledUploadRate"), 0l );
}


void Settings::SetTorrentThrottledUploadRate( int speed )
{
  m_config->Write( _T("/Torrent/ThrottledUploadRate"), speed );
}


int Settings::GetTorrentThrottledDownloadRate()
{
    return  m_config->Read( _T("/Torrent/ThrottledDownloadRate"), 0l );
}


void Settings::SetTorrentThrottledDownloadRate( int speed )
{
  m_config->Write( _T("/Torrent/ThrottledDownloadRate"), speed );
}


int Settings::GetTorrentSystemAutoStartMode()
{
    return  m_config->Read( _T("/Torrent/AutoStartMode"), 0l );
}


void Settings::SetTorrentSystemAutoStartMode( int mode )
{
  m_config->Write( _T("/Torrent/AutoStartMode"), mode );
}


void Settings::SetTorrentMaxConnections( int connections )
{
  m_config->Write( _T("/Torrent/MaxConnections"), connections );
}


int Settings::GetTorrentMaxConnections()
{
    return  m_config->Read( _T("/Torrent/MaxConnections"), 250 );
}

void Settings::SetTorrentListToResume( const wxArrayString& list )
{
  unsigned int TorrentCount = list.GetCount();
  m_config->DeleteGroup( _T("/Torrent/ResumeList") );
  for ( unsigned int i = 0; i < TorrentCount; i++ )
  {
    m_config->Write( _T("/Torrent/ResumeList/") + TowxString(i), list[i] );
  }
}


wxArrayString Settings::GetTorrentListToResume()
{
  wxArrayString list;
  wxString old_path = m_config->GetPath();
  m_config->SetPath( _T("/Torrent/ResumeList") );
  unsigned int TorrentCount = m_config->GetNumberOfEntries( false );
  for ( unsigned int i = 0; i < TorrentCount; i++ )
  {
    wxString ToAdd;
    if ( m_config->Read( _T("/Torrent/ResumeList/") + TowxString(i), &ToAdd ) ) list.Add( ToAdd );
  }

  m_config->SetPath( old_path );
  return list;
}


wxString Settings::GetTorrentsFolder()
{
  wxString path = GetLobbyWriteDir() +_T("torrents") + wxFileName::GetPathSeparator();
    if ( !wxFileName::DirExists( path ) )
  {
    if ( !wxFileName::Mkdir(  path  ) ) return wxEmptyString;
  }
  return path;
}


wxString Settings::GetTempStorage()
{
  return wxFileName::GetTempDir();
}


void Settings::SaveBattleMapOptions(IBattle *battle){
  if ( !battle ){
        wxLogError(_T("Settings::SaveBattleMapOptions called with null argument"));
        return;
      }
  wxString map_name=battle->GetHostMapName();
  //SetLastHostMap(map_name);
  wxString option_prefix=_T("/Hosting/Maps/")+map_name+_T("/");
  long longval;
  battle->CustomBattleOptions()->getSingleValue( _T("startpostype") , EngineOption ).ToLong( &longval );
  int start_pos_type=longval;

  m_config->Write( option_prefix+_T("startpostype"), start_pos_type);
  if(start_pos_type==ST_Choose){
    int n_rects=battle->GetNumRects();
    m_config->Write( option_prefix+_T("n_rects"), n_rects);


    for ( int i = 0; i < n_rects; ++i ) {
      BattleStartRect rect = battle->GetStartRect( i );
      if ( !rect.exist || rect.todelete ) {
        m_config->DeleteEntry(option_prefix+_T("rect_")+TowxString(i)+_T("_ally"));
        m_config->DeleteEntry(option_prefix+_T("rect_")+TowxString(i)+_T("_left"));
        m_config->DeleteEntry(option_prefix+_T("rect_")+TowxString(i)+_T("_top"));
        m_config->DeleteEntry(option_prefix+_T("rect_")+TowxString(i)+_T("_right"));
        m_config->DeleteEntry(option_prefix+_T("rect_")+TowxString(i)+_T("_bottom"));
      }else{
        m_config->Write(option_prefix+_T("rect_")+TowxString(i)+_T("_ally"), (int)rect.ally);
        m_config->Write(option_prefix+_T("rect_")+TowxString(i)+_T("_top"), (int)rect.top);
        m_config->Write(option_prefix+_T("rect_")+TowxString(i)+_T("_left"), (int)rect.left);
        m_config->Write(option_prefix+_T("rect_")+TowxString(i)+_T("_right"), (int)rect.right);
        m_config->Write(option_prefix+_T("rect_")+TowxString(i)+_T("_bottom"), (int)rect.bottom);
      }
    }
  }
}

void Settings::LoadBattleMapOptions(IBattle *battle){
  if ( !battle ){
        wxLogError(_T("Settings::LoadBattleMapOptions called with null argument"));
        return;
      }
  wxString map_name=battle->GetHostMapName();
  wxString option_prefix=_T("/Hosting/Maps/")+map_name+_T("/");
  int start_pos_type=m_config->Read(option_prefix+_T("startpostype") , 0L );
  battle->CustomBattleOptions()->setSingleOption( _T("startpostype"), TowxString(start_pos_type), EngineOption );
  if(start_pos_type==ST_Choose){

    battle->ClearStartRects();

    int n_rects=m_config->Read( option_prefix+_T("n_rects"), 0L);
/*
    for(int i=n_rects;i<battle->GetNumRects();++i){
      battle->RemoveStartRect(i);
    }*/

    for(int i=0;i<n_rects;++i){
      int ally=m_config->Read(option_prefix+_T("rect_")+TowxString(i)+_T("_ally"),-1L);
      int top=m_config->Read(option_prefix+_T("rect_")+TowxString(i)+_T("_top"),-1L);
      int left=m_config->Read(option_prefix+_T("rect_")+TowxString(i)+_T("_left"),-1L);
      int right=m_config->Read(option_prefix+_T("rect_")+TowxString(i)+_T("_right"),-1L);
      int bottom=m_config->Read(option_prefix+_T("rect_")+TowxString(i)+_T("_bottom"),-1L);
      if(ally>=0) battle->AddStartRect(ally,left,top,right,bottom);
    }
  }
}

void Settings::SetShowTooltips( bool show)
{
    m_config->Write(_T("GUI/ShowTooltips"), show );
}

bool Settings::GetShowTooltips()
{
    return m_config->Read(_T("GUI/ShowTooltips"), 1l);
}

void Settings::SaveLayout( wxString& layout_name, wxString& layout )
{
    m_config->Write( _T("/Layout/") + layout_name, layout );
}

wxString Settings::GetLayout( wxString& layout_name )
{
    return  m_config->Read( _T("/Layout/") + layout_name, _T("") );
}

void Settings::SetColumnWidth( const wxString& list_name, const int coloumn_ind, const int coloumn_width )
{
    m_config->Write(_T("GUI/ColoumnWidths/") + list_name + _T("/") + TowxString(coloumn_ind), coloumn_width );
}

int Settings::GetColumnWidth( const wxString& list_name, const int coloumn )
{
    return m_config->Read(_T("GUI/ColoumnWidths/") + list_name + _T("/") + TowxString(coloumn), columnWidthUnset);
}

void Settings::SetPeopleList( const wxArrayString& friends, const wxString& group  )
{
    unsigned int friendsCount = friends.GetCount();
    m_config->DeleteGroup( _T("/Groups/") + group + _T("/Members/") );
    for ( unsigned int i = 0; i < friendsCount ; i++ )
    {
        m_config->Write(_T("/Groups/") + group + _T("/Members/") + TowxString(i), friends[i] );
    }
}

wxArrayString Settings::GetPeopleList( const wxString& group  ) const
{
    wxArrayString list;
    wxString old_path = m_config->GetPath();
    m_config->SetPath( _T("/Groups/") + group + _T("/Members/") );
    unsigned int friendsCount  = m_config->GetNumberOfEntries( false );
    for ( unsigned int i = 0; i < friendsCount ; i++ )
    {
        wxString ToAdd;
        if ( m_config->Read( _T("/Groups/") + group + _T("/Members/") +  TowxString(i), &ToAdd ) ) list.Add( ToAdd );
    }
    m_config->SetPath( old_path );
    return list;
}

void Settings::SetGroupHLColor( const wxColor& color, const wxString& group )
{
    m_config->Write( _T("/Groups/") + group + _T("/Opts/HLColor"), GetColorString( color ) );

}

wxColor Settings::GetGroupHLColor( const wxString& group  ) const
{
    return wxColour( GetColorFromStrng( m_config->Read( _T("/Groups/") + group + _T("/Opts/HLColor") , _T( "100 100 140" ) ) ) );
}

wxArrayString Settings::GetGroups( ) const
{
    wxString old_path = m_config->GetPath();
    m_config->SetPath( _T("/Groups/") );
    wxArrayString ret;
    long dummy;
    wxString tmp;
    bool cont = m_config->GetFirstGroup( tmp, dummy );
    while ( cont )
    {
        ret.Add( tmp );
        cont = m_config->GetNextGroup( tmp, dummy );
    }

    m_config->SetPath( old_path );
    return ret;
}

void Settings::AddGroup( const wxString& group )
{
    if ( !m_config->Exists( _T("/Groups/") + group ) )
    {
        m_config->Write( _T("/Groups/") , group );
        //set defaults
        SetGroupActions( group, UserActions::ActNone );
        SetGroupHLColor( defaultHLcolor, group );
    }
}

void Settings::DeleteGroup( const wxString& group )
{
    if ( m_config->Exists( _T("/Groups/") + group ) )
    {
        m_config->DeleteGroup( _T("/Groups/") + group );
    }
}

void Settings::SetGroupActions( const wxString& group, UserActions::ActionType action )
{
    //m_config->Write( _T("/Groups/") + group + _T("/Opts/Actions"), action );
  wxString key=_T("/Groups/")+group+_T("/Opts/ActionsList");
  m_config->DeleteGroup( key );
  key+=_T("/");
  unsigned int tmp=action&((UserActions::ActLast<<1)-1);
  int i=0;
  while(tmp!=0)
  {
    if(tmp&1)m_config->Write(key+m_configActionNames[i], true);
    i++;
    tmp>>=1;
  }
}

UserActions::ActionType Settings::GetGroupActions( const wxString& group ) const
{
  wxString key=_T("/Groups/")+group+_T("/Opts/Actions");
  if(m_config->HasEntry(key)){/// Backward compatibility.
    wxLogMessage(_T("loading deprecated group actions and updating config"));
    UserActions::ActionType action=(UserActions::ActionType) m_config->Read( key, (long) UserActions::ActNone ) ;
    m_config->DeleteEntry(key);

/// a bit ugly, but i want to update options
    Settings *this_nonconst=const_cast<Settings *>(this);
    this_nonconst->SetGroupActions(group,action);

    return action;
  }
  key=_T("/Groups/")+group+_T("/Opts/ActionsList");
  if(!m_config->Exists(key))return UserActions::ActNone;
  key+=_T("/");
  int i=0;
  int mask=1;
  int result=0;
  while(mask<=UserActions::ActLast){
    if( m_config->Read( key+m_configActionNames[i], (long)0) ){
      result|=mask;
    }
    i++;
    mask<<=1;
  }
  if(result==0)return UserActions::ActNone;
  return (UserActions::ActionType) result;
}


void Settings::SaveCustomColors( const wxColourData& _cdata, const wxString& paletteName  )
{
    //note 16 colors is wx limit
    wxColourData cdata = _cdata;
    for ( int i = 0; i < 16; ++i)
    {
        wxColor col = cdata.GetCustomColour( i );
        if ( !col.IsOk() )
            col = wxColor ( 255, 255, 255 );
        m_config->Write( _T("/CustomColors/") + paletteName + _T("/") + TowxString(i), GetColorString( col ) ) ;
    }
}

wxColourData Settings::GetCustomColors( const wxString& paletteName )
{
    wxColourData cdata;
    //note 16 colors is wx limit
    for ( int i = 0; i < 16; ++i)
    {
        wxColor col = GetColorFromStrng ( m_config->Read( _T("/CustomColors/") + paletteName + _T("/") + TowxString(i),
                                        GetColorString(  wxColor ( 255, 255, 255 ) ) ) );
        cdata.SetCustomColour( i, col );
    }

    return cdata;
}

bool Settings::GetReportStats()
{
    return m_config->Read( _T("/General/reportstats"), 1l );
}


void Settings::SetReportStats(const bool value)
{
    m_config->Write( _T("/General/reportstats"), value );
}


void Settings::SetAutoUpdate( const bool value )
{
    m_config->Write( _T("/General/AutoUpdate"), value );
}


bool Settings::GetAutoUpdate()
{
    return m_config->Read( _T("/General/AutoUpdate"), true );
}
