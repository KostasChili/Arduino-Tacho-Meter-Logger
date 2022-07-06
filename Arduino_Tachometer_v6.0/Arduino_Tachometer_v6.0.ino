#include <RTClib.h>
#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <LiquidCrystal.h>
#include <SPI.h>
#include <SD.h>
#include<Wire.h>


#define interrupts() sei()
#define noInterrupts() cli()

//Sensor and periferals pins
const byte CS_PIN = 10;
const byte sensorPin = 8;
//buttons

const byte leftButton = A3;
const byte upButton = A2;
const byte downButton = A1;
const byte rightButton = A0;
const byte enterButton = 9;


//time keeping
unsigned long lastInt_time = 0;
unsigned long currentInt_time = 0;
unsigned long lastMillis = 0;
unsigned long buttonTimeStamp = 0;

//used for rpm calculations
unsigned long curCapt = 0;
unsigned long lastCapt = 0;
volatile unsigned long period = 0;

float rpm = 0;
bool intDebounce = 0;

//log
int logInterval = 0;
bool writeToSD = 0;
unsigned  int lasthour = 61 ; // συγκριση για if με την  τιμη του xronoskatagrafis
unsigned  int xronoskatagrafis = 0 ; //Δινεται απο το menu για (μετρηση ανα λεπτο,ωρα,μερα)

unsigned int year ; //για να γραφτει η τιμη του now.year και να μπει στην writedata
unsigned int month ;// ""   ""     now.month ""   ""
unsigned int day ;
unsigned int hour ;
unsigned int minute ;
unsigned int second ;


//screen pins and registers
const byte rs = 3, en = 4, d4 = 2, d5 = 5, d6 = 6, d7 = 7;
LiquidCrystal lcd (rs, en, d4, d5, d6, d7);

File file;
RTC_DS1307 rtc;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

void setup() {
  Serial.begin(57600);
  //............................................DO NOT F REMOVE..............................//
  pinmode();
  lastMillis=millis();
  SLR();//serial, lcd,rtc begin
  
  lastMillis = millis();
  tim1SetUp();
  lastMillis = millis();
  sdSetUp();
  lastMillis = millis();
  logInterval = loggSet();
}



int loggSet()
{
  bool setupLog = 0;
  bool willLog = 0;
  int indexer0 = 0;
  while (!willLog)
  {
    if (millis() - lastMillis >= 500)
    {
      lastMillis = millis();
      lcd.clear();
      lcd.setCursor(2, 0);
      lcd.print("Save in LOG?");
      lcd.setCursor(3, 1);
      if (indexer0 == 0)
        lcd.print(">YES     NO" );
      else if (indexer0 == 1)
        lcd.print("YES     >NO" );
    }

    if (digitalRead(leftButton) == LOW)
    {
      if (millis() - buttonTimeStamp >= 150)
      {
        buttonTimeStamp = millis();
        if (indexer0 > 0)
          indexer0 += -1;
        else
          indexer0 = 1;
      }
    }
    if (digitalRead(rightButton) == LOW)
    {
      if (millis() - buttonTimeStamp >= 150)
      {
        buttonTimeStamp = millis();
        if (indexer0 < 1)
          indexer0++;
        else
          indexer0 = 0;
      }
    }
    if (digitalRead(enterButton) == LOW)
    {
      if (millis() - buttonTimeStamp >= 150)
      {
        buttonTimeStamp = millis();
        if (indexer0 == 0)
          setupLog = 1;
        else
          setupLog == 0;
        willLog = 1;
      }
    }
  }

  if (setupLog)
  {
    bool logSet = 0;
    bool checkSD=0;
    int indexer = 0;
    lcd.clear();
    lcd.print("CHECK SD CARD!!!");
    lcd.setCursor(0,1);
    lcd.print("HIT ENTER");
    while(!checkSD)
    {
      if(digitalRead(enterButton)==LOW)
      {
        if(millis()-buttonTimeStamp>300)
        {
        checkSD=1;
        buttonTimeStamp=millis();
        }
      }
    }
    while (!logSet)
    {
      if (millis() - lastMillis >= 500)
      {
        lastMillis = millis();
        lcd.clear();
        lcd.setCursor(1, 0);
        lcd.print("Set log Period");
        lcd.setCursor(0, 1);
        if (indexer == 0)
        {
          lcd.print(">hour min sec");
        }
        else if (indexer == 1)
          lcd.print("hour >min sec");
        else if (indexer == 2)
          lcd.print("hour min >sec");
      }
      if (digitalRead(leftButton) == LOW)
      {
        if (millis() - buttonTimeStamp >= 150)
        {
          buttonTimeStamp = millis();
          if (indexer > 0)
            indexer += -1;
          else
            indexer = 2;
        }
      }
      if (digitalRead(rightButton) == LOW)
      {
        if (millis() - buttonTimeStamp >= 150)
        {
          buttonTimeStamp = millis();
          if (indexer < 2)
            indexer++;
          else
            indexer = 0;
        }
      }
      if (digitalRead(enterButton) == LOW)
      {
        if (millis() - buttonTimeStamp >= 150)
        {
          buttonTimeStamp = millis();
          logSet = 1;

        }
      }
    }
    writeToSD = 1;
    return indexer;
  }
  else if (!setupLog)
  {
    return 5;
  }

  //......................................................DONT EVEN THINK ABOUT IT.......................................//
  lcd.begin(16, 2);
  pinMode(sensorPin, INPUT_PULLUP); // set capture pin as input
  lastMillis = millis();
  tim1SetUp();
  lastMillis = millis();
  sdSetUp();
}


