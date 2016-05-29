
SOURCE_NAME=thermometer
MCU=attiny13a
CC=avr-gcc
OBJCOPY=avr-objcopy
CFLAGS=-g -mmcu=$(MCU) -Wall
#-------------------
all: $(SOURCE_NAME).hex
#-------------------
$(SOURCE_NAME).hex : $(SOURCE_NAME).out 
	$(OBJCOPY) -R .eeprom -O ihex $(SOURCE_NAME).out $(SOURCE_NAME).hex 
$(SOURCE_NAME).out : $(SOURCE_NAME).o 
	$(CC) $(CFLAGS) -o $(SOURCE_NAME).out -Wl,-Map,$(SOURCE_NAME).map $(SOURCE_NAME).o 
$(SOURCE_NAME).o : $(SOURCE_NAME).c 
	$(CC) $(CFLAGS) -Os -c $(SOURCE_NAME).c
load: $(SOURCE_NAME).hex
	sudo avrdude -p attiny13 -c usbtiny -U flash:w:$(SOURCE_NAME).hex:i
#-------------------
clean:
	rm -f *.o *.map *.out
#-------------------

