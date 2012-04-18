#include <PalmOS.h>
#include <stdarg.h>

#include "hercule.h"
#include "hercule_event.h"
#include "herculeRsc.h"
#include "hercule_engine_palm.h"
#include "progress_dlg.h"
#include "scoredb.h"
#include "hercule_sortdlg.h"
#include "hercule_tilesets.h"


#define DBNAME             "HerculeDB"
#define DBVER              ( 12 )
#define DBVER_LAST         (DBVER-1)

/* DBVER history:
 *   (1) initial version
 *   (2) added g_prefs.auto_advance, g_prefs.auto_deduce, and g_prefs.warn_on_bad
 *   (3) added flag for whether or not the puzzle has been defined yet
 *   (4) added info for game statistics (warnings, undos, play-time)
 *   (5) added the hint counter to game stats
 *   (6) added screen depth
 *   (7) moved the user preference information to the application preferences
 *   (8) added high-score-count and 'last-initials' to the prefs
 *   (9) added show-timer, sort-info, clue read order to the prefs.
 *  (10) added the custom row tile specification, and g_random_row_tiles
 *  (11) added the games_played and average_seconds values
 *  (12) changed total_seconds to UInt32, since UInt16 wasn't big enough.
 */

#define MINVERSION         0x03000000

#define MAX_TUTORIAL_PAGE  8

#define PUZZLE_ACCURACY    5

#define PUZZLE_COL_WIDTH   30
#define PUZZLE_COL_HEIGHT  20

#define CLUE_TILE_WIDTH    10
#define CLUE_TILE_HEIGHT   10

#define CLUE_READ_LEFT_TO_RIGHT   ( 0 )
#define CLUE_READ_TOP_TO_BOTTOM   ( 1 )

#define MAX_UNDO_DEPTH     15

#if defined( REGISTERED )
# define MAX_PUZZLE_SEED    ( 99999L ) 
#else
# define MAX_PUZZLE_SEED    (     9L )
#endif

#define ALL_VALUES ( 0x01 | 0x02 | 0x04 | 0x08 | 0x10 )

#define GETXOFS()         ( ( 160 - ( PUZZLE_COL_WIDTH * PUZZLE_COLS + PUZZLE_COLS ) ) / 2 )
#define GETYOFS()         ( 2 )
#define GETTILEX(ofs,col) ( ofs + (PUZZLE_COL_WIDTH+1)*col )
#define GETTILEY(ofs,row) ( ofs + (PUZZLE_COL_HEIGHT+1)*row )

#define PREV_BUTTON       ( 0x01 )
#define DONE_BUTTON       ( 0x02 )
#define NEXT_BUTTON       ( 0x04 )
#define UP_BUTTON         ( 0x08 )
#define DOWN_BUTTON       ( 0x10 )

#define BUILD_RECT( rect, x1, y1, x2, y2 ) \
  (rect)->topLeft.x = x1; \
  (rect)->topLeft.y = y1; \
  (rect)->extent.x = x2; \
  (rect)->extent.y = y2

typedef struct {
  UInt8   version;
  Boolean puzzle_defined;

  UInt32  seed;
  t_uint8 handicap;
  Boolean assert_exists;
  Boolean auto_deduce;
  Boolean auto_advance;
  Boolean warn_on_bad;

  /* game stat counters */

  UInt16  warnings;
  UInt16  seconds;
  UInt16  undos;
  UInt16  hints;
  UInt32  screen_depth;

  UInt32  started_at;

  UInt8   highScoreCount;
  Char    last_initials[ 4 ];
  Boolean show_timer;
  UInt8   clue_read_order;

  UInt16  games_played;
  UInt32  total_seconds;
} USER_PREFS;


static UInt8     g_screen_depths[ 3 ];

static DmOpenRef g_scores_db;
static LocalID   g_local_id;
static UInt16    g_card_no;
static Int16     g_timer_offset;

static MemHandle g_left_of_tile;
static MemHandle g_same_column_tile;

static MemHandle g_background_tile;

       t_hercule_puzzle         g_puzzle;
static t_hercule_solution_space g_solution;

static USER_PREFS               g_prefs;

/* game stat counters */

static MemHandle                g_puzzle_number_buffer;
static MemHandle                g_warnings_buffer;
static MemHandle                g_undos_buffer;
static MemHandle                g_hints_buffer;
static MemHandle                g_time_buffer;
static MemHandle                g_avgtime_buffer;
static MemHandle                g_initials_buffer;

static Boolean                  g_seen_pendown;
static Boolean                  g_game_ended;
static t_hercule_clue           g_tracking_clue;
static WinHandle                g_saved_bits;
static Int16                    g_last_x;
static Int16                    g_last_y;
static Int8                     g_last_dx;
static Int8                     g_last_dy;
static UInt8                    g_invisible_clues;
static UInt32                   g_move_events_seen;

static UInt8                    g_tutorial_page = 0;
static UInt8                    g_first_line = 0;
static Boolean                  g_more_lines = true;
static UInt8                    g_visible_components;

static WinHandle                g_display_buffer = NULL;

static t_hercule_solution_space_state_stack g_undo_stack;

static Char** g_screen_depth_descriptions;

static UInt16 g_rank;

static UInt16 StartApplication( void );
static void StopApplication( void );
static Err RomVersionCompatible( UInt32 requiredVersion, UInt16 launchFlags );
static Boolean herculeFormHandleEvent( EventPtr event );
static Boolean herculeNewHandleEvent( EventPtr event );
static Boolean herculePrefsHandleEvent( EventPtr event );
static Boolean herculeWinHandleEvent( EventPtr event );
static Boolean herculeTutorialHandleEvent( EventPtr event );
static Boolean herculeScoresHandleEvent( EventPtr event );
static Boolean herculeAverageTimeHandleEvent( EventPtr event );
static Boolean herculeInitialsHandleEvent( EventPtr event );
static void herculeFormInit( FormPtr frm );
static void herculeNewInit( FormPtr frm );
static void herculePrefsInit( FormPtr frm );
static void herculeWinInit( FormPtr frm );
static void herculeScoresInit( FormPtr frm );
static void herculeNewDone( FormPtr frm );
static void herculeWinDone( FormPtr frm );
static void herculeInitialsDone( FormPtr frm );
static void herculeAverageTimeInit( FormPtr frm );
static void herculeAverageTimeDone( FormPtr frm );
static void incrementGameNumber( FormPtr frm, Int8 by );
static UInt32 getSeed( FormPtr frm );
static UInt8 getHandicap( FormPtr frm );
static MemHandle getItemBitmap( t_hercule_coord* item );
static void drawBitmapHandle( MemHandle bmpH, Int16 x, Int16 y );
static Boolean trackPen( FormPtr frm, UInt8 tapcount, Int16 x, Int16 y, Int16 ex, Int16 ey, Boolean penDown );
static Boolean herculeFormAboutHandleEvent( EventPtr event );
static Boolean initData( UInt16 user_id );
static void saveData( UInt16 user_id );
static void updateAssertButtons( FormPtr frm );
static void generateNewPuzzle( FormPtr frm );
static void puzzleReset( FormPtr frm );
static void doUndo( FormPtr frm );
static void doHint( FormPtr frm );
static void setScreenDepth( FormPtr frm );
static void endGame( void );
static void scoreListDrawFunc( Int16 itemNum, RectangleType *bounds, Char **itemsText );
static void nextPuzzle( Boolean notify );
static void static_update_timer( FormPtr frm, Boolean redraw );
static void static_count_invisible_clues( void );
static void static_add_new_win_to_average( void );
static void doClueSwap( FormPtr frm );

/* tutorial routines */

static void showTutorialPageObject( FormPtr frm, UInt16 id, UInt8 bit, Boolean show ) ;
static void drawTutorialPage( FormPtr frm );
static void drawTutorialPage_00( FormPtr frm );
static void drawTutorialPage_01( FormPtr frm );
static void drawTutorialPage_02( FormPtr frm );
static void drawTutorialPage_03( FormPtr frm );
static void drawTutorialPage_04( FormPtr frm );
static void drawTutorialPage_05( FormPtr frm );
static void drawTutorialPage_06( FormPtr frm );
static void drawTutorialPage_07( FormPtr frm );

static Boolean wordWrap( FormPtr frm,
                         UInt16 string_id,
                         UInt16 font_id,
                         UInt16 x, UInt16 y,
                         UInt16 width, UInt16 height,
                         UInt16 first_line,
                         UInt16* end_line,
                         UInt16* end_y );

/* display routines */

static void updateScreen( FormPtr frm,
                          RectangleType* rect,
                          Boolean redraw );

static void drawClueTilesAt( FormPtr frm,
                             Int16 x, Int16 y,
                             Boolean draw_frame,
                             Boolean visible,
                             MemHandle tile1,
                             MemHandle tile2,
                             MemHandle tile3 );

static void drawClueAt( FormPtr frm,
                        t_hercule_clue clue,
                        Int16 x, Int16 y,
                        Boolean draw_frame );

static void drawClue( FormPtr frm,
                      t_hercule_clue clue,
                      Int16 which_clue,
                      Boolean draw_frame );

static void drawPuzzle( FormPtr frm );

static void drawPuzzleBoardAt( FormPtr frm, Int16 x, Int16 y );

static void drawTilesAt( FormPtr frm,
                         Int16 x, Int16 y,
                         Int16 row, Int16 col );

static void drawTileAt( FormPtr frm, Int16 x, Int16 y, Int16 row, Int16 tile );

#ifdef COLOR
static Int16 getIndexOfLeastSignificantBit( UInt32 word );
#endif


static UInt16 StartApplication( void ) /* {{{ */
{
  UInt8 row;
  UInt8 col;
  UInt32 width;
  UInt32 height;
  UInt32 valid_depths;
#ifdef COLOR
  Int16 depth;
#endif
  Boolean color;
  DmSearchStateType searchState;

  HerculeEventInit();

  HerculeRegisterForm( HerculeMain,         herculeFormHandleEvent );
  HerculeRegisterForm( HerculeNew,          herculeNewHandleEvent );
  HerculeRegisterForm( HerculeAbout,        herculeFormAboutHandleEvent );
  HerculeRegisterForm( HerculePrefs,        herculePrefsHandleEvent );
  HerculeRegisterForm( HerculeWin,          herculeWinHandleEvent );
  HerculeRegisterForm( HerculeTutorial,     herculeTutorialHandleEvent );
  HerculeRegisterForm( HerculeHighScores,   herculeScoresHandleEvent );
  HerculeRegisterForm( HerculeAverageScore, herculeAverageTimeHandleEvent );
  HerculeRegisterForm( HerculeGetInitials,  herculeInitialsHandleEvent );

  HerculeRegisterSortDlg();
  HerculeRegisterTileSetsDlg();

  /* set the key-mask so that the page-down key does not cause any events */
  KeySetMask( KeySetMask( 0 ) & ~( keyBitPageDown ) );

  DmGetNextDatabaseByTypeCreator( true, &searchState, sysFileTApplication, DBID, true, 
                                  &g_card_no, &g_local_id );

  /* register to recieve the sleep and wake-up notifications */
  SysNotifyRegister( g_card_no, g_local_id, sysNotifySleepNotifyEvent, NULL, sysNotifyNormalPriority, NULL );
  SysNotifyRegister( g_card_no, g_local_id, sysNotifyLateWakeupEvent, NULL, sysNotifyNormalPriority, NULL );

  /* initialize the tileset */
  for( row = 0; row < PUZZLE_ROWS; row++ ) {
    for( col = 0; col < PUZZLE_COLS; col++ ) {
      g_large_tiles[ row ][ col ] = NULL;
      g_small_tiles[ row ][ col ] = NULL;
    }
  }

  g_left_of_tile = DmGetResource( bitmapRsc, Left_Of );
  g_same_column_tile = DmGetResource( bitmapRsc, Same_Column );
  g_background_tile = DmGetResource( bitmapRsc, Background_Tile );

  g_screen_depth_descriptions = (Char**)MemPtrNew( sizeof(Char*) * 3 );
  g_screen_depth_descriptions[0] = (Char*)MemPtrNew( 14 );
  g_screen_depth_descriptions[1] = (Char*)MemPtrNew( 14 );
  g_screen_depth_descriptions[2] = (Char*)MemPtrNew( 14 );

  if( !initData( 0 ) ) {
    g_prefs.seed = 0;
    g_prefs.handicap = 0;
    g_puzzle = NULL;
    g_solution = NULL;
    g_undo_stack = NULL;
    g_prefs.assert_exists = false;
    g_prefs.warn_on_bad = true;
    g_prefs.auto_deduce = true;
    g_prefs.auto_advance = true;
    g_prefs.highScoreCount = 10;
    StrCopy( g_prefs.last_initials, "" );
    g_prefs.show_timer = true;
    g_prefs.clue_read_order = CLUE_READ_LEFT_TO_RIGHT;
    g_prefs.games_played = 0;
    g_prefs.total_seconds = 0;

    /* find the highest screen depth usable by this device */
    width = 160;
    height = 160;
#ifdef COLOR
    color = true;
#else
    color = false;
#endif
    WinScreenMode( winScreenModeGetSupportedDepths, &width, &height, &valid_depths, &color );

#ifdef COLOR
    depth = getIndexOfLeastSignificantBit( valid_depths & 0xFFFFFF80 );
    if( depth < 0 ) {
      FrmAlert( Alert_herculeNoColor );
      StopApplication();
      return 1;
    }
    g_prefs.screen_depth = depth;
#else
    if( valid_depths & 0x08 ) { /* bit #4 */
      g_prefs.screen_depth = 4;
    } else if( valid_depths & 0x02 ) { /* bit #2 */
      g_prefs.screen_depth = 2;
    } else {
      g_prefs.screen_depth = 1;
    }
#endif
  }

  HerculeOpenTilesets();

  g_game_ended = false;
  g_display_buffer = NULL;
  setScreenDepth( NULL );

  g_scores_db = OpenScoreDB();
  if( g_scores_db == NULL ) {
    msg_box( "Could not open scores database!" );
  }

  return 0;
}
/* }}} */

