
//#define   F_CPU   1000000UL
#include <util/delay.h>
#include <avr/sleep.h> //Needed for sleep_mode
#include <avr/wdt.h> //Needed to enable/disable watch dog timer


//#include <SoftwareSerial.h>

//SoftwareSerial mySerial(2, 1); // RX, TX

//Pin definitions for regular Arduino Uno (used during development)
/*
const byte buzzer1 = 8;
const byte statLED = 13;
const byte waterSensor = A0;
const byte waterSensorPower = 7; //the digital pin for the moisture probe power (you power intermittent as to reduce corrosion)
*/
 
 
const byte buzzer1 = 1;//5;//1; //the digital pin for the piezo speaker
const byte statLED = 3;//6;//0; // for indications
const byte waterSensor = 2;//A4;//4; //the analog pin for the moisture probe (take reading only after turning on power pin)
const byte waterSensorPower = 0; //the digital pin for the moisture probe power (you power intermittent as to reduce corrosion)
//int watchdog_counter = 0;


 

//Pin definitions for ATtiny
/*const byte buzzer1 = 0;
const byte buzzer2 = 1;
const byte statLED = 4;
const byte waterSensor = A1;*/

//Variables

//This is the average analog value found during startup. Usually ~995
//When hit with water, the analog value will drop to ~400. A diff of 100 is good.
int minWetness = 0; 
//int maxDifference = 0; //A diff of more than 100 in the analog value will trigger the system.

  int timesWithoutAlarm = 0; //track number of times havent had an issue so can go deeper
  int timesBeforeDeepSleep = 5; //number of times to not have an issue before going deeper



//This runs each time the watch dog wakes us up from sleep
ISR(WDT_vect) {
  //Don't do anything. This is just here so that we wake up.
  //watchdog_counter++;
}


//Read sensor
int readSensorAvg(){
  int wetness = 0;
  
  //boot sensor power
  digitalWrite(waterSensorPower,HIGH);
  delay(50); //stabalise, may not be needed
  for(int x = 0 ; x < 8 ; x++)
    {
      wetness += analogRead(waterSensor);
  
      //During power up, blink the LED to let the world know we're alive
      if(digitalRead(statLED) == LOW)
        digitalWrite(statLED, HIGH);
      else
        digitalWrite(statLED, LOW);
  
      delay(50);
    }
    digitalWrite(waterSensorPower,LOW);
  wetness /= 8;
  wetness = map(wetness, 0, 1023, 0, 100); 
  return wetness;
}




long readVcc() {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  //#if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  //  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  //#elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
  //  ADMUX = _BV(MUX5) | _BV(MUX0);
  //#elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADMUX = _BV(MUX3) | _BV(MUX2);
  //#else
  //  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  //#endif  

  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH; // unlocks both

  long result = (high<<8) | low;

  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}







void setup()
{
  
  //Setup the processor speed
  //This doesn't work now, set via bootloader
  /*
  cli();
  CLKPR = (1<<CLKPCE);
  CLKPR = (0<<CLKPS0);
  CLKPR = (0<<CLKPS1);
  CLKPR = (1<<CLKPS2);
  CLKPR = (0<<CLKPS3);
  sei();
  */
  
  //Configure IO
  pinMode(buzzer1, OUTPUT);
  pinMode(statLED, OUTPUT);
  pinMode(waterSensorPower, OUTPUT);

  //pinMode(waterSensor, INPUT_PULLUP);
  pinMode(2, INPUT); //When setting the pin mode we have to use 2 instead of A1
  digitalWrite(2, HIGH); //Hack for getting around INPUT_PULLUP

  //mySerial.begin(9600);
  //mySerial.println("Wet Sensor");

  //Take a series of readings from the water sensor and average them
  minWetness = readSensorAvg(); //10 is buffer

  //mySerial.print("minWetness: ");
  //mySerial.println(minWetness);

  //During power up, beep the buzzer to verify function
  alarmSound();
  //delay(100);
  //alarmSound();

  digitalWrite(buzzer1, LOW);
  digitalWrite(statLED, LOW);
  

  //Power down various bits of hardware to lower power usage  
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); //Power down everything, wake up from WDT
  sleep_enable();

}

