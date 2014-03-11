/*
 * LP.c
 * 
 * Created: 27.11.2013 22:09:58
 *  Author: The Long-Run Smoke
 */ 

#define Red 1
#define Blue 3
#define Green 4
#define F_CPU 8000000
#define Hysteresis 3
#define Vb 178
#define Vmax Vb+Hysteresis
#define Vmin Vb-Hysteresis

#include <util/delay.h>
#include <avr/io.h>
#include <avr/iotn85.h>
#include <avr/interrupt.h>

volatile uint8_t Count = 0;
volatile uint8_t TimeForWork = 0;
volatile uint8_t Voltage = 0;
volatile uint16_t VoltageArray = 0;
volatile uint16_t Hue = 0;
volatile uint16_t HueArray = 0;
volatile uint8_t VoltageTime = 1;

void Init(void)
{
	//Настройка пинов
	DDRB = (1<<PB0); //PB0 на выход
	//Предварительная настройка АЦП	
	ADMUX = (1<<ADLAR);				//Выравнивание влево
	DIDR0 = (1<<ADC0D)|(1<<ADC1D);	//Отключаю триггера на входах АЦП
	ADCSRA = (1<<ADEN);				//Включаю АЦП
	//Настойка TIMER0 для SEPIC
	TCCR0A = (1<<COM0A1)|(1<<WGM00)|(1<<WGM01);		//ШИМ на PB0/OC1A, FastRWM.
	//Настройка TIMER1 для ШИМ
	GTCCR = (1<<PWM1B)|(1<<COM1B0);
	TCCR1 = (1<<PWM1A)|(1<<COM1A1);
	PLLCSR = (1<<PLLE);					//Запуск асинхронного ВЧ осцилятора (64МГц)
	while (((PLLCSR & (1<<PLOCK)) == 0)){}	//Жду выход ВЧ осцилятора в режим.
	PLLCSR |= (1<<PCKE);				//Переключаю на асинхрон.
	TCCR1 |= (1<<CS10);		//Запуск TIMER1 без пределителя, F = 64 МГц
	TCCR0B |= (1<<CS00);	//Запуск TIMER0 без пределителя, F = 8 МГц
	TIMSK |= (1<<TOIE1);	//Разрешаю прерывания по переполнению TIMER1
}

ISR( TIM1_OVF_vect){
	if (Count < 251)	//Отсчитываю 251 переполнение = 1 мс
	{
		Count++;
	} 
	else
	{
		Count = 0;
		TimeForWork = 1;
	}
}

int main(void)
{
	Init();
	OCR0A = 0x80;		    //Устанавливаю заполнение для SEPIC  D = 0.5
	while(1)
    { 
		  asm("cli");
		  if (TimeForWork == 1)
		  {
			  if (VoltageTime == 1)
			  {
				  ADMUX |= (1<<MUX0)|(1<<REFS1);	//Подключаю канал PB2/ADC1, устанавливаю Vref = 1.1в
				  _delay_ms(1);
				  VoltageArray = 0;
				  for (uint8_t i=0; i<5; i++)
				  {
					  ADCSRA |= (1<<ADSC);
					  while (ADCSRA & (1<<ADSC)){}
					  VoltageArray += ADCH;
				  }
				  Voltage = VoltageArray/5;
				  VoltageTime = 0;
				  if (Voltage > Vmax)
				  {
					  OCR0A--;
				  } 
				  else
				  {
					  if (Voltage < Vmin)
					  {
						  OCR0A++;
					  }
				  }
			  } 
			  else
			  {
				  ADMUX &= ~((1<<MUX0)|(1<<MUX1)|(1<<MUX2)|(1<<MUX3)|(1<<REFS1)|(1<<REFS0)|(1<<REFS2));	//Подключаю канал PB5/ADC0, Vref = Vcc
				  HueArray = 0;
				  for (uint8_t i=0; i<6; i++)
				  {
					  ADCSRA |= (1<<ADSC);
					  while (ADCSRA & (1<<ADSC)){}
					  HueArray += ADCH;
				  }
				  Hue = ((HueArray/6) * 3.08) + 14; //3.08 - коэффициент растяжения, нужен для работы 4 пункта, 14 - калибровочный.
				  VoltageTime = 1;
			  }
			  TimeForWork = 0;
		  }
		  asm("sei");
			
			
		  switch ( (Hue - ( Hue % 255 )) / 255 )
		  {
			  case 0: {
				  if ( (DDRB & ((1<<Red)|(1<<Blue)|(1<<Green))) != ((1<<Red)|(1<<Green)) )
				  {
					  DDRB |= (1<<Red)|(1<<Green);
					  DDRB &= ~(1<<Blue);
				  }
				  OCR1A = 255 - (Hue % 255); //Красный
				  OCR1B = Hue;		 //Зеленый
				  break;
			  }
			  case 1: {
				  if ( (DDRB & ((1<<Red)|(1<<Green)|(1<<Blue))) != ((1<<Green)|(1<<Blue)) )
				  {
					  DDRB |= (1<<Green)|(1<<Blue);
					  DDRB &= ~(1<<Red);
				  }
				  OCR1B = 255 - ( Hue % 255 ); //Синий, зеленый в инверсе				  
				  break;
			  }
			  case 2: {
				  if ( (DDRB & ((1<<Red)|(1<<Green)|(1<<Blue))) != ((1<<Red)|(1<<Blue)) )
				  {
					  DDRB |= (1<<Red)|(1<<Blue);
					  DDRB &= ~(1<<Green);
				  }
				  OCR1A = Hue % 255; //Красный
				  OCR1B = Hue % 255;	//Синий, он тоже инверсный
				  break;
			  }
			  case 3: {
				  if ( (DDRB & ((1<<Red)|(1<<Blue)|(1<<Green))) != ((1<<Red)|(1<<Blue)|(1<<Green)) )
				  {
					  DDRB |= (1<<Red)|(1<<Blue)|(1<<Green);
				  }
				  OCR1A = 0x80;
				  OCR1B = 0x80;
				  break;
			  }
			  default: ;
		  }

    }
}