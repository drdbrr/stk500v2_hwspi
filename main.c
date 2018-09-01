#include <stdlib.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include "isp.h"
#include "clock.h"
#include "uart.h"
#include <avr/sfr_defs.h>
#include <util/delay.h>
#include <avr/wdt.h>
#include "main.h"
#include "command.h"
#include "usbasp.h"

#define CONFIG_PARAM_BUILD_NUMBER_LOW   0
#define CONFIG_PARAM_BUILD_NUMBER_HIGH  1
#define CONFIG_PARAM_HW_VER             2
#define D_CONFIG_PARAM_SW_MAJOR         2
#define D_CONFIG_PARAM_SW_MINOR         0x0a
#define CONFIG_PARAM_VADJUST            25
#define CONFIG_PARAM_OSC_PSCALE         0
#define CONFIG_PARAM_OSC_CMATCH         1

static unsigned char msg_buf[295];
static unsigned long address=0;
static unsigned char larger_than_64k=0;
static unsigned char new_address=0;
static unsigned char extended_address=0;
static uint16_t saddress=0;

static unsigned char prg_state=0;
static unsigned char param_controller_init=0;
static unsigned char detected_vtg_from_reset_pin=1;

void transmit_answer(unsigned char seqnum,uint16_t len)
{
        unsigned char cksum;
        unsigned char c;
        uint16_t i;
        if (len>285 || len <1){
            // software error
            len = 2;
            // msg_buf[0]: not changed
            msg_buf[1] = STATUS_CMD_FAILED;
        }
        uart_putc(MESSAGE_START); // 0x1B
        cksum = MESSAGE_START^0;
        uart_putc(seqnum);
        cksum^=seqnum;
        c=(len>>8)&0xFF;
        uart_putc(c);
        cksum^=c;
        c=len&0xFF;
        uart_putc(c);
        cksum^=c;
        uart_putc(TOKEN); // 0x0E
        cksum^=TOKEN;
        //wd_kick();
        for(i=0;i<len;i++){
            uart_putc(msg_buf[i]);
            cksum^=msg_buf[i];
        }
        uart_putc(cksum);
}

