/* Arduino Bidirectional SPI Mailbox Library	*/
/* W. Esposito 6/2014	R01						*/
/************************************************/
/* This library is for bidirectional 			*/
/* communication between two arduinos, one 		*/
/* configured as an SPI master and one as an 	*/
/* SPI slave. In addtion to the normal three 	*/
/* wire spi interface, this method will require */
/* the fourth signal line to request a transmit	*/
/* event from the 'master' to the 'slave' 		*/
/* device be pin 10.							*/
/************************************************/
/* Pins Required: 10 (SS), 11(MOSI), 12(MISO),	*/
/* 13 (SCK), 2(Master Select / MS).				*/
/************************************************/

#include "pins_arduino.h"
#include "mailbox.h"
#include <Arduino.h>

// Definition of interrupt names
#include <avr/io.h>
// ISR interrupt service routine
#include <avr/interrupt.h>

mailbox shieldMailbox;

int mailbox::begin(bool mode) {
	begin(mode, nullMailboxCallback);
	delay(5000);
}
int mailbox::begin(bool mode, void (*callbackFunction)()) {
	newMessage = false;
	messageIndex = 0;
	receiveCallback = callbackFunction;
	if(mode == SPI_MASTER) //we are the master. Setup to be in control
	{
		spiMode = mode;
		digitalWrite(SS, HIGH);  // ensure SS stays high for now
		pinMode(SS, OUTPUT);
		// Put SCK, MOSI, SS pins into output mode
		// also put SCK, MOSI into LOW state, and SS into HIGH state.
		// Then put SPI hardware into Master mode and turn SPI on
		SPIClass::begin();
		
		// Slow down the master a bit
		SPIClass::setClockDivider(SPI_CLOCK_DIV8);
		pinMode(MS, INPUT); //master select line
		digitalWrite(MS, HIGH);
		//attach master select interrupt
		// Global Enable INT0 interrupt
		EIMSK |= ( 1 << INT0);
		//Falling edge triggers interrupt
		//EICRA |= ( 1 << ISC01);
		//EICRA &= ( 0 << ISC00);
		EIFR = 0;
		sei();
		delay(5000);
	}
	else //spi_slave
	{
		SPIClass::begin();
		
		// Slow down the master a bit
		SPIClass::setClockDivider(SPI_CLOCK_DIV8);
		// have to send on master in, *slave out*
		pinMode(MISO, OUTPUT);
		pinMode(MS, OUTPUT); //master select line
		digitalWrite(MS, HIGH);
		pinMode(SS, INPUT);
		// turn on SPI in slave mode
		SPCR |= _BV(SPE);

		// now turn on interrupts
		SPIClass::attachInterrupt();
		delay(5000);
	}
	return 1;
}
int mailbox::end() {
	SPIClass::end(); //SPI End
	EIMSK &= ( 0 << INT0); //Detach GPIO Interrupt.
	return 1;
}

