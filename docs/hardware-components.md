# Hardware Components

Проєкт: геліостат — двовісна платформа сонячної панелі, що стежить за джерелом світла, на базі STM32.

Платформа: STM32 Nucleo-64 **NUCLEO-C031C6** (MCU STM32C031C6T6, Arm Cortex-M0+, 48 MHz, 32 KB Flash, 12 KB RAM). У симуляторі Wokwi використовується `board-st-nucleo-c031c6`.

Для реалізації проєкту використовуються компоненти з різними інтерфейсами: ADC, PWM (TIM), I2C, SPI та GPIO. Для діагностики та виведення інформації використовується UART.

Тактування: HSI48 → HSISYS (/1) → SYSCLK = HCLK = PCLK = 48 MHz.

---

## 1. Фоторезистори GM5528 (4 шт.)

**Призначення:**  
Чотири фоторезистори розташовані квадрантом (верх-ліво, верх-право, низ-ліво, низ-право). Різниця освітленості між ними показує, в який бік потрібно повернути панель.

**Інтерфейс:** ADC

**Модель у Wokwi:** `wokwi-photoresistor-sensor` — модуль, у якому фоторезистор увімкнений послідовно з резистором 10 кОм, вихід AO знімається з середньої точки дільника.

**Підключення:**

| Датчик | Мітка в CubeMX | Пін | Канал ADC |
|---|---|---|---|
| верх-ліво | `LDR_TL` | PA0 | ADC1_IN0 |
| верх-право | `LDR_TR` | PA1 | ADC1_IN1 |
| низ-ліво | `LDR_BL` | PA4 | ADC1_IN4 |
| низ-право | `LDR_BR` | PB1 | ADC1_IN18 |

Для кожного модуля: VCC -> 3.3V, GND -> GND, AO -> пін ADC.

**Параметри ADC:**
- ADC: ADC1
- Resolution: 12 bit, діапазон цифрових значень 0–4095
- Clock Prescaler: Synchronous clock mode divided by 2 (PCLK / 2 = 24 MHz)
- Sampling Time Common 1: 39.5 cycles
- Режим: програмний запуск, по одному перетворенню на канал

**Залежність сигналу від освітленості:**  
Чим яскравіше світло, тим менший опір фоторезистора і тим **нижча** напруга на AO. Для модуля Wokwi (rl10 = 50 кОм, gamma = 0.7) при VCC = 5 V документація наводить: 10 000 lux → 0.19 V, 1 000 lux → 0.83 V, 400 lux → 1.37 V, 100 lux → 2.50 V. У проєкті модуль живиться від 3.3 V, тому напруги пропорційно менші.

**Функція у проєкті:**  
Визначення напрямку на джерело світла (порівняння квадрантів) та рівня загальної освітленості (нічний режим).

**Перевірка:**  
Значення чотирьох каналів виводяться у UART щосекунди:

```text
tick 1 | LDR TL=xxxx TR=xxxx BL=xxxx BR=xxxx | servo AZ=1000 EL=2000 us
```

При зміні параметра `lux` будь-якого датчика у Wokwi значення відповідного каналу змінюється.

**Особливість симуляції:**  
Багатоканальне сканування секвенсором ADC (один запуск → чотири перетворення) у Wokwi для STM32C031 не завершується — `HAL_ADC_PollForConversion` повертає timeout. Тому канали зчитуються по одному: у секвенсорі залишається лише потрібний канал (`ADC_RANK_NONE` для решти), далі Start → Poll → GetValue → Stop.

