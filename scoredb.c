#include <PalmOS.h>

#include "hercule.h"
#include "scoredb.h"

#define  SCOREDB "herculeScoreDB"


static Int16 ScoreCompare( SCORE_RECORD* rec1, SCORE_RECORD* rec2 );



DmOpenRef OpenScoreDB( void )
{
  LocalID dbId;

  dbId = DmFindDatabase( 0, SCOREDB );
  if( dbId == 0 ) {
    if( DmCreateDatabase( 0, SCOREDB, DBID, DBTYPE, false ) != errNone ) {
      return NULL;
    }
    dbId = DmFindDatabase( 0, SCOREDB );
  }

  return DmOpenDatabase( 0, dbId, dmModeReadWrite );
}


void CloseScoreDB( DmOpenRef scoreDB )
{
  DmCloseDatabase( scoreDB );
}


Boolean ScoreInTop( DmOpenRef scoreDB, SCORE_RECORD* score, Int16 topScoreCount )
{
  MemHandle     record;
  UInt16        idx = 0;
  UInt16        count;
  SCORE_RECORD* recordP;
  Boolean       found_in_top = false;

  count = DmNumRecords( scoreDB );
  if( count < topScoreCount ) return true;

  /* find where to add the record */
  while( idx < count ) {
    record = DmQueryRecord( scoreDB, idx );
    recordP = (SCORE_RECORD*)MemHandleLock( record );
    found_in_top = ( ScoreCompare( score, recordP ) < 0 );
    MemHandleUnlock( record );
    DmReleaseRecord( scoreDB, idx, false );
    if( found_in_top ) break;
    idx++;
  }

  return found_in_top;
}


Int16 AddScore( DmOpenRef scoreDB, SCORE_RECORD* score, Int16 topScoreCount, UInt16* rank )
{
  MemHandle     record;
  UInt16        idx = 0;
  UInt16        count;
  SCORE_RECORD* recordP;
  Boolean       found_insert = false;

  count = DmNumRecords( scoreDB );

  /* find where to add the record */
  while( idx < count ) {
    record = DmQueryRecord( scoreDB, idx );
    recordP = (SCORE_RECORD*)MemHandleLock( record );
    found_insert = ( ScoreCompare( score, recordP ) < 0 );
    MemHandleUnlock( record );
    DmReleaseRecord( scoreDB, idx, false );
    if( found_insert ) break;
    idx++;
  }

  idx = ( found_insert ? idx : dmMaxRecordIndex );

  /* add the record */
  record = DmNewRecord( scoreDB, &idx, sizeof( SCORE_RECORD ) );
  recordP = (SCORE_RECORD*)MemHandleLock( record );
  DmWrite( recordP, 0, score, sizeof( SCORE_RECORD ) );
  MemHandleUnlock( record );
  DmReleaseRecord( scoreDB, idx, true );

  if( rank != NULL ) {
    *rank = idx+1;
  }

  /* delete trailing scores beyond the topScoreCount */
  while( ( idx = DmNumRecords( scoreDB ) ) > topScoreCount ) {
    DmRemoveRecord( scoreDB, idx-1 );
  }

  return 0;
}


void ClearScores( DmOpenRef scoreDB )
{
  while( DmNumRecords( scoreDB ) > 0 ) {
    DmRemoveRecord( scoreDB, 0 );
  }
}


#define SCORE(x)   ((SCORE_RECORD*)x)

#define DATE(x)       SCORE(x)->date
#define SEED(x)       SCORE(x)->seed
#define SECONDS(x)    SCORE(x)->seconds
#define HANDICAP(x)   SCORE(x)->handicap
#define AUTODEDUCE(x) SCORE(x)->auto_deduce
#define WARNONBAD(x)  SCORE(x)->warn_on_bad
#define WARNINGS(x)   SCORE(x)->warnings
#define UNDOS(x)      SCORE(x)->undos
#define HINTS(x)      SCORE(x)->hints

#define EVALUATE(sort_before,x,y,z) ( (x) < (y) ? (sort_before) : ( (x) > (y) ? -(sort_before) : (z) ) )

static Int16 ScoreCompare( SCORE_RECORD* rec1, SCORE_RECORD* rec2 )
{
  return EVALUATE( -1, HANDICAP(rec1), HANDICAP(rec2),
           EVALUATE( -1, HINTS(rec1), HINTS(rec2),
             EVALUATE( -1, WARNINGS(rec1), WARNINGS(rec2),
               EVALUATE( -1, SECONDS(rec1), SECONDS(rec2),
                 EVALUATE( -1, DATE(rec1), DATE(rec2),
                   EVALUATE( -1, SEED(rec1), SEED(rec2), 0 ) ) ) ) ) );
}

