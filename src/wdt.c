// Copyright (c) 2024 Ziga Miklosic
// All Rights Reserved
// This software is under MIT licence (https://opensource.org/licenses/MIT)
////////////////////////////////////////////////////////////////////////////////
/**
*@file      wdt.c
*@brief     Watchdog
*@author    Ziga Miklosic
*@email     ziga.miklosic@gmail.com
*@date      02.07.2023
*@version   V1.2.0
*/
////////////////////////////////////////////////////////////////////////////////
/*!
* @addtogroup WATCHDOG
* @{ <!-- BEGIN GROUP -->
*/
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <stdlib.h>
#include <stdint.h>
#include <stdatomic.h>

#include "wdt.h"
#include "../../wdt_if.h"

#if ( 1== WDT_CFG_STATS_EN )
    #include <math.h>
    #include <string.h>
#endif

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////

#if ( WDT_CFG_STATS_EN && WDT_CFG_DEBUG_EN )

    /**
     *  Trace buffer size
     */
    #define WDT_TRACE_BUFFER_SIZE           ( 32 )

    /**
     *    Watchdog task statistics
    */
    typedef struct
    {
        /**
         *  Report time stats 
         */
        struct
        {
            uint32_t avg;   /**<Average report time */
            uint32_t sum;   /**<Average window sum */
            uint32_t min;   /**<Minimum report time */
            uint32_t max;   /**<Maximum report time */
        } time;

        uint32_t    num_of_reports;     /**<Total number of reports */
        uint32_t    num_of_samp;        /**<Number of samples */
    } wdt_stats_t;

#endif

/**
 *  Watchdog task
 */
typedef struct
{
	_Atomic uint32_t report_timestamp;   /**<Timestamp of last task report */
    _Atomic bool   	 enable;             /**<Task enable state */
} wdt_task_t;

/**
 *  Watchdog control 
 */
typedef struct 
{    
    #if ( WDT_CFG_STATS_EN && WDT_CFG_DEBUG_EN )
        wdt_stats_t     stats[eWDT_TASK_NUM_OF];        /**<Statistics of task reportation */
        wdt_task_opt_t  trace[WDT_TRACE_BUFFER_SIZE];   /**<Trace buffer of task reports */
    #endif

    wdt_task_t  task[eWDT_TASK_NUM_OF];     /**<Watchdog tasks */
    uint32_t    last_kick;                  /**<Previous timestamp of kicking the dog */
    bool        valid;                      /**<Everything is OK */
    bool        start;                      /**<Watchdog start flag */
} wdt_ctrl_t;

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////

/**
 *  Initialization flag
 */
static bool gb_is_init = false;

/**
 *  Watchdog control block
 */
static wdt_ctrl_t g_wdt_ctrl;

/**
 *  Pointer to configuration table
 */
static const wdt_cfg_t * gp_wdt_cfg_table = NULL;

////////////////////////////////////////////////////////////////////////////////
// Function prototypes
////////////////////////////////////////////////////////////////////////////////
static void wdt_kick_hndl           (void);
static void wdt_check_task_reports  (void);

#if ( 1 == WDT_CFG_STATS_EN )
    static void wdt_stats_init          (void);
    static void wdt_stats_calc            (const wdt_task_opt_t task, const uint32_t timestamp, const uint32_t timestamp_prev);
    static void wdt_stats_clear_counts  (void);
    static void wdt_stats_clear_timings (void);
    static void wdt_stats_count_hndl    (void);
    static void wdt_trace_buffer_put    (const wdt_task_opt_t task);
#endif

