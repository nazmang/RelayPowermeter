// Node ID
#define MY_NODE_ID 2

// Enable debug prints to serial monitor
#define MY_DEBUG 

// Enable and select radio type attached
#define MY_RADIO_NRF24
//#define MY_RADIO_RFM69

// Enabled repeater feature for this node
#define MY_REPEATER_FEATURE

#include <SPI.h>
#include <MySensors.h>
#include <Bounce2.h>
#include <TimeLib.h> 
#include <DS3232RTC.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define RELAY_PIN  4  // Arduino Digital I/O pin number for relay 
#define BUTTON_PIN  3  // Arduino Digital I/O pin number for button 
#define POWERMETER_PIN A2
#define CHILD_ID_RELAY 1   // Id of the sensor child
#define CHILD_ID_POWER 2   // Id of powermeter
#define CHILD_ID_TEMP 3	   // Id of internal DS3231 temperature sensor	
#define RELAY_ON 1
#define RELAY_OFF 0


#define ACS712_05B 0.0264
#define ACS712_20B 0.0490
#define ACS712_30A 0.0742

class GenericHallEffectSensor
{
public:
	GenericHallEffectSensor() {};
	GenericHallEffectSensor(uint8_t id, float fact) { pin = id; factor = fact; }
	bool operator()(float trigger) { return readAcCurrent(100) >= trigger; }

	float readAcCurrent(uint16_t navg)
	{
		acc = 0;
		for (i = 0; i<navg; i++)
		{
			adc = analogRead(pin) - 512;
			acc += (adc*adc);
			delay(1);
		}
		return sqrt(acc / navg)*factor;
	}

	float readDcCurrent(uint16_t navg)
	{
		average = 0;
		for (i = 0; i<navg; i++)
		{
			average = average + (factor * analogRead(pin) - 13.513) / navg;
			delay(1);
		}
		return average;
	}

private:
	uint8_t pin;
	long acc, adc;
	float average;
	uint16_t i;
	/* Factor for:
	+-5 À (ACS712-05B) = 0.0264
	+-20 À (ACS712-20B) = 0.0490
	+-30 À (ACS712-30A) = 0.0742   */
	float factor;
};

GenericHallEffectSensor io_ACS1 = GenericHallEffectSensor(POWERMETER_PIN, ACS712_20B);

Bounce debouncer = Bounce(); 
int oldValue=0;
bool state;
float measure;
float threshold;
bool timeReceived = false;
unsigned long lastUpdate=0, lastRequest=0;

MyMessage msg(CHILD_ID_RELAY, V_STATUS);
MyMessage msg2(CHILD_ID_POWER, V_WATT);
MyMessage msg3(CHILD_ID_TEMP, V_TEMP);

// Initialize display. Google the correct settings for your display. 
// The follwoing setting should work for the recommended display in the MySensors "shop".
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

void setup()  
{  
  // Setup the button
  pinMode(BUTTON_PIN,INPUT);
  // Activate internal pull-up
  digitalWrite(BUTTON_PIN,HIGH);

  // the function to get the time from the RTC
  setSyncProvider(RTC.get);  

  // Request latest time from controller at startup
  requestTime();  

  threshold = 30.0;

  // After setting up the button, setup debouncer
  debouncer.attach(BUTTON_PIN);
  debouncer.interval(5);

  // Then set relay pins in output mode
  pinMode(RELAY_PIN, OUTPUT);
  // Make sure relays are off when starting up
  digitalWrite(RELAY_PIN, RELAY_OFF);
  

  // Set relay to last known state (using eeprom storage) 
  state = loadState(CHILD_ID_RELAY);
  digitalWrite(RELAY_PIN, state?RELAY_ON:RELAY_OFF);
  //send(msg.set(state ? false : true), true);

  // initialize the lcd for 16 chars 2 lines and turn on backlight
  lcd.begin(16,2); 
  lcd.clear();
}

void presentation()  {
  // Send the sketch version information to the gateway and Controller
  sendSketchInfo("Relay & Powermeter", "1.0");

  // Register all sensors to gw (they will be created as child devices)
  present(CHILD_ID_RELAY, S_BINARY);
  present(CHILD_ID_POWER, S_POWER);
  present(CHILD_ID_TEMP, S_TEMP);
  
}

// This is called when a new time value was received
void receiveTime(unsigned long controllerTime) {
	// Ok, set incoming time 
	Serial.print("Time value received: ");
	Serial.println(controllerTime);
	RTC.set(controllerTime); // this sets the RTC to the time from controller - which we do want periodically
	timeReceived = true;
}

/*
*  Example on how to asynchronously check for new messages from gw
*/
void loop() 
{
  debouncer.update();
  // Get the update value
  int value = debouncer.read();
  if (value != oldValue && value==0) {
      send(msg.set(state?false:true), true); // Send new state and request ack back
  }
  oldValue = value;

  measure = io_ACS1.readAcCurrent(1000);
  send(msg2.set(measure,2), false);
  send(msg3.set(RTC.temperature() / 4), false);

  unsigned long now = millis();
  // If no time has been received yet, request it every 10 second from controller
  // When time has been received, request update every hour
  if ((!timeReceived && (now-lastRequest) > (10UL*1000UL))
    || (timeReceived && (now-lastRequest) > (60UL*1000UL*60UL))) {
    // Request time from controller. 
    Serial.println("requesting time");
    requestTime();  
    lastRequest = now;
  }

  // Update display every second
  if (now-lastUpdate > 1000) {
    updateDisplay();  
    lastUpdate = now;
  }

  smartSleep(5000);

} 

void receive(const MyMessage &message) {
  // We only expect one type of message from controller. But we better check anyway.
  if (message.isAck()) {
     Serial.println(F("This is an ack from gateway"));
  }
 
  if (message.type == V_STATUS) {
     // Change relay state
     state = message.getBool();
     digitalWrite(RELAY_PIN, state?RELAY_ON:RELAY_OFF);
     // Store state in eeprom
     saveState(CHILD_ID_RELAY, state);

     // Write some debug info
     Serial.print(F("Incoming change for sensor:"));
     Serial.print(message.sensor);
     Serial.print(", New status: ");
     Serial.println(message.getBool());
   } 
}

void printDigits(int digits) {
	if (digits < 10)
		lcd.print('0');
	lcd.print(digits);
}

void updateDisplay(){
  tmElements_t tm;
  RTC.read(tm);
  lcd.home();
  printDigits(tm.Hour);
  lcd.print(":");
  printDigits(tm.Minute);
  
  state?lcd.print(" Relay ON "): lcd.print(" Relay OFF");

  // Go to next line and print temperature
  lcd.setCursor ( 0, 1 );  
  lcd.print("t=");
  lcd.print(RTC.temperature()/4);
  lcd.write(223); // Degree-sign
  lcd.print("C");

  // Print current
  if (io_ACS1(threshold)) {
	  lcd.clear();
	  lcd.home();
	  lcd.println(F("ALARM!          "));
	  lcd.setCursor(0, 1);
	  lcd.println(F("Current too high"));
  }
  else {
	  measure = io_ACS1.readAcCurrent(1000);
	  //lcd.setCursor(1, 0);
	  lcd.print(" I=");
	  lcd.print(measure);
	  lcd.print("A");
  }

}