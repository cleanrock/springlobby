/* Copyright (C) 2007, 2008 The SpringLobby Team. All rights reserved. */
//
// Class: Settings
//

#include "settings.h"

#include "springsettings/se_utils.h"
#include "helper/wxTranslationHelper.h"
#include "helper/slconfig.h"
#include "defines.h" //to get HAVEWX??

#include <wx/filename.h>
#include <wx/filefn.h>
#include <wx/intl.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/colour.h>
#include <wx/cmndata.h>
#include <wx/font.h>
#include <wx/log.h>
#include <wx/wfstream.h>
#include <wx/settings.h>
#include <wx/tokenzr.h>
#ifdef HAVE_WX29
	#include <wx/colourdata.h>
#endif
#include <set>
#include <lslutils/misc.h>
#include <lslutils/globalsmanager.h>
#include <lslunitsync/c_api.h>
#include <lslunitsync/unitsync.h>

#include "nonportable.h"
#include "utils/conversion.h"
#include "utils/debug.h"
#include "utils/platform.h"
#include "uiutils.h"
#include "battlelist/battlelistfiltervalues.h"
#include "playback/playbackfiltervalues.h"
#include "customlistctrl.h"
#include "springsettings/presets.h"
#include "helper/sortutil.h"
#include "mainwindow.h"
#ifdef SL_DUMMY_COL
    #include "utils/customdialogs.h"
#endif
#include "utils/pathlistfactory.h"

#ifdef __WXMSW__
	#define BIN_EXT _T(".exe")
#else
	#define BIN_EXT wxEmptyString
#endif

const wxChar sep = wxFileName::GetPathSeparator();
const wxString sepstring = wxString(sep);

SLCONFIG("/General/SettingsVersion", SETTINGS_VERSION, "version of settings file");
SLCONFIG("/General/firstrun", true, "true if app is run first time");



Settings& sett()
{
    static LSL::Util::LineInfo<Settings> m( AT );
    static LSL::Util::GlobalObjectHolder<Settings, LSL::Util::LineInfo<Settings> > m_sett( m );
	return m_sett;
}

Settings::Settings()
	:m_forced_springconfig_path(wxEmptyString)
{
	m_config = &cfg();
}

Settings::~Settings()
{
}


void Settings::Setup(wxTranslationHelper* translationhelper)
{
	SetSettingsStandAlone( false );

	long settversion = cfg().ReadLong(_T("/General/SettingsVersion"));
	long cacheversion = cfg().ReadLong(_T( "/General/CacheVersion" ));

	const wxString userConfigDir = GetConfigfileDir();
	if ( IsFirstRun() && !wxDirExists( userConfigDir ) ) {
		wxMkdir( userConfigDir );
	}
	if ( (cacheversion < CACHE_VERSION) && !IsFirstRun() ) {
		SetMapCachingThreadProgress( 0 ); // reset map cache thread
		SetModCachingThreadProgress( 0 ); // reset mod cache thread
		if ( wxDirExists( GetCachePath() )  ) {
			wxLogWarning( _T("erasing old cache ver %d (app cache ver %d)"), cacheversion, CACHE_VERSION );
			wxString file = wxFindFirstFile( GetCachePath() + wxFILE_SEP_PATH + _T("*") );
			while ( !file.empty() ) {
				wxRemoveFile( file );
				file = wxFindNextFile();
			}
		}
		// after resetting cache, set current cache version
		cfg().Write( _T( "/General/CacheVersion" ), CACHE_VERSION );
	}

	if ( !cfg().ReadBool(_T("/General/firstrun") )) {
		ConvertSettings(translationhelper, settversion);
	}

	if ( ShouldAddDefaultServerSettings() || ( settversion < 14 && GetServers().Count() < 2  ) )
		SetDefaultServerSettings();

	if ( ShouldAddDefaultChannelSettings() ) {
		AddChannelJoin( _T("main"), wxEmptyString );
		AddChannelJoin( _T("newbies"), wxEmptyString );
		if ( translationhelper ) {
			if ( translationhelper->GetLocale() ) {
				wxString localecode = translationhelper->GetLocale()->GetCanonicalName();
				if ( localecode.Find(_T("_")) != -1 ) localecode = localecode.BeforeFirst(_T('_'));
				AddChannelJoin( localecode, wxEmptyString ); // add locale's language code to autojoin
			}
		}
	}
}

void Settings::ConvertSettings(wxTranslationHelper* translationhelper, long settversion)
{
	if( settversion < 19 ) {
		//the dummy column hack was removed on win
		m_config->DeleteGroup(_T("/GUI/ColumnWidths/"));
	}
	if ( settversion < 22 ) {
		if ( translationhelper ) {
			// add locale's language code to autojoin
			if ( translationhelper->GetLocale() ) {
				wxString localecode = translationhelper->GetLocale()->GetCanonicalName();
				if ( localecode.Find(_T("_")) != -1 ) localecode = localecode.BeforeFirst(_T('_'));
				if ( localecode == _T("en") ) // we'll skip en for now, maybe in the future we'll reactivate it
					AddChannelJoin( localecode, wxEmptyString );
			}
		}
	}
	if ( settversion < 23 ) {
		ConvertLists();
	}
	if ( settversion < 24 ) {
		DeleteServer( _T("Backup server") );
		DeleteServer( _T("Backup server 1") );
		DeleteServer( _T("Backup server 2") );
		SetDefaultServerSettings();
	}
	if ( settversion < 25 ) {
		SetDisableSpringVersionCheck(false);
	}
	// after updating, set current version
	m_config->Write(_T("/General/SettingsVersion"), SETTINGS_VERSION);
}

wxString Settings::FinalConfigPath() const
{
	return m_config->GetFilePath();
}

//! @brief Saves the settings to file
void Settings::SaveSettings()
{
	m_config->Write( _T( "/General/firstrun" ), false );
	m_config->SaveFile();
}

bool Settings::IsPortableMode() const
{
	bool portable = false;
	m_config->Read(_T("/portable"), &portable);
	return portable;
}


bool Settings::IsFirstRun()
{
	return m_config->ReadBool( _T( "/General/firstrun" ) );
}


wxString Settings::GetLobbyWriteDir()
{
	//FIXME: make configureable
	return GetConfigfileDir();
}


bool Settings::UseOldSpringLaunchMethod()
{
	return m_config->Read( _T( "/Spring/UseOldLaunchMethod" ), 0l );
}

bool Settings::GetNoUDP()
{
	return m_config->Read( _T( "/General/NoUDP" ), 0l );
}

void Settings::SetNoUDP( bool value )
{
	m_config->Write( _T( "/General/NoUDP" ), value );
}

int Settings::GetClientPort()
{
	return m_config->Read( _T( "/General/ClientPort" ), 0l );
}

void Settings::SetClientPort( int value )
{
	m_config->Write( _T( "/General/ClientPort" ), value );
}

bool Settings::GetShowIPAddresses()
{
	return m_config->Read( _T( "/General/ShowIP" ), 0l );
}

void Settings::SetShowIPAddresses( bool value )
{
	m_config->Write( _T( "/General/ShowIP" ), value );
}

void Settings::SetOldSpringLaunchMethod( bool value )
{
	m_config->Write( _T( "/Spring/UseOldLaunchMethod" ), value );
}


int Settings::GetHTTPMaxParallelDownloads()
{
	return m_config->Read(_T("/General/ParallelHTTPCount"),3);
}
void Settings::SetHTTPMaxParallelDownloads(int value)
{
	m_config->Write(_T("/General/ParallelHTTPCount"),value);
}



bool Settings::GetWebBrowserUseDefault()
{
	// See note on ambiguities, in wx/confbase.h near line 180.
	bool useDefault;
	m_config->Read( _T( "/General/WebBrowserUseDefault" ), &useDefault, DEFSETT_WEB_BROWSER_USE_DEFAULT );
	return useDefault;
}

void Settings::SetWebBrowserUseDefault( bool useDefault )
{
	m_config->Write( _T( "/General/WebBrowserUseDefault" ), useDefault );
}

wxString Settings::GetWebBrowserPath()
{
	return m_config->Read( _T( "/General/WebBrowserPath" ), wxEmptyString );
}


void Settings::SetWebBrowserPath( const wxString& path )
{
	m_config->Write( _T( "/General/WebBrowserPath" ), path );
}


wxString Settings::GetCachePath()
{
	wxString path = GetLobbyWriteDir() + sepstring + _T( "cache" ) + sep;
	if ( !wxFileName::DirExists( path ) ) {
		if ( !wxFileName::Mkdir(  path, 0755  ) ) return wxEmptyString;
	}
	return path;
}

void Settings::SetMapCachingThreadProgress( unsigned int index )
{
	m_config->Write( _T( "/General/LastMapCachingThreadIndex" ), ( int )index );
}


unsigned int Settings::GetMapCachingThreadProgress()
{
	return m_config->Read( _T( "/General/LastMapCachingThreadIndex" ), 0l );
}