void programcmd(unsigned char seqnum){
        unsigned char tmp,tmp2,addressing_is_word,ci,cj,cstatus;
        uint16_t answerlen;
        uint16_t i,nbytes;
        unsigned int poll_address=0;
        addressing_is_word=1; // 16 bit is default
        
        switch(msg_buf[0]){
            case CMD_SIGN_ON:
                msg_buf[0] = CMD_SIGN_ON; // 0x01
                msg_buf[1] = STATUS_CMD_OK; // 0x00
                msg_buf[2] = 8; //len
                strcpy((char *)&(msg_buf[3]),"STK500_2"); // note: this copies also the null termination
                answerlen=11;
                //ispSetSCKOption(USBASP_ISP_SCK_187_5);
                //ispConnect();
                break;
                
                //NEED TEST
            case CMD_SET_PARAMETER:
                if (msg_buf[1]==PARAM_SCK_DURATION){
                    ispSetSCKOption(msg_buf[2]);
                }else if(msg_buf[1]==PARAM_CONTROLLER_INIT){
                        param_controller_init=msg_buf[2];
                }
                answerlen = 2;
                msg_buf[1] = STATUS_CMD_OK;
                break;
                
                //NEED TEST
            case CMD_GET_PARAMETER:
                tmp=0xff;
                tmp2=0; // msg understood
                switch(msg_buf[1])
                {
                    case PARAM_BUILD_NUMBER_LOW:
                        tmp = CONFIG_PARAM_BUILD_NUMBER_LOW;
                        break;
                    case PARAM_BUILD_NUMBER_HIGH:
                        tmp = CONFIG_PARAM_BUILD_NUMBER_HIGH;
                        break;
                    case PARAM_HW_VER:
                        tmp = CONFIG_PARAM_HW_VER;
                        break;
                    case PARAM_SW_MAJOR:
                        tmp = D_CONFIG_PARAM_SW_MAJOR;
                        break;
                    case PARAM_SW_MINOR:
                        tmp = D_CONFIG_PARAM_SW_MINOR;
                        break;
                    case PARAM_VTARGET:
                        if (detected_vtg_from_reset_pin)
                            tmp = detected_vtg_from_reset_pin; // unit: voltage*10: 50 means 5V
                            break;
                    case PARAM_VADJUST:
                        tmp = CONFIG_PARAM_VADJUST;
                        break;
                    case PARAM_SCK_DURATION:
                        tmp = 0;// DISABLED spi_get_sck_duration();
                        break;
                    case PARAM_RESET_POLARITY:   // is actually write only, list anyhow
                                // 1=avr (reset active=low), 0=at89 (not supported by avrusb500)
                        tmp = 1;
                        break;
                    case PARAM_CONTROLLER_INIT:
                        tmp = param_controller_init;
                        break;
                    case PARAM_OSC_PSCALE:
                        tmp = CONFIG_PARAM_OSC_PSCALE;
                        break;
                    case PARAM_OSC_CMATCH:
                        tmp = CONFIG_PARAM_OSC_CMATCH;
                        break;
                    case PARAM_TOPCARD_DETECT: // stk500 only
                        tmp = 0xFF; // no card
                        break;
                    case PARAM_DATA: // stk500 only
                        tmp = 0; 
                        break;
                    default: 
                        tmp2=1; // command not understood
                        break;
                }
                if (tmp2 ==1){
                    // command not understood
                    answerlen = 2;
                    //msg_buf[0] = CMD_GET_PARAMETER;
                    msg_buf[1] = STATUS_CMD_UNKNOWN;
                }else{
                    answerlen = 3;
                    //msg_buf[0] = CMD_GET_PARAMETER;
                    msg_buf[1] = STATUS_CMD_OK;
                    msg_buf[2] = tmp;
                }
                break;
                
                case CMD_LOAD_ADDRESS:
                    address =  ((unsigned long)msg_buf[1])<<24;
                    address |= ((unsigned long)msg_buf[2])<<16;
                    address |= ((unsigned long)msg_buf[3])<<8;
                    address |= ((unsigned long)msg_buf[4]);
                    if (msg_buf[1] >= 0x80) {
                        larger_than_64k = 1;
                    }else{
                        larger_than_64k = 0;
                    }
                    extended_address = msg_buf[2];
                    new_address = 1;
                    answerlen = 2;
                    msg_buf[1] = STATUS_CMD_OK;
                    break;
                
                case CMD_FIRMWARE_UPGRADE:
                    answerlen = 2;
                    msg_buf[1] = STATUS_CMD_FAILED;
                    break;
                
                case CMD_READ_OSCCAL_ISP:
                case CMD_READ_SIGNATURE_ISP:
                case CMD_READ_LOCK_ISP:
                case CMD_READ_FUSE_ISP:
                    for(ci=0;ci<4;ci++){
                        tmp = ispTransmit(msg_buf[ci+2]);
                        if (msg_buf[1] == (ci + 1)){
                            msg_buf[2] = tmp;
                        }
                    }
                    answerlen = 4;
                    msg_buf[1] = STATUS_CMD_OK;
                    msg_buf[3] = STATUS_CMD_OK;
                    break;
                
                case CMD_LEAVE_PROGMODE_ISP:
                    prg_state=0;
                    ispDisconnect();
                    detected_vtg_from_reset_pin=0;
                    answerlen = 2;
                    msg_buf[1] = STATUS_CMD_OK;
                    break;
                
                case CMD_ENTER_PROGMODE_ISP:
                    prg_state=1;
                    answerlen=2;
                    msg_buf[1] = STATUS_CMD_FAILED;
                    i=0;
                    if (msg_buf[4]> 48){
                        msg_buf[4]=48;
                    }
                    if (msg_buf[5] < 1){
                        msg_buf[5]=1;
                    }
                    ispConnect();
                    while(i<msg_buf[4]){//synchLoops
                    
                        wdt_reset();
                        //_delay_ms(msg_buf[3]); //cmdexeDelay
                        i++;
                        
                        if (!(ispEnterProgrammingMode()))
                            msg_buf[1] = STATUS_CMD_OK;
                        
                        /*
                        ispTransmit(msg_buf[8]);//cmd1
                        //_delay_ms(msg_buf[5]); //byteDelay
                        ispTransmit(msg_buf[9]); //cmd2
                        //_delay_ms(msg_buf[5]); //byteDelay
                        tmp=ispTransmit(msg_buf[10]);//cmd3
                        
                        //_delay_ms(msg_buf[5]); //byteDelay
                        tmp2=ispTransmit(msg_buf[11]);//cmd4
                        //
                        //7=pollIndex, 6=pollValue
                        if(msg_buf[7]==3 && tmp==msg_buf[6]) {
                                msg_buf[1] = STATUS_CMD_OK;
                        }
                        //7=pollIndex, 6=pollValue
                        if(msg_buf[7]!=3 && tmp2==msg_buf[6]) {
                                msg_buf[1] = STATUS_CMD_OK;
                        }
                        if(msg_buf[7]==0) { //pollIndex
                                msg_buf[1] = STATUS_CMD_OK;
                        }
                        if(msg_buf[1] == STATUS_CMD_OK ) {
                                i=msg_buf[4];// end loop
                        }else{
                            ispConnect();
                        }
                        */
                        if(msg_buf[1] == STATUS_CMD_FAILED ) {
                                prg_state=0;
                        }
                }
                break;
                
                case CMD_CHIP_ERASE_ISP:
                    //spi_scklow();       
                    ispTransmit(msg_buf[3]);
                    ispTransmit(msg_buf[4]);
                    ispTransmit(msg_buf[5]);
                    ispTransmit(msg_buf[6]);
                    if(msg_buf[2]==0) {
                        // pollMethod use delay
                        //_delay_ms(msg_buf[1]); // eraseDelay
                    } else {
                        // pollMethod RDY/BSY cmd
                        ci=150; // timeout
                        while((ispTransmit32(0xF0000000)&1)&&ci){
                                ci--;
                        }
                    }
                    answerlen = 2;
                    msg_buf[1] = STATUS_CMD_OK;
                    break;
                
                
                //====================================================================================
                case CMD_PROGRAM_EEPROM_ISP:
                    addressing_is_word=0;
                case CMD_PROGRAM_FLASH_ISP:
                    poll_address=0;
                    ci=150;
                    if (msg_buf[4] < 4){
                        msg_buf[4]=4;
                    }
                    if (msg_buf[4] > 32){
                        msg_buf[4]=32;
                    }
                    saddress=(address&0xffff); // previous address, start address 
                    nbytes = (unsigned int)( (msg_buf[1]<<8) |msg_buf[2]);
                    if (nbytes> 280){
                        answerlen = 2;
                        msg_buf[1] = STATUS_CMD_FAILED;
                        break;
                    }
                    tmp2=msg_buf[3];
                    cstatus=STATUS_CMD_OK;
                    //spi_scklow();       
                    if ((msg_buf[3]&1)==0){
                        for(i=0;i<nbytes;i++)
                        {        
                            if(addressing_is_word && i&1) {
                                ispTransmit(msg_buf[5]|(1<<3));
                            } else {
                                ispTransmit(msg_buf[5]);
                            }
                            ispTransmit16(address&0xffff);
                            ispTransmit(msg_buf[i+10]);
                            if(msg_buf[8]!=msg_buf[i+10]) {
                                poll_address = address&0xFFFF;
                                msg_buf[3]=tmp2;
                            } else {
                                msg_buf[3]= 0x02;
                            }
                            wdt_reset();
                            if (!addressing_is_word){
                                        // eeprom writing, eeprom needs more time
                                        //delay_ms(2);
                                clockWait(7);
                            }
                            if(msg_buf[3]& 0x04) {
                                tmp=msg_buf[8];
                                ci=150; // timeout
                                while(tmp==msg_buf[8] && ci ){
                                    if(addressing_is_word && i&1) {
                                        ispTransmit(msg_buf[7]|(1<<3));
                                    } else {
                                        ispTransmit(msg_buf[7]);
                                    }
                                    ispTransmit16(poll_address);
                                    tmp=ispTransmit(0x00);
                                    ci--;
                                }
                            } else if(msg_buf[3]&0x08){
                                        //RDY/BSY polling
                                ci=150; // timeout
                                while((ispTransmit32(0xF0000000)&1)&&ci){
                                    ci--;
                                }
                            }else{
                                        //timed delay (waiting)
                                clockWait(msg_buf[4]);
                            }
                            if (addressing_is_word){
                                if(i&1) address++;
                            }else{
                                address++;
                            }
                            if (ci==0){
                                cstatus=STATUS_CMD_TOUT;
                            }
                        }                        
                    }else{
                        //page mode, all modern chips, atmega etc...
                        i=0;
                        while(i < nbytes){
                                wdt_reset();
                                if (larger_than_64k && ((address&0xFFFF)==0 || new_address)){
                                        // load extended addr byte 0x4d
                                        ispTransmit(0x4d);
                                        ispTransmit(0x00);
                                        ispTransmit(extended_address);
                                        ispTransmit(0x00);
                                        new_address = 0;
                                }
                                if(addressing_is_word && i&1) {
                                        ispTransmit(msg_buf[5]|(1<<3));
                                } else {
                                        ispTransmit(msg_buf[5]);
                                }
                                ispTransmit16(address&0xffff);
                                ispTransmit(msg_buf[i+10]);
                                if(msg_buf[8]!=msg_buf[i+10]) {
                                        poll_address = address&0xFFFF;
                                } else {
                                        //switch the mode to timed delay (waiting)
                                        //we must preserve bit 0x80
                                        msg_buf[3]= (msg_buf[3]&0x80)|0x10;
                                
                                }
                                if (addressing_is_word){
                                        //increment word address only when we have an uneven byte
                                        if(i&1) {
                                                address++;
                                                if((address&0xFFFF)==0xFFFF){ 
                                                        extended_address++;
                                                }
                                        }
                                }else{
                                        address++;
                                }
                                i++;
                        }
                        if(msg_buf[3]&0x80) {
                                ispTransmit(msg_buf[6]);
                                ispTransmit16(saddress);
                                ispTransmit(0);
                                if (!addressing_is_word){
                                        // eeprom writing, eeprom needs more time
                                        clockWait(3);
                                }
                                ci=150; // timeout
                                if(msg_buf[3]&0x20 && poll_address) {
                                        tmp=msg_buf[8];
                                        while(tmp==msg_buf[8] && ci){
                                                if(poll_address&1) {
                                                        ispTransmit(msg_buf[7]|(1<<3));
                                                } else {
                                                        ispTransmit(msg_buf[7]);
                                                }
                                                ispTransmit16(poll_address);
                                                tmp=ispTransmit(0x00);
                                                ci--;
                                        }
                                        if (ci==0){
                                                cstatus=STATUS_CMD_TOUT;
                                        }
                                } else if(msg_buf[3]& 0x40){
                                        //RDY/BSY polling
                                        while((ispTransmit32(0xF0000000)&1)&&ci){
                                                ci--;
                                        }
                                        if (ci==0){
                                                cstatus=STATUS_RDY_BSY_TOUT;
                                        }
                                }else{
                                        // simple waiting
                                        clockWait(msg_buf[4]);
                                }
                        }
                }
                answerlen = 2;
                //msg_buf[0] = CMD_PROGRAM_FLASH_ISP; or CMD_PROGRAM_EEPROM_ISP
                msg_buf[1] = cstatus;
                break;
                //====================================================================================
                case CMD_READ_EEPROM_ISP:
                    addressing_is_word=0;  // address each byte
                case CMD_READ_FLASH_ISP:
                    nbytes = ((unsigned int)msg_buf[1])<<8;
                    nbytes |= msg_buf[2];
                    tmp = msg_buf[3];
                    if (nbytes> 280){
                        nbytes=280;
                    }
                    i=0;
                    //spi_scklow();       
                    while(i<nbytes)
                    {
                        wdt_reset();
                        if (larger_than_64k && ((address&0xFFFF)==0 || new_address)){
                                // load extended addr byte 0x4d
                                ispTransmit(0x4d);
                                ispTransmit(0x00);
                                ispTransmit(extended_address);
                                ispTransmit(0x00);
                                new_address = 0;
                        }
                    if(addressing_is_word && i&1) {
                        ispTransmit(tmp|(1<<3));
                    } else {
                        ispTransmit(tmp);
                    }
                    ispTransmit16(address&0xffff);
                    msg_buf[i+2] = ispTransmit(0);
                    if (addressing_is_word){
                        if(i&1) {
                            address++;
                            if((address&0xFFFF)==0xFFFF){
                                extended_address++;
                            }
                        }
                    }else{
                        address++;
                    }
                    i++;
                }
                answerlen = nbytes+3;
                msg_buf[1] = STATUS_CMD_OK;
                msg_buf[nbytes+2] = STATUS_CMD_OK;
                break;
                //====================================================================================
                
                case CMD_PROGRAM_LOCK_ISP:
                case CMD_PROGRAM_FUSE_ISP:
                //spi_scklow();       
                    ispTransmit(msg_buf[1]);
                    ispTransmit(msg_buf[2]);
                    ispTransmit(msg_buf[3]);
                    ispTransmit(msg_buf[4]);
                    answerlen =3;
                // msg_buf[0] = CMD_PROGRAM_FUSE_ISP; or CMD_PROGRAM_LOCK_ISP
                    msg_buf[1] = STATUS_CMD_OK;
                    msg_buf[2] = STATUS_CMD_OK;
                    break;
                
                case CMD_SPI_MULTI:
                // 0: CMD_SPI_MULTI
                // 1: NumTx
                // 2: NumRx 
                // 3: RxStartAddr counting from zero
                // 4+: TxData (len in NumTx)
                // example: 0x1d 0x04 0x04 0x00   0x30 0x00 0x00 0x00
                    tmp=msg_buf[2];
                    tmp2=msg_buf[3];
                    cj=0; 
                    ci=0;
                    for (cj=0; cj<msg_buf[1]; cj++) {
                        clockWait(20);
                        if (cj >= tmp2 && ci <tmp){
                            msg_buf[ci+2]=ispTransmit(msg_buf[cj+4]);
                            ci++;
                        }else{
                            ispTransmit(msg_buf[cj+4]);
                        }
                    }
                    while(ci<tmp){
                        msg_buf[ci+2]=0;
                        ci++;
                    }
                    answerlen = ci+3;
                    msg_buf[1] = STATUS_CMD_OK;
                    msg_buf[ci+2] = STATUS_CMD_OK;
                    break;
                
                default:
                    answerlen = 2;
                    msg_buf[1] = STATUS_CMD_UNKNOWN;
                    break;
        }
        transmit_answer(seqnum,answerlen);
}

