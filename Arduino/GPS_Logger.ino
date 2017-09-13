/*
 
Copyright 2017 Matthias Pletl

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/


#include <SoftwareSerial.h>
#include <TinyGPS.h>
#include <SPI.h>
#include "SdFat.h"

// GPS
TinyGPS gps;
SoftwareSerial nss(3, 5);

// SD Card
#define SD_CS_PIN SS
SdFat SD;
File LogFile;

String strFilename = "";
bool boolFirstRun = true;

#define RedLed 6 //D6
#define GreenLed 7 //D7

void setup()
{
  // Debug
  Serial.begin(115200);
  // GPS
  nss.begin(9600);

  Serial.println("GPSLogger - Matthias Pletl");
  Serial.println();

  // Note that even if it's not used as the CS pin, the hardware SS pin
  // (10 on most Arduino boards, 53 on the Mega) must be left as an output
  // or the SD library functions will not work.
  pinMode(10, OUTPUT);

  // RED LED
  pinMode(RedLed, OUTPUT);
  // GREEN LED
  pinMode(GreenLed, OUTPUT);


  // Initialize SD card
  while (!SD.begin(SD_CS_PIN))
  {
    Serial.println("SD card initialization failed!");
    digitalWrite(GreenLed, HIGH);
    digitalWrite(RedLed, HIGH);
    delay(2000);
  }

  Serial.println("SD card initialization done.");
  digitalWrite(RedLed, LOW);
  digitalWrite(GreenLed, LOW);

  // Create Logfile
  // max Logfiles = 100
  for (int i = 0; i < 101; i++)
  {
    char charbuffer[21];

    String tmpString = "GPS_Log_";

    char numstr[21]; // enough to hold all numbers up to 64-bits
    sprintf(numstr, "%d", i);
    tmpString = tmpString + numstr + ".txt";

    strcpy(charbuffer, tmpString.c_str());

    if (!SD.exists(charbuffer))
    {
      String str(charbuffer);
      strFilename = str;
      Serial.println("Logfilename created.");
      break;
    }
  }
}

void loop()
{
  if (nss.available())
  {
    if (gps.encode(nss.read()))
    {
      Serial.print("Satellites in view: ");
      Serial.println(gps.satsinview(), DEC);

      float flat, flon;
      unsigned long fix_age; 
      
      // returns +- latitude/longitude in degrees
      gps.f_get_position(&flat, &flon, &fix_age);
      
      if (fix_age == TinyGPS::GPS_INVALID_AGE)
        Serial.println("No fix detected");
      else if (fix_age > 5000)
        Serial.println("Warning: possible stale data!");
      else
        Serial.println("Data is current.");


      if (gps.fixtype() == TinyGPS::GPS_FIX_NO_FIX)
      {
        Serial.print("No fix.");
        digitalWrite(RedLed, HIGH);
        delay(500);
        digitalWrite(RedLed, LOW);
      }
      else if (gps.fixtype() == TinyGPS::GPS_FIX_3D || gps.fixtype() == TinyGPS::GPS_FIX_2D)
      {
        // We have a fix (GPS_FIX_2D or GPS_FIX_3D)
        // Toggle Led once
        if (boolFirstRun)
        {
          boolFirstRun = false;
          toggleGreenLed();
        }

        long lat, lon;
        float flat, flon;
        unsigned long age, date, time, chars;
        int year;
        byte month, day, hour, minute, second, hundredths;
        unsigned short sentences, failed;


        LogFile = SD.open(strFilename, FILE_WRITE);

        // If the file opened okay, write to it:
        if (LogFile) {

          digitalWrite(7, HIGH);

          // LAT, LONG ;
          gps.f_get_position(&flat, &flon, &age);
          LogFile.print(flat, 5); LogFile.print(","); LogFile.print(flon, 5);

          LogFile.print(";");
          feedgps();

          // Date, time
          gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths, &age);
          if (age == TinyGPS::GPS_INVALID_AGE) // write placeholder
          {
            LogFile.print(0);
            LogFile.print(";");
            LogFile.print(0);
          }
          else
          {
            LogFile.print(static_cast<int>(day)); LogFile.print("/"); LogFile.print(static_cast<int>(month)); LogFile.print("/"); LogFile.print(year);
            LogFile.print(";");
            LogFile.print(static_cast<int>(hour)); LogFile.print(":"); LogFile.print(static_cast<int>(minute)); LogFile.print(":"); LogFile.print(static_cast<int>(second));
          }
          LogFile.print(";");
          feedgps();

          // Hight in m
          LogFile.print(gps.altitude() / 100 == TinyGPS::GPS_INVALID_ALTITUDE ? 0 : gps.altitude() / 100);
          LogFile.print(";");
          feedgps();

          // Speed in km/h
          LogFile.print(gps.f_speed_kmph() == TinyGPS::GPS_INVALID_SPEED ? 0 : gps.f_speed_kmph());
          LogFile.print(";");
          feedgps();


          //2D or 3D
          if (gps.fixtype() == TinyGPS::GPS_FIX_2D)
            LogFile.print("2D");
          else
            LogFile.print("3D");

          LogFile.print(";");
          feedgps();

          // Sats
          LogFile.print(gps.satsused(), DEC);
          LogFile.print(";");
          feedgps();


          LogFile.println();
          LogFile.close();
          Serial.println("done.");
          digitalWrite(GreenLed, LOW);

        }
        else
        {
          // if the file didn't open, print an error:
          Serial.println("error opening " + strFilename);
          toggleRedLed();
        }
      }
    }
  }
}


void toggleGreenLed()
{
  for (int i = 0; i < 6; i++)
  {
    digitalWrite(GreenLed, HIGH);
    delay(400);
    digitalWrite(GreenLed, LOW);
    delay(400);
  }
}

void toggleRedLed()
{
  for (int i = 0; i < 6; i++)
  {
    digitalWrite(RedLed, HIGH);
    delay(400);
    digitalWrite(RedLed, LOW);
    delay(400);
  }
}

bool feedgps()
{
  while (nss.available())
  {
    if (gps.encode(nss.read()))
      return true;
  }
  return false;
}