void loop() 
{


  
  //Check for water
  ADCSRA |= (1<<ADEN); //Enable ADC

  
  
  int wetnessRead = readSensorAvg();

  
  //mySerial.print("minWetness: ");
  //mySerial.print(minWetness);
  //mySerial.print("Now: ");
  //mySerial.println(wetnessRead);
  

  
  
  if(wetnessRead <  minWetness+15) //Are we too Dry? 5 buffer as may not get as dry
  
  {
    wdt_disable(); //Turn off the WDT!!
    
    long startTime = millis(); //Record the current time
    long timeSinceBlink = millis(); //Record the current time for blinking
    timesWithoutAlarm=0; //alarm was triggered, track it
    digitalWrite(statLED, HIGH); //Start out with the LED on

    //Loop until we don't detect DRYNESS AND 2 seconds of CHIRP have completed
    while((millis() - startTime) < 2000)
    {
      alarmSound(); //Make noise!!

      if(millis() - timeSinceBlink > 100) //Toggle the LED every 100ms
      {
        timeSinceBlink = millis();

        if(digitalRead(statLED) == LOW) 
          digitalWrite(statLED, HIGH);
        else
          digitalWrite(statLED, LOW);
      }
      
      //sleep here for shorter time
      //use delay for now TODO:REPLACE!
      //delay(5000);
      
      

      
      

      //waterDifference = abs(analogRead(waterSensor) - minWetness); //Take a new reading
      //wetnessRead = analogRead(waterSensor);
      
      //mySerial.print("minWetness: ");
      //mySerial.print(minWetness);
      //mySerial.print("Now: ");
      //mySerial.println(wetnessRead);
    } //Loop until we don't detect water AND 2 seconds of alarm have completed

    digitalWrite(buzzer1, LOW);
    digitalWrite(statLED, LOW); //No more alarm. Turn off LED
  } else {
    timesWithoutAlarm++; //didn't detect dryness,
  
  }
  
 
 
     //BATTERY WARNING: Chirp about battery warning here
     //Push out after thourough testing, would cause annoyance if wrong
      if (readVcc() < 2100){ //check voltage, Attiny stops at 1800mV, alarm less than 2100mV //set to 2800 for AtTiny85-20PU, set to 2100mV for AtTiny85V-10PU
        tweet(random(2,6),2);
      }
  
  //go to sleep for a bit
  
 //if this is the first bit, only go to sleep a little


//if havent alarmed for last 6 times go deep 
  
  if (timesWithoutAlarm < timesBeforeDeepSleep){
    //lite sleep
      ADCSRA &= ~(1<<ADEN); //Disable ADC, saves ~230uA
      setup_watchdog(9); //Setup watchdog to go off after 1sec
      sleep_mode(); //Go to sleep! Wake up 1sec later and check water
  
  }else{

    
    
    //deep sleep
    for (int i=0; i <= 6; i++){ //i is number of 9 second groups
        ADCSRA &= ~(1<<ADEN); //Disable ADC, saves ~230uA
        setup_watchdog(9); //Setup watchdog to go off after 1sec
        sleep_mode(); //Go to sleep! Wake up 1sec later and check water
    }
  }
  
  


  
  

      

      
      
      
      
  

}

//This is just a unique (annoying) sound we came up with, there is no magic to it
//Comes from the Simon Says game/kit actually: https://www.sparkfun.com/products/10547
//250us to 79us
void alarmSound(void)
{

  
  
   highChirp(random(5,9),random(3,8));
   delay(random(50,120));
   lowChirp(random(20,50)*4,random(2,5));
   delay(random(100));
   //tweet(random(2,6),2);
  lowChirp(random(20,50)*4,2);
  
  
}



void highChirp(int intensity, int chirpsNumber){
  int i;
  int x;
  
  for(int veces=0; veces<=chirpsNumber; veces++){
    for (i=100; i>0; i--){
     for  (x=0; x<intensity;  x++){
       digitalWrite (buzzer1,HIGH);
       delayMicroseconds (i);
       digitalWrite (buzzer1,LOW);
       delayMicroseconds (i);
     }
    }
  }
}

void lowChirp(int intensity, int chirpsNumber){
int i;
int x;

for(int veces=0; veces<=chirpsNumber; veces++){

for (i=0; i<200; i++)
{
   digitalWrite (buzzer1,HIGH);
   delayMicroseconds(i);
   digitalWrite(buzzer1,LOW);
   delayMicroseconds(i);
  }
   
  for (i=90; i>80; i--)
  {
   for  ( x=0; x<5;  x++)
   {
   digitalWrite (buzzer1,HIGH);
   delayMicroseconds (i);
   digitalWrite (buzzer1,LOW);
   delayMicroseconds (i);
   }
  }
}
}

void tweet(int intensity, int chirpsNumber){
  
  int i;
  int x;
  
  //normal chirpsNumber 3, normal intensity 5
  
  for(int veces=0; veces<chirpsNumber; veces++){
      
    for (int i=80; i>0; i--){
     for  (int x=0; x<intensity;  x++){
       digitalWrite (buzzer1,HIGH);
       delayMicroseconds (i);
       digitalWrite (buzzer1,LOW);
       delayMicroseconds (i);
     }
    }
  }
}






//Sets the watchdog timer to wake us up, but not reset
//0=16ms, 1=32ms, 2=64ms, 3=128ms, 4=250ms, 5=500ms
//6=1sec, 7=2sec, 8=4sec, 9=8sec
//From: http://interface.khm.de/index.php/lab/experiments/sleep_watchdog_battery/
void setup_watchdog(int timerPrescaler) {

  if (timerPrescaler > 9 ) timerPrescaler = 9; //Limit incoming amount to legal settings

  byte bb = timerPrescaler & 7; 
  if (timerPrescaler > 7) bb |= (1<<5); //Set the special 5th bit if necessary

  //This order of commands is important and cannot be combined
  MCUSR &= ~(1<<WDRF); //Clear the watch dog reset
  WDTCR |= (1<<WDCE) | (1<<WDE); //Set WD_change enable, set WD enable
  WDTCR = bb; //Set new watchdog timeout value
  WDTCR |= _BV(WDIE); //Set the interrupt enable, this will keep unit from resetting after each int
}
