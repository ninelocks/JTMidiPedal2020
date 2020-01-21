/*
====================================================
   System Exclusive Configuration Messages
=====================================================

Configuration of the project is by Midi System Exclusive MEssages, the format of which are now described

The midi header counting from byte 0

0 0xF0 Start of sysex
1 0x7D indicating research/school/not  a manufacturer
2 0xNN 4 Character key , the same in the arduino application
3 0xNN
4 0xNN
5 0xNN
6 0xMM A device ID, you choose identify your arduino project
7 0xMM A Command 
.
. Variable number of bytes depending on message
.
. 0xF7 End of SysEx marker

The 4 character code is set in the arduino source and can be configured in the windows pedal manager (see its documentaion )

=====================================================
  Request arduino send out its current configuration
=====================================================

0 0xF0 Start of sysex
1 0x7D indicating research/school/not  a manufacturer
2 0xNN 4 Character key , the same in the arduino application
3 0xNN
4 0xNN
5 0xNN
6 0xMM A device ID, you choose identify your arduino project
7 0x02 Command 0x01 is send config
8 0xf0 End of SysEx marker


On receipt of correctly formatted message the Arduino should reply

 0 0xF0 Start of sysex
 1 0x7D indicating research/school/not  a manufacturer
 2 0xNN 4 Character key , the same as in the arduino application
 3 0xNN
 4 0xNN
 5 0xNN
 6 0xMM A device ID, you choose to identify your arduino project
 7 0x02 indictates this is response to a "give me your configuration" request 
  
     //then 

 8       // midi channel slider 1
 9       // midi channel slider 2
  
10       // midi channel button 1
11       // midi channel button 2 
12       // midi channel button 3
13       // midi channel button 4 
14       // midi channel button 5
 
   
15       // Expression Pedal  1 controller number
16       // Expression Pedal  2 controller number
   
17       // switch 1 controller number
18       // switch 2 controller number
19       // switch 3 controller number
20       // switch 4 controller number
21       // switch 5 controller number
   
22       // switch 1 toggle mode (1 is toggle mode, 0 is normal)
23       // switch 2 toggle mode (1 is toggle mode, 0 is normal)
24       // switch 3 toggle mode (1 is toggle mode, 0 is normal)
25       // switch 4 toggle mode (1 is toggle mode, 0 is normal)
26       // switch 5 toggle mode (1 is toggle mode, 0 is normal) 
   
27  System firmware version Major Number; //report arduino program version (you set this in the arduino code)
28  System firmware version Minor Number
29  0xf7 End of SysEx Marker


=====================================================
 Send configuration to  the Arduino
=====================================================

 0  0x7d;
 1  device_key0;        
 2  device_key1;       
 3  device_key2;      
 4  device_key3;      
 5  0x01;               // device model/type is 1
 6  0x01;               // command byte 0x02 indicates a config command

 7  Expression Pedal1 Midi_channel                                                   
 8  Expression Pedal2 Midi_channel       

 9  Switch1 Midi_channel     
10  Switch2 Midi_channel     
11  Switch3 Midi_channel   
12  Switch4 Midi_channel    
13  Switch5 Midi_channel     

14  Expression Pedal1 Control_no
15  Expression Pedal2 Control_no

16  Switch1 Control_no
17  Switch2 Control_no
18  Switch3 Control_no
19  Switch4 Control_no
20  Switch5 Control_no

21  Switch1 Toggle_mode
22  Switch2 Toggle_mode
23  Switch3 Toggle_mode
24  Switch4 Toggle_mode
25  Switch5 Toggle_mode

26  0xF7;

Arduino will reply with a minimal acknowledgement

 F0 7D 4A 6F 6E 54 01 00 F7  if the config was accepted

 F0 7D 4A 6F 6E 54 01 01 F7  if the config was rejected
 
 bytes 1-4 being the key as embedded into the arduino code as ID
 
 */


//===================================================================================================
// receive sysex
//===================================================================================================
 
void doSysExSendConfigToManager(){
  
 // byte testex[] = {0xf0, 0x7d, 0x4a, 0x6f, 0x6d, 0x54,0x42,0x42, 0xf7};
byte sysexMsg[30];

sysexMsg[0] = 0xf0;
sysexMsg[1] = 0x7d;
sysexMsg[2] = midi_key[0];
sysexMsg[3] = midi_key[1];
sysexMsg[4] = midi_key[2];
sysexMsg[5] = midi_key[3];
sysexMsg[6] = sysDevId; // our device id
sysexMsg[7] = 2;        //the command we are responding to 
                        // partly here for future use and more complicated 
                        // protocol

   for (int n = 0; n < NUM_SLIDERS; n++)
    {
        sysexMsg[SYSEX_SLIDER_CHANNEL_BASE + n] =  conf.sliderChannel[n];
        sysexMsg[SYSEX_SLIDER_CONTROL_ID_BASE + n]= conf.slider_c_number[n];
    }

    for (int n = 0; n < NUM_BUTTONS; n++)
    {
       sysexMsg[SYSEX_SWITCH_CHANNEL_BASE + n] =  conf.buttonChannel[n];
       sysexMsg[SYSEX_SWITCH_CONTROL_ID_BASE + n] = conf.button_c_number[n];
       sysexMsg[SYSEX_SWITCH_TOGGLE_BASE + n] =  conf.btnmode[n];
    }
 
  sysexMsg[27] = sysversionMajor; //report our program version
  sysexMsg[28] = sysversionMinor;
  sysexMsg[29] = 0xf7;
  
  usbMIDI.sendSysEx(30, sysexMsg, true); 

}


