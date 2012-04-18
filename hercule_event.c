#include <PalmOS.h>

#include "hercule.h"
#include "hercule_event.h"


typedef struct {
  UInt16 formId;
  Boolean (*handler)( EventPtr );
  MemHandle next;
} RegisteredForm;


static MemHandle g_registered_forms;

static Int32     g_timer_length;
static Int32     g_ticks_remaining;

static Boolean   static_hercule_handle_event( EventPtr event );


void HerculeEventInit( void ) /* {{{ */
{
  g_timer_length = evtWaitForever;
  g_ticks_remaining = evtWaitForever;
  g_registered_forms = NULL;
}
/* }}} */

void HerculeEventCleanup( void ) /* {{{ */
{
  MemHandle next;
  RegisteredForm* itemP;

  while( g_registered_forms != NULL ) {
    itemP = (RegisteredForm*)MemHandleLock( g_registered_forms );
    next = itemP->next;
    MemHandleUnlock( g_registered_forms );
    MemHandleFree( g_registered_forms );
    g_registered_forms = next;
  }
}
/* }}} */


void HerculeRegisterForm( UInt16 formId, Boolean (*handler)( EventPtr ) ) /* {{{ */
{
  MemHandle item;
  RegisteredForm* itemP;

  item = MemHandleNew( sizeof( RegisteredForm ) );
  itemP = (RegisteredForm*)MemHandleLock( item );

  itemP->formId  = formId;
  itemP->handler = handler;
  itemP->next = g_registered_forms;

  MemHandleUnlock( item );
  g_registered_forms = item;
}
/* }}} */

void HerculeEventLoop( void ) /* {{{ */
{
  EventType event;
  EventType timer_event;
  UInt32    last_tick;

  do {
    last_tick = TimGetTicks();
    EvtGetEvent( &event, g_ticks_remaining );

    if( g_timer_length != evtWaitForever ) {
      g_ticks_remaining -= ( TimGetTicks() - last_tick );
      if( g_ticks_remaining <= 0 ) {
        if( event.eType == nilEvent ) {
          event.eType = HerculeEvent_Timer;
        } else {
          MemSet( &timer_event, sizeof( timer_event ), 0 );
          timer_event.eType = HerculeEvent_Timer;
          EvtAddEventToQueue( &timer_event );
        }
        g_ticks_remaining = g_timer_length;
      }
    }

    HerculeHandleEvent( &event );
  } while( event.eType != appStopEvent );
}
/* }}} */

Boolean HerculeHandleEvent( EventPtr event ) /* {{{ */
{
  UInt16 error;

  if( !SysHandleEvent( event ) )
    if( !MenuHandleEvent( 0, event, &error ) )
      if( !static_hercule_handle_event( event ) )
        return FrmDispatchEvent( event );

  return true;
}
/* }}} */


void HerculeStartTimer( Int32 ticks ) /* {{{ */
{
  g_timer_length = g_ticks_remaining = ticks;
}
/* }}} */

void HerculeStopTimer( void ) /* {{{ */
{
  g_timer_length = g_ticks_remaining = evtWaitForever;
}
/* }}} */



static Boolean static_hercule_handle_event( EventPtr event ) /* {{{ */
{
  UInt16          formId;
  FormPtr         frmP;
  MemHandle       item;
  MemHandle       next;
  RegisteredForm* itemP;

  if( event->eType == frmLoadEvent ) {
    formId = event->data.frmLoad.formID;
    frmP = FrmInitForm( formId );
    FrmSetActiveForm( frmP );

    item = g_registered_forms;
    while( item != NULL ) {
      itemP = (RegisteredForm*)MemHandleLock( item );
      if( itemP->formId == formId ) {
        FrmSetEventHandler( frmP, itemP->handler );
        MemHandleUnlock( item );
        break;
      }
      next = itemP->next;
      MemHandleUnlock( item );
      item = next;
    }

    if( item == NULL ) {
      ErrNonFatalDisplay( "Invalid Form Load Event" );
    }

    return true;
  }

  return false;
}
/* }}} */

