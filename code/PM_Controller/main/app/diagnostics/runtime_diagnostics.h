#ifndef PM_RUNTIME_DIAGNOSTICS_H
#define PM_RUNTIME_DIAGNOSTICS_H

/* Main task only, after application tasks have been created. No display lock,
 * new task or timer. Task handles must remain alive after registration. */
void runtime_diagnostics_init(void);
void runtime_diagnostics_poll(void);
#endif
