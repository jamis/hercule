#include <PalmOS.h>
#include <stdarg.h>

#include "hercule.h"
#include "herculeRsc.h"
#include "hercule_engine_palm.h"
#include "progress_dlg.h"
#include "hercule_sortdlg.h"

/*#define DBG_MSG(x)           msg_box x*/
#define DBG_MSG(x)

#define UNLESS( x )          if( !(x) )

#define LOCK_GRID( sol )     (t_uint8*)MemHandleLock( sol->grid )
#define UNLOCK_GRID( sol )   MemHandleUnlock( sol->grid )

#define LOCK_SOLUTION( h )   (HERC_SOLUTION_SPACE*)MemHandleLock( h )
#define UNLOCK_SOLUTION( h )  MemHandleUnlock( h )

#define LOCK_PUZZLE( h )     (HERC_PUZZLE*)MemHandleLock( h )
#define UNLOCK_PUZZLE( h )   MemHandleUnlock( h )

#define LOCK_CLUE( h )       (HERC_CLUE*)MemHandleLock( h )
#define UNLOCK_CLUE( h )     MemHandleUnlock( h )

#define LOCK_STACK( h )      (HERC_SOLUTION_STATE_STACK*)MemHandleLock( h )
#define UNLOCK_STACK( h )    MemHandleUnlock( h )

#define LOCK_STACK_ITEM( h ) (HERC_SOLUTION_STATE_STACK_ITEM*)MemHandleLock( h )
#define UNLOCK_STACK_ITEM(h) MemHandleUnlock( h )

#define SET_HINT( h, a, r, c, v ) \
  h->assert = a; \
  h->position.row = r; \
  h->position.col = c; \
  h->value = v

#define PZL_CLUE_LIST_GROW_INC    (   4 )
#define ELIMINATE_SCORE           (   1 )
#define DISCOVER_SCORE            ( 100 )

#define COMPUTE_IDX(max_width,row,col)   ( row * max_width + col )
#define AT( sol, grid, row, col ) grid[ COMPUTE_IDX( sol->cols, row, col ) ]
#define IS_POSSIBLE( sol, grid, row, col, item ) ( ( AT( sol, grid, row, col ) & static_bit[ item ] ) != 0 )

#define PZL_DECL( var )         HERC_PUZZLE* var
#define CLUE_DECL( var )        HERC_CLUE* var
#define SOL_DECL( var )         HERC_SOLUTION_SPACE* var
#define STACK_DECL( var )       HERC_SOLUTION_STATE_STACK* var
#define STACK_ITEM_DECL( var )  HERC_SOLUTION_STATE_STACK_ITEM* var

#define GENERIC_GET_FN( rval, fn, objtype, interntype, field ) \
  rval fn( objtype parm ) \
  { \
    interntype* var = (interntype*)MemHandleLock( parm ); \
    rval f = var->field; \
    MemHandleUnlock( parm ); \
    return f; \
  }

#define PZL_GET_FN( rval, fn, field ) GENERIC_GET_FN( rval, fn, t_hercule_puzzle, HERC_PUZZLE, field )
#define CLUE_GET_FN( rval, fn, field ) GENERIC_GET_FN( rval, fn, t_hercule_clue, HERC_CLUE, field )


typedef struct {
  t_hercule_coord foundation;
  t_hercule_coord op1;
  t_hercule_coord op2;
  t_uint8 clue_type;
  t_uint8 data;
} HERC_CLUE;

typedef struct {
  t_uint8 rows : 4;
  t_uint8 cols : 4;
  t_uint32 seed;
  t_uint8 handicap;
  t_uint8 accuracy;
  MemHandle grid;
  MemHandle clues;
  t_uint8 clue_count;
  t_uint8 clue_capacity;
} HERC_PUZZLE;

typedef struct {
  MemHandle grid;
  MemHandle next;
  MemHandle previous;
} HERC_SOLUTION_STATE_STACK_ITEM;

typedef struct {
  MemHandle head;
  MemHandle tail;
  t_uint8 length;
  t_uint8 max_length;
  t_uint8 overflow_behavior;
} HERC_SOLUTION_STATE_STACK;

typedef struct {
  t_hercule_puzzle puzzle;
  t_uint8 rows : 4;
  t_uint8 cols : 4;
  MemHandle grid;
} HERC_SOLUTION_SPACE;

typedef struct {
  t_uint32 score;
  t_hercule_clue clue;
} HERC_WEIGHTED_CLUE;

/* ---- Hercule Private Constants ---------------- */

static t_uint8 static_bits_set[] = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4 };
static t_uint8 static_all_bits[] = { 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF };
static t_uint8 static_bit[]      = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

static FormPtr static_progress_dlg = NULL;
static t_bool  static_this_counts = 0;

/* ---- Hercule Private Method Interface ---------------- */

static void static_swap_elements( t_uint8* grid, t_uint8 idx1, t_uint8 idx2 ) HERCULE_ENGINE;

static void static_generate_clues( t_hercule_puzzle puzzle ) HERCULE_ENGINE;

static t_uint32 static_clear_row_except_col( HERC_SOLUTION_SPACE* sol,
                                             t_uint8 row,
                                             t_uint8 col,
                                             t_uint8 value ) HERCULE_ENGINE;

static t_uint8 static_count_bits( t_uint8 byte ) HERCULE_ENGINE;

static t_uint8 static_least_sig_bit( t_uint8 byte ) HERCULE_ENGINE;

static t_uint8 static_get_bit( t_uint8 byte, t_uint8 which ) HERCULE_ENGINE;

static t_uint32 static_apply_left_of( HERC_SOLUTION_SPACE* sol,
                                      t_uint8* grid,
                                      t_hercule_coord* foundation,
                                      t_hercule_coord* op,
                                      t_hercule_hint* hint ) HERCULE_ENGINE;

static t_uint32 static_apply_next_to( HERC_SOLUTION_SPACE* sol,
                                      t_uint8* grid,
                                      t_hercule_coord* foundation,
                                      t_hercule_coord* op,
                                      t_hercule_hint* hint ) HERCULE_ENGINE;

static t_uint32 static_apply_same_column( HERC_SOLUTION_SPACE* sol,
                                          t_uint8* grid,
                                          t_hercule_coord* foundation,
                                          t_hercule_coord* op,
                                          t_hercule_hint* hint ) HERCULE_ENGINE;

static t_uint32 static_apply_between( HERC_SOLUTION_SPACE* sol,
                                      t_uint8* grid,
                                      t_hercule_coord* foundation,
                                      t_hercule_coord* op1,
                                      t_hercule_coord* op2,
                                      t_hercule_hint* hint ) HERCULE_ENGINE;

static t_uint32 static_assert( HERC_SOLUTION_SPACE* solution,
                               t_uint8* grid,
                               t_uint8 row,
                               t_uint8 col,
                               t_uint8 value,
                               t_bool exists,
                               t_bool auto_deduce,
                               t_hercule_hint* hint ) HERCULE_ENGINE;

static t_uint32 static_apply_between_a_and_b( HERC_SOLUTION_SPACE* sol,
                                              t_uint8* grid,
                                              t_hercule_coord* a,
                                              t_hercule_coord* b,
                                              t_uint8 aitem, 
                                              t_uint8 bitem,
                                              t_hercule_coord* foundation,
                                              t_uint8 fitem,
                                              t_uint8 col,
                                              t_bool* do_exit,
                                              t_hercule_hint* hint ) HERCULE_ENGINE;

static t_hercule_clue static_get_random_clue( t_hercule_solution_space solution ) HERCULE_ENGINE;

t_uint32 static_solution_space_apply( HERC_SOLUTION_SPACE* sol,
                                      t_hercule_clue clue,
                                      t_hercule_hint* hint ) HERCULE_ENGINE;

t_uint8 static_solution_percent_solved( HERC_SOLUTION_SPACE* sol ) HERCULE_ENGINE;

void static_destroy_stack_item( MemHandle item ) HERCULE_ENGINE;


/* ---- Hercule Serialization Interface Implementation ---------------- */

