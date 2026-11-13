#ifndef UART_LOG_H
#define UART_LOG_H

void log_init(void);
// declares a variadic function — a function that accepts a variable number of
// arguments after fmt pointer
void log_printf(const char *fmt, ...);
void uart_log_task(void *arg);

#endif
