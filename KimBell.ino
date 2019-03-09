/*
 Version 1.0
*/ 

#define HostName	"KimBell"

#define DEBUG 1

#include <Arduino.h>
#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>  // For OTA
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WebSocketsClient.h>	//  https://github.com/kakopappa/sinric/wiki/How-to-add-dependency-libraries 
#include <ArduinoJson.h>		// https://github.com/kakopappa/sinric/wiki/How-to-add-dependency-libraries
#include <StreamString.h>

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;
WiFiClient client;

#define MyApiKey "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxx" // TODO: Change to your sinric API Key. Your API Key is displayed on sinric.com dashboard
#define MydeviceID "xxxxxxxxxxxxxxxxxxxxxxxx"			// TODO: Change to your sinric device Key. Your device Key is displayed on sinric.com dashboard
#define MySSID "SSID"								// TODO: Change to your Wifi network SSID
#define MyWifiPassword "PassWord"						// TODO: Change to your Wifi network password

#define API_ENDPOINT "http://sinric.com"
#define HEARTBEAT_INTERVAL 300000 // 5 Minutes 

uint64_t heartbeatTimestamp = 0;
bool isConnected = false;


// LEDstrip
#define LED_PIN     D5  // To Do, Change to hardware SPI pin
#define CHIPSET     WS2812B
#define COLOR_ORDER GRB
#define NUM_LEDS    24
#define BRIGHTNESS  128
#define MAXHUE 256*6	// max Hue
#define BREATH_RATE 10UL
CRGB leds[NUM_LEDS];

volatile uint8_t kid_state = 0;
int LED_brightness = 30;
int position = 0;
uint16_t LED_hue = 0;
uint8_t LED_counter = NUM_LEDS;
bool LightOn = false;
bool Flip = false;


// For Switch
const int postingInterval = 3 * 60 * 1000; // post server update data every 3 mins 
unsigned long previousMillis = 0;
#define SWITCH_PIN D0


// FastLED provides these pre-conigured incandescent color profiles:
//     Candle, Tungsten40W, Tungsten100W, Halogen, CarbonArc,
//     HighNoonSun, DirectSunlight, OvercastSky, ClearBlueSky,
// FastLED provides these pre-configured gaseous-light color profiles:
//     WarmFluorescent, StandardFluorescent, CoolWhiteFluorescent,
//     FullSpectrumFluorescent, GrowLightFluorescent, BlackLightFluorescent,
//     MercuryVapor, SodiumVapor, MetalHalide, HighPressureSodium,
// FastLED also provides an "Uncorrected temperature" profile
//    UncorrectedTemperature;


// Forward declarations
void setPowerStateOnServer(String deviceId, String value);
void KidLed(int SEThue, int SETbrigtness);

void fadeall() { for (int i = 0; i < NUM_LEDS; i++) { leds[i].nscale8(250); } }



void LedLight(int SETbrigtness) {
	if (LightOn == 0) {
		for (int i = (NUM_LEDS)-1; i >= 0; i--) {
			leds[i] = Candle;
		}
		FastLED.setBrightness(SETbrigtness);
		FastLED.show();
		LightOn = 1;
	}


}


void turnOn(String deviceId) {
  if (deviceId == MydeviceID) // Device ID of light device
  { 
#ifdef DEBUG 
	Serial.print("Turn on device id: ");
    Serial.println(deviceId);
#endif
  }
  else {
#ifdef DEBUG 
    Serial.print("Turn on for unknown device id: ");
    Serial.println(deviceId);
#endif
  }     
}