void main(void)
{
    float dcp=0.5;
    DDRC |= (1<<PC0) | (1<<PC1);
    PORTC |= (1<<PC0) | (1<<PC1);
    //DDRC=0b00000011;
    //PORTC = 0b00000011;
    unsigned char* charr;
    unsigned char c;
    unsigned char cksum=0;
    unsigned char seqnum=0;
    uint16_t msglen=0;
    uint16_t d=0;
    unsigned char msgparsestate=MSG_IDLE;
    wdt_enable(WDTO_2S);
    ispSetSCKOption(USBASP_ISP_SCK_187_5);
    uart_init();
    clockInit();
    spiInit();
    sei();
	while (1)
	{
        uart_getc(&c);
        if (msgparsestate==MSG_IDLE && c == MESSAGE_START){
            msgparsestate=MSG_WAIT_SEQNUM;
            cksum = c^0;
            continue;
        }
        if(msgparsestate==MSG_WAIT_SEQNUM){
            seqnum=c;
            cksum^=c;
            msgparsestate=MSG_WAIT_SIZE1;
            continue;
        }
        if (msgparsestate==MSG_WAIT_SIZE1){
            cksum^=c;
            msglen |= (c<<8);
            msgparsestate=MSG_WAIT_SIZE2;
            continue;
        }
        if (msgparsestate==MSG_WAIT_SIZE2){
            cksum^=c;
            msglen|=(c & 0xff);
            msgparsestate=MSG_WAIT_TOKEN;
            continue;
        }
        if (msgparsestate==MSG_WAIT_TOKEN){
            cksum^=c;
            if (c==TOKEN){
                msgparsestate=MSG_WAIT_MSG;
                d=0;
            }else{
                msgparsestate=MSG_IDLE;
            }
            continue;
        }
        if (msgparsestate==MSG_WAIT_MSG && d<msglen && d<280){
            cksum^=c;
            msg_buf[d]=c;
            d++;
            if (d==msglen){
                msgparsestate=MSG_WAIT_CKSUM;
            }
            continue;
        }
        if (msgparsestate==MSG_WAIT_CKSUM){
            if (c==cksum && msglen > 0){
                programcmd(seqnum);
            }else{
                msg_buf[0] = ANSWER_CKSUM_ERROR;
                msg_buf[1] = STATUS_CKSUM_ERROR;
                transmit_answer(seqnum,2);
            }
        }
        
        msgparsestate=MSG_IDLE;
        msglen=0;
        seqnum=0;
        d=0;
        wdt_reset();
    }
}
