#ifndef ___H__
#define ___H__

#include "hercule_sortdlg.h"

#define HERCULE_ENGINE   __attribute__ (( section("engine") ))

#define HERCULE_CLUE_LEFT_OF         (  0 )
#define HERCULE_CLUE_NEXT_TO         (  1 )
#define HERCULE_CLUE_BETWEEN         (  2 )
#define HERCULE_CLUE_SAME_COLUMN     (  3 )

#define HERCULE_CLUE_GIVEN           (  6 )


#define ON_OVERFLOW_IGNORE           (  0 )
#define ON_OVERFLOW_DELETE_OLDEST    (  1 )

typedef UInt8   t_uint8;
typedef UInt32  t_uint32;
typedef Boolean t_bool;

typedef MemHandle t_handle;

typedef t_handle  t_hercule_puzzle;
typedef t_handle  t_hercule_clue;
typedef t_handle  t_hercule_solution_space;
typedef t_handle  t_hercule_solution_space_state_stack;

typedef struct {
  t_uint8 row : 4;
  t_uint8 col : 4;
} t_hercule_coord;

typedef struct {
  t_bool          assert;
  t_hercule_coord position;
  t_uint8         value;
  t_hercule_clue  clue;
  t_uint8         which_clue;
} t_hercule_hint;

/* --- Hercule Serialization Interface --------------------------- */

t_uint32 hercule_serialize_session( t_uint8* buffer,
                                    t_hercule_puzzle puzzle,
                                    t_hercule_solution_space solution,
                                    t_hercule_solution_space_state_stack undo_stack,
                                    t_bool dont_write ) HERCULE_ENGINE;

t_uint8 hercule_unserialize_session( t_uint8* buffer,
                                     t_hercule_puzzle* puzzle,
                                     t_hercule_solution_space* solution,
                                     t_hercule_solution_space_state_stack* undo_stack ) HERCULE_ENGINE;

/* --- Hercule Puzzle Interface --------------------------- */

t_hercule_puzzle   hercule_puzzle_new( t_uint8 rows,
                                       t_uint8 cols,
                                       t_uint32 seed,
                                       t_uint8 handicap,
                                       t_uint8 accuracy ) HERCULE_ENGINE;

void               hercule_puzzle_destroy( t_hercule_puzzle puzzle ) HERCULE_ENGINE;

t_uint8            hercule_puzzle_get_rows( t_hercule_puzzle puzzle ) HERCULE_ENGINE;
t_uint8            hercule_puzzle_get_cols( t_hercule_puzzle puzzle ) HERCULE_ENGINE;
t_uint32           hercule_puzzle_get_seed( t_hercule_puzzle puzzle ) HERCULE_ENGINE;
t_uint8            hercule_puzzle_get_handicap( t_hercule_puzzle puzzle ) HERCULE_ENGINE;

t_uint8            hercule_puzzle_get_clue_count( t_hercule_puzzle puzzle ) HERCULE_ENGINE;
t_hercule_clue*    hercule_puzzle_open_clues( t_hercule_puzzle puzzle ) HERCULE_ENGINE;
void               hercule_puzzle_close_clues( t_hercule_puzzle puzzle ) HERCULE_ENGINE;

void               hercule_puzzle_add_clue( t_hercule_puzzle puzzle,
                                            t_hercule_clue clue ) HERCULE_ENGINE;

t_uint8            hercule_puzzle_at( t_hercule_puzzle puzzle,
                                      t_uint8 row,
                                      t_uint8 col ) HERCULE_ENGINE;

/* --- Hercule Clue Interface --------------------------- */

t_hercule_clue hercule_clue_new( t_hercule_coord* foundation,
                                 t_uint8          clue_type,
                                 t_hercule_coord* op1,
                                 t_hercule_coord* op2 ) HERCULE_ENGINE;
void           hercule_clue_destroy( t_hercule_clue clue ) HERCULE_ENGINE;

void           hercule_clue_get_foundation( t_hercule_clue clue,
                                            t_hercule_coord* coord ) HERCULE_ENGINE;
void           hercule_clue_get_op1( t_hercule_clue clue,
                                     t_hercule_coord* coord ) HERCULE_ENGINE;
void           hercule_clue_get_op2( t_hercule_clue clue,
                                     t_hercule_coord* coord ) HERCULE_ENGINE;

t_uint8        hercule_clue_get_clue_type( t_hercule_clue clue ) HERCULE_ENGINE;

void           hercule_clue_set_data( t_hercule_clue clue, t_uint8 data ) HERCULE_ENGINE;

t_uint8        hercule_clue_get_data( t_hercule_clue clue ) HERCULE_ENGINE;

void           hercule_clue_sort( t_hercule_puzzle puzzle, 
                                  HerculeClueSortInfo* sortInfo ) HERCULE_ENGINE;

/* --- Hercule Solution Space State Stack Interface --------------- */

t_hercule_solution_space_state_stack hercule_solution_space_state_stack_new( t_uint8 max_length,
                                                                             t_uint8 on_overflow ) HERCULE_ENGINE;

void hercule_solution_space_state_stack_destroy( t_hercule_solution_space_state_stack stack ) HERCULE_ENGINE;

void hercule_solution_space_state_stack_push( t_hercule_solution_space_state_stack stack,
                                              t_hercule_solution_space solution ) HERCULE_ENGINE;

void hercule_solution_space_state_stack_pop( t_hercule_solution_space_state_stack stack,
                                             t_hercule_solution_space solution ) HERCULE_ENGINE;

t_bool hercule_solution_space_state_stack_empty( t_hercule_solution_space_state_stack stack ) HERCULE_ENGINE;

/* --- Hercule Solution Space Interface --------------------------- */

t_hercule_solution_space  hercule_solution_space_new( t_hercule_puzzle puzzle ) HERCULE_ENGINE;
void                      hercule_solution_space_destroy( t_hercule_solution_space solution ) HERCULE_ENGINE;

t_uint32                  hercule_solution_space_assert( t_hercule_solution_space solution,
                                                         t_uint8 row,
                                                         t_uint8 col,
                                                         t_uint8 value,
                                                         t_bool exists,
                                                         t_bool auto_deduce ) HERCULE_ENGINE;

t_uint8                   hercule_solution_space_at( t_hercule_solution_space solution,
                                                     t_uint8 row,
                                                     t_uint8 col ) HERCULE_ENGINE;

void                      hercule_solution_space_put( t_hercule_solution_space solution,
                                                      t_uint8 row,
                                                      t_uint8 col,
                                                      t_uint8 value ) HERCULE_ENGINE;

void                      hercule_solution_space_push_state( t_hercule_solution_space solution ) HERCULE_ENGINE;
void                      hercule_solution_space_pop_state( t_hercule_solution_space solution ) HERCULE_ENGINE;

t_bool                    hercule_solution_space_solved( t_hercule_solution_space solution ) HERCULE_ENGINE;

t_bool                    hercule_solution_space_correct( t_hercule_solution_space solution ) HERCULE_ENGINE;

t_bool                    hercule_solution_space_solved_at( t_hercule_solution_space solution,
                                                            t_uint8 row,
                                                            t_uint8 col ) HERCULE_ENGINE;

t_uint32                  hercule_solution_space_apply( t_hercule_solution_space solution,
                                                        t_hercule_clue clue ) HERCULE_ENGINE;

t_bool                    hercule_solution_space_hint( t_hercule_solution_space solution,
                                                       t_hercule_hint* hint ) HERCULE_ENGINE;

#endif /* ___H__ */

