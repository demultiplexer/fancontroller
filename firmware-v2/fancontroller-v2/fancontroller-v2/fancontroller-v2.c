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
 */ 

#include "fancontroller-v2.h"						//contains macros and named numbers
#include <avr/io.h>									//required to do anything
#include <avr/interrupt.h>							//interrupt handling
#include <avr/eeprom.h>								//eeprom reading/writing
#include <util/delay.h>								//delay handling

//tach/pwm period variables
volatile	uint16_t	tach_period_m		= 0;	//variable that stores the measured number of cpu cycles between an up- and downslope on the sck_tachout signal
volatile	uint16_t	tach_period			= 0;	//variable that stores the computed output tach signal to be sent to MISO_TACHMOBO
volatile	uint16_t	pwm_duty_m			= 0;	//variable that stores the measured number of cpu cycles between an up- and downslope on the PWM signal
volatile	uint16_t	pwm_duty			= 0;	//variable that stores the computed output pwm signal to be sent to MOSI_PWMOUT
volatile	uint16_t	pwm_period_m		= 0;	//variable that stores the total pwm period, i.e. the number of cpu cycles between an up and second up-slope.

//measurement cycle variables
volatile	uint16_t	meas				= 0;	//variable that increments every fan PWM cycle, after MEAS_TIMEOUT cycles triggers a measurement cycle
volatile	uint8_t		currently_measuring = 0;	//boolean that tells us if we're currently in a measurement cycle
volatile	uint8_t		meas_cycle			= 0;	//variable that tells us WHAT we are currently measuring
													//0 = tach_period start
													//1 = tach_period synced
													//2 = tach_period end
													//3 = pwm_period start
													//4 = pwm_period synced
													//5 = pwm_period end
													//6 = pwm_duty
													//7 = done (go to finalize_measurements)

//'physical' variables corresponding to the _offset values stored in EEPROM									
volatile	uint16_t	TACH_LOW_SETTING;
volatile	uint16_t	TACH_HIGH_SETTING;
volatile	uint16_t	PWM_LOW_SETTING;
volatile	uint16_t	PWM_HIGH_SETTING;

//actual values we store in EEPROM
//the values go from 1 through 19, which have the meaning:
//Tach: absolute RPM limits in steps of 500 (up to 10), then 1000 (up to 20). Default is 5000rpm
//pwm: min and max output PWM, so 0-100% PWM from the motherboard gets mapped to low_offset-high_offset %
//e.g. if low_offset is at 1, 0-100% input gets mapped to -100-100% output, i.e. the PWM output is zero for half
//of the range. This way you can either force a fan to stay off or force a fan to stay on irrespective of PWM
volatile	uint8_t		TACH_LOW_offset		= 10;
volatile	uint8_t		TACH_HIGH_offset	= 10;
volatile	uint8_t		PWM_LOW_offset		= 10;
volatile	uint8_t		PWM_HIGH_offset		= 10;
volatile	uint8_t		CURRENT_SETTING		= 0;	//contains the setting currently being manipulated

//						 1     2     3     4     5     6     7     8    9    10   11   12   13   14   15    16    17    18    19
//						 500   1000  1500  2000  2500  3000  3500  4000 4500 5000 6000 7000 8000 9000 10000 11000 12000 13000 14000
uint16_t tach_lut[20] = {7500, 3750, 2500, 1875, 1500, 1250, 1071, 938, 833, 750, 625, 536, 469, 417, 375,  341,  313,  288,  268};
	
//button variables
volatile uint8_t		button_pressed		= 0;	//boolean telling the main loop whether a button was pressed