void Settings::SetModCachingThreadProgress( unsigned int index )
{
	m_config->Write( _T( "/General/LastModCachingThreadIndex" ), ( int )index );
}


unsigned int Settings::GetModCachingThreadProgress()
{
	return m_config->Read( _T( "/General/LastModCachingThreadIndex" ), 0l );
}

bool Settings::ShouldAddDefaultServerSettings()
{
	return !m_config->Exists( _T( "/Server" ) );
}

//! @brief Restores default settings
void Settings::SetDefaultServerSettings()
{
	SetServer(  DEFSETT_DEFAULT_SERVER_NAME,  DEFSETT_DEFAULT_SERVER_HOST, DEFSETT_DEFAULT_SERVER_PORT );
	SetServer( _T( "Backup server 1" ), _T( "lobby1.springlobby.info" ), 8200 );
	SetServer( _T( "Backup server 2" ), _T( "lobby2.springlobby.info" ), 8200 );
	SetServer( _T( "Test server" ), _T( "lobby.springrts.com" ), 7000 );
	SetDefaultServer( DEFSETT_DEFAULT_SERVER_NAME );
}

//! @brief Checks if the server name/alias exists in the settings
bool Settings::ServerExists( const wxString& server_name )
{
	return m_config->Exists( _T( "/Server/Servers/" ) + server_name );
}


//! @brief Get the name/alias of the default server.
//!
//! @note Normally this will be the previously selected server. But at first run it will be a server that is set as the default.
wxString Settings::GetDefaultServer()
{
    wxString serv = DEFSETT_DEFAULT_SERVER_NAME;
    return m_config->Read( _T("/Server/Default"), serv );
}

void Settings::SetAutoConnect( bool do_autoconnect )
{
	m_config->Write( _T( "/Server/Autoconnect" ),  do_autoconnect );
}

bool Settings::GetAutoConnect( )
{
	return m_config->Read( _T( "/Server/Autoconnect" ), 0l );
}


//! @brief Set the name/alias of the default server.
//!
//! @param server_name the server name/alias
//! @see GetDefaultServer()
void   Settings::SetDefaultServer( const wxString& server_name )
{
	m_config->Write( _T( "/Server/Default" ),  server_name );
}


//! @brief Get hostname of a server.
//!
//! @param server_name the server name/alias
wxString Settings::GetServerHost( const wxString& server_name )
{
    wxString host = DEFSETT_DEFAULT_SERVER_HOST;
    return m_config->Read( _T("/Server/Servers/")+ server_name +_T("/Host"), host );
}


//! @brief Set hostname of a server.
//!
//! @param server_name the server name/alias
//! @param the host url address
//! @param the port where the service is run
void   Settings::SetServer( const wxString& server_name, const wxString& url, int port )
{
	m_config->Write( _T( "/Server/Servers/" ) + server_name + _T( "/Host" ), url );
	m_config->Write( _T( "/Server/Servers/" ) + server_name + _T( "/Port" ), port );
}

//! @brief Deletes a server from the list.
//!
//! @param server_name the server name/alias
void Settings::DeleteServer( const wxString& server_name )
{
	m_config->DeleteGroup( _T( "/Server/Servers/" ) + server_name );
}


//! @brief Get port number of a server.
//!
//! @param server_name the server name/alias
int    Settings::GetServerPort( const wxString& server_name )
{
	return m_config->Read( _T( "/Server/Servers/" ) + server_name + _T( "/Port" ), DEFSETT_DEFAULT_SERVER_PORT );
}

//! @brief Get list of server aliases
wxArrayString Settings::GetServers()
{
	return cfg().GetGroupList( _T( "/Server/Servers/" ) );
}


//! @brief Get nickname of the default account for a server.
//!
//! @param server_name the server name/alias
wxString Settings::GetServerAccountNick( const wxString& server_name )
{
	return m_config->Read( _T( "/Server/Servers/" ) + server_name + _T( "/Nick" ), wxEmptyString ) ;
}


//! @brief Set nickname of the default account for a server.
//!
//! @param server_name the server name/alias
//! @param value the vaule to be set
void Settings::SetServerAccountNick( const wxString& server_name, const wxString& value )
{
	m_config->Write( _T( "/Server/Servers/" ) + server_name + _T( "/Nick" ), value );
}


//! @brief Get password of the default account for a server.
//!
//! @param server_name the server name/alias
//! @todo Implement
wxString Settings::GetServerAccountPass( const wxString& server_name )
{
	return m_config->Read( _T( "/Server/Servers/" ) + server_name + _T( "/Pass" ), wxEmptyString );
}


//! @brief Set password of the default account for a server.
//!
//! @param server_name the server name/alias
//! @param value the vaule to be set
//! @todo Implement
void   Settings::SetServerAccountPass( const wxString& server_name, const wxString& value )
{
	m_config->Write( _T( "/Server/Servers/" ) + server_name + _T( "/Pass" ), value );
}


//! @brief Get if the password should be saved for a default server account.
//!
//! @param server_name the server name/alias
//! @todo Implement
bool   Settings::GetServerAccountSavePass( const wxString& server_name )
{
	return m_config->Read( _T( "/Server/Servers/" ) + server_name + _T( "/savepass" ), ( long int )false );
}


//! @brief Set if the password should be saved for a default server account.
//!
//! @param server_name the server name/alias
//! @param value the vaule to be set
//! @todo Implement
void Settings::SetServerAccountSavePass( const wxString& server_name, const bool value )
{
	m_config->Write( _T( "/Server/Servers/" ) + server_name + _T( "/savepass" ), ( long int )value );
}


int Settings::GetNumChannelsJoin()
{
	return cfg().GetGroupCount( _T( "/Channels/AutoJoin" ) );
}

void Settings::AddChannelJoin( const wxString& channel , const wxString& key )
{
	int index = GetNumChannelsJoin();

	m_config->Write( wxFormat( _T( "/Channels/AutoJoin/Channel%d/Name" ) ) % index, channel );
	m_config->Write( wxFormat( _T( "/Channels/AutoJoin/Channel%d/Password" ) ) % index, key );
}


void Settings::RemoveChannelJoin( const wxString& channel )
{
	int index = GetChannelJoinIndex( channel );
	if ( index == -1 ) return;
	int total = GetNumChannelsJoin();
	m_config->DeleteGroup( _T( "/Channels/AutoJoin/Channel" ) + TowxString( index ) );
	m_config->RenameGroup( _T( "/Channels/AutoJoin/Channel" ) + TowxString( total - 1 ), _T( "/Channels/AutoJoin/Channel" ) + TowxString( index ) );
}

void Settings::RemoveAllChannelsJoin()
{
	m_config->DeleteGroup( _T( "/Channels/AutoJoin" ) );
}


int Settings::GetChannelJoinIndex( const wxString& name )
{
	int numchannels = GetNumChannelsJoin();
	int ret = -1;
	for ( int i = 0; i < numchannels; i++ )
	{
		if ( m_config->Read( wxFormat( _T( "/Channels/AutoJoin/Channel%d/Name" ) ) % i, wxEmptyString ) == name ) ret = i;
	}
	return ret;
}

std::vector<ChannelJoinInfo> Settings::GetChannelsJoin()
{
	std::vector<ChannelJoinInfo> ret;
//	int num = GetNumChannelsJoin();
	wxArrayString channels = cfg().GetGroupList( _T("/Channels/AutoJoin/") );
	slConfig::PathGuard pathguard( m_config, _T("/Channels/AutoJoin/") );
	for ( size_t i = 0; i < channels.Count(); ++i )
	{
		if( !channels[i].StartsWith( _T("Channel") ) )
			continue;
		ChannelJoinInfo info;
		info.name = m_config->Read( channels[i] + _T("/Name" ) );
		info.password = m_config->Read( channels[i] + _T("/Password" ) );
		ret.push_back( info );
	}
	return ret;
}

bool Settings::ShouldAddDefaultChannelSettings()
{
	return !m_config->Exists( _T( "/Channels" ) );
}



// ========================================================

wxString Settings::AutoFindSpringBin()
{
	wxPathList pl;

	pl.AddEnvList( _T( "%ProgramFiles%" ) );
	pl.AddEnvList( _T( "PATH" ) );

    pl = PathlistFactory::AdditionalSearchPaths( pl );

	return pl.FindValidPath( SPRING_BIN );
}

wxString Settings::AutoFindUnitSync(wxPathList pl) const
{
	wxString retpath = pl.FindValidPath( _T( "unitsync" ) + GetLibExtension() );
	if ( retpath.IsEmpty() )
		retpath = pl.FindValidPath( _T( "libunitsync" ) + GetLibExtension() );
	return retpath;
}

wxString Settings::AutoFindBundle()
{
    wxPathList pl = PathlistFactory::ConfigFileSearchPaths();
	return pl.FindValidPath( _T( "Spring.app" ) );
}


std::map<wxString, LSL::SpringBundle> Settings::GetSpringVersionList() const
{
	return m_spring_versions;
}