//************SYSEX SECTION**************
/*
 * I take the aprroach that I may want to add to the sysex messages the device handles later
 * so...
 * Ive split the sysex handling up into checking if send matches our key
 * then worry about handling individual messages
 * if going to later handle universal sysex can add that it
 * 
*/

//===================================================================================================
// receive sysex
//===================================================================================================
bool isSysExForUs(byte *sysExBytes, unsigned int howlong){

if (howlong < 8) {     //if theres less bytes than SysEx Start(0xf0) + 0x7D + 4 (our key length)   + 1( our device ID)  + SysEx End (0xF7)              
  return false;       //no point in checking anything else
}

if (sysExBytes[0] == 0xf0
        && sysExBytes[1]  == 0x7D // 7D is private use (non-commercial)
        && sysExBytes[2]  == midi_key[0] // 4-byte 'key' - JonT in hex 0x4A is ascii J, 0x6f o, etc
        && sysExBytes[3]  == midi_key[1]
        && sysExBytes[4]  == midi_key[2]
        && sysExBytes[5]  == midi_key[3]
        && sysExBytes[SYSEX_DEVICE_ID] == sysDevId ) { 
         
          return true;         
        }
 
        return false;
}

//===================================================================================================

void sysexDoConfig(byte *sysExBytes, unsigned int howlong){

      /* length and message type was validated before we get here */ 
       
      // Serial.println(" in config function ");     //uncomment when debugging 
                                         
      blink_n_times(2,50,50);  // blink our external LED to show sys looked ok -ish
 
      for (int n = 0; n < NUM_SLIDERS; n++)
      {
          conf.sliderChannel[n] = sysExBytes[SYSEX_SLIDER_CHANNEL_BASE + n];
          conf.slider_c_number[n] = sysExBytes[SYSEX_SLIDER_CONTROL_ID_BASE + n];
      }
  
      for (int n = 0; n < NUM_BUTTONS; n++)
      {
         conf.buttonChannel[n] = sysExBytes[ SYSEX_SWITCH_CHANNEL_BASE + n];
         conf.button_c_number[n] = sysExBytes[SYSEX_SWITCH_CONTROL_ID_BASE + n];
         conf.btnmode[n] = sysExBytes[SYSEX_SWITCH_TOGGLE_BASE + n];
      }
   
      save_config();                                  // write config to eeprom
  
     // byte data[] = { 0xF0, 0x7D, 0xF7 , true};       // ACK msg - should be safe for any device even if listening for 7D
     // usbMIDI.sendSysEx(3, data,true);                // SEND
  
            
     sendSysExAckOrNak(SYSEX_OK);
      
      for (int i = 0; i < NUM_BUTTONS; i++) {
        rtr.toggled[i] = false;                       // start in OFF position (oddons code)
      }
   
 
}

//===================================================================================================
//sysex clallback handler
//===================================================================================================
void mySysEx(byte *sysExBytes, unsigned int howlong){

    // could check universal sysEx?
    // but  we dont handle them in this version

    /* first check if the sysex was really aimed at us */
    
    if (! isSysExForUs(sysExBytes,howlong)){
      blink_n_times(2,100,50);     
        Serial.println(" command too short ");                                     
      return; //maybe flash an led or somethein
    }
 

    /* if we get here then the header is correct and the message was intended for us      */
    /* given the system identified this as a sysex message we dont really need to check the kast byte is f7 do we? */
   
    /* check if message was config command from the manager application                   */
    /* config message is 28 bytes long so end of sysex end flag should be at index 27     */ 
    
    if ( sysExBytes[27] == 0xf7 && howlong == 28 && sysExBytes[SYSEX_DEV_COMMAND] == 1){    //1 is Do Config
                   blink_n_times(1,50,50);    
                   sysexDoConfig(sysExBytes,  howlong);
                   return;
    }

   /* check if message was request for  config command from the manager application              */
   /* request message  message is 9 bytes long so end of sysex end flag should be at index 8     */ 
    
    if ( sysExBytes[8] == 0xf7 && howlong == 9  && sysExBytes[SYSEX_DEV_COMMAND] == 2)   {    // 2 is send me your config     
               doSysExSendConfigToManager();  
               blink_n_times(3,100,100);
               return;   
    }  

    // if land here then something we havent implemented yet :-) )
    blink_n_times(2,500,250);  

    /* short command coule be done with a switch but as we only handle 1 short command at the moment
     *  an if , as above, seems reasonable
 
 
    if  (sysExBytes[8] == 0xf7 && howlong == 9)   {    
        switch ( sysExBytes[SYSEX_DEV_COMMAND]) {
                case 2: doSysExSendConfigToManager();  blink_n_times(3,100,100); break;
 
                default: break;
        }
    }
    */

    /* if we get here then message was destined for us as key was correct but not a message we handle
     *  so send back an NAK (negative acknowledge)
     */
    sendSysExAckOrNak(SYSEX_FAIL);
   

}


//===================================================================================
// Support functions
//===================================================================================
void sendSysExAckOrNak(byte status_value){
  
 byte sysexAckMsg[10];       // this is used for our acknowledgement reply  
   sysexAckMsg[0] = 0xf0;
      sysexAckMsg[1] = 0x7d;
      sysexAckMsg[2] = midi_key[0];
      sysexAckMsg[3] = midi_key[1];
      sysexAckMsg[4] = midi_key[2];
      sysexAckMsg[5] = midi_key[3];
      sysexAckMsg[6] = sysDevId;           //our device id
      sysexAckMsg[7] = status_value; //0 == everything went OK 1 = Error
      sysexAckMsg[8] = 0xf7; // end of sysex
      usbMIDI.sendSysEx(9, sysexAckMsg,true); 
}
  