////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/**
*        Watchdog kicking handler
*
* @brief    Every "WDT_CFG_KICK_WINDOW_TIME_MS" period of time watchdog will
*           be kicked if global "g_wdt_ctrl.valid" flag is set.
*
*           "g_wdt_ctrl.valid" flag is set only when all protected task are
*           reported at least once within their specified timeout.
*
* @return        void
*/
////////////////////////////////////////////////////////////////////////////////
static void wdt_kick_hndl(void)
{
    // All WDT task reported in time
    if ( true == g_wdt_ctrl.valid )
    {
		#if ( 0 == WDT_CFG_KICK_PERIOD_TIME_MS )

		    // Kick failure has no recovery action available here; discard.
		    (void) wdt_if_kick();

		#else
		    // Get current timestamp
		    const uint32_t timestamp = wdt_if_get_systick();

		    // Its time to kick the dog
		    if ((uint32_t)( timestamp - g_wdt_ctrl.last_kick ) >= WDT_CFG_KICK_PERIOD_TIME_MS )
		    {
		        g_wdt_ctrl.last_kick = timestamp;

		        // Kick failure has no recovery action available here; discard.
		        (void) wdt_if_kick();
		    }
		#endif
        
    }

    #if ( WDT_CFG_STATS_EN && WDT_CFG_DEBUG_EN )
        // Number of reports count handler
        wdt_stats_count_hndl();
    #endif
}

////////////////////////////////////////////////////////////////////////////////
/**
*        Checker for task reports wihtin specified timeout time
*
* @return        void
*/
////////////////////////////////////////////////////////////////////////////////
static void wdt_check_task_reports(void)
{
	// Check each task
	for ( wdt_task_opt_t task_it = 0; task_it < eWDT_TASK_NUM_OF; task_it++ )
	{
		// Is task protection enabled
		if ( true == atomic_load_explicit( &g_wdt_ctrl.task[task_it].enable, memory_order_relaxed ))
		{
			// Get time from last report. Unsigned subtraction wraps modulo 2^32, so this
			// stays correct across systick rollover as long as the elapsed time never
			// exceeds ~49.7 days.
			const uint32_t time_pass = (uint32_t)( wdt_if_get_systick() - atomic_load_explicit( &g_wdt_ctrl.task[task_it].report_timestamp, memory_order_relaxed ));

			// Task not reported in specified time
			// Kill me...
			if ( time_pass > gp_wdt_cfg_table[task_it].timeout )
			{
				g_wdt_ctrl.valid = false;

				WDT_DBG_PRINT( "Task %s not reported in time!", gp_wdt_cfg_table[task_it].p_name );

				break;
			}
		}
	}
}