bool LocateSystemInstalledSpring(LSL::SpringBundle& bundle)
{
	wxPathList paths = PathlistFactory::ConfigFileSearchPaths();
	for (const auto path: paths) {
		if (bundle.AutoComplete(STD_STRING(path))) {
			return true;
		}
	}
	return false;
}


void Settings::RefreshSpringVersionList(bool autosearch, const LSL::SpringBundle* additionalbundle)
{
/*
FIXME: move to LSL's GetSpringVersionList() which does:

	1. search system installed spring + unitsync (both paths independant)
	2. search for user installed spring + unitsync (assume bundled)
	3. search / validate paths from config
	4. search path / unitsync given from user input

needs to change to sth like: GetSpringVersionList(std::list<LSL::Bundle>)

*/
	wxLogDebugFunc( wxEmptyString );
	std::list<LSL::SpringBundle> usync_paths;

	if (additionalbundle != NULL) {
		usync_paths.push_back(*additionalbundle);
	}
	if (autosearch) {
		LSL::SpringBundle systembundle;
		if (LocateSystemInstalledSpring(systembundle)) {
			usync_paths.push_back(systembundle);
		}

		wxPathList ret;
		wxPathList paths = PathlistFactory::AdditionalSearchPaths(ret);
		const wxString springbin(SPRING_BIN);
		for (const auto path: paths) {
			LSL::SpringBundle bundle;
			bundle.path = STD_STRING(path);
			usync_paths.push_back(bundle);
		}
	}

	wxArrayString list = cfg().GetGroupList( _T( "/Spring/Paths" ) );
	const int count = list.GetCount();
	for ( int i = 0; i < count; i++ ) {
		LSL::SpringBundle bundle;
		bundle.unitsync = STD_STRING(GetUnitSync(list[i]));
		bundle.spring = STD_STRING(GetSpringBinary(list[i]));
		bundle.path = STD_STRING(GetBundle(list[i]));
		bundle.version = STD_STRING(list[i]);
		usync_paths.push_back(bundle);
	}

	cfg().DeleteGroup(_T("/Spring/Paths"));

	m_spring_versions.clear();
	try {
		const auto versions = LSL::susynclib().GetSpringVersionList( usync_paths );
		for(const auto pair : versions) {
			const LSL::SpringBundle& bundle = pair.second;
			const wxString version = TowxString(bundle.version);
			m_spring_versions[version] = bundle;
			SetSpringBinary(version, TowxString(bundle.spring));
			SetUnitSync(version, TowxString(bundle.unitsync));
			SetBundle(version, TowxString(bundle.path));
		}
	} catch (const std::runtime_error& e) {
		wxLogError(wxString::Format(_T("Couldn't get list of spring versions: %s"), e.what()));
	} catch ( ...) {
		wxLogError(_T("Unknown Execption caught in Settings::RefreshSpringVersionList"));
	}
}

wxString Settings::GetCurrentUsedSpringIndex()
{
    return m_config->Read( _T( "/Spring/CurrentIndex" ), _T( "default" ) );
}

void Settings::SetUsedSpringIndex( const wxString& index )
{
    m_config->Write( _T( "/Spring/CurrentIndex" ), TowxString(index) );
}


bool Settings::GetSearchSpringOnlyInSLPath()
{
	return m_config->Read( _T( "/Spring/SearchSpringOnlyInSLPath" ), ( long int )false );
}

void Settings::SetSearchSpringOnlyInSLPath( bool value )
{
	m_config->Write( _T( "/Spring/SearchSpringOnlyInSLPath" ), value );
}

void Settings::DeleteSpringVersionbyIndex( const wxString& index )
{
	m_config->DeleteGroup( _T( "/Spring/Paths/" ) + index );
	if ( GetCurrentUsedSpringIndex() == index ) SetUsedSpringIndex( wxEmptyString );
}


bool Settings::IsInsideSpringBundle()
{
	return wxFileName::FileExists(GetExecutableFolder() + sepstring + _T("spring") + BIN_EXT) && wxFileName::FileExists(GetExecutableFolder() + sepstring + _T("unitsync") + GetLibExtension());
}

bool Settings::GetBundleMode()
{
	#ifndef __WXMAC__
		return false;
	#endif
	return m_config->Read(_T("/Spring/EnableBundleMode"), true);
}


bool Settings::GetUseSpringPathFromBundle()
{
	#ifndef __WXMAC__
		return false;
	#endif
	if ( !GetBundleMode() ) return false;
	return m_config->Read(_T("/Spring/UseSpringPathFromBundle"), IsInsideSpringBundle());
}

void Settings::SetUseSpringPathFromBundle( bool value )
{
	m_config->Write(_T("/Spring/UseSpringPathFromBundle"), value );
}

wxString Settings::GetCurrentUsedDataDir()
{
	wxString dir;
	if ( LSL::susynclib().IsLoaded() )
	{
        if ( LSL::susynclib().VersionSupports( LSL::USYNC_GetDataDir ) )
            dir = TowxString(LSL::susynclib().GetSpringDataDir());
        else
            dir = TowxString(LSL::susynclib().GetSpringConfigString("SpringData", ""));
	}
#ifdef __WXMSW__
	if ( dir.IsEmpty() )
        dir = GetExecutableFolder(); // fallback
#else
    if ( IsPortableMode() )
        dir = GetExecutableFolder();
	if ( dir.IsEmpty() )
        dir = wxFileName::GetHomeDir() + sepstring + _T( ".spring" ); // fallback
#endif
	wxString stripped;
	if ( dir.EndsWith(sepstring,&stripped) ) return stripped;
	return dir;
}


wxString Settings::GetCurrentUsedSpringBinary()
{
	return GetSpringBinary( GetCurrentUsedSpringIndex() );
}

wxString Settings::GetCurrentUsedUnitSync()
{
	return GetUnitSync( GetCurrentUsedSpringIndex() );
}

wxString Settings::GetCurrentUsedBundle()
{
	return GetBundle( GetCurrentUsedSpringIndex() );
}

wxString Settings::GetCurrentUsedSpringConfigFilePath()
{
	wxString path;
	try {
		path = TowxString(LSL::susynclib().GetConfigFilePath());
	} catch ( std::runtime_error& e) {
		wxLogError( wxString::Format( _T("Couldn't get SpringConfigFilePath, exception caught:\n %s"), e.what()  ) );
	}
	return path;
}

wxString Settings::GetUnitSync( const wxString& index )
{
	if ( GetBundleMode() ) return GetBundle( index )+ sepstring + _T("Contents") + sepstring + _T("MacOS") + sepstring + _T("libunitsync") +  GetLibExtension();
	return m_config->Read( _T( "/Spring/Paths/" ) + index + _T( "/UnitSyncPath" ), AutoFindUnitSync() );
}


wxString Settings::GetSpringBinary( const wxString& index )
{
	if ( GetBundleMode() )  return GetBundle( index )+ sepstring + _T("Contents") + sepstring + _T("MacOS") + sepstring + _T("spring");
	return m_config->Read( _T( "/Spring/Paths/" ) + index + _T( "/SpringBinPath" ), AutoFindSpringBin() );
}

wxString Settings::GetBundle( const wxString& index )
{
	return m_config->Read( _T( "/Spring/Paths/" ) + index + _T( "/SpringBundlePath" ), AutoFindBundle() );
}

void Settings::SetUnitSync( const wxString& index, const wxString& path )
{
	m_config->Write( _T( "/Spring/Paths/" ) + index + _T( "/UnitSyncPath" ), path );
}

void Settings::SetSpringBinary( const wxString& index, const wxString& path )
{
	m_config->Write( _T( "/Spring/Paths/" ) + index + _T( "/SpringBinPath" ), path );
}

void Settings::SetBundle( const wxString& index, const wxString& path )
{
	m_config->Write( _T( "/Spring/Paths/" ) + index + _T( "/SpringBundlePath" ), path );
}

void Settings::SetForcedSpringConfigFilePath( const wxString& path )
{
	m_forced_springconfig_path = path;
}

wxString Settings::GetForcedSpringConfigFilePath()
{
	if ( IsPortableMode() )
        return GetCurrentUsedDataDir() + sepstring + _T( "springsettings.cfg" );
	else
		return m_forced_springconfig_path;
}

// ===================================================

bool Settings::GetChatLogEnable()
{
	if ( !m_config->Exists( _T( "/ChatLog/chatlog_enable" ) ) ) SetChatLogEnable( true );
	return m_config->Read( _T( "/ChatLog/chatlog_enable" ), true );
}


void Settings::SetChatLogEnable( const bool value )
{
	m_config->Write( _T( "/ChatLog/chatlog_enable" ), value );
}


wxString Settings::GetChatLogLoc()
{
	wxString path = GetLobbyWriteDir() +  sepstring + _T( "chatlog" );
	if ( !wxFileName::DirExists( path ) )
	{
		if ( !wxFileName::Mkdir(  path, 0755  ) ) return wxEmptyString;
	}
	return path;
}


wxString Settings::GetLastHostDescription()
{
	return m_config->Read( _T( "/Hosting/LastDescription" ), wxEmptyString );
}