static void StopApplication( void ) /* {{{ */
{
  FrmCloseAllForms();

  HerculeCloseTilesets();

  DmReleaseResource( g_left_of_tile );
  DmReleaseResource( g_same_column_tile );

  DmReleaseResource( g_background_tile );

  MemPtrFree( g_screen_depth_descriptions[0] );
  MemPtrFree( g_screen_depth_descriptions[1] );
  MemPtrFree( g_screen_depth_descriptions[2] );
  MemPtrFree( g_screen_depth_descriptions );

  if( g_display_buffer != NULL ) WinDeleteWindow( g_display_buffer, false );
  if( g_scores_db != NULL ) CloseScoreDB( g_scores_db );

  SysNotifyUnregister( g_card_no, g_local_id, sysNotifySleepNotifyEvent, sysNotifyNormalPriority );
  SysNotifyUnregister( g_card_no, g_local_id, sysNotifyLateWakeupEvent, sysNotifyNormalPriority );

  HerculeEventCleanup();
}
/* }}} */

static Err RomVersionCompatible( UInt32 requiredVersion, UInt16 launchFlags ) /* {{{ */
{
  UInt32 romVersion;

  // See if we're on in minimum required version of the ROM or later.
  FtrGet( sysFtrCreator, sysFtrNumROMVersion, &romVersion );
  if( romVersion < requiredVersion )
  {
    if( ( launchFlags & ( sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp ) ) == ( sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp ) )
    {
      FrmAlert( RomIncompatibleAlert );
    
      // Pilot 1.0 will continuously relaunch this app unless we switch to 
      // another safe one.
      if( romVersion < 0x02000000 )
      {
        AppLaunchWithCommand( sysFileCDefaultApp, sysAppLaunchCmdNormalLaunch, NULL );
      }
    }
    
    return sysErrRomIncompatible;
  }

  return 0;
}
/* }}} */

static Boolean herculeTutorialHandleEvent( EventPtr event ) /* {{{ */
{
  FormPtr frm;

  frm = FrmGetActiveForm();

  switch( event->eType ) {
    case frmOpenEvent:
      FrmDrawForm( frm );
      g_visible_components = 0xFF;
      showTutorialPageObject( frm, Btn_Next, NEXT_BUTTON, false );
      showTutorialPageObject( frm, Btn_Previous, PREV_BUTTON, false );
      showTutorialPageObject( frm, Btn_Up, UP_BUTTON, false );
      showTutorialPageObject( frm, Btn_Down, DOWN_BUTTON, false );
      drawTutorialPage( frm );
      return true;

    case frmUpdateEvent:
      FrmDrawForm( frm );
      drawTutorialPage( frm );
      return true;

    case ctlSelectEvent:
      switch( event->data.ctlSelect.controlID ) {
        case Btn_Up:
          g_first_line--;
          drawTutorialPage( frm );
          return true;

        case Btn_Down:
          g_first_line++;
          drawTutorialPage( frm );
          return true;

        case Btn_Previous:
          if( g_tutorial_page > 0 ) {
            g_tutorial_page--;
            g_first_line = 0;
            g_more_lines = true;
            drawTutorialPage( frm );
          }
          return true;

        case Btn_OK:
          FrmReturnToForm( 0 );
          return true;

        case Btn_Next:
          if( g_tutorial_page+1 < MAX_TUTORIAL_PAGE ) {
            g_tutorial_page++;
            g_first_line = 0;
            g_more_lines = true;
            drawTutorialPage( frm );
          }
          return true;
       }
      break;

    default:
      /* nothing */
  }

  return false;
}
/* }}} */

static Boolean herculeFormHandleEvent( EventPtr event ) /* {{{ */
{
  FormPtr frm;
  Boolean handled;
  RectangleType rect;

  frm = FrmGetActiveForm();

  if( event->eType == winExitEvent )
  {
    if( event->data.winExit.exitWindow == FrmGetWindowHandle( FrmGetFormPtr( HerculeMain ) ) ) {
      if( g_display_buffer != NULL ) {
        HerculeStopTimer();
        return true;
      }
    }
  }
  else if( event->eType == winEnterEvent )
  {
    if( event->data.winEnter.enterWindow == FrmGetWindowHandle( FrmGetFormPtr( HerculeMain ) ) &&
        event->data.winEnter.enterWindow == FrmGetWindowHandle( FrmGetFirstForm() ) )
    {
      EvtFlushPenQueue();
      if( g_display_buffer != NULL ) {
        HerculeStartTimer( SysTicksPerSecond() );
        static_update_timer( frm, true );
        return true;
      }
    }
  }
  else if( event->eType == frmOpenEvent )
  {
    herculeFormInit( frm );
    FrmDrawForm( frm );
    updateScreen( frm, NULL, true );
    return true;
  }
  else if( event->eType == frmUpdateEvent )
  {
    FrmDrawForm( frm );
    updateScreen( frm, NULL, ( event->data.frmUpdate.updateCode == formPuzzleRedraw ) );
    return true;
  }
  else if( event->eType == frmCloseEvent )
  {
    saveData( 0 ); /* save the session to a database */

    if( g_solution != NULL ) {
      hercule_solution_space_destroy( g_solution );
    }
    if( g_puzzle != NULL ) {
      hercule_puzzle_destroy( g_puzzle );
    }
    if( g_undo_stack != NULL ) {
      hercule_solution_space_state_stack_destroy( g_undo_stack );
    }
    g_solution = NULL;
    g_puzzle = NULL;
    g_undo_stack = NULL;
    return true;
  }
  else if( event->eType == menuEvent )
  {
    switch( event->data.menu.itemID ) {
      case HerculeMainMenu_About:
        FrmPopupForm( HerculeAbout );
        return true;
      case HerculeMainMenu_New:
        FrmPopupForm( HerculeNew );
        return true;
      case HerculeMainMenu_EndPuzzle:
        if( g_game_ended || ( FrmAlert( Alert_herculeEndPuzzle ) == 0 ) ) {
          endGame();
          updateScreen( frm, NULL, true );
        }
        return true;
        
      case Eliminate_CmdBar_Icon:
        g_prefs.assert_exists = false;
        updateAssertButtons( frm );
        return true;

      case Assert_CmdBar_Icon:
        g_prefs.assert_exists = true;
        updateAssertButtons( frm );
        return true;
      case Hercule_Cmd_Undo:
        doUndo( frm );
        return true;
      case Hercule_Cmd_Swap:
        doClueSwap( frm );
        return true;
      case HerculeMainMenu_Prefs:
        FrmPopupForm( HerculePrefs );
        return true;
      case Hercule_Cmd_Reset:
        if( g_solution != NULL ) {
          if( FrmAlert( Alert_herculeReset ) == 0 ) {
            puzzleReset( frm );
          }
        }
        return true;
      case Hercule_Cmd_Hint:
        if( g_solution != NULL ) {
          doHint( frm );
        }
        return true;
      case HerculeMainMenu_Help:
        FrmHelp( Hercule_Help_HowToPlay );
        return true;
      case HerculeMainMenu_Tutorial:
        FrmPopupForm( HerculeTutorial );
        return true;
      case Hercule_Cmd_Scores:
        g_rank = 0;
        FrmPopupForm( HerculeHighScores );
        return true;
      case Hercule_Cmd_Average:
        FrmPopupForm( HerculeAverageScore );
        return true;
#if !defined( REGISTERED )
      case HerculeMainMenu_Register:
        FrmHelp( Hercule_Register );
        return true;
#endif
      case HerculeMainMenu_NextPuzzle:
        if( g_game_ended || FrmAlert( Alert_herculeEndPuzzle ) == 0 ) {
          if( !g_game_ended ) {
            nextPuzzle( true );
          }
          generateNewPuzzle( frm );
          return true;
        }
        break;
      case HerculeMainMenu_Sort:
        FrmPopupForm( HerculeSortDlg );
        return true;
      case HerculeMainMenu_TileSets:
        FrmPopupForm( HerculeTileSets );
        return true;
    }
  }
  else if( event->eType == ctlSelectEvent )
  {
    switch( event->data.ctlSelect.controlID ) {
      case Assert_CmdBar_Icon:
        g_prefs.assert_exists = true;
        break;
      case Eliminate_CmdBar_Icon:
        g_prefs.assert_exists = false;
        break;
      case Hercule_Cmd_Undo:
        doUndo( frm );
        return true;
      case Hercule_Cmd_Swap:
        doClueSwap( frm );
        return true;
      case Hercule_Cmd_Reset:
        if( g_solution != NULL ) {
          if( FrmAlert( Alert_herculeReset ) == 0 ) {
            puzzleReset( frm );
          }
        }
        return true;
      case Hercule_Cmd_Hint:
        if( g_solution != NULL ) {
          doHint( frm );
        }
        return true;
    }
  }
  else if( event->eType == penDownEvent )
  {
    g_seen_pendown = true;
    g_tracking_clue = NULL;
    g_move_events_seen = 0;

    handled = trackPen( frm, event->tapCount,
                        event->screenX,
                        event->screenY,
                        0, 0, true );

    if( !handled ) return false;
    if( !g_game_ended && g_solution != NULL ) {
      if( hercule_solution_space_solved( g_solution ) ) {
        if( hercule_solution_space_correct( g_solution ) ) {
          FrmPopupForm( HerculeWin );
        } else {
          FrmAlert( Alert_herculeIncorrect );
        }
      }
    }

    return true;
  }
  else if( event->eType == penMoveEvent )
  {
    if( ( g_move_events_seen % 2 == 0 ) && ( g_tracking_clue != NULL ) && ( g_saved_bits != NULL ) ) {
      BUILD_RECT( &rect, g_last_x, g_last_y, 32, 12 );
      updateScreen( frm, &rect, false );
      g_last_x = event->screenX - g_last_dx;
      g_last_y = event->screenY - g_last_dy;
      BUILD_RECT( &rect, 0, 0, 32, 12 );
      WinCopyRectangle( g_saved_bits, FrmGetWindowHandle( frm ), &rect, g_last_x, g_last_y, winPaint );
    }
    g_move_events_seen++;
  }
  else if( event->eType == penUpEvent )
  {
    if( !g_seen_pendown ) return false;
    g_seen_pendown = false;

    g_tracking_clue = NULL;
    if( g_saved_bits != NULL ) {
      WinDeleteWindow( g_saved_bits, false );
      g_saved_bits = NULL;
    }

    handled = trackPen( frm, event->tapCount,
                        event->data.penUp.start.x,
                        event->data.penUp.start.y,
                        event->data.penUp.end.x,
                        event->data.penUp.end.y,
                        false );

    return handled;
  }
  else if( event->eType == HerculeEvent_Timer )
  {
    static_update_timer( frm, true );
    return true;
  }
  else if( event->eType == HerculeEvent_SortClues )
  {
    hercule_clue_sort( g_puzzle, &g_sort_info );
    updateScreen( frm, NULL, true );
    return true;
  }

  return false;
}
/* }}} */

