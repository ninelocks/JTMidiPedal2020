
#include <Bounce.h>
#include <ResponsiveAnalogRead.h>
#include <EEPROM.h>

/*
     Midi footswitch and volume/wah pedal controller
     J.Trinder jont<at>ninelocks.com January 2020

    I want to make my life easy to expand to more inputs on other devices so am making
    this a bit future proof to allow for up to 16 buttons and 16 sliders
    I am using an teensy lc which has 128 locations so plenty of room to leave space
    for future extra sliders/buttons but not have to change the internal structure much.

    Obviously you do need to configure suitable analog and digital pins.
    
    Also check out oddos project at:-
    
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

  * * * expression pedal wiring * * *
  Varies a lot see
 
  
*/


/**************************************************************************/
/*
   Defines and constants
*/
/**************************************************************************/

#define NUM_BUTTONS 2     // Buttons are simple digital inputs
#define NUM_SLIDERS 2     // slider are analog inputs, pots, sliders, 
// expression pedals


const int main_delay = 5; // Delay between polling the switches and sliders
const int ledPin    = 13; // A led that can be flashed to help debugging etc
 


const int RAR_ACTIVITY_THRESHOLD = 25;  // ResponsiveAnalogRead threshold
                                        // this is used to remove noise from
                                        // the slider inputs
                                        // you may have to alter this value
                                        // to suit your hardware



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
  bool btnmode[NUM_BUTTONS] = {0, 0}; //0 for normal 1 for toggle

  /*
       note the controller numbers here are in, *shudder* decimal
       not hex
  */
  byte slider_c_number[NUM_SLIDERS] =  {11, 81};    //the controller number
  byte button_c_number[NUM_BUTTONS] =  {64, 65};    //the controller number
};

/*
   This struct keeps the status of things happening at runtime together
   You could just have each of the attributes as a separate global
   but I like to keep them together
*/

struct runtime_record {
  int adc_prev[NUM_SLIDERS];                  //slider previous value
  bool toggled[NUM_BUTTONS] = {false, false}; //  toggle indicates when toggle  in effect
};

//now define variable that we use to reference the structs

struct config_record conf;
struct runtime_record rtr;

//button debounce objects

Bounce button[3] = {Bounce(0, 5), Bounce(1, 5), Bounce(2, 5)}; // init 'bounce' for 3 buttons


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
  pinMode(0, INPUT_PULLUP); // set up three digital pins
  pinMode(1, INPUT_PULLUP); // with PULLUPs
  pinMode(2, INPUT_PULLUP); // not used but for test purpose

  // init the thresholds for the ResponsiveAnalogeRead objects

  for (int n = 0; n < NUM_SLIDERS; n++) {
    analog_reads[n].setActivityThreshold(RAR_ACTIVITY_THRESHOLD);
  }
                                      //to save sysexin here is a way to factory reset using
                                      //the state of the pedals

  if ( digitalRead(0) == LOW) {
     Serial.println("Factory Reset");   
     save_config();                   //which will be the default state of the struct config is in                                                  
  }            
                                      //if factory default we could skit this but it take ms
  
  load_config();                      // load configuration from eeprom
 
  Serial.print("Started");            // tell the world we have hot this far
}

//===================================================================================================
// *************THE MAIN LOOP************
//===================================================================================================

void loop() {
  // if (usbMIDI.read() && usbMIDI.getType() == 7) { //if MIDI is sysEx
  if (usbMIDI.read() && usbMIDI.getType() == usbMIDI.SystemExclusive ) {
    doSysEx(); // look for CC change msg
  }

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
      usbMIDI.sendControlChange(conf.slider_c_number[n], valX, conf.chnl); // calculate CC for analog   and send
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
        usbMIDI.sendControlChange(conf.button_c_number[i], 0, conf.chnl);  // unlatch to OFF
        rtr.toggled[i] = false      ;  // toggled to false for next time
      }
      else { // either latched and toggled==false or non-latched...
        usbMIDI.sendControlChange(conf.button_c_number[i], 127, conf.chnl); //...either way to to ON state
        if (conf.btnmode[i] == 1) {
          rtr.toggled[i] = true      ;  // but only change toggled state if in latched mode
        }
      }
    }
    if (button[i].risingEdge()) { // button release - pullup to HIGH
      if (not(conf.btnmode[i] == 1)) { // if non-latched
        usbMIDI.sendControlChange(conf.button_c_number[i], 0, conf.chnl);  // to to OFF
      }
    }
  }
}