wxString Settings::GetLastHostMod()
{
	return m_config->Read( _T( "/Hosting/LastMod" ), wxEmptyString );
}


wxString Settings::GetLastHostPassword()
{
	return m_config->Read( _T( "/Hosting/LastPassword" ), wxEmptyString );
}


int Settings::GetLastHostPort()
{
	return m_config->Read( _T( "/Hosting/LastPort" ), DEFSETT_SPRING_PORT );
}


int Settings::GetLastHostPlayerNum()
{
	return m_config->Read( _T( "/Hosting/LastPlayerNum" ), 4 );
}


int Settings::GetLastHostNATSetting()
{
	return m_config->Read( _T( "/Hosting/LastNATSetting" ), ( long )0 );
}


wxString Settings::GetLastHostMap()
{
	return m_config->Read( _T( "/Hosting/LastMap" ), wxEmptyString );
}

int Settings::GetLastRankLimit()
{
	return m_config->Read( _T( "/Hosting/LastRank" ), 0l );
}

bool Settings::GetTestHostPort()
{
	return m_config->Read( _T( "/Hosting/TestHostPort" ), 0l );
}

bool Settings::GetLastAutolockStatus()
{
	return m_config->Read( _T( "/Hosting/LastAutoLock" ), true );
}

bool Settings::GetLastHostRelayedMode()
{
	return m_config->Read( _T( "/Hosting/LastRelayedMode" ), 0l );
}

wxColour Settings::GetBattleLastColour()
{
	return  wxColour( m_config->Read( _T( "/Hosting/MyLastColour" ), _T( "#FFFF00" ) ) );
}


void Settings::SetBattleLastColour( const wxColour& col )
{
	m_config->Write( _T( "/Hosting/MyLastColour" ), col.GetAsString( wxC2S_HTML_SYNTAX ) );
}

void Settings::SetLastHostDescription( const wxString& value )
{
	m_config->Write( _T( "/Hosting/LastDescription" ), value );
}


void Settings::SetLastHostMod( const wxString& value )
{
	m_config->Write( _T( "/Hosting/LastMod" ), value );
}


void Settings::SetLastHostPassword( const wxString& value )
{
	m_config->Write( _T( "/Hosting/LastPassword" ), value );
}


void Settings::SetLastHostPort( int value )
{
	m_config->Write( _T( "/Hosting/LastPort" ), value );
}


void Settings::SetLastHostPlayerNum( int value )
{
	m_config->Write( _T( "/Hosting/LastPlayerNum" ), value );
}


void Settings::SetLastHostNATSetting( int value )
{
	m_config->Write( _T( "/Hosting/LastNATSetting" ), value );
}


void Settings::SetLastHostMap( const wxString& value )
{
	m_config->Write( _T( "/Hosting/LastMap" ), value );
}

void Settings::SetLastRankLimit( int rank )
{
	m_config->Write( _T( "/Hosting/LastRank" ), rank );
}

void Settings::SetLastAI( const wxString& ai )
{
	m_config->Write( _T( "/SinglePlayer/LastAI" ), ai );
}

void Settings::SetLastBattleId(int battleId)
{
	m_config->Write( _T( "/Hosting/MyLastBattleId" ), battleId);
}

void Settings::SetLastScriptPassword( const wxString& scriptPassword )
{
	m_config->Write( _T( "/Hosting/MyLastScriptPassword" ), scriptPassword );
}

void Settings::SetTestHostPort( bool value )
{
	m_config->Write( _T( "/Hosting/TestHostPort" ), value );
}

void Settings::SetLastAutolockStatus( bool value )
{
	m_config->Write( _T( "/Hosting/LastAutoLock" ), value );
}

void Settings::SetLastHostRelayedMode( bool value )
{
	m_config->Write( _T( "/Hosting/LastRelayedMode" ), value );
}

void Settings::SetHostingPreset( const wxString& name, int optiontype, std::map<wxString, wxString> options )
{
	m_config->DeleteGroup( _T( "/Hosting/Preset/" ) + name + _T( "/" ) + TowxString( optiontype ) );
	for ( std::map<wxString, wxString>::const_iterator it = options.begin(); it != options.end(); ++it )
	{
		m_config->Write( _T( "/Hosting/Preset/" ) + name + _T( "/" ) + TowxString( optiontype ) + _T( "/" ) + it->first , it->second );
	}
}

std::map<wxString, wxString> Settings::GetHostingPreset( const wxString& name, int optiontype )
{
	wxString path_base = _T( "/Hosting/Preset/" ) + name + _T( "/" ) + TowxString( optiontype );
	std::map<wxString, wxString> ret;
	wxArrayString list = cfg().GetEntryList( path_base );

	slConfig::PathGuard pathGuard ( m_config, path_base );

	int count = list.GetCount();
	for ( int i = 0; i < count; i ++ )
	{
		wxString keyname = list[i];
		wxString val = m_config->Read( keyname );
		ret[keyname] = val;
	}
	return ret;
}


wxArrayString Settings::GetPresetList()
{
	return cfg().GetGroupList( _T( "/Hosting/Preset" ) );
}


void Settings::DeletePreset( const wxString& name )
{
	m_config->DeleteGroup( _T( "/Hosting/Preset/" ) + name );

	//delete mod default preset associated
	wxArrayString list = cfg().GetEntryList( _T( "/Hosting/ModDefaultPreset" ) );
	int count = list.GetCount();
	for ( int i = 0; i < count; i ++ )
	{
		wxString keyname = list[i];
		if ( m_config->Read( keyname ) == name ) m_config->DeleteEntry( keyname );
	}
}


wxString Settings::GetModDefaultPresetName( const wxString& modname )
{
	return m_config->Read( _T( "/Hosting/ModDefaultPreset/" ) + modname );
}


void Settings::SetModDefaultPresetName( const wxString& modname, const wxString& presetname )
{
	m_config->Write( _T( "/Hosting/ModDefaultPreset/" ) + modname, presetname );
}


void Settings::SetBalanceMethod( int value )
{
	m_config->Write( _T( "/Hosting/BalanceMethod" ), value );
}
int Settings::GetBalanceMethod()
{
	return m_config->Read( _T( "/Hosting/BalanceMethod" ), 1l );
}

void Settings::SetBalanceClans( bool value )
{
	m_config->Write( _T( "/Hosting/BalanceClans" ), value );
}
bool Settings::GetBalanceClans()
{
	return m_config->Read( _T( "/Hosting/BalanceClans" ), true );
}

void Settings::SetBalanceStrongClans( bool value )
{
	m_config->Write( _T( "/Hosting/BalanceStrongClans" ), value );
}

bool Settings::GetBalanceStrongClans()
{
	return m_config->Read( _T( "/Hosting/BalanceStrongClans" ), 0l );
}

void Settings::SetBalanceGrouping( int value )
{
	m_config->Write( _T( "/Hosting/BalanceGroupingSize" ), value );
}

int Settings::GetBalanceGrouping()
{
	return m_config->Read( _T( "/Hosting/BalanceGroupingSize" ), 0l );
}


void Settings::SetFixIDMethod( int value )
{
	m_config->Write( _T( "/Hosting/FixIDMethod" ), value );
}
int Settings::GetFixIDMethod()
{
	return m_config->Read( _T( "/Hosting/FixIDMethod" ), 1l );
}

void Settings::SetFixIDClans( bool value )
{
	m_config->Write( _T( "/Hosting/FixIDClans" ), value );
}
bool Settings::GetFixIDClans()
{
	return m_config->Read( _T( "/Hosting/FixIDClans" ), true );
}

void Settings::SetFixIDStrongClans( bool value )
{
	m_config->Write( _T( "/Hosting/FixIDStrongClans" ), value );
}

bool Settings::GetFixIDStrongClans()
{
	return m_config->Read( _T( "/Hosting/FixIDStrongClans" ), 0l );
}

void Settings::SetFixIDGrouping( int value )
{
	m_config->Write( _T( "/Hosting/FixIDGroupingSize" ), value );
}

int Settings::GetFixIDGrouping()
{
	return m_config->Read( _T( "/Hosting/FixIDGroupingSize" ), 0l );
}


wxString Settings::GetLastAI()
{
	return m_config->Read( _T( "/SinglePlayer/LastAI" ), wxEmptyString );
}

int Settings::GetLastBattleId()
{
	return m_config->Read( _T( "/Hosting/MyLastBattleId" ), -1 );
}

wxString Settings::GetLastScriptPassword()
{
	return m_config->Read( _T( "/Hosting/MyLastScriptPassword" ), wxEmptyString );
}

void Settings::SetDisplayJoinLeave( bool display, const wxString& channel  )
{
	m_config->Write( _T( "/Channels/DisplayJoinLeave/" )  + channel, display );
}

bool Settings::GetDisplayJoinLeave( const wxString& channel  )
{
    return m_config->Read( _T( "/Channels/DisplayJoinLeave/" ) +  channel, 0l );
}


void Settings::SetChatHistoryLenght( int historylines )
{
	m_config->Write( _T( "/Chat/HistoryLinesLenght" ), historylines );
}