void turnOff(String deviceId) {
   if (deviceId == MydeviceID) // Device ID of light device
   { 
	   kid_state = 0;
#ifdef DEBUG 
     Serial.print("Turn off Device ID: ");
     Serial.println(deviceId);
#endif
  }
  else {
#ifdef DEBUG 
     Serial.print("Turn off for unknown device id: ");
     Serial.println(deviceId);
#endif
  }
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      isConnected = false;    
      Serial.printf("[WSc] Webservice disconnected from sinric.com!\n");
      break;
    case WStype_CONNECTED: {
      isConnected = true;
      Serial.printf("[WSc] Service connected to sinric.com at url: %s\n", payload);
      Serial.printf("Waiting for commands from sinric.com ...\n");        
      }
      break;
    case WStype_TEXT: {
#ifdef DEBUG
        Serial.printf("[WSc] get text: %s\n", payload);
#endif
        // Example payloads

        // For Light device type
        // {"deviceId": xxxx, "action": "setPowerState", value: "ON"} // https://developer.amazon.com/docs/device-apis/alexa-powercontroller.html
        // {"deviceId": xxxx, "action": "AdjustBrightness", value: 3} // https://developer.amazon.com/docs/device-apis/alexa-brightnesscontroller.html
        // {"deviceId": xxxx, "action": "setBrightness", value: 42} // https://developer.amazon.com/docs/device-apis/alexa-brightnesscontroller.html
        // {"deviceId": xxxx, "action": "SetColor", value: {"hue": 350.5,  "saturation": 0.7138, "brightness": 0.6501}} // https://developer.amazon.com/docs/device-apis/alexa-colorcontroller.html
        // {"deviceId": xxxx, "action": "DecreaseColorTemperature"} // https://developer.amazon.com/docs/device-apis/alexa-colortemperaturecontroller.html
        // {"deviceId": xxxx, "action": "IncreaseColorTemperature"} // https://developer.amazon.com/docs/device-apis/alexa-colortemperaturecontroller.html
        // {"deviceId": xxxx, "action": "SetColorTemperature", value: 2200} // https://developer.amazon.com/docs/device-apis/alexa-colortemperaturecontroller.html
        
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject((char*)payload); 
        String deviceId = json ["deviceId"];     
        String action = json ["action"];
        
        if(action == "setPowerState") { // Switch or Light
            String value = json ["value"];
            if(value == "ON") {
				kid_state = 1;
                turnOn(deviceId);
            } else {
				kid_state = 0;
                turnOff(deviceId);
            }
        }
        else if(action == "SetColor") {
            // Alexa, set the device name to red
            // get text: {"deviceId":"xxxx","action":"SetColor","value":{"hue":0,"saturation":1,"brightness":1}}
            String sting_hue = json ["value"]["hue"];
			//y = map(x, 1, 50, 50, 1);
			//LED_hue = map((int) json["value"]["hue"],0,360,0,255);

			uint16 temp_hue = json["value"]["hue"];
			LED_hue = map(temp_hue, 0, 360, 0, 255);
            String saturation = json ["value"]["saturation"];

			/* 
			// include if use brightness when setting colour
			// Alexa sends realy high default brightness values !!
			double tempBrightnes = json["value"]["brightness"];  // double 0.0 to 1.0 
			tempBrightnes = tempBrightnes * 255; // convert to 0-255
			LED_brightness = (int)tempBrightnes;  // convert to int	
            Serial.print("[WSc] brightness: ");
			Serial.println(LED_brightness);
			*/
#ifdef DEBUG 
           Serial.println("[WSc] hue: " + sting_hue);
		   Serial.println(LED_hue);
           Serial.println("[WSc] saturation: " + saturation);
#endif



        }
        else if(action == "SetBrightness") {
			// {"deviceId": xxxx, "action": "setBrightness", value: 42}
			LED_brightness = map((int)json["value"],0,100,0,255); // int 0-100
#ifdef DEBUG 
			Serial.print("[WSc] brightness: ");
			Serial.println(LED_brightness);
#endif        
        }
        else if(action == "AdjustBrightness") {
          
        }
		else if (action == "SetColorTemperature") {
			kid_state = 2;
			String sting_value = json["value"];
#ifdef DEBUG 
			Serial.print("[WSc] SetColorTemperature: ");
			Serial.println(sting_value);
#endif
		}
        else if (action == "test") {
#ifdef DEBUG 
            Serial.println("[WSc] received test command from sinric.com");
#endif
        }
      }
      break;
    case WStype_BIN:
#ifdef DEBUG 
      Serial.printf("[WSc] get binary length: %u\n", length);
#endif
      break;
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(SWITCH_PIN, INPUT);

  // My neopixel had red and green swapped "GRB"
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
 

  WiFiMulti.addAP(MySSID, MyWifiPassword);
  Serial.println();
  Serial.print("Connecting to Wifi: ");
  Serial.println(MySSID);  

  // Waiting for Wifi connect
  while(WiFiMulti.run() != WL_CONNECTED) {
    delay(25);
    Serial.print(".");


	static uint8_t setuphue = 0;
	// First slide the led in one direction
	for (int i = 0; i < NUM_LEDS; i++) {
		// Set the i'th led to red 
		leds[i] = CHSV(setuphue++, 255, 50);
		// Show the leds
		FastLED.show();
		// now that we've shown the leds, reset the i'th led to black
		// leds[i] = CRGB::Black;
		fadeall();
		// Wait a little bit before we loop around and do it again
		delay(10);
	}

	// Now go in the other direction.  
	for (int i = (NUM_LEDS)-1; i >= 0; i--) {
		// Set the i'th led to red 
		leds[i] = CHSV(setuphue++, 255, 50);
		FastLED.show();						// Show the leds
		// now that we've shown the leds, reset the i'th led to black
		// leds[i] = CRGB::Black;
		fadeall();
		delay(10);  // Wait a little bit before we loop around and do it again
	}

  }

  if(WiFiMulti.run() == WL_CONNECTED) {
	FastLED.clear();
	FastLED.show();
    Serial.println("");
    Serial.print("WiFi connected. ");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }


  // Nice to update over the air !!
  ArduinoOTA.setHostname(HostName);

  ArduinoOTA.onStart([]() {
	  String type;
	  if (ArduinoOTA.getCommand() == U_FLASH) {
		  type = "sketch";
	  }
	  else { // U_SPIFFS
		  type = "filesystem";
	  }

	  // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
	  Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
	  Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
	  Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
	  Serial.printf("Error[%u]: ", error);
	  if (error == OTA_AUTH_ERROR) {
		  Serial.println("Auth Failed");
	  }
	  else if (error == OTA_BEGIN_ERROR) {
		  Serial.println("Begin Failed");
	  }
	  else if (error == OTA_CONNECT_ERROR) {
		  Serial.println("Connect Failed");
	  }
	  else if (error == OTA_RECEIVE_ERROR) {
		  Serial.println("Receive Failed");
	  }
	  else if (error == OTA_END_ERROR) {
		  Serial.println("End Failed");
	  }
  });
  ArduinoOTA.begin();



  // server address, port and URL
  webSocket.begin("iot.sinric.com", 80, "/");

  // event handler
  webSocket.onEvent(webSocketEvent);
  webSocket.setAuthorization("apikey", MyApiKey);
  
  // try again every 5000ms if connection has failed
  webSocket.setReconnectInterval(5000);   // If you see 'class WebSocketsClient' has no member named 'setReconnectInterval' error update arduinoWebSockets

  setPowerStateOnServer(MydeviceID, "OFF");
}