static Boolean herculeNewHandleEvent( EventPtr event ) /* {{{ */
{
  FormPtr frm;
  Boolean handled = false;

  frm = FrmGetActiveForm();

  if( event->eType == frmOpenEvent )
  {
    herculeNewInit( frm );
    FrmDrawForm( frm );
    return true;
  }
  else if( event->eType == ctlRepeatEvent )
  {
    switch( event->data.ctlSelect.controlID ) {
      case Icon_Left:
        incrementGameNumber( frm, -1 );
        break;

      case Icon_Right:
        incrementGameNumber( frm, 1 );
        break;
    }
  }
  else if( event->eType == ctlSelectEvent )
  {
    switch( event->data.ctlSelect.controlID ) {
      case Puzzle_Cancel:
        herculeNewDone( frm );
        FrmReturnToForm( 0 );
        return true;

      case Puzzle_Create:
        g_prefs.seed = getSeed( frm );
        if( g_prefs.seed > MAX_PUZZLE_SEED ) {
          g_prefs.seed = MAX_PUZZLE_SEED;
#if !defined( REGISTERED )
          FrmAlert( Alert_herculeUnregistered );
#endif
        }
        g_prefs.handicap = getHandicap( frm );

        herculeNewDone( frm );
        FrmReturnToForm( 0 );
        generateNewPuzzle( FrmGetActiveForm() );

        return true;
    }
  }

  return handled;
}
/* }}} */

static Boolean herculePrefsHandleEvent( EventPtr event ) /* {{{ */
{
  FormPtr frm;
  UInt16  depth;
  UInt8   read_order;
  Boolean need_redraw;

  frm = FrmGetActiveForm();

  if( event->eType == frmOpenEvent )
  {
    herculePrefsInit( frm );
    FrmDrawForm( frm );
    return true;
  }
  else if( event->eType == ctlSelectEvent )
  {
    switch( event->data.ctlSelect.controlID ) {
      case Btn_OK:
        g_prefs.warn_on_bad = CtlGetValue( (ControlPtr)GETOBJECT( frm, HerculePrefs_WarnBad ) );
        g_prefs.auto_advance = CtlGetValue( (ControlPtr)GETOBJECT( frm, HerculePrefs_AutoAdvance ) );
        g_prefs.auto_deduce = CtlGetValue( (ControlPtr)GETOBJECT( frm, HerculePrefs_AutoDeduce ) );
        g_prefs.show_timer = CtlGetValue( (ControlPtr)GETOBJECT( frm, HerculePrefs_ShowTime ) );
        read_order = ( CtlGetValue( (ControlPtr)GETOBJECT( frm, HerculePrefs_ReadOrderLtR ) ) ?
              CLUE_READ_LEFT_TO_RIGHT : CLUE_READ_TOP_TO_BOTTOM );

        depth = g_screen_depths[ LstGetSelection( (ListPtr)GETOBJECT( frm, HerculePrefs_ColorList ) ) ];

        FrmReturnToForm( 0 );

        need_redraw = false;

        if( read_order != g_prefs.clue_read_order ) {
          g_prefs.clue_read_order = read_order;
          need_redraw = true;
        }

        if( depth != g_prefs.screen_depth ) {
          g_prefs.screen_depth = depth;
          setScreenDepth( FrmGetActiveForm() );
          need_redraw = false;
        }

        if( need_redraw ) {
          updateScreen( FrmGetActiveForm(), NULL, true );
        }

        return true;

      case Btn_Cancel:
        FrmReturnToForm( 0 );
        return true;
    }
  }

  return false;
}
/* }}} */

static Boolean herculeWinHandleEvent( EventPtr event ) /* {{{ */
{
  FormPtr frm;

  frm = FrmGetActiveForm();

  if( event->eType == frmOpenEvent )
  {
    herculeWinInit( frm );
    FrmDrawForm( frm );
    return true;
  }
  else if( event->eType == frmCloseEvent )
  {
    /* this is in case they tap 'home' and exit hercule while on this form... otherwise,
     * the close event is never received. */
    g_game_ended = true;
    nextPuzzle( false );
    return true;
  }
  else if( event->eType == ctlSelectEvent )
  {
    switch( event->data.ctlSelect.controlID ) {
      case Btn_OK:
        g_game_ended = true;
        herculeWinDone( frm );
        FrmReturnToForm( 0 );

        nextPuzzle( g_prefs.auto_advance );

        if( g_prefs.auto_advance ) {
          generateNewPuzzle( FrmGetActiveForm() );
        }
        return true;
    }
  }

  return false;
}
/* }}} */

static Boolean herculeAverageTimeHandleEvent( EventPtr event ) /*{{{*/
{
  FormPtr frm;

  frm = FrmGetActiveForm();

  if( event->eType == frmOpenEvent )
  {
    g_hints_buffer = MemHandleNew( 6 );    /* games played */
    g_time_buffer = MemHandleNew( 9 );     /* total time */
    g_avgtime_buffer = MemHandleNew( 8 );  /* time/game */

    herculeAverageTimeInit( frm );
    FrmDrawForm( frm );
    return true;
  }
  else if( event->eType == frmUpdateEvent )
  {
    herculeAverageTimeInit( frm );
    FrmDrawForm( frm );
    return true;
  }
  else if( event->eType == frmCloseEvent )
  {
    herculeAverageTimeDone( frm );
    return true;
  }
  else if( event->eType == ctlSelectEvent )
  {
    switch( event->data.ctlSelect.controlID ) {
      case Btn_OK:
        herculeAverageTimeDone( frm );
        FrmReturnToForm( 0 );
        return true;

      case Hercule_Avg_ResetBtn:
        if( FrmAlert( Alert_herculeResetAverage ) == 0 ) {
          g_prefs.total_seconds = 0;
          g_prefs.games_played = 0;
          FrmUpdateForm( FrmGetFormId( frm ), 0 );
        }
        return true;
    }
  }

  return false;
}
/*}}}*/

static Boolean herculeScoresHandleEvent( EventPtr event ) /* {{{ */
{
  FormPtr frm;

  frm = FrmGetActiveForm();

  if( event->eType == frmOpenEvent )
  {
    herculeScoresInit( frm );
    FrmDrawForm( frm );
    return true;
  }
  else if( event->eType == frmUpdateEvent )
  {
    herculeScoresInit( frm );
    FrmDrawForm( frm );
    return true;
  }
  else if( event->eType == ctlSelectEvent )
  {
    switch( event->data.ctlSelect.controlID ) {
      case Btn_OK:
        FrmReturnToForm( 0 );
        return true;

      case Btn_Reset:
        if( FrmAlert( Alert_herculeDeleteScores ) == 0 ) {
          ClearScores( g_scores_db );
          FrmUpdateForm( FrmGetFormId( frm ), 0 );
        }
        return true;
    }
  }

  return false;
}
/* }}} */

static Boolean herculeInitialsHandleEvent( EventPtr event ) /* {{{ */
{
  FormPtr frm;
  Char*   ptr;
  SCORE_RECORD score;

  frm = FrmGetActiveForm();

  if( event->eType == frmOpenEvent )
  {
    g_initials_buffer = MemHandleNew( 4 );
    ptr = (Char*)MemHandleLock( g_initials_buffer );
    StrCopy( ptr, g_prefs.last_initials );
    MemHandleUnlock( g_initials_buffer );
    FldSetTextHandle( (FieldPtr)GETOBJECT( frm, HerculeInitials ), g_initials_buffer );
    FrmDrawForm( frm );
    FrmSetFocus( frm, FrmGetObjectIndex( frm, HerculeInitials ) );
    return true;
  }
  else if( event->eType == ctlSelectEvent )
  {
    switch( event->data.ctlSelect.controlID ) {
      case Btn_OK:
      case Btn_Cancel:
        if( event->data.ctlSelect.controlID == Btn_OK ) {
          StrCopy( g_prefs.last_initials, FldGetTextPtr( (FieldPtr)GETOBJECT( frm, HerculeInitials ) ) );
        } else {
          StrCopy( g_prefs.last_initials, "" );
        }

        StrCopy( score.player, g_prefs.last_initials );

        score.date = TimGetSeconds();
        score.seed = g_prefs.seed;
        score.seconds = g_prefs.seconds;
        score.handicap = g_prefs.handicap;
        score.auto_deduce = g_prefs.auto_deduce;
        score.warn_on_bad = g_prefs.warn_on_bad;
        score.warnings = g_prefs.warnings;
        score.undos = g_prefs.undos;
        score.hints = g_prefs.hints;

        AddScore( g_scores_db, &score, g_prefs.highScoreCount, &g_rank );
        herculeInitialsDone( frm );
        FrmReturnToForm( 0 );

        /* display the high scores */
        FrmPopupForm( HerculeHighScores );
        return true;
    }
  }

  return false;
}
/* }}} */

static void herculeFormInit( FormPtr frm ) /* {{{ */
{
  updateAssertButtons( frm );

  if( g_solution != NULL ) {
    FrmSetMenu( FrmGetActiveForm(), HerculeMainMenu_InGame );
  }
}
/* }}} */

static void herculeNewInit( FormPtr frm ) /* {{{ */
{
  FieldPtr field;
  ListPtr  lst;
  ControlPtr ctlPtr;
  Char*    buffer;
  Char*    label;

  g_puzzle_number_buffer = MemHandleNew( 6 );
  buffer = (Char*)MemHandleLock( g_puzzle_number_buffer );
  StrPrintF( buffer, "%ld", g_prefs.seed );
  MemHandleUnlock( g_puzzle_number_buffer );

  field = (FieldPtr)GETOBJECT( frm, Game_Number );
  FldSetTextHandle( field, g_puzzle_number_buffer );

  lst = (ListPtr)GETOBJECT( frm, Handicap_List );
  LstSetSelection( lst, g_prefs.handicap );
  label = LstGetSelectionText( lst, g_prefs.handicap );

  ctlPtr = (ControlPtr)GETOBJECT( frm, Handicap_Trigger );
  CtlSetLabel( ctlPtr, label );
}
/* }}} */