int Settings::GetChatHistoryLenght()
{
	return m_config->Read( _T( "/Chat/HistoryLinesLenght" ), 1000l );
}


void Settings::SetChatPMSoundNotificationEnabled( bool enabled )
{
	m_config->Write( _T( "/Chat/PMSound" ), enabled );
}


bool Settings::GetChatPMSoundNotificationEnabled()
{
	return m_config->Read( _T( "/Chat/PMSound" ), 1l );
}

wxColour ConvertOldRGBFormat( wxString color )
{
	long R = 0, G = 0, B = 0;
	color.BeforeFirst( _T( ' ' ) ).ToLong( &R );
	color = color.AfterFirst( _T( ' ' ) );
	color.BeforeFirst( _T( ' ' ) ).ToLong( &G );
	color = color.AfterFirst( _T( ' ' ) );
	color.BeforeFirst( _T( ' ' ) ).ToLong( &B );
	return wxColour( R % 256, G % 256, B % 256 );
}

wxColour Settings::GetChatColorNormal()
{
	return wxColour( m_config->Read( _T( "/Chat/Colour/Normal" ), _T( "#000000" ) ) );
}

void Settings::SetChatColorNormal( wxColour value )
{
	m_config->Write( _T( "/Chat/Colour/Normal" ), value.GetAsString( wxC2S_CSS_SYNTAX ) );
}


wxColour Settings::GetChatColorBackground()
{
	return wxColour( m_config->Read( _T( "/Chat/Colour/Background" ), _T( "#FFFFFF" ) ) );
}

void Settings::SetChatColorBackground( wxColour value )
{
	m_config->Write( _T( "/Chat/Colour/Background" ), value.GetAsString( wxC2S_CSS_SYNTAX ) );
}

wxColour Settings::GetChatColorHighlight()
{
	return wxColour( m_config->Read( _T( "/Chat/Colour/Highlight" ), _T( "#FF0000" ) ) );
}

void Settings::SetChatColorHighlight( wxColour value )
{
	m_config->Write( _T( "/Chat/Colour/Highlight" ), value.GetAsString( wxC2S_CSS_SYNTAX ) );
}

wxColour Settings::GetChatColorMine()
{
	return wxColour( m_config->Read( _T( "/Chat/Colour/Mine" ), _T( "#8A8A8A" ) ) );
}

void Settings::SetChatColorMine( wxColour value )
{
	m_config->Write( _T( "/Chat/Colour/Mine" ), value.GetAsString( wxC2S_CSS_SYNTAX ) );
}

wxColour Settings::GetChatColorNotification()
{
	return wxColour( m_config->Read( _T( "/Chat/Colour/Notification" ), _T( "#FF2828" ) ) );
}

void Settings::SetChatColorNotification( wxColour value )
{
	m_config->Write( _T( "/Chat/Colour/Notification" ), value.GetAsString( wxC2S_CSS_SYNTAX ) );
}

wxColour Settings::GetChatColorAction()
{
	return wxColour( m_config->Read( _T( "/Chat/Colour/Action" ), _T( "#E600FF" ) ) );
}

void Settings::SetChatColorAction( wxColour value )
{
	m_config->Write( _T( "/Chat/Colour/Action" ), value.GetAsString( wxC2S_CSS_SYNTAX ) );
}

wxColour Settings::GetChatColorServer()
{
	return wxColour( m_config->Read( _T( "/Chat/Colour/Server" ), _T( "#005080" ) ) );
}

void Settings::SetChatColorServer( wxColour value )
{
	m_config->Write( _T( "/Chat/Colour/Server" ), value.GetAsString( wxC2S_CSS_SYNTAX ) );
}

wxColour Settings::GetChatColorClient()
{
	return wxColour( m_config->Read( _T( "/Chat/Colour/Client" ), _T( "#14C819" ) ) );
}

void Settings::SetChatColorClient( wxColour value )
{
	m_config->Write( _T( "/Chat/Colour/Client" ), value.GetAsString( wxC2S_CSS_SYNTAX ) );
}

wxColour Settings::GetChatColorJoinPart()
{
	return wxColour( m_config->Read( _T( "/Chat/Colour/JoinPart" ), _T( "#42CC42" ) ) );
}

void Settings::SetChatColorJoinPart( wxColour value )
{
	m_config->Write( _T( "/Chat/Colour/JoinPart" ), value.GetAsString( wxC2S_CSS_SYNTAX ) );
}

wxColour Settings::GetChatColorError()
{
	return wxColour( m_config->Read( _T( "/Chat/Colour/Error" ), _T( "#800000" ) ) );
}

void Settings::SetChatColorError( wxColour value )
{
	m_config->Write( _T( "/Chat/Colour/Error" ), value.GetAsString( wxC2S_CSS_SYNTAX ) );
}

wxColour Settings::GetChatColorTime()
{
	return wxColour( m_config->Read( _T( "/Chat/Colour/Time" ), _T( "#64648C" ) ) );
}

void Settings::SetChatColorTime( wxColour value )
{
	m_config->Write( _T( "/Chat/Colour/Time" ), value.GetAsString( wxC2S_CSS_SYNTAX ) );
}

wxFont Settings::GetChatFont()
{
	wxString info = m_config->Read( _T( "/Chat/Font" ), wxEmptyString );
	if ( info != wxEmptyString ) {
		wxFont f;
		f.SetNativeFontInfo( info );
		return f;
	}
	else {
		wxFont f( 8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL );
		return f;
	}
}

void Settings::SetChatFont( wxFont value )
{
	m_config->Write( _T( "/Chat/Font" ), value.GetNativeFontInfoDesc() );
}


bool Settings::GetSmartScrollEnabled()
{
	return m_config->Read( _T( "/Chat/SmartScrollEnabled" ), true );
}

void Settings::SetSmartScrollEnabled( bool value ) {
	m_config->Write( _T( "/Chat/SmartScrollEnabled" ), value );
}

bool Settings::GetAlwaysAutoScrollOnFocusLost()
{
	return m_config->Read( _T( "/Chat/AlwaysAutoScrollOnFocusLost" ), true );
}

void Settings::SetAlwaysAutoScrollOnFocusLost( bool value )
{
	m_config->Write( _T( "/Chat/AlwaysAutoScrollOnFocusLost" ), value );
}

void Settings::SetUseIrcColors( bool value )
{
	m_config->Write( _T( "/Chat/UseIrcColors" ), value );
}

bool Settings::GetUseIrcColors()
{
	return m_config->Read( _T( "/Chat/UseIrcColors" ), true );
}

void Settings::setFromList(const wxArrayString& list, const wxString& path)
{
    wxString string;
    for ( unsigned int i = 0; i < list.GetCount(); i++ )
        string << list[i] << _T( ";" );
    m_config->Write( path, string );
}

wxArrayString Settings::getFromList(const wxString& path)
{
    return wxStringTokenize( m_config->Read( path, wxString() ), _T(";") );
}

void Settings::SetHighlightedWords( const wxArrayString& words )
{
    setFromList( words, _T("/Chat/HighlightedWords") );
}

wxArrayString Settings::GetHighlightedWords()
{
    return getFromList( _T("/Chat/HighlightedWords") );
}

void Settings::ConvertLists()
{
    const wxArrayString current_hl = cfg().GetEntryList( _T( "/Chat/HighlightedWords" ) );
    m_config->DeleteGroup( _T( "/Chat/HighlightedWords" ) );
    SaveSettings();
    SetHighlightedWords( current_hl );
    SaveSettings();
}

void Settings::SetRequestAttOnHighlight( const bool req )
{
	m_config->Write( _T( "/Chat/ReqAttOnHighlight" ), req );
}

bool Settings::GetRequestAttOnHighlight( )
{
	return m_config->Read( _T( "/Chat/ReqAttOnHighlight" ), 0l );
}