t_uint32 hercule_serialize_session( t_uint8* buffer,
                                    t_hercule_puzzle puzzle,
                                    t_hercule_solution_space solution,
                                    t_hercule_solution_space_state_stack undo_stack,
                                    t_bool dont_write )
{
  PZL_DECL( pzl );
  SOL_DECL( sol );
  STACK_DECL( stack );
  STACK_ITEM_DECL( item );
  CLUE_DECL( clue );
  t_uint8* grid;
  t_uint8  i;
  t_uint32 size;
  t_hercule_clue* clues;
  MemHandle itemH;
  MemHandle prevH;

  size = 0;

  /* serialize the puzzle */

  pzl = LOCK_PUZZLE( puzzle );
  UNLESS( dont_write ) MemMove( buffer+size, pzl, 7 ); /* write the first 5 fields */
  size += 7;
  UNLESS( dont_write ) MemMove( buffer+size, &pzl->clue_count, 1 );
  size += 1;

  grid = LOCK_GRID( pzl );
  UNLESS( dont_write ) MemMove( buffer+size, grid, pzl->rows*pzl->cols );
  size += pzl->rows * pzl->cols;
  UNLOCK_GRID( pzl );

  clues = (t_hercule_clue*)MemHandleLock( pzl->clues );
  for( i = 0; i < pzl->clue_count; i++ ) {
    clue = LOCK_CLUE( clues[i] );
    UNLESS( dont_write ) MemMove( buffer+size, clue, sizeof( *clue ) );
    size += sizeof( *clue );
    UNLOCK_CLUE( clues[i] );
  }
  MemHandleUnlock( pzl->clues );

  UNLOCK_PUZZLE( puzzle );

  /* serialize the solution */
  
  sol = LOCK_SOLUTION( solution );
  grid = LOCK_GRID( sol );
  UNLESS( dont_write ) MemMove( buffer+size, grid, sol->rows*sol->cols );
  size += sol->rows * sol->cols;
  UNLOCK_GRID( sol );

  /* serialize the undo stack */

  stack = LOCK_STACK( undo_stack );
  UNLESS( dont_write ) MemMove( buffer+size, &stack->max_length, sizeof( stack->max_length ) );
  size += sizeof( stack->max_length );
  UNLESS( dont_write ) MemMove( buffer+size, &stack->overflow_behavior, sizeof( stack->overflow_behavior ) );
  size += sizeof( stack->overflow_behavior );
  UNLESS( dont_write ) MemMove( buffer+size, &stack->length, sizeof( stack->length ) );
  size += sizeof( stack->length );
  itemH = stack->head;
  while( itemH != NULL ) {
    item = LOCK_STACK_ITEM( itemH );
    prevH = item->previous;
    UNLESS( dont_write ) {
      grid = LOCK_GRID( item );
      MemMove( buffer+size, grid, sol->rows*sol->cols );
      UNLOCK_GRID( item );
    }
    size += sol->rows * sol->cols;
    UNLOCK_STACK_ITEM( itemH );
    itemH = prevH;
  }
  UNLOCK_STACK( undo_stack );
  UNLOCK_SOLUTION( solution );

  return size;
}


t_uint8 hercule_unserialize_session( t_uint8* buffer,
                                     t_hercule_puzzle* puzzle,
                                     t_hercule_solution_space* solution,
                                     t_hercule_solution_space_state_stack* undo_stack )
{
  PZL_DECL( pzl );
  SOL_DECL( sol );
  STACK_DECL( stack );
  STACK_ITEM_DECL( item );
  CLUE_DECL( clue );
  t_uint8* grid;
  t_uint8  i;
  t_uint32 size;
  t_hercule_clue* clues;
  MemHandle itemH;

  size = 0;

  /* unserialize the puzzle */

  *puzzle = MemHandleNew( sizeof( HERC_PUZZLE ) );
  pzl = LOCK_PUZZLE( *puzzle );
  MemMove( pzl, buffer+size, 7 ); /* read the first 5 fields */
  size += 7;
  MemMove( &pzl->clue_count, buffer+size, 1 );
  size += 1;
  pzl->clue_capacity = pzl->clue_count;

  pzl->grid = MemHandleNew( pzl->rows * pzl->cols );
  grid = LOCK_GRID( pzl );
  MemMove( grid, buffer+size, pzl->rows * pzl->cols );
  size += pzl->rows * pzl->cols;
  UNLOCK_GRID( pzl );

  pzl->clues = MemHandleNew( pzl->clue_count * sizeof( t_hercule_clue* ) );
  clues = (t_hercule_clue*)MemHandleLock( pzl->clues );
  for( i = 0; i < pzl->clue_count; i++ ) {
    clues[ i ] = MemHandleNew( sizeof( HERC_CLUE ) );
    clue = LOCK_CLUE( clues[i] );
    MemMove( clue, buffer+size, sizeof( *clue ) );
    size += sizeof( *clue );
    UNLOCK_CLUE( clues[i] );
  }
  MemHandleUnlock( pzl->clues );

  /* unserialize the solution */

  *solution = MemHandleNew( sizeof( HERC_SOLUTION_SPACE ) );
  sol = LOCK_SOLUTION( *solution );

  sol->rows = pzl->rows;
  sol->cols = pzl->cols;
  sol->puzzle = *puzzle;
  sol->grid = MemHandleNew( sol->rows * sol->cols );
  grid = LOCK_GRID( sol );
  MemMove( grid, buffer+size, sol->rows * sol->cols );
  size += sol->rows * sol->cols;
  UNLOCK_GRID( sol );

  /* unserialize the undo stack */

  *undo_stack = MemHandleNew( sizeof( HERC_SOLUTION_STATE_STACK ) );
  stack = LOCK_STACK( *undo_stack );

  MemMove( &stack->max_length, buffer+size, sizeof( stack->max_length ) );
  size += sizeof( stack->max_length );
  MemMove( &stack->overflow_behavior, buffer+size, sizeof( stack->overflow_behavior ) );
  size += sizeof( stack->overflow_behavior );
  MemMove( &stack->length, buffer+size, sizeof( stack->length ) );
  size += sizeof( stack->length );
  stack->head = stack->tail = NULL;

  for( i = 0; i < stack->length; i++ ) {
    itemH = MemHandleNew( sizeof( HERC_SOLUTION_STATE_STACK_ITEM ) );
    item = LOCK_STACK_ITEM( itemH );
    item->next = stack->tail;
    item->previous = NULL;
    item->grid = MemHandleNew( sol->rows * sol->cols );
    grid = LOCK_GRID( item );
    MemMove( grid, buffer+size, sol->rows * sol->cols );
    UNLOCK_GRID( item );
    size += sol->rows * sol->cols;
    UNLOCK_STACK_ITEM( itemH );

    if( stack->head == NULL ) {
      stack->head = itemH;
    }

    if( stack->tail != NULL ) {
      item = LOCK_STACK_ITEM( stack->tail );
      item->previous = itemH;
      UNLOCK_STACK_ITEM( stack->tail );
    }

    stack->tail = itemH;
  }

  UNLOCK_STACK( *undo_stack );
  UNLOCK_SOLUTION( *solution );
  UNLOCK_PUZZLE( *puzzle );

  return 0;
}


/* ---- Hercule Puzzle Interface Implementation ---------------- */

t_hercule_puzzle hercule_puzzle_new( t_uint8 rows,
                                     t_uint8 cols,
                                     t_uint32 seed,
                                     t_uint8 handicap,
                                     t_uint8 accuracy )
{
  MemHandle pzl_handle;
  HERC_PUZZLE* pzl;
  t_uint8* grid;
  t_uint8 i;
  t_uint8 j;
  t_uint8 size;

  pzl_handle = MemHandleNew( sizeof( HERC_PUZZLE ) );
  pzl = (HERC_PUZZLE*)MemHandleLock( pzl_handle );
  pzl->rows = rows;
  pzl->cols = cols;
  pzl->seed = seed;
  pzl->handicap = handicap;
  pzl->accuracy = accuracy;
  pzl->clue_count = 0;
  pzl->clue_capacity = 1;

  pzl->clues = MemHandleNew( sizeof( MemHandle ) );

  size = pzl->rows * pzl->cols;
  pzl->grid = MemHandleNew( size );
  grid = (t_uint8*)MemHandleLock( pzl->grid );

  /* initialize the random seed to the given seed value */
  SysRandom( pzl->seed );

  /* each row of the puzzle is numbered 0 to the number of columns-1 */
  for( i = 0; i < size; i++ ) {
    grid[ i ] = i % pzl->cols;
  }

  /* now, we randomize each row to create the puzzle "goal" */
  for( i = 0; i < pzl->rows; i++ ) {
    for( j = 0; j < pzl->cols; j++ ) {
      static_swap_elements( grid,
                            SysRandom(0) % pzl->cols + i * pzl->cols,
                            SysRandom(0) % pzl->cols + i * pzl->cols );
    }
  }

  MemHandleUnlock( pzl->grid );
  MemHandleUnlock( pzl_handle );

  /* generate the clues that allow this puzzle to be solved */
  static_generate_clues( (t_hercule_puzzle)pzl_handle );

  return (t_hercule_puzzle)pzl_handle;
}


void hercule_puzzle_destroy( t_hercule_puzzle puzzle )
{
  PZL_DECL( pzl );
  t_hercule_clue* clues;
  int i;

  pzl = LOCK_PUZZLE( puzzle );
  MemHandleFree( pzl->grid );

  if( pzl->clue_capacity > 0 ) {
    clues = (t_hercule_clue*)MemHandleLock( pzl->clues );
    for( i = 0; i < pzl->clue_count; i++ ) {
      hercule_clue_destroy( clues[ i ] );
    }
    MemHandleUnlock( pzl->clues );
    MemHandleFree( pzl->clues );
  }

  UNLOCK_PUZZLE( puzzle );
  MemHandleFree( puzzle );
}


PZL_GET_FN( t_uint8,  hercule_puzzle_get_rows, rows );
PZL_GET_FN( t_uint8,  hercule_puzzle_get_cols, cols );
PZL_GET_FN( t_uint32, hercule_puzzle_get_seed, seed );
PZL_GET_FN( t_uint8,  hercule_puzzle_get_handicap, handicap );
PZL_GET_FN( t_uint8,  hercule_puzzle_get_clue_count, clue_count );


t_hercule_clue* hercule_puzzle_open_clues( t_hercule_puzzle puzzle )
{
  PZL_DECL( pzl );
  t_hercule_clue* clues;
  pzl = LOCK_PUZZLE( puzzle );
  clues = (t_hercule_clue*)MemHandleLock( pzl->clues );
  UNLOCK_PUZZLE( puzzle );
  return clues;
}


