
#include <Bounce.h>
#include <ResponsiveAnalogRead.h>
#include <EEPROM.h>
/*
    oh no yet another  Midi footswitch and volume/wah pedal controller
    J.Trinder jont<at>ninelocks.com January 2020

    more details below......after the revision history
*/
//======================================================================================
//  Revision History
//======================================================================================

/*

2020/01/27 firmware version 1.7

There was a problem in that midi library uses channels 1 to 16 
whereas I nortmally work 0-15 in ACTUAL hex numbers of the received data

So in this module we use 1-16 and conversion to real hex numbers is done
in the sysex module.

2020/01/21 firmware version 1.6

Sysex code moved to separate file.
Comments and explanation revised

2020/01/20
modify the acknowledgement sent after configuration command so the ack
is identified with our key AND if things fail it flags that in the reply.

Also for other incorrect messages ie ones that are too long or have wrong fields 
an NAK is sent.

for messages below  dont contain at least our key, id and a command byte 
no reply is sent at all


2020/01/19 firmware version 1.4
Corrected the id key typo which had been JomT in hex rathern the JonT
 
2020/01/19 firmware version 1.3
the sysex key nore stored in an array midi_key to make it easier to change

2020/01/18

Modified sysex messages so there is also a byte specifying which command was requested
mainly to allow future expansion.
Sysex function now borken up into sections as it makes it more readable (at least I think
so.
 
2020/01/15
* moved to using callbacks to handle events rather than checking the message type
* New sysex upload request added so the windows pedal manager application can
* upload the current settings,


 
2020/01/14 

* added led to connect externally to alert/show activity
* added extra switch channels, so now 5 on/off inputs and 2 analogue(exp pedals)
* modified sysex handler to be more compact and hopefully less prone to errors
* added routine to flash the external led so can communicate errors etc etc
* added flashes to indicate good/bad sysex see sysex receive function 

2020/01/11 
* Changed sysex so that each control can have midi channel set independently
* 
*/


//======================================================================================
 /*
  * 
    oh no yet another  Midi footswitch and volume/wah pedal controller
    J.Trinder jont<at>ninelocks.com January 2020

    I appreciate that this is rather complicated compared to most midi pedals.
    Ive put in lots of extra code especially around sysex handling and acknowledgement or messages
    and sending back a message indicating success or failure.

    Some of this is superflous for a usb connected device but I also intend to build one with
    conventional serial MIDI interface.

    These may be extended in future to give more information.

    The system also does a crc of the eeprom stored configuration as, I am concious of the pain of failing 
    eeprom :-).

    

    I want to make my life easy to expand to more inputs on other devices so am making
    this a bit future proof to allow for up to 16 buttons and 16 sliders
    I am using an teensy lc which has 128 locations so plenty of room to leave space
    for future extra sliders/buttons but not have to change the internal structure much.

    Obviously you do need to configure suitable analog and digital pins.
    
    Also check out oddons project at:-
    
    from https://forum.pjrc.com/threads/24537-Footsy-Teensy-2-based-MIDI-foot-pedal-controller


    Many people get problems with noise and jitter on pots see 
   
    other with similar problems of noise and edge of range
    https://hackaday.io/project/8027-twister-a-play-on-midi-controllers/log/38042-arduino-analogread-averaging-and-alliteration

    ResponsiveAnalogRead is used to attempt to tame nooise from pots and sliders


    In the code there are some  print statments to flag error conditions
    There are some commented out print statements that you may find useful to uncomment when
    debugging your hardware.

    You will want make change to the sysex section to personalise things.

    See sysex function for more explanation.

    Also there is a windows control application to configure this project
    see 


  Factory reset by holding down pedal which is connected to digital input 0
*/

/**************************************************************************/
/*
   Defines and constants
*/
/**************************************************************************/
byte midi_key[4] = {'J', 'o', 'n', 'T'};  //key used to identify ourselves in sysex
                                                //most likely value someone else would change
                                                //for their own pedal    

// I would normally use an enum but for today these const are for sysex ACK/NAK Responses
// ACK = Acknowledge, NAK = negative acknowledge ie fail