BattleListFilterValues Settings::GetBattleFilterValues( const wxString& profile_name )
{
	BattleListFilterValues filtervalues;
	filtervalues.description =      m_config->Read( _T( "/BattleFilter/" ) + profile_name + _T( "/description" ), wxEmptyString );
	filtervalues.host =             m_config->Read( _T( "/BattleFilter/" ) + profile_name + _T( "/host" ), wxEmptyString );
	filtervalues.map =               m_config->Read( _T( "/BattleFilter/" ) + profile_name + _T( "/map" ), wxEmptyString );
	filtervalues.map_show =         m_config->Read( _T( "/BattleFilter/" ) + profile_name + _T( "/map_show" ), 0L );
	filtervalues.maxplayer =        m_config->Read( _T( "/BattleFilter/" ) + profile_name + _T( "/maxplayer" ), _T( "All" ) );
	filtervalues.maxplayer_mode =   m_config->Read( _T( "/BattleFilter/" ) + profile_name + _T( "/maxplayer_mode" ), _T( "=" ) );
	filtervalues.mod =              m_config->Read( _T( "/BattleFilter/" ) + profile_name + _T( "/mod" ), wxEmptyString );
	filtervalues.mod_show =         m_config->Read( _T( "/BattleFilter/" ) + profile_name + _T( "/mod_show" ), 0L );
	filtervalues.player_mode =      m_config->Read( _T( "/BattleFilter/" ) + profile_name + _T( "/player_mode" ), _T( "=" ) );
	filtervalues.player_num  =      m_config->Read( _T( "/BattleFilter/" ) + profile_name + _T( "/player_num" ), _T( "All" ) );
	filtervalues.rank =             m_config->Read( _T( "/BattleFilter/" ) + profile_name + _T( "/rank" ), _T( "All" ) );
	filtervalues.rank_mode =        m_config->Read( _T( "/BattleFilter/" ) + profile_name + _T( "/rank_mode" ), _T( "<" ) );
	filtervalues.spectator =        m_config->Read( _T( "/BattleFilter/" ) + profile_name + _T( "/spectator" ), _T( "All" ) );
	filtervalues.spectator_mode =   m_config->Read( _T( "/BattleFilter/" ) + profile_name + _T( "/spectator_mode" ), _T( "=" ) );
	filtervalues.status_full =      m_config->Read( _T( "/BattleFilter/" ) + profile_name + _T( "/status_full" ), true );
	filtervalues.status_locked =    m_config->Read( _T( "/BattleFilter/" ) + profile_name + _T( "/status_locked" ), true );
	filtervalues.status_open =      m_config->Read( _T( "/BattleFilter/" ) + profile_name + _T( "/status_open" ), true );
	filtervalues.status_passworded = m_config->Read( _T( "/BattleFilter/" ) + profile_name + _T( "/status_passworded" ), true );
	filtervalues.status_start =     m_config->Read( _T( "/BattleFilter/" ) + profile_name + _T( "/status_start" ), true );
	filtervalues.highlighted_only = m_config->Read( _T( "/BattleFilter/" ) + profile_name + _T( "/highlighted_only" ), 0l );
	return filtervalues;
}

void Settings::SetBattleFilterValues( const BattleListFilterValues& filtervalues, const wxString& profile_name )
{
	m_config->Write( _T( "/BattleFilter/" ) + profile_name + _T( "/description" ), filtervalues.description );
	m_config->Write( _T( "/BattleFilter/" ) + profile_name + _T( "/host" ), filtervalues.host );
	m_config->Write( _T( "/BattleFilter/" ) + profile_name + _T( "/map" ), filtervalues.map );
	m_config->Write( _T( "/BattleFilter/" ) + profile_name + _T( "/map_show" ), filtervalues.map_show );
	m_config->Write( _T( "/BattleFilter/" ) + profile_name + _T( "/maxplayer" ), filtervalues.maxplayer );
	m_config->Write( _T( "/BattleFilter/" ) + profile_name + _T( "/maxplayer_mode" ), filtervalues.maxplayer_mode );
	m_config->Write( _T( "/BattleFilter/" ) + profile_name + _T( "/mod" ), filtervalues.mod );
	m_config->Write( _T( "/BattleFilter/" ) + profile_name + _T( "/mod_show" ), filtervalues.mod_show );
	m_config->Write( _T( "/BattleFilter/" ) + profile_name + _T( "/player_mode" ), filtervalues.player_mode );
	m_config->Write( _T( "/BattleFilter/" ) + profile_name + _T( "/player_num" ), filtervalues.player_num );
	m_config->Write( _T( "/BattleFilter/" ) + profile_name + _T( "/rank" ), filtervalues.rank );
	m_config->Write( _T( "/BattleFilter/" ) + profile_name + _T( "/rank_mode" ), filtervalues.rank_mode );
	m_config->Write( _T( "/BattleFilter/" ) + profile_name + _T( "/spectator" ), filtervalues.spectator );
	m_config->Write( _T( "/BattleFilter/" ) + profile_name + _T( "/spectator_mode" ), filtervalues.spectator_mode );
	m_config->Write( _T( "/BattleFilter/" ) + profile_name + _T( "/status_full" ), filtervalues.status_full );
	m_config->Write( _T( "/BattleFilter/" ) + profile_name + _T( "/status_locked" ), filtervalues.status_locked );
	m_config->Write( _T( "/BattleFilter/" ) + profile_name + _T( "/status_open" ), filtervalues.status_open );
	m_config->Write( _T( "/BattleFilter/" ) + profile_name + _T( "/status_passworded" ), filtervalues.status_passworded );
	m_config->Write( _T( "/BattleFilter/" ) + profile_name + _T( "/status_start" ), filtervalues.status_start );
	m_config->Write( _T( "/BattleFilter/" ) + profile_name + _T( "/highlighted_only" ), filtervalues.highlighted_only );
	m_config->Write( _T( "/BattleFilter/lastprofile" ), profile_name );
}

bool Settings::GetBattleFilterActivState() const
{
	return m_config->Read( _T( "/BattleFilter/Active" ) , 0l );
}

void Settings::SetBattleFilterActivState( const bool state )
{
	m_config->Write( _T( "/BattleFilter/Active" ) , state );
}

bool Settings::GetBattleLastAutoStartState()
{
	return m_config->Read( _T( "/Hosting/AutoStart" ) , 0l );
}

void Settings::SetBattleLastAutoStartState( bool value )
{
	m_config->Write( _T( "/Hosting/AutoStart" ), value );
}

bool Settings::GetBattleLastAutoControlState()
{
	return m_config->Read( _T( "/Hosting/AutoControl" ) , 0l );
}

void Settings::SetBattleLastAutoControlState( bool value )
{
	m_config->Write( _T( "/Hosting/AutoControl" ), value );
}

int Settings::GetBattleLastAutoSpectTime()
{
	return m_config->Read( _T( "/Hosting/AutoSpectTime" ) , 0l );
}

void Settings::SetBattleLastAutoSpectTime( int value )
{
	m_config->Write( _T( "/Hosting/AutoSpectTime" ) , value );
}

bool Settings::GetBattleLastAutoAnnounceDescription()
{
	return m_config->Read( _T( "/Hosting/AutoAnnounceDescription" ) , 0l );
}

void Settings::SetBattleLastAutoAnnounceDescription( bool value )
{
	m_config->Write( _T( "/Hosting/AutoAnnounceDescription" ) , value );
}

void Settings::SetBattleLastSideSel( const wxString& modname, int sidenum )
{
	m_config->Write(_T("/Battle/Sides/" + modname), sidenum);
}

int Settings::GetBattleLastSideSel( const wxString& modname )
{
	if (modname.IsEmpty())
		return 0;
	return m_config->Read( _T("/Battle/Sides/" + modname), 0l );
}

void Settings::SetMapLastStartPosType( const wxString& mapname, const wxString& startpostype )
{
	m_config->Write( _T( "/Hosting/MapLastValues/" ) + mapname + _T( "/startpostype" ), startpostype );
}

void Settings::SetMapLastRectPreset( const wxString& mapname, std::vector<Settings::SettStartBox> rects )
{
	wxString basepath = _T( "/Hosting/MapLastValues/" ) + mapname + _T( "/Rects" );
	m_config->DeleteGroup( basepath );
	for ( std::vector<Settings::SettStartBox>::const_iterator itor = rects.begin(); itor != rects.end(); ++itor )
	{
		SettStartBox box = *itor;
		wxString additionalpath = basepath + _T( "/Rect" ) + TowxString( box.ally ) + _T( "/" );
		m_config->Write( additionalpath + _T( "TopLeftX" ), box.topx );
		m_config->Write( additionalpath + _T( "TopLeftY" ), box.topy );
		m_config->Write( additionalpath + _T( "BottomRightX" ), box.bottomx );
		m_config->Write( additionalpath + _T( "BottomRightY" ), box.bottomy );
		m_config->Write( additionalpath + _T( "AllyTeam" ), box.ally );
	}
}

wxString Settings::GetMapLastStartPosType( const wxString& mapname )
{
	return m_config->Read( _T( "/Hosting/MapLastValues/" ) + mapname + _T( "/startpostype" ), wxEmptyString );
}

std::vector<Settings::SettStartBox> Settings::GetMapLastRectPreset( const wxString& mapname )
{
	wxString basepath = _T( "/Hosting/MapLastValues/" ) + mapname + _T( "/Rects" );
	wxArrayString boxes = cfg().GetGroupList( basepath );
	std::vector<Settings::SettStartBox> ret;
	for ( unsigned int i = 0; i < boxes.GetCount(); i++ )
	{
		wxString additionalpath = basepath + _T( "/" ) + boxes[i] + _T( "/" );
		SettStartBox box;
		box.topx = m_config->Read( additionalpath + _T( "TopLeftX" ), -1 );
		box.topy = m_config->Read( additionalpath + _T( "TopLeftY" ), -1 );
		box.bottomx = m_config->Read( additionalpath + _T( "BottomRightX" ), -1 );
		box.bottomy = m_config->Read( additionalpath + _T( "BottomRightY" ), -1 );
		box.ally = m_config->Read( additionalpath + _T( "AllyTeam" ), -1 );
		ret.push_back( box );
	}
	return ret;
}