static void herculePrefsInit( FormPtr frm ) /* {{{ */
{
  Char* label;
  ListPtr lst;
  ControlPtr ctlPtr;
  UInt16 idx;
  UInt32 width;
#ifndef COLOR
  UInt32 height;
  Boolean color_enabled;
  UInt32 valid_depths;
#endif

  CtlSetValue( (ControlPtr)GETOBJECT( frm, HerculePrefs_AutoDeduce ), g_prefs.auto_deduce );
  CtlSetValue( (ControlPtr)GETOBJECT( frm, HerculePrefs_AutoAdvance ), g_prefs.auto_advance );
  CtlSetValue( (ControlPtr)GETOBJECT( frm, HerculePrefs_WarnBad ), g_prefs.warn_on_bad );
  CtlSetValue( (ControlPtr)GETOBJECT( frm, HerculePrefs_ShowTime ), g_prefs.show_timer );
  CtlSetValue( (ControlPtr)GETOBJECT( frm, HerculePrefs_ReadOrderLtR ),
    ( g_prefs.clue_read_order == CLUE_READ_LEFT_TO_RIGHT ) );

#ifdef COLOR
  width = 0;
  g_screen_depths[ 0 ] = g_prefs.screen_depth;
  StrCopy( g_screen_depth_descriptions[ width ], "Color" );
  width++;
#else
  width = 160;
  height = 160;
  color_enabled = false;
  WinScreenMode( winScreenModeGetSupportedDepths, &width, &height, &valid_depths, &color_enabled );

  /* okay, we're cheating and reusing 'width' as the count of supported screen depths */
  width = 0;
  if( valid_depths & 0x01 ) { /* bit #1 - 1-bit color */
    StrCopy( g_screen_depth_descriptions[ width ], "Black & White" );
    g_screen_depths[ width++ ] = 1;
  }
  if( valid_depths & 0x02 ) { /* bit #2 - 2-bit color */
    StrCopy( g_screen_depth_descriptions[ width ], "4 Level Grey" );
    g_screen_depths[ width++ ] = 2;
  }
  if( valid_depths & 0x08 ) { /* bit #4 - 4-bit color */
    StrCopy( g_screen_depth_descriptions[ width ], "16 Level Grey" );
    g_screen_depths[ width++ ] = 4;
  }
#endif

  lst = (ListPtr)GETOBJECT( frm, HerculePrefs_ColorList );
  LstSetListChoices( lst, g_screen_depth_descriptions, width );
  LstSetHeight( lst, width );

  /* find the index in the g_screen_depths array for the current screen depth */
  for( idx = 0; idx < width; idx++ ) {
    if( g_screen_depths[ idx ] == g_prefs.screen_depth ) {
      break;
    }
  }
  if( idx == width ) idx = 0;

  LstSetSelection( lst, idx );
  label = LstGetSelectionText( lst, idx );

  ctlPtr = (ControlPtr)GETOBJECT( frm, HerculePrefs_ColorTrigger );
  CtlSetLabel( ctlPtr, label );
}
/* }}} */

static void herculeWinInit( FormPtr frm ) /* {{{ */
{
  Char* ptr;
  SCORE_RECORD score;
  int avg;

  g_warnings_buffer = MemHandleNew( 6 );
  g_undos_buffer = MemHandleNew( 6 );
  g_hints_buffer = MemHandleNew( 6 );
  g_time_buffer = MemHandleNew( 8 );
  g_avgtime_buffer = MemHandleNew( 8 );

  ptr = (Char*)MemHandleLock( g_warnings_buffer );
  StrPrintF( ptr, "%d", g_prefs.warnings );
  MemHandleUnlock( g_warnings_buffer );

  ptr = (Char*)MemHandleLock( g_undos_buffer );
  StrPrintF( ptr, "%d", g_prefs.undos );
  MemHandleUnlock( g_undos_buffer );

  ptr = (Char*)MemHandleLock( g_hints_buffer );
  StrPrintF( ptr, "%d", g_prefs.hints );
  MemHandleUnlock( g_hints_buffer );

  g_prefs.seconds += ( TimGetTicks() - g_prefs.started_at ) / SysTicksPerSecond();
  ptr = (Char*)MemHandleLock( g_time_buffer );
  StrPrintF( ptr, "%d:%02d", g_prefs.seconds/60, g_prefs.seconds%60 );
  MemHandleUnlock( g_time_buffer );

  static_add_new_win_to_average();

  avg = g_prefs.total_seconds / g_prefs.games_played;
  ptr = (Char*)MemHandleLock( g_avgtime_buffer );
  StrPrintF( ptr, "%d:%02d", avg/60, avg%60 );
  MemHandleUnlock( g_avgtime_buffer );

  FldSetTextHandle( (FieldPtr)GETOBJECT( frm, HerculeWin_Warnings ), g_warnings_buffer );
  FldSetTextHandle( (FieldPtr)GETOBJECT( frm, HerculeWin_Undos ), g_undos_buffer );
  FldSetTextHandle( (FieldPtr)GETOBJECT( frm, HerculeWin_Hints ), g_hints_buffer );
  FldSetTextHandle( (FieldPtr)GETOBJECT( frm, HerculeWin_Time ), g_time_buffer );
  FldSetTextHandle( (FieldPtr)GETOBJECT( frm, HerculeWin_Average ), g_avgtime_buffer );
  
  score.date = TimGetSeconds();
  score.seed = g_prefs.seed;
  score.seconds = g_prefs.seconds;
  score.handicap = g_prefs.handicap;
  score.auto_deduce = g_prefs.auto_deduce;
  score.warn_on_bad = g_prefs.warn_on_bad;
  score.warnings = g_prefs.warnings;
  score.undos = g_prefs.undos;
  score.hints = g_prefs.hints;

  if( ScoreInTop( g_scores_db, &score, g_prefs.highScoreCount ) ) {
    FrmPopupForm( HerculeGetInitials );
  }
}
/* }}} */

static void herculeScoresInit( FormPtr frm ) /* {{{ */
{
  ListPtr  lst;

  lst = (ListPtr)GETOBJECT( frm, HerculeHighScore_List );

  LstSetDrawFunction( lst, scoreListDrawFunc );
  LstSetListChoices( lst, NULL, DmNumRecords( g_scores_db ) );

  /* select the score that was just attained */
  if( g_rank > 0 ) {
    LstSetSelection( lst, g_rank-1 );
  }
}
/* }}} */

static void herculeNewDone( FormPtr frm ) /* {{{ */
{
  FldSetTextHandle( (FieldPtr)GETOBJECT( frm, Game_Number ), NULL );
  MemHandleFree( g_puzzle_number_buffer );
}
/* }}} */

static void herculeWinDone( FormPtr frm ) /* {{{ */
{
  FldSetTextHandle( (FieldPtr)GETOBJECT( frm, HerculeWin_Warnings ), NULL );
  FldSetTextHandle( (FieldPtr)GETOBJECT( frm, HerculeWin_Undos ), NULL );
  FldSetTextHandle( (FieldPtr)GETOBJECT( frm, HerculeWin_Hints ), NULL );
  FldSetTextHandle( (FieldPtr)GETOBJECT( frm, HerculeWin_Time ), NULL );
  FldSetTextHandle( (FieldPtr)GETOBJECT( frm, HerculeWin_Average ), NULL );
  MemHandleFree( g_warnings_buffer );
  MemHandleFree( g_undos_buffer );
  MemHandleFree( g_hints_buffer );
  MemHandleFree( g_time_buffer );
  MemHandleFree( g_avgtime_buffer );
}
/* }}} */

static void herculeInitialsDone( FormPtr frm ) /* {{{ */
{
  FldSetTextHandle( (FieldPtr)GETOBJECT( frm, HerculeInitials ), NULL );
  MemHandleFree( g_initials_buffer );
}
/* }}} */

static void herculeAverageTimeInit( FormPtr frm ) /*{{{*/
{
  int avg;
  Char* ptr;

  ptr = (Char*)MemHandleLock( g_hints_buffer );
  StrPrintF( ptr, "%d", g_prefs.games_played );
  MemHandleUnlock( g_hints_buffer );

  ptr = (Char*)MemHandleLock( g_time_buffer );
  if( g_prefs.total_seconds >= 60*60 ) { /* at least an hour */
    StrPrintF( ptr, "%d:%02d:%02d", g_prefs.total_seconds/(60*60),
                                    (g_prefs.total_seconds%(60*60))/60,
                                    g_prefs.total_seconds%60 );
  } else {
    StrPrintF( ptr, "%d:%02d", g_prefs.total_seconds/60, g_prefs.total_seconds%60 );
  }
  MemHandleUnlock( g_time_buffer );

  ptr = (Char*)MemHandleLock( g_avgtime_buffer );

  if( g_prefs.games_played > 0 )
  {
    avg = g_prefs.total_seconds / g_prefs.games_played;
    StrPrintF( ptr, "%d:%02d", avg/60, avg%60 );
  }
  else
  {
    StrCopy( ptr, "(n/a)" );
  }
  MemHandleUnlock( g_avgtime_buffer );

  FldSetTextHandle( (FieldPtr)GETOBJECT( frm, Hercule_Avg_GamesPlayed ), g_hints_buffer );
  FldSetTextHandle( (FieldPtr)GETOBJECT( frm, Hercule_Avg_TotalTime ), g_time_buffer );
  FldSetTextHandle( (FieldPtr)GETOBJECT( frm, Hercule_Avg_AverageTime ), g_avgtime_buffer );
}
/*}}}*/

static void herculeAverageTimeDone( FormPtr frm ) /*{{{*/
{
  FldSetTextHandle( (FieldPtr)GETOBJECT( frm, Hercule_Avg_GamesPlayed ), NULL );
  FldSetTextHandle( (FieldPtr)GETOBJECT( frm, Hercule_Avg_TotalTime ), NULL );
  FldSetTextHandle( (FieldPtr)GETOBJECT( frm, Hercule_Avg_AverageTime ), NULL );
  MemHandleFree( g_hints_buffer );
  MemHandleFree( g_time_buffer );
  MemHandleFree( g_avgtime_buffer );
}
/*}}}*/

void msg_box( Char* fmt, ... ) /* {{{ */
{
  va_list arg;
  Char buffer[ 128 ];

  va_start( arg, fmt );
  StrVPrintF( buffer, fmt, arg );
  va_end( arg );

  FrmCustomAlert( Alert_herculeDebug, buffer, "", "" );
}
/* }}} */

static void incrementGameNumber( FormPtr frm, Int8 by ) /* {{{ */
{
  FieldPtr field;
  Char*    buffer;

  field = (FieldPtr)GETOBJECT( frm, Game_Number );

  g_prefs.seed = StrAToI( FldGetTextPtr( field ) );

  if( ( by < 0 ) && ( -by > g_prefs.seed ) ) {
    g_prefs.seed = 0;
  } else if( g_prefs.seed + by > MAX_PUZZLE_SEED ) {
    g_prefs.seed = MAX_PUZZLE_SEED;
  } else {
    g_prefs.seed += by;
  }

  buffer = (Char*)MemHandleLock( g_puzzle_number_buffer );
  StrPrintF( buffer, "%ld", g_prefs.seed );
  MemHandleUnlock( g_puzzle_number_buffer );

  FldSetTextHandle( field, g_puzzle_number_buffer );
  FldDrawField( field );
} /* }}} */

static UInt32 getSeed( FormPtr frm ) /* {{{ */
{
  FieldPtr field;
  field = (FieldPtr)GETOBJECT( frm, Game_Number );
  return StrAToI( FldGetTextPtr( field ) );
}
/* }}} */

static UInt8 getHandicap( FormPtr frm ) /* {{{ */
{
  ListPtr  lst;
  lst = (ListPtr)GETOBJECT( frm, Handicap_List );
  return LstGetSelection( lst );
}
/* }}} */

static MemHandle getItemBitmap( t_hercule_coord* item ) /* {{{ */
{
  return g_small_tiles[ item->row ][ hercule_puzzle_at( g_puzzle, item->row, item->col ) ];
}
/* }}} */

static void drawBitmapHandle( MemHandle bmpH, Int16 x, Int16 y ) /* {{{ */
{
  BitmapPtr bmp;
  bmp = (BitmapPtr)MemHandleLock( bmpH );
  WinDrawBitmap( bmp, x, y );
  MemHandleUnlock( bmpH );
}
/* }}} */

