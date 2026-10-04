/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"


/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int _write(int fd, char *ptr, int len)
{
  (void)fd;
  HAL_UART_Transmit(&huart2, (uint8_t *)ptr, (uint16_t)len, HAL_MAX_DELAY);
  return len;
}

static void report(const char *what, HAL_StatusTypeDef st)
{
  printf("  %-22s %s\r\n", what, st == HAL_OK ? "OK" : "FAIL");
}

static void i2c_scan(void)
{
  int found = 0;
  printf("  I2C1 scan:");
  for (uint8_t addr = 0x08; addr <= 0x77; addr++)
  {
    if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(addr << 1), 1, 5) == HAL_OK)
    {
      printf(" 0x%02X", addr);
      found++;
    }
  }
  printf(found ? "\r\n" : " none\r\n");
}

static const uint32_t ldr_channels[4] = { ADC_CHANNEL_0, ADC_CHANNEL_1, ADC_CHANNEL_4, ADC_CHANNEL_18 };

static HAL_StatusTypeDef adc_select(uint32_t channel, uint32_t rank)
{
  ADC_ChannelConfTypeDef c = {0};
  c.Channel = channel;
  c.Rank = rank;
  c.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
  return HAL_ADC_ConfigChannel(&hadc1, &c);
}

static HAL_StatusTypeDef adc_read_all(uint16_t out[4])
{
  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < 4; j++)
    {
      if (adc_select(ldr_channels[j], j == i ? ADC_RANK_CHANNEL_NUMBER : ADC_RANK_NONE) != HAL_OK)
      {
        printf("  ADC: config ch%d failed\r\n", j);
        return HAL_ERROR;
      }
    }
    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
      printf("  ADC: start failed\r\n");
      return HAL_ERROR;
    }
    if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK)
    {
      printf("  ADC: timeout on ch%d\r\n", i);
      HAL_ADC_Stop(&hadc1);
      return HAL_TIMEOUT;
    }
    out[i] = (uint16_t)HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
  }
  return HAL_OK;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_SPI1_Init();
  MX_TIM3_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  printf("hello from STM32C031 (heliostat LR1)\r\n");
  printf("SYSCLK = %lu Hz\r\n", HAL_RCC_GetSysClockFreq());

  printf("MX init: GPIO ADC1 I2C1 SPI1 TIM3 USART2 -> HAL_OK\r\n");
  printf("Bring-up checks:\r\n");

  report("TIM3 CH1 PWM start", HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1));
  report("TIM3 CH2 PWM start", HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2));

  uint8_t dummy[10];
  memset(dummy, 0xFF, sizeof dummy);
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
  report("SPI1 80 dummy clocks", HAL_SPI_Transmit(&hspi1, dummy, sizeof dummy, 100));

  report("I2C1 OLED @0x3C ready", HAL_I2C_IsDeviceReady(&hi2c1, 0x3C << 1, 3, 10));
  i2c_scan();

  uint16_t ldr[4];
  report("ADC1 4-channel scan", adc_read_all(ldr));
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {    
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    HAL_Delay(1000);
    HAL_GPIO_TogglePin(LD4_GPIO_Port, LD4_Pin);

    static int flip = 0;
    flip ^= 1;
    uint32_t az_us = flip ? 1000 : 2000;
    uint32_t el_us = flip ? 2000 : 1000;
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, az_us);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, el_us);

    if (adc_read_all(ldr) == HAL_OK)
      printf("tick %lu | LDR TL=%4u TR=%4u BL=%4u BR=%4u | servo AZ=%lu EL=%lu us\r\n",
             HAL_GetTick() / 1000, ldr[0], ldr[1], ldr[2], ldr[3], az_us, el_us);
    else
      printf("tick %lu | ADC error | servo AZ=%lu EL=%lu us\r\n",
             HAL_GetTick() / 1000, az_us, el_us);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
