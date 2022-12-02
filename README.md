# Watchdog
Watchdog module implmentation in C for general embedded platform usage. It support RTOS multi-task as well as simple single loop protection. 

Complete module is configurable via configuration and interface file pairs: **wdt_cfg.c/.h** and **wdt_if.c/.h**. For easier debugging statistics are supported in order to tell timings of each protected tasks. Statistics are by default disabled and must be enabled in configuration file. Afterwards can be put into "LiveWatch" inside prefered IDE in order to get stats of each protection task.

## Dependencies
---
Watchdog module is highly dependedt from used embedded platform, therefore user must fill low level WDT handling inside **wdt_if.c/.h**.


## **General Embedded C Libraries Ecosystem**
In order to be part of *General Embedded C Libraries Ecosystem* this module must be placed in following path: 

```
root/middleware/watchdog/watchdog/"module_space"
```

 ## API
---

| API Functions | Description | Prototype |
| --- | ----------- | ----- |
| **wdt_init** | Initialization of watchdog | wdt_status_t wdt_init(void) |
| **wdt_is_init** | Get initialization flag | wdt_status_t 	wdt_is_init(bool * const p_is_init) |
| **wdt_hndl** | Main watchdog handler | wdt_status_t wdt_hndl(void) |
| **wdt_start** | Start watchdog | wdt_status_t wdt_start(void) |
| **wdt_task_report** | Report to watchdog | wdt_status_t wdt_task_report(const wdt_task_t task) |
| **wdt_pre_reset_isr_callback** | Watchdog pre-reset callback | void wdt_pre_reset_isr_callback(void) |
	

## How to use
---

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

	eWDT_TASK_1 = 0,
	eWDT_TASK_2,
	eWDT_TASK_3,

	// USER CODE END...

	eWDT_TASK_NUM_OF
} wdt_task_t;
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

	// ------------------------------------------------------------------
	//	Task name			    Report timeout [ms]	    Enable flag 	
	// ------------------------------------------------------------------
    { .p_name = "Task 1",      .timeout=100UL,         .enable=true        }
    { .p_name = "Task 2",      .timeout=2000UL,        .enable=true        }


    // Example for enable usage
    #if ( RELEASE_MODE )
        { .p_name = "Task 3",    .timeout=500UL,         .enable=false    }
    #else
        { .p_name = "Task 3",    .timeout=500UL,         .enable=true     }
    #endif
    // ------------------------------------------------------------------

    // USER CODE END...
}
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
