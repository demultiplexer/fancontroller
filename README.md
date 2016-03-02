# Open source programmable fan controller

![Fan controller PCB and bottom printed enclosure shell (top shell removed)](/fancontroller-withoutcover-vignette.jpg)

This is an open source (commons) hardware project to create a fully programmable computer fan controller. The function of this board is to allow the user to both modify the PWM signal from the motherboard ("how fast the motherboard tells the fan to go") as well as the tachometer signal from the fan ("how fast the fan says it's spinning").

One application of this device is in server equipment that require fans to be present and spinning at a certain speed. These fans are often noisy and it helps to replace them with slower-spinning, quieter alternatives. However, motherboard firmware often rejects these fans. With this fan controller, the fan can pretend it's rotating faster than it really is.

This fan controller is completely open hardware and I highly encourage forking. Attribution is not required but I would really like it if you do mention me. I sell these boards including enclosure and wiring on my Tindie shop: https://www.tindie.com/stores/mux/

# Usage

The 4-pin fan connector on the left (marked "INPUT") should be connected to the motherboard using a straight 4-pin jumper cable. The 4-pin fan connector on the right should be connected to the fan. By default, the board does not interfere in the signals between motherboard and fan; it will just relay them as if it does not exist. 

By pressing the 'SELECT' (rightmost) button, you can cycle through the 4 settings: Tach min/max and PWM min/max. The corresponding LED will light up.

By pressing "UP" or "DOWN", the selected setting will be modified positively and negatively, respectively. 

Increasing or decreasing the "Tach output min" increases and decreases the minimum RPM signal that the board will send to the motherboard in steps of 500 rpm (steps of 1000 rpm above 5000 rpm). This way, the motherboard will 'think' the fan turns quicker than it actually does. The "Tach output max" signal works the same, but with respect to the upper limit of RPM.

Modifying the "PWM output min" and "PWM output max" settings changes how the fan responds to motherboard PWM signals. For instance, if both these settings are at 0, the fan controller will always output 0% PWM irrespective of the motherboard signal. Similarly, if the 'min' setting is at 50% and the 'max' setting is at 100%, the fan controller will instruct the fan to rotate at 50% even when the motherboard says it should rotate slower.

# How to build this

First of all, you can [buy it from me](https://www.tindie.com/stores/mux/). Barring that, you will need:

 - The PCB. You can order this at many PCB manufacturers like Itead studio, Seeed studio, Elecrow, PCBCART, Ledsee, Olimex, Eurocircuits. I have conveniently included a .zip file with all the gerber files in the required format for most manufacturers. Just go to their PCB prototyping service and upload [this file](/Gerber files/Orderable file format/fancontroller.zip).
 - The parts. I order my parts at nl.farnell.com, the Dutch branch of Element 14. The part list uses Farnell part numbers. If you have a Farnell account, go to your shopping cart and go to the link 'Quick Paste'. Then paste the text inside [this file](/schematics and PCB layout/BOM-element14.txt) into the entry box and you are done.
 - The enclosure. This can be printed using the .stl files provided in [this folder](/enclosure/). This folder also has some examples of how the enclosure should look in your 3D printing software. *PLEASE READ THE INSTRUCTIONS IN 'enclosure/instructions.txt'*
 - A straight 4-pin fan cable. You can either make this yourself using [this connector housing](http://nl.farnell.com/wurth-elektronik/61900411621/housing-2-54mm-4way/dp/1841379), [these contacts](http://nl.farnell.com/wurth-elektronik/61900113722dec/contact-2-54mm-crimp-awg28-22/dp/1841425), some wire and suitable crimpling pliers, or you can try to find ready-made cables on e.g. eBay. I do not currently have any recommendations for ready-made cables.
 - An AVRISP mkII or other AVR programmer witht 6-pin programming header
 - Either Atmel Studio 6.2+ or avrdude. The project is Atmel Studio-based, but you can just flash [the .hex file](/firmware-v2/fancontroller-v2/fancontroller-v2/Debug)
 
 The PCB uses pretty coarse parts, it should be solderable by anybody with a smattering of soldering experience. Don't be afraid of the SOT-323 parts on the board, nor the 0402 resistors; anybody can solder those if you just try :) For the best soldering results, I highly recommend using solder paste and oven reflow. The buttons ARE reflow compatible. I recommend the website [smtstencil.co.uk](http://smtstencil.co.uk/) for all your stencil needs. The gerber files for standard stencils are inside the gerber folder.
 
 After you produced the board and enclosure, you need to program it. The board has a side-mounted footprint for a standard AVRISP mkII programming header, with pin 1 on the REVERSE SIDE of the board. The enclosure does not support this header, so after you are done programming you need to desolder it to fit it into the enclosure.
 
 # Firmware hacking
 
I highly recommend watching the in-depth technical explanation videos on my Youtube channel: https://youtube.com/PowerElectronicsBlog
