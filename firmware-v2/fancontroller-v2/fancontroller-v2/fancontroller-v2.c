/*
 * fancontroller_v2.c
 *
 * Created: 25/07/2015 14:23:02
 *  Author: Emile Nijssen aka mux aka demultiplexer
 *
 * This is an almost-completely interrupt driven programmable fan controller firmware
 * the firmware is kind of complicated as a result of kind of underpowered hardware 
 * (to reduce cost and make the board easier to solder for beginners/casual hackers)
 * I highly recommend taking note of the documentation provided if you wish to modify
 * the firmware. 
 *
 * This is fully open source, i.e. I release this into commons. Do with this as you like
 * Attribution is not required but still encouraged
 *
 *
 * If you like this project, I sell these boards as well as other projects in my Tindie store:
 * https://www.tindie.com/stores/mux/
 *
 * I do provide support on this project via github, but I tend to be busy and kind of slow to respond
 * So in practice you are probably on your own for complicated bugs or feature requests
 *
 
 
 /*
 General firmware layout
 
 This board allows the user to clip the PWM and RPM signals of a fan/motherboard to some lower
 or higher value. 
 
 Given that timer1 is connected to MOSI_PWMOUT and that interrupt needs to be automated,
 we use this to create 25kHz PWM. That leaves us with timer2 for generating MISO_TACHMOBO.
 
 Unfortunately, after lots of testing, it seems that actually MISO_TACHMOBO is most important
 to get 100% right, so we're going to sporadically use timer1 for measuring other stuff.
 
 Essentially:
 - Timer1 always outputs the PWM duty cycle to OC1A
 - Timer1, on overflow, triggers an interrupt that increments the value tach
 - when tach reaches tach_period, MISO_TACHMOBO is XORed and lasttach is incremented
 - Timer0 just freeruns
 - whenever an RPM pulse from the fan is detected, we check if lasttach is exactly 1
 - if lasttach is less than 1, we output too few tachometer pulses to the motherboard so we speed up
 - likewise if lasttach is more than 1, we are outputting too many tach pulses and we need to slow down
 - the main loop services button presses
 - the main loop also measures the PWM duty cycle using timer0
 
 The user can set minima and maxima for the PWM duty cycle and reported RPM using the buttons
 - Adjusting "tach output max" clips the max. RPM reported from the fan to the motherboard in 500rpm decrements 
 - Adjusting "tach output min" clips the min. RPM reported from the fan to the motherboard in 500rpm increments 
 - Adjusting "pwm output max" clips the max. PWM sent to the fan to the motherboard in 5% decrements 
 - Adjusting "pwm output min" clips the max. RPM sent to the fan to the motherboard in 5% increments
 
 */ 

#include "fancontroller-v2.h"						//contains macros and named numbers
#include <avr/io.h>									//required to do anything
#include <avr/interrupt.h>							//interrupt handling
#include <avr/eeprom.h>								//eeprom reading/writing
#include <util/delay.h>								//delay handling

//tach/pwm period variables
volatile	uint16_t	tach_period			= 0;	//variable that stores the computed output tach signal to be sent to MISO_TACHMOBO
volatile	uint16_t	tach				= 0;	//running variable generating the tach signal
volatile	uint16_t	lasttach			= 0;	//running variable used to compare to tach 
volatile	uint16_t	pwm_duty_m			= 0;	//variable that stores the measured number of cpu cycles between an up- and downslope on the PWM signal

//The settings are stored in EEPROM as a value 0-20 that can be manipulated with the buttons,
//but the actual meaning of those values is determined by:
//  PWM settings: phys_setting[3:4] = eeprom_settings[3:4] * 2
//	Tach settings: phys_setting[0:1] = tach_lut[eeprom_settings[3:4]]

volatile	uint8_t		eeprom_settings[4]	= {0,20,10,10};
volatile	uint16_t	phys_settings[4]	= {750,27,20,20};
volatile	uint8_t		CURRENT_SETTING		= 0;	//contains the setting currently being manipulated

//												1     2     3     4     5     6     7     8    9    10   11   12   13   14   15    16    17    18    19   20
//											   500   1000  1500  2000  2500  3000  3500  4000 4500 5000 6000 7000 8000 9000 10000 11000 12000 13000 14000 15000
			uint16_t	tach_lut[20]		= {750,  375,  250,  188,  150,  125,  107,  94,  83,  75,  63,  54,  47,  42,  38,   34,   31,   29,   27,   25};
	
//button variables
volatile uint8_t		button_pressed		= 0;	//boolean telling the main loop whether a button was pressed

		uint8_t			i					= 0;	//iteration variable
		uint8_t			temp				= 0;

int main(void)
{
	load_settings_from_eeprom();
	initialize_hardware();
   
    while(1)
    {
		//a button was pressed
        if(button_pressed){	
			if(button_pressed == 1){	//up
				if(eeprom_settings[CURRENT_SETTING] < 19) eeprom_settings[CURRENT_SETTING]++;
			} else {					//down
				if(eeprom_settings[CURRENT_SETTING] > 0) eeprom_settings[CURRENT_SETTING]--;
			}
			
			
			//and modify the corresponding phys_setting
			if(CURRENT_SETTING < 2){
				phys_settings[CURRENT_SETTING] = tach_lut[eeprom_settings[CURRENT_SETTING]];
			} else {
				phys_settings[CURRENT_SETTING] = eeprom_settings[CURRENT_SETTING] * 2;
			}
			
			button_pressed = 0;	//release flag so we can service another button press
		}
		
		//when we're not servicing button presses, we're measuring the PWM signal. 
		//this is a very fast signal, so we don't want to use interrupts for this
		//hence we're doing this in the main loop so we can be easily interrupted by more
		//important stuff.
		
		//wait for a leading edge
		temp = 50;
		while(!(PINB & PWM_IN_PIN) && temp){temp++;}	//we include a counter to allow this loop to exit after a short timeout
		temp = 50;	//reset temp
		TCNT0 = 0;	//start the timer
		while(PINB & PWM_IN_PIN && temp){temp++;}
		pwm_duty_m = TCNT0;
			
		//at this time we either have some TCNT value between 0-40(ish) representing the duty cycle of the PWM signal
		//or we have a value of temp=0 because we had a timeout
		if(temp){	//non-timeout
			if(pwm_duty_m < phys_settings[3] && pwm_duty_m > phys_settings[2]){
				OCR1A = pwm_duty_m;				
			}
		}
    }
}