static Boolean trackPen( FormPtr frm, UInt8 tapcount, Int16 x, Int16 y, Int16 ex, Int16 ey, Boolean penDown ) /* {{{ */
{
  UInt8 row;
  UInt8 col;
  Int16 xofs;
  Int16 yofs;
  UInt8 i;
  UInt8 value;
  UInt8 which_clue;
  UInt8 which_clue2;
  UInt8 clue_idx;
  UInt8 clue_idx2;
  UInt8 max_row;
  UInt8 max_col;
  t_hercule_clue* clues;
  t_uint8 clue_count;
  t_hercule_clue tmp;
  RectangleType rect;
  Boolean assert_exists;

  if( g_solution == NULL ) {
    if( ( x >= 50 ) && ( x <= 108 ) && ( y >= 105 ) && ( y <= 136 ) ) {
      FrmPopupForm( HerculeNew );
      return true;
    }
    return false;
  }

  /* each grid is:
   *   (PUZZLE_COL_WIDTH+1) pixels wide
   *   (PUZZLE_COL_HEIGHT+1) pixels tall
   * the grid is offset by
   *   +xofs, +yofs
   *
   * the grid number of a given pixel is obtained by:
   *   (x-xofs)/(PUZZLE_COL_WIDTH+1), (y-yofs)/(PUZZLE_COL_HEIGHT+1)
   */

  if( !g_game_ended && penDown && y < ( 2 + PUZZLE_COL_HEIGHT * PUZZLE_ROWS + PUZZLE_ROWS ) )
  { /* we're in the puzzle area */
    xofs = GETXOFS();
    yofs = GETYOFS();
    col = ( x - xofs ) / ( PUZZLE_COL_WIDTH+1 );
    row = ( y - yofs ) / ( PUZZLE_COL_HEIGHT+1 );

    if( ( col < PUZZLE_COLS ) && ( row < PUZZLE_ROWS ) )
    { /* valid bounds, let's figure out what, exactly, was clicked */
      xofs += col * ( PUZZLE_COL_WIDTH+1 );
      yofs += row * ( PUZZLE_COL_HEIGHT+1 );

      if( hercule_solution_space_solved_at( g_solution, row, col ) ) {
        /* if the square was already solved, and it was clicked, then we reset the square to be
         * all possible options for that square */
        if( x < xofs+5 || x > xofs+25 ) {
          /* nothing clicked but grey */
          return false;
        }
        hercule_solution_space_state_stack_push( g_undo_stack, g_solution );
        value = hercule_solution_space_at( g_solution, row, col );
        hercule_solution_space_put( g_solution, row, col, ALL_VALUES );
        for( i = 0; i < PUZZLE_COLS; i++ ) {
          hercule_solution_space_put( g_solution, row, i,
              hercule_solution_space_at( g_solution, row, i ) | value );
        }
      } else {
        /* otherwise, find the exact small icon clicked and assert that it does/doesn't exist there */
        if( y - yofs > 10 ) { /* second row */
          if( x < xofs+5 || x > xofs+25 ) {
            /* nothing clicked but grey */
            return false;
          }
          i = 3 + (x-xofs-5)/10;
          i = ( i > 4 ? 4 : i );
        } else {
          i = (x-xofs)/10;
          i = ( i > 2 ? 2 : i );
        }

        if( ( hercule_solution_space_at( g_solution, row, col ) & ( 1 << i ) ) == 0 ) {
          /* grey area -- item has been removed.  Since it doesn't exist there, let's put
           * it back */
          hercule_solution_space_state_stack_push( g_undo_stack, g_solution );
          hercule_solution_space_put( g_solution, row, col,
              hercule_solution_space_at( g_solution, row, col ) | ( 1 << i ) );
        } else {
          /* let the "page-down" key perform the reverse of the current operation */
          assert_exists = g_prefs.assert_exists;
          if( KeyCurrentState() & keyBitPageDown ) assert_exists = !assert_exists;

          value = hercule_puzzle_at( g_puzzle, row, col );
          if( g_prefs.warn_on_bad && ( ( assert_exists && ( value != i ) ) || ( !assert_exists && ( value == i ) ) ) ) {
            g_prefs.warnings++;
            FrmAlert( Alert_herculeWarn );
          } else {
            hercule_solution_space_state_stack_push( g_undo_stack, g_solution );
            hercule_solution_space_assert( g_solution, row, col, i, assert_exists, g_prefs.auto_deduce );
          }
        }
      }

      xofs = xofs - col * ( PUZZLE_COL_WIDTH+1 );
      for( i = 0; i < PUZZLE_COLS; i++ ) {
        drawTilesAt( frm, GETTILEX( xofs, i )+1, yofs+1, row, i );
      }

      BUILD_RECT( &rect,
                  GETXOFS(), GETYOFS() + ( PUZZLE_COL_HEIGHT + 1 ) * row,
                  ( PUZZLE_COL_WIDTH+1 ) * PUZZLE_COLS, PUZZLE_COL_HEIGHT+1 );
      updateScreen( frm, &rect, false );

      return true;
    }
  } else if( y > 90 ) {
    yofs = 90;
    xofs = 4;

    col = ( x - xofs ) / 40;
    row = ( y - yofs ) / 12;

    clue_count = hercule_puzzle_get_clue_count( g_puzzle );
    clues = hercule_puzzle_open_clues( g_puzzle );

    max_col = 4;
    max_row = ( clue_count - g_invisible_clues - 1 ) / max_col + 1;

    /* which visible clue is it? */
    if( g_prefs.clue_read_order == CLUE_READ_LEFT_TO_RIGHT ) {
      /* 4r+c */
      value = row * max_col + col;
    } else {
      /* (max_row)*c+r */
      value = max_row * col + row;
    }

    if( ( col >= max_col ) || ( row >= max_row ) ) {
      hercule_puzzle_close_clues( g_puzzle );
      return false;
    }

    which_clue = 0;
    which_clue2 = 0;
    clue_idx = clue_count;
    clue_idx2 = clue_count;

    /* find which clue was the first one clicked */
    for( i = 0; i < clue_count; i++ ) {
      if( hercule_clue_get_clue_type( clues[i] ) != HERCULE_CLUE_GIVEN ) {
        if( which_clue == value ) {
          clue_idx = i;
          break;
        }
        which_clue++;
      }
    }

    if( penDown ) {

      if( clue_idx < clue_count ) {
        Int16 error;

        g_tracking_clue = clues[ clue_idx ];
        g_last_dx = x - ( xofs + col * 40 + 1 );
        g_last_dy = y - ( yofs + row * 11 );
        g_last_x = x - g_last_dx;
        g_last_y = y - g_last_dy;
        BUILD_RECT( &rect, g_last_x-1, g_last_y-1, 32, 12 );
        g_saved_bits = WinSaveBits( &rect, &error );
      }

      hercule_puzzle_close_clues( g_puzzle );
      return (clue_idx < clue_count);
    }

    if( clue_idx < clue_count ) {
      col = ( ex - xofs ) / 40;
      row = ( ey - yofs ) / 12;

      /* which visible clue is it? */
      if( g_prefs.clue_read_order == CLUE_READ_LEFT_TO_RIGHT ) {
        /* 4r+c */
        value = row * max_col + col;
      } else {
        /* (max_row)*c+r */
        value = max_row * col + row;
      }

      if( ( col >= max_col ) || ( row >= max_row ) ) {
        value = 0xFF;
      }

      /* find which clue was the second one clicked (where the pen was lifted) */
      for( i = 0; i < clue_count; i++ ) {
        if( hercule_clue_get_clue_type( clues[i] ) != HERCULE_CLUE_GIVEN ) {
          if( which_clue2 == value ) {
            clue_idx2 = i;
            break;
          }
          which_clue2++;
        }
      }

      /* if the clues are different, then we are dealing with clues having been moved.  Otherwise,
       * we are being asked to toggle whether a clue is visible or not. */

      if( ( clue_idx2 < clue_count ) && ( which_clue2 != which_clue ) ) {
        tmp = clues[ clue_idx ];
        clues[ clue_idx ] = clues[ clue_idx2 ];
        clues[ clue_idx2 ] = tmp;
        drawClue( frm, clues[ clue_idx ], which_clue, false );
        drawClue( frm, clues[ clue_idx2 ], which_clue2, false );
      } else {
        if( hercule_clue_get_data( clues[ clue_idx ] ) == 1 ) {
          hercule_clue_set_data( clues[ clue_idx ], 0 );
        } else {
          hercule_clue_set_data( clues[ clue_idx ], 1 );
        }
        drawClue( frm, clues[ clue_idx ], which_clue, false );
      }

      updateScreen( frm, NULL, false );
    }

    hercule_puzzle_close_clues( g_puzzle );
    return true;
  }
   
  return false;
}
/* }}} */

static Boolean herculeFormAboutHandleEvent( EventPtr event ) /* {{{ */
{
  FormPtr frm;

  frm = FrmGetActiveForm();
  switch( event->eType ) {
    case frmOpenEvent:
      FrmDrawForm( frm );
      return true;

    case ctlSelectEvent:
      switch( event->data.ctlSelect.controlID ) {
        case Btn_OK:
          FrmReturnToForm( 0 );
          return true;
      }
      break;

    default:
  }

  return false;
}
/* }}} */

static Boolean initData( UInt16 user_id ) /* {{{ */
{
  MemHandle rec;
  UInt8*    recptr;
  DmOpenRef db;
  LocalID   dbId;
  Boolean   gotData = false;

  dbId = DmFindDatabase( 0, DBNAME );
  if( dbId == NULL ) {
    return gotData;
  }

  db = DmOpenDatabase( 0, dbId, dmModeReadOnly );
  if( db == NULL ) {
    msg_box( "The preferences database could not be opened." );
    return gotData;
  }

  rec = DmQueryRecord( db, 0 );
  recptr = (UInt8*)MemHandleLock( rec );

  MemMove( &g_prefs, recptr, sizeof( g_prefs ) );
  recptr += sizeof( g_prefs );

  /* set the start-at time to the current time */
  g_prefs.started_at = TimGetTicks();

  /* make sure the preferences version matches */
  if( g_prefs.version == DBVER )
  {
    HerculeSortDlgRead( &recptr );
    HerculeTilesetsUnpackage( &recptr );

    if( g_prefs.puzzle_defined ) {
      hercule_unserialize_session( recptr, &g_puzzle, &g_solution, &g_undo_stack );
      static_count_invisible_clues();
      HerculeStartTimer( SysTicksPerSecond() );
    } else {
      g_puzzle = NULL;
      g_solution = NULL;
      g_undo_stack = NULL;
    }

    gotData = true;
  }
  else if( g_prefs.version == DBVER_LAST )
  {
    /* change recptr so that it points to the end of the OLD
     * g_prefs record... */

    recptr -= sizeof( UInt16 );

    HerculeSortDlgRead( &recptr );
    HerculeTilesetsUnpackage( &recptr );

    g_prefs.puzzle_defined = false;
    g_prefs.version = DBVER;

    g_puzzle = g_solution = g_undo_stack = NULL;

    g_prefs.total_seconds >>= sizeof( UInt16 );

    gotData = true;
  }

  MemHandleUnlock( rec );
  DmCloseDatabase( db );

  return gotData;
}
/* }}} */

static void saveData( UInt16 user_id ) /* {{{ */
{
  MemHandle rec;
  UInt8*    data;
  UInt8*    dataP;
  UInt8*    recP;
  UInt8*    sortP;
  UInt8*    tileP;
  UInt16    size;
  UInt16    sort_size;
  UInt16    tile_size;
  UInt16    total_size;
  DmOpenRef db;
  LocalID   dbId;
  UInt16    idx;

  dbId = DmFindDatabase( 0, DBNAME );
  if( dbId == NULL ) {
    DmCreateDatabase( 0, DBNAME, DBID, DBTYPE, false );
    dbId = DmFindDatabase( 0, DBNAME );
  }

  db = DmOpenDatabase( 0, dbId, dmModeReadWrite );
  while( DmNumRecords( db ) > 0 ) {
    DmRemoveRecord( db, 0 );
  }

  g_prefs.version = DBVER;

  if( !g_game_ended && g_puzzle != NULL ) {
    g_prefs.puzzle_defined = true;
    size = hercule_serialize_session( NULL, g_puzzle, g_solution, g_undo_stack, true );
  } else { 
    g_prefs.puzzle_defined = false;
    size = 0;
  }

  /* increment the time that the puzzle has been active by the difference between when this
   * session was started and now */

  g_prefs.seconds += ( TimGetTicks() - g_prefs.started_at ) / SysTicksPerSecond();
  sortP = HerculeSortDlgStore( &sort_size );
  tileP = HerculeTilesetsPackage( &tile_size );

  total_size = size + sizeof( g_prefs ) + sort_size + tile_size;

  dataP = data = MemPtrNew( total_size );

  MemMove( dataP, &g_prefs, sizeof( g_prefs ) );
    dataP += sizeof( g_prefs );
  MemMove( dataP, sortP, sort_size );
    dataP += sort_size;
  MemMove( dataP, tileP, tile_size );
    dataP += tile_size;
  if( g_prefs.puzzle_defined ) {
    hercule_serialize_session( dataP, g_puzzle, g_solution, g_undo_stack, false );
  }

  idx = 0;
  rec = DmNewRecord( db, &idx, total_size );
  recP = (UInt8*)MemHandleLock( rec );

  DmWrite( recP, 0, data, total_size );

  MemPtrFree( data );
  MemPtrFree( sortP );
  MemPtrFree( tileP );
  MemHandleUnlock( rec );
  DmReleaseRecord( db, idx, true );

  DmCloseDatabase( db );
}
/* }}} */