void hercule_puzzle_close_clues( t_hercule_puzzle puzzle )
{
  PZL_DECL( pzl );
  pzl = LOCK_PUZZLE( puzzle );
  MemHandleUnlock( pzl->clues );
  UNLOCK_PUZZLE( puzzle );
}


void hercule_puzzle_add_clue( t_hercule_puzzle puzzle,
                              t_hercule_clue clue )
{
  PZL_DECL( pzl );
  t_hercule_clue* clues;

  pzl = LOCK_PUZZLE( puzzle );
  if( pzl->clue_count == pzl->clue_capacity ) {
    /* resize the clue list */
    pzl->clue_capacity += PZL_CLUE_LIST_GROW_INC;
    MemHandleResize( pzl->clues, pzl->clue_capacity * sizeof( t_hercule_clue ) );
  }

  clues = (t_hercule_clue*)MemHandleLock( pzl->clues );
  clues[ pzl->clue_count++ ] = clue;
  MemHandleUnlock( pzl->clues );
  UNLOCK_PUZZLE( puzzle );
}


t_uint8 hercule_puzzle_at( t_hercule_puzzle puzzle,
                           t_uint8 row,
                           t_uint8 col )
{
  PZL_DECL( pzl );
  t_uint8 at;
  t_uint8* grid;
  pzl = LOCK_PUZZLE( puzzle );
  grid = LOCK_GRID( pzl );
  at = grid[ COMPUTE_IDX( pzl->cols, row, col ) ];
  UNLOCK_GRID( pzl );
  UNLOCK_PUZZLE( puzzle );
  return at;
}


/* ---- Hercule Clue Interface Implementation ---------------- */

t_hercule_clue hercule_clue_new( t_hercule_coord* foundation,
                                 t_uint8          clue_type,
                                 t_hercule_coord* op1,
                                 t_hercule_coord* op2 )
{
  t_hercule_clue clue_handle;
  HERC_CLUE* clue;

  clue_handle = (t_hercule_clue)MemHandleNew( sizeof( HERC_CLUE ) );
  clue = (HERC_CLUE*)MemHandleLock( clue_handle );
  clue->clue_type = clue_type;
  clue->data = 0;

  MemMove( &clue->foundation, foundation, sizeof( clue->foundation ) );
  
  if( op1 != NULL ) {
    MemMove( &clue->op1, op1, sizeof( clue->op1 ) );
  }

  if( op2 != NULL ) {
    MemMove( &clue->op2, op2, sizeof( clue->op2 ) );
  }

  MemHandleUnlock( clue_handle );
  return clue_handle;
}


void hercule_clue_destroy( t_hercule_clue clue )
{
  MemHandleFree( clue );
}


void hercule_clue_get_foundation( t_hercule_clue clue, t_hercule_coord* coord )
{
  CLUE_DECL( c );
  c = LOCK_CLUE( clue );
  MemMove( coord, &c->foundation, sizeof( c->foundation ) );
  UNLOCK_CLUE( clue );
}


void hercule_clue_get_op1( t_hercule_clue clue, t_hercule_coord* coord )
{
  CLUE_DECL( c );
  c = LOCK_CLUE( clue );
  MemMove( coord, &c->op1, sizeof( c->op1 ) );
  UNLOCK_CLUE( clue );
}


void hercule_clue_get_op2( t_hercule_clue clue, t_hercule_coord* coord )
{
  CLUE_DECL( c );
  c = LOCK_CLUE( clue );
  MemMove( coord, &c->op2, sizeof( c->op2 ) );
  UNLOCK_CLUE( clue );
}


CLUE_GET_FN( t_uint8, hercule_clue_get_clue_type, clue_type );


void hercule_clue_set_data( t_hercule_clue clue, t_uint8 data )
{
  CLUE_DECL( c );
  c = LOCK_CLUE( clue );
  c->data = data;
  UNLOCK_CLUE( clue );
}


CLUE_GET_FN( t_uint8, hercule_clue_get_data, data );


void hercule_clue_sort( t_hercule_puzzle puzzle, HerculeClueSortInfo* sortInfo )
{
  PZL_DECL( pzl );
  t_hercule_clue* clues;
  t_hercule_clue  temp;
  UInt8 type1;
  UInt8 type2;
  UInt8 order1;
  UInt8 order2;
  UInt8 i;
  UInt8 j;

  pzl = LOCK_PUZZLE( puzzle );
  clues = hercule_puzzle_open_clues( puzzle );

  for( i = 0; i < pzl->clue_count-1; i++ ) {
    for( j = i+1; j < pzl->clue_count; j++ ) {
      type1 = hercule_clue_get_clue_type( clues[i] );
      type2 = hercule_clue_get_clue_type( clues[j] );

      if( type1 == sortInfo->first ) {
        order1 = 1;
      } else if( type1 == sortInfo->second ) {
        order1 = 2;
      } else if( type1 == sortInfo->third ) {
        order1 = 3;
      } else if( type1 == sortInfo->fourth ) {
        order1 = 4;
      } else {
        order1 = 5;
      }

      if( type2 == sortInfo->first ) {
        order2 = 1;
      } else if( type2 == sortInfo->second ) {
        order2 = 2;
      } else if( type2 == sortInfo->third ) {
        order2 = 3;
      } else if( type2 == sortInfo->fourth ) {
        order2 = 4;
      } else {
        order2 = 5;
      }

      /* if j sould sort before i, then swap them */
      if( order2 < order1 ) {
        temp = clues[i];
        clues[i] = clues[j];
        clues[j] = temp;
      }
    }
  }

  hercule_puzzle_close_clues( puzzle );
  UNLOCK_PUZZLE( puzzle );
}

/* ---- Hercule Solution State Stack Interface Implementation ---------------- */

t_hercule_solution_space_state_stack hercule_solution_space_state_stack_new( t_uint8 max_length,
                                                                             t_uint8 on_overflow )
{
  t_hercule_solution_space_state_stack stack_handle;
  HERC_SOLUTION_STATE_STACK* stack;

  stack_handle = (t_hercule_solution_space_state_stack)MemHandleNew( sizeof( HERC_SOLUTION_STATE_STACK ) );
  stack = LOCK_STACK( stack_handle );

  stack->head = stack->tail = NULL;
  stack->length = 0;

  stack->max_length = max_length;
  stack->overflow_behavior = on_overflow;

  UNLOCK_STACK( stack_handle );
  return stack_handle;
}


void hercule_solution_space_state_stack_destroy( t_hercule_solution_space_state_stack stack )
{
  STACK_DECL( stack_ptr );
  STACK_ITEM_DECL( item );
  MemHandle next;

  stack_ptr = LOCK_STACK( stack );
  while( stack_ptr->head != NULL ) {
    item = LOCK_STACK_ITEM( stack_ptr->head );

    MemHandleFree( item->grid );
    next = item->previous;
    UNLOCK_STACK_ITEM( stack_ptr->head );
    MemHandleFree( stack_ptr->head );

    stack_ptr->head = next;
  }

  UNLOCK_STACK( stack );
  MemHandleFree( stack );
}


void hercule_solution_space_state_stack_push( t_hercule_solution_space_state_stack stack,
                                              t_hercule_solution_space solution )
{
  SOL_DECL( sol );
  STACK_DECL( stack_ptr );
  STACK_ITEM_DECL( item );

  t_uint8 size;
  t_uint8 *new_grid;
  t_uint8 *old_grid;
  MemHandle new_grid_handle;
  MemHandle item_handle;

  stack_ptr = LOCK_STACK( stack );

  if( stack_ptr->max_length > 0 && stack_ptr->length > stack_ptr->max_length ) {
    if( stack_ptr->overflow_behavior == ON_OVERFLOW_IGNORE ) {
      UNLOCK_STACK( stack );
      return;
    } else if( stack_ptr->overflow_behavior == ON_OVERFLOW_DELETE_OLDEST ) {
      item_handle = stack_ptr->tail;
      item = LOCK_STACK_ITEM( item_handle );
      stack_ptr->tail = item->next;
      UNLOCK_STACK_ITEM( item_handle );
      static_destroy_stack_item( item_handle );
      stack_ptr->length--;
      item = LOCK_STACK_ITEM( stack_ptr->tail );
      item->previous = NULL;
      UNLOCK_STACK_ITEM( stack_ptr->tail );
    }
  }

  sol = LOCK_SOLUTION( solution );

  /* create a copy of the current grid */
  size = sol->cols * sol->rows;
  new_grid_handle = MemHandleNew( size );
  new_grid = (t_uint8*)MemHandleLock( new_grid_handle );
  old_grid = LOCK_GRID( sol );
  MemMove( new_grid, old_grid, size );
  UNLOCK_GRID( sol );
  MemHandleUnlock( new_grid_handle );

  item_handle = MemHandleNew( sizeof( HERC_SOLUTION_STATE_STACK_ITEM ) );
  item = LOCK_STACK_ITEM( item_handle );
  item->grid = new_grid_handle;
  item->previous = stack_ptr->head;
  item->next = NULL;
  UNLOCK_STACK_ITEM( item_handle );

  if( stack_ptr->head == NULL ) {
    stack_ptr->tail = item_handle;
  } else {
    item = LOCK_STACK_ITEM( stack_ptr->head );
    item->next = item_handle;
    UNLOCK_STACK_ITEM( stack_ptr->head );
  }

  stack_ptr->head = item_handle;
  stack_ptr->length++;

  UNLOCK_STACK( stack );
  UNLOCK_SOLUTION( solution );
}


