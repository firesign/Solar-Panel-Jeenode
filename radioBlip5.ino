 /* 
 Solar-Panel-Jeenode
 Send out a radio packet every minute, consuming as little power as possible.
 Uses DHT22 temp & humidity sensor, DS1820 temperature sensor for battery pack temp
 Code from Radioblip
 2012-05-09 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php
 
 Battery monitor uses a 100k resistor divider through a 4066 digital switch so that the
 divider does not draw current while the rest of the system is powered down. The
 digital switch is driven through DIO7.
 */

#include <JeeLib.h>
#include <avr/sleep.h>
#include "DHT.h"
#include <OneWire.h>
#include <DallasTemperature.h>

#define DHTPIN 8
#define DHTTYPE DHT22   // DHT 22  (AM2302)

#define BLIP_NODE 5     // wireless node ID to use for sending blips
#define BLIP_GRP  212   // wireless net group to use for sending blips
//#define BLIP_ID   1   // set this to a unique ID to disambiguate multiple nodes
#define SEND_MODE 2     // set to 3 if fuses are e=06/h=DE/l=CE, else set to 2


#define ONE_WIRE_BUS 4	// Data wire is plugged into DIO4 on the Arduino
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);	// Pass our oneWire reference to Dallas Temperature. 

DHT dht(DHTPIN, DHTTYPE);

int rdivider = 7;	// Resistor divider enable to 4066
int batteryPin = 0; 	// Analog 0
int adc;

// Charge control
boolean chargeState = true;		// is it charging?
unsigned long solarOff;			// time of charging pause start, in ms
unsigned long pause = 60000 * 4;	// pause charging for 8 minutes
int chargeStatePin = 6;			// an LED to light if we are charging batteries


struct {
	int temp;	// temperature
	int hum;	// humidity
	int batt;	// 1/2 of battery level
	int batttemp;	// battery pack temperature	
} payload;

// this must be defined since we're using the watchdog for low-power waiting
ISR(WDT_vect) { Sleepy::watchdogEvent(); }

void setup() {
    Serial.begin(57600);
    pinMode(chargeStatePin,OUTPUT);
    digitalWrite(chargeStatePin, 1);
    delay(5);
    Serial.println("Starting");
    dht.begin();			// DHT22
    sensors.begin();			// DS1820

    analogReference(EXTERNAL);		// connect 3.4V to AREF (pin 21)
    pinMode (batteryPin, INPUT);	// set up battery level input
    pinMode (rdivider, OUTPUT);		// set up battery sense enable

    rf12_initialize(BLIP_NODE, RF12_433MHZ, BLIP_GRP);
    // see http://tools.jeelabs.org/rfm12b
    rf12_control(0xC040); // set low-battery level to 2.2V i.s.o. 3.1V
    rf12_sleep(RF12_SLEEP);
	
}

static byte sendPayload () {
	rf12_sleep(RF12_WAKEUP);
	while (!rf12_canSend())
	rf12_recvDone();
	rf12_sendStart(0, &payload, sizeof payload);
	rf12_sendWait(SEND_MODE);
	rf12_sleep(RF12_SLEEP);
  
}


void loop() {
 
    // DHT SECTION ******************************
    int tt, hh;
    float h = dht.readHumidity();
    delay(200);
    float t = dht.readTemperature();
    delay(200);
    digitalWrite(rdivider, HIGH);		// enable resistor divider
    if (isnan(t) || isnan(h)) {
	Serial.println("Failed to read from DHT");
    } else {	
	t = t * 10;
	tt = (int) t;
	h = h * 10;
	hh = (int) h;
	//Serial.print("Humidity: "); 
	//Serial.print(hh);
	//Serial.print(" %\t");
	//Serial.print("Temperature: "); 
	//Serial.print(tt);
	//Serial.println(" *C");
    }
    payload.temp = tt;
    payload.hum = hh;


    //delay(2);				// wait 2ms
    adc = analogRead(batteryPin);		// get battery level
    digitalWrite(rdivider, LOW);		// disable resistor divider
    //Serial.print("Voltage from divider: ");
    //Serial.println(adc);
    float reconstitutedV = (3.4 * adc) / 512;
    //Serial.print("Actual Input Voltage: ");
    //Serial.print(reconstitutedV);
    //Serial.println(" Volts");
    //Serial.println(""); 
    payload.batt = adc;
    
    sensors.requestTemperatures(); // Send the command to DS1820 get temperatures
    payload.batttemp = sensors.getTempCByIndex(0) * 100;	// in Celcius * 100
    //Serial.print("Battery Temperature: ");
    //Serial.println(payload.batttemp);
    sendPayload();
    
    
    // CHARGE CONTROL
    // if not charging, and still too hot, reset pause timer
    if ((chargeState == false) && (payload.batttemp > 3500)) {
	delay(10);
	Serial.println("chargestate: false and battery temp is greater than 3500");
	solarOff = millis();    // start pause timer
	printBattTemp();    
    } 
    
    // if not charging, but temp has fallen, count down timer
    if ((chargeState == false) && (payload.batttemp <= 3499)) {
	delay(10);
	Serial.println("chargestate: false and battery temp is less than 3499");
	unsigned long currentMillis = millis();
	if (currentMillis > (pause + solarOff)) {      // if timer countdown complete, allow charging to resume
	    chargeState = true;
	    digitalWrite(chargeStatePin, 1);
	    Serial.println("Charging ON");
	}
	printBattTemp(); 
    }
  
    // if charging, and temp is too high, stop charging and start countdown timer
    if ((chargeState == true) && (payload.batttemp > 3500)) {
	delay(10);
	Serial.println("chargestate: true and battery temp is greater than 3500");
	chargeState = false;
	digitalWrite(chargeStatePin, 0);
	Serial.println("Charging OFF");
	solarOff = millis();    // start pause timer
	Serial.print("solarOff: ");
	Serial.println(solarOff);
	printBattTemp();
    } 
  
    // if charging, and battery temp is ok, then report temp
    if ((chargeState == true) && (payload.batttemp <= 3499)) {
	delay(10);
	Serial.println("chargestate: true and battery temp is less than 3499");
	printBattTemp();
    } 
	Sleepy::loseSomeTime(60000); //wake up and report in every 2 minutes
	//Sleepy::loseSomeTime(30000); //wake up and report in every 1 minute
	//delay(30000);
}

void printBattTemp() {
    delay(10);
    Serial.print("Battery Temperature: ");
    Serial.println(payload.batttemp);
    delay(10);
}