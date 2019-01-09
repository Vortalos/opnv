#include <SoftwareSerial.h>
#include <Wire.h>  // Comes with Arduino IDE
#include <dht.h>
#include "RTClib.h"
#include <SPI.h>
#include <SD.h>

SoftwareSerial mySerial(A0, A1); // A0 and A1 on Arduino to RX, TX on MH-Z14 respectively
RTC_DS1307 rtc;

// Calibrate zero point command  
//byte cmd[9] = {0xFF,0x01,0x87,0x00,0x00,0x00,0x00,0x00,0x78};
//another calibrate zero point command, not sure which is correct
//byte cmd[9] = {0xff, 0x87, 0x87, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf2}; 

//Calibrate span point command
//byte cmd[9] = {0xFF,0x01,0x88,0x07,0xD0,0x00,0x00,0x00,0xA0};
//another calibrate span point command, not sure which is correct
//byte cmd[9] = {0xff, 0x88, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0};


//Reqest Gas concentration command
byte cmd[9] = {0xFF,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79}; 
char response[9]; 

String ppmString = " ";

#define Hall_Pin 5 //Pin of the Hall-Sensor for Pluviometer
#define DHT11_Pin 4 //Pin of the Temp/Humidity-Sensor

dht DHT;

const int chipSelect = 10;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

int Rain_count = 0;
int oldHallState = 0;
int newHallState = 0;

int responseHigh;
int responseLow;
int ppm;

int current_sensor;

String oldWeekDay = " ";
String newWeekDay;
int midnight_log = 0;

int o2_pin = 5;
int o2_value = 0;

String currentFileName = "";

String delimiter = "; ";

int logDelayMinutes = 0; //delay between logs, minutes
int logDelaySeconds = 20; //delay between logs, seconds

int precipitation_multiplier = 1; //precipitation per  in mm

DateTime now;
DateTime nextLog;

void setup() {  
  // Run serial to connect to a computer
  Serial.begin(9600);

  
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
  }
  if (! rtc.isrunning()) { //check if RTC is running
    Serial.println("RTC is NOT running!");
  }

  // following line sets the RTC to the date & time this sketch was compiled
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  //  This line sets the RTC with an explicit date & time, for example to set
  // January 21, 2018 at 3am you would call:
  rtc.adjust(DateTime(2018, 1, 21, 23, 59, 20));
  Serial.println("Successfully adjusted time");

  // Start the serial connection to the MH-Z14 CO2-Sensor
  mySerial.begin(9600);
  pinMode(Hall_Pin, INPUT);

  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    return;
  }
  now = rtc.now();
  //write_serial();

  nextLog = now + TimeSpan(0, 0, 0, 10);
  check_sensors();
}

void loop() 
{
  now = rtc.now();

  newWeekDay = String(daysOfTheWeek[now.dayOfTheWeek()]);
  if (now.hour() == 23 && now.minute() == 59 && now.second() == 59 && midnight_log==0){
    create_filename();
    write_sd_card(currentFileName); //create new log entry
    midnight_log = 1; //change value so it will only write 1 log file right before midnight
    Rain_count = 0;
  }
  if (oldWeekDay != newWeekDay) { //check if day has changed, if so change current file name and save a log file
    create_filename();
    oldWeekDay = newWeekDay;
    write_sd_card(currentFileName); //create new log entry
    nextLog = now + TimeSpan(0, 0, logDelayMinutes, logDelaySeconds); //creates new date at which to add a log file
    midnight_log = 0;
  }

  
  newHallState = digitalRead(Hall_Pin);
  if (newHallState == LOW && oldHallState == HIGH){
    Rain_count ++;
    Serial.print(" Triggered: ");
    Serial.print(Rain_count);
    Serial.println("x");
  }
  oldHallState = newHallState;

  
  //write_serial();

  if (nextLog.unixtime() - now.unixtime() == 0){  //check if time until nextLog is shorter than 1 second
    check_sensors();
    write_sd_card(currentFileName); //create new log entry
    nextLog = now + TimeSpan(0, 0, logDelayMinutes, logDelaySeconds); //creates new date at which to add a log file
  }

  delay(3);
  
}