void hercule_solution_space_state_stack_pop( t_hercule_solution_space_state_stack stack,
                                             t_hercule_solution_space solution )
{
  SOL_DECL( sol );
  STACK_DECL( stack_ptr );
  STACK_ITEM_DECL( item );
  MemHandle itemH;

  stack_ptr = LOCK_STACK( stack );

  /* if the stack is not empty... */
  if( stack_ptr->length > 0 ) {
    sol = LOCK_SOLUTION( solution );

    /* destroy the current grid, and... */
    MemHandleFree( sol->grid );

    /* replace it with the one on the top of the stack */
    item = LOCK_STACK_ITEM( stack_ptr->head );
    sol->grid = item->grid;

    UNLOCK_SOLUTION( solution );

    /* pop the stack */
    itemH = stack_ptr->head;
    stack_ptr->head = item->previous;
    UNLOCK_STACK_ITEM( itemH );
    MemHandleFree( itemH );

    if( stack_ptr->head != NULL ) {
      item = LOCK_STACK_ITEM( stack_ptr->head );
      item->next = NULL;
      UNLOCK_STACK_ITEM( stack_ptr->head );
    } else {
      stack_ptr->tail = NULL;
    }

    stack_ptr->length--;
  }

  UNLOCK_STACK( stack );
}


t_bool hercule_solution_space_state_stack_empty( t_hercule_solution_space_state_stack stack )
{
  STACK_DECL( stack_ptr );
  t_bool empty;

  stack_ptr = LOCK_STACK( stack );
  empty = ( stack_ptr->length == 0 );
  UNLOCK_STACK( stack );

  return empty;
}


/* ---- Hercule Solution Interface Implementation ---------------- */

t_hercule_solution_space hercule_solution_space_new( t_hercule_puzzle puzzle )
{
  t_hercule_solution_space sol_handle;
  HERC_SOLUTION_SPACE* solution;
  t_uint8 size;
  t_uint8 i;
  t_uint8 bits;
  t_uint8* grid;

  sol_handle = (t_hercule_solution_space)MemHandleNew( sizeof( HERC_SOLUTION_SPACE ) );
  solution = LOCK_SOLUTION( sol_handle );
  solution->puzzle = puzzle;

  /* this is duplicating data, but it makes the info easier to obtain */
  solution->rows = hercule_puzzle_get_rows( puzzle );
  solution->cols = hercule_puzzle_get_cols( puzzle );

  size = solution->rows * solution->cols;
  solution->grid = MemHandleNew( size );
  grid = LOCK_GRID( solution );

  /* initialize each position on the solution space to be all possible values */

  bits = static_all_bits[ solution->cols ];
  for( i = 0; i < size; i++ ) {
    grid[ i ] = bits;
  }

  UNLOCK_GRID( solution );
  UNLOCK_SOLUTION( sol_handle );

  return sol_handle;
}


void hercule_solution_space_destroy( t_hercule_solution_space solution )
{
  SOL_DECL( sol );

  sol = LOCK_SOLUTION( solution );
  MemHandleFree( sol->grid );

  UNLOCK_SOLUTION( solution );
  MemHandleFree( solution );
}


t_uint32 hercule_solution_space_assert( t_hercule_solution_space solution,
                                        t_uint8 row,
                                        t_uint8 col,
                                        t_uint8 value,
                                        t_bool exists,
                                        t_bool auto_deduce )
{
  SOL_DECL( sol );
  t_uint8* grid;
  t_uint32 score;
  sol = LOCK_SOLUTION( solution );
  grid = LOCK_GRID( sol );
  score = static_assert( sol, grid, row, col, value, exists, auto_deduce, NULL );
  UNLOCK_GRID( sol );
  UNLOCK_SOLUTION( solution );
  return score;
}


t_uint8 hercule_solution_space_at( t_hercule_solution_space solution,
                                   t_uint8 row,
                                   t_uint8 col )
{
  SOL_DECL( sol );
  t_uint8* grid;
  t_uint8  at;
  
  sol = LOCK_SOLUTION( solution );
  grid = LOCK_GRID( sol );  
  at = grid[ COMPUTE_IDX( sol->cols, row, col ) ];
  UNLOCK_GRID( sol );
  UNLOCK_SOLUTION( solution );

  return at;
}


void hercule_solution_space_put( t_hercule_solution_space solution,
                                 t_uint8 row,
                                 t_uint8 col,
                                 t_uint8 value )
{
  SOL_DECL( sol );
  t_uint8* grid;
  
  sol = LOCK_SOLUTION( solution );
  grid = LOCK_GRID( sol );  
  grid[ COMPUTE_IDX( sol->cols, row, col ) ] = value;
  UNLOCK_GRID( sol );
  UNLOCK_SOLUTION( solution );
}


t_bool hercule_solution_space_solved( t_hercule_solution_space solution )
{
  SOL_DECL( sol );
  t_uint8 i;
  t_uint8 size;
  t_uint8* grid;

  sol = LOCK_SOLUTION( solution );
  grid = LOCK_GRID( sol );

  size = sol->rows * sol->cols;
  for( i = 0; i < size; i++ ) {
    if( static_count_bits( grid[ i ] ) > 1 ) {
      UNLOCK_GRID( sol );
      UNLOCK_SOLUTION( solution );
      return 0;
    }
  }

  UNLOCK_GRID( sol );
  UNLOCK_SOLUTION( solution );
  return 1;
}


t_bool hercule_solution_space_correct( t_hercule_solution_space solution )
{
  SOL_DECL( sol );
  t_uint8 i;
  t_uint8 size;
  t_uint8* grid;

  sol = LOCK_SOLUTION( solution );
  grid = LOCK_GRID( sol );

  /* return true if the grid is both solved, and correct (as compared to the puzzle) */

  size = sol->rows * sol->cols;
  for( i = 0; i < size; i++ ) {
    if( grid[ i ] != static_bit[ hercule_puzzle_at( sol->puzzle, ( i / sol->cols ), ( i % sol->cols ) ) ] ) {
      UNLOCK_GRID( sol );
      UNLOCK_SOLUTION( solution );
      return 0;
    }
  }

  UNLOCK_GRID( sol );
  UNLOCK_SOLUTION( solution );
  return 1;
}

 
t_bool hercule_solution_space_solved_at( t_hercule_solution_space solution,
                                         t_uint8 row,
                                         t_uint8 col )
{
  SOL_DECL( sol );
  t_bool solved;
  t_uint8* grid;

  sol = LOCK_SOLUTION( solution );
  grid = LOCK_GRID( sol );
  solved = ( static_count_bits( AT( sol, grid, row, col ) ) == 1 );
  UNLOCK_GRID( sol );
  UNLOCK_SOLUTION( solution );

  return solved;
}


t_uint32 hercule_solution_space_apply( t_hercule_solution_space solution,
                                       t_hercule_clue clue )
{
  SOL_DECL( sol );
  t_uint32 score;

  sol = LOCK_SOLUTION( solution );
  score = static_solution_space_apply( sol, clue, NULL );
  UNLOCK_SOLUTION( solution );

  return score;
}


t_bool hercule_solution_space_hint( t_hercule_solution_space solution,
                                    t_hercule_hint* hint )
{
  SOL_DECL( sol );
  t_hercule_clue* clues;
  t_uint8 clue_count;
  t_uint8 i;
  t_bool  hint_found = 0;
  
  t_hercule_solution_space_state_stack stack;

  stack = hercule_solution_space_state_stack_new( 0, 0 );
  hercule_solution_space_state_stack_push( stack, solution );

  sol = LOCK_SOLUTION( solution );
  clue_count = hercule_puzzle_get_clue_count( sol->puzzle );
  clues = hercule_puzzle_open_clues( sol->puzzle );

  for( i = 0; i < clue_count; i++ ) {
    hint->clue = clues[i];
    hint->which_clue = i;
    if( static_solution_space_apply( sol, hint->clue, hint ) > 0 ) {
      hint_found = 1;
      break;
    }
  }

  hercule_puzzle_close_clues( sol->puzzle );
  UNLOCK_SOLUTION( solution );

  hercule_solution_space_state_stack_pop( stack, solution );
  hercule_solution_space_state_stack_destroy( stack );

  return hint_found;
}

/* ---- Hercule Private Method Implementation ---------------- */

static void static_swap_elements( t_uint8* grid, t_uint8 idx1, t_uint8 idx2 )
{
  t_uint8 tmp;

  tmp = grid[ idx1 ];
  grid[ idx1 ] = grid[ idx2 ];
  grid[ idx2 ] = tmp;
}


