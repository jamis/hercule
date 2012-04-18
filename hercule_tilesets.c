#include <PalmOS.h>

#include "hercule.h"
#include "hercule_event.h"
#include "herculeRsc.h"
#include "hercule_tilesets.h"

#define MAX_TILESET_NAME_LEN  ( 20 )

#ifdef COLOR
static UInt16 g_large_tile_sets[ TILE_SETS ][ PUZZLE_COLS ] = {
  { Tile_l11, Tile_l12, Tile_l13, Tile_l14, Tile_l15 },
  { Tile_l21, Tile_l22, Tile_l23, Tile_l24, Tile_l25 },
  { Tile_l31, Tile_l32, Tile_l33, Tile_l34, Tile_l35 },
  { Tile_l41, Tile_l42, Tile_l43, Tile_l44, Tile_l45 },
  { Tile_l51, Tile_l52, Tile_l53, Tile_l54, Tile_l55 },
  { Tile_l61, Tile_l62, Tile_l63, Tile_l64, Tile_l65 },
  { Tile_l71, Tile_l72, Tile_l73, Tile_l74, Tile_l75 },
  { Tile_l81, Tile_l82, Tile_l83, Tile_l84, Tile_l85 },
  { Tile_l91, Tile_l92, Tile_l93, Tile_l94, Tile_l95 },
  { Tile_l10_1, Tile_l10_2, Tile_l10_3, Tile_l10_4, Tile_l10_5 }
};

static UInt16 g_small_tile_sets[ TILE_SETS ][ PUZZLE_COLS ] = {
  { Tile_s11, Tile_s12, Tile_s13, Tile_s14, Tile_s15 },
  { Tile_s21, Tile_s22, Tile_s23, Tile_s24, Tile_s25 },
  { Tile_s31, Tile_s32, Tile_s33, Tile_s34, Tile_s35 },
  { Tile_s41, Tile_s42, Tile_s43, Tile_s44, Tile_s45 },
  { Tile_s51, Tile_s52, Tile_s53, Tile_s54, Tile_s55 },
  { Tile_s61, Tile_s62, Tile_s63, Tile_s64, Tile_s65 },
  { Tile_s71, Tile_s72, Tile_s73, Tile_s74, Tile_s75 },
  { Tile_s81, Tile_s82, Tile_s83, Tile_s84, Tile_s85 },
  { Tile_s91, Tile_s92, Tile_s93, Tile_s94, Tile_s95 },
  { Tile_s10_1, Tile_s10_2, Tile_s10_3, Tile_s10_4, Tile_s10_5 }
};
#else
static UInt16 g_large_tile_sets[ TILE_SETS ][ PUZZLE_COLS ] = {
  { Tile_l11, Tile_l12, Tile_l13, Tile_l14, Tile_l15 },
  { Tile_l21, Tile_l22, Tile_l23, Tile_l24, Tile_l25 },
  { Tile_l31, Tile_l32, Tile_l33, Tile_l34, Tile_l35 },
  { Tile_l41, Tile_l42, Tile_l43, Tile_l44, Tile_l45 },
  { Tile_l51, Tile_l52, Tile_l53, Tile_l54, Tile_l55 },
  { Tile_l61, Tile_l62, Tile_l63, Tile_l64, Tile_l65 },
  { Tile_l71, Tile_l72, Tile_l73, Tile_l74, Tile_l75 },
  { Tile_l81, Tile_l82, Tile_l83, Tile_l84, Tile_l85 }
};

static UInt16 g_small_tile_sets[ TILE_SETS ][ PUZZLE_COLS ] = {
  { Tile_s11, Tile_s12, Tile_s13, Tile_s14, Tile_s15 },
  { Tile_s21, Tile_s22, Tile_s23, Tile_s24, Tile_s25 },
  { Tile_s31, Tile_s32, Tile_s33, Tile_s34, Tile_s35 },
  { Tile_s41, Tile_s42, Tile_s43, Tile_s44, Tile_s45 },
  { Tile_s51, Tile_s52, Tile_s53, Tile_s54, Tile_s55 },
  { Tile_s61, Tile_s62, Tile_s63, Tile_s64, Tile_s65 },
  { Tile_s71, Tile_s72, Tile_s73, Tile_s74, Tile_s75 },
  { Tile_s81, Tile_s82, Tile_s83, Tile_s84, Tile_s85 }
};
#endif

UInt8 g_active_tiles[ PUZZLE_ROWS ] = { 0, 1, 2, 3 };

MemHandle g_large_tiles[ PUZZLE_ROWS ][ PUZZLE_COLS ];
MemHandle g_small_tiles[ PUZZLE_ROWS ][ PUZZLE_COLS ];

