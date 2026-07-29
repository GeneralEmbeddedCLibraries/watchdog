# Changelog
All notable changes to this project/module will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project/module adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---
## V2.0.0 - 29.07.2026

### Changed
 - **Breaking:** `wdt_is_init` now returns the initialization flag directly (`bool`) instead of via status + output pointer
 - **Breaking:** `wdt_task_get_enable` now returns the task enable state directly (`bool`) instead of via status + output pointer
 - Task time pass calculation now uses unsigned wraparound arithmetic instead of the platform-specific `int32_t` workaround
 - `wdt_task_set_enable` only updates state when the enable flag actually changes
 - `WDT_CFG_MIN_TIMEOUT_TIME_MS` / `WDT_CFG_MAX_TIMEOUT_TIME_MS` are now `uint32_t` (ms) and enforced against the configuration table on init
 - Include guards renamed to drop the reserved leading double underscore (`__WDT_H` -> `WDT_H`, etc.)

### Fixed
 - `wdt_task_get_enable` no longer returns `true` on an error path (uninitialized module / invalid task index previously coerced to `true`)
 - `wdt_if_init` no longer runs the low-level watchdog init after a failed mutex create, which previously overwrote the error status
 - `WDT_CFG_DEBUG_EN` is now forced off outside `DEBUG` builds
 - Discarded return values (`wdt_if_kick`, `wdt_if_release_mutex`, `osMutexRelease`) are now explicitly `(void)`-cast with rationale comments
 - `g_wdt_mutex_attr` given internal linkage (`static`)
 - Removed dead commented-out code and stale `TODO`

### Todo
 - ISR support before reset event
---
## V1.2.0 - 02.07.2024

### Added
 - Ability to enable and disable protected task

### Fixed
 - Fixed incorrect usage of OS mutex

### Todo
 - ISR support before reset event

---
## V1.1.0 - 27.08.2023

### Added
 - Replace old "version.txt" with this changelog

### Changed
 - Updated readme, adding details of statistics
 - Hiding configuration data type
 - Removed doxygen as it is replaced README.md

### Todo
 - ISR support before reset event
 - Replace OS mutex with simple lock inside module

---
## V1.0.0 - 17.09.2021

### Added
 - Single loop or RTOS task protection
 - Configuration via single table
 - Platform independent based on interface
 - Statistics support

### Todo
 - ISR support before reset event
---