void loop() {
  ArduinoOTA.handle();
  webSocket.loop();
/*
  if ((millis() - previousMillis >= postingInterval)) {

	  previousMillis = millis();  // Remember the time

	  // Publish to server eg:
	  if (Flip == false) {
		setPowerStateOnServer("5c7b92a949bfdd2e14bb55f3", "OFF");
		Flip = true;
	  }
	  if (Flip == true) {
		  setPowerStateOnServer("5c7b92a949bfdd2e14bb55f3", "ON");
		  Flip = false;
	  }

	  //    setTargetTemperatureOnServer

  }
*/

  
  if(isConnected) {
      uint64_t now = millis();   
      // Send heartbeat in order to avoid disconnections during ISP resetting IPs over night. Thanks @MacSass
      if((now - heartbeatTimestamp) > HEARTBEAT_INTERVAL) {
          heartbeatTimestamp = now;
          webSocket.sendTXT("H");   
      }

	  switch (kid_state) {

		case 0:    // Led is of
			LightOn = 0;
			FastLED.clear();
			FastLED.show();
		  break;

		case 1:    // ledstrip in KidMode
			if (LightOn == true) {
				FastLED.clear();
				FastLED.show();
				LightOn = false;
			}
			
			KidLed(LED_hue, LED_brightness);
			break;

		case 2:    // Ledstrip in lightmode
			if (LightOn == false) {
				LedLight(LED_brightness);
				FastLED.clear();
			}
		  break;

		case 3:    // Ledstrip in partymode

		  break;
	  
	  }
  }   
}


void setPowerStateOnServer(String deviceId, String value) {

	DynamicJsonBuffer jsonBuffer;

	JsonObject& root = jsonBuffer.createObject();

	root["deviceId"] = deviceId;

	root["action"] = "setPowerState";

	root["value"] = value;

	StreamString databuf;

	root.printTo(databuf);

	webSocket.sendTXT(databuf);

}

void KidLed(int SEThue, int SETbrigtness) {

	for (int i = (NUM_LEDS)-1; i >= 0; i--) {

		leds[i] = CHSV(SEThue, 255, SETbrigtness);
		float breath = (exp(sin(millis() / 2000.0*PI)) - 0.36787944)*108.0;
		FastLED.setBrightness(breath);
		FastLED.show();
	}

}