void tim1SetUp()
{
  TCCR1A = 0; //clear presets
  TCCR1B = 0; //clear presets
  TCCR1B |= (1 << CS12) | (0 << CS11) | (1 << CS10) | (1 << ICES1) | (1 << ICNC1); //rising edge noise canceler
  TIMSK1 |= (1 << ICIE1); //enable t1 capture interrupt
  TIFR1 |= (1 << ICF1); //enable capture
  TCCR1A |= (0 << WGM12) | (0 << WGM11) | (0 << WGM10); //normal mode
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Timer Set");
  while (millis() - lastMillis <= 1000)
  {
    //do nothing
  }
}



void sdSetUp()
{
  initializeSD();
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("SD is Set");
  while (millis() - lastMillis <= 1000)
  {
    //do nothing
  }
}

void loop()
{


  if (millis() - lastMillis > 1000)
  {
    storeData();
    rpmCalc();

    lastMillis = millis();

  }
}

void rpmCalc()
{
  cli();
  float tempPeriod = period;
  rpm = 120 / ((tempPeriod * 64) / 1000000);
  lcd.clear();
  lcd.setCursor(0,1);
  lcd.print("RPM : ");
  lcd.print(rpm);
  // storeData(rpm);
  sei();
}



ISR (TIMER1_CAPT_vect) {
  currentInt_time = millis(); //time stamp of the interrupt
  if (currentInt_time - lastInt_time > 2)
  {
    if (intDebounce)
    {
      //check saving SREG
      curCapt = ICR1;
      period = curCapt - lastCapt;
      lastCapt = curCapt;
      intDebounce = 0;
    }
    else
      intDebounce = 1;
  }
  lastInt_time = currentInt_time;
}

//.........................................SD.................................................//

void initializeSD()
{
  pinMode(CS_PIN, OUTPUT);
  if (SD.begin())
  {

  }
  else
  {
    lcd.clear();
    lcd.print("Enter SD Card");
    return;
  }
}

int createFile (char filename[])
{
  file = SD.open(filename, FILE_WRITE);

  if (file)
  {

    return 1;
  }
  else
  {

    return 0;
  }
}



int writeToFile(char text[])
{
  if (file)
  {
    file.print(text);
    lcd.setCursor(15, 1);
    lcd.print("S");
    return 1;
  }
  else
  {
    return 0;
  }
}

void closeFile()
{
  if (file)
  {
    file.close();
  }
}

void storeData()
{
  lastMillis = millis();
  if (writeToSD) // αν το datasavebutton εχει πατηθει
  {
    changeTimeinterval(); // εκχωρει τα δεδομενα του μενου σε διαφορες if αναλογως τι επελεξε ο χρηστης (ανα ωρα ή μερα ή λεπτο)
    if (lasthour != xronoskatagrafis) // συγκρινει Πχ τα λεπτα και αν εχει αλλαξει το λεπτο μπαινει στην ρουτινα
    {

      DateTime now = rtc.now(); //  εκχωρησει τιμων απο το rtc σε int μεταβλητες
      year = now.year(), DEC ;
      month = now.month(), DEC ;
      day = now.day(), DEC ;

      hour = now.hour(), DEC ;
      minute = now.minute(), DEC ;
      second = now.second(), DEC ;


      char text [16];// απαραιτηταα για την μετατροπη απο rtc data(int) σε char για εκτυπωση
      char year_ch [16] ;
      char month_ch [16] ;
      char day_ch [16] ;
      char hour_ch [16] ;
      char minute_ch [16] ;
      char second_ch [16] ;
      //create temp char array
      cli(); //stop all interrpupts
      itoa(year , year_ch, 10); // μετατροπη σε char
      itoa(month , month_ch, 10);
      itoa(day , day_ch, 10);
      itoa(hour , hour_ch, 10);
      itoa(minute , minute_ch, 10);
      itoa(second , second_ch, 10);
      itoa(rpm, text, 10); //convert rpm to text
      
      createFile("LOG.txt");  //if !file create it
      //write the rpm vale throught the temp variable to the sd file
      // γραφει την ημερομηνια ωρα και rpm
      writeToFile(day_ch);
      file.print("/");
      writeToFile(month_ch);
      file.print("/");
      writeToFile(year_ch);
      writeToFile("\t");
      writeToFile (hour_ch);
      file.print(":");
      writeToFile(minute_ch);
      file.print(":");
      writeToFile(second_ch);
      file.print("\t");
      writeToFile(text);
      file.print("\n");
      closeFile();
      lasthour = xronoskatagrafis ; //εκχωρει την τωρινη τιμη του λεπτου για να μην ξαναμπει στην ρουτινα χωρις να περασει ενα λεπτο
      sei();
    }
  }
}


