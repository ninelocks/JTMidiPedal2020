# JTMidiPedal2020
A midi pedal with footswitches and volume pedal support to send midi control information.
Uses a teensy micro to act as a usb device.

### hardware
The project as descriobed here supports

* 5 switch type inputs 
* 2 Analog inputs for expression Pedals/Sliders

The circuit diagram is in wiki.

The Arduino Project stores its configuratation in eeprom. The program is stored along with a CRC check code
which is used to detect any corruption of the configuration.

##### Factory Reset

At startup the pedal can be reset to default values if the input to to switch 0 is held low.

##### Configuring the Pedal Via System Exclusive Messages

The configuration of the pedal is through system exclusive messages and an companion Windows application (works under wine in Linux is also available for download.

The sysex format is described in  [jtsysexformat.txt](jtsysexformat.txt)