static void updateAssertButtons( FormPtr frm ) /* {{{ */
{
  CtlSetValue( (ControlPtr)GETOBJECT( frm, Eliminate_CmdBar_Icon ), !g_prefs.assert_exists );
  CtlSetValue( (ControlPtr)GETOBJECT( frm, Assert_CmdBar_Icon ), g_prefs.assert_exists );
}
/* }}} */

static void generateNewPuzzle( FormPtr frm ) /* {{{ */
{
  HerculeStopTimer();

  if( g_puzzle != NULL ) {
    hercule_puzzle_destroy( g_puzzle );
  }

  g_puzzle = hercule_puzzle_new( PUZZLE_ROWS, PUZZLE_COLS, g_prefs.seed+1, g_prefs.handicap, PUZZLE_ACCURACY );
  static_count_invisible_clues();

  g_prefs.warnings = 0;
  g_prefs.seconds = 0;
  g_prefs.undos = 0;

  g_prefs.started_at = TimGetTicks();
  g_game_ended = false;

  puzzleReset( frm );
  if( g_random_row_tiles ) {
    HerculeJumbleTilesets();
  }

  HerculeStartTimer( SysTicksPerSecond() );

  /* throw away any lingering pen-up events... */
  EvtFlushPenQueue();
}
/* }}} */

static void puzzleReset( FormPtr frm ) /* {{{ */
{
  t_hercule_clue* clues;
  t_uint8 clue_count;
  t_uint8 i;

  if( g_puzzle == NULL ) {
    return;
  }

  if( g_solution != NULL ) {
    hercule_solution_space_destroy( g_solution );
  }

  if( g_undo_stack != NULL ) {
    hercule_solution_space_state_stack_destroy( g_undo_stack );
  }

  g_solution = hercule_solution_space_new( g_puzzle );
  g_undo_stack = hercule_solution_space_state_stack_new( MAX_UNDO_DEPTH, ON_OVERFLOW_DELETE_OLDEST );

  clue_count = hercule_puzzle_get_clue_count( g_puzzle );
  clues = hercule_puzzle_open_clues( g_puzzle );
  for( i = 0; i < clue_count; i++ ) {
    hercule_clue_set_data( clues[i], 0 ); /* reset the invisibility flag for the given clue */
    if( hercule_clue_get_clue_type( clues[i] ) == HERCULE_CLUE_GIVEN ) {
      hercule_solution_space_apply( g_solution, clues[i] );
    }
  }
  hercule_puzzle_close_clues( g_puzzle );

  FrmUpdateForm( FrmGetFormId( frm ), formPuzzleRedraw );
  FrmSetMenu( frm, HerculeMainMenu_InGame );

  g_prefs.assert_exists = false;
  updateAssertButtons( frm );

  g_prefs.warnings = 0;
  g_prefs.seconds = 0;
  g_prefs.undos = 0;
  g_prefs.hints = 0;
  g_prefs.started_at = TimGetTicks();

  g_game_ended = false;
}
/* }}} */

static void doUndo( FormPtr frm ) /* {{{ */
{
  if( !g_game_ended && g_solution != NULL ) {
    hercule_solution_space_state_stack_pop( g_undo_stack, g_solution );
    g_prefs.undos++;
    updateScreen( frm, NULL, true );
  }
}
/* }}} */

static void doHint( FormPtr frm ) /* {{{ */
{
  t_hercule_hint hint;
  EventType event;
  t_bool on;
  Int16 xofs;
  Int16 yofs;
  t_uint8 on_val;
  t_uint8 off_val;
  t_uint8 which_clue;
  t_uint8 i;
  t_hercule_clue* clues;

  if( g_game_ended ) return;

  if( hercule_solution_space_hint( g_solution, &hint ) ) {
    g_prefs.hints++;

    on = false;
    xofs = GETTILEX( GETXOFS(), hint.position.col )+1;
    yofs = GETTILEY( GETYOFS(), hint.position.row )+1;

    /* figure out which *visible* clue this is (not which clue it is, sequentially) */
    clues = hercule_puzzle_open_clues( g_puzzle );
    which_clue = 0;
    for( i = 0; i < hint.which_clue; i++ ) {
      if( hercule_clue_get_clue_type( clues[i] ) != HERCULE_CLUE_GIVEN ) {
        which_clue++;
      }
    }
    hercule_puzzle_close_clues( g_puzzle );

    on_val = hercule_solution_space_at( g_solution, hint.position.row, hint.position.col );

    if( hint.assert ) {
      off_val = 1 << hint.value;
    } else {
      off_val = on_val & ~( 1 << hint.value );
    }

    /* first, eat all events waiting in the queue... */
    while( true ) {
      EvtGetEvent( &event, 0 );
      if( event.eType == nilEvent ) break;
    }

    /* blink! */
    do {
      if( on ) {
        hercule_solution_space_put( g_solution, hint.position.row, hint.position.col, on_val );
        hercule_clue_set_data( hint.clue, 0 );
      } else {
        hercule_solution_space_put( g_solution, hint.position.row, hint.position.col, off_val );
        hercule_clue_set_data( hint.clue, 1 );
      }

      drawClue( frm, hint.clue, which_clue, false );
      drawTilesAt( frm, xofs, yofs, hint.position.row, hint.position.col );
      updateScreen( frm, NULL, false );

      EvtGetEvent( &event, SysTicksPerSecond() / ( on ? 3 : 5 ) );
      if( event.eType != nilEvent ) {
        break;
      }

      on = !on;
    } while( true );

    hercule_clue_set_data( hint.clue, 0 );
    hercule_solution_space_put( g_solution, hint.position.row, hint.position.col, on_val );

    drawClue( frm, hint.clue, which_clue, false );
    drawTilesAt( frm, xofs, yofs, hint.position.row, hint.position.col );
    updateScreen( frm, NULL, false );

    HerculeHandleEvent( &event );
  }
}
/* }}} */

static void setScreenDepth( FormPtr frm ) /* {{{ */
{
#ifdef COLOR
  Boolean color = true;
#else
  Boolean color = false;
#endif

  if( g_display_buffer != NULL ) {
    WinDeleteWindow( g_display_buffer, false );
    g_display_buffer = NULL;
  }

  WinScreenMode( winScreenModeSet, NULL, NULL, &g_prefs.screen_depth, &color );

  if( frm != NULL ) {
    FrmUpdateForm( FrmGetFormId( frm ), formPuzzleRedraw );
  }
}
/* }}} */

static void endGame( void ) /* {{{ */
{
  if( g_solution != NULL ) {
    hercule_solution_space_destroy( g_solution );
  }
  if( g_puzzle != NULL ) {
    hercule_puzzle_destroy( g_puzzle );
  }
  if( g_undo_stack != NULL ) {
    hercule_solution_space_state_stack_destroy( g_undo_stack );
  }
  g_solution = NULL;
  g_puzzle = NULL;
  g_undo_stack = NULL;
  g_game_ended = false;
  FrmSetMenu( FrmGetActiveForm(), HerculeMainMenu );
}
/* }}} */

static void scoreListDrawFunc( Int16 itemNum, RectangleType *bounds, Char **itemsText ) /* {{{ */
{
  Char buffer[ 10 ];
  UInt16 x;
  UInt16 zero_width;
  SCORE_RECORD* recP;
  MemHandle rec;

  rec = DmQueryRecord( g_scores_db, itemNum );
  recP = (SCORE_RECORD*)MemHandleLock( rec );

  zero_width = FntCharWidth( '0' );

  x = bounds->topLeft.x;

  StrPrintF( buffer, "%d)", itemNum+1 );
  WinDrawChars( buffer, StrLen( buffer ),
                x + zero_width * 3 - FntCharsWidth( buffer, StrLen( buffer ) ),
                bounds->topLeft.y );
  x += zero_width * 4;

  StrPrintF( buffer, "%d:%02d", recP->seconds/60, recP->seconds%60 );
  WinDrawChars( buffer, StrLen( buffer ),
                x + zero_width * 5 - FntCharsWidth( buffer, StrLen( buffer ) ),
                bounds->topLeft.y );
  x += zero_width * 6;

  if( recP->seed < 1000 ) {
    StrPrintF( buffer, "%ld", recP->seed );
  } else {
    StrPrintF( buffer, "%ld,%03ld", recP->seed / 1000, recP->seed % 1000 );
  }

  WinDrawChars( buffer, StrLen( buffer ),
                x + zero_width * 6 - FntCharsWidth( buffer, StrLen( buffer ) ),
                bounds->topLeft.y );
  x += zero_width * 7;

  StrPrintF( buffer, "%d", recP->handicap );
  WinDrawChars( buffer, StrLen( buffer ),
                x, bounds->topLeft.y );
  x += zero_width * 2;

  StrPrintF( buffer, "%d", recP->hints );
  WinDrawChars( buffer, StrLen( buffer ),
                x + zero_width * 2 - FntCharsWidth( buffer, StrLen( buffer ) ),
                bounds->topLeft.y );
  x += zero_width * 3;

  StrPrintF( buffer, "%d", recP->warnings );
  WinDrawChars( buffer, StrLen( buffer ),
                x + zero_width * 2 - FntCharsWidth( buffer, StrLen( buffer ) ),
                bounds->topLeft.y );
  x += zero_width * 3;

  WinDrawChars( recP->player, StrLen( recP->player ),
                x, bounds->topLeft.y );
  x += zero_width * 4;

  MemHandleUnlock( rec );
  DmReleaseRecord( g_scores_db, itemNum, false );
}
/* }}} */

static void nextPuzzle( Boolean notify ) /* {{{ */
{
  g_prefs.seed++; /* auto-increment to the next puzzle */
  if( g_prefs.seed > MAX_PUZZLE_SEED ) {
    g_prefs.seed = MAX_PUZZLE_SEED;
#if !defined( REGISTERED )
    if( notify && g_prefs.auto_advance ) {
      FrmAlert( Alert_herculeUnregistered );
    }
#endif
  }
}
/* }}} */

static void static_update_timer( FormPtr frm, Boolean redraw ) /* {{{ */
{
  UInt32    total_seconds;
  Char      buffer[ 12 ];
  WinHandle old_window;
  RectangleType rect;

  if( g_puzzle == NULL || g_game_ended ) {
    return;
  }

  total_seconds = g_prefs.seconds + ( TimGetTicks() - g_prefs.started_at ) / SysTicksPerSecond();
  StrPrintF( buffer, "%ld:%02ld:%02ld", total_seconds/3600, total_seconds/60, total_seconds%60 );

  rect.topLeft.x = g_timer_offset;
  rect.topLeft.y = 159 - FntCharHeight();
  rect.extent.x  = FntCharsWidth( buffer, StrLen( buffer ) );
  rect.extent.y  = FntCharHeight();

  old_window = WinGetDrawWindow();
  WinSetDrawWindow( g_display_buffer );

  WinEraseRectangle( &rect, 0 );
  if( g_prefs.show_timer ) {
    WinDrawChars( buffer, StrLen( buffer ), rect.topLeft.x, rect.topLeft.y );
  }

  WinSetDrawWindow( old_window );

  if( redraw ) {
    updateScreen( frm, &rect, false );
  }
}
/* }}} */

