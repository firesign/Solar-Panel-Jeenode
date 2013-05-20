 /* 
 Solar-Panel-Jeenode
 Send out a radio packet every minute, consuming as little power as possible.
 Uses DHT22 temp & humidity sensors
 Code from Radioblip
 2012-05-09 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php 
 */

#include <JeeLib.h>
#include <avr/sleep.h>
#include "DHT.h"

#define DHTPIN 8
#define DHTTYPE DHT22   // DHT 22  (AM2302)

#define BLIP_NODE 5     // wireless node ID to use for sending blips
#define BLIP_GRP  212   // wireless net group to use for sending blips
//#define BLIP_ID   1   // set this to a unique ID to disambiguate multiple nodes
#define SEND_MODE 2     // set to 3 if fuses are e=06/h=DE/l=CE, else set to 2

DHT dht(DHTPIN, DHTTYPE);

int batteryPin = 0; // Analog 0
int adc;

struct {
	int temp;	  // temperature
	int hum;	  // humidity
	int batt;	  // 1/2 of battery level
} payload;

// this must be defined since we're using the watchdog for low-power waiting
ISR(WDT_vect) { Sleepy::watchdogEvent(); }

void setup() {
	dht.begin();

	analogReference(EXTERNAL);		// connect 3.4V to AREF (pin 21)
	pinMode (batteryPin, OUTPUT);

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
	if (isnan(t) || isnan(h)) {
		//Serial.println("Failed to read from DHT");
	} else {	
		  
		t = t * 10;
		tt = (int) t;
		h = h * 10;
		hh = (int) h;
		/* Serial.print("Humidity: "); 
		Serial.print(hh);
		Serial.print(" %\t");
		Serial.print("Temperature: "); 
		Serial.print(tt);
		Serial.println(" *C"); */
	}
	payload.temp = tt;
	payload.hum = hh;

	adc = analogRead(batteryPin);
	/* Serial.print("Voltage from divider: ");
	Serial.println(adc); */
	float reconstitutedV = (3.4 * adc) / 512;
	/* Serial.print("Actual Input Voltage: ");
	Serial.print(reconstitutedV);
	Serial.println(" Volts");
	Serial.println(""); */
	payload.batt = adc;

	sendPayload();
	Sleepy::loseSomeTime(60000); //wake up and report in every 2 minutes

	//delay(30000);
}
