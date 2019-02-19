// Enable debug prints to serial monitor
#define MY_DEBUG

// Enable and select radio type attached
#define MY_RADIO_RF24

// Enable repeater functionality for this node
//#define MY_REPEATER_FEATURE
//#define MY_REGISTRATION_FEATURE;
//#define MY_REGISTRATION_DEFAULT;
//#define MY_TRANSPORT_MAX_TX_FAILURES 5;
//#define MY_REGISTRATION_RETRIES 1;
//#define MY_REGISTRATION_DEFAULT;
//set how long to wait for transport ready in milliseconds
//set how long to wait for transport ready in milliseconds
#define MY_TRANSPORT_WAIT_READY_MS 10000

#include "timer-api.h"
#include <Wire.h>
#include <Adafruit_ADS1015.h>
#include <MySensors.h>
#include <Bounce2-master\Bounce2.h>

#define VOlT_CHILD_ID 13
#define RELAY_CHILD_ID 14
#define MODE_WORK_CHILD_ID 15
#define MY_NODE_ID 19

#define RELAY_PIN 4     // Arduino Digital I/O pin number for first relay (second on pin+1 etc)
#define LED_State_PIN 6 // номер вывода светодиода
#define BUTTON_PIN 7    // номер вывода кнопки
#define RELAY_ON 0  // GPIO value to write to turn on attached relay
#define RELAY_OFF 1 // GPIO value to write to turn off attached relay
#define Buzzer 3

//установка состояний реле из eeprom
/*
void before()
{
int sensor = 1;
// Then set relay pins in output mode
pinMode(RELAY_PIN, OUTPUT);
// Set relay to last known state (using eeprom storage)
digitalWrite(RELAY_PIN, loadState(sensor) ? RELAY_ON : RELAY_OFF);
}
*/


Adafruit_ADS1015 ads(0x48);		// 0x48 - ADDR pin connected to GROUND (default)
Bounce debouncer = Bounce();
int oldValue = -1; // переменная для антидребезга

MyMessage msgMode(MODE_WORK_CHILD_ID, V_VAR1);
MyMessage msgVLT(VOlT_CHILD_ID, V_VOLTAGE);
MyMessage msgSTATE(RELAY_CHILD_ID, V_STATUS);

#define xGAIN GAIN_TWOTHIRDS   // делитель
#define x1BIT 3      // значение 1 бита
#define const_12V 295 //коэффициент преобразования для делителя напряжения
int i_volt = 0;
unsigned long time_ms = 0;
int16_t ADCres;				// переменная для чтения результата 16 бит напряжения
float Voltage = 0.0;		// переменная для расчета напряжения
int volt_int = 0;			//значение напряжения в int*1000
boolean out_v_12 = false;	//признак выдачи напряжения на потребители
boolean OldOut_v_12  = false;
bool timer_100 = false;		//флаг срабатывания 100 Гц счетчика

int volt_last = 0;			//предыдущеее значение напряжения при анализе падения напряжения
int volt_now = 0;			//текущее напряжение при анализе падения напряжения
int count_low = 0;			//счетчик пониженного напряжения

bool timer_3000 = false;	//флаг срабытывания 3 с счетчика	

bool state; // текущее состояние реле

int i_3000 = 0;				//
int delayLED = 0;
int longvalue = 0; //длинное нажатие кнопки
int CountValue = 0;//счетчик циклов 3 сек
int CountTimer3000 = 0;
int StartLongLoadOFF = 0;
bool metric = true;
int StateCONTROL = 0; /*режима работы самой коробки 0 - питание выключено
													1 - питание выключено двигатель выключен
													2 - питание вклювено режим длительного выключения
													3 - питание включено по кнопке
													4 - пиатние включено двигатель запущен
					  
					  */ 

void setup()
{
	//Serial.begin(9600);
	timer_init_ISR_100Hz(TIMER_DEFAULT);
	ads.begin();
	ads.setGain(xGAIN);
	Serial.println("GO");

	pinMode(Buzzer, OUTPUT);
	digitalWrite(Buzzer, LOW);

	pinMode(LED_State_PIN, OUTPUT);
	StateLED();
	pinMode(RELAY_PIN, OUTPUT);
	digitalWrite(RELAY_PIN, HIGH);
	pinMode(BUTTON_PIN, INPUT);

	digitalWrite(BUTTON_PIN,HIGH);
	debouncer.attach(BUTTON_PIN);
	debouncer.interval(5);

	metric = getControllerConfig().isMetric;
	BuzzerSate(0);
}
void presentation()
{
	// Send the sketch version information to the gateway and Controller
	sendSketchInfo("POWER_SHIELD", "1.0");

	present(MODE_WORK_CHILD_ID, S_BINARY,"Statys work MODE");
	present(VOlT_CHILD_ID, S_MULTIMETER, "Volt");
	present(RELAY_CHILD_ID, S_BINARY, "STATE_Relay");
	
	metric = getControllerConfig().isMetric;
}