//-----------------------------------------SD_END------------------------------------------//

//--------------------------------------INITIALIZATIONS-----------------------------------//
void pinmode()
{
  pinMode(sensorPin, INPUT_PULLUP); // set capture pin as input
  pinMode(upButton, INPUT_PULLUP); //button
  pinMode(downButton, INPUT_PULLUP); //button
  pinMode(leftButton, INPUT_PULLUP); //button
  pinMode(rightButton, INPUT_PULLUP); //button
  pinMode(enterButton, INPUT_PULLUP); //button
}
void SLR()
{
  Serial.begin(57600);
  lcd.begin(16, 2);
  if (! rtc.begin()) {
    Serial.flush();
    while (1) delay(10);
  }

  if (! rtc.isrunning()) {
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:    
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    manualSetTime();
  }

}




//-------------------------------END OF INIT------------------------------//


//-------------------------------RCT FUNCTIONS----------------------------//
void changeTimeinterval()
{
  DateTime now = rtc.now();
  if (logInterval == 0) // wra
  {
    xronoskatagrafis = now.hour(), DEC ;

  }
  else if (logInterval == 1) // lepto
  {

    xronoskatagrafis = now.minute(), DEC ;
  }
  else if (logInterval == 2) // deuterolepto
  {

    xronoskatagrafis = now.second(), DEC  ;
  }

}


