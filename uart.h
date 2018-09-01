/* vim: set sw=8 ts=8 si et: */
/*************************************************************************
 Title:   C include file for uart
 Target:    atmega8
 Copyright: GPL
***************************************************************************/
#include <avr/pgmspace.h>

extern void uart_init(void);
extern void uart_putc(char c);
extern void uart_sendstr(char *s);
extern void uart_sendstr_p(const char *progmem_s);
unsigned char uart_getc(unsigned char*);
extern void uart_flushRXbuf(void);

