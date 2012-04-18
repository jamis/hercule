#include "progress_dlg.h"

#define WIDTH ( 100 )
#define HEIGHT ( 60 )

#define PROGRESS_WIDTH  ( 90 )
#define PROGRESS_HEIGHT (  6 )

static UInt16 progress_x;
static UInt16 progress_y;

static CustomPatternType pattern = { 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55 };


FormPtr OpenProgressDlg( const Char* label )
{
  FormPtr progress;
  RectangleType rect;
  Coord x, y;
  Int16 width;

  WinGetDisplayExtent( &x, &y );
  x = ( x - WIDTH ) / 2;
  y = ( y - HEIGHT ) / 2;

  progress = FrmNewForm( 0, "Working...",
                         x, y, 
                         WIDTH, HEIGHT,
                         true, 0, 0, 0 );
  FrmDrawForm( progress );

  WinSetActiveWindow( FrmGetWindowHandle( progress ) );

  progress_x = ( WIDTH - PROGRESS_WIDTH ) / 2;
  progress_y = HEIGHT * 0.7;
  rect.topLeft.x = progress_x;
  rect.topLeft.y = progress_y;
  rect.extent.x = PROGRESS_WIDTH;
  rect.extent.y = PROGRESS_HEIGHT;

  WinDrawRectangle( &rect, 2 );

  rect.topLeft.x++;
  rect.topLeft.y++;
  rect.extent.x -= 2;
  rect.extent.y -= 2;

  WinInvertRectangle( &rect, 2 );

  FntSetFont( stdFont );
  width = FntCharsWidth( label, StrLen( label ) );
  x = ( WIDTH - width ) / 2;
  y = 20;
  WinDrawChars( label, StrLen( label ), x, y );

  return progress;
}


void CloseProgressDlg( FormPtr progress, UInt16 delay )
{
  if( delay > 0 ) {
    SysTaskDelay( delay );
  }
  FrmEraseForm( progress );
  FrmDeleteForm( progress );
}


void UpdateProgressDlg( FormPtr progress, UInt8 percent )
{
  WinHandle win;
  RectangleType rect;

  win = FrmGetWindowHandle( progress );
  WinSetActiveWindow( win );

  rect.topLeft.x = progress_x+1;
  rect.topLeft.y = progress_y+1;
  rect.extent.x = ( PROGRESS_WIDTH - 2 ) * percent / 100;
  rect.extent.y = PROGRESS_HEIGHT - 2;

  WinSetPattern( (const CustomPatternType*)&pattern );
  WinFillRectangle( &rect, 2 );
}