void manualSetTime()
{
  bool dateTimeSet=0;
  bool yearSet=0;
  bool daySet=0;
  bool monthSet=0;
  bool hourSet=0;
  bool minuteSet=0;
  bool secondSet=0;
  lcd.clear();
  lcd.print("Set Date/Time");
  lcd.setCursor(0,1);
  unsigned int tempYear=2022;
  lcd.print("YEAR : ");
  while(!yearSet)
  {
    lcd.setCursor(10,1);
    lcd.print(tempYear);
    if(digitalRead(upButton)==LOW)
    {
      if(millis()-buttonTimeStamp>=300)
      {
      tempYear++;
      buttonTimeStamp=millis();
      }
    }
    else if(digitalRead(downButton)==LOW)
    {
      if(millis()-buttonTimeStamp>=300)
      {
      buttonTimeStamp=millis();
      tempYear+=-1;
      }
    }
    if(digitalRead(enterButton)==LOW)
    {
      if(millis()-buttonTimeStamp>=500)
      {
      yearSet=1;
      buttonTimeStamp=millis();
      }
    }
  }//year while
  unsigned int tempMonth=1;
   while(!monthSet)
  {
    if(millis()-lastMillis>=300)
    {
    lcd.clear();
    lcd.print("Set Date/Time");
    lcd.setCursor(0,1);
    lcd.print("MONTH : ");
    lcd.setCursor(10,1);
    lcd.print(tempMonth);
    lastMillis=millis();

    }
    if(digitalRead(upButton)==LOW)
    {
      if(millis()-buttonTimeStamp>=300)
      {
       if(tempMonth<12)
       {
        tempMonth++;
       }
       else if(tempMonth==12)
       {
        tempMonth=1;
       }
      buttonTimeStamp=millis();
      }
    }
    else if(digitalRead(downButton)==LOW)
    {
      if(millis()-buttonTimeStamp>=300)
      {
        if(tempMonth>1)
        {
          tempMonth+=-1;
        }
        else if(tempMonth==1)
        tempMonth=12;
        buttonTimeStamp=millis();
      }
    }
    if(digitalRead(enterButton)==LOW)
    {
      if(millis()-buttonTimeStamp>=400)
      {
      monthSet=1;
      buttonTimeStamp=millis();
      }
    }
  }//month while
  unsigned int tempDay=1;
     while(!daySet)
  {
    if(millis()-lastMillis>=300)
    {
    lcd.clear();
    lcd.print("Set Date/Time");
    lcd.setCursor(0,1);
    lcd.print("Day : ");
    lcd.setCursor(10,1);
    lcd.print(tempDay);
    lastMillis=millis();

    }
    if(digitalRead(upButton)==LOW)
    {
      if(millis()-buttonTimeStamp>=300)
      {
       if(tempDay<31)
       {
        tempDay++;
       }
       else if(tempDay==31)
       {
        tempDay=1;
       }
      buttonTimeStamp=millis();
      }
    }
    else if(digitalRead(downButton)==LOW)
    {
      if(millis()-buttonTimeStamp>=300)
      {
        if(tempDay>1)
        {
          tempDay+=-1;
        }
        else if(tempDay==1)
        tempDay=31;
        buttonTimeStamp=millis();
      }
    }
    if(digitalRead(enterButton)==LOW)
    {
      if(millis()-buttonTimeStamp>=400)
      {
      daySet=1;
      buttonTimeStamp=millis();
      }
    }
  }//day while
  unsigned int tempHour=0;
     while(!hourSet)
  {
    if(millis()-lastMillis>=300)
    {
    lcd.clear();
    lcd.print("Set Date/Time");
    lcd.setCursor(0,1);
    lcd.print("HOUR : ");
    lcd.setCursor(10,1);
    lcd.print(tempHour);
    lastMillis=millis();

    }
    if(digitalRead(upButton)==LOW)
    {
      if(millis()-buttonTimeStamp>=300)
      {
       if(tempHour<24)
       {
        tempHour++;
       }
       else if(tempHour==24)
       {
        tempHour=0;
       }
      buttonTimeStamp=millis();
      }
    }
    else if(digitalRead(downButton)==LOW)
    {
      if(millis()-buttonTimeStamp>=300)
      {
        if(tempHour>0)
        {
          tempHour+=-1;
        }
        else if(tempHour==0)
        tempHour=24;
        buttonTimeStamp=millis();
      }
    }
    if(digitalRead(enterButton)==LOW)
    {
      if(millis()-buttonTimeStamp>=400)
      {
      hourSet=1;
      buttonTimeStamp=millis();
      }
    }
  }//hour while
  unsigned int tempMinute=0;
     while(!minuteSet)
  {
    if(millis()-lastMillis>=300)
    {
    lcd.clear();
    lcd.print("Set Date/Time");
    lcd.setCursor(0,1);
    lcd.print("Minute : ");
    lcd.setCursor(10,1);
    lcd.print(tempMinute);
    lastMillis=millis();

    }
    if(digitalRead(upButton)==LOW)
    {
      if(millis()-buttonTimeStamp>=300)
      {
       if(tempMinute<60)
       {
        tempMinute++;
       }
       else if(tempMinute==60)
       {
        tempMinute=0;
       }
      buttonTimeStamp=millis();
      }
    }
    else if(digitalRead(downButton)==LOW)
    {
      if(millis()-buttonTimeStamp>=300)
      {
        if(tempMinute>0)
        {
          tempMinute+=-1;
        }
        else if(tempMinute==0)
        tempMinute=60;
        buttonTimeStamp=millis();
      }
    }
    if(digitalRead(enterButton)==LOW)
    {
      if(millis()-buttonTimeStamp>=400)
      {
      minuteSet=1;
      buttonTimeStamp=millis();
      }
    }
  }//minute while
  unsigned int tempSecond=0;
     while(!secondSet)
  {
    if(millis()-lastMillis>=300)
    {
    lcd.clear();
    lcd.print("Set Date/Time");
    lcd.setCursor(0,1);
    lcd.print("SECOND : ");
    lcd.setCursor(10,1);
    lcd.print(tempSecond);
    lastMillis=millis();

    }
    if(digitalRead(upButton)==LOW)
    {
      if(millis()-buttonTimeStamp>=300)
      {
       if(tempSecond<60)
       {
        tempSecond++;
       }
       else if(tempSecond==60)
       {
        tempSecond=0;
       }
      buttonTimeStamp=millis();
      }
    }
    else if(digitalRead(downButton)==LOW)
    {
      if(millis()-buttonTimeStamp>=300)
      {
        if(tempSecond>0)
        {
          tempSecond+=-1;
        }
        else if(tempSecond==0)
        tempSecond=60;
        buttonTimeStamp=millis();
      }
    }
    if(digitalRead(enterButton)==LOW)
    {
      if(millis()-buttonTimeStamp>=400)
      {
      secondSet=1;
      buttonTimeStamp=millis();
      }
    }
  }//second while



 rtc.adjust((DateTime(tempYear,tempDay,tempMonth,tempHour,tempMinute,tempSecond)));

}
