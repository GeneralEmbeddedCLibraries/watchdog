// Copyright (c) 2021 Ziga Miklosic
// All Rights Reserved
// This software is under MIT licence (https://opensource.org/licenses/MIT)
////////////////////////////////////////////////////////////////////////////////
/**
*@file      wdt.h
*@brief     Watchdog API
*@author    Ziga Miklosic
*@date      02.09.2021
*@version   V1.0.0
*/
////////////////////////////////////////////////////////////////////////////////
/**
*@addtogroup WATCHDOG_API
* @{ <!-- BEGIN GROUP -->
*/
////////////////////////////////////////////////////////////////////////////////
#ifndef __WDT_H
#define __WDT_H

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <stdint.h>
#include <stdbool.h>
#include "../../wdt_cfg.h"

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////

/**
 * 	Module version
 */
#define WDT_VER_MAJOR			( 1 )
#define WDT_VER_MINOR			( 0 )
#define WDT_VER_DEVELOP			( 0 )

/**
 * 	Watchdog status
 */
typedef enum
{
	eWDT_OK				= 0,		/**<Normal operation */
	eWDT_ERROR			= 0x01,		/**<General error */
	eWDT_ERROR_INIT		= 0x02,		/**<Initialization error  */
	eWDT_ERROR_CFG		= 0x04,		/**<Settings error */
} wdt_status_t;

/**
 * 	Watchdog configuration table
 */
typedef struct
{
	const char *	p_name;		/**<Name of protected task */
	uint32_t		timeout;	/**<Timeout time in ms */
	bool			enable;		/**<Protection enable/disable */
} wdt_cfg_t;

////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////
wdt_status_t 	wdt_init					(void);
wdt_status_t 	wdt_deinit					(void);
wdt_status_t 	wdt_is_init					(bool * const p_is_init);
wdt_status_t 	wdt_hndl					(void);
wdt_status_t 	wdt_start					(void);
wdt_status_t 	wdt_task_report				(const wdt_task_t task);
void			wdt_pre_reset_isr_callback	(void);		

#endif // __WDT_H

////////////////////////////////////////////////////////////////////////////////
/**
* @} <!-- END GROUP -->
*/
////////////////////////////////////////////////////////////////////////////////