const byte SYSEX_OK   =  0;
const byte SYSEX_FAIL =  1;


#define NUM_BUTTONS 5   // Buttons are simple digital inputs
#define NUM_SLIDERS 2   // slider are analog inputs, pots, sliders, exp pedals

// expression pedals
          /* version of firmware */
const byte sysversionMajor = 1; //sent back to config manager application
const byte sysversionMinor = 7; //rmember to changeif you want to identify anything

          /* id for this device, in case I build others using similar sysex */
const byte sysDevId = 1;        //ID of this device in case we haz multiple of them
       



const int main_delay = 5; // Delay between loops
const int ledPin     = 13; // On board led that can be flashed to help debugging etc
const int ledPinB    = 5;  // An external led 


const int RAR_ACTIVITY_THRESHOLD = 25;  // ResponsiveAnalogRead threshold
                                        // this is used to remove noise from
                                        // the slider inputs
                                        // you may have to alter this value
                                        // to suit your hardware


/**************************************************************************/
/*
   Sysex Information see doSysEx for how used
   these are the position within a sysex request where we expect
   parameters to be
*/
/**************************************************************************/

const int SYSEX_DEVICE_ID              = 6;
const int SYSEX_DEV_COMMAND            = 7;
const int SYSEX_SLIDER_CHANNEL_BASE    = 8;                  //these are used show where different parts of the sysex are within                                               
const int SYSEX_SWITCH_CHANNEL_BASE    = 10;                 //the received sysex array
const int SYSEX_SLIDER_CONTROL_ID_BASE = 15;
const int SYSEX_SWITCH_CONTROL_ID_BASE = 17;
const int SYSEX_SWITCH_TOGGLE_BASE     = 22;

/**************************************************************************/
/*
   Globals
*/
/**************************************************************************/

/*
   I know this is a c thing but I like keeping config and runtime variable
   together in a struct.
   for more on struct usage
   see for example
   https://www.circuitxcode.com/using-arduino-struct/
*/

/*
   This struct keeps the configuration information together
*/

struct config_record {
  byte chnl; //midi channel
  byte buttonChannel[NUM_BUTTONS] = {0, 0, 0, 0, 0};     //button midi channels as 1-16
  byte sliderChannel[NUM_SLIDERS] = {0, 0};              //slider midi channels as 1-16
  bool btnmode[NUM_BUTTONS] = {0, 0, 0, 0, 0}; //0 for normal 1 for toggle
 
  /*
       note the controller numbers here are in, *shudder* decimal
       not hex
  */
          
  byte slider_c_number[NUM_SLIDERS] =  {11, 81};    //the controller number
  byte button_c_number[NUM_BUTTONS] =  {64, 65, 66, 67, 68};    //the controller number
};

/*
   This struct keeps the status of things happening at runtime together
   You could just have each of the attributes as a separate global
   but I like to keep them together
*/

struct runtime_record {
  int adc_prev[NUM_SLIDERS];                  //slider previous value
  bool toggled[NUM_BUTTONS] = {false, false, false , false, false}; //  toggle indicates when toggle  in effect
};

//now define variable that we use to reference the structs

struct config_record conf;
struct runtime_record rtr;

//button debounce objects

Bounce button[5] = {Bounce(0, 5), Bounce(1, 5), Bounce(2, 5), Bounce(3, 5), Bounce(4, 5)}; // init 'bounce' for 5 buttons


// ResponsiveAnalogRead objects
//you could do this with an array of pins but figure its as readable to do this

ResponsiveAnalogRead analog_reads[NUM_SLIDERS] = {
  ResponsiveAnalogRead(A0, true),
  ResponsiveAnalogRead(A1, true)
};

//===================================================================================================
// ************* setup************
//===================================================================================================