static void static_generate_clues( t_hercule_puzzle puzzle )
{
  PZL_DECL( pzl );
  SOL_DECL( sol );
  t_hercule_solution_space solution;
  HERC_WEIGHTED_CLUE* clue_set;
  t_hercule_clue new_clue;
  t_hercule_coord coord;
  t_uint8 clue_set_pos;
  t_uint8 i;
  t_uint32 score;
  t_uint32 sub_score;
  t_hercule_clue heaviest_item;
  t_hercule_clue* clues;
  t_hercule_solution_space_state_stack state_stack;
  t_uint8 a;
  t_uint8 b;

  static_progress_dlg = OpenProgressDlg( "Building Puzzle..." );
  static_this_counts = false;

  pzl = LOCK_PUZZLE( puzzle );

  solution = hercule_solution_space_new( puzzle );
  sol = LOCK_SOLUTION( solution );

  state_stack = hercule_solution_space_state_stack_new( 0, 0 );

  clue_set = (HERC_WEIGHTED_CLUE*)MemPtrNew( pzl->accuracy * sizeof( HERC_WEIGHTED_CLUE ) );

  pzl = LOCK_PUZZLE( puzzle );
  /* first, assign the handicap squares */
  for( i = 0; i < pzl->handicap; i++ ) {
    do {
      coord.row = SysRandom(0) % pzl->rows;
      coord.col = SysRandom(0) % pzl->cols;
    } while( hercule_solution_space_solved_at( solution, coord.row, coord.col ) );
    new_clue = hercule_clue_new( &coord, HERCULE_CLUE_GIVEN, NULL, NULL );
    hercule_solution_space_apply( solution, new_clue );
    hercule_puzzle_add_clue( puzzle, new_clue );
  }

  /* build set of other clues */
  while( !hercule_solution_space_solved( solution ) )
  {
    UpdateProgressDlg( static_progress_dlg, static_solution_percent_solved( sol ) );

    /* save the solution state */
    hercule_solution_space_state_stack_push( state_stack, solution );
 
    clues = hercule_puzzle_open_clues( puzzle );

    /* we're going to look at 'accuracy' random clues to see which one has the greatest effect on the solution
     * state.  "greatest effect" is determined as follows:
     *
     *   a single possibility eliminated is worth 1 point
     *   a single square solved is worth 100 points
     *
     * each clue is applied, and then all clues are reapplied repeatedly, until no more changes occur.
     * The clue (and it's score) is then added to the "clue_set" array. */

    clue_set_pos = 0;
    while( clue_set_pos < pzl->accuracy )
    {
      score = 0;

      new_clue = static_get_random_clue( solution );
      if( new_clue == (t_hercule_clue)NULL ) continue;

      /* save the solution state */
      hercule_solution_space_state_stack_push( state_stack, solution );

      score = static_solution_space_apply( sol, new_clue, NULL );
      if( score < 1 )
      { /* the clue had no effect on the solution space, so we ignore it */
        hercule_clue_destroy( new_clue );
        hercule_solution_space_state_stack_pop( state_stack, solution );
        continue;
      }

      /* reapply all previous clues (as well as the new one), in case the new clue opens up
       * new possibilities for a previous clue. Continue reapplying the clues until no more
       * changes are possible. */
      do {
        sub_score = 0;
        for( i = 0; i < pzl->clue_count; i++ ) {
          sub_score += static_solution_space_apply( sol, clues[ i ], NULL );
        }
        sub_score += static_solution_space_apply( sol, new_clue, NULL );
        score += sub_score;
      } while( sub_score > 0 );

      clue_set[ clue_set_pos ].score = score;
      clue_set[ clue_set_pos ].clue = new_clue;
      clue_set_pos++;

      /* restore the solution state */
      hercule_solution_space_state_stack_pop( state_stack, solution );
    }

    /* restore the solution state and find the clue with the greatest score */
    hercule_solution_space_state_stack_pop( state_stack, solution );

    heaviest_item = 0;
    score = 0;
    for( i = 0; i < pzl->accuracy; i++ ) {
      if( clue_set[ i ].score > score ) {
        score = clue_set[ i ].score;
        heaviest_item = clue_set[ i ].clue;
      }
    }

    /* once we find the heaviest item, then we go back and destroy all the other
     * clues, which are now irrelevant */

    for( i = 0; i < pzl->accuracy; i++ ) {
      if( clue_set[ i ].clue != heaviest_item ) {
        hercule_clue_destroy( clue_set[ i ].clue );
      }
    }

    hercule_puzzle_close_clues( puzzle );

    /* add the clue to the puzzle */
    hercule_puzzle_add_clue( puzzle, heaviest_item );

    static_this_counts = 1;

    /* reapply all changes for this clue to the solution state */
    static_solution_space_apply( sol, heaviest_item, NULL );

    clues = hercule_puzzle_open_clues( puzzle );
    do {
      sub_score = 0;
      for( i = 0; i < pzl->clue_count; i++ ) {
        sub_score += static_solution_space_apply( sol, clues[ i ], NULL );
      }
    } while( sub_score > 0 );
    hercule_puzzle_close_clues( puzzle );

    static_this_counts = 0;
  }

  /* randomize the clues so that they aren't in the same order used by
   * the engine to generate them. */

  clues = hercule_puzzle_open_clues( puzzle );

  for( i = 0; i < pzl->clue_count; i++ ) {
    a = SysRandom( 0 ) % pzl->clue_count;
    do { b = SysRandom( 0 ) % pzl->clue_count; } while( a == b );
    heaviest_item = clues[a];
    clues[a] = clues[b];
    clues[b] = heaviest_item;
  }

  /* sort the clues -- horizontal vs. vertical */
  if( g_sort_info.auto_sort ) {
    hercule_clue_sort( puzzle, &g_sort_info );
  }

  hercule_puzzle_close_clues( puzzle );

  UpdateProgressDlg( static_progress_dlg, 100 );

  /* clean-up, and exit! */
  MemPtrFree( clue_set );

  UNLOCK_SOLUTION( solution );
  hercule_solution_space_destroy( solution );
  hercule_solution_space_state_stack_destroy( state_stack );

  UNLOCK_PUZZLE( puzzle );

  CloseProgressDlg( static_progress_dlg, SysTicksPerSecond() / 4 );
}


static t_uint32 static_clear_row_except_col( HERC_SOLUTION_SPACE* sol,
                                             t_uint8 row,
                                             t_uint8 col,
                                             t_uint8 value )
{
  t_uint8 start;
  t_uint8 i;
  t_uint32 score = 0;
  t_uint8* grid;

  grid = LOCK_GRID( sol );
  start = sol->cols * row;
  for( i = 0; i < sol->cols; i++ ) {
    if( i == col ) continue;
    if( ( grid[ start + i ] & static_bit[ value ] ) != 0 ) {
      score += ELIMINATE_SCORE;
      grid[ start + i ] &= ~static_bit[ value ];
      if( static_count_bits( grid[ start+i ] ) == 1 ) {
        /* if, by eliminating that value, we are left with exactly one answer, we increment our
         * score and recursively call static_clear_row_except_col to remove the given bit from all
         * other positions. */
        score += DISCOVER_SCORE;
        score += static_clear_row_except_col( sol, row, i, static_least_sig_bit( grid[start+i] ) );
      }
    }
  }
  UNLOCK_GRID( sol );

  return score;
}


static t_uint8 static_count_bits( t_uint8 byte )
{
  t_uint8 count = 0;
  count += static_bits_set[ byte & 0x0F ];
  byte >>= 4;
  count += static_bits_set[ byte & 0x0F ];
  return count;
}


static t_uint8 static_least_sig_bit( t_uint8 byte )
{
  t_uint8 which = 0;
  while( ( ( byte >> which ) & 0x01 ) == 0 ) {
    which++;
  }
  return which;
}


static t_uint32 static_apply_left_of( HERC_SOLUTION_SPACE* sol,
                                      t_uint8* grid,
                                      t_hercule_coord* foundation,
                                      t_hercule_coord* op,
                                      t_hercule_hint* hint )
{
  t_uint32 score = 0;
  t_uint8  fitem;
  t_uint8  opitem;
  t_uint8  col;
  t_uint8  i;
  t_uint8  count;
  t_uint8  at = 0;

  fitem = hercule_puzzle_at( sol->puzzle, foundation->row, foundation->col );
  opitem = hercule_puzzle_at( sol->puzzle, op->row, op->col );

  /* first, find the right-most instance of opitem */
  col = sol->cols-1;
  while( ( col > 0 ) && !IS_POSSIBLE( sol, grid, op->row, col, opitem ) ) col--;

  /* check to see if this 'opitem' is solved.  If it is, look to see if there is only
   * one possible 'fitem' to the left of here.  If there is, then that has to be the
   * one. */
  if( AT( sol, grid, op->row, col ) == ( 1 << opitem ) )
  {
    count = 0;
    for( i = 0; i < col; i++ )
    {
      if( IS_POSSIBLE( sol, grid, foundation->row, i, fitem ) ) {
        count++;
        at = i;
        if( count > 1 ) break;
      }
    }
    if( count == 1 ) {
      score += static_assert( sol, grid, foundation->row, at, fitem, 1, 1, hint );
      if( score > 0 ) return score;
    }
  }

  /* once found, remove all fitems beginning in that column and proceeding rightward */
  while( col < sol->cols ) {
    score += static_assert( sol, grid, foundation->row, col++, fitem, 0, 1, hint );
    if( ( hint != NULL ) && ( score > 0 ) ) return score;
  }

  /* next, find the left-most instance of fitem */
  col = 0;
  while( ( col < sol->cols-1 ) && !IS_POSSIBLE( sol, grid, foundation->row, col, fitem ) ) col++;

  /* check to see if this 'fitem' is solved.  If it is, look to see if there is only
   * one possible 'opitem' to the right of here.  If there is, then that has to be the
   * one. */
  if( AT( sol, grid, foundation->row, col ) == ( 1 << fitem ) )
  {
    count = 0;
    for( i = col+1; i < sol->cols; i++ )
    {
      if( IS_POSSIBLE( sol, grid, op->row, i, opitem ) ) {
        count++;
        at = i;
        if( count > 1 ) break;
      }
    }
    if( count == 1 ) {
      score += static_assert( sol, grid, op->row, at, opitem, 1, 1, hint );
      if( score > 0 ) return score;
    }
  }

  /* finally, remove all opitems beginning in that column and proceeding leftward */
  for( i = 0; i <= col; i++ ) {
    score += static_assert( sol, grid, op->row, i, opitem, 0, 1, hint );
    if( ( hint != NULL ) && ( score > 0 ) ) return score;
  }

  return score;
}