//===================================================================================================
// receive sysex
//===================================================================================================

/*
   SysEx receive
   Although Ive allowed for multiple switches etc here I only use two of each so
   Format of the sysex message
   byte number, value or use
   0 0xF0  //sys always starts with 0xFO
   1 0x7D  // 7D is private indicator as we arent a midi registered manufacturer
           // next 4 bytes are a key we use to make sure a message is really ours
           // this is done as oddson project at
           // https://forum.pjrc.com/threads/24537-Footsy-Teensy-2-based-MIDI-foot-pedal-controller
           
   2 0x4A  // J  our id is JonT in hex, each character specified in ascii 0x4a is a J
   3 0x6F  // o 
   4 0x6d  // n
   5 0x54  // T
   6 0xNN  // our product number /id not used yet
   7 0xXX   // for future use possibly version number
   8       // midi channel
   9       // pedal 0 controller number
   10      // pedal 1 controller number
   11      // switch 0 controller number
   12      // switch 0 toggle mode
   13      // switch 1 controller number
   14      // switch 1 toggle mode (1 is yes)
   15 0xF7 //end of sysex marker
*/
//************SYSEX SECTION**************
void doSysEx() {

  // its got more convoluted than the setup by oddson
  // from https://forum.pjrc.com/threads/24537-Footsy-Teensy-2-based-MIDI-foot-pedal-controller
  // but I had grand plans for fancy different configs...
  // eg having one of the sysexbytes specify how many sliders/buttons there are in use
 
  // Serial.println("sysex rx");

   
 
  
  byte *sysExBytes = usbMIDI.getSysExArray();
  if (sysExBytes[0] == 0xf0
      && sysExBytes[15] == 0xf7 // ************ count how long our message should be and put it in here
      && sysExBytes[1]  == 0x7D // 7D is private use (non-commercial)
      && sysExBytes[2]  == 0x4A // 4-byte 'key' - JonT in hex
      && sysExBytes[3]  == 0x6F
      && sysExBytes[4]  == 0x6d
      && sysExBytes[5]  == 0x54) { // read and compare static bytes to ensure valid msg

 
                                                     
    
     
   
    digitalWrite(ledPin, !digitalRead(ledPin));     // invert the led output so led changes on to off
                                                    // or off to on, each time we recive a sysex aimed
                                                    // at us with the correct header  

    /*
       jt will add a program version id field in case we want to change it in future and do fancy stuff
       as we check position of systex end , the F7, we know we have correct number of bytes
       if moving to a setup where variable number could be downloaded then check counts to make sure we dont try and access beyone array
    */

    conf.chnl = sysExBytes[8];
    for (int n = 0; n < NUM_SLIDERS; n++)
    {
      conf.slider_c_number[n] = sysExBytes[9 + n];
    }
    //load button channel numbers
    for (int n = 0; n < NUM_BUTTONS; n++)
    {
      conf.button_c_number[n] = sysExBytes[11 + n];
    }
    //load button toggle mode
    for (int n = 0; n < NUM_BUTTONS; n++)
    {
      conf.btnmode[n] = sysExBytes[13 + n];
    }

    save_config();

    byte data[] = { 0xF0, 0x7D, 0xF7 }; // ACK msg - should be safe for any device even if listening for 7D
    usbMIDI.sendSysEx(3, data);         // SEND
    
    for (int i = 0; i < NUM_BUTTONS; i++) {
      rtr.toggled[i] = false;           // start in OFF position (oddons code)
    }

  }
}


//===================================================================================
// Support functions
//===================================================================================

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