void setup() {
  Serial.begin(31250);

  delay(500);

  
  pinMode(ledPin, OUTPUT);
  pinMode(ledPinB, OUTPUT);
  pinMode(0, INPUT_PULLUP); // set up three digital pins
  pinMode(1, INPUT_PULLUP); // with PULLUPs
  pinMode(2, INPUT_PULLUP); //  
  pinMode(3, INPUT_PULLUP); //  
  pinMode(4, INPUT_PULLUP); //  
  

  // init the thresholds for the ResponsiveAnalogeRead objects

  for (int n = 0; n < NUM_SLIDERS; n++) {
    analog_reads[n].setActivityThreshold(RAR_ACTIVITY_THRESHOLD);
  }
                                      //to save sysexin here is a way to factory reset using
                                      //the state of the pedals

  if ( digitalRead(0) == LOW) {
     Serial.println("Factory Reset");   
     save_config();                   //which will be the default state of the struct config is in    
     blink_n_times(5,100,100);                                              
  }            
                                      //if factory default we could skit this but it take ms
  
  load_config();                      // load configuration from eeprom
 
  Serial.print("Started");            // tell the world we have hot this far
  digitalWrite(ledPinB, false);
  blink_n_times(2,200,100);

  usbMIDI.setHandleSystemReset(mySystemReset);  //callback for when a sys reset arrives
  usbMIDI.setHandleSystemExclusive(mySysEx);
 
}


void factoryReset(){


  
}
//===================================================================================================
// *************THE MAIN LOOP************
//===================================================================================================

void loop() {
  
  usbMIDI.read();

  /* or could use this mechanism
  if (usbMIDI.read() && usbMIDI.getType() == usbMIDI.SystemExclusive ) {
    doSysEx(); // look for CC change msg
  }
 */

  jtGetAnalogData();
  getDigitalData(); // get/process digital pins
  delay(main_delay);
 
}



//===================================================================================================
// load_config
// first check if the eeprom looks like it contains valid data
// and if so load it into our running configuration
//
// note that by valid I mean ensure the eeprom data is not corrupted from what was saved in the
// eeprom.
// if really panaoid you could check all the configuration values are within the limits for the
// parameter
//===================================================================================================

void load_config() {
  unsigned long crc;
  unsigned long stored_crc;

  int data_store_start = sizeof(unsigned long);   // we store our data in the eeprom AFTER the space
  // reserved for the checksum/crc. That is stored
  // in an unsigned long integer so we need to know
  // how long one of those is as that will be the
  // first addrerss where data is stored.

  EEPROM.get(0, stored_crc);                      // load the stored crc which is in address 0 of
                                                  // the eeprom
                                                
                                                  // Serial.println("stored ");
                                                  // Serial.print(stored_crc);

  crc = eeprom_crc(data_store_start);             // calculate the crc of what is IN the eeprom
                                                  // Serial.println("calcd ");
                                                  // Serial.print(crc);


  if (crc != stored_crc) {                              // now make sure they are the same
    Serial.println("CRC Check failed.Trying new save"); // send fail message to serial monitor

    save_config();  //so try saving current config      // if not, use default values and try saving
                                                        //  to eeprom again
    return;
  }

  EEPROM.get(data_store_start, conf);                  //otherwise looks good so go and get the config
}


//===================================================================================================
// save_config
// saves configuration struct to eeprom
// and also calculates and saves a crc at the start of the eeprom
// the crc is used to check the contents of the eeprom are not corrupt
// see https://www.arduino.cc/en/Tutorial/EEPROMCrc
//===================================================================================================
void save_config() {
  unsigned long crc;

  int data_store_start = sizeof(unsigned long);   // we store our data in the eeprom AFTER the space
                                                  // reserved for the checksum/crc. That is stored
                                                  // in an unsigned long integer so we need to know
                                                  // how long one of those is as that will be the
                                                  // first addrerss where data is stored.
  EEPROM.put(data_store_start, conf);
  crc = eeprom_crc(data_store_start);             //now we calculate the crc for the contents of
                                                  //the eprom EXCLUDING the checksum itself
                                                  //which is store at the start of the eeprom


  EEPROM.put(0, crc);                             //save the crc in the eeprom
}