//главный цикл работы 100Гц
void timer_handle_interrupts(int timer)
{
	timer_100 = true;
	i_3000++;

	if (i_3000 == 300)	{	
		i_3000 = 0;
		timer_3000 = true; //начало работы 3 сек циклы работы	
		CountTimer3000++;
	}
}


void loop()
{
	LoadLongOFF(StartLongLoadOFF);//обработчик длинного нажатия
	//непосредственное включение назрузки
	if (out_v_12 != OldOut_v_12)
	{
		if (out_v_12 == true) {
			digitalWrite(RELAY_PIN, LOW);
			Serial.println("Relay ON_1");
		}

		else
		{
			digitalWrite(RELAY_PIN, HIGH);
			Serial.println("Relay OFF_1");
		}
			

		OldOut_v_12 = out_v_12;
	}
	debouncer.update();

	int value = debouncer.read();				// текущее состояние кнопки
	
	//если изменилось состояние кнопки
	if (value != oldValue)
	{
		Serial.println(value);
		// тут описывать поведение кнопки в том числеи длинные и короткие нажатия
		//включение нагрузки по нажатию кнопки
		if (value == 0 && out_v_12 == false && longvalue == 0) {
			StateCONTROL = 3;
			BuzzerSate(3);
			Serial.println("Relay ON_Button");
			out_v_12 = true;
		} 
		//выключение нагрузки придлительном нажатии
		if (longvalue == 1 && out_v_12 == true) {
			BuzzerSate(2);
			delayLED = 1;//всключить режим моргания светодиода каждую секунду
			Serial.println("Button long OFF");
			StartLongLoadOFF = CountTimer3000;
			LoadLongOFF (RELAY_ON);		//выключение нагрузки через 1 час
			longvalue = 0;
		}

		if (value == 1) {
			CountValue = 0;
			//longvalue = 0;
		}
			
		oldValue = value;
	}
	// 100 мс цикл работы
	if (timer_100 == true)
	{
		StateLED();
		ADCres = ads.readADC_SingleEnded(0);		 // читаем результат единичного преобразования
		Voltage = ADCres * x1BIT;			// const_12V;				// расчитываем напряжение
		volt_int = (Voltage * 1000) / const_12V;
		//включение нагрузки если крутит стартер
		if ((volt_int < 10900 && volt_int > 9000 && out_v_12 == false) || (volt_int > 13500 && out_v_12 == false))
		{
			count_low++;

			if (count_low > 3 && volt_int > 12500)	{
				out_v_12 = true;
				BuzzerSate(4);
				StateCONTROL = 4;
				Serial.println("Power_ON_Drive");
				count_low = 0;
			}
		}

		timer_100 = false;
	}


	//3 сек цикл работы 
	if (timer_3000 == true)
	{
		//если нажата и удерживается кнопка более 6 секунд то longvalue = 1;
		if (value == 0) {
			CountValue++;
		}			
		if (value == 0 && CountValue == 2) {
			longvalue = 1;
			CountValue = 0;
		}
		////////////////////////////////////////////////////////////////////////
		// если питание на потребители подано
			if (out_v_12 == true && StateCONTROL != 3)
			{
				volt_now = volt_int; //

				//если текущее напряжение меньше предыдущего и напряжение меньше 13В то инкримент события
				if (volt_now < volt_last && volt_now < 13000)
				{
					//volt_last = volt_now;
					i_volt++;
					Serial.println("++");
				}

				//если в течении 2 минут падение напряжения то выключить потребителей
				if (i_volt > 20)
				{
					Serial.println("Power_OFF");
					out_v_12 = false;
					StateCONTROL = 1;
					i_volt = 0;
					BuzzerSate(1);
				}
				volt_last = volt_now;
			}
		///////////////////////////////////////////////////////////////////////
		send(msgVLT.set(volt_int,1));
		Serial.print("Input VOLT = ");
		Serial.println(volt_int);
		Serial.print("mode WORK = ");
		Serial.println(StateCONTROL);
		send(msgMode.set(StateCONTROL));//лучше наверное передавать не состояние кнопки а состояние режима работы дублироваться будет миганием светодиода
		//send(msgSTATE.set(state));
		timer_3000 = false;
	}
}

