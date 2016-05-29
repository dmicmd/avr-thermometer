// sudo avrdude -p attiny13 -c usbtiny -U flash:w:t.hex:i

/* ПО для мк ATtiny13a с датчиком температуры DS1621 и
 * двумя семисегментными индикаторами.
 * Раз в секунду читает температуру по протоколу I2C
 * и последовательно выводит в два сдвиговых регистра 74HC595.
 * Повышенная точность датчика не используется, так как 
 * для неё нет индикатора.
 */

// Частота микропроцессора
#define F_CPU 9600000UL

// Ввод/вывод, задержка
#include <avr/io.h>
#include <util/delay.h>

// Порты для семисегментных индикаторов на регистрах-защелках
#define LCD_DATA PB3
#define LCD_CLK PB2
#define LCD_LATCH PB4

// Порты для протокола I2C
#define SDA PB1
#define SCL PB0

// I2C протокол
#define SDA_UP() {DDRB &= ~(1<<SDA); _delay_us(10);}
#define SDA_DOWN() {DDRB |= 1<<SDA; _delay_us(10);}
#define SCL_UP() {DDRB &= ~(1<<SCL); _delay_us(10);}
#define SCL_DOWN() {DDRB |= 1<<SCL; _delay_us(10);}

// Адрес датчика температуры
#define SLAVE_ADDRESS 0b10010000
// Коды управления датчиком
#define CONFIG_CODE 0xAC
#define START_CONV_CODE 0xEE
#define READ_TEMP_CODE 0xAA
#define ONE_SHOT_MODE 0b00000011

// Вывод на индикаторы
void lcd_write(uint8_t number)
{
	uint8_t out_data;
	uint8_t i,j;
	const uint8_t symbol[] = {
		0b01111110,  // 0
		0b00110000,  // 1
		0b01101101,  // 2
		0b01111001,  // 3
		0b00110011,  // 4
		0b01011011,  // 5
		0b01011111,  // 6
		0b01110000,  // 7
		0b01111111,  // 8
		0b01111011,  // 9
		0b00000000   // 10, пустой символ
	};

	// Вывод двух младших разрядов числа
	for (j = 0; j < 2; j++) {
		// Младший разряд числа во временную переменную
		out_data = number % 10;

		// Ноль в старшем разряде заменяется на пустой символ
		if (out_data == 0 && j == 1) out_data = 10;

		// Из массива изображение нужного символа
		out_data = symbol[out_data];

		// Проход по битам символа
		for (i = 0; i < 8; i++)
		{
			// Отправка крайнего бита в порт
			if (out_data & 1)
			{
				// Установка бита данных
				PORTB |= 1<<LCD_DATA;
			}
			else
			{
				// Сброс бита данных
				PORTB &= ~(1<<LCD_DATA);
			}

			// Тактирование
			PORTB &= ~(1<<LCD_CLK);
			PORTB |= 1<<LCD_CLK;

			// Сдвиг битов вправо на один
			out_data >>= 1;
		}

		// Сдвиг разрядов числа вправо на один
		number /= 10;
	}

	// Защелкивание, выставляет загруженные данные на выход регистров
	PORTB &= ~(1<<LCD_LATCH);
	PORTB |= 1<<LCD_LATCH;
}

// Отправка одного байта с проверкой передачи в интерфейс I2C
uint8_t i2c_send_byte(uint8_t out_data)
{
	uint8_t i;
	uint8_t ack;

	// Передача байта
	for(i = 0; i < 8; i++)
	{
		if(0b10000000 & out_data) SDA_UP() else SDA_DOWN();
		SCL_UP();
		SCL_DOWN();
		out_data <<= 1;
	}

	// Сохранение кода подтверждения в ack
	SDA_UP(); // не ап, а отпускание линии в данном случае
	SCL_UP();
	ack = PINB & 1<<SDA;
	SCL_DOWN();

	// Если не было подтверждения (в SDA высокий уровень), то возврат 1
	if (ack) return 1;

	return 0;
}

