#ifndef __HERCULE_TILESETS_H__
#define __HERCULE_TILESETS_H__

#include "hercule.h"

#ifdef COLOR
#define TILE_SETS ( 10 )
#else
#define TILE_SETS (  8 )
#endif

extern UInt8   g_active_tiles[ PUZZLE_ROWS ];
extern Boolean g_random_row_tiles;

extern MemHandle g_large_tiles[ PUZZLE_ROWS ][ PUZZLE_COLS ];
extern MemHandle g_small_tiles[ PUZZLE_ROWS ][ PUZZLE_COLS ];


void HerculeRegisterTileSetsDlg( void );
void HerculeOpenTilesets( void );
void HerculeCloseTilesets( void );
void HerculeJumbleTilesets( void );

UInt8* HerculeTilesetsPackage( UInt16* size );
void   HerculeTilesetsUnpackage( UInt8** buffer );

#endif /* __HERCULE_TILESETS_H__ */