void create_filename(){
  currentFileName = String(now.year());
    //currentFileName += "-";
    if (now.month() < 10){ currentFileName += "0";}
    currentFileName += String(now.month());
    //currentFileName += "-";
    if (now.day() < 10){ currentFileName += "0";}
    currentFileName += String(now.day());
}

void write_sd_card (String fileName) {
  String dataString = ""; //create String

  if (now.day() < 10){ dataString += "0";}
  dataString += String(now.day());
  dataString += ".";
  if (now.month() < 10){ dataString += "0";}
  dataString += String(now.month());
  dataString += ".";
  dataString += String(now.year());
  dataString += delimiter;
  dataString += String(daysOfTheWeek[now.dayOfTheWeek()]);
  dataString += delimiter;
  if (now.hour() < 10){ dataString += "0";}
  dataString += now.hour();
  dataString += delimiter;
  if (now.minute() <10){dataString += "0";}
  dataString += now.minute();
  dataString += delimiter;
  if (now.second() < 10){ dataString += "0";}
  dataString += now.second();
  dataString += delimiter;

  dataString += String(DHT.temperature);
  dataString += delimiter;
  dataString += String(DHT.humidity);
  dataString += delimiter;

  dataString += ppmString;
  dataString += delimiter;

  dataString += o2_value;
  dataString += delimiter;

  dataString += Rain_count;
  dataString += delimiter;
  dataString += Rain_count * precipitation_multiplier;



  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  fileName += ".txt";
  File dataFile = SD.open(fileName, FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
    // print to the serial port too:
    Serial.print("Successfully written following data to ");
    Serial.print(fileName);
    Serial.println(" :");
    Serial.println(dataString);
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.print("error opening log file ");
    Serial.println(fileName);
  }
  
}

void check_sensors() {  
  int chk = DHT.read11(DHT11_Pin);
  
  //Pinging CO2, we read, reply and translate intoPPM
  mySerial.write(cmd,9);
  mySerial.readBytes(response, 9);
  responseHigh = (int) response[2];
  responseLow = (int) response[3];
  ppm = (256*responseHigh)+responseLow;

  ppmString = String(ppm); //int to string
  
  o2_value = analogRead(o2_pin);
  
}

void write_serial() {  
  for (int i=0; i<5; i++){
    Serial.println();
  }
  Serial.println("SENSOR READINGS");
  Serial.println("---");
  if (now.day()<10){Serial.print("0");}
  Serial.print(now.day(), DEC);
  Serial.print('.');
  if (now.month()<10){Serial.print("0");}
  Serial.print(now.month(), DEC);
  Serial.print('.');
  Serial.print(now.year(), DEC);
  Serial.print(" (");
  Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);   
  Serial.print(") ");
  if (now.hour()<10){Serial.print("0");}
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  if (now.minute()<10){Serial.print("0");}
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  if (now.second()<10){Serial.print("0");}
  Serial.print(now.second(), DEC);
  Serial.println();

  Serial.println("---");
  Serial.print("Triggered ");
  Serial.print(Rain_count);
  Serial.println(" times.");
  
  Serial.println("---");
  Serial.print("Temperature: ");
  Serial.println(DHT.temperature);
  Serial.print("Rel. Humidity: ");
  Serial.println(DHT.humidity);

  
  Serial.println("---");
  //Serial.print("Int: ");
  //for(int x=0;x<9;x++)
  //{
  //  Serial.print((int)response[x], HEX);
  //  Serial.print(" ");
  //}
  //Serial.println();
  //Serial.print("Hex: ");
  //for(int x=0;x<9;x++)
  //{
  //  Serial.print((int)response[x]);
  //  Serial.print(" ");
  //}
  //Serial.print("\n");
  Serial.println("High:" + (String)responseHigh + " Low:" + (String)responseLow);
  Serial.print("PPM ");
  Serial.println(ppm);
  Serial.println("---");
}