static t_uint32 static_apply_next_to( HERC_SOLUTION_SPACE* sol,
                                      t_uint8* grid,
                                      t_hercule_coord* foundation,
                                      t_hercule_coord* op,
                                      t_hercule_hint* hint )
{
  t_uint32 score = 0;
  t_uint8  fitem;
  t_uint8  opitem;
  t_uint8  col;
  t_uint8  col2;
  t_uint8  next_to;

  fitem = hercule_puzzle_at( sol->puzzle, foundation->row, foundation->col );
  opitem = hercule_puzzle_at( sol->puzzle, op->row, op->col );
 
  /* first, look at the foundation row. For each foundation possibility, consider */
  /* the operand positions to the left and right.  If operand does not exist in */
  /* either position, eliminate this foundation position as a possibility. */

  for( col = 0; col < sol->cols; col++ )
  {
    /* if foundation has been solved, eliminate all operand possibilities except those */
    /* to the left and right, and then exit. */
    if( grid[ COMPUTE_IDX( sol->cols, foundation->row, col ) ] == static_bit[ fitem ] )
    {
      /* look FIRST for whether or not we can deduce the location of op1 */
      if( ( col < 1 ) || !IS_POSSIBLE( sol, grid, op->row, col-1, opitem ) ) {
        score += static_assert( sol, grid, op->row, col+1, opitem, 1, 1, hint );
        return score;
      } else if( ( col >= sol->cols-1 ) || !IS_POSSIBLE( sol, grid, op->row, col+1, opitem ) ) {
        score += static_assert( sol, grid, op->row, col-1, opitem, 1, 1, hint );
        return score;
      }

      for( col2 = 0; col2 < sol->cols; col2++ )
      {
        if( ( col2 == col-1 ) || ( col2 == col+1 ) ) continue;
        score += static_assert( sol, grid, op->row, col2, opitem, 0, 1, hint );
      }
      return score;
    }

    /* if the op has been solved, eliminate all foundation possibilities except those */
    /* to the left and right, and then exit. */
    if( grid[ COMPUTE_IDX( sol->cols, op->row, col ) ] == static_bit[ opitem ] )
    {
      /* look FIRST for whether or not we can deduce the location of foundation */
      if( ( col < 1 ) || !IS_POSSIBLE( sol, grid, foundation->row, col-1, fitem ) ) {
        score += static_assert( sol, grid, foundation->row, col+1, fitem, 1, 1, hint );
        return score;
      } else if( ( col >= sol->cols-1 ) || !IS_POSSIBLE( sol, grid, foundation->row, col+1, fitem ) ) {
        score += static_assert( sol, grid, foundation->row, col-1, fitem, 1, 1, hint );
        return score;
      }

      for( col2 = 0; col2 < sol->cols; col2++ )
      {
        if( ( col2 == col-1 ) || ( col2 == col+1 ) ) continue;
        score += static_assert( sol, grid, foundation->row, col2, fitem, 0, 1, hint );
      }
      return score;
    }
  }

  for( col = 0; col < sol->cols; col++ ) {
    if( IS_POSSIBLE( sol, grid, foundation->row, col, fitem ) ) {
      next_to = 0;
      if( ( col > 0 ) && IS_POSSIBLE( sol, grid, op->row, col-1, opitem ) ) next_to++;
      if( ( col < sol->cols-1 ) && IS_POSSIBLE( sol, grid, op->row, col+1, opitem ) ) next_to++;
      if( next_to == 0 ) {
        score += static_assert( sol, grid, foundation->row, col, fitem, 0, 1, hint );
        if( ( hint != NULL ) && ( score > 0 ) ) return score;
      }
    }

    if( IS_POSSIBLE( sol, grid, op->row, col, opitem ) ) {
      next_to = 0;
      if( ( col > 0 ) && IS_POSSIBLE( sol, grid, foundation->row, col-1, fitem ) ) next_to++;
      if( ( col < sol->cols-1 ) && IS_POSSIBLE( sol, grid, foundation->row, col+1, fitem ) ) next_to++;
      if( next_to == 0 ) {
        score += static_assert( sol, grid, op->row, col, opitem, 0, 1, hint );
        if( ( hint != NULL ) && ( score > 0 ) ) return score;
      }
    }
  }

  return score;
}


static t_uint32 static_apply_same_column( HERC_SOLUTION_SPACE* sol,
                                          t_uint8* grid,
                                          t_hercule_coord* foundation,
                                          t_hercule_coord* op,
                                          t_hercule_hint* hint )
{
  t_uint32 score = 0;
  t_uint8  fitem;
  t_uint8  opitem;
  t_uint8  col;

  fitem = hercule_puzzle_at( sol->puzzle, foundation->row, foundation->col );
  opitem = hercule_puzzle_at( sol->puzzle, op->row, op->col );
 
  /* look for solved items in both the foundation and op rows */
  for( col = 0; col < sol->cols; col++ )
  {
    if( AT( sol, grid, foundation->row, col ) == static_bit[ fitem ] )
    { /* foundation is solved, so we can deduce the location of op and exit */
      score += static_assert( sol, grid, op->row, col, opitem, 1, 1, hint );
      if( ( hint != NULL ) && ( score > 0 ) ) return score;
      break;
    }

    if( AT( sol, grid, op->row, col ) == static_bit[ opitem ] )
    { /* op is solved, so we can deduce the location of foundation and exit */
      score += static_assert( sol, grid, foundation->row, col, fitem, 1, 1, hint );
      if( ( hint != NULL ) && ( score > 0 ) ) return score;
      break;
    }
  }

  /* if no solved items were found, then we look for places where the items _cannot_ be */
  for( col = 0; col < sol->cols; col++ ) {
    if( !IS_POSSIBLE( sol, grid, foundation->row, col, fitem ) )
    { /* if the foundation is not a possible solution here, we know that op is not, either */
      score += static_assert( sol, grid, op->row, col, opitem, 0, 1, hint );
      if( ( hint != NULL ) && ( score > 0 ) ) return score;
    }
    else if( !IS_POSSIBLE( sol, grid, op->row, col, opitem ) )
    { /* if op is not a possible solution here, we know that the foundation is not, either */
      score += static_assert( sol, grid, foundation->row, col, fitem, 0, 1, hint );
      if( ( hint != NULL ) && ( score > 0 ) ) return score;
    }
  }

  return score;
}


