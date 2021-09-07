// Copyright (c) 2021 Ziga Miklosic
// All Rights Reserved
// This software is under MIT licence (https://opensource.org/licenses/MIT)
////////////////////////////////////////////////////////////////////////////////
/**
*@file      wdt.c
*@brief     Watchdog
*@author    Ziga Miklosic
*@date      02.09.2021
*@version   V1.0.0
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
	 *	Watchdog task statistics
	*/
	typedef struct
	{
        /**
         *  Report time stats 
         */
		struct
		{
			uint32_t avg;			   	/**<Average report time */
			uint32_t sum;			    /**<Average window sum */
			uint32_t min;			    /**<Minimum report time */
			uint32_t max;			    /**<Maximum report time */
		} time;

		uint32_t	num_of_reports;		/**<Total number of reports */
		uint32_t    num_of_samp;	    /**<Number of samples */   
	} wdt_stats_t;

#endif

/**
 *  Watchdog control 
 */
typedef struct 
{    
    #if ( WDT_CFG_STATS_EN && WDT_CFG_DEBUG_EN )
        wdt_stats_t stats[eWDT_TASK_NUM_OF];        /**<Statistics of task reportation */
        wdt_task_t  trace[WDT_TRACE_BUFFER_SIZE];   /**<Trace buffer of task reports */   
    #endif

    uint32_t    report_timestamp[eWDT_TASK_NUM_OF]; /**<Timestamp of last task report */
    uint32_t    last_kick;                          /**<Previous timestamp of kicking the dog */
    bool        valid;                              /**<Everything is OK */
    bool        start;                              /**<Watchdog start flag */
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
    static void wdt_stats_calc			(const wdt_task_t task, const uint32_t timestamp, const uint32_t timestamp_prev);
    static void wdt_stats_clear_counts  (void);
    static void wdt_stats_clear_timings (void);
    static void wdt_stats_count_hndl	(void);
    static void wdt_trace_buffer_put    (const wdt_task_t task);
#endif

////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/**
*		Watchdog kicking handler
*
* @brief    Every /ref WDT_CFG_KICK_WINDOW_TIME_MS period of time watchdog will
*           be kicked if global /ref g_wdt_ctrl.valid flag is set.
*
*           /ref g_wdt_ctrl.valid flag is set only when all protecte task are
*           reported at least once within their specified timeout.
*
* @return		void
*/
////////////////////////////////////////////////////////////////////////////////
static void wdt_kick_hndl(void)
{
    uint32_t timestamp = 0UL;

    // Get current timestamp
    timestamp = wdt_if_get_systick();

    // Its time to kick the dog
    if ((uint32_t)( timestamp - g_wdt_ctrl.last_kick ) >= WDT_CFG_KICK_WINDOW_TIME_MS )
    {
        g_wdt_ctrl.last_kick = timestamp;

        // Alles gut
        if ( true == g_wdt_ctrl.valid )
        {
            wdt_if_kick();
        }
    }

    #if ( WDT_CFG_STATS_EN && WDT_CFG_DEBUG_EN )

    	// Number of reports count handler
    	wdt_stats_count_hndl();

    #endif
}

////////////////////////////////////////////////////////////////////////////////
/**
*		Checker for task reports wihtin specified timeout time
*
* @return		void
*/
////////////////////////////////////////////////////////////////////////////////
static void wdt_check_task_reports(void)
{
    uint32_t task      = 0UL;
    uint32_t time_pass  = 0UL;

    // Check each task
    for ( task = 0; task < eWDT_TASK_NUM_OF; task++ )
    {
        // Is task protection enabled
        if ( true == gp_wdt_cfg_table[task].enable )
        {
            // Get time from last report
            time_pass = (uint32_t)((uint32_t) wdt_if_get_systick() - g_wdt_ctrl.report_timestamp[task] );

            // Task not reported in specified time
            // Kill me...
            if ( time_pass > gp_wdt_cfg_table[task].timeout )
            {
                g_wdt_ctrl.valid = false;
                break;
            }
        }
    }
}

#if (  WDT_CFG_STATS_EN && WDT_CFG_DEBUG_EN  )

    ////////////////////////////////////////////////////////////////////////////////
    /**
    *		Initialize statistics vars
    *
    * @return		void
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
    *		Calculation of protected task stats
    *
    * @brief    For every task report statistics is being obtained for easier
    *           debugging.
    * 
    * @note     Beside AVG, MIN and MAX report time there is also report trace
    *           buffer for task report histroy check.
    *
    * @param[in]    task        	- Protected task enumeration
    * @param[in]    timestamp   	- Task report current timestamp
    * @param[in]    timestamp_prev  - Previous task report current timestamp
    * @return		void
    */
    ////////////////////////////////////////////////////////////////////////////////
    static void wdt_stats_calc(const wdt_task_t task, const uint32_t timestamp, const uint32_t timestamp_prev)
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
    *		Manage task report trace buffer
    *
    * @param[in]    task    - Protected task enumeration
    * @return		void
    */
    ////////////////////////////////////////////////////////////////////////////////
    static void wdt_trace_buffer_put(const wdt_task_t task)
    {
        // Make space for newcommers
       // memcpy( &g_wdt_ctrl.trace[1], &g_wdt_ctrl.trace[0], ( WDT_TRACE_BUFFER_SIZE - 1 ));

        uint32_t i = 0;

        // More each element to higher index to make space for new
        for ( i = 0; i < ( WDT_TRACE_BUFFER_SIZE - 1); i++ )
        {
            g_wdt_ctrl.trace[i+1] = g_wdt_ctrl.trace[i];
        }

        // Put new to start
        g_wdt_ctrl.trace[0] = task;        
    }

    ////////////////////////////////////////////////////////////////////////////////
    /**
    *		Clear statistics task report counts
    *
    * @return		void
    */
    ////////////////////////////////////////////////////////////////////////////////
    static void wdt_stats_clear_counts(void)
    {
        uint32_t task_num = 0;

        for ( task_num = 0; task_num < eWDT_TASK_NUM_OF; task_num++ )
        {
            g_wdt_ctrl.stats[task_num].num_of_reports = 0;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    /**
    *		Clear statistic timings
    *
    * @return		void
    */
    ////////////////////////////////////////////////////////////////////////////////
    static void wdt_stats_clear_timings (void)
    {
        uint32_t task_num = 0;

        for ( task_num = 0; task_num < eWDT_TASK_NUM_OF; task_num++ )
        {
            g_wdt_ctrl.stats[task_num].time.avg = 0;
            g_wdt_ctrl.stats[task_num].time.sum = 0;
            g_wdt_ctrl.stats[task_num].time.max = 0;
            g_wdt_ctrl.stats[task_num].time.min = 0xFFFFFFFF;
        }
    }

    ////////////////////////////////////////////////////////////////////////////////
    /**
    *		Handle statistics counts
    *
    * @brief	Calculation of number of reports within each task timeout window.
    *
    * 			This information is important in order to check how many times task
    * 			reports within specified timeout period.
    *
    * @return		void
    */
    ////////////////////////////////////////////////////////////////////////////////
    static void wdt_stats_count_hndl(void)
    {
    			uint32_t task_num = 0;
        static 	uint32_t timestamp[eWDT_TASK_NUM_OF] = {0};

        for ( task_num = 0; task_num < eWDT_TASK_NUM_OF; task_num++ )
        {
        	// Timeout window
            if ((uint32_t)( wdt_if_get_systick() - timestamp[task_num] ) >= gp_wdt_cfg_table[task_num].timeout )
            {
            	timestamp[task_num] = wdt_if_get_systick();

            	// Clear number of reports
            	g_wdt_ctrl.stats[task_num].num_of_reports = 0;
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
*		Watchdog initialization
*
* @note     Must be done only once.
*
* @return		status  - Status of initialization
*/
////////////////////////////////////////////////////////////////////////////////
wdt_status_t wdt_init(void)
{
    wdt_status_t status = eWDT_OK;

    WDT_ASSERT( false == gb_is_init );

    if ( false == gb_is_init )
    {
        // Get configuration table
        gp_wdt_cfg_table = wdt_cfg_get_table();

        if ( NULL != gp_wdt_cfg_table )
        {
            // Initialize WDT platform dependent stuff
            status = wdt_if_init();

            if ( eWDT_OK == status )
            {
                gb_is_init = true;
                g_wdt_ctrl.start = false;
                WDT_DBG_PRINT( "WDT init success!" );
            }
            else
            {
                status = eWDT_ERROR_INIT;
                WDT_DBG_PRINT( "WDT init error: Function wdt_if_init() failed..." );
            }
        }
        else
        {
            status = eWDT_ERROR_INIT;
            WDT_DBG_PRINT( "WDT init error: Configuration table missing..." );
        }
    }
    else
    {
        status = eWDT_ERROR_INIT;
        WDT_DBG_PRINT( "WDT init error: Module already initialized..." );
    }

    // Init stats
    #if (  WDT_CFG_STATS_EN && WDT_CFG_DEBUG_EN  )
        wdt_stats_init();
    #endif

    return status;
}

////////////////////////////////////////////////////////////////////////////////
/**
*		Watchdog de-initialization
*
* @note     Can be performed only after sucseeded initialization
*
* @return		status - Status of de-initialization
*/
////////////////////////////////////////////////////////////////////////////////
wdt_status_t wdt_deinit(void)
{
    wdt_status_t status = eWDT_OK;

    WDT_ASSERT( true == gb_is_init );

    if ( true == gb_is_init )
    {
        status = wdt_if_deinit();

        if ( eWDT_OK == status )
        {
            gb_is_init = false;
            g_wdt_ctrl.start = false;
            WDT_DBG_PRINT( "WDT de-init success!" );
        }
        else
        {
            status = eWDT_ERROR_INIT;
            WDT_DBG_PRINT( "WDT init error: Function wdt_if_deinit() failed..." );
        }
    }
    else
    {
        status = eWDT_ERROR_INIT;
    }

    return status;
}

////////////////////////////////////////////////////////////////////////////////
/**
*		Get watchdog init flag
*
* @param[in]    p_is_init   - Pointer to init flag
* @return		status      - Status of operation
*/
////////////////////////////////////////////////////////////////////////////////
wdt_status_t wdt_is_init(bool * const p_is_init)
{
    wdt_status_t status = eWDT_OK;

    if ( NULL != p_is_init)
    {
        *p_is_init = gb_is_init;
    }
    else
    {
        status = eWDT_ERROR;
    }

    return status;
}

////////////////////////////////////////////////////////////////////////////////
/**
*		Main watchdog handler
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
* @return		status - Status of operation
*/
////////////////////////////////////////////////////////////////////////////////
wdt_status_t wdt_hndl(void)
{
    wdt_status_t status = eWDT_OK;

    WDT_ASSERT( true == gb_is_init );

    if ( true == gb_is_init )
    {
        if ( true == g_wdt_ctrl.start )
        {
            // Check that alles is gut
            wdt_check_task_reports();

            // Handle WDT kicking
            wdt_kick_hndl();
        }
        else
        {
            status = eWDT_ERROR;
        }
    }
    else
    {
        status = eWDT_ERROR_INIT;
    }

    return status;
}

////////////////////////////////////////////////////////////////////////////////
/**
*		Start watchdog
*
* @brief    This function starts watchdog and all protected task shall be ready 
*           to report within specified timeout time. 
*
*           Watchdog cannot be stopped when started.
*
* @return		status - Status of operation
*/
////////////////////////////////////////////////////////////////////////////////
wdt_status_t wdt_start(void)
{
    wdt_status_t    status      = eWDT_OK;
    uint32_t        timestamp   = 0UL;
    uint32_t        tasks       = 0UL;

    WDT_ASSERT( true == gb_is_init );

    if ( true == gb_is_init )
    {   
        // Get current timestamp
        timestamp = wdt_if_get_systick();

        // Init WDT controls
        g_wdt_ctrl.last_kick = timestamp;
        g_wdt_ctrl.valid = true;

        for ( tasks = 0; tasks < eWDT_TASK_NUM_OF; tasks++ )
        {
            g_wdt_ctrl.report_timestamp[tasks] = timestamp;
        }

        // Start WDT 
        status = wdt_if_start();

        if ( eWDT_OK == status )
        {
            g_wdt_ctrl.start = true;
            WDT_DBG_PRINT( "WDT has been started!" );
        }
        else
        {
            WDT_DBG_PRINT( "WDT start error..." );
        }
    }
    else
    {
        status = eWDT_ERROR_INIT;
    }

    return status;
}

////////////////////////////////////////////////////////////////////////////////
/**
*		Task report 
*
* @brief    Each protected task shall call this function at least once within
*           specified timeout time period. 
*
* @param[in]    task    - Protected task enumeration
* @return		status  - Status of operation
*/
////////////////////////////////////////////////////////////////////////////////
wdt_status_t wdt_task_report(const wdt_task_t task)
{
    wdt_status_t    status      = eWDT_OK;
    uint32_t        timestamp   = 0UL;

    WDT_ASSERT( true == gb_is_init );
    WDT_ASSERT( task < eWDT_TASK_NUM_OF );

    if ( true == gb_is_init )
    {
        if ( task < eWDT_TASK_NUM_OF )
        {
            // Get mutex
            if ( eWDT_OK == wdt_if_aquire_mutex())
            {
                // Get timestamp
                timestamp = wdt_if_get_systick();

                // Perform statistics
                #if ( WDT_CFG_STATS_EN && WDT_CFG_DEBUG_EN )

                    // Calculate statistics
                    wdt_stats_calc( task, timestamp, g_wdt_ctrl.report_timestamp[task] );

                    // Put to trace buffer
                    wdt_trace_buffer_put( task );

                #endif

				// Store report timestamp
				g_wdt_ctrl.report_timestamp[task] = timestamp;

                // Release mutex
                wdt_if_release_mutex();
            }
            else
            {
                status = eWDT_ERROR;
            }
        }
        else
        {
            status = eWDT_ERROR;
        }
    }
    else
    {
        status = eWDT_ERROR_INIT;
    }

    return status;
}

////////////////////////////////////////////////////////////////////////////////
/**
*		Early wakeup watchdog reset callback
*
* @brief    User shall define definition of that function. 
*           Common usage is to log error data into some NVM space.
*
* @note     Not all uC periphery support interrupt before watchdog kill...
*
* @return		status - Status of operation
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