#if (  WDT_CFG_STATS_EN && WDT_CFG_DEBUG_EN  )

    ////////////////////////////////////////////////////////////////////////////////
    /**
    *        Initialize statistics vars
    *
    * @return        void
    */
    ////////////////////////////////////////////////////////////////////////////////
    static void wdt_stats_init(void)
    {
        // Clear trace buffer
        memset( &g_wdt_ctrl.trace, 0, sizeof( g_wdt_ctrl.trace ));

        // Set all timings & counts to zero
        wdt_stats_clear_counts();
        wdt_stats_clear_timings();
    }

    ////////////////////////////////////////////////////////////////////////////////
    /**
    *        Calculation of protected task stats
    *
    * @brief    For every task report statistics is being obtained for easier
    *           debugging.
    * 
    * @note     Beside AVG, MIN and MAX report time there is also report trace
    *           buffer for task report histroy check.
    *
    * @param[in]    task            - Protected task enumeration
    * @param[in]    timestamp       - Task report current timestamp
    * @param[in]    timestamp_prev  - Previous task report current timestamp
    * @return       void
    */
    ////////////////////////////////////////////////////////////////////////////////
    static void wdt_stats_calc(const wdt_task_opt_t task, const uint32_t timestamp, const uint32_t timestamp_prev)
    {
        const uint32_t timestamp_dlt = (uint32_t) ( timestamp - timestamp_prev );

        // Manage time
        g_wdt_ctrl.stats[task].num_of_samp++;
        g_wdt_ctrl.stats[task].time.sum += timestamp_dlt;
        g_wdt_ctrl.stats[task].time.avg = (uint32_t) ( g_wdt_ctrl.stats[task].time.sum / g_wdt_ctrl.stats[task].num_of_samp );
        g_wdt_ctrl.stats[task].time.min = fminl( g_wdt_ctrl.stats[task].time.min, timestamp_dlt );
        g_wdt_ctrl.stats[task].time.max = fmaxl( g_wdt_ctrl.stats[task].time.max, timestamp_dlt );

        // Manage counts
        g_wdt_ctrl.stats[task].num_of_reports++;
    }

    ////////////////////////////////////////////////////////////////////////////////
    /**
    *        Manage task report trace buffer
    *
    * @param[in]    task    - Protected task enumeration
    * @return       void
    */
    ////////////////////////////////////////////////////////////////////////////////
    static void wdt_trace_buffer_put(const wdt_task_opt_t task)
    {
        // More each element to higher index to make space for new
        for ( uint32_t i = 0U; i < ( WDT_TRACE_BUFFER_SIZE - 1); i++ )
        {
            g_wdt_ctrl.trace[i+1] = g_wdt_ctrl.trace[i];
        }

        // Put new to start
        g_wdt_ctrl.trace[0] = task;        
    }

    ////////////////////////////////////////////////////////////////////////////////
    /**
    *        Clear statistics task report counts
    *
    * @return        void
    */
    ////////////////////////////////////////////////////////////////////////////////
    static void wdt_stats_clear_counts(void)
    {
        for ( uint32_t task_it = 0U; task_it < eWDT_TASK_NUM_OF; task_it++ )
        {
            g_wdt_ctrl.stats[task_it].num_of_reports = 0;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    /**
    *        Clear statistic timings
    *
    * @return        void
    */
    ////////////////////////////////////////////////////////////////////////////////
    static void wdt_stats_clear_timings (void)
    {
        for ( uint32_t task_it = 0U; task_it < eWDT_TASK_NUM_OF; task_it++ )
        {
            g_wdt_ctrl.stats[task_it].time.avg = 0U;
            g_wdt_ctrl.stats[task_it].time.sum = 0U;
            g_wdt_ctrl.stats[task_it].time.max = 0U;
            g_wdt_ctrl.stats[task_it].time.min = 0xFFFFFFFFU;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    /**
    *        Handle statistics counts
    *
    * @brief    Calculation of number of reports within each task timeout window.
    *
    *             This information is important in order to check how many times task
    *             reports within specified timeout period.
    *
    * @return        void
    */
    ////////////////////////////////////////////////////////////////////////////////
    static void wdt_stats_count_hndl(void)
    {
        static uint32_t timestamp[eWDT_TASK_NUM_OF] = {0};

        for ( uint32_t task_it = 0U; task_it < eWDT_TASK_NUM_OF; task_it++ )
        {
            // Timeout window
            if ((uint32_t)( wdt_if_get_systick() - timestamp[task_it] ) >= gp_wdt_cfg_table[task_it].timeout )
            {
                timestamp[task_it] = wdt_if_get_systick();

                // Clear number of reports
                g_wdt_ctrl.stats[task_it].num_of_reports = 0;
            }
        }
    }

#endif

////////////////////////////////////////////////////////////////////////////////
/**
* @} <!-- END GROUP -->
*/
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/**
*@addtogroup WATCHDOG_API
* @{ <!-- BEGIN GROUP -->
*
* Following functions are part of API
*/
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/**
*        Watchdog initialization
*
* @note     Must be done only once.
*
* @return        status  - Status of initialization
*/
////////////////////////////////////////////////////////////////////////////////
wdt_status_t wdt_init(void)
{
    // Already init -> skip
    if ( wdt_is_init()) return eWDT_OK;

    // Get configuration table
    gp_wdt_cfg_table = wdt_cfg_get_table();
    WDT_ASSERT( NULL != gp_wdt_cfg_table )

    // Initialize WDT platform dependent stuff
    if ( eWDT_OK == wdt_if_init())
    {
        gb_is_init = true;
        g_wdt_ctrl.start = false;
        WDT_DBG_PRINT( "WDT init success!" );

        // Get current timestamp
        const uint32_t timestamp = wdt_if_get_systick();

        // Init watchdog tasks
        for ( uint32_t task_it = 0U; task_it < eWDT_TASK_NUM_OF; task_it++ )
        {
            atomic_store_explicit( &g_wdt_ctrl.task[task_it].enable, gp_wdt_cfg_table[task_it].enable, memory_order_relaxed );
            atomic_store_explicit( &g_wdt_ctrl.task[task_it].report_timestamp, timestamp, memory_order_relaxed );
        }

        // Init stats
        #if (  WDT_CFG_STATS_EN && WDT_CFG_DEBUG_EN  )
            wdt_stats_init();
        #endif

        return eWDT_OK;
    }
    else
    {
        WDT_DBG_PRINT( "WDT init error: Function wdt_if_init() failed..." );
        return eWDT_ERROR_INIT;        
    }
}

////////////////////////////////////////////////////////////////////////////////
/**
*        Get watchdog init flag
*
* @return       Initialization state
*/
////////////////////////////////////////////////////////////////////////////////
bool wdt_is_init(void)
{
    return gb_is_init;
}

////////////////////////////////////////////////////////////////////////////////
/**
*        Main watchdog handler
*
* @brief    It is recommended that this handler is called from high priority
*           task or ISR in order to get consistent WDT process period!
*
*           All protected task are checked if they report to watchdog module
*           within specified timeout time. 
*
* @note     This function shall be called at least 10x faster that hadrware
*           WDT timer window size in order to have a propper time resolution!
*           E.g. WDT window = 10ms, then this handler shall be called every 1ms
*
* @return        status - Status of operation
*/
////////////////////////////////////////////////////////////////////////////////
wdt_status_t wdt_hndl(void)
{
    WDT_ASSERT( true == wdt_is_init());
    if ( true != wdt_is_init()) return eWDT_ERROR_INIT;

    if ( true == g_wdt_ctrl.start )
    {
        // Check that alles is gut
        wdt_check_task_reports();

        // Handle WDT kicking
        wdt_kick_hndl();
    }

    return eWDT_OK;
}

////////////////////////////////////////////////////////////////////////////////
/**
*        Start watchdog
*
* @brief    This function starts watchdog and all protected task shall be ready 
*           to report within specified timeout time. 
*
*           Watchdog cannot be stopped when started.
*
* @return        status - Status of operation
*/
////////////////////////////////////////////////////////////////////////////////
wdt_status_t wdt_start(void)
{
    WDT_ASSERT( true == wdt_is_init());
    if ( true != wdt_is_init()) return eWDT_ERROR_INIT;

    // Get current timestamp
    const uint32_t timestamp = wdt_if_get_systick();

    // Init WDT controls
    g_wdt_ctrl.last_kick    = timestamp;
    g_wdt_ctrl.valid        = true;

    for ( uint32_t task_it = 0U; task_it < eWDT_TASK_NUM_OF; task_it++ )
    {
        atomic_store_explicit( &g_wdt_ctrl.task[task_it].report_timestamp, timestamp, memory_order_relaxed );
    }

    // Start WDT 
    if ( eWDT_OK == wdt_if_start())
    {
        g_wdt_ctrl.start = true;
        WDT_DBG_PRINT( "WDT has been started!" );
        return eWDT_OK;
    }
    else
    {
        WDT_DBG_PRINT( "WDT start error..." );
        return eWDT_ERROR;
    }
}

////////////////////////////////////////////////////////////////////////////////
/**
*        Task report
*
* @brief    Each protected task shall call this function at least once within
*           specified timeout time period. 
*
* @param[in]    task    - Protected task enumeration
* @return       status  - Status of operation
*/
////////////////////////////////////////////////////////////////////////////////
wdt_status_t wdt_task_report(const wdt_task_opt_t task)
{
    WDT_ASSERT( true == wdt_is_init());
    if ( true != wdt_is_init()) return eWDT_ERROR_INIT;

    WDT_ASSERT( task < eWDT_TASK_NUM_OF );
    if ( task >= eWDT_TASK_NUM_OF ) return eWDT_ERROR;

    // Get timestamp
    const uint32_t timestamp = wdt_if_get_systick();

// Perform statistics
#if ( WDT_CFG_STATS_EN && WDT_CFG_DEBUG_EN )
    // Get mutex
    if ( eWDT_OK == wdt_if_aquire_mutex())
    {
        // Calculate statistics
        wdt_stats_calc( task, timestamp, atomic_load_explicit( &g_wdt_ctrl.task[task].report_timestamp, memory_order_relaxed ));

        // Put to trace buffer
        wdt_trace_buffer_put( task );

        // Release failure has no recovery action available here; discard.
        (void) wdt_if_release_mutex();
    }
#endif

    // Store report timestamp
    atomic_store_explicit( &g_wdt_ctrl.task[task].report_timestamp, timestamp, memory_order_relaxed );

    return eWDT_OK;
}

////////////////////////////////////////////////////////////////////////////////
/**
*       Enable/Disable watchdog task
*
* @brief    In case protected task is not expected to execute periodically but rather
*           on rare occasions, then respective wdt task can be disabled and enabled
*           back on when task is expected to execute.
*
* @param[in]    task    - Protected task enumeration
* @param[in]    enable  - Enable or disable protected task
* @return       status  - Status of operation
*/
////////////////////////////////////////////////////////////////////////////////
wdt_status_t wdt_task_set_enable(const wdt_task_opt_t task, const bool enable)
{
    WDT_ASSERT( true == wdt_is_init());
    if ( true != wdt_is_init()) return eWDT_ERROR_INIT;

    WDT_ASSERT( task < eWDT_TASK_NUM_OF );
    if ( task >= eWDT_TASK_NUM_OF ) return eWDT_ERROR;

    // State changed?
    if ( enable != atomic_load_explicit( &g_wdt_ctrl.task[task].enable, memory_order_relaxed ))
    {
        atomic_store_explicit( &g_wdt_ctrl.task[task].report_timestamp, wdt_if_get_systick(), memory_order_relaxed );
        atomic_store_explicit( &g_wdt_ctrl.task[task].enable, enable, memory_order_relaxed );
    }

    return eWDT_OK;
}

////////////////////////////////////////////////////////////////////////////////
/**
*       Get watchdog task enable state
*
* @param[in]    task - Protected task enumeration
* @return       State of protected task enable
*/
////////////////////////////////////////////////////////////////////////////////
bool wdt_task_get_enable(const wdt_task_opt_t task)
{
    WDT_ASSERT( true == wdt_is_init());
    if ( true != wdt_is_init()) return false;

    WDT_ASSERT( task < eWDT_TASK_NUM_OF );
    if ( task >= eWDT_TASK_NUM_OF ) return false;

    return atomic_load_explicit( &g_wdt_ctrl.task[task].enable, memory_order_relaxed );
}

////////////////////////////////////////////////////////////////////////////////
/**
*        Early wakeup watchdog reset callback
*
* @brief    User shall define definition of that function. 
*           Common usage is to log error data into some NVM space.
*
* @note     Not all uC periphery support interrupt before watchdog kill...
*
* @return        status - Status of operation
*/
////////////////////////////////////////////////////////////////////////////////
__WDT_WEAK_FNC__ void wdt_pre_reset_isr_callback(void)
{
    // Leave empty for user definition...
}

////////////////////////////////////////////////////////////////////////////////
/**
* @} <!-- END GROUP -->
*/
////////////////////////////////////////////////////////////////////////////////
