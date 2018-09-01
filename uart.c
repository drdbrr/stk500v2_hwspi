/* vim: set sw=8 ts=8 si et: */
/*********************************************
* UART interface without interrupt
* Author: Guido Socher, Copyright: GPL 
* Copyright: GPL
**********************************************/
#include <avr/interrupt.h>
#include <string.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include "uart.h"

void uart_init(void) 
{
        unsigned int baud=10;  
        UBRR0H=(unsigned char) (baud >>8);
        UBRR0L=(unsigned char) (baud & 0xFF);
        UCSR0B =  (1<<RXEN0) | (1<<TXEN0);
        UCSR0C = (1<<UCSZ01)|(1<<UCSZ00);
}

void uart_putc(char c) 
{
        while (!(UCSR0A & (1<<UDRE0)));
        UDR0=c;
}

void uart_sendstr(char *s) 
{
        while (*s){
                uart_putc(*s);
                s++;
        }
}

void uart_sendstr_p(const char *progmem_s)
{
        char c;
        while ((c = pgm_read_byte(progmem_s++))) {
                uart_putc(c);
        }

}

unsigned char uart_getc(unsigned char* data)  
{
        while(!(UCSR0A & (1<<RXC0))){
            wdt_reset();
        }
        *data = UDR0;
        return 1;
}

void uart_flushRXbuf(void)  
{
        unsigned char tmp __attribute__((unused));
        while(UCSR0A & (1<<RXC0)){
                tmp=UDR0;
        }
}