int mailbox::attachHandler(void (*callbackFunction)()) {
	receiveCallback = callbackFunction;
}
int mailbox::detachHandler() {
	receiveCallback = nullMailboxCallback;
}
void mailbox::nullMailboxCallback() //this is a do nothing function
{
	//Serial.println("null callcback");
}
int mailbox::transmit(byte *vector, unsigned int vectorSize) {
	// *outbox = *vector;
	// outboxSize = vectorSize;
	byte transmitChecksum = 0;
	bool transmitSuccess = false;
	if(spiMode == SPI_MASTER)
	{
		Serial.println("here");
		//get ready to transmit, pull slave select low.
		digitalWrite(SS, LOW);
		
		while(!transmitSuccess)
		{
			transmitChecksum = 0;
			byte packetCount = 1;
			byte packetIn = 0;
			//first transmit a start flag
			packetIn = SPIClass::transfer('$');
			delayMicroseconds(10);
			//next, transmit the size
			packetIn = SPIClass::transfer(vectorSize>>8);
			delayMicroseconds(10);
			// Serial.println(packetIn);
			// Serial.println(packetCount);
			// if(packetCount != packetIn)
			// {
			// 	digitalWrite(SS, HIGH);
			// 	SPIClass::transfer(0);
			// 	break;
			// }
			// packetCount++;
			packetIn = SPIClass::transfer(vectorSize);
			delayMicroseconds(10);
			// if(packetCount != packetIn)
			// {
			// 	digitalWrite(SS, HIGH);
			// 	SPIClass::transfer(0);
			// 	break;
			// }
			// packetCount++;
			//iterate over the struct, transmitting
			for(unsigned int i = 0; i < vectorSize; i++)
			{
				transmitChecksum += vector[i];
				packetIn = SPIClass::transfer(vector[i]);
				delayMicroseconds(10);
				// if(packetCount != packetIn)
				// {
				// 	digitalWrite(SS, HIGH);
				// 	SPIClass::transfer(0);
				// 	break;
				// }
				// packetCount++;
			}

			//transmit a checksum
			packetIn = SPIClass::transfer(transmitChecksum);
			delayMicroseconds(10);
			// if(packetCount != packetIn)
			// {
			// 	digitalWrite(SS, HIGH);
			// 	SPIClass::transfer(0);
			// 	break;
			// }
			// packetCount++;
			//done tranmitting, pull up slave select pin.
			transmitSuccess = true;
		}
		
		digitalWrite(SS, HIGH);
	}
	else //SPI_SLAVE
	{
		//Serial.println("transmit function in slave mode.");
		SPIClass::detachInterrupt();
		int tryCount = 0;
		while(!transmitSuccess)
		{
			transmitChecksum = 0;
			byte packetCount = 1;
			byte packetIn = 0;
			//while(transmitFlag){}; //wait for any outgoing transmissions to complete.
			//first, a sentinel
			for(unsigned int i = 0; i < vectorSize; i++)
			{
				transmitChecksum += vector[i];
			}
			digitalWrite(MS, LOW);
			SPDR = '$';
			SPIClass::transfer('$');
			packetIn = SPIClass::transfer(transmitChecksum);
			packetIn = SPIClass::transfer(vectorSize>>8);
			// if(packetCount != packetIn)
			// {
			// 	break;
			// }
			packetCount++;
			packetIn = SPIClass::transfer(vectorSize);
			// if(packetCount != packetIn)
			// {
			// 	break;
			// }
			packetCount++;
			//iterate over the struct, transmitting
			for(unsigned int i = 0; i < vectorSize; i++)
			{
				packetIn = SPIClass::transfer(vector[i]);
				// if(packetCount != packetIn)
				// {
				// 	break;
				// }
				packetCount++;
			}

			//transmit a checksum
			// if(packetCount != packetIn)
			// {
			// 	break;
			// }
			//transmitSuccess = true;
			packetIn = SPIClass::transfer(0x00);
			//Serial.println(packetIn, HEX);
			
			if(packetIn == MB_ACK)
			{
			 	transmitSuccess = true;
			}
			else
			{
				digitalWrite(MS, HIGH);
				delay(50); //delay to avoid oveloading the DSP
			}
			tryCount++;
			if(tryCount > RETRIES) //give up after too many failed attempts at transmission
			{
				break;
			}
			break; //temporary skip since retry isn't working;
		}
			//done tranmitting, pull up slave select pin.
			digitalWrite(MS, HIGH);
			SPIClass::attachInterrupt();
			delay(50); //delay to avoid oveloading the DSP
	}

	return transmitSuccess;
}
int mailbox::receive() {
	newMessage = false; //this just blocks until the interrup recieves a new message.
	while(!newMessage)
	{}
	return 1;
}