void receive(const MyMessage &message)
{
	// We only expect one type of message from controller. But we better check anyway.
	if (message.type == V_VAR2) {
		BuzzerSate(5);
	}
	if (message.type == V_STATUS) {
		// Change relay state
		state = message.getBool();
		
		digitalWrite(RELAY_PIN, state ? RELAY_ON : RELAY_OFF);
		// Store state in eeprom
		saveState(message.sensor, message.getBool());
		// Write some debug info
		Serial.print("Incoming change for sensor:");
		Serial.print(message.sensor);
		Serial.print(", New status: ");
		Serial.println(message.getBool());
	}
}

void LoadLongOFF(int i)
{
	if (i > 1){
		//через CountTimer3000 - StartLongLoadOFF выключится нагрузка
		StateCONTROL = 3;
		if (CountTimer3000 - StartLongLoadOFF > 30 && StateCONTROL != 1) {
			Serial.println("Relay LONG OFF");
			delayLED = 0;//включить режим светодиода постоянно гореть
			out_v_12 = false;
			StartLongLoadOFF = 0;
			i = 0;
			StateCONTROL = 1;
		}
	}
	
}

void StateLED() {
	switch (delayLED)
	{ 
	case 0:
		digitalWrite(LED_State_PIN, HIGH);
		break;
	case 1:
		if (i_3000 == 0 || i_3000 == 50 || i_3000 == 100 || i_3000 == 150 || i_3000 == 200 || i_3000 == 250 || i_3000 == 300 ) {
			if (i_3000 == 0 || i_3000 == 200) {
				digitalWrite(LED_State_PIN, HIGH);
			} else {
				digitalWrite(LED_State_PIN, LOW);
			}
		}
		break;
	case 2:
		break;


	}
		

}
void BuzzerSate(int i) {
	switch (i)
	{
	case 0:// длиннный 2-х секундный писк
		digitalWrite(Buzzer, HIGH);
		wait(2000);
		digitalWrite(Buzzer, LOW);
		break;
	case 1://один длинный пикс и один короткий
		digitalWrite(Buzzer, HIGH);
		wait(3000);
		digitalWrite(Buzzer, LOW);
		wait(500);
		digitalWrite(Buzzer, HIGH);
		wait(500);
		digitalWrite(Buzzer, LOW);
		break;
	case 2:// три длинных писка - длинное снятие питаиния
		digitalWrite(Buzzer, HIGH);
		wait(1000);
		digitalWrite(Buzzer, LOW);
		wait(500);
		digitalWrite(Buzzer, HIGH);
		wait(1000);
		digitalWrite(Buzzer, LOW);
		wait(500);
		digitalWrite(Buzzer, HIGH);
		wait(1000);
		digitalWrite(Buzzer, LOW);
		break;
	case 3:// два коротких писка - включние питания по кнопке
		digitalWrite(Buzzer, HIGH);
		wait(500);
		digitalWrite(Buzzer, LOW);
		wait(500);
		digitalWrite(Buzzer, HIGH);
		wait(500);
		digitalWrite(Buzzer, LOW);
		break;

	case 4:// два длинных писка - включение питания если двигатель запущен.
		digitalWrite(Buzzer, HIGH);
		wait(1000);
		digitalWrite(Buzzer, LOW);
		wait(500);
		digitalWrite(Buzzer, HIGH);
		wait(1000);
		digitalWrite(Buzzer, LOW);
		break;

	case 5:// 4 коротких писка - установка на охрану
		digitalWrite(Buzzer, HIGH);
		wait(500);
		digitalWrite(Buzzer, LOW);
		wait(500);
		digitalWrite(Buzzer, HIGH);
		wait(500);
		digitalWrite(Buzzer, LOW);
		wait(500);
		digitalWrite(Buzzer, HIGH);
		wait(500);
		digitalWrite(Buzzer, LOW);
		wait(500);
		digitalWrite(Buzzer, HIGH);
		wait(500);
		digitalWrite(Buzzer, LOW);
		break;

	}
}