int main(void)
{
	uint16_t temp_setting = 6;
	
	load_settings_from_eeprom();
	initialize_hardware();
    while(1)
    {
        //buttons are sensed by PCINT1
		//if a button press is sensed (i.e. any change in level), button_pressed
		//will contain information on which button was pressed (1 = up, 2 = down)
		//this is used to modify the offset setting, then translate it into a the physical setting,
		//store it in EEPROM and lastly we wait 50ms and rearm the interrupt
		if(button_pressed){
			switch(CURRENT_SETTING){
				case 0:	//pwm_low
					if(button_pressed == 1 && PWM_LOW_offset < 19)	PWM_LOW_offset++;
					if(button_pressed == 2 && PWM_LOW_offset > 0)	PWM_LOW_offset--;
					PWM_LOW_SETTING		= 256 + (PWM_LOW_offset - 10) * 25;
					temp_setting		= PWM_LOW_offset;
					break;
				case 1: //pwm high
					if(button_pressed == 1 && PWM_HIGH_offset < 19) PWM_HIGH_offset++;
					if(button_pressed == 2 && PWM_HIGH_offset > 0)	PWM_HIGH_offset--;
					PWM_HIGH_SETTING	= 512 + (PWM_HIGH_offset - 10) * 25;
					temp_setting		= PWM_HIGH_offset;
					break;
				case 2: //tach_low
					if(button_pressed == 1 && TACH_LOW_offset < 19) TACH_LOW_offset++;
					if(button_pressed == 2 && TACH_LOW_offset > 0)	TACH_LOW_offset--;
					TACH_LOW_SETTING	= tach_lut[TACH_LOW_offset];
					temp_setting		= TACH_LOW_offset;
					break;
				case 3: //tach high
					if(button_pressed == 1 && TACH_HIGH_offset < 19) TACH_HIGH_offset++;
					if(button_pressed == 2 && TACH_HIGH_offset > 0)	TACH_HIGH_offset--;
					TACH_HIGH_SETTING	= tach_lut[TACH_HIGH_offset];
					temp_setting		= TACH_HIGH_offset;
					break;
			}
			
			eeprom_write_byte((uint8_t *) CURRENT_SETTING, temp_setting);	//write the updated value to EEPROM
			_delay_ms(50);													//wait 50ms as 'debounce'
			button_pressed	= 0;											//and cleared for the next button press
			PORTA		   ^= ALL_LED_MASK;									//do stuff with the LEDs as user feedback
		}		
    }
}

/****************************/
/* INITIALIZATION FUNCTIONS */
/****************************/

void load_settings_from_eeprom(void){
	//load the individual settings from EEPROM
	PWM_LOW_offset		= eeprom_read_byte((uint8_t *) 0); if(PWM_LOW_offset	> 19)	PWM_LOW_offset		= 10;
	PWM_HIGH_offset		= eeprom_read_byte((uint8_t *) 1); if(PWM_HIGH_offset	> 19)	PWM_HIGH_offset		= 10;
	TACH_LOW_offset		= eeprom_read_byte((uint8_t *) 2); if(TACH_LOW_offset	> 19)	TACH_LOW_offset		= 10;
	TACH_HIGH_offset	= eeprom_read_byte((uint8_t *) 3); if(TACH_HIGH_offset	> 19)	TACH_HIGH_offset	= 10;
	CURRENT_SETTING		= eeprom_read_byte((uint8_t *) 4); 
	
	//increment current setting, check if it isn't exceeding our settings pages and re-store the incremented value to EEPROM
	CURRENT_SETTING++; if(CURRENT_SETTING > MAX_SETTING) CURRENT_SETTING = 0;
	eeprom_write_byte((uint8_t *) 4, CURRENT_SETTING);
	
	//translate the offset settings into actual usable numbers for our calculations
	TACH_LOW_SETTING	= tach_lut[TACH_LOW_offset];
	TACH_HIGH_SETTING	= tach_lut[TACH_HIGH_offset];
	PWM_LOW_SETTING		= 256 + (PWM_LOW_offset - 10) * 25;
	PWM_HIGH_SETTING	= 512 + (PWM_HIGH_offset - 10) * 25;
	
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
	//PCIE0 is not set in GIMSK for PCINT0 because it is only armed during a measurement cycle
	
	//initialize PCINT1
	//PCINT1 triggers on any state change of the UP and DOWN buttons
	PCMSK1 |= (1 << PCINT8) | (1 << PCINT9);
	GIMSK  |= (1 << PCIE1);
	
	//INT0 is also used, but needs no initialization code here	
	
	//initialize TCNT0
	//Timer/counter0 is used for the PWM generation, and only that. It just dutifully outputs pwm_period at 32500Hz
	TCCR0B	= TIMER_CLKSEL_DIV1;
	TIMSK0	= TIMER_ENABLE_OVF_INTERRUPT | TIMER_ENABLE_COMPA_INTERRUPT;
	OCR0A	= 25;	//default is 10%, this gets overwritten after the first measurement cycle though
		
	//initialize TCNT1
	//Timer/counter1 is a 16-bit counter that just counts at clkio/1
	//There are two operational modes: 
	// 1) regular pulse-generating mode. Here it outputs a '2 pulses per revolution' tachometer signal. The max RPM signal is 20krpm, or ocr1a=200. The on-time is always 100 cpu cycles (=12.5us)
	// 2) measurement mode. Here we measure:
	//    a) PWM period     -> pwm_period_m
	//    b) PWM duty cycle -> pwm_duty_m
	//    c) tach period    -> tach_period_m
	// mode 1 is active unless meas=0, where it sweeps the three measurements.
	TCCR1A	= (1 << WGM11);																	//see TCCR1B
	TCCR1B	= TIMER_CLKSEL_DIV64 | (1 << WGM13) | (1 << WGM12);								//run at clkio/64, use WGM3:0=1110, i.e. fast PWM with TOP=ICR1 and both COMPA and COMPB intact
	TIMSK1	= TIMER_ENABLE_OVF_INTERRUPT | TIMER_ENABLE_COMPA_INTERRUPT;					//enable overflow and compare A interrupt
	OCR1A	= 100;																			//duty is 100
	ICR1	= 500;																			//period is 500, equivalent to 7500rpm. This gets overwritten very quickly by the first measurement cycle
	
	sei();																					//enable global interrupts
}