// Отправка команды старта конвертации или включения 1shot mode
uint8_t ds1621_write(uint8_t command_code)
{
	// Старт посылки
	SDA_UP();
	SCL_UP();
	SDA_DOWN();

	// Прижимаем SCL к земле
	SCL_DOWN();

	// Если прижать не удалось — уходим, линия занята
	if (PINB & 1<<SCL) return 1;

	// Если отправка адреса или кода команды не удалась - выход
	if(i2c_send_byte(SLAVE_ADDRESS) || i2c_send_byte(command_code)) return 1;

	// Если запись в конфигурационный байт, то отправить код 1shot режима
	if (command_code == CONFIG_CODE)
	{
		if(i2c_send_byte(ONE_SHOT_MODE)) return 1;
	}

	// Стоп посылки
	SDA_DOWN();
	SCL_UP();
	SDA_UP();

	return 0;
}

// Чтение одного или двух байт (в зависимости от command_code) из термометра
uint8_t ds1621_read(uint8_t command_code)
{
	uint8_t i;
	uint8_t in_data = 0;

	// Старт посылки
	SDA_UP();
	SCL_UP();
	SDA_DOWN();

	// Прижимаем SCL к земле
	SCL_DOWN();

	// Если прижать не удалось — уходим, линия занята
	if (PINB & 1<<SCL) return 1;

	// Если отправка адреса или кода команды не удалась - выход
	if(i2c_send_byte(SLAVE_ADDRESS) || i2c_send_byte(command_code)) return 1;

	// Повторный старт (особенность чтения)
	SDA_UP();
	SCL_UP();
	SDA_DOWN();
	SCL_DOWN();

	// Отправка адреса с битом для чтения (адрес | бит чтения)
	if(i2c_send_byte(SLAVE_ADDRESS | 1)) return 1;

	// Отпускание линии
	SDA_UP();

	// Чтение байта
	for(i = 0; i < 8; i++)
	{
		SCL_UP();
		in_data |= ((PINB >> SDA) & 1) << (7 - i);
		SCL_DOWN();
	}

	// Для температуры нужно читать еще один байт
	if (command_code == READ_TEMP_CODE)
	{
		// Подтверждение принятия
		SDA_DOWN();
		SCL_UP();
		SCL_DOWN();
		SDA_UP();

		// Имитация чтения байта, он нужен только для точности в полградуса
		for(i = 0; i < 8; i++)
		{
			SCL_UP();
			SCL_DOWN();
		}
	}

	// Подтверждение принятия
	SDA_UP();
	SCL_UP();
	SCL_DOWN();

	// Стоп посылки
	SDA_DOWN();
	SCL_UP();
	SDA_UP();

	return in_data;
}

int main(void)
{
	// Переменная под температуру, 88 включает все светодиоды
	uint8_t temperature = 88;

	// Порты семисегментных индикаторов на выход
	DDRB = 1<<LCD_DATA | 1<<LCD_CLK | 1<<LCD_LATCH;

	// Порты шины I2C на вход, уровень изменяется направлением
	DDRB = ~(1<<SDA | 1<<SCL);

	// Сброс порта B
	PORTB = 0x00;

	// Включение one shot mode на термометре
	ds1621_write(CONFIG_CODE);

	// Основной цикл
	while(1)
	{
		// Вывод на индикаторы
		lcd_write(temperature);

		// Задержка для того, чтобы не дергать датчик часто
		_delay_ms(1000);

		// Если конвертация готова (старший бит выставлен), то читать температуру
		if(ds1621_read(CONFIG_CODE) & 0b10000000)
		{
			// Чтение температуры
			temperature = ds1621_read(READ_TEMP_CODE);

			// Старт новой конвертации
			ds1621_write( START_CONV_CODE );
		}
	};

	return 0;
}
