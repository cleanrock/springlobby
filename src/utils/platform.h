/* This file is part of the Springlobby (GPL v2 or later), see COPYING */

#ifndef SPRINGLOBBY_HEADERGUARD_PLATFORM_H
#define SPRINGLOBBY_HEADERGUARD_PLATFORM_H

class wxLogWindow;
class wxWindow;
class wxString;
class wxLogChain;

#include <wx/string.h>
#include <wx/arrstr.h>

/**
    let origin be /path/to/some/dir and destination /some/other/path
    this will copy dir (and everything below that recursively to /some/other/path/dir
    \return true if successful
*/
bool CopyDirWithFilebackupRename( wxString from, wxString to, bool overwrite = true, bool backup = true );

//! returns false on !win, checks for regkey on win
bool IsUACenabled();

//! set new cwd in ctor, reset to old in dtor
class CwdGuard {
    wxString m_old_cwd;
    public:
        CwdGuard( const wxString& new_cwd );
        ~CwdGuard();
};

//! remember pwd in ctor and reset in dtor
class PwdGuard {
	wxString m_old_pwd;
	public:
		PwdGuard( );
		~PwdGuard();
};


#ifdef __WXMSW__
bool IsPreVistaWindows();
#endif

/**
  \in Format string with a single %s
  \out wxString with %s replaced with GetAppName()
  **/
wxString IdentityString(const wxString& format, bool lowerCase = false );

int RunProcess(const wxString& cmd, const wxArrayString& params, const bool async = false, const bool root = false);
int BrowseFolder(const wxString& path);
int WaitForExit(int pid);

#ifdef __WXMSW__
#include <wx/msw/registry.h>
template < typename T >
static T GetRegkeyVal( const wxRegKey& reg, const wxString& name, const T& def )
{
	T val = def;
	if ( reg.QueryValue( name, &val ) )
		return val;
	else
		return def;
}

template < >
wxString GetRegkeyVal( const wxRegKey& reg, const wxString& name, const wxString& def )
{
	wxString val = def;
	if ( reg.QueryValue( name, val ) )
		return val;
	else
		return def;
}
#endif

//!

#endif // SPRINGLOBBY_HEADERGUARD_PLATFORM_H