Boolean g_random_row_tiles = false;


static Char    **g_tileset_names;
static Boolean   g_ok;


static Boolean static_tilesets_handle_event( EventPtr event );
static void    static_tilesets_dlg_init( FormPtr frm );
static void    static_tilesets_dlg_done( FormPtr frm );
static void    static_resolve_pop_select_event( EventPtr event, FormPtr frm );
static void    static_init_list( FormPtr frm, UInt16 listId, UInt16 triggerId, UInt8 row );
static Boolean static_is_item_swapped( FormPtr frm, EventPtr event, UInt16 listId, UInt16 triggerId );


void HerculeRegisterTileSetsDlg( void ) /* {{{ */
{
  HerculeRegisterForm( HerculeTileSets, static_tilesets_handle_event );
}
/* }}} */



static Boolean static_tilesets_handle_event( EventPtr event ) /* {{{ */
{
  FormPtr frm;

  frm = FrmGetActiveForm();

  switch( event->eType ) {
    case frmOpenEvent:
      static_tilesets_dlg_init( frm );
      FrmDrawForm( frm );
      return true;

    case ctlSelectEvent:
      switch( event->data.ctlSelect.controlID ) {
        case Btn_OK:
        case Btn_Cancel:
          g_ok = ( event->data.ctlSelect.controlID == Btn_OK );
          static_tilesets_dlg_done( frm );
          FrmReturnToForm( 0 );
          return true;
      }
      break;

    case popSelectEvent:
      static_resolve_pop_select_event( event, frm );
      return false;

    default:
  }

  return false;
}
/* }}} */

void HerculeOpenTilesets( void ) /* {{{ */
{
  UInt8 row;
  UInt8 col;

  for( row = 0; row < PUZZLE_ROWS; row++ ) {
    for( col = 0; col < PUZZLE_COLS; col++ ) {
      g_large_tiles[ row ][ col ] = DmGetResource( bitmapRsc, g_large_tile_sets[ g_active_tiles[ row ] ][ col ] );
      if( g_large_tiles[row][col] == NULL ) {
        msg_box( "Could not get bitmap resource for large tile %d,%d", row, col );
        break;
      }
      g_small_tiles[ row ][ col ] = DmGetResource( bitmapRsc, g_small_tile_sets[ g_active_tiles[ row ] ][ col ] );
      if( g_large_tiles[row][col] == NULL ) {
        msg_box( "Could not get bitmap resource for small tile %d,%d", row, col );
        break;
      }
    }
  }
}
/* }}} */

void HerculeCloseTilesets( void ) /* {{{ */
{
  UInt8 row;
  UInt8 col;

  for( row = 0; row < PUZZLE_ROWS; row++ ) {
    for( col = 0; col < PUZZLE_COLS; col++ ) {
      if( g_large_tiles[row][col] != NULL ) DmReleaseResource( g_large_tiles[ row ][ col ] );
      if( g_small_tiles[row][col] != NULL ) DmReleaseResource( g_small_tiles[ row ][ col ] );
      g_large_tiles[row][col] = NULL;
      g_small_tiles[row][col] = NULL;
    }
  }
}
/* }}} */

void HerculeJumbleTilesets( void ) /* {{{ */
{
  UInt8 i;
  UInt8 times;
  UInt8 row1;
  UInt8 row2;
  UInt8 tmp;

  HerculeCloseTilesets();

  SysRandom( TimGetTicks() );
  times = PUZZLE_ROWS/2+1;
  for( i = 0; i < times; i++ ) {
    do {
      row1 = SysRandom( 0 ) % PUZZLE_ROWS;
      row2 = SysRandom( 0 ) % PUZZLE_ROWS;
    } while( row1 == row2 );
    tmp = g_active_tiles[ row1 ];
    g_active_tiles[ row1 ] = g_active_tiles[ row2 ];
    g_active_tiles[ row2 ] = tmp;
  }

  HerculeOpenTilesets();
}
/* }}} */


UInt8* HerculeTilesetsPackage( UInt16* size ) /* {{{ */
{
  UInt8* buffer;

  *size  = sizeof( g_active_tiles );
  *size += sizeof( g_random_row_tiles );

  buffer = (UInt8*)MemPtrNew( *size );
  MemMove( buffer, g_active_tiles, sizeof( g_active_tiles ) );
  MemMove( buffer+sizeof( g_active_tiles ), &g_random_row_tiles, sizeof( g_random_row_tiles ) );

  return buffer;
}
/* }}} */