/**********************/
/* INTERRUPT HANDLERS */
/**********************/

//timer0 overflow interrupt, set MOSI_PWMOUT
ISR(TIM0_OVF_vect){
	PORTA |= MOSI_PWMOUT_PIN;
	meas++;
	if(meas > MEAS_TIMEOUT){
		meas = 0;
		currently_measuring = 1;
	}
}

//timer0 compare A interrupt, clear MOSI_PWMOUT
ISR(TIM0_COMPA_vect){
	PORTA &= ~MOSI_PWMOUT_PIN;
}

//timer1 overflow interrupt, set MISO_TACHMOBO
//timer1 overflow also acts as an arbiter for the measurement cycle
ISR(TIM1_OVF_vect){
	if(!currently_measuring){		//mode 1: just output PWM
		PORTA |= MISO_TACHMOBO_PIN;
	} else {						//mode 2, entered when currently_measuring = 1, starts or continues the measurement cycle. See documents for a schematic overview of this system
		switch(meas_cycle){
			case 0:	//just getting started
				PORTA &= ~(MISO_TACHMOBO_PIN);
				meas_cycle = 1;
				ICR1 = 18750;												//the slowest fan speed we want to measure is 200rpm, which is 18750 cycles
				GIMSK |= (1 << PCIE0);										//arm PCINT0				
				break;
			case 1: //skip to 2
			case 2:	//we should not get here unless the measurement cycle timed out, which means no tachometer signal was detected
				//we assume that no fan is present, so we default to the low tach setting
				meas_cycle = 3;												//skip to PWM measurement (note there is no break)
				GIMSK &= ~(1 << PCIE0);										//disarm PCINT0
				tach_period_m = 65535;										//make sure finalize_measurements understands we've timed out					
			case 3: //starting the PWM measurement, we need to run the clock much faster to capture it.
				meas_cycle = 4;												//make sure we never end up here again
				TCCR1B = TIMER_CLKSEL_DIV1 | (1 << WGM13) | (1 << WGM12);	//run at clkio/1
				ICR1 = 375;													//the maximum possible time a PWM cycle can last is about 375 cpu cycles (20% buffer on top of 25kHz), so lets double that and call it quits
				TCNT1 = 0;													//reset the clock
				MCUCR |= (1 << ISC00) | (1 << ISC01);						//arm INT0 on positive edge
				GIMSK |= (1 << INT0);
				break;
			case 4: 
			case 5:
			case 6:
				//ending up at 4, 5 or 6 means we've timed out of either period or duty cycle measurement.
				//this can mean three things: We're measuring no flanks because PWM is 100%, PWM is 0% or no PWM is present
				GIMSK &= ~(1 << INT0);										//disarm INT0
				pwm_period_m = 65535;										//make sure finalize_measurements understands we've timed out
				pwm_duty_m = 65535;											//same
				meas_cycle = 7;
				finalize_measurements();
				break;
			case 7:
				//we're done. We accidentally triggered this overflow, because finalize_measurements is supposed to take care of everything.
				break;
		}
	}
}

//timer1 compare A interrupt, clear MISO_TACHMOBO
ISR(TIM1_COMPA_vect){
	PORTA &= ~MISO_TACHMOBO_PIN;
}

