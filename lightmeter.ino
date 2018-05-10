/*
  DIY Kuffner-Sternwarte Lightmeter

  TODO:
  * Check if SD Card space is full.
  * Fix negative lux values.
  * Less flush?
*/
#include <SD.h> //SD Card
#include <EEPROM.h> //For EEPROM storage
#include <ArduinoJson.h> //For JSON data formatting
#include <Adafruit_SleepyDog.h> //For power down mode between logging
#include "rtc.h"
#include "lightsensor.h"

#define _MAJORV 2 //major firmware version
#define _MINORV 1 //minor firmware version

/* BEGIN USER CONFIG */
const byte SD_PIN = 4; //pin connected to the chip select line of the SD card
const String FILE_NAME = "data"; //filename for the data file; 8 chars or less!
const String FILE_EXTENSION = ".txt"; //file extension for the data file; 3 chars or less!
const uint32_t MAX_FILESIZE = 500000000; //max filesize in byte, here it's 500MB (NOTE FAT32 SIZE LIMIT!)
const uint16_t M_INTERVAL = 1000; //measurement interval for data logging, in ms
/* END USER CONFIG */

TSL2591 lightsensor = TSL2591(0, 0); //create the objects for my classes
DS3231 rtc = DS3231();

File dataFile; //global variable that will hold the data file
uint8_t fileNum = 0; //global number of file
String filePath; //this will hold the file path globally

/* SIGNAL LED FUNCTION FOR ERRORS AND USER INTERFACE  */
void signal_led(byte flashes)
{
  /* ERROR/SIGNAL FLASHES:
   *  1: NO LIGHTSENSOR
   *  2: NO SD CARD
   *  3: FILE WRITE ERROR
   *  4: SD FILESYSTEM ERROR
   *  5: NO RTC
   *  6: OTHER MISC ERROR
   *  7: RTC TIME NOT SET
   */
  for(byte x = 0; x < flashes; x++)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
  }
  Watchdog.sleep(800); //800ms sleep delay after a LED signal to mark a distinct end to the signal
}

/* ARDUINO SETUP FUNCTION */
void setup()
{
  pinMode(LED_BUILTIN, OUTPUT); //set builtin LED to output
  pinMode(13, OUTPUT); //sd card led

  /* Display some basic information on this sensor
  lightsensor.displaySensorDetails(); */

  // INIT TSL2591 IF PRESENT
  if(!lightsensor.begin())
  {
    while(1)
    {
      signal_led(1);
    }
  }

  // INIT SD CARD IF PRESENT
  if(!SD.begin(SD_PIN))
  {
    while(1)
    {
      signal_led(2);
    }
  }

  //INIT RTC
  if(!rtc.begin())
  {
    while(1)
    {
      signal_led(5);
    }
  }

  //update filePath to point to file_name.file_extenion.
  filePath = FILE_NAME + FILE_EXTENSION;
  dataFile = SD.open(filePath, FILE_WRITE); // open the data file, only one file at a time!

  // FIRMWARE LED FLASH
  for(byte x = 0; x < _MAJORV; x++) //flash major version
  {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
  }
  
  delay(400); //400ms delay between the stages
  
  for(byte x = 0; x < _MINORV; x++) //flash minor version
  {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
  }
  
  Watchdog.sleep(1000); //1000ms sleep delay between coming errors and firmware flash
}

/* MAIN LOOP */
void loop()
{
  if(rtc.lostPower()) //check if RTC lost power and time is wiped
  {
    while(1)
    {
      signal_led(7);
    }
  }
  
  // new dynamic json buffer, let's just assume 20 bytes for now
  DynamicJsonBuffer jsonBuffer(30);
  
  // create new json object that will contain all the logged data
  JsonObject& data = jsonBuffer.createObject();
  
  data["unixtime"] = rtc.unixtime(); //input current RTC unixtime
  data["lux"] = lightsensor.luxRead(); //input lux value
  
  //to-be/future size of file with the new data in bytes
  uint32_t future_size = dataFile.size() + data.measureLength();
  
  //check if future_size is bigger than specified max size, if yes, write to new file
  if(future_size > MAX_FILESIZE)
  {
    fileNum++; //add 1 to the file number
    filePath = FILE_NAME + fileNum + FILE_EXTENSION; //update filename to include the file number
    dataFile = SD.open(filePath, FILE_WRITE);
  }

  //if the data file is available, write the data to it
  if(dataFile)
  {
    data.printTo(dataFile); //compactly append to the file
    dataFile.flush(); //save the data to the file, needs up to 3x the power
  }
  else //if the file is not available, flash an error
  {
    signal_led(3);
  }

  Watchdog.sleep(M_INTERVAL); //enable sleep mode for the time interval between measurements
}

