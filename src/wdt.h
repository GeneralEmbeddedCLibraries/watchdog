// Copyright (c) 2024 Ziga Miklosic
// All Rights Reserved
// This software is under MIT licence (https://opensource.org/licenses/MIT)
////////////////////////////////////////////////////////////////////////////////
/**
*@file      wdt.h
*@brief     Watchdog API
*@author    Ziga Miklosic
*@email     ziga.miklosic@gmail.com
*@date      02.07.2023
*@version   V1.2.0
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
#define WDT_VER_MINOR			( 2 )
#define WDT_VER_DEVELOP			( 0 )

/**
 * 	Watchdog status
 */
typedef enum
{
	eWDT_OK				= 0x00U,		/**<Normal operation */
	eWDT_ERROR			= 0x01U,		/**<General error */
	eWDT_ERROR_INIT		= 0x02U,		/**<Initialization error  */
	eWDT_ERROR_CFG		= 0x04U,		/**<Settings error */
} wdt_status_t;

////////////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////////////
wdt_status_t 	wdt_init					(void);
wdt_status_t 	wdt_is_init					(bool * const p_is_init);
wdt_status_t 	wdt_hndl					(void);
wdt_status_t 	wdt_start					(void);
wdt_status_t 	wdt_task_report				(const wdt_task_opt_t task);
wdt_status_t    wdt_task_set_enable         (const wdt_task_opt_t task, const bool enable);
wdt_status_t    wdt_task_get_enable         (const wdt_task_opt_t task, bool * const p_enable);
void			wdt_pre_reset_isr_callback	(void);		

#endif // __WDT_H

////////////////////////////////////////////////////////////////////////////////
/**
* @} <!-- END GROUP -->
*/
////////////////////////////////////////////////////////////////////////////////
