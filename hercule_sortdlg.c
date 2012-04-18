#include <PalmOS.h>

#include "hercule.h"
#include "herculeRsc.h"
#include "hercule_sortdlg.h"
#include "hercule_event.h"
#include "hercule_engine_palm.h"


HerculeClueSortInfo g_sort_info;


static Boolean static_sort_dlg_handle_event( EventPtr event );
static void    static_sort_dlg_init( FormPtr frm );
static void    static_resolve_pop_select_event( EventPtr event, FormPtr frm );


void HerculeRegisterSortDlg( void ) /* {{{ */
{
  HerculeRegisterForm( HerculeSortDlg, static_sort_dlg_handle_event );

  g_sort_info.auto_sort = false;
  g_sort_info.first  = HERCULE_CLUE_LEFT_OF;
  g_sort_info.second = HERCULE_CLUE_NEXT_TO;
  g_sort_info.third  = HERCULE_CLUE_BETWEEN;
  g_sort_info.fourth = HERCULE_CLUE_SAME_COLUMN;
}
/* }}} */


UInt8* HerculeSortDlgStore( UInt16* size ) /* {{{ */
{
  UInt8* ptr;

  *size = sizeof( g_sort_info );

  ptr = (UInt8*)MemPtrNew( *size );
  MemMove( ptr, &g_sort_info, *size );

  return ptr;
}
/* }}} */

void HerculeSortDlgRead( UInt8** buffer ) /* {{{ */
{
  MemMove( &g_sort_info, *buffer, sizeof( g_sort_info ) );
  *buffer += sizeof( g_sort_info );
}
/* }}} */


static Boolean static_sort_dlg_handle_event( EventPtr event ) /* {{{ */
{
  FormPtr frmP;
  EventType newEvent;

  frmP = FrmGetActiveForm();

  switch( event->eType )
  {
    case frmOpenEvent:
      static_sort_dlg_init( frmP );
      FrmDrawForm( frmP );
      return true;

    case ctlSelectEvent:
      switch( event->data.ctlSelect.controlID ) {
        case Btn_Sort:
          MemSet( &newEvent, sizeof( newEvent ), 0 );
          newEvent.eType = HerculeEvent_SortClues;
          EvtAddEventToQueue( &newEvent );
          /* fall through to "OK" */

        case Btn_OK:
          g_sort_info.auto_sort = CtlGetValue( (ControlPtr)GETOBJECT( frmP, HerculePrefs_AutoSort ) );
          g_sort_info.first     = LstGetSelection( (ListPtr)GETOBJECT( frmP, HerculePrefs_SortFirstList ) );
          g_sort_info.second    = LstGetSelection( (ListPtr)GETOBJECT( frmP, HerculePrefs_SortSecondList ) );
          g_sort_info.third     = LstGetSelection( (ListPtr)GETOBJECT( frmP, HerculePrefs_SortThirdList ) );
          g_sort_info.fourth    = LstGetSelection( (ListPtr)GETOBJECT( frmP, HerculePrefs_SortFourthList ) );
          FrmReturnToForm( 0 );
          return true;

        case Btn_Cancel:
          FrmReturnToForm( 0 );
          return true;
      }
      break;

    case popSelectEvent:
      static_resolve_pop_select_event( event, frmP );
      return false;

    default:
  }

  return false;
}
/* }}} */


static void static_sort_dlg_init( FormPtr frm ) /* {{{ */
{
  ControlPtr ctrl;
  ListPtr    list;
  Char*      label;

  ctrl = (ControlPtr)GETOBJECT( frm, HerculePrefs_AutoSort );
  CtlSetValue( ctrl, g_sort_info.auto_sort );

  list = (ListPtr)GETOBJECT( frm, HerculePrefs_SortFirstList );
  ctrl = (ControlPtr)GETOBJECT( frm, HerculePrefs_SortFirstTrigger );
  LstSetSelection( list, g_sort_info.first );
  label = LstGetSelectionText( list, g_sort_info.first );
  CtlSetLabel( ctrl, label );

  list = (ListPtr)GETOBJECT( frm, HerculePrefs_SortSecondList );
  ctrl = (ControlPtr)GETOBJECT( frm, HerculePrefs_SortSecondTrigger );
  LstSetSelection( list, g_sort_info.second );
  label = LstGetSelectionText( list, g_sort_info.second );
  CtlSetLabel( ctrl, label );

  list = (ListPtr)GETOBJECT( frm, HerculePrefs_SortThirdList );
  ctrl = (ControlPtr)GETOBJECT( frm, HerculePrefs_SortThirdTrigger );
  LstSetSelection( list, g_sort_info.third );
  label = LstGetSelectionText( list, g_sort_info.third );
  CtlSetLabel( ctrl, label );

  list = (ListPtr)GETOBJECT( frm, HerculePrefs_SortFourthList );
  ctrl = (ControlPtr)GETOBJECT( frm, HerculePrefs_SortFourthTrigger );
  LstSetSelection( list, g_sort_info.fourth );
  label = LstGetSelectionText( list, g_sort_info.fourth );
  CtlSetLabel( ctrl, label );

  if( g_puzzle == NULL ) {
    FrmHideObject( frm, FrmGetObjectIndex( frm, Btn_Sort ) );
  }
}
/* }}} */

static void static_resolve_pop_select_event( EventPtr event, FormPtr frm ) /* {{{ */
{
  UInt16  listId;
  UInt16  new_sel;
  UInt16  old_sel;
  Char*   label;
  ListPtr list;

  listId  = event->data.popSelect.listID;
  new_sel = event->data.popSelect.selection;
  old_sel = event->data.popSelect.priorSelection;

  list = (ListPtr)GETOBJECT( frm, HerculePrefs_SortFirstList );
  if( ( listId != HerculePrefs_SortFirstList ) && ( LstGetSelection( list ) == new_sel ) )
  {
    LstSetSelection( list, old_sel );
    label = LstGetSelectionText( list, old_sel );
    CtlSetLabel( (ControlPtr)GETOBJECT( frm, HerculePrefs_SortFirstTrigger ), label );
    return;
  }

  list = (ListPtr)GETOBJECT( frm, HerculePrefs_SortSecondList );
  if( ( listId != HerculePrefs_SortSecondList ) && ( LstGetSelection( list ) == new_sel ) )
  {
    LstSetSelection( list, old_sel );
    label = LstGetSelectionText( list, old_sel );
    CtlSetLabel( (ControlPtr)GETOBJECT( frm, HerculePrefs_SortSecondTrigger ), label );
    return;
  }

  list = (ListPtr)GETOBJECT( frm, HerculePrefs_SortThirdList );
  if( ( listId != HerculePrefs_SortThirdList ) && ( LstGetSelection( list ) == new_sel ) )
  {
    LstSetSelection( list, old_sel );
    label = LstGetSelectionText( list, old_sel );
    CtlSetLabel( (ControlPtr)GETOBJECT( frm, HerculePrefs_SortThirdTrigger ), label );
    return;
  }

  list = (ListPtr)GETOBJECT( frm, HerculePrefs_SortFourthList );
  if( ( listId != HerculePrefs_SortFourthList ) && ( LstGetSelection( list ) == new_sel ) )
  {
    LstSetSelection( list, old_sel );
    label = LstGetSelectionText( list, old_sel );
    CtlSetLabel( (ControlPtr)GETOBJECT( frm, HerculePrefs_SortFourthTrigger ), label );
    return;
  }
}
/* }}} */
