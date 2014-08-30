/* This file is part of the Springlobby (GPL v2 or later), see COPYING */

#include <wx/intl.h>
#include <wx/stattext.h>
#include <wx/statline.h>
#include <wx/textdlg.h>
#include <wx/combobox.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/checkbox.h>
#include <wx/filename.h>
#include <wx/log.h>
#if wxUSE_TOGGLEBTN
    #include <wx/tglbtn.h>
#endif

#include "playbacktab.h"
#include "playbacklistctrl.h"
#include "replaylist.h"
#include "playbackthread.h"
#include "gui/ui.h"
#include "gui/chatpanel.h"
#include "gui/uiutils.h"
#include "settings.h"
#include "gui/mapctrl.h"
#include "playbackfilter.h"
#include "iconimagelist.h"
#include "storedgame.h"
#include "utils/conversion.h"

#include "gui/customdialogs.h"
#include "gui/hosting/battleroomlistctrl.h"
#include "log.h"

BEGIN_EVENT_TABLE( PlaybackTab, wxScrolledWindow )

    EVT_BUTTON              ( PLAYBACK_WATCH                , PlaybackTab::OnWatch          )
    EVT_BUTTON              ( PLAYBACK_RELOAD               , PlaybackTab::OnReload         )
    EVT_BUTTON              ( PLAYBACK_DELETE               , PlaybackTab::OnDelete         )
    EVT_LIST_ITEM_SELECTED  ( RLIST_LIST                    , PlaybackTab::OnSelect         )
    // this doesn't get triggered (?)
    EVT_LIST_ITEM_DESELECTED( wxID_ANY                      , PlaybackTab::OnDeselect       )
    EVT_CHECKBOX            ( PLAYBACK_LIST_FILTER_ACTIV    , PlaybackTab::OnFilterActiv    )
    EVT_COMMAND             ( wxID_ANY, PlaybackLoader::PlaybacksLoadedEvt  , PlaybackTab::AddAllPlaybacks  )
    #if  wxUSE_TOGGLEBTN
    EVT_TOGGLEBUTTON        ( PLAYBACK_LIST_FILTER_BUTTON   , PlaybackTab::OnFilter         )
    #else
    EVT_CHECKBOX            ( PLAYBACK_LIST_FILTER_BUTTON   , PlaybackTab::OnFilter         )
    #endif

END_EVENT_TABLE()