//PCINT0 is armed on SCK_TACHOUT
ISR(PCINT0_vect){
	if(PINA & SCK_TACHOUT_PIN){	//positive edge
		if(meas_cycle == 1){
			meas_cycle = 2;
			TCNT1 = 0;	//reset timer
		} else if(meas_cycle == 2){
			meas_cycle = 3;
			tach_period_m = TCNT1;
			GIMSK &= ~(1 << PCIE0);		//disarm PCINT0
			TCNT1 = 18740;
			//now we just let it overflow and let TIMER1_OVF handle the rest
		}
	}
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

//INT0 is used in the measurement cycle to trigger on the PWM signal from the motherboard
ISR(EXT_INT0_vect){
	switch(meas_cycle){
		case 4:	//start: we're synced with a positive flank
			TCNT1 = 0;
			meas_cycle = 5;
			break;
		case 5: //we captured a second positive flank
			pwm_period_m = TCNT1;
			TCNT1 = 0;
			MCUCR &= ~(1 << ISC00);	//arm INT0 to negative edge
			meas_cycle = 6;
			break;
		case 6: //and we captured a negative flank after a positive one
			pwm_duty_m = TCNT1;
			GIMSK &= ~(1 << INT0);	//disarm INT0
			meas_cycle = 7;
			finalize_measurements();
			break;
	}			
}

//this routine gets called after both tachometer and PWM measurements are done. This routine translates the measured (xxxx_m) values into their physical values
//and programs them into the timers to output
void finalize_measurements(void){
	uint16_t temp;
	
	//return timer1 to defaults	
	TCCR1B = TIMER_CLKSEL_DIV64 | (1 << WGM13) | (1 << WGM12);	
	meas_cycle = 0;
	currently_measuring = 0;
	
	//TACH_PERIOD
	if(tach_period_m == 65535){	//timeout on tach_period
		//set tach to lowest allowed setting
		tach_period = TACH_LOW_SETTING;
	} else {
		if(tach_period_m > TACH_LOW_SETTING){		tach_period = TACH_LOW_SETTING;}	//clip on low
		else if(tach_period_m < TACH_HIGH_SETTING){ tach_period = TACH_HIGH_SETTING;}	//clip on high
		else{										tach_period = tach_period_m;}		//otherwise just transmit exactly what you measure
		ICR1 = tach_period;																//set period
		OCR1A = tach_period >> 1;														//and make sure it's a 50% square wave
	}
	
	//PWM
	//this math is a bit more annoying, because we need to scale instead of use absolute values.
	//0-100% PWM input translates into 0[+/-100%]-100[+/-100]% PWM
	
	//first off, deal with the trivial case
	if(pwm_period_m == 65535 || pwm_duty_m == 65535){									//no signal is present or the duty timed out
		pwm_duty = PWM_LOW_SETTING;														//default output is PWM_LOW_SETTING, a value between 0-512
		if(pwm_duty < 256)	pwm_duty = 1;												//if the setting is less than 256, clip it to 1
		if(pwm_duty > 255)	pwm_duty -= 255;											//otherwise just subtract 255
		OCR0A = pwm_duty;																//and throw it into OCR0A
	} else {		//valid signal is present
		//First, we need to calculate the % PWM normalized to 0-256. Ideally we just scale by a float, but this is too expensive. 
		//best alternative is to integer multiply and divide, i.e. we want to do: pwm_duty = 256 x pwm_duty_m / pwm_period_m
		//say the period is 320 and duty cycle is 10%, we get 64 x 32 / 320 = 25. Bang on.
		//unfortunately, this clashes with the max. value we get in the next calculation, so we have to scale everything down by 2 bits
		pwm_period_m >>= 2;
		pwm_duty = (pwm_duty_m << 6) / pwm_period_m;
		
		//now we know the pwm duty cycle input, we need to transform this to output. 
		//the output is ((64 - pwm_duty) x pwm_low + pwm_duty x pwm_high) / 64
		//so for instance if it is 25% (pwm_duty = 64) and pwm_low=0, pwm_high = 256, we get [(64 - 64) x 0 + 16 x 256]/64 = 64
		temp = (64 - pwm_duty) * PWM_LOW_SETTING;	//add contribution of PWM_LOW_SETTING
		temp += pwm_duty * PWM_HIGH_SETTING;		//add contribution of PWM_HIGH_SETTING
		temp >>= 6;									//divide by 64
		
		if(temp < 257) temp = 257;					//check if its 'negative' (less than 256)
		if(temp > 511) temp = 511;					//check if its 'more than 100%' (more than 511)
		
		temp -= 256;								//shift range to 0-256
		
		OCR0A = (uint8_t) temp;						//and throw it into the mix
	}	
}