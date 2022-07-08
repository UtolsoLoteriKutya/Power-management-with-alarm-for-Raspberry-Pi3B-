/*
 * rpi_pm.c
 *
 * Created: 2022. 05. 09. 16:23:01
 * Author : huboiv
 */ 

#define TMR_PON		120000		// 120000 ~2mp
#define TMR_BOOTING	1800000		// 1800000 ~30mp
#define TMR_SHDN	1400000		// 1000000
#define TMR_PWROFF	40000

#define LED_PWM_MAX	200
#define LED_PWM_MIN	1

#define POWER_LED_IN	(1<<3) // PB3
#define ALARM_IN		(1<<4) // PB4
#define PEN_OUT			(1<<2) // PB2
#define LED1			(1<<1) // PB1
#define LED2			(1<<0) // PB0

#define POWER_LED_IN_DDR	DDRB
#define ALARM_IN_DDR		DDRB
#define PEN_OUT_DDR			DDRB

#define POWER_LED_IN_PORT	PORTB
#define ALARM_IN_PORT		PORTB
#define PEN_OUT_PORT		PORTB

#define POWER_LED_IN_PIN	PINB
#define ALARM_IN_PIN		PINB

#define LED1_DDR			DDRB
#define LED1_PORT			PORTB

#define LED2_DDR			DDRB
#define LED2_PORT			PORTB

#define isPowerLedIn	(POWER_LED_IN_PIN & POWER_LED_IN)
#define isAlarmIn		(ALARM_IN_PIN & ALARM_IN)

#define PenBe()	(PEN_OUT_PORT |= PEN_OUT)
#define Penki()	(PEN_OUT_PORT &= ~PEN_OUT)

#define Led1Be()	(LED1_PORT |= LED1)
#define Led1Ki()	(LED1_PORT &= ~LED1)

#define Led2Be()	(LED2_PORT |= LED2)
#define Led2Ki()	(LED2_PORT &= ~LED2)

#include <avr/io.h>
#include <avr/interrupt.h>

volatile uint8_t PowerLedFelfutoElDetektalva;
volatile uint8_t PowerLedLefutoElDetektalva;
volatile uint8_t AlarmLefutoElDetektalva;
// volatile uint8_t AlarmBemenetValtozas;
volatile uint8_t prevIsAlarmIn = 1;


#define PON_TMR		0	// Bekapcsolás után egy kis várakozás.
#define BOOTING		1	// Bootolás közben. Várjuk a Power LED alacsony szintet.
#define RUNNING		2	// Rendszer fut.
#define SHDN_TMR	3	// Leállítási folyamat közben, de lehet csak reboot.
#define STOP_TMR	4	// Rendszer leállítva és várunk egy kicsit PEN tiltásig.
#define POWER_OFF	5	// A rendszer továbbra is áll, PEN tiltás.

volatile uint8_t state = 0;

#define GYORS 3
#define LASSU 15

volatile uint8_t LedPwmDir	= 1;
volatile uint16_t IntDiv = 0;
volatile uint16_t IntSebesseg = LASSU;
volatile uint8_t IntVillogasEngedelyezes = 0;
volatile uint8_t IntLedPwm = 0;

volatile uint32_t tmrcnt;

void IoInit(void) {
	PEN_OUT_PORT |= PEN_OUT;				// Alapból magasszint, csak leállítás után alacsony indulásig.
	PEN_OUT_DDR |= PEN_OUT;
	LED1_DDR |= LED1;
	LED2_DDR |= LED2;
	POWER_LED_IN_DDR &= ~POWER_LED_IN;	// Bemenet.
	POWER_LED_IN_PORT |= POWER_LED_IN;	// Pull up.
	ALARM_IN_DDR &= ~ALARM_IN;
	ALARM_IN_PORT |= ALARM_IN;
	
	// MCUCR |= (1<<ISC01) | (1<<ISC01);	// Bármilyen logikai szintváltozásra INT0 megszakítás.
	GIMSK |= (0<<INT0) | (1<<PCIE);			// (INT0 nem) és PCIE engedélyezése.
	PCMSK |= (1<<PCINT3) | (1<<PCINT4);		// ALARM (PB3) bemenet változás megszakítás maszk.

	TCCR0A |= (1 << WGM01) | (1 << WGM00);
	TIMSK0 |= (1 << TOIE0);
	TCCR0B |= (1 << CS01) | (0 << CS00);
}