ISR (SPI_STC_vect) //handles recieve in slave mode.
//ISR routine grabs data from SPI buffer, deserializes it in the inbox, 
//and then when the inbox is full or the serialize flag is recieved, calls
//the assigned user function to parse the data.
{
	byte counter = 0;
	shieldMailbox.success = false;
	byte data = SPDR;
	//Serial.println(data, HEX);
	if(data == '$') //look for message start flag;
	{

		//we got the message start sentinel. Next we wait for the checksum.
		shieldMailbox.checksum = 0;
		byte recvdChecksum = (byte) SPI.transfer(counter++);

		//the next 16 bits are the message length.
		shieldMailbox.inboxSize = (byte) SPI.transfer(counter++);
		shieldMailbox.inboxSize <<= 8;

		shieldMailbox.inboxSize += (byte) SPI.transfer(counter++);

		//now we allocate memory for the message itself.
		shieldMailbox.inbox = (byte*) realloc(shieldMailbox.inbox, shieldMailbox.inboxSize);
		//Iterate until we get enough bytes for the message.
		for(unsigned int i = 0; i < shieldMailbox.inboxSize; i+=2)
		{
			shieldMailbox.inbox[i+1] = (byte) SPI.transfer(counter++);
			shieldMailbox.inbox[i] = (byte) SPI.transfer(counter++);
			
			int checksumTemp = shieldMailbox.inbox[i+1];
			checksumTemp <<=8;
			checksumTemp += shieldMailbox.inbox[i];
			shieldMailbox.checksum += checksumTemp;
		}
		//Serial.println(shieldMailbox.inboxSize);
		//return;
		//if(1) //temporarily commented out check, for testing.
		if(shieldMailbox.checksum == recvdChecksum) //check to see if we got the message correctly.
		{
			shieldMailbox.success = true; //we did, flag successful new message.
			// for(int i = 0; i < 12; i++)
			// {
			// 	Serial.print(shieldMailbox.inbox[i], HEX);
			// 	Serial.print(',');
			// }
			// Serial.print('\n');
			shieldMailbox.receiveCallback(); //call the user defined message recieve function.
		}
		shieldMailbox.messageIndex = 0;
		shieldMailbox.newMessage = true;
	}
}  // end of interrupt routine SPI_STC_vect

ISR(INT0_vect) {
	//#define DALY 5
	digitalWrite(SS, LOW);
	byte counter = 0;
	shieldMailbox.success = false;
	byte c;
	c = (byte) SPI.transfer(counter++);
	//delayMicroseconds(DALY);
	if(c == '$') //look for message start flag;
	{
		shieldMailbox.inboxSize = (byte) SPI.transfer(counter++);
		shieldMailbox.inboxSize <<= 8;
		if(digitalRead(MS))
		{
			return;
		}
		//delayMicroseconds(DALY);
		shieldMailbox.inboxSize += (byte) SPI.transfer(counter++);
		if(digitalRead(MS))
		{
			return;
		}
		//delayMicroseconds(DALY);
		shieldMailbox.inbox = (byte*) realloc(shieldMailbox.inbox, shieldMailbox.inboxSize);

		for(unsigned int i = 0; i < shieldMailbox.inboxSize; i++)
		{
			shieldMailbox.inbox[i] = (byte) SPI.transfer(counter++);
			if(digitalRead(MS))
			{
				return;
			}
			//delayMicroseconds(DALY);
		}
		byte newChecksum = (byte) SPI.transfer(counter++);
		if(digitalRead(MS))
		{
			return;
		}
		//delayMicroseconds(DALY);
		shieldMailbox.checksum = 0;
		for(unsigned int i = 0; i < shieldMailbox.inboxSize; i++)
		{
			shieldMailbox.checksum += shieldMailbox.inbox[i];	
		}
		if(shieldMailbox.checksum == newChecksum)
		{
			shieldMailbox.success = true; //transfer successful

			//user function goes here	
			shieldMailbox.receiveCallback();
		}
		shieldMailbox.messageIndex = 0;
		shieldMailbox.checksum = 0;
		shieldMailbox.newMessage = true;
	}		
	digitalWrite(SS, HIGH);
}