bool Settings::GetDisableSpringVersionCheck()
{
	bool ret;
	m_config->Read( _T( "/Spring/DisableVersionCheck" ), &ret, false );
	return ret;
}

void Settings::SetDisableSpringVersionCheck(bool disable)
{
    m_config->Write( _T( "/Spring/DisableVersionCheck" ), ( bool )disable );
}

wxString Settings::GetLastBattleFilterProfileName()
{
	return  m_config->Read( _T( "/BattleFilter/lastprofile" ), _T( "default" ) );
}

wxString Settings::GetTempStorage()
{
	return wxFileName::GetTempDir();
}


void Settings::SetShowTooltips( bool show )
{
	m_config->Write( _T( "/GUI/ShowTooltips" ), show );
}

bool Settings::GetShowTooltips()
{
	return m_config->Read( _T( "/GUI/ShowTooltips" ), 1l );
}

void Settings::RemoveLayouts()
{
	m_config->DeleteEntry(_T("/GUI/DefaultLayout"));
	m_config->DeleteGroup(_T("/Layout"));
	m_config->DeleteGroup(_T("/GUI/AUI"));
}

void Settings::SetColumnWidth( const wxString& list_name, const int column_ind, const int column_width )
{
	m_config->Write( _T( "GUI/ColumnWidths/" ) + list_name + _T( "/" ) + TowxString( column_ind ), column_width );
}

int Settings::GetColumnWidth( const wxString& list_name, const int column )
{
	const int orgwidth = m_config->Read( _T( "/GUI/ColumnWidths/" ) + list_name + _T( "/" ) + TowxString( column ), columnWidthUnset );
	int width = orgwidth;
	if ( orgwidth > -1 ) //-3 is unset, -2 and -1 used for auto size by wx
		width = std::max ( width, int( Settings::columnWidthMinimum ) ); //removing the temporary creation here gives me undefined ref error (koshi)
	return width;
}

void Settings::SaveCustomColors( const wxColourData& _cdata, const wxString& paletteName  )
{
	//note 16 colors is wx limit
	wxColourData cdata = _cdata;
	for ( int i = 0; i < 16; ++i )
	{
		wxColour col = cdata.GetCustomColour( i );
		if ( !col.IsOk() )
			col = wxColour ( 255, 255, 255 );
		m_config->Write( _T( "/CustomColors/" ) + paletteName + _T( "/" ) + TowxString( i ),  col.GetAsString( wxC2S_HTML_SYNTAX ) ) ;
	}
}

wxColourData Settings::GetCustomColors( const wxString& paletteName )
{
	wxColourData cdata;
	//note 16 colors is wx limit
	for ( int i = 0; i < 16; ++i )
	{
		wxColour col( m_config->Read( _T( "/CustomColors/" ) + paletteName + _T( "/" ) + TowxString( i ), wxColour ( 255, 255, 255 ).GetAsString( wxC2S_HTML_SYNTAX ) ) );
		cdata.SetCustomColour( i, col );
	}

	return cdata;
}

PlaybackListFilterValues Settings::GetReplayFilterValues( const wxString& profile_name )
{
	PlaybackListFilterValues filtervalues;
	filtervalues.duration =         m_config->Read( _T( "/ReplayFilter/" ) + profile_name + _T( "/duration" ), wxEmptyString );
	filtervalues.map =               m_config->Read( _T( "/ReplayFilter/" ) + profile_name + _T( "/map" ), wxEmptyString );
	filtervalues.map_show =         m_config->Read( _T( "/ReplayFilter/" ) + profile_name + _T( "/map_show" ), 0L );
	filtervalues.filesize  =        m_config->Read( _T( "/ReplayFilter/" ) + profile_name + _T( "/filesize" ), wxEmptyString );
	filtervalues.filesize_mode  =   m_config->Read( _T( "/ReplayFilter/" ) + profile_name + _T( "/filesize_mode" ), _T( ">" ) );
	filtervalues.duration_mode  =   m_config->Read( _T( "/ReplayFilter/" ) + profile_name + _T( "/duration_mode" ), _T( ">" ) );
	filtervalues.mod =              m_config->Read( _T( "/ReplayFilter/" ) + profile_name + _T( "/mod" ), wxEmptyString );
	filtervalues.mod_show =         m_config->Read( _T( "/ReplayFilter/" ) + profile_name + _T( "/mod_show" ), 0L );
	filtervalues.player_mode =      m_config->Read( _T( "/ReplayFilter/" ) + profile_name + _T( "/player_mode" ), _T( "=" ) );
	filtervalues.player_num  =      m_config->Read( _T( "/ReplayFilter/" ) + profile_name + _T( "/player_num" ), _T( "All" ) );

	return filtervalues;
}

void Settings::SetReplayFilterValues( const PlaybackListFilterValues& filtervalues, const wxString& profile_name )
{
	m_config->Write( _T( "/ReplayFilter/" ) + profile_name + _T( "/duration" ), filtervalues.duration );
	m_config->Write( _T( "/ReplayFilter/" ) + profile_name + _T( "/map" ), filtervalues.map );
	m_config->Write( _T( "/ReplayFilter/" ) + profile_name + _T( "/map_show" ), filtervalues.map_show );
	m_config->Write( _T( "/ReplayFilter/" ) + profile_name + _T( "/filesize" ), filtervalues.filesize );
	m_config->Write( _T( "/ReplayFilter/" ) + profile_name + _T( "/filesize_mode" ), filtervalues.filesize_mode );
	m_config->Write( _T( "/ReplayFilter/" ) + profile_name + _T( "/duration_mode" ), filtervalues.duration_mode );
	m_config->Write( _T( "/ReplayFilter/" ) + profile_name + _T( "/mod" ), filtervalues.mod );
	m_config->Write( _T( "/ReplayFilter/" ) + profile_name + _T( "/mod_show" ), filtervalues.mod_show );
	m_config->Write( _T( "/ReplayFilter/" ) + profile_name + _T( "/player_mode" ), filtervalues.player_mode );
	m_config->Write( _T( "/ReplayFilter/" ) + profile_name + _T( "/player_num" ), filtervalues.player_num );
	m_config->Write( _T( "/ReplayFilter/lastprofile" ), profile_name );
}

bool Settings::GetReplayFilterActivState() const
{
	return m_config->Read( _T( "/ReplayFilter/Active" ) , 0l );
}

void Settings::SetReplayFilterActivState( const bool state )
{
	m_config->Write( _T( "/ReplayFilter/Active" ) , state );
}

wxString Settings::GetLastReplayFilterProfileName()
{
	return  m_config->Read( _T( "/ReplayFilter/lastprofile" ), _T( "default" ) );
}
wxString Settings::GetLastRelayedHost()
{
    return  m_config->Read(_T("/General/RelayHost"),wxEmptyString);
}
void Settings::SetLastRelayedHost(wxString relhost)
{
    m_config->Write(_T("/General/RelayHost"),relhost);
}
void Settings::SetCompletionMethod( CompletionMethod method )
{
	m_config->Write( _T( "/General/CompletionMethod" ), ( int )method );
}
Settings::CompletionMethod Settings::GetCompletionMethod(  ) const
{
	return  ( CompletionMethod )m_config->Read( _T( "/General/CompletionMethod" ), ( int )MatchExact );
}


unsigned int Settings::GetHorizontalSortkeyIndex()
{
	return m_config->Read( _T( "/GUI/MapSelector/HorizontalSortkeyIndex" ), 0l );
}

void Settings::SetHorizontalSortkeyIndex( const unsigned int idx )
{
	m_config->Write( _T( "/GUI/MapSelector/HorizontalSortkeyIndex" ), ( int ) idx );
}

unsigned int Settings::GetVerticalSortkeyIndex()
{
	return m_config->Read( _T( "/GUI/MapSelector/VerticalSortkeyIndex" ), 0l );
}

void Settings::SetVerticalSortkeyIndex( const unsigned int idx )
{
	m_config->Write( _T( "/GUI/MapSelector/VerticalSortkeyIndex" ), ( int ) idx );
}

bool Settings::GetHorizontalSortorder()
{
	return m_config->Read( _T( "/GUI/MapSelector/HorizontalSortorder" ), 0l );
}

void Settings::SetHorizontalSortorder( const bool order )
{
	m_config->Write( _T( "/GUI/MapSelector/HorizontalSortorder" ), order );
}

bool Settings::GetVerticalSortorder()
{
	return m_config->Read( _T( "/GUI/MapSelector/VerticalSortorder" ), 0l );
}

void Settings::SetVerticalSortorder( const bool order )
{
	m_config->Write( _T( "/GUI/MapSelector/VerticalSortorder" ), order );
}

void Settings::SetMapSelectorFollowsMouse( bool value )
{
	m_config->Write( _T( "/GUI/MapSelector/SelectionFollowsMouse" ), value );
}

bool Settings::GetMapSelectorFollowsMouse()
{
	return m_config->Read( _T( "/GUI/MapSelector/SelectionFollowsMouse" ), 0l );
}