static t_uint32 static_apply_between( HERC_SOLUTION_SPACE* sol,
                                      t_uint8* grid,
                                      t_hercule_coord* foundation,
                                      t_hercule_coord* op1,
                                      t_hercule_coord* op2,
                                      t_hercule_hint* hint )
{
  t_uint32 score = 0;
  t_uint8  fitem;
  t_uint8  op1item;
  t_uint8  op2item;
  t_uint8  col;
  t_uint8  col2;
  t_bool   do_exit;
  t_bool   solved_f = 0;
  t_bool   solved_o1 = 0;
  t_bool   solved_o2 = 0;

  fitem = hercule_puzzle_at( sol->puzzle, foundation->row, foundation->col );
  op1item = hercule_puzzle_at( sol->puzzle, op1->row, op1->col );
  op2item = hercule_puzzle_at( sol->puzzle, op2->row, op2->col );

  /* first, we eliminate foundation from the far left and far right positions (if it
   * is between two items, it can't be on an edge) */

  score += static_assert( sol, grid, foundation->row, 0, fitem, 0, 1, hint );
  if( ( hint != NULL ) && ( score > 0 ) ) return score;
  score += static_assert( sol, grid, foundation->row, sol->cols-1, fitem, 0, 1, hint );
  if( ( hint != NULL ) && ( score > 0 ) ) return score;

  /* then, we look to see if any of the items have been solved.  If any two have been solved,
   * we can easily deduce the third. Also, if any one has been solved, we can possibly deduce
   * others. */

  for( col = 0; col < sol->cols; col++ )
  {
    if( AT( sol, grid, foundation->row, col ) == ( 1 << fitem ) ) {
      solved_f = 1;
    }
    if( AT( sol, grid, op1->row, col ) == ( 1 << op1item ) ) {
      solved_o1 = 1;
    }
    if( AT( sol, grid, op2->row, col ) == ( 1 << op2item ) ) {
      solved_o2 = 1;
    }
  }

  if( solved_o1 ) {
    /* if o1 is solved, look to see if there is only one possible o2 and/or f to assert */
    if( ( op1->col < 2 ) || !IS_POSSIBLE( sol, grid, op2->row, op1->col-2, op2item ) ) {
      score += static_assert( sol, grid, op2->row, op1->col+2, op2item, 1, 1, hint );
      solved_o2 = 1;
    } else if( ( op1->col >= sol->cols-2 ) || !IS_POSSIBLE( sol, grid, op2->row, op1->col+2, op2item ) ) {
      score += static_assert( sol, grid, op2->row, op1->col-2, op2item, 1, 1, hint );
      solved_o2 = 1;
    } else if( ( op1->col < 1 ) || !IS_POSSIBLE( sol, grid, foundation->row, op1->col-1, fitem ) ) {
      score += static_assert( sol, grid, foundation->row, op1->col+1, fitem, 1, 1, hint );
      solved_f = 1;
    } else if( ( op1->col >= sol->cols-1 ) || !IS_POSSIBLE( sol, grid, foundation->row, op1->col+1, fitem ) ) {
      score += static_assert( sol, grid, foundation->row, op1->col-1, fitem, 1, 1, hint );
      solved_f = 1;
    }
  } else if( solved_o2 ) {
    /* if o2 is solved, look to see if there is only one possible o1 and/or f to assert */
    if( ( op2->col < 2 ) || !IS_POSSIBLE( sol, grid, op1->row, op2->col-2, op1item ) ) {
      score += static_assert( sol, grid, op1->row, op2->col+2, op1item, 1, 1, hint );
      solved_o1 = 1;
    } else if( ( op2->col >= sol->cols-2 ) || !IS_POSSIBLE( sol, grid, op1->row, op2->col+2, op1item ) ) {
      score += static_assert( sol, grid, op1->row, op2->col-2, op1item, 1, 1, hint );
      solved_o1 = 1;
    } else if( ( op2->col < 1 ) || !IS_POSSIBLE( sol, grid, foundation->row, op2->col-1, fitem ) ) {
      score += static_assert( sol, grid, foundation->row, op2->col+1, fitem, 1, 1, hint );
      solved_f = 1;
    } else if( ( op2->col >= sol->cols-1 ) || !IS_POSSIBLE( sol, grid, foundation->row, op2->col+1, fitem ) ) {
      score += static_assert( sol, grid, foundation->row, op2->col-1, fitem, 1, 1, hint );
      solved_f = 1;
    }
  } else if( solved_f ) {
    /* if f is solved, look to see if there is only one possible o1 and/or o2 to assert */
    if( ( foundation->col < 1 ) || !IS_POSSIBLE( sol, grid, op1->row, foundation->col-1, op1item ) ) {
      score += static_assert( sol, grid, op1->row, foundation->col+1, op1item, 1, 1, hint );
      solved_o1 = 1;
    } else if( ( foundation->col >= sol->cols-1 ) || !IS_POSSIBLE( sol, grid, op1->row, foundation->col+1, op1item ) ) {
      score += static_assert( sol, grid, op1->row, foundation->col-1, op1item, 1, 1, hint );
      solved_o1 = 1;
    } else if( ( foundation->col < 1 ) || !IS_POSSIBLE( sol, grid, op2->row, foundation->col-1, op2item ) ) {
      score += static_assert( sol, grid, op2->row, foundation->col+1, op2item, 1, 1, hint );
      solved_o2 = 1;
    } else if( ( foundation->col >= sol->cols-1 ) || !IS_POSSIBLE( sol, grid, op2->row, foundation->col+1, op2item ) ) {
      score += static_assert( sol, grid, op2->row, foundation->col-1, op2item, 1, 1, hint );
      solved_o2 = 1;
    }
  }

  if( ( hint != NULL ) && ( score > 0 ) ) return score;

  if( solved_f + solved_o1 + solved_o2 >= 2 )  {
    if( solved_f && solved_o1 ) {
      if( op1->col < foundation->col ) {
        score += static_assert( sol, grid, op2->row, foundation->col+1, op2item, 1, 1, hint );
      } else {
        score += static_assert( sol, grid, op2->row, foundation->col-1, op2item, 1, 1, hint );
      }
    } else if( solved_o1 && solved_o2 ) {
      if( op1->col < op2->col ) {
        score += static_assert( sol, grid, foundation->row, op1->col+1, fitem, 1, 1, hint );
      } else {
        score += static_assert( sol, grid, foundation->row, op1->col-1, fitem, 1, 1, hint );
      }
    } else if( solved_o2 && solved_f ) {
      if( op2->col < foundation->col ) {
        score += static_assert( sol, grid, op1->row, foundation->col+1, op1item, 1, 1, hint );
      } else {
        score += static_assert( sol, grid, op1->row, foundation->col-1, op1item, 1, 1, hint );
      }
    }
    return score;
  }

  /* then, consider the foundation row, looking for a solved foundation item.  If we
   * find one, then we eliminate op1 and op2 from any position except to the left and
   * right of foundation. */

  for( col = 0; col < sol->cols; col++ )
  {
    if( AT( sol, grid, foundation->row, col ) == static_bit[ fitem ] )
    { /* the foundation item has been solved, so we can eliminate op1 and op2 from any
       * position except for the left and right of foundation.  Then, we can exit. */
      for( col2 = 0; col2 < sol->cols; col2++ )
      {
        if( ( col2 == col-1 ) || ( col2 == col+1 ) ) continue;
        score += static_assert( sol, grid, op1->row, col2, op1item, 0, 1, hint );
        if( ( hint != NULL ) && ( score > 0 ) ) break;
        score += static_assert( sol, grid, op2->row, col2, op2item, 0, 1, hint );
        if( ( hint != NULL ) && ( score > 0 ) ) break;
      }
      break;
    }
    else if( IS_POSSIBLE( sol, grid, foundation->row, col, fitem ) )
    { /* if a foundation item is possible here, we need to make sure that the op's are also
       * possible to either side. */
      t_bool op1left;
      t_bool op2left;
      t_bool op1right;
      t_bool op2right;

      op1left  = ( ( col > 0 ) && IS_POSSIBLE( sol, grid, op1->row, col-1, op1item ) );
      op2left  = ( ( col > 0 ) && IS_POSSIBLE( sol, grid, op2->row, col-1, op2item ) );
      op1right = ( ( col > 0 ) && IS_POSSIBLE( sol, grid, op1->row, col+1, op1item ) );
      op2right = ( ( col > 0 ) && IS_POSSIBLE( sol, grid, op2->row, col+1, op2item ) );

      if( !( op1left && op2right ) && !( op1right && op2left ) )
      {
        score += static_assert( sol, grid, foundation->row, col, fitem, 0, 1, hint );
        if( ( hint != NULL ) && ( score > 0 ) ) break;
      }
    }

    score += static_apply_between_a_and_b( sol,
                                           grid,
                                           op1, op2, op1item, op2item,
                                           foundation, fitem,
                                           col, &do_exit, hint );
    if( do_exit ) break;
    if( ( hint != NULL ) && ( score > 0 ) ) break;

    score += static_apply_between_a_and_b( sol,
                                           grid,
                                           op2, op1, op2item, op1item,
                                           foundation, fitem,
                                           col, &do_exit, hint );
    if( do_exit ) break;
    if( ( hint != NULL ) && ( score > 0 ) ) break;
  }

  return score;
}

static t_uint32 static_assert( HERC_SOLUTION_SPACE* solution,
                               t_uint8* grid,
                               t_uint8 row,
                               t_uint8 col,
                               t_uint8 value,
                               t_bool exists,
                               t_bool auto_deduce,
                               t_hercule_hint* hint )
{
  t_uint32 score = 0;
  t_uint8 idx;
  t_uint8 found_in_col;
  t_uint8 found_count;
  t_uint8 i;

  idx = COMPUTE_IDX( solution->cols, row, col );

  if( exists ) /* we are going to assert that the value exists here */
  {
    if( grid[ idx ] != static_bit[ value ] )
    {
      score += DISCOVER_SCORE;
      grid[ idx ] = static_bit[ value ];
      if( hint != NULL ) {
        SET_HINT( hint, exists, row, col, value );
      } else if( auto_deduce ) {
        score += static_clear_row_except_col( solution, row, col, value );
      }
    }
  }
  else /* we are going to assert that the value does NOT exist here */
  {
    if( ( grid[ idx ] & static_bit[ value ] ) != 0 ) {
      score += ELIMINATE_SCORE;
      grid[ idx ] &= ~static_bit[ value ];
      if( hint != NULL ) {
        SET_HINT( hint, exists, row, col, value );
      } else if( auto_deduce && ( static_count_bits( grid[ idx ] ) == 1 ) ) {
        /* by eliminating that value, we have shown there is exactly one solution at
         * this position, so we increment our score and call clear_row_except_col */
        score += DISCOVER_SCORE;
        score += static_clear_row_except_col( solution, row, col, static_least_sig_bit( grid[idx] ) );
      }
    }
  }

  /* look for any more values in this row that we can assert.  these will be "orphaned" values,
   * those that are now the only possible instance of themselves. */

  if( ( hint == NULL ) && ( auto_deduce ) ) {
    idx = COMPUTE_IDX( solution->cols, row, 0 );
    value = 0;
    while( value < solution->cols ) {
      found_count = 0;
      found_in_col = 0;
      for( i = 0; i < solution->cols; i++ ) {
        if( ( grid[ idx+i ] & static_bit[ value ] ) != 0 ) {
          if( grid[ idx+i ] == static_bit[ value ] ) {
            /* ignore this value if it is already solved */
            found_count = 5;
            break;
          }
          found_count++;
          if( found_count > 1 ) {
            break;
          }
          found_in_col = i;
        }
      }
      if( found_count == 1 ) {
        score += DISCOVER_SCORE;
        grid[ idx+found_in_col ] = static_bit[ value ];
        /* reset the loop, so we double check all the columns every time we solve a column */
        value = 0;
      } else {
        value++;
      }
    }
  }

  if( static_this_counts )
    UpdateProgressDlg( static_progress_dlg, static_solution_percent_solved( solution ) );

  return score;
}


