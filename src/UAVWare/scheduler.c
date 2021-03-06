#include <stdlib.h>
#include "scheduler.h"
// #include "stm32f4xx.h"
#include "../core/main.h"


// The array of tasks
// Check array size in scheduler header file
static sTask_t sch_tasks_g[SCH_MAX_TASKS];

// Tick count
static volatile uint32_t tick_count_g = 0;
static volatile bool scheduler_overrun_flag = false;

/*----------------------------------------------------------------------------*-

  sch_tick_handler()

  This is the scheduler ISR callback.  It is called at a rate
  determined by the timer settings in the UVOS_TIME_init() function
  in uvos_time file.

  PARAMETERS:
     None.

  LONG-TERM DATA:
     tick_count_g (W)

  MCU HARDWARE:
     TIMEBASE_TIM timer.

  PRE-CONDITION CHECKS:
     None.

  POST-CONDITION CHECKS:
     None.

  ERROR DETECTION / ERROR HANDLING:
     Checks tick_count_g value => 'Fail Safe'

  RETURN VALUE:
     None.

-*----------------------------------------------------------------------------*/
void sch_tick_handler( void )
{

  // LL_GPIO_SetOutputPin( testPin1_GPIO_Port, testPin1_Pin );

  // Increment tick count
  tick_count_g++;

  debug_putc( tick_count_g );

  // check against limit
  if ( tick_count_g > SCH_TICK_COUNT_LIMIT ) {
    // One or more tasks has taken too long to complete
    scheduler_overrun_flag = true;
  }

  // LL_GPIO_ResetOutputPin( testPin1_GPIO_Port, testPin1_Pin );

}

/*----------------------------------------------------------------------------*/
void sch_init_hz( const uint32_t TICKhz )
{
  for ( uint32_t Task_id = 0; Task_id < SCH_MAX_TASKS; Task_id++ ) {
    sch_tasks_g[Task_id].pTask = SCH_NULL_PTR; // Set pTask to "null pointer"
  }
  UVOS_TIME_RegisterTickCallback( sch_tick_handler );
  UVOS_TIME_sched_init( TICKhz );
}

/*----------------------------------------------------------------------------*/
void sch_start( void )
{
  tick_count_g = 0;
  scheduler_overrun_flag = false;
  UVOS_TIME_sched_start();
}

/*----------------------------------------------------------------------------*/
void sch_stop( void )
{
  UVOS_TIME_sched_stop();
}

/*----------------------------------------------------------------------------*-

  sch_dispatch_tasks()

  This is the 'dispatcher' function.  When a task (function)
  is due to run, SCH_Dispatch_Tasks() will run it.
  This function must be called (repeatedly) from the main loop.

  Triggers move to "idle" mode when all tasks have been released.

  PARAMETERS:
     None.

  LONG-TERM DATA:
     sch_tasks_g (W)
     tick_count_g (W)

  MCU HARDWARE:
     Triggers move to idle mode.

  PRE-CONDITION CHECKS:
     None.

  POST-CONDITION CHECKS:
     None.

  ERROR DETECTION / ERROR HANDLING:
     None.

  RETURN VALUE:
     scheduler_overrun_flag.

-*----------------------------------------------------------------------------*/
int sch_dispatch_tasks( void )
{


  __disable_irq();
  uint32_t update_required = ( tick_count_g > 0 ); // Check tick count
  __enable_irq();

  while ( update_required ) {

    // LL_GPIO_SetOutputPin( testPin2_GPIO_Port, testPin2_Pin );

    // Go through the task array
    for ( uint32_t Task_id = 0; Task_id < SCH_MAX_TASKS; Task_id++ ) {
      // Check if there is a task at this location
      if ( sch_tasks_g[Task_id].pTask != SCH_NULL_PTR ) {
        if ( --sch_tasks_g[Task_id].Delay == 0 ) {
          ( *sch_tasks_g[Task_id].pTask )(); // Run the task

          // All tasks are periodic: schedule task to run again
          sch_tasks_g[Task_id].Delay = sch_tasks_g[Task_id].Period;
        }
      }
    }

    __disable_irq();
    tick_count_g--;                       // Decrement the count
    update_required = ( tick_count_g > 0 ); // Check again
    __enable_irq();

    // LL_GPIO_ResetOutputPin( testPin2_GPIO_Port, testPin2_Pin );

  }

  // The scheduler enters idle mode at this point
  // __WFI();

  return scheduler_overrun_flag;

}

/*----------------------------------------------------------------------------*-

  sch_add_task()

  Adds a task (function) to the schedule.
  The task will be released periodically by the scheduler.

  PARAMETERS:
     pTask  : The name of the task (function) to be scheduled.
              NOTE: All scheduled functions must be 'void, void' -
              that is, they must take no parameters, and have
              a void return type (in this design).

     DELAY  : The interval (ticks) before the task is first executed.

     PERIOD : Task period (in ticks).  Must be > 0.

  LONG-TERM DATA:
     sch_tasks_g (W)

  MCU HARDWARE:
     None.

  PRE-CONDITION CHECKS:
     1. There is space in the task array.
     2. The task is periodic ('one-shot' tasks are not supported.

  POST-CONDITION CHECKS:
     None.

  ERROR DETECTION / ERROR HANDLING:
     PROCESSOR_Perform_Safe_Shutdown() is called:
     - if the task cannot be added to the schedule (array too small)
     - if an attempt is made to schedule a "one shot" task

  RETURN VALUE:
     Success or failure.

-*----------------------------------------------------------------------------*/
int sch_add_task( void ( * pTask )(),
                  const uint32_t DELAY,
                  const uint32_t PERIOD )
{
  uint32_t Task_id = 0;

  // First find a gap in the array (if there is one)
  while ( ( sch_tasks_g[Task_id].pTask != SCH_NULL_PTR )
          && ( Task_id < SCH_MAX_TASKS ) ) {
    Task_id++;
  }

  // Have we reached the end of the list?
  if ( Task_id == SCH_MAX_TASKS ) {
    // Task list is full - fatal error
    return EXIT_FAILURE;
  }

  // Check for "one shot" tasks
  if ( PERIOD == 0 ) {
    // We do not allow "one shot" tasks (all tasks must be periodic)
    return EXIT_FAILURE;
  }

  // If we're here, there is a space in the task array
  // and the task to be added is periodic
  sch_tasks_g[Task_id].pTask  = pTask;

  sch_tasks_g[Task_id].Delay  = DELAY + 1;
  sch_tasks_g[Task_id].Period = PERIOD;

  return EXIT_SUCCESS;
}