/****************************/
/* INITIALIZATION FUNCTIONS */
/****************************/

void load_settings_from_eeprom(void){
	//load the individual settings from EEPROM
	for(i = 0; i < 4; i++){
		eeprom_settings[i]	= eeprom_read_byte((uint8_t *) i); if(eeprom_settings[i] > 19){ eeprom_settings[i] = 10; }
	}
	
	CURRENT_SETTING		= eeprom_read_byte((uint8_t *) 4); 
	
	//increment current setting, check if it isn't exceeding our settings pages and re-store the incremented value to EEPROM
	CURRENT_SETTING++; if(CURRENT_SETTING > MAX_SETTING) CURRENT_SETTING = 0;
	eeprom_write_byte((uint8_t *) 4, CURRENT_SETTING);
	
	//translate the offset settings into actual usable numbers for our calculations
	phys_settings[0] = tach_lut[eeprom_settings[0]];
	phys_settings[1] = tach_lut[eeprom_settings[1]];
	phys_settings[2] = eeprom_settings[2] * 2;
	phys_settings[3] = eeprom_settings[3] * 2;
	
	//turn on the relevant LED
	PORTA |= (1 << CURRENT_SETTING);
}

void initialize_hardware(void){
	//set port direction bits to 1 for all output pins
	DDRA	= ADJ_PWMOUT_MIN_PIN | ADJ_PWMOUT_MAX_PIN | ADJ_TACHOUT_MIN_PIN | ADJ_TACHOUT_MAX_PIN |
			  MISO_TACHMOBO_PIN | MOSI_PWMOUT_PIN;
	DDRB	= 0;
	
	//pull-up on SCK_TACHMOBO, UP_PIN and DOWN_PIN
	PORTA  |= MISO_TACHMOBO_PIN;
	PORTB  |= UP_PIN | DOWN_PIN;
	
	//initialize PCINT0
	//PCINT0 triggers on SCK_TACHOUT, measuring the tachometer signal produced by the attached fan
	//it always triggers on the positive edge
	PCMSK0 |= (1 << PCINT4);
	
	//initialize PCINT1
	//PCINT1 triggers on any state change of the UP and DOWN buttons
	PCMSK1 |= (1 << PCINT8) | (1 << PCINT9);
	GIMSK  |= (1 << PCIE1) | (1 << PCIE0);
	
	//initialize TCNT0
	//Timer/counter0 is used for measurements only, and simply freeruns
	TCCR0B	= TIMER_CLKSEL_DIV8;
		
	//initialize TCNT1
	//Timer/counter1 is a 16-bit counter that just counts at clkio/2
	//It overflows at 160 to generate a 25kHz waveform
	//and it outputs to PA6 (OC1A) to physically output that PWM waveform
	//The overflow interrupt is used to generate the MISO_TACHMOBO signal on PA5
	TCCR1A	= (1 << COM1A1) | (1 << WGM11);													//enable output to OCR1A
	TCCR1B	= TIMER_CLKSEL_DIV8 | (1 << WGM13) | (1 << WGM12);								//run at clkio/64, use WGM3:0=1110, i.e. fast PWM with TOP=ICR1 and both COMPA and COMPB intact
	TIMSK1	= TIMER_ENABLE_OVF_INTERRUPT;													//enable overflow interrupt
	OCR1A	= 20;																			//duty is 50% by default
	ICR1	= 40;																			//period is 160, equivalent to 25kHz.
	
	sei();																					//enable global interrupts
}

/**********************/
/* INTERRUPT HANDLERS */
/**********************/

//timer1 overflow interrupt, set MISO_TACHMOBO
//timer1 overflow also acts as an arbiter for the measurement cycle
ISR(TIM1_OVF_vect){
	tach++;
	if(tach > tach_period){
		PORTA ^= MISO_TACHMOBO_PIN;
		tach = 0;	
		lasttach++;
	}	
}

//PCINT0 is armed on SCK_TACHOUT
//when PCINT0 is triggered, the fan has completed half a revolution
//ideally, in that time we also want to have output exactly 1 pulse, except
//when we're at the setting limits. So if we actually output more than one
//pulse we should slow down, and conversely.
ISR(PCINT0_vect){
	if(!lasttach){	//we're outputting too slow a signal, speed up!
		if(tach_period > phys_settings[0]){ tach_period--; }
	}
	
	if(lasttach > 1){
		if(tach_period < phys_settings[1]){ tach_period++; }
	}
	
	lasttach = 0;
}

//PCINT1 is armed on UP and DOWN
//a button press is going to cause either UP or DOWN to change to LOW, so we test for that to ascertain
//whether we need to increment or decrement the current value
ISR(PCINT1_vect){
	if(!button_pressed){	//wait until main() cleared the previous interrupt	
		if(!(PINB & UP_PIN)){
			button_pressed = 1;	//UP
		} 
		if(!(PINB & DOWN_PIN)){
			button_pressed = 2; //DOWN
		}
	}
	
	//the rest is done in main() so as not to choke out other interrupts
}