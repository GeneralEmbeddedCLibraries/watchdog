# Watchdog
Watchdog module is implemented in C language for usage in embedded system in order to ensure system stability and foult-tolerance in single or multi-task environments. 

By incorporating the Watchdog module into multi-task applications, developers can allocate watchdog to individual tasks, ensuring that each task continues to execute within its defined timeframe. This approach prevents any single task from monopolizing system resources or stalling the entire system due to unexpected issues.

Complete Watchdog module is written to be higly portable, flexible and microcontroller agnostics. This is achieved via configuration and interface file pairs: **wdt_cfg.c/.h** and **wdt_if.c/.h**. Inside that files end-target application specifics are defined.

## Watchdog Statistics
For easier detection of missing watchdog reports from tasks, debugging statistics are supported. Statistics are by default disabled and must be enabled in configuration file. In order to enable watchdog statistics following macros must be addressed inside *wdt_cfg.h* file:

```C
/**
 * 	Enable/Disable statistics
 *
 * @note	WDT_CFG_DEBUG_EN macro must be enabled in order
 * 			to support statistics.
 */
#define WDT_CFG_STATS_EN						( 1 )

/**
 * 	Enable/Disable debug mode
 */
#define WDT_CFG_DEBUG_EN						( 1 )
```

After enabling statistics observe following variables:
```C
// General watchdog stats by each task (number of reports, min, max, avg timings)
g_wdt_ctrl.stats

// Trace buffer of pass 32 watchdog reports 
g_wdt_ctrl.trace
```

## Dependencies

### **1. Low Level Watchdog Driver**
Low level watchdog handling must be provided by user via module interface **wdt_if.c/.h**

## **General Embedded C Libraries Ecosystem**
In order to be part of *General Embedded C Libraries Ecosystem* this module must be placed in following path: 

```
root/middleware/watchdog/watchdog/"module_space"
```

 ## API
| API Functions | Description | Prototype |
| --- | ----------- | ----- |
| **wdt_init**                      | Initialization of watchdog            | wdt_status_t wdt_init(void) |
| **wdt_is_init**                   | Get initialization flag               | wdt_status_t 	wdt_is_init(bool * const p_is_init) |
| **wdt_hndl**                      | Main watchdog handler                 | wdt_status_t wdt_hndl(void) |
| **wdt_start**                     | Start watchdog                        | wdt_status_t wdt_start(void) |
| **wdt_task_report**               | Report to watchdog                    | wdt_status_t wdt_task_report(const wdt_task_t task) |
| **wdt_task_set_enable**           | Enable/Disable task from protection   | wdt_status_t wdt_task_set_enable(const wdt_task_opt_t task, const bool enable) |
| **wdt_task_get_enable**           | Get task protection enable state      | wdt_status_t wdt_task_get_enable(const wdt_task_opt_t task, bool * const p_enable) |
| **wdt_pre_reset_isr_callback**    | Watchdog pre-reset callback           | void wdt_pre_reset_isr_callback(void) |
	
## How to use
1. List all protected task inside **wdt_cfg.h** file:
```C
/**
 * 		Watchdog protected task list
 * 
 * @brief	User shall list all protected task here! 
 */
typedef enum
{
	// USER CODE START...

	eWDT_TASK_COM = 0,      /**<Communication task */
	eWDT_TASK_HMI,          /**<Human Machine Interface task */
	eWDT_TASK_COM_NSU,      /**<Nordic serial upgrade task */
	eWDT_TASK_COM_ESF,      /**<Espresive serial upgrade task */

	// USER CODE END...

	eWDT_TASK_NUM_OF
} wdt_task_opt_t;
```

2. Set up configuration table inside **wdt_cfg.c** file:
```C
/**
 *      Watchdog protection task definitions
 * 
 */
static const wdt_cfg_t g_wdt_cfg_table[eWDT_TASK_NUM_OF] = 
{
    // USER CODE START...
    
    // --------------------------------------------------------------------------------------------------
    //	                        Task name               Report timeout [ms]	    Enable default
    // --------------------------------------------------------------------------------------------------
    
    [eWDT_TASK_COM]         = { .p_name = "COM",        .timeout = 1000UL,      .enable = true              },
    [eWDT_TASK_HMI]         = { .p_name = "HMI",        .timeout = 5000UL,      .enable = true              },
    [eWDT_TASK_COM_NSU]     = { .p_name = "COM_NSU",    .timeout = 15000UL,     .enable = false             },
    [eWDT_TASK_COM_ESF]     = { .p_name = "COM_ESF",    .timeout = 5000UL,      .enable = false             },
    
    // NOTE: HC task has highest priority, thus handling watchdog and dedicated task is not needed!
    
    // USER CODE END...
};

```

3. Initialize & start:
```C
// Init watchdog
if ( eWDT_OK != wdt_init())
{
    // Init failed...
    // Further actions here...
}
else
{
    // Start WDT
    wdt_start();
}
```

After succsessfull initialization and start watchdog needs to be process and feed:

### Process watchdog
Recommended to call as fast as possible in order to have high time resolution.
```C
// High priority ISR or OS task
@1ms period
{
    wdt_hndl();
}
```

### Feed/Report watchdog
It is recommended that task report to watchdog is peformed ten times faster (if possible) than specified timeout in configuration table in order not to miss report timeout period.
```C
// Protected task: task_1
@10ms period
{
    wdt_task_report( eWDT_TASK_1 );
}

// Protected task: task_2
@1000 ms period
{
    wdt_task_report( eWDT_TASK_2 );
}

// Protected task: task_3
@50 ms period
{
    wdt_task_report( eWDT_TASK_3 );
}

```

### Pre-reset ISR callback
User must enable WDT pre-reset interrupt and put call of "wdt_pre_reset_isr_callback()" into defined ISR handler! Look at the **wdt_if.c** for example.
After that pre-reset ISR callback can be used. E.g.:
```C
// Watchdog pre-reset callback
void wdt_pre_reset_isr_callback(void)
{
    // Something went wrong...
    
    // Log error here
    nvm_write( eNVM_REGION_EEPROM, ERR_LOG_ADDR, WDT_KILL_ERR );
}
```