static void static_count_invisible_clues( void ) /* {{{ */
{
  UInt8 i;
  t_hercule_clue* clues;
  UInt8 clue_count;

  clue_count = hercule_puzzle_get_clue_count( g_puzzle );
  g_invisible_clues = 0;
  clues = hercule_puzzle_open_clues( g_puzzle );

  for( i = 0; i < clue_count; i++ ) {
    if( hercule_clue_get_clue_type( clues[i] ) == HERCULE_CLUE_GIVEN ) {
      g_invisible_clues++;
    }
  }

  hercule_puzzle_close_clues( g_puzzle );
}
/* }}} */

static void static_add_new_win_to_average( void ) /*{{{*/
{
  g_prefs.games_played++;
  g_prefs.total_seconds += g_prefs.seconds;
}
/*}}}*/

static void doClueSwap( FormPtr frm ) /*{{{*/
{
  UInt8 clue_count;
  UInt8 i;
  t_hercule_clue* clues;

  clue_count = hercule_puzzle_get_clue_count( g_puzzle );
  clues = hercule_puzzle_open_clues( g_puzzle );

  for( i = 0; i < clue_count; i++ )
  {
    if( hercule_clue_get_data( clues[ i ] ) == 1 )
    {
      hercule_clue_set_data( clues[ i ], 0 );
    }
    else
    {
      hercule_clue_set_data( clues[ i ], 1 );
    }
  }

  hercule_puzzle_close_clues( g_puzzle );

  updateScreen( frm, NULL, true );
}
/*}}}*/


static void showTutorialPageObject( FormPtr frm, UInt16 id, UInt8 bit, Boolean show ) /* {{{ */
{
  if( show ) {
    if( ( g_visible_components & bit ) == 0 ) {
      FrmShowObject( frm, FrmGetObjectIndex( frm, id ) );
      g_visible_components |= bit;
    }
  } else {
    if( ( g_visible_components & bit ) != 0 ) {
      FrmHideObject( frm, FrmGetObjectIndex( frm, id ) );
      g_visible_components &= ~bit;
    }
  }
}
/* }}} */

static void drawTutorialPage( FormPtr frm ) /* {{{ */
{
  RectangleType rect;

  WinScreenLock( winLockCopy );

  rect.topLeft.x = 0;
  rect.topLeft.y = 16;
  rect.extent.x = 160;
  rect.extent.y = 127;

  WinEraseRectangle( &rect, 0 );

  switch( g_tutorial_page ) {
    case 0:
      drawTutorialPage_00( frm );
      break;

    case 1:
      drawTutorialPage_01( frm );
      break;

    case 2:
      drawTutorialPage_02( frm );
      break;

    case 3:
      drawTutorialPage_03( frm );
      break;

    case 4:
      drawTutorialPage_04( frm );
      break;

    case 5:
      drawTutorialPage_05( frm );
      break;

    case 6:
      drawTutorialPage_06( frm );
      break;

    case 7:
      drawTutorialPage_07( frm );
      break;
  }

  showTutorialPageObject( frm, Btn_Previous, PREV_BUTTON, ( g_tutorial_page > 0 ) );
  showTutorialPageObject( frm, Btn_Next, NEXT_BUTTON, ( g_tutorial_page < MAX_TUTORIAL_PAGE-1 ) );
  showTutorialPageObject( frm, Btn_Up, UP_BUTTON, ( g_first_line > 0 ) );
  showTutorialPageObject( frm, Btn_Down, DOWN_BUTTON, g_more_lines );

  WinScreenUnlock();
}
/* }}} */

static void drawTutorialPage_00( FormPtr frm ) /* {{{ */
{
  UInt16 end_line;
  UInt16 end_y;
  UInt8  i;
  UInt8  j;
  UInt16 xofs;
  UInt16 yofs;
  RectangleType rect;

  g_more_lines = !wordWrap( frm, Hercule_Tut01_Para01, stdFont, 3, 18, 150, 120, 
                            g_first_line, &end_line, &end_y );

  xofs = ( 160 - PUZZLE_COLS*13 ) / 2;
  yofs = end_y + 5;

  rect.topLeft.x = xofs;
  rect.topLeft.y = yofs;
  rect.extent.x = PUZZLE_COLS*13 + 1;
  rect.extent.y = PUZZLE_ROWS*11 + 1;

  WinDrawRectangleFrame( simpleFrame, &rect );

  for( i = 0; i < PUZZLE_ROWS; i++ ) {
    for( j = 0; j < PUZZLE_COLS; j++ ) {
      drawBitmapHandle( g_small_tiles[ i ][ j ], xofs + 2 + j*13, yofs + 1 + i*11 );
    }
  }
}
/* }}} */

static void drawThisClue( Int16 x, Int16 y, /* {{{ */
                          MemHandle item1,
                          MemHandle item2,
                          MemHandle item3 )
{
  RectangleType rect;

  rect.topLeft.x = x;
  rect.topLeft.y = y;
  rect.extent.x = 30;
  rect.extent.y = 10;

  WinDrawRectangleFrame( simpleFrame, &rect );

  drawBitmapHandle( item1, x, y );
  drawBitmapHandle( item2, x+10, y );
  drawBitmapHandle( item3, x+20, y );
}
/* }}} */

static void drawTutorialPage_01( FormPtr frm ) /* {{{ */
{
  UInt16 end_line;
  UInt16 end_y;
  UInt16 xofs;
  UInt16 yofs;

  g_more_lines = !wordWrap( frm, Hercule_Tut02_Para01, stdFont, 3, 18, 150, 120,
                            g_first_line, &end_line, &end_y );

  xofs = 90;
  yofs = end_y+5;
  WinDrawChars( "Left-Of:", 8, 20, yofs );
  drawThisClue( xofs, yofs, g_small_tiles[ 1 ][ 2 ], g_left_of_tile, g_small_tiles[ 3 ][ 0 ] );
  yofs += 18;
  WinDrawChars( "Between:", 8, 20, yofs );
  drawThisClue( xofs, yofs, g_small_tiles[ 0 ][ 2 ], g_small_tiles[ 0 ][ 4 ], g_small_tiles[ 3 ][ 1 ] );
  yofs += 18;
  WinDrawChars( "Next-To:", 8, 20, yofs );
  drawThisClue( xofs, yofs, g_small_tiles[ 2 ][ 2 ], g_small_tiles[ 1 ][ 1 ], g_small_tiles[ 2 ][ 2 ] );
  yofs += 18;
  WinDrawChars( "Same-Column:", 12, 20, yofs );
  drawThisClue( xofs, yofs, g_small_tiles[ 0 ][ 4 ], g_same_column_tile, g_small_tiles[ 3 ][ 4 ] );
}
/* }}} */

static void drawTutorialPage_02( FormPtr frm ) /* {{{ */
{
  UInt16 end_line;
  UInt16 end_y;

  WinDrawChars( "Left-Of:", 8, 20, 17 );
  drawThisClue( 90, 17, g_small_tiles[ 1 ][ 2 ], g_left_of_tile, g_small_tiles[ 3 ][ 0 ] );
  WinDrawLine( 30, 31, 100, 31 );

  g_more_lines = !wordWrap( frm, Hercule_Tut03_Para01, stdFont, 3, 33, 150, 120, g_first_line, &end_line, &end_y );
}
/* }}} */

static void drawTutorialPage_03( FormPtr frm ) /* {{{ */
{
  UInt16 end_line;
  UInt16 end_y;

  WinDrawChars( "Between:", 8, 20, 17 );
  drawThisClue( 90, 17, g_small_tiles[ 0 ][ 2 ], g_small_tiles[ 0 ][ 4 ], g_small_tiles[ 3 ][ 1 ] );
  WinDrawLine( 30, 31, 100, 31 );

  g_more_lines = !wordWrap( frm, Hercule_Tut04_Para01, stdFont, 3, 33, 150, 120, g_first_line, &end_line, &end_y );
}
/* }}} */

static void drawTutorialPage_04( FormPtr frm ) /* {{{ */
{
  UInt16 end_line;
  UInt16 end_y;

  WinDrawChars( "Next-To:", 8, 20, 17 );
  drawThisClue( 90, 17, g_small_tiles[ 2 ][ 2 ], g_small_tiles[ 1 ][ 1 ], g_small_tiles[ 2 ][ 2 ] );
  WinDrawLine( 30, 31, 100, 31 );

  g_more_lines = !wordWrap( frm, Hercule_Tut05_Para01, stdFont, 3, 33, 150, 120, g_first_line, &end_line, &end_y );
}
/* }}} */

static void drawTutorialPage_05( FormPtr frm ) /* {{{ */
{
  UInt16 end_line;
  UInt16 end_y;

  WinDrawChars( "Same-Column:", 12, 20, 17 );
  drawThisClue( 90, 17, g_small_tiles[ 0 ][ 4 ], g_same_column_tile, g_small_tiles[ 3 ][ 4 ] );
  WinDrawLine( 30, 31, 100, 31 );

  g_more_lines = !wordWrap( frm, Hercule_Tut06_Para01, stdFont, 3, 33, 150, 120, g_first_line, &end_line, &end_y );
}
/* }}} */

static void drawTutorialPage_06( FormPtr frm ) /* {{{ */
{
  UInt16 end_line;
  UInt16 end_y;

  FntSetFont( boldFont );
  WinDrawChars( "Clues", 5, 3, 17 );
  WinDrawLine( 3, 29, 50, 29 );

  g_more_lines = !wordWrap( frm, Hercule_Tut07_Para01, stdFont, 3, 33, 150, 120, g_first_line, &end_line, &end_y );
}
/* }}} */

static void drawTutorialPage_07( FormPtr frm ) /* {{{ */
{
  UInt16 end_line;
  UInt16 end_y;

  FntSetFont( boldFont );
  WinDrawChars( "Tips for Beginning Sleuths", 26, 3, 17 );
  WinDrawLine( 3, 29, 155, 29 );

  g_more_lines = !wordWrap( frm, Hercule_Tut08_Para01, stdFont, 3, 33, 150, 120, g_first_line, &end_line, &end_y );
}
/* }}} */

static Boolean wordWrap( FormPtr frm, /* {{{ */
                         UInt16 string_id,
                         UInt16 font_id,
                         UInt16 x, UInt16 y,
                         UInt16 width, UInt16 height,
                         UInt16 first_line,
                         UInt16* end_line,
                         UInt16* end_y )
{
  MemHandle string_handle;
  Char*     string_ptr;
  Int16     str_width;
  Int16     str_length;
  Int16     initial_length;
  Boolean   fit_in_width;
  UInt16    line;
  Int16     line_height;
  UInt16    parsed_length;
  Char      c;

  WinScreenLock( winLockCopy );

  string_handle = DmGetResource( 'tSTR', string_id );
  string_ptr = (Char*)MemHandleLock( string_handle );

  FntSetFont( font_id );

  line_height = FntLineHeight();
  line = 0;
  parsed_length = 0;
  initial_length = StrLen( string_ptr );

  do {
    str_width = width;
    str_length = initial_length - parsed_length;
    FntCharsInWidth( string_ptr+parsed_length, &str_width, &str_length, &fit_in_width );

    if( !fit_in_width ) {
      c = *( string_ptr + parsed_length + str_length - 1 );
      while( ( parsed_length + str_length + 1 < initial_length ) && ( c != ' ' ) && ( c != '-' ) )
      {
        str_length--;
        c = *( string_ptr + parsed_length + str_length - 1 );
      }
    }

    if( line >= first_line ) {
      WinDrawChars( string_ptr+parsed_length, str_length, x, y + ( line - first_line ) * line_height );
    }

    parsed_length += str_length;
    line++;
  } while( ( parsed_length < initial_length ) && ( ( line < first_line ) || ( (line-first_line+1)*line_height <= height ) ) );

  fit_in_width = ( parsed_length >= initial_length );

  MemHandleUnlock( string_handle );
  DmReleaseResource( string_handle );

  WinScreenUnlock();

  *end_line = line-1;
  *end_y = y + ( line - first_line ) * line_height;

  return fit_in_width;
}
/* }}} */