//***************************************************************************************************
// CAllback handlers for the midi things
//***************************************************************************************************
/*
 * you may wish to do more but all I need it to make sure any toggles are untoggeled!
 * 
 */ 
 void mySystemReset(){

   for (int i = 0; i < NUM_BUTTONS; i++) {
        rtr.toggled[i] = false;                       
      }
  blink_n_times(1,500,100);  // blink our external LED  
  
 }

//===================================================================================================
// jtGetAnalogData
//===================================================================================================
void jtGetAnalogData() {
  int adcValue = 0;                 // used to store value read from responsiveanalogread object
  int valX;                         //used to store our 0-127 scaled version of the analog read value

  for (int n = 0; n < NUM_SLIDERS; n++)
  {
    analog_reads[n].update();                 //do the responsiveanalogread update
    adcValue = analog_reads[n].getValue();    //read the value out from the adc
    //Serial.println(adcValue);               //when debugging it cann be useful to see values read
                                              // to find your pdeal/sliders actual range
    valX = (map(adcValue, 60, 980, 0, 127));  //map the range of values we read somehwere 0-1024
                                              // to a range of 0-127 as wanted by midi
                                              //the lower and upper leverls will depend on your
                                              //hardware
    valX = constrain(valX, 0, 127);           //ensure no negative values


                                              //check if analog value has changed since we last sent it
                                              // if so send it and save the value so next time around
                                              // we know what its value WAS
    if (valX != rtr.adc_prev[n]) {
      usbMIDI.sendControlChange(conf.slider_c_number[n], valX, conf.sliderChannel[n]); // calculate CC for analog   and send
      rtr.adc_prev[n] = valX;
    }

  }

}


//===================================================================================================
// getDigitalData
//===================================================================================================

//this pretty much same as https://forum.pjrc.com/threads/24537-Footsy-Teensy-2-based-MIDI-foot-pedal-controller
// credit to

void getDigitalData() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    button[i].update(); //update bounce object for each button
    if (button[i].fallingEdge()) { // button press to ground
      if (rtr.toggled[i] && conf.btnmode[i] == 1) { // if toggled state and toggle behavoiour both true...
        usbMIDI.sendControlChange(conf.button_c_number[i], 0, conf.buttonChannel[i]);  // unlatch to OFF
        rtr.toggled[i] = false      ;  // toggled to false for next time
      }
      else { // either latched and toggled==false or non-latched...
        usbMIDI.sendControlChange(conf.button_c_number[i], 127, conf.buttonChannel[i]); //...either way to to ON state
        if (conf.btnmode[i] == 1) {
          rtr.toggled[i] = true      ;  // but only change toggled state if in latched mode
        }
      }
    }
    if (button[i].risingEdge()) { // button release - pullup to HIGH
      if (not(conf.btnmode[i] == 1)) { // if non-latched
        usbMIDI.sendControlChange(conf.button_c_number[i], 0, conf.buttonChannel[i]);  // to to OFF
      }
    }
  }
}





//===================================================================================
// Support functions
//===================================================================================

void blink_n_times(int number_of_blinks, int ontime, int offtime){

bool original_state;
  original_state = digitalRead(ledPinB); //so we can put it back to how it was
  digitalWrite(ledPinB, false);
  delay(offtime);
  for (int n=0; n < number_of_blinks; n++){
     digitalWrite(ledPinB, true);
     delay(ontime);
     digitalWrite(ledPinB, false);
     delay(offtime);  
  }

  delay(2*offtime);
  digitalWrite(ledPinB,original_state);
  
}
//===================================================================================================
// calculate crc
//===================================================================================================

// crc code from https://www.arduino.cc/en/Tutorial/EEPROMCrc
// modified as we dont want crc of entire eprom just a section
// so this calcs crc from start_address to the end of the 
// eeprom

unsigned long eeprom_crc(int start_address) {

  const unsigned long crc_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
  };

  unsigned long crc = ~0L;

  for (int index = start_address ; index < EEPROM.length()  ; ++index) {
    crc = crc_table[(crc ^ EEPROM[index]) & 0x0f] ^ (crc >> 4);
    crc = crc_table[(crc ^ (EEPROM[index] >> 4)) & 0x0f] ^ (crc >> 4);
    crc = ~crc;
  }
  return crc;
}