void HerculeTilesetsUnpackage( UInt8** buffer ) /* {{{ */
{
  MemMove( g_active_tiles, *buffer, sizeof( g_active_tiles ) );
    *buffer += sizeof( g_active_tiles );
  MemMove( &g_random_row_tiles, *buffer, sizeof( g_random_row_tiles ) );
    *buffer += sizeof( g_random_row_tiles );
}
/* }}} */


static void static_tilesets_dlg_init( FormPtr frm ) /* {{{ */
{
  UInt8 i;

  HerculeCloseTilesets();

  g_tileset_names = (Char**)MemPtrNew( TILE_SETS * sizeof( Char* ) );

  /* get the names of each of the tilesets */
  for( i = 0; i < TILE_SETS; i++ ) {
    g_tileset_names[ i ] = (Char*)MemPtrNew( MAX_TILESET_NAME_LEN );
    SysStringByIndex( TileSet_Names, i, g_tileset_names[ i ], MAX_TILESET_NAME_LEN );
  }

  CtlSetValue( (ControlPtr)GETOBJECT( frm, HerculePrefs_RandomizeRows ), g_random_row_tiles );

  static_init_list( frm, HerculePrefs_Row1List, HerculePrefs_Row1Trigger, 0 );
  static_init_list( frm, HerculePrefs_Row2List, HerculePrefs_Row2Trigger, 1 );
  static_init_list( frm, HerculePrefs_Row3List, HerculePrefs_Row3Trigger, 2 );
  static_init_list( frm, HerculePrefs_Row4List, HerculePrefs_Row4Trigger, 3 );
}
/* }}} */

static void static_tilesets_dlg_done( FormPtr frm ) /* {{{ */
{
  UInt8 i;

  if( g_ok ) {
    g_active_tiles[0] = LstGetSelection( (ListPtr)GETOBJECT( frm, HerculePrefs_Row1List ) );
    g_active_tiles[1] = LstGetSelection( (ListPtr)GETOBJECT( frm, HerculePrefs_Row2List ) );
    g_active_tiles[2] = LstGetSelection( (ListPtr)GETOBJECT( frm, HerculePrefs_Row3List ) );
    g_active_tiles[3] = LstGetSelection( (ListPtr)GETOBJECT( frm, HerculePrefs_Row4List ) );

    g_random_row_tiles = CtlGetValue( (ControlPtr)GETOBJECT( frm, HerculePrefs_RandomizeRows ) );
  }

  HerculeOpenTilesets();
  for( i = 0; i < TILE_SETS; i++ ) {
    MemPtrFree( g_tileset_names[ i ] );
  }

  MemPtrFree( g_tileset_names );

  if( g_ok ) {
    FrmUpdateForm( HerculeMain, formPuzzleRedraw );
  }
}
/* }}} */

static void static_resolve_pop_select_event( EventPtr event, FormPtr frm ) /* {{{ */
{
  if( !static_is_item_swapped( frm, event, HerculePrefs_Row1List, HerculePrefs_Row1Trigger ) )
    if( !static_is_item_swapped( frm, event, HerculePrefs_Row2List, HerculePrefs_Row2Trigger ) )
      if( !static_is_item_swapped( frm, event, HerculePrefs_Row3List, HerculePrefs_Row3Trigger ) )
        static_is_item_swapped( frm, event, HerculePrefs_Row4List, HerculePrefs_Row4Trigger );
}
/* }}} */

static void static_init_list( FormPtr frm, UInt16 listId, UInt16 triggerId, UInt8 row ) /* {{{ */
{
  ListPtr list;
  ControlPtr ctl;
  Char* label;

  list = (ListPtr)GETOBJECT( frm, listId );
  ctl  = (ControlPtr)GETOBJECT( frm, triggerId );
  LstSetListChoices( list, g_tileset_names, TILE_SETS );
  LstSetHeight( list, ( TILE_SETS > 6 ? 6 : TILE_SETS ) );
  LstSetSelection( list, g_active_tiles[row] );
  label = LstGetSelectionText( list, g_active_tiles[row] );
  CtlSetLabel( ctl, label );
}
/* }}} */

static Boolean static_is_item_swapped( FormPtr frm, EventPtr event, UInt16 listId, UInt16 triggerId ) /* {{{ */
{
  Char*   label;
  ListPtr list;

  list = (ListPtr)GETOBJECT( frm, listId );
  if( ( event->data.popSelect.listID != listId ) &&
      ( LstGetSelection( list ) == event->data.popSelect.selection ) )
  {
    LstSetSelection( list, event->data.popSelect.priorSelection );
    label = LstGetSelectionText( list, event->data.popSelect.priorSelection );
    CtlSetLabel( (ControlPtr)GETOBJECT( frm, triggerId ), label );
    return true;
  }

  return false;
}
/* }}} */
