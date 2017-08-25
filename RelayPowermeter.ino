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
//#include <LiquidCrystal_I2C.h>

#define RELAY_PIN  4  // Arduino Digital I/O pin number for relay 
#define BUTTON_PIN  3  // Arduino Digital I/O pin number for button 
#define CHILD_ID 1   // Id of the sensor child
#define RELAY_ON 1
#define RELAY_OFF 0

Bounce debouncer = Bounce(); 
int oldValue=0;
bool state;

bool timeReceived = false;
unsigned long lastUpdate=0, lastRequest=0;

MyMessage msg(CHILD_ID,V_LIGHT);

// Initialize display. Google the correct settings for your display. 
// The follwoing setting should work for the recommended display in the MySensors "shop".
//LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

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

  // After setting up the button, setup debouncer
  debouncer.attach(BUTTON_PIN);
  debouncer.interval(5);

  // Make sure relays are off when starting up
  digitalWrite(RELAY_PIN, RELAY_OFF);
  // Then set relay pins in output mode
  pinMode(RELAY_PIN, OUTPUT);   

  // Set relay to last known state (using eeprom storage) 
  state = loadState(CHILD_ID);
  digitalWrite(RELAY_PIN, state?RELAY_ON:RELAY_OFF);

  // initialize the lcd for 16 chars 2 lines and turn on backlight
  //lcd.begin(16,2); 
}

void presentation()  {
  // Send the sketch version information to the gateway and Controller
  sendSketchInfo("Relay & Powermeter", "1.0");

  // Register all sensors to gw (they will be created as child devices)
  present(CHILD_ID, S_LIGHT);
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
  /*if (now-lastUpdate > 1000) {
    updateDisplay();  
    lastUpdate = now;
  }*/
    
} 

void receive(const MyMessage &message) {
  // We only expect one type of message from controller. But we better check anyway.
  if (message.isAck()) {
     Serial.println("This is an ack from gateway");
  }

  if (message.type == V_LIGHT) {
     // Change relay state
     state = message.getBool();
     digitalWrite(RELAY_PIN, state?RELAY_ON:RELAY_OFF);
     // Store state in eeprom
     saveState(CHILD_ID, state);

     // Write some debug info
     Serial.print("Incoming change for sensor:");
     Serial.print(message.sensor);
     Serial.print(", New status: ");
     Serial.println(message.getBool());
   } 
}

/*
void updateDisplay(){
  tmElements_t tm;
  RTC.read(tm);

  // Print date and time 
  lcd.home();
  lcd.print(tm.Day);
  lcd.print("/");
  lcd.print(tm.Month);
//  lcd.print(" ");
//  lcd.print(tmYearToCalendar(tm.Year)-2000);

  lcd.print(" ");
  printDigits(tm.Hour);
  lcd.print(":");
  printDigits(tm.Minute);
  lcd.print(":");
  printDigits(tm.Second);

  // Go to next line and print temperature
  lcd.setCursor ( 0, 1 );  
  lcd.print("Temp: ");
  lcd.print(RTC.temperature()/4);
  lcd.write(223); // Degree-sign
  lcd.print("C");
}


void printDigits(int digits){
  if(digits < 10)
    lcd.print('0');
  lcd.print(digits);
}*/
