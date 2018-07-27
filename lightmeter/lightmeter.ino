/*
  All-In-One Arduino Lightmeter

  FIRMWARE VERSION 1.1, PRE-RELEASE

  TO DO:
  * Check what happens if the SD card space is full.
  * Option to use date (daily) as file name.
  * Open up settings in settings file (JSON) so that you don't need to reflash
      the whole thing if you want to change anything.
  * Add option to set an upper lux limit for the lightmeter to reduce the amount of data written.
  * TSL2591 timing + 100ms delay between failed measurements.
  * Subtract active time from M_INTERVAL time between measurements to get an accurate interval.
  * SD_CD pin not working correctly?
  * Add optional temperature logging capabilities.
*/
#include <SD.h> //SD Card
#include <ArduinoJson.h> //For JSON data formatting

#include "RTC.h" //My DS3231 stuff
#include "LIGHTSENSOR.h" //My TSL2591 stuff
#include "EXTRA.h" //All the extra functions

#define SD_CD 7 //pin connected to the uSD card detect pin
#define SD_CS 4 //pin connected to the uSD card CS pin

/* ===================
 *  BEGIN USER CONFIG
*/
const String FILE_NAME = "data"; //filename for the data file; 8 chars or less!
const String FILE_EXTENSION = "txt"; //file extension for the data file; 3 chars or less!
const uint32_t MAX_FILESIZE = 500000000; //max filesize in byte, here it's 500MB (NOTE FAT32 SIZE LIMIT!)
const uint16_t M_INTERVAL = 1000; //time between measurements, in ms
const byte MAX_TRIES = 5; //max number of re-tries after an invalid measurement before just continuing
/*  END USER CONFIG
 * ===================
*/

TSL2591 lightsensor = TSL2591(1, 2); //create the objects for my classes
RTC_DS3231 rtc;

File dataFile; //global variable that will hold the data file
byte fileNum = 0; //global number of file
String filePath; //this will hold the file path globally

/* ARDUINO SETUP FUNCTION */
void setup()
{
  pinMode(LED_BUILTIN, OUTPUT); //set builtin LED to output
  pinMode(SD_CD, INPUT_PULLUP); //setup the uSD card detect pin
  
  /* Display some basic information on this sensor
  lightsensor.displaySensorDetails(); */
  
  // INIT TSL2591 IF PRESENT
  if(!lightsensor.begin())
  {
    while(1)
    {
      extra::signal_led(1);
    }
  }
  
  // INIT SD CARD IF PRESENT
  if(!SD.begin(SD_CS))
  {
    while(1)
    {
      extra::signal_led(2);
    }
  }
  
  //INIT RTC
  if(!rtc.begin())
  {
    while(1)
    {
      extra::signal_led(5);
    }
  }
  
  //CHECK IF RTC LOST POWER AND TIME IS INVALID
  if(rtc.lostPower())
  {
    while(1)
    {
      extra::signal_led(7);
    }
  }
  
  //update filePath to point to file_name.file_extenion.
  filePath = FILE_NAME + "." + FILE_EXTENSION;
  dataFile = SD.open(filePath, FILE_WRITE); // open the data file, only one file at a time!
  
  // FLASH 5x REALLY QUICKLY TO SIGNAL OK
  for(byte x = 0; x < 5; x++)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
  }

  //Serial.begin(9600); //DEBUGGING SERIAL
}

/* MAIN LOOP */
void loop()
{
  if(!digitalRead(SD_CD)) //if there is physically no card inserted return
  {
    extra::signal_led(2); //flash no SD card error once
    return;
  }
  
  float lux = lightsensor.luxRead(); //get lux values
  byte tries = 0; //counter for tries
  
  while(lux <= 0. && tries < MAX_TRIES) //check if lux value is valid
  {
    extra::sleep(600); //sleep for 600ms (worst case to let the lightsensor calm down)
    lux = lightsensor.luxRead();
    tries++;
  }
    
  //Serial.println(lux,6); //DEBUG LUX FOR CALIBRATION
    
  //get latest unix timestamp
  uint32_t timestamp = rtc.now().unixtime();
  
  // new static (faster than dynamic) json document and allocate 40 bytes (worst case)
  StaticJsonDocument<40> jsonDoc;
    
  // create new json object that will contain all the logged data
  JsonObject data = jsonDoc.to<JsonObject>();
  
  data[F("unixtime")] = timestamp; //input current RTC unixtime
  data[F("lux")] = lux; //input lux value
    
  //to-be/future size of file with the new data in bytes
  uint32_t future_size = dataFile.size() + measureJson(data);
    
  //check if future_size is bigger than specified max size, iterate until a valid file is opened
  while(future_size > MAX_FILESIZE)
  {
    fileNum++; //add 1 to the file number
    filePath = FILE_NAME + fileNum + "." + FILE_EXTENSION; //update filename to include the file number
    dataFile = SD.open(filePath, FILE_WRITE);
    future_size = dataFile.size() + measureJson(data);
  }
    
  //if the data file is available, write the data to it
  if(dataFile)
  {
    serializeJson(data, dataFile); //append the compact data to the file
    dataFile.flush(); //physically save the data to the file, needs up to 3x the power though
  }
  else //if the file is not available, flash an error
  {
    extra::signal_led(3);
  }
  
  extra::sleep(M_INTERVAL); //enable sleep mode for the time interval between measurements
}