unsigned int Settings::GetMapSelectorFilterRadio()
{
	return m_config->Read( _T( "/GUI/MapSelector/FilterRadio" ), 0l );
}

void Settings::SetMapSelectorFilterRadio( const unsigned int val )
{
	m_config->Write( _T( "/GUI/MapSelector/FilterRadio" ), ( int ) val );
}

//////////////////////////////////////////////////////////////////////////////
///                            SpringSettings                              ///
//////////////////////////////////////////////////////////////////////////////


long Settings::getMode()
{
	return m_config->Read( _T( "/SpringSettings/mode" ), SET_MODE_SIMPLE );
}

void Settings::setMode( long mode )
{
	m_config->Write( _T( "/SpringSettings/mode" ), mode );
}

bool Settings::getDisableWarning()
{
	return m_config->Read( _T( "/SpringSettings/disableWarning" ), 0l );
}

void Settings::setDisableWarning( bool disable )
{
	m_config->Write( _T( "/SpringSettings/disableWarning" ), disable );
}


wxString Settings::getSimpleRes()
{
	wxString def = vl_Resolution_Str[1];
	m_config->Read( _T( "/SpringSettings/SimpleRes" ), &def );
	return def;
}
void Settings::setSimpleRes( wxString res )
{
	m_config->Write( _T( "/SpringSettings/SimpleRes" ), res );
}

wxString Settings::getSimpleQuality()
{
	wxString def = wxT( "medium" );
	m_config->Read( _T( "/SpringSettings/SimpleQuality" ), &def );
	return def;
}

void Settings::setSimpleQuality( wxString qual )
{
	m_config->Write( _T( "/SpringSettings/SimpleQuality" ), qual );
}

wxString Settings::getSimpleDetail()
{
	wxString def = wxT( "medium" );
	m_config->Read( _T( "/SpringSettings/SimpleDetail" ), &def );
	return def;
}

void Settings::setSimpleDetail( wxString det )
{
	m_config->Write( _T( "/SpringSettings/SimpleDetail" ), det );
}

bool Settings::IsSpringBin( const wxString& path )
{
	if ( !wxFile::Exists( path ) ) return false;
	if ( !wxFileName::IsFileExecutable( path ) ) return false;
	return true;
}

void Settings::SetLanguageID ( const long id )
{
	m_config->Write( _T( "/General/LanguageID" ) , id );
}

long Settings::GetLanguageID ( )
{
	return m_config->Read( _T( "/General/LanguageID" ) , wxLANGUAGE_DEFAULT  );
}

SortOrder Settings::GetSortOrder( const wxString& list_name )
{
	SortOrder order;
	slConfig::PathGuard pathGuard ( m_config, _T( "/UI/SortOrder/" ) + list_name + _T( "/" ) );
	unsigned int entries  = m_config->GetNumberOfGroups( false ); //do not recurse
	for ( unsigned int i = 0; i < entries ; i++ )
	{
		SortOrderItem it;
		it.direction = m_config->Read( TowxString( i ) + _T( "/dir" ), 1 );
		it.col = m_config->Read( TowxString( i ) + _T( "/col" ), i );
		order[i] = it;
	}
	return order;
}

void Settings::SetSortOrder( const wxString& list_name, const SortOrder& order  )
{
	SortOrder::const_iterator it = order.begin();
	for ( ; it != order.end(); ++it ) {
		m_config->Write( _T( "/UI/SortOrder/" ) + list_name + _T( "/" ) + TowxString( it->first ) + _T( "/dir" ), it->second.direction );
		m_config->Write( _T( "/UI/SortOrder/" ) + list_name + _T( "/" ) + TowxString( it->first ) + _T( "/col" ), it->second.col );
	}
}

int Settings::GetSashPosition( const wxString& window_name )
{
	return m_config->Read( _T( "/GUI/SashPostion/" ) + window_name , 200l );
}

void Settings::SetSashPosition( const wxString& window_name, const int pos )
{
	m_config->Write( _T( "/GUI/SashPostion/" ) + window_name , pos );
}

bool Settings::GetSplitBRoomHorizontally()
{
	return m_config->Read( _T( "/GUI/SplitBRoomHorizontally" ) , 1l );
}

void Settings::SetSplitBRoomHorizontally( const bool vertical )
{
	m_config->Write( _T( "/GUI/SplitBRoomHorizontally" ) , vertical );
}

wxString Settings::GetEditorPath( )
{
    #if defined(__WXMSW__)
        wxString def = wxGetOSDirectory() + sepstring + _T("system32") + sepstring + _T("notepad.exe");
        if ( !wxFile::Exists( def ) )
            def = wxEmptyString;
    #else
        wxString def = wxEmptyString;
    #endif
    return m_config->Read( _T( "/GUI/Editor" ) , def );
}

void Settings::SetEditorPath( const wxString& path )
{
    m_config->Write( _T( "/GUI/Editor" ) , path );
}

bool Settings::GetShowXallTabs()
{
    return m_config->Read( _T( "/GUI/CloseOnAll" ) , 0l );
}

void Settings::SetShowXallTabs( bool show )
{
    m_config->Write( _T( "/GUI/CloseOnAll" ) , show );
}

void Settings::SavePerspective( const wxString& notebook_name, const wxString& perspective_name, const wxString& layout_string )
{
	wxString entry = wxFormat( _T( "/GUI/AUI/%s/%s" ) ) % perspective_name % notebook_name;
    m_config->Write( entry, layout_string );
}

wxString Settings::LoadPerspective( const wxString& notebook_name, const wxString& perspective_name )
{
	wxString entry = wxFormat( _T( "/GUI/AUI/%s/%s" ) ) % perspective_name % notebook_name;
    return m_config->Read( entry , wxEmptyString );
}

wxString Settings::GetLastPerspectiveName( )
{
    return m_config->Read( _T( "/GUI/AUI/lastperspective_name" ), _T("default") );
}

void Settings::SetLastPerspectiveName( const wxString&  name )
{
    m_config->Write( _T( "/GUI/AUI/lastperspective_name" ), name );
}

void Settings::SetAutosavePerspective( bool autosave )
{
    m_config->Write( _T( "/GUI/AUI/autosave" ), autosave );
}

bool Settings::GetAutosavePerspective( )
{
    return m_config->Read( _T( "/GUI/AUI/autosave" ), 1l );
}

wxArrayString Settings::GetPerspectives()
{
    wxArrayString list = cfg().GetGroupList( _T( "/GUI/AUI" ) );
    wxArrayString ret;
    for ( size_t i = 0; i < list.GetCount(); ++i) {
    	if ( !list[i].EndsWith( BattlePostfix ) )
            ret.Add( list[i] );
        else  {
            wxString stripped = list[i].Left( list[i].Len() - BattlePostfix.Len() );
            if ( !PerspectiveExists( stripped ) )
                ret.Add( stripped );
        }
    }
    return ret;
}

bool Settings::PerspectiveExists( const wxString& perspective_name )
{
    wxArrayString list = cfg().GetGroupList( _T( "/GUI/AUI" ) );
    for ( size_t i = 0; i < list.GetCount(); ++i) {
        if ( list[i] == perspective_name )
            return true;
    }
    return false;
}

void Settings::SetAutoloadedChatlogLinesCount( const int count )
{
    m_config->Write( _T( "/GUI/AutoloadedChatlogLinesCount" ), std::abs( count ) );
}

int Settings::GetAutoloadedChatlogLinesCount( )
{
    return m_config->Read( _T( "/GUI/AutoloadedChatlogLinesCount" ), 10l );
}

void Settings::SetUseNotificationPopups( const bool use )
{
	m_config->Write( _T("/GUI/UseNotificationPopups"), use );
}

bool Settings::GetUseNotificationPopups()
{
	return m_config->Read( _T("/GUI/UseNotificationPopups"), true );
}

void Settings::SetNotificationPopupPosition( const size_t index )
{
	m_config->Write( _T("/GUI/NotificationPopupPosition"), (long)index );
}

size_t Settings::GetNotificationPopupPosition()
{
	return m_config->Read( _T("/GUI/NotificationPopupPosition"), (long)ScreenPosition::bottom_right );
}


void Settings::SetNotificationPopupDisplayTime( const unsigned int seconds )
{
	m_config->Write( _T("/GUI/NotificationPopupDisplayTime"), (long)seconds );
}

unsigned int Settings::GetNotificationPopupDisplayTime( )
{
	return m_config->Read( _T("/GUI/NotificationPopupDisplayTime"), 5l );
}

bool Settings::DoResetPerspectives()
{
	return m_config->Read(_T( "/reset_perspectives" ) , 0l );
}

void Settings::SetDoResetPerspectives( bool do_it )
{
	m_config->Write(_T( "/reset_perspectives" ) , (long)do_it );
}

wxString Settings::GetDefaultNick()
{
	return TowxString(LSL::usync().IsLoaded() ? LSL::usync().GetDefaultNick() : "invalid");
}
