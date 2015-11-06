/* Macros */
#define		TIMER_CLKSEL_DIV1				(1 << CS10)
#define		TIMER_CLKSEL_DIV64				(1 << CS10) | (1 << CS11)
#define		TIMER_ENABLE_OVF_INTERRUPT		(1 << TOIE1)
#define		TIMER_ENABLE_COMPA_INTERRUPT	(1 << OCIE0A)

#define		MEAS_TIMEOUT					10000
#define		MAX_SETTING						3

/* pins */
//PORTA
#define		ADJ_PWMOUT_MIN_PIN	(1 << 0)
#define		ADJ_PWMOUT_MAX_PIN	(1 << 1)
#define		ADJ_TACHOUT_MIN_PIN	(1 << 2)
#define		ADJ_TACHOUT_MAX_PIN	(1 << 3)
#define		SCK_TACHOUT_PIN		(1 << 4)
#define		MISO_TACHMOBO_PIN	(1 << 5)
#define		MOSI_PWMOUT_PIN		(1 << 6)
#define		NTC_PIN				(1 << 7)

#define		ALL_LED_MASK		0b00001111

//PORTB
#define		UP_PIN				(1 << 1)	//ERRATUM: Up&Down on schematic wrong way around compared to layout, so we're fixing this in firmware
#define		DOWN_PIN			(1 << 0)	//ERRATUM: Up&Down on schematic wrong way around compared to layout, so we're fixing this in firmware
#define		PWM_IN_PIN			(1 << 2)
#define		SELECT_PIN			(1 << 3)

/* Function prototypes */
void initialize_hardware(void);
void finalize_measurements(void);
void load_settings_from_eeprom(void);