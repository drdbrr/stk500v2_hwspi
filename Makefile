MCU		=	atmega328p
PROJECTNAME	=	stk500v2
PROJECTSRC	=	main.c uart.c isp.c clock.c
#avr-uart/uart.c
F_CPU		=	20000000UL

INC		=	-I/usr/avr/include

OPTLEVEL	=	s

AVRDUDE_MCU	=	m328p
AVRDUDE_PROGID	=	usbasp
#AVRDUDE_PORT	=	/dev/ttyUSB1

# -DF_CPU=$(F_CPU)
CFLAGS		=	-I. $(INC) -g -mmcu=$(MCU) -O$(OPTLEVEL)  -DF_CPU=$(F_CPU)\
			-mcall-prologues -std=gnu99 -W -Wall \
			-fpack-struct -fshort-enums \
			-funsigned-bitfields -funsigned-char    \
			-Wall -Wstrict-prototypes -ffreestanding \
			-Wa,-ahlms=$(firstword                  \
			$(filter %.lst, $(<:.c=.lst)))

CPPFLAGS	=	-fno-exceptions               \
			-Wa,-ahlms=$(firstword         \
			$(filter %.lst, $(<:.cpp=.lst))\
			$(filter %.lst, $(<:.cc=.lst)) \
			$(filter %.lst, $(<:.C=.lst)))

ASMFLAGS	=	-I. $(INC) -mmcu=$(MCU)        \
			-x assembler-with-cpp            \
			-Wa,-gstabs,-ahlms=$(firstword   \
			$(<:.S=.lst) $(<.s=.lst))

LDFLAGS=-Wl,-Map,$(TRG).map -mmcu=$(MCU) $(LIBS) 
	 
CC=avr-gcc
OBJCOPY=avr-objcopy
OBJDUMP=avr-objdump
SIZE=avr-size
AVRDUDE=avrdude
REMOVE=rm -f -v

TRG=$(PROJECTNAME).out
DUMPTRG=$(PROJECTNAME).s
HEXROMTRG=$(PROJECTNAME).hex 
HEXTRG=$(HEXROMTRG) $(PROJECTNAME).ee.hex

CPPFILES=$(filter %.cpp, $(PROJECTSRC))
CCFILES=$(filter %.cc, $(PROJECTSRC))
BIGCFILES=$(filter %.C, $(PROJECTSRC))
CFILES=$(filter %.c, $(PROJECTSRC))
ASMFILES=$(filter %.S, $(PROJECTSRC))
OBJDEPS=$(CFILES:.c=.o)    \
	$(CPPFILES:.cpp=.o)\
	$(BIGCFILES:.C=.o) \
	$(CCFILES:.cc=.o)  \
	$(ASMFILES:.S=.o)
LST=$(filter %.lst, $(OBJDEPS:.o=.lst))
GENASMFILES=$(filter %.s, $(OBJDEPS:.o=.s)) 

.SUFFIXES : .c .cc .cpp .C .o .out .s .S \
	.hex .ee.hex .h .hh .hpp

all: $(TRG)

disasm: $(DUMPTRG) stats

stats: $(TRG)
	$(OBJDUMP) -h $(TRG)
	$(SIZE) $(TRG) 

hex: $(HEXTRG)

writeflash: hex
	$(AVRDUDE) -c $(AVRDUDE_PROGID)   \
	 -p $(AVRDUDE_MCU) -P $(AVRDUDE_PORT) -e        \
	 -U flash:w:$(HEXROMTRG) -y

install: writeflash

clean:
	$(REMOVE) $(TRG) $(TRG).map $(DUMPTRG)
	$(REMOVE) $(OBJDEPS)
	$(REMOVE) $(LST)
	$(REMOVE) $(GENASMFILES)
	$(REMOVE) $(HEXTRG)

svn:	clean
	$(REMOVE) *~

$(DUMPTRG): $(TRG) 
	$(OBJDUMP) -S  $< > $@


$(TRG): $(OBJDEPS) 
	$(CC) $(LDFLAGS) -o $(TRG) $(OBJDEPS)
	$(SIZE) $(TRG) 
	
%.s: %.c
	$(CC) -S $(CFLAGS) $< -o $@

%.s: %.S
	$(CC) -S $(ASMFLAGS) $< > $@

.cpp.s .cc.s .C.s :
	$(CC) -S $(CFLAGS) $(CPPFLAGS) $< -o $@

.c.o: 
	$(CC) $(CFLAGS) -c $< -o $@

.cc.o .cpp.o .C.o :
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

.S.o :
	$(CC) $(ASMFLAGS) -c $< -o $@


.out.hex:
	$(OBJCOPY) -j .text                    \
		-j .data                       \
		-O ihex $< $@

.out.ee.hex:
	$(OBJCOPY) -j .eeprom                  \
		--change-section-lma .eeprom=0 \
		-O ihex $< $@
	