PlaybackTab::PlaybackTab( wxWindow* parent, bool replay):
	wxScrolledWindow( parent, -1 ),
	m_replay_loader ( 0 ),
	m_isreplay(replay)
{
	wxLogMessage( _T( "PlaybackTab::PlaybackTab()" ) );

	m_replay_loader = new PlaybackLoader( this, true );

	wxBoxSizer* m_main_sizer;
	m_main_sizer = new wxBoxSizer( wxVERTICAL );

	wxBoxSizer* m_filter_sizer;
	m_filter_sizer = new wxBoxSizer( wxVERTICAL );

	wxBoxSizer* m_replaylist_sizer;
	m_replaylist_sizer = new wxBoxSizer( wxVERTICAL );

	m_replay_listctrl = new PlaybackListCtrl( this );
	m_replaylist_sizer->Add( m_replay_listctrl, 1, wxEXPAND);

	m_main_sizer->Add( m_replaylist_sizer, 1, wxEXPAND);;

	wxBoxSizer* m_info_sizer;
	m_info_sizer = new wxBoxSizer( wxHORIZONTAL );

	m_minimap = new MapCtrl( this, 100, 0, true, true, false, false );
	m_info_sizer->Add( m_minimap, 0, wxALL, 5 );

	wxFlexGridSizer* m_data_sizer;
	m_data_sizer = new wxFlexGridSizer( 4, 2, 0, 0 );

	m_map_lbl = new wxStaticText( this, wxID_ANY, _( "Map:" ), wxDefaultPosition, wxDefaultSize, 0 );
	m_data_sizer->Add( m_map_lbl, 1, wxALL | wxEXPAND, 5 );

	m_map_text = new wxStaticText( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_data_sizer->Add( m_map_text, 1, wxALL | wxEXPAND, 5 );

	m_mod_lbl = new wxStaticText( this, wxID_ANY, _( "Game:" ), wxDefaultPosition, wxDefaultSize, 0 );
	m_data_sizer->Add( m_mod_lbl, 1, wxALL | wxEXPAND, 5 );

	m_mod_text = new wxStaticText( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_data_sizer->Add( m_mod_text, 1, wxALL | wxEXPAND, 5 );

	m_players_lbl = new wxStaticText( this, wxID_ANY, _( "Players:" ), wxDefaultPosition, wxDefaultSize, 0 );
	m_data_sizer->Add( m_players_lbl, 1, wxALL | wxEXPAND, 5 );

	m_players_text = new wxStaticText( this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	m_data_sizer->Add( m_players_text, 1, wxALL | wxEXPAND, 5 );

	m_info_sizer->Add( m_data_sizer, 1, wxEXPAND | wxALL, 0 );

	m_players = new BattleroomListCtrl( this, 0, true, false );
	m_info_sizer->Add( m_players , 2, wxALL | wxEXPAND, 0 );

	m_main_sizer->Add( m_info_sizer, 0, wxEXPAND, 5 );


	m_filter = new PlaybackListFilter( this , wxID_ANY, this , wxDefaultPosition, wxSize( -1, -1 ), wxEXPAND );
	m_filter_sizer->Add( m_filter, 0, wxEXPAND, 5 );

	m_main_sizer->Add( m_filter_sizer, 0, wxEXPAND, 5 );

	m_buttons_sep = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
	m_main_sizer->Add( m_buttons_sep, 0, wxALL | wxEXPAND, 5 );

	wxBoxSizer* m_buttons_sizer;
	m_buttons_sizer = new wxBoxSizer( wxHORIZONTAL );

#if  wxUSE_TOGGLEBTN
	m_filter_show = new wxToggleButton( this, PLAYBACK_LIST_FILTER_BUTTON , wxT( " Filter " ), wxDefaultPosition , wxSize( -1, 28 ), 0 );
#else
	m_filter_show = new wxCheckBox( this, PLAYBACK_LIST_FILTER_BUTTON , wxT( " Filter " ), wxDefaultPosition , wxSize( -1, 28 ), 0 );
#endif

	m_buttons_sizer->Add( m_filter_show, 0, 0, 5 );

	m_filter_activ = new wxCheckBox( this, PLAYBACK_LIST_FILTER_ACTIV , wxT( "Activated" ), wxDefaultPosition, wxDefaultSize, 0 );
	m_buttons_sizer->Add( m_filter_activ, 1, wxALL | wxEXPAND, 5 );

	m_buttons_sizer->Add( 0, 0, 1, wxEXPAND, 0 );

	m_watch_btn = new wxButton( this, PLAYBACK_WATCH, replay ? _( "Watch" ) : _( "Load" ), wxDefaultPosition, wxSize( -1, 28 ), 0 );
	m_buttons_sizer->Add( m_watch_btn, 0, wxBOTTOM | wxLEFT | wxRIGHT, 5 );

	m_delete_btn = new wxButton( this, PLAYBACK_DELETE, _( "Delete" ), wxDefaultPosition, wxSize( -1, 28 ), 0 );
	m_buttons_sizer->Add( m_delete_btn, 0, wxBOTTOM | wxRIGHT, 5 );

	m_reload_btn = new wxButton( this, PLAYBACK_RELOAD, _( "Reload list" ), wxDefaultPosition, wxSize( -1, 28 ), 0 );
	m_buttons_sizer->Add( m_reload_btn, 0, wxBOTTOM | wxRIGHT, 5 );

	m_main_sizer->Add( m_buttons_sizer, 0, wxEXPAND, 5 );

	m_filter->Hide();

	SetSizer( m_main_sizer );

    ReloadList();

	//none selected --> shouldn't watch/delete that
	Deselect();

	SetScrollRate( SCROLL_RATE, SCROLL_RATE );
	Layout();
	ConnectGlobalEvent(this, GlobalEvent::OnUnitsyncReloaded, wxObjectEventFunction(&PlaybackTab::OnUnitsyncReloaded));
	ConnectGlobalEvent(this, GlobalEvent::OnSpringTerminated, wxObjectEventFunction(&PlaybackTab::OnSpringTerminated));
}

PlaybackTab::~PlaybackTab()
{
	m_minimap->SetBattle( NULL );
	if ( m_filter != 0 )
		m_filter->SaveFilterValues();

	wxLogDebug("%s");
}

void PlaybackTab::AddAllPlaybacks( wxCommandEvent& /*unused*/ )
{
	assert(wxThread::IsMain());
	const auto& replays = replaylist().GetPlaybacksMap();

	for ( auto i = replays.begin();i != replays.end();++i ) {
		AddPlayback( i->second  );
	}
	m_replay_listctrl->SortList( true );
}

void PlaybackTab::AddPlayback( const StoredGame& replay ) {

	if ( m_filter->GetActiv() && !m_filter->FilterPlayback( replay ) ) {
		return;
	}

	m_replay_listctrl->AddPlayback( replay );
}

void PlaybackTab::RemovePlayback( const StoredGame& replay )
{
	int index = m_replay_listctrl->GetIndexFromData( &replay );

	if ( index == -1 )
		return;

	if ( index == m_replay_listctrl->GetSelectedIndex() )
		Deselect();

	m_replay_listctrl->RemovePlayback( replay );
}

void PlaybackTab::RemovePlayback( const int index )
{
	if ( index == -1 )
		return;

	if ( index == m_replay_listctrl->GetSelectedIndex() )
		Deselect();

	m_replay_listctrl->RemovePlayback( index );
}

void PlaybackTab::UpdatePlayback( const StoredGame& replay )
{
	if ( m_filter->GetActiv() && !m_filter->FilterPlayback( replay ) ) {
		RemovePlayback( replay );
		return;
	}

	int index = m_replay_listctrl->GetIndexFromData( &replay );

	if ( index != -1 )
		m_replay_listctrl->RefreshItem( index );
	else
		AddPlayback( replay );

}

void PlaybackTab::RemoveAllPlaybacks()
{
	m_replay_listctrl->Clear();
	//shouldn't list be cleared too here? (koshi)
}


void PlaybackTab::UpdateList()
{
	const auto& replays = replaylist().GetPlaybacksMap();

	for (auto i = replays.begin(); i != replays.end(); ++i ) {
		UpdatePlayback( i->second );
	}
	m_replay_listctrl->RefreshVisibleItems();
}


void PlaybackTab::SetFilterActiv( bool activ )
{
	m_filter->SetActiv( activ );
	m_filter_activ->SetValue( activ );
}

void PlaybackTab::OnFilter( wxCommandEvent& /*unused*/ )
{
	if ( m_filter_show->GetValue() ) {
		m_filter->Show(  );
		this->Layout();
	}
	else {
		m_filter->Hide(  );
		this->Layout();
	}
}

void PlaybackTab::OnWatch( wxCommandEvent& /*unused*/ )
{
	if ( m_replay_listctrl->GetSelectedIndex() != -1 ) {
		int m_sel_replay_id = m_replay_listctrl->GetSelectedData()->id;

		wxString type = m_isreplay ? _( "replay" ) : _( "savegame" ) ;
		wxLogMessage( _T( "Watching %s %d " ), type.c_str(), m_sel_replay_id );
		try {
			StoredGame& rep = replaylist().GetPlaybackById( m_sel_replay_id );

			bool versionfound = ui().IsSpringCompatible("spring", rep.SpringVersion);
			if ( rep.type == StoredGame::SAVEGAME )
                versionfound = true; // quick hack to bypass spring version check
			if ( !versionfound ) {
				wxString message = wxFormat( _( "No compatible installed spring version has been found, this %s requires version: %s\n" ) ) % type% rep.battle.GetEngineVersion();
				customMessageBox( SL_MAIN_ICON, message, _( "Spring error" ), wxICON_EXCLAMATION | wxOK );
				wxLogWarning ( _T( "no spring version supported by this replay found" ) );
				AskForceWatch( rep );
				return;
			}
            rep.battle.GetMe().SetNick(STD_STRING(sett().GetDefaultNick()));
			bool watchable = rep.battle.MapExists() && rep.battle.ModExists();
			if ( watchable ) {
				rep.battle.StartSpring();
			} else {
				ui().DownloadArchives(rep.battle);
			}
		} catch ( std::runtime_error ) {
			return;
		}
	} else {
		Deselected();
	}
}

void PlaybackTab::AskForceWatch( StoredGame& rep ) const
{
	if ( customMessageBox( SL_MAIN_ICON, _( "I don't think you will be able to watch this replay.\nTry anyways? (MIGHT CRASH!)" ) , _( "Invalid replay" ), wxYES_NO | wxICON_QUESTION ) == wxYES ) {
		rep.battle.StartSpring();
	}
}

void PlaybackTab::OnDelete( wxCommandEvent& /*unused*/ )
{
	int sel_index = m_replay_listctrl->GetSelectedIndex();
	if ( sel_index >= 0 ) {
		try {
			const StoredGame& rep = *m_replay_listctrl->GetSelectedData();
			int m_sel_replay_id = rep.id;
			int index = m_replay_listctrl->GetIndexFromData( &rep );
			wxLogMessage( _T( "Deleting replay %d " ), m_sel_replay_id );
			wxString fn = TowxString(rep.Filename);
			if ( !replaylist().DeletePlayback( m_sel_replay_id ) )
				customMessageBoxNoModal( SL_MAIN_ICON, _( "Could not delete Replay: " ) + fn,
				                         _( "Error" ) );
			else {
				RemovePlayback( index ); // Deselect is called in there too
			}
		} catch ( std::runtime_error ) {
			return;
		}
	} else {
		Deselected();
	}
}

void PlaybackTab::OnFilterActiv( wxCommandEvent& /*unused*/ )
{
	m_filter->SetActiv( m_filter_activ->GetValue() );
}

void PlaybackTab::OnSelect( wxListEvent& event )
{
	slLogDebugFunc("");
	if ( event.GetIndex() == -1 ) {
		Deselect();
	}
	else
	{
		try
		{
			m_watch_btn->Enable( true );
			m_delete_btn->Enable( true );
			int index = event.GetIndex();
			m_replay_listctrl->SetSelectedIndex( index );

			//this might seem a bit backwards, but it's currently the only way that doesn't involve casting away constness
			int m_sel_replay_id = m_replay_listctrl->GetDataFromIndex( index )->id;
			StoredGame& rep = replaylist().GetPlaybackById( m_sel_replay_id );


			wxLogMessage( _T( "Selected replay %d " ), m_sel_replay_id );

			m_players_text->SetLabel( wxEmptyString );
			m_map_text->SetLabel( TowxString(rep.battle.GetHostMapName()) );
			m_mod_text->SetLabel( TowxString(rep.battle.GetHostModName()) );
			m_minimap->SetBattle( &( rep.battle ) );
			m_minimap->UpdateMinimap();

			m_players->Clear();
			m_players->SetBattle( ( IBattle* )&rep.battle );
			for ( size_t i = 0; i < rep.battle.GetNumUsers(); ++i ) {
				try {
					User& usr = rep.battle.GetUser( i );
					m_players->AddUser( usr );
				}
				catch ( ... )
                {}
			}
		}
		catch ( ... ) {
			Deselect();
		}
		event.Skip();
	}
}

void PlaybackTab::OnDeselect( wxListEvent& /*unused*/ )
{
	Deselected();
}

void PlaybackTab::Deselect()
{
	m_replay_listctrl->SelectNone();
	Deselected();
}

void PlaybackTab::Deselected()
{
	m_watch_btn->Enable( false );
	m_delete_btn->Enable( false );
	m_players_text->SetLabel( wxEmptyString );
	m_map_text->SetLabel( wxEmptyString );
	m_mod_text->SetLabel( wxEmptyString );
	m_minimap->SetBattle( NULL );
	m_minimap->UpdateMinimap();
	m_minimap->Refresh();
	m_players->Clear();
	m_players->SetBattle( NULL );
}

void PlaybackTab::ReloadList()
{
	Deselect();
	m_replay_listctrl->Clear();
	m_replay_loader->Run();
}

void PlaybackTab::OnReload( wxCommandEvent& /*unused*/ )
{
	ReloadList();
}

void PlaybackTab::OnSpringTerminated( wxCommandEvent& /*data*/ )
{
    ReloadList();
}

void PlaybackTab::OnUnitsyncReloaded( wxCommandEvent& /*data*/ )
{
	ReloadList();
}