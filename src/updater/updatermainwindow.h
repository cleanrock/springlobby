/* This file is part of the Springlobby (GPL v2 or later), see COPYING */

#ifndef SPRINGLOBBY_HEADERGUARD_UPDATERMAINWINDOW_H
#define SPRINGLOBBY_HEADERGUARD_UPDATERMAINWINDOW_H

#include <wx/frame.h>

class wxCloseEvent;

class UpdaterMainwindow : public wxFrame
{

public:
	UpdaterMainwindow();
	virtual ~UpdaterMainwindow();

	void OnClose( wxCloseEvent& evt );

	void OnUpdateFinished( wxCommandEvent& /*data*/ );

protected:
	DECLARE_EVENT_TABLE()
};

#endif // SPRINGLOBBY_HEADERGUARD_UPDATERMAINWINDOW_H