static void updateScreen( FormPtr frm, /* {{{ */
                          RectangleType* rect,
                          Boolean redraw )
{
  RectangleType r;

  if( redraw || g_display_buffer == NULL ) {
    drawPuzzle( frm );
  }

  WinSetDrawWindow( g_display_buffer );

  if( g_puzzle != NULL ) {
    /* draw the buttons */
    FrmShowObject( frm, FrmGetObjectIndex( frm, Eliminate_CmdBar_Icon ) );
    FrmShowObject( frm, FrmGetObjectIndex( frm, Assert_CmdBar_Icon ) );
    FrmShowObject( frm, FrmGetObjectIndex( frm, Hercule_Cmd_Undo ) );
    FrmShowObject( frm, FrmGetObjectIndex( frm, Hercule_Cmd_Reset ) );
    FrmShowObject( frm, FrmGetObjectIndex( frm, Hercule_Cmd_Hint ) );
    FrmShowObject( frm, FrmGetObjectIndex( frm, Hercule_Cmd_Swap ) );

  } else {
    MemHandle bmp;
    bmp = DmGetResource( bitmapRsc, Hercule_Logo_Bmp );
    drawBitmapHandle( bmp, 0, 0 ); 
    DmReleaseResource( bmp );
  }

  WinSetDrawWindow( FrmGetWindowHandle( frm ) );

  if( rect == NULL ) {
    BUILD_RECT( &r, 0, 0, 160, 160 );
    rect = &r;
  }
  WinCopyRectangle( g_display_buffer,
                    FrmGetWindowHandle( frm ),
                    rect,
                    rect->topLeft.x,
                    rect->topLeft.y,
                    winPaint );
}
/* }}} */

static void drawClueTilesAt( FormPtr frm, /* {{{ */
                             Int16 x, Int16 y,
                             Boolean draw_frame,
                             Boolean visible,
                             MemHandle tile1,
                             MemHandle tile2,
                             MemHandle tile3 )
{
  RectangleType rect;

  BUILD_RECT( &rect, x, y, CLUE_TILE_WIDTH*3, CLUE_TILE_HEIGHT );

  if( draw_frame ) {
    WinDrawRectangleFrame( simpleFrame, &rect );
  }

  if( visible ) {
    drawBitmapHandle( tile1, x, y );
    drawBitmapHandle( tile2, x+10, y );
    drawBitmapHandle( tile3, x+20, y );
  } else {
    WinEraseRectangle( &rect, 0 );
  }
}
/* }}} */

static void drawClueAt( FormPtr frm, /* {{{ */
                        t_hercule_clue clue,
                        Int16 x, Int16 y,
                        Boolean draw_frame )
{
  t_uint8 type;
  t_hercule_coord item1;
  t_hercule_coord item2;
  t_hercule_coord item3;
  Boolean visible;

  type = hercule_clue_get_clue_type( clue );
  hercule_clue_get_foundation( clue, &item1 );
  hercule_clue_get_op1( clue, &item2 );

  visible = ( hercule_clue_get_data( clue ) != 1 );

  switch( type ) {
    case HERCULE_CLUE_LEFT_OF:
      drawClueTilesAt( frm, x, y, draw_frame, visible,
                       getItemBitmap( &item1 ), g_left_of_tile, getItemBitmap( &item2 ) );
      break;

    case HERCULE_CLUE_SAME_COLUMN:
      drawClueTilesAt( frm, x, y, draw_frame, visible,
                       getItemBitmap( &item1 ), g_same_column_tile, getItemBitmap( &item2 ) );
      break;

    case HERCULE_CLUE_NEXT_TO:
      drawClueTilesAt( frm, x, y, draw_frame, visible,
                       getItemBitmap( &item2 ), getItemBitmap( &item1 ), getItemBitmap( &item2 ) );
      break;

    case HERCULE_CLUE_BETWEEN:
      hercule_clue_get_op2( clue, &item3 );
      drawClueTilesAt( frm, x, y, draw_frame, visible,
                       getItemBitmap( &item2 ), getItemBitmap( &item1 ), getItemBitmap( &item3 ) );
      break;
  }
}
/* }}} */

static void drawClue( FormPtr frm, /* {{{ */
                      t_hercule_clue clue,
                      Int16 which_clue,
                      Boolean draw_frame )
{
  Int8 row;
  Int8 col;
  Int16 x;
  Int16 y;

  if( g_prefs.clue_read_order == CLUE_READ_LEFT_TO_RIGHT ) {
    row = which_clue / 4;
    col = which_clue % 4;
  } else {
    UInt8 clue_count;
    UInt8 row_count;

    clue_count = hercule_puzzle_get_clue_count( g_puzzle );
    row_count = ((clue_count-g_invisible_clues-1)/4)+1;
    row = which_clue % row_count;
    col = which_clue / row_count;
  }

  x = 4 + col*40 + 1;
  y = 90;

  WinSetDrawWindow( g_display_buffer );
  drawClueAt( frm, clue, x, y + 11 * row, draw_frame );
  WinSetDrawWindow( FrmGetWindowHandle( frm ) );
}
/* }}} */

static void drawPuzzle( FormPtr frm ) /* {{{ */
{
  Int16 error;
  Int16 x;
  Int16 y;
  Int16 xofs;
  Int16 yofs;
  t_hercule_clue* clues;
  t_uint8 clue_count;
  t_uint8 i;
  t_uint8 which_clue;
  Char puzzle_no[ 17 ];

  if( g_display_buffer == NULL ) {
    g_display_buffer = WinCreateOffscreenWindow( 160, 160, screenFormat, &error );
  }
  WinSetDrawWindow( g_display_buffer );
  WinEraseWindow();

  xofs = GETXOFS();
  yofs = GETYOFS();

  /* draw the (blank) puzzle board */
  drawPuzzleBoardAt( frm, xofs, yofs );

  if( g_puzzle != NULL ) {
    /* draw the tiles */
    for( x = 0; x < PUZZLE_COLS; x++ ) {
      for( y = 0; y < PUZZLE_ROWS; y++ ) {
        drawTilesAt( frm, xofs + x * ( PUZZLE_COL_WIDTH + 1 ) + 1,
                     yofs + y * ( PUZZLE_COL_HEIGHT + 1 ) + 1,
                     y, x );
      }
    }

    /* draw the clues */
    clue_count = hercule_puzzle_get_clue_count( g_puzzle );
    clues = hercule_puzzle_open_clues( g_puzzle );

    which_clue = 0;
    for( i = 0; i < clue_count; i++ ) {
      if( hercule_clue_get_clue_type( clues[i] ) != HERCULE_CLUE_GIVEN ) {
        drawClue( frm, clues[i], which_clue, true );
        which_clue++;
      }
    }

    hercule_puzzle_close_clues( g_puzzle );
  }

  WinSetDrawWindow( g_display_buffer );

  if( g_prefs.seed >= 1000 ) {
    StrPrintF( puzzle_no, "Puzzle: %ld,%03ld", g_prefs.seed / 1000, g_prefs.seed % 1000 );
  } else {
    StrPrintF( puzzle_no, "Puzzle: %ld", g_prefs.seed );
  }
  WinDrawChars( puzzle_no, StrLen( puzzle_no ), 3, 159-FntCharHeight() );
  g_timer_offset = 3 + FntCharsWidth( puzzle_no, StrLen( puzzle_no ) ) + 5;

  static_update_timer( frm, false );

  /* set the draw window back to the form itself, so controls will be updated correctly */
  WinSetDrawWindow( FrmGetWindowHandle( frm ) );
}
/* }}} */

static void drawPuzzleBoardAt( FormPtr frm, Int16 x, Int16 y ) /* {{{ */
{
  RectangleType rect;
  t_uint8 i;

  WinSetDrawWindow( g_display_buffer );

  /* the frame will include the specified rectangle */
  rect.topLeft.x = x+1;
  rect.topLeft.y = y+1;
  rect.extent.x = ( PUZZLE_COL_WIDTH + 1 ) * PUZZLE_COLS-1;
  rect.extent.y = ( PUZZLE_COL_HEIGHT + 1 ) * PUZZLE_ROWS-1;

  WinDrawRectangleFrame( simpleFrame, &rect );

  /* draw the vertical lines within the frame */
  for( i = 1; i < PUZZLE_COLS; i++ ) {
    WinDrawLine( x + i * ( PUZZLE_COL_WIDTH+1 ), rect.topLeft.y,
                 x + i * ( PUZZLE_COL_WIDTH+1 ), rect.topLeft.y + rect.extent.y );
  }

  /* draw the horizontal lines within the frame */
  for( i = 1; i < PUZZLE_ROWS; i++ ) {
    WinDrawLine( rect.topLeft.x, y + i * ( PUZZLE_COL_HEIGHT+1 ),
                 rect.topLeft.x + rect.extent.x, y + i * ( PUZZLE_COL_HEIGHT+1 ) );
  }

  WinSetDrawWindow( FrmGetWindowHandle( frm ) );
}
/* }}} */

static void drawTilesAt( FormPtr frm, /* {{{ */
                         Int16 x, Int16 y,
                         Int16 row, Int16 col )
{
  UInt8 byte;

  WinSetDrawWindow( g_display_buffer );

  drawBitmapHandle( g_background_tile, x, y );

  byte = hercule_solution_space_at( g_solution, row, col );

  if( hercule_solution_space_solved_at( g_solution, row, col ) ) {
    byte = ( byte == 4 ? 3 : ( byte == 8 ? 4 : ( byte == 16 ? 5 : byte ) ) ) - 1;
    drawBitmapHandle( g_large_tiles[ row ][ byte ], x+5, y );
  } else {
    if( byte & 0x01 ) drawTileAt( frm, x,    y,    row, 0 );
    if( byte & 0x02 ) drawTileAt( frm, x+10, y,    row, 1 );
    if( byte & 0x04 ) drawTileAt( frm, x+20, y,    row, 2 );
    if( byte & 0x08 ) drawTileAt( frm, x+5,  y+10, row, 3 );
    if( byte & 0x10 ) drawTileAt( frm, x+15, y+10, row, 4 );
  }

  WinSetDrawWindow( FrmGetWindowHandle( frm ) );
}
/* }}} */

static void drawTileAt( FormPtr frm, Int16 x, Int16 y, Int16 row, Int16 tile ) /* {{{ */
{
  drawBitmapHandle( g_small_tiles[ row ][ tile ], x, y );
}
/* }}} */

#ifdef COLOR
static Int16 getIndexOfLeastSignificantBit( UInt32 word ) /* {{{ */
{
  Int16 idx = 0;

  while( idx < 32 ) {
    if( ( ( word >> idx ) & 0x01 ) != 0 ) {
      return idx;
    }
    idx++;
  }

  return -1;
}
/* }}} */
#endif


UInt32 PilotMain( UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags ) /* {{{ */
{
  UInt16 error;
  SysNotifyParamType* parm;

  error = RomVersionCompatible( MINVERSION, launchFlags );
  if (error) return (error);

  if( cmd == sysAppLaunchCmdNormalLaunch )
  {
    error = StartApplication();
    if( error ) return error;

    FrmGotoForm( HerculeMain );

    HerculeEventLoop();
    StopApplication();
  }
  else if( cmd == sysAppLaunchCmdNotify )
  {
    parm = (SysNotifyParamType*)cmdPBP;
    if( parm->notifyType == sysNotifySleepNotifyEvent ) {
      g_prefs.seconds += ( TimGetTicks() - g_prefs.started_at ) / SysTicksPerSecond();
    } else if( parm->notifyType == sysNotifyLateWakeupEvent ) {
      g_prefs.started_at = TimGetTicks();
    }
  }

  return (0);
}
/* }}} */