static t_uint32 static_apply_between_a_and_b( HERC_SOLUTION_SPACE* sol,
                                              t_uint8* grid,
                                              t_hercule_coord* a,
                                              t_hercule_coord* b,
                                              t_uint8 aitem, 
                                              t_uint8 bitem,
                                              t_hercule_coord* foundation,
                                              t_uint8 fitem,
                                              t_uint8 col,
                                              t_bool* do_exit,
                                              t_hercule_hint* hint )
{
  t_uint32 score = 0;
  t_uint8  found_nearby;
  t_uint8  col2;
  t_uint8  col3;
  t_uint8  next_to;

  *do_exit = 0;
  if( AT( sol, grid, a->row, col ) == static_bit[ aitem ] )
  {
    /* if we find a solved 'a', we then look to the left and right to see if there is a
     * 'b' nearby.  If there is only one, we assert that it is the correct one, and assert
     * that the foundation must be between them.  If there are two, we eliminate all foundation
     * pieces except those between the 'a' and 'b's.  Then we exit. */

    found_nearby = 0;
    for( col2 = 0; col2 < sol->cols; col2++ )
    {
      if( IS_POSSIBLE( sol, grid, b->row, col2, bitem ) )
      {
        if( ( col2 == col-2 ) || ( col2 == col+2 ) )
        {
          found_nearby++;
          continue;
        }
        score += static_assert( sol, grid, b->row, col2, bitem, 0, 1, hint );
        if( ( hint != NULL ) && ( score > 0 ) ) return score;
      }
    }

    if( found_nearby == 1 )
    {
      if( ( col-2 >= 0 ) && IS_POSSIBLE( sol, grid, b->row, col-2, bitem ) )
      {
        score += static_assert( sol, grid, foundation->row, col-1, fitem, 1, 1, hint );
      }
      else
      {
        score += static_assert( sol, grid, foundation->row, col+1, fitem, 1, 1, hint );
      }
      if( ( hint != NULL ) && ( score > 0 ) ) return score;
    }
    else
    {
      for( col3 = 0; col3 < sol->cols; col3++ ) {
        if( ( col3 == col-1 ) || ( col3 == col+1 ) ) continue;
        score += static_assert( sol, grid, foundation->row, col3, fitem, 0, 1, hint );
        if( ( hint != NULL ) && ( score > 0 ) ) return score;
      }
    }

    *do_exit = 1;
  }
  else if( IS_POSSIBLE( sol, grid, a->row, col, aitem ) )
  {
    next_to = 0;
    if( ( col > 1 ) && IS_POSSIBLE( sol, grid, b->row, col-2, bitem ) ) next_to++;
    if( ( col < sol->cols-2 ) && IS_POSSIBLE( sol, grid, b->row, col+2, bitem ) ) next_to++;
    if( next_to == 0 ) {
      score += static_assert( sol, grid, a->row, col, aitem, 0, 1, hint );
      if( ( hint != NULL ) && ( score > 0 ) ) return score;
    }
  }

  return score;
}


static t_hercule_clue static_get_random_clue( t_hercule_solution_space solution )
{
  SOL_DECL( sol );
  t_hercule_coord foundation;
  t_hercule_coord op1;
  t_hercule_coord op2;
  t_uint8 clue_type;
  t_uint8 clue_set;
  t_uint8* grid;
  t_hercule_clue clue;

  sol = LOCK_SOLUTION( solution );
  grid = LOCK_GRID( sol );

  /* find an unsolved position on the grid */
  do {
    foundation.row = SysRandom(0) % sol->rows;
    foundation.col = SysRandom(0) % sol->cols;
  } while( static_count_bits( AT( sol, grid, foundation.row, foundation.col ) ) == 1 );

  /* find a random clue type appropriate for that position */
  clue_set = 0;
  clue_set |= ( 1 << HERCULE_CLUE_SAME_COLUMN );
  clue_set |= ( 1 << HERCULE_CLUE_NEXT_TO );

  if( foundation.col < sol->cols-1 ) clue_set |= ( 1 << HERCULE_CLUE_LEFT_OF );
  if( foundation.col > 0 && foundation.col < sol->cols-1 ) clue_set |= ( 1 << HERCULE_CLUE_BETWEEN );

  clue_type = static_get_bit( clue_set, SysRandom(0) % static_count_bits( clue_set ) );

  /* find the value of op1 (and possibly op2) based on the type of clue chosen */
  clue = NULL;
  switch( clue_type ) {
    case HERCULE_CLUE_LEFT_OF:
      /* choose a column greater than the foundation's column */
      op1.row = SysRandom(0) % sol->rows;
      op1.col = SysRandom(0) % ( sol->cols - foundation.col - 1 ) + foundation.col + 1;
      clue = hercule_clue_new( &foundation, clue_type, &op1, NULL );
      break;

    case HERCULE_CLUE_NEXT_TO:
      /* choose a column to the right or left of the foundation's column */
      op1.row = SysRandom(0) % sol->rows;
      if( foundation.col < 1 )
        op1.col = 1;
      else if( foundation.col >= sol->cols-1 )
        op1.col = foundation.col-1;
      else
        op1.col = foundation.col + ( SysRandom(0) % 2 == 0 ? -1 : 1 );
      clue = hercule_clue_new( &foundation, clue_type, &op1, NULL );
      break;

    case HERCULE_CLUE_BETWEEN:
      /* clue_set here represents the order in which op1 and op2 are created */
      clue_set = SysRandom(0) % 2;
      op1.row = SysRandom(0) % sol->rows;
      op1.col = foundation.col + ( clue_set == 0 ? -1 :  1 );
      op2.row = SysRandom(0) % sol->rows;
      op2.col = foundation.col + ( clue_set == 0 ?  1 : -1 );
      clue = hercule_clue_new( &foundation, clue_type, &op1, &op2 );
      break;

    case HERCULE_CLUE_SAME_COLUMN:
      op1.col = foundation.col;
      do {
        op1.row = SysRandom(0) % sol->rows;
      } while( op1.row == foundation.row );
      clue = hercule_clue_new( &foundation, clue_type, &op1, NULL );
      break;
  }

  UNLOCK_GRID( sol );
  UNLOCK_SOLUTION( solution );

  return clue;
}


static t_uint8 static_get_bit( t_uint8 byte, t_uint8 which )
{
  t_uint8 i; 
  t_uint8 count = 0;

  for( i = 0; i < 8; i++ ) {
    if( ( ( byte >> i ) & 0x01 ) == 1 ) {
      if( count == which  ) return i;
      count++;
    }
  }

  return 8;
}


t_uint32 static_solution_space_apply( HERC_SOLUTION_SPACE* sol,
                                      t_hercule_clue clue,
                                      t_hercule_hint* hint )
{
  t_uint32 score = 0;
  t_hercule_coord foundation;
  t_hercule_coord op1;
  t_hercule_coord op2;
  t_uint8* grid;

  grid = LOCK_GRID( sol );

  hercule_clue_get_foundation( clue, &foundation );
  hercule_clue_get_op1( clue, &op1 );

  switch( hercule_clue_get_clue_type( clue ) ) {
    case HERCULE_CLUE_LEFT_OF:
      score += static_apply_left_of( sol, grid, &foundation, &op1, hint );
      break;
    case HERCULE_CLUE_NEXT_TO:
      score += static_apply_next_to( sol, grid, &foundation, &op1, hint );
      break;
    case HERCULE_CLUE_BETWEEN:
      hercule_clue_get_op2( clue, &op2 );
      score += static_apply_between( sol, grid, &foundation, &op1, &op2, hint );
      break;
    case HERCULE_CLUE_SAME_COLUMN:
      score += static_apply_same_column( sol, grid, &foundation, &op1, hint );
      break;
    case HERCULE_CLUE_GIVEN:
      score += static_assert( sol, grid, foundation.row, foundation.col,
                              hercule_puzzle_at( sol->puzzle, foundation.row, foundation.col ),
                              1, 1, hint );
      break;
  }

  UNLOCK_GRID( sol );
  return score;

}


t_uint8 static_solution_percent_solved( HERC_SOLUTION_SPACE* sol )
{
  t_uint8 i;
  UInt16 size;
  t_uint8* grid;
  UInt16 remaining;
  UInt16 possible;

  grid = LOCK_GRID( sol );
  size = sol->rows * sol->cols;
  possible = size * sol->cols - size;
  remaining = 0;
  for( i = 0; i < size; i++ ) {
    remaining += static_count_bits( grid[i] );
  }

  size = (t_uint8)( (UInt16)( 100 * ( possible - remaining ) ) / possible );

  UNLOCK_GRID( sol );
  return size;
}


void static_destroy_stack_item( MemHandle item )
{
  STACK_ITEM_DECL( item_ptr );

  item_ptr = LOCK_STACK_ITEM( item );
  MemHandleFree( item_ptr->grid );
  UNLOCK_STACK_ITEM( item );
  MemHandleFree( item );
}
