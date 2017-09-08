/*
 * Homie for SonOff relay.
 * Key features:
 * - countdown timer
 * - initial state configuration of relay
 * - keepalive from server
 */

#define FW_NAME "homie-sonoff"
#define FW_VERSION "2.12"

/* Magic sequence for Autodetectable Binary Upload */
const char *__FLAGGED_FW_NAME = "\xbf\x84\xe4\x13\x54" FW_NAME "\x93\x44\x6b\xa7\x75";
const char *__FLAGGED_FW_VERSION = "\x6a\x3f\x3e\x0e\xe1" FW_VERSION "\xb0\x30\x48\xd4\x1a";
/* End of magic sequence for Autodetectable Binary Upload */

#include <Homie.h>
#include <EEPROM.h>
#include <Bounce2.h>

const int PIN_RELAY = 12;
const int PIN_LED = 13;
const int PIN_BUTTON = 0;

unsigned long downCounterStart;
unsigned long downCounterLimit=0;

unsigned long keepAliveReceived=0;
int lastButtonValue = 1;

// EEPROM structure
struct EEpromDataStruct {
  int initialState; // 1 - ON, other - OFF
  int keepAliveValue; // 0 - disabled, keepalive time - seconds
};

EEpromDataStruct EEpromData;

Bounce debouncerButton = Bounce();

HomieNode relayNode("relay01", "relay");
HomieNode keepAliveNode("keepalive", "keepalive");

bool relayState(const String& value) {
  if (value == "ON") {
    digitalWrite(PIN_RELAY, HIGH);
    digitalWrite(PIN_LED, LOW);
    //Homie.setNodeProperty(relayNode, "relayState", "ON");
    relayNode.setProperty("relayState").send("ON");
    //Serial.println("Switch is on");
  } else if (value == "OFF") {
    digitalWrite(PIN_RELAY, LOW);
    digitalWrite(PIN_LED, HIGH);
    //Homie.setNodeProperty(relayNode, "relayState", "OFF");
    relayNode.setProperty("relayState").send("OFF");
    //Serial.println("Switch is off");
  } else {
    return false;
  }
  return true;
}

// Event typu relay - manualne wymuszenie stanu
bool relayStateHandler(const HomieRange& range, const String& value) {
  relayState(value);
}

// Keepliave tick handler
bool keepAliveTickHandler(const HomieRange& range, const String& value)
{
  keepAliveReceived=millis();
  return true;
}

// Keepalive mode
bool keepAliveValueHandler(const HomieRange& range, const String& value)
{
  int oldValue = EEpromData.keepAliveValue;
  if (value.toInt() > 0)
  {
    EEpromData.keepAliveValue = value.toInt();
  }
  if (value=="0")
  {
    EEpromData.keepAliveValue = 0;
  }
  if (oldValue!=EEpromData.keepAliveValue)
  {
    EEPROM.put(0, EEpromData);
    EEPROM.commit();
  }
}

// Czasowe wymuszenie stanu
bool relayTimerHandler(const HomieRange& range, const String& value)
{
  if (value.toInt() > 0)
  {
    digitalWrite(PIN_RELAY, HIGH);
    digitalWrite(PIN_LED, LOW);
    downCounterStart = millis();
    downCounterLimit = value.toInt()*1000;
    // Homie.setNodeProperty(relayNode, "relayState", "ON");
    relayNode.setProperty("relayState").send("ON");
    //Homie.setNodeProperty(relayNode, "relayTimer", value);
    relayNode.setProperty("relayState").send(value);
    return true;
  } else {
    return false;
  }
}


// Initial mode handler
bool relayInitModeHandler(const HomieRange& range, const String& value)
{
  int oldValue = EEpromData.initialState;
  if (value.toInt() == 1 or value=="ON")
  {
    //Homie.setNodeProperty(relayNode, "relayInitMode", "1");
    relayNode.setProperty("relayInitMode").send("1");
    EEpromData.initialState=1;
  } else {
    //Homie.setNodeProperty(relayNode, "relayInitMode", "0");
    relayNode.setProperty("relayInitMode").send("0");
    EEpromData.initialState=0;
  }
  if (oldValue!=EEpromData.initialState)
  {
    EEPROM.put(0, EEpromData);
    EEPROM.commit();
  }
  return true;
}

// Homie setup handler
void setupHandler()
{

  if (EEpromData.initialState==1)
  {
    //Homie.setNodeProperty(relayNode, "relayState", "ON");
    relayNode.setProperty("relayState").send("ON");
    //Homie.setNodeProperty(relayNode, "relayInitMode", "1");
    relayNode.setProperty("relayInitMode").send("1");
  } else {
    //Homie.setNodeProperty(relayNode, "relayState", "OFF");
    relayNode.setProperty("relayState").send("OFF");
    //Homie.setNodeProperty(relayNode, "relayInitMode", "0");
    relayNode.setProperty("relayInitMode").send("0");
  }
  String outMsg = String(EEpromData.keepAliveValue);
  //Homie.setNodeProperty(keepAliveNode, "keepAliveValue", outMsg);
  keepAliveNode.setProperty("keepAliveValue").send(outMsg);
  keepAliveReceived=millis();
}