void LedPulsatingEnable(uint8_t value) {
	if(value) {
		OCR0A = value;
		} else {
		OCR0A = LED_PWM_MIN;
	}
	IntVillogasEngedelyezes = 1;
	TCCR0A |= (1 << COM0A1);
}

void LedPulsatingDisable(void) {
	IntVillogasEngedelyezes = 0;
	OCR0A = LED_PWM_MIN;
	TCCR0A &= ~(1 << COM0A1);
}

int main(void) {
    
	IoInit();

	PenBe();
	
	// Egy kis várakozás, mi lesz?
	PowerLedFelfutoElDetektalva = 0;
	PowerLedLefutoElDetektalva = 0;
	AlarmLefutoElDetektalva = 0;
	
	sei(); // Globális megszakítás engedélyezés.

	tmrcnt = TMR_PON;
	
	while (1) {
		switch(state) {
			case PON_TMR :
				PenBe();
				LedPulsatingDisable();
				tmrcnt--;
				if(tmrcnt == 0) { state = BOOTING; tmrcnt = TMR_BOOTING; }
				break;

			case BOOTING :
				LedPulsatingDisable();
				Led1Be();
				Led2Be();
				PenBe();
				tmrcnt--;			
				if(tmrcnt == 0) {
					state = RUNNING;
					PowerLedFelfutoElDetektalva = 0;
					PowerLedLefutoElDetektalva = 0;
				}
				break;

			case RUNNING :
				LedPulsatingDisable();
				PenBe();
				Led2Be();
				if(PowerLedFelfutoElDetektalva) {
					state = SHDN_TMR;
					tmrcnt=TMR_SHDN;
					PowerLedFelfutoElDetektalva = 0;
					PowerLedLefutoElDetektalva = 0;
				}
				break;

			case SHDN_TMR :
				LedPulsatingDisable();
				PenBe();
				tmrcnt--;
				if(tmrcnt == 0) { state = STOP_TMR; tmrcnt = TMR_PWROFF; }
				if(PowerLedLefutoElDetektalva) {
					state = RUNNING;
					PowerLedLefutoElDetektalva = 0;
					PowerLedFelfutoElDetektalva = 0;
					Led2Ki();
				}
				break;

			case STOP_TMR :
				PenBe();
				tmrcnt--;
				if(tmrcnt == 0) { state = POWER_OFF; AlarmLefutoElDetektalva = 0; LedPulsatingEnable(LED_PWM_MAX); }
				break;

			case POWER_OFF :
				Led1Ki();
				Penki();
				if(AlarmLefutoElDetektalva) { state = BOOTING; tmrcnt = TMR_BOOTING; AlarmLefutoElDetektalva = 0; }
				break;
		} // switch

    } // while
}

/*
// Power LED bemenet.
ISR(INT0_vect) {
	if(isPowerLedIn) {
		PowerLedLefutoElDetektalva = 0;
		PowerLedFelfutoElDetektalva = 1;
	} else {
		PowerLedFelfutoElDetektalva = 0;
		PowerLedLefutoElDetektalva = 1;
	}
}
*/

// Power LED, Alarm bemenet.
ISR(PCINT0_vect) {
	if(isAlarmIn && prevIsAlarmIn == 0) {	// Felfutó él.
		prevIsAlarmIn = 1;
		// AlarmLefutoElDetektalva = 1;
	}
	
	if(!isAlarmIn && prevIsAlarmIn == 1) {	// Lelfutó él.
		prevIsAlarmIn = 0;
		AlarmLefutoElDetektalva = 1;
	}

	if(!isPowerLedIn) {
		PowerLedLefutoElDetektalva = 0;
		PowerLedFelfutoElDetektalva = 1;
		} else {
		PowerLedFelfutoElDetektalva = 0;
		PowerLedLefutoElDetektalva = 1;
	}
}

// LED villogáshoz.
ISR(TIM0_OVF_vect) {
	if(IntVillogasEngedelyezes) {
		if(IntDiv == IntSebesseg) {
			IntDiv = 0;
			// Fenyero fokozas / csokkentes.
			OCR0A += LedPwmDir;
			// Iranyvaltas.
			if(OCR0A > LED_PWM_MAX || OCR0A < LED_PWM_MIN) LedPwmDir = -LedPwmDir;
		}
		IntDiv++;
	}
}
