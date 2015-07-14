/* 
* July 13, 2015
* Bill Esposito
* Reid Kovacs
*
* This sketch sends a number from the serial monitor to the DSP Shield, which doubles the number and returns it. 
* Make sure the DSP Shield has MailboxCallResponse_DSP.ino running.
*/

#include <SPI.h>
#include <DSPShield.h>
#include <mailbox.h>

int val = 0;
int message[1] = {val};

void setup() {
shieldMailbox.begin(SPI_SLAVE);
shieldMailbox.attachHandler(printFirst);
Serial.begin(9600);
Serial.println("Please wait 5 seconds");
Serial.println("Type a number up to 16383 and get the value doubled back.");
}

void loop() {
  while (!Serial.available());
  int val = Serial.parseInt();
  Serial.print("Sent: ");
  Serial.print(val);
  Serial.print("\t");
  message[0] = val;
  shieldMailbox.transmit((byte*)message, 2); //send two bytes
}

void printFirst(){
  Serial.print("Received: ");
  int recvdNum = (shieldMailbox.inbox[1] << 8) + (shieldMailbox.inbox[0]); //the 16 bits sent by the DSP get stuffed into two bytes by the Arduino.
  Serial.print(recvdNum);
  Serial.print("\n");
}