// Homie loop handler
void loopHandler()
{
  if (downCounterLimit>0)
  {
    if ((millis() - downCounterStart ) > downCounterLimit)
    {
      // Turn off relay
      digitalWrite(PIN_RELAY, LOW);
      digitalWrite(PIN_LED, HIGH);
      //Homie.setNodeProperty(relayNode, "relayState", "OFF");
      relayNode.setProperty("relayState").send("OFF");
      //Homie.setNodeProperty(relayNode, "relayTimer", "0");
      relayNode.setProperty("relayTimer").send("0");
      downCounterLimit=0;
    }
  }
  int buttonValue = debouncerButton.read();

  if (buttonValue != lastButtonValue)
  {
    lastButtonValue = buttonValue;
    int relayValue = digitalRead(PIN_RELAY);
    if (buttonValue == HIGH)
    {
      relayState(!relayValue ? "ON" : "OFF");
    }
  }
  debouncerButton.update();

  // Check if keepalive is supported and expired
  if (EEpromData.keepAliveValue != 0 && (millis() - keepAliveReceived) > EEpromData.keepAliveValue*1000 )
  {
    ESP.restart();
  }
}

// Homie event
void onHomieEvent(const HomieEvent& event) {
  switch(event.type) {
    case HomieEventType::CONFIGURATION_MODE: // Default eeprom data in configuration mode
      digitalWrite(PIN_RELAY, LOW);
      EEpromData.initialState=0;
      EEpromData.keepAliveValue = 0;
      EEPROM.put(0, EEpromData);
      EEPROM.commit();
      break;
    //case HOMIE_NORMAL_MODE:
    case HomieEventType::NORMAL_MODE:
      // Do whatever you want when normal mode is started
      break;
    case HomieEventType::OTA_STARTED:
      // Do whatever you want when OTA mode is started
      digitalWrite(PIN_RELAY, LOW);
      break;
    case HomieEventType::ABOUT_TO_RESET:
      // Do whatever you want when the device is about to reset
      break;
    case HomieEventType::WIFI_CONNECTED:
      // Do whatever you want when Wi-Fi is connected in normal mode
      break;
    case HomieEventType::WIFI_DISCONNECTED:
      // Do whatever you want when Wi-Fi is disconnected in normal mode
      break;
    case HomieEventType::MQTT_READY:
      // Do whatever you want when MQTT is connected in normal mode
      break;
    case HomieEventType::MQTT_DISCONNECTED:
      // Do whatever you want when MQTT is disconnected in normal mode
      break;
  }
}

// Main setup
void setup()
{
  EEPROM.begin(sizeof(EEpromData));
  EEPROM.get(0,EEpromData);

  Serial.begin(115200);
  Serial.println();
  Serial.println();
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUTTON, INPUT);
	digitalWrite(PIN_BUTTON, HIGH);
	debouncerButton.attach(PIN_BUTTON);
	debouncerButton.interval(50);

  if (EEpromData.initialState==1)
  {
    digitalWrite(PIN_RELAY, HIGH);
    digitalWrite(PIN_LED, LOW);
  } else {
    digitalWrite(PIN_RELAY, LOW);
    digitalWrite(PIN_LED, HIGH);
    EEpromData.initialState==0;
  }

  Homie_setFirmware(FW_NAME, FW_VERSION);
  //Homie.setSetupFunction(setupHandler);
  //Homie.setLoopFunction(loopHandler);
  Homie.setSetupFunction(setupHandler).setLoopFunction(loopHandler);

  Homie.setLedPin(PIN_LED, LOW);
  Homie.setResetTrigger(PIN_BUTTON, LOW, 10000);
  //relayNode.subscribe("relayState", relayStateHandler);
  relayNode.advertise("relayState").settable(relayStateHandler);
  //relayNode.subscribe("relayInitMode", relayInitModeHandler);
  relayNode.advertise("relayInitMode").settable(relayInitModeHandler);
  //relayNode.subscribe("relayTimer", relayTimerHandler);
  relayNode.advertise("relayTimer").settable(relayTimerHandler);
  //keepAliveNode.subscribe("tick", keepAliveTickHandler);
  keepAliveNode.advertise("tick").settable(keepAliveTickHandler);
  //keepAliveNode.subscribe("keepAliveValue",keepAliveValueHandler);
  keepAliveNode.advertise("keepAliveValue").settable(keepAliveValueHandler);
  //Homie.registerNode(relayNode);
  //Homie.registerNode(keepAliveNode);
  Homie.onEvent(onHomieEvent);
  Homie.setup();
}

// Main loop
void loop()
{
  Homie.loop();
}