**Документація:**
- [Wokwi: wokwi-photoresistor-sensor](https://docs.wokwi.com/parts/wokwi-photoresistor-sensor)
- [STM32C031C6 datasheet](https://www.st.com/resource/en/datasheet/stm32c031c6.pdf) — характеристики ADC

---

## 2. Сервоприводи SG92R (2 шт.)

**Призначення:**  
Повертають панель по двох осях: азимут (горизонтально) та елевація (вертикально).

**Інтерфейс:** PWM (таймер TIM3)

**Модель у Wokwi:** `wokwi-servo`

**Підключення:**

| Серво | Пін | Сигнал | Alternate function |
|---|---|---|---|
| азимут (AZ) | PA6 | TIM3_CH1 | AF1 |
| елевація (EL) | PA7 | TIM3_CH2 | AF1 |

Для кожного серво: PWM -> пін, GND -> GND, V+ -> живлення.

**Живлення:**  
У Wokwi серво під'єднані до 3.3V, оскільки симулятор не моделює струм. Реальний SG92R живиться від 5 V і під навантаженням споживає сотні міліампер, тому на реальній платі потрібне окреме джерело 5 V зі спільною землею з Nucleo.

**Параметри TIM3:**
- Clock Source: Internal Clock (48 MHz)
- Prescaler: 47 → 48 MHz / 48 = 1 MHz, 1 тік = 1 мкс
- Counter Period: 19999 → період 20 000 мкс = 50 Hz
- Auto-reload preload: Enable
- PWM mode 1, Pulse при старті: 1500 мкс (середнє положення)
- Робочий діапазон імпульсу: 1000–2000 мкс

**Функція у проєкті:**  
Встановлення кутів панелі за командами алгоритму стеження.

**Перевірка:**  
`HAL_TIM_PWM_Start` для обох каналів повертає `HAL_OK`. У тестовому циклі імпульс щосекунди перемикається між 1000 та 2000 мкс — у Wokwi обидва серво рухаються між крайніми положеннями.

Логічний аналізатор Wokwi (канал D2 → PA6) зафіксував:
- період сигналу: 20 000 мкс (50 Hz);
- ширина імпульсів: 1000, 1500 та 2000 мкс.

**Документація:**
- [Wokwi: wokwi-servo](https://docs.wokwi.com/parts/wokwi-servo)
- [STM32C031C6 datasheet](https://www.st.com/resource/en/datasheet/stm32c031c6.pdf) — таблиця alternate functions (TIM3_CH1/CH2)

---

## 3. OLED-дисплей SSD1306

**Призначення:**  
Відображення стану системи: режим роботи, кути серво, значення фоторезисторів.

**Інтерфейс:** I2C

**I2C адреса:** 0x3C

**Модель у Wokwi:** `board-ssd1306` (128×64)

**Підключення:**
- VCC -> 3.3V
- GND -> GND
- SCL -> PB8 (I2C1_SCL, AF6)
- SDA -> PB9 (I2C1_SDA, AF6)

**Параметри I2C1:**
- Speed Mode: Standard Mode, 100 kHz
- Timing: 0x10805D88 (розраховано CubeMX для 48 MHz)
- Addressing: 7 bit
- Пін-режим: Alternate Function Open-Drain

**Функція у проєкті:**  
Локальний інтерфейс користувача без підключення до комп'ютера.

**Перевірка:**  
Наявність дисплея на шині перевіряється функцією:

```c
HAL_I2C_IsDeviceReady(&hi2c1, 0x3C << 1, 3, 10);
```

Сканування шини (адреси 0x08–0x77) знаходить один пристрій:

```text
I2C1 OLED @0x3C ready  OK
I2C1 scan: 0x3C
```

Логічний аналізатор (D0 → PB8, D1 → PB9) показує тактові імпульси SCL з періодом 10 мкс (100 kHz) та пакети по 9 тактів (адреса + ACK) під час сканування.

**Документація:**
- [Wokwi: board-ssd1306](https://docs.wokwi.com/parts/board-ssd1306)
- [SSD1306 datasheet (Solomon Systech)](https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf)

---

## 4. microSD-карта

**Призначення:**  
Енергонезалежне зберігання журналу роботи (кути, освітленість, зміни режимів) та калібрувальних коефіцієнтів.

**Інтерфейс:** SPI

**Модель у Wokwi:** `wokwi-microsd-card` (FAT16, 8 MB)

**Підключення:**

| Пін модуля | Пін MCU | Сигнал |
|---|---|---|
| SCK | PB3 | SPI1_SCK (AF0) |
| DO (MISO) | PB4 | SPI1_MISO (AF0) |
| DI (MOSI) | PB5 | SPI1_MOSI (AF0) |
| CS | PB0 | GPIO Output, мітка `SD_CS` |
| VCC | 3.3V | |
| GND | GND | |

**Параметри SPI1:**
- Mode: Full-Duplex Master
- Hardware NSS: Disable (CS керується програмно через PB0)
- Data Size: 8 bit, MSB First
- CPOL = Low, CPHA = 1 Edge (SPI Mode 0)
- Prescaler: 128 → 48 MHz / 128 = 375 kHz (SD-карта при ініціалізації вимагає ≤ 400 kHz)
- CS при старті: High (карта не вибрана)

**Чому microSD, а не W25Q64:**  
У каталозі проєктів для геліостата запропоновано SPI-флеш W25Q64, але у Wokwi такого компонента немає. microSD також працює по SPI, а журнал у файловій системі FAT можна прочитати на комп'ютері.

**Функція у проєкті:**  
Журнал роботи та збереження конфігурації між перезапусками.

**Перевірка:**  
Перед ініціалізацією SD-карта потребує щонайменше 74 тактів SCK при CS = High. Прошивка надсилає 10 байт 0xFF (80 тактів):

```text
SPI1 80 dummy clocks   OK
```

Логічний аналізатор (D3 → PB3) зафіксував 80 тактів SCK з періодом ≈ 2.67 мкс (≈ 375 kHz).

**Документація:**
- [Wokwi: wokwi-microsd-card](https://docs.wokwi.com/parts/wokwi-microsd-card)
- [SD Association: Physical Layer Simplified Specification](https://www.sdcard.org/downloads/pls/) — SPI-режим, ініціалізація

---

## 5. Кнопка B1 та світлодіод LD4

**Призначення:**  
Кнопка — перемикання режиму роботи (автоматичний / ручний). Світлодіод — індикація роботи прошивки.

**Інтерфейс:** GPIO

**Підключення (вбудовані на платі Nucleo):**
- B1 -> PC13, GPIO Input, мітка `B1`
- LD4 -> PA5, GPIO Output Push-Pull, мітка `LD4`, при старті Low

**Перевірка:**  
LD4 перемикається щосекунди в основному циклі прошивки.

**Документація:**
- [Wokwi: board-st-nucleo-c031c6](https://docs.wokwi.com/parts/board-st-nucleo-c031c6)

---

## 6. USART2

**Призначення:**  
Діагностика системи та виведення інформації у Serial Monitor Wokwi; надалі — командний інтерфейс (CLI).

**Інтерфейс:** UART

**Параметри:**
- Baud rate: 115200
- Data bits: 8
- Stop bits: 1
- Parity: None
- Mode: TX/RX

**Підключення:**
- PA2 -> USART2_TX -> Serial Monitor RX
- PA3 -> USART2_RX -> Serial Monitor TX

`printf` перенаправлено в USART2 через перевизначення функції `_write`:

```c
int _write(int fd, char *ptr, int len)
{
  (void)fd;
  HAL_UART_Transmit(&huart2, (uint8_t *)ptr, (uint16_t)len, HAL_MAX_DELAY);
  return len;
}
```

**Перевірка:**

```text
hello from STM32C031 (heliostat LR1)
SYSCLK = 48000000 Hz
MX init: GPIO ADC1 I2C1 SPI1 TIM3 USART2 -> HAL_OK
Bring-up checks:
  TIM3 CH1 PWM start     OK
  TIM3 CH2 PWM start     OK
  SPI1 80 dummy clocks   OK
  I2C1 OLED @0x3C ready  OK
  I2C1 scan: 0x3C
  ADC1 4-channel scan    OK
tick 1 | LDR TL=... TR=... BL=... BR=... | servo AZ=1000 EL=2000 us
```

---

## 7. Логічний аналізатор Wokwi

**Призначення:**  
Перевірка того, що на шинах присутні реальні сигнали (заміна Saleae у формі B).

**Підключення:**
- D0 -> PB8 (I2C1_SCL)
- D1 -> PB9 (I2C1_SDA)
- D2 -> PA6 (TIM3_CH1, PWM азимуту)
- D3 -> PB3 (SPI1_SCK)
- GND -> GND

Після зупинки симуляції Wokwi зберігає захоплення у файл `wokwi.vcd`, який відкривається у PulseView або GTKWave.

---

## Підсумок

| Компонент | Інтерфейс | Пін(и) | Статус перевірки |
|---|---|---|---|
| Фоторезистори GM5528 ×4 | ADC | PA0, PA1, PA4, PB1 | OK |
| Сервоприводи SG92R ×2 | PWM (TIM3) | PA6, PA7 | OK, рух у Wokwi |
| OLED SSD1306 | I2C | PB8, PB9 | OK, знайдено 0x3C |
| microSD | SPI | PB3, PB4, PB5, PB0 | OK, 80 тактів SCK |
| Кнопка B1 / LED LD4 | GPIO | PC13, PA5 | OK |
| USART2 | UART | PA2, PA3 | OK |

У проєкті використовуються компоненти з п'ятьма різними інтерфейсами: ADC, PWM, I2C, SPI та GPIO. USART2 використовується як додатковий інтерфейс для діагностики та виведення результатів.
