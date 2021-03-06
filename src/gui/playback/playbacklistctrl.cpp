/* This file is part of the Springlobby (GPL v2 or later), see COPYING */

#include <wx/intl.h>
#include <wx/menu.h>
#include <wx/filename.h>
#include <wx/log.h>

#include "storedgame.h"
#include "user.h"
#include "iconimagelist.h"
#include "gui/uiutils.h"
#include "gui/ui.h"
#include "utils/conversion.h"
#include "playbacklistctrl.h"
#include "replaylist.h"


BEGIN_EVENT_TABLE(PlaybackListCtrl, CustomVirtListCtrl )

  EVT_LIST_ITEM_RIGHT_CLICK( RLIST_LIST, PlaybackListCtrl::OnListRightClick )
  EVT_MENU                 ( RLIST_DLMAP, PlaybackListCtrl::OnDLMap )
  EVT_MENU                 ( RLIST_DLMOD, PlaybackListCtrl::OnDLMod )
  EVT_LIST_COL_CLICK       ( RLIST_LIST, PlaybackListCtrl::OnColClick )
  EVT_KEY_DOWN			   ( PlaybackListCtrl::OnChar )

END_EVENT_TABLE()


template<class T,class L> SortOrder CustomVirtListCtrl<T,L>::m_sortorder = SortOrder();

PlaybackListCtrl::PlaybackListCtrl( wxWindow* parent  ):
  PlaybackListCtrl::BaseType(parent, RLIST_LIST, wxDefaultPosition, wxDefaultSize,
							wxSUNKEN_BORDER | wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_ALIGN_LEFT,
							_T("PlaybackListCtrl"), 4, &PlaybackListCtrl::CompareOneCrit )
{

	AddColumn( 0, wxLIST_AUTOSIZE, _("Date"), _("Date of recording") );
	AddColumn( 1, wxLIST_AUTOSIZE, _("Game"), _("Game name") );
	AddColumn( 2, wxLIST_AUTOSIZE, _("Map"), _("Mapname") );
	AddColumn( 3, wxLIST_AUTOSIZE_USEHEADER, _("Players"), _("Number of players") );
	AddColumn( 4, wxLIST_AUTOSIZE_USEHEADER, _("Duration"), _T("Duration") );
	AddColumn( 5, wxLIST_AUTOSIZE_USEHEADER, _("Version"), _("Version of the engine") );
	AddColumn( 6, wxLIST_AUTOSIZE, _("Filesize"), _("Filesize in kilobyte") );
	AddColumn( 7, wxLIST_AUTOSIZE, _("File"), _T("Filename") );

    if ( m_sortorder.size() == 0 ) {
      m_sortorder[0].col = 0;
      m_sortorder[0].direction = true;
      m_sortorder[1].col = 1;
      m_sortorder[1].direction = true;
      m_sortorder[2].col = 2;
      m_sortorder[2].direction = true;
      m_sortorder[3].col = 3;
      m_sortorder[3].direction = true;
      Sort( );
    }


    m_popup = new wxMenu( wxEmptyString );
    // &m enables shortcout "alt + m" and underlines m
    m_popup->Append( RLIST_DLMAP, _("Download &map") );
    m_popup->Append( RLIST_DLMOD, _("Download m&od") );
}

PlaybackListCtrl::~PlaybackListCtrl()
{
  delete m_popup;
}

void PlaybackListCtrl::OnListRightClick( wxListEvent& /*unused*/ )
{
  PopupMenu( m_popup );
}

void PlaybackListCtrl::AddPlayback( const StoredGame& replay )
{
	if ( AddItem( &replay ) ) {
		SetColumnWidth(0, wxLIST_AUTOSIZE);
		SetColumnWidth(1, wxLIST_AUTOSIZE);
		SetColumnWidth(2, wxLIST_AUTOSIZE);
		SetColumnWidth(6, wxLIST_AUTOSIZE);
		SetColumnWidth(7, wxLIST_AUTOSIZE);
		return;
	}

    wxLogWarning( _T("Replay already in list.") );
}

void PlaybackListCtrl::RemovePlayback( const StoredGame& replay )
{
    if ( RemoveItem( &replay) )
        return;

    wxLogError( _T("Didn't find the replay to remove.") );
}

void PlaybackListCtrl::OnDLMap( wxCommandEvent& /*unused*/ )
{
    if ( m_selected_index > 0 &&  (long)m_data.size() > m_selected_index ) {
		const OfflineBattle& battle = m_data[m_selected_index]->battle;
        ui().Download("map", battle.GetHostMapName(), battle.GetHostMapHash());
    }
}

void PlaybackListCtrl::OnDLMod( wxCommandEvent& /*unused*/ )
{
    if ( m_selected_index > 0 &&  (long)m_data.size() > m_selected_index ) {
		const OfflineBattle& battle = m_data[m_selected_index]->battle;
        ui().Download("game", battle.GetHostModName(), battle.GetHostModHash());
    }
}

void PlaybackListCtrl::Sort()
{
    if ( m_data.size() > 0 ) {
        SaveSelection();
        SLInsertionSort( m_data, m_comparator );
        RestoreSelection();
    }
}

