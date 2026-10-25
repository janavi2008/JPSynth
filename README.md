# JPSynth
a finger drum machine with 8 drum pads, gesture controls, and its own amp and speaker. its also got a rotary encoder and an oled screen. the pads are piezo discs, and theres a distance sensor, so moving your hand up and down can bend the pitch or distort the sound

# Design
Ive used waveshare esp32 s3 zero module as the microcontroller, 8 piezoelectric discs, a vl53l0x sensor, an EC11 rotary encoder, an 1.3in oled screen, 2 pushbuttons, a MAX98357A amp module, and a 3W 8ohm speaker. i designed the enclosure, lid and caps for the drum pads in CAD.
each pad acts as its own drum - a kick drum, hi-hat, snare, crash, ride, mid tom, high tom and floor tom. one of the buttons will act as a hi-hat pedal. the rotary encoder can control the volume or change the kit. the distance sensor measures how high your left hand is and bends the pitch according to where you move it. the oled screen dispays the settings selected, and shows sound waves while playing.
Schematic:
<img width="548" height="311" alt="image" src="https://github.com/user-attachments/assets/95c1f23e-1a62-4587-9024-70e26e9f45d1" />

Pcb:
<img width="268" height="362" alt="image" src="https://github.com/user-attachments/assets/1609b433-6156-4ea7-a06e-9685fa8545b4" />

CAD:
<img width="412" height="256" alt="image" src="https://github.com/user-attachments/assets/64a3f82f-620f-4224-9270-e77849240464" />
