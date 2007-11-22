#include "configHandler.h"
#include "frame_get_usync.h"
#include <iostream>
#ifndef RUNMODE_STANDALONE
	// if extra include for springlobby unitsync method needed
#endif

#define LOCK_UNITSYNC wxCriticalSectionLocker lock_criticalsection(m_lock)

inline wxString _S (const std::string str)
{
	return wxString(str.c_str(),*wxConvCurrent);
}


ConfigHandler* ConfigHandler::instance=0;

bool ConfigHandler::LoadUnitSyncLib( const wxString& springdir, const wxString& unitsyncloc )
{
	if ( is_loaded ) return true;

	wxSetWorkingDirectory( wxT(".") );

	// Load the library.
	std::string loc = getUsyncLoc();
	std::cout <<( "Loading from: " + loc );

	// Check if library exists
	while ( !wxFileName::FileExists( _S(loc)) ) {
		std::cout <<( "File not found: "+ loc );
		GetUSyncFrame* getUSyncFrame = new GetUSyncFrame();
		
		// get new location from dialog
		
		delete getUSyncFrame;
		return false;
	}

	try {
		m_libhandle = new wxDynamicLibrary(_S(loc));
		if (!m_libhandle->IsLoaded()) {
			std::cout <<("wxDynamicLibrary created, but not loaded!");
			delete m_libhandle;
			m_libhandle = 0;
		}
	} catch(...) {
		m_libhandle = 0;
	}

	if (m_libhandle == 0) {
		std::cout <<( "Couldn't load the unitsync library" );
		return false;
	}

	is_loaded = true;

	// Load all function from library.
	try {

		h_GetSpringConfigInt = (GetSpringConfigInt)GetLibFuncPtr("GetSpringConfigInt");
		h_GetSpringConfigString = (GetSpringConfigString)GetLibFuncPtr("GetSpringConfigString");
		h_GetSpringConfigInt = (GetSpringConfigInt)GetLibFuncPtr("GetSpringConfigInt");
		h_SetSpringConfigString = (SetSpringConfigString)GetLibFuncPtr("SetSpringConfigString");
		h_SetSpringConfigInt = (SetSpringConfigInt)GetLibFuncPtr("SetSpringConfigInt");

	}
	catch ( ... ) {
		std::cout <<( "Failed to load Unitsync lib." );
		//_FreeUnitSyncLib();
		return false;
	}

	return true;
}




void* ConfigHandler::GetLibFuncPtr( const std::string& name )
{
	//ASSERT_LOGIC( m_loaded, "Unitsync not loaded" );
	void* ptr = m_libhandle->GetSymbol(_S(name));
	//ASSERT_RUNTIME( ptr, "Couldn't load " + name + " from unitsync library" );
	return ptr;
}

ConfigHandler& ConfigHandler::GetInstance(){
	if (!instance){
		instance = new ConfigHandler();
		instance->LoadUnitSyncLib(wxT(""),wxT(""));
	}
	return *instance;
}

void ConfigHandler::SetInt(std::string name, int value){
	LOCK_UNITSYNC;
	h_SetSpringConfigInt(name.c_str(),value);
}


void ConfigHandler::SetString(std::string name, std::string value){
	LOCK_UNITSYNC;
	h_SetSpringConfigString(name.c_str(),value.c_str());
}


std::string ConfigHandler::GetString(std::string name, std::string def) {
	LOCK_UNITSYNC;
	return h_GetSpringConfigString(name.c_str(),def.c_str());
}

int ConfigHandler::GetInt(std::string name, int def){
	LOCK_UNITSYNC;
	return h_GetSpringConfigInt(name.c_str(),def);
}

ConfigHandler::~ConfigHandler()
{
	Deallocate();
}

void ConfigHandler::Deallocate()
{
	if (instance)
		delete instance;
	instance=0;
}

std::string ConfigHandler::getUsyncLoc()
{
	std::string loc ;
	#ifdef RUNMODE_STANDALONE
		// own method
		
	#else
		// springlobby method
		
	#endif
	
}
	