int PlaybackListCtrl::CompareOneCrit( DataType u1, DataType u2, int col, int dir ) const
{
    switch ( col ) {
        case 0: return dir * compareSimple( u1->date, u2->date );
        case 1: return dir * TowxString(u1->battle.GetHostModName()).CmpNoCase( TowxString(u2->battle.GetHostModName()) );
        case 2: return dir * TowxString(u1->battle.GetHostMapName()).CmpNoCase( TowxString(u2->battle.GetHostMapName()) );
        case 3: return dir * compareSimple( u1->battle.GetNumUsers() - u1->battle.GetSpectators(), u2->battle.GetNumUsers() - u2->battle.GetSpectators() );
        case 4: return dir * compareSimple( u1->duration,u2->duration );
        case 5: return dir * TowxString(u1->SpringVersion).CmpNoCase( TowxString(u2->SpringVersion) ) ;
        case 6: return dir * compareSimple( u1->size, u2->size ) ;
        case 7: return dir * TowxString(u1->Filename).AfterLast( wxFileName::GetPathSeparator() ).CmpNoCase( TowxString(u2->Filename).AfterLast( wxFileName::GetPathSeparator() ) );
        default: return 0;
    }
}

void PlaybackListCtrl::SetTipWindowText( const long item_hit, const wxPoint& position)
{
    if ( item_hit < 0 || item_hit >= (long)m_data.size() )
        return;

    const StoredGame& replay = *GetDataFromIndex( item_hit );

    int column = getColumnFromPosition( position );
    if (column > (int)m_colinfovec.size() || column < 0)
    {
        m_tiptext = wxEmptyString;
    }
    else
    {
        switch (column) {
            case 0: // date
		m_tiptext = TowxString(replay.date_string);
                break;
            case 1: // modname
                m_tiptext = TowxString(replay.ModName);
                break;
            case 2: // mapname
                m_tiptext = TowxString(replay.MapName);
                break;
            case 3: //playernum
                m_tiptext = TowxString(replay.ModName);
                break;
            case 4: // spring version
                m_tiptext = TowxString(replay.SpringVersion);
                break;
            case 5: // filenam
                m_tiptext = TowxString(replay.Filename);
                break;

            default: m_tiptext = wxEmptyString;
            break;
        }
    }
}

wxString PlaybackListCtrl::GetItemText(long item, long column) const
{
    if ( m_data[item] == NULL )
        return wxEmptyString;

    const StoredGame& replay = *m_data[item];

    switch ( column ) {
        case 0: return TowxString(replay.date_string);
        case 1: return TowxString(replay.battle.GetHostModName());
        case 2: return TowxString(replay.battle.GetHostMapName());
		case 3: return wxFormat(_T("%d") ) % ( replay.battle.GetNumUsers() - replay.battle.GetSpectators() );
        case 4: return wxFormat(_T("%02ld:%02ld:%02ld") )
									% (replay.duration / 3600)
									% ((replay.duration%3600)/60)
									% ((replay.duration%60)/60 ) ;
        case 5: return TowxString(replay.SpringVersion);
		case 6: return wxFormat( _T("%d KB") ) % ( replay.size/1024 );
        case 7: return TowxString(replay.Filename).AfterLast( wxFileName::GetPathSeparator() );

        default: return wxEmptyString;
    }
}

int PlaybackListCtrl::GetItemImage(long item) const
{
    if ( m_data[item] == NULL )
        return -1;

    return -1;//icons().GetBattleStatusIcon( *m_data[item] );
}

int PlaybackListCtrl::GetItemColumnImage(long /*item*/, long /*column*/) const
{
    //nothing's been done here atm
    return -1;

//    if ( m_data[item] == NULL )
//        return -1;
//
//    const StoredGame& replay = *m_data[item];
//
//    switch ( column ) {
//        default: return -1;
//    }
}

wxListItemAttr* PlaybackListCtrl::GetItemAttr(long /*unused*/) const
{
    //not neded atm
//    if ( item < m_data.size() && item > -1 ) {
//        const Replay& replay = *m_data[item];
//    }
    return NULL;
}

void PlaybackListCtrl::RemovePlayback( const int index )
{
    if ( index != -1 && index < long(m_data.size()) ) {
        m_data.erase( m_data.begin() + index );
        SetItemCount( m_data.size() );
        RefreshVisibleItems( );
        return;
    }
    wxLogError( _T("Didn't find the replay to remove.") );
}

int PlaybackListCtrl::GetIndexFromData( const DataType& data ) const
{
    DataCIter it = m_data.begin();
    for ( int i = 0; it != m_data.end(); ++it, ++i ) {
        if ( *it != 0 && data->Equals( *(*it) ) )
            return i;
    }
    wxLogError( _T("PlaybackListCtrl: didn't find the battle.") );
    return -1;
}

void PlaybackListCtrl::OnChar(wxKeyEvent & event)
{
	const int keyCode = event.GetKeyCode();
	if ( keyCode == WXK_DELETE )
		DeletePlayback();
	else
		event.Skip();
}


void PlaybackListCtrl::DeletePlayback()
{
	int sel_index = GetSelectedIndex();
	if ( sel_index < 0 ) {
		return;
	}
	try {
		const StoredGame& rep = *GetSelectedData();
		int m_sel_replay_id = rep.id;
		wxLogMessage( _T( "Deleting replay %d " ), m_sel_replay_id );
		wxString fn = TowxString(rep.Filename);
		if ( !replaylist().DeletePlayback( m_sel_replay_id ) )
			customMessageBoxNoModal( SL_MAIN_ICON, _( "Could not delete Replay: " ) + fn,
									 _( "Error" ) );
		else {
			RemovePlayback(GetSelectedIndex());
		}
	} catch ( std::runtime_error ) {}
}
