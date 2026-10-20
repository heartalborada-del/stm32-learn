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
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdbool.h"
#include "string.h"
#include "stdio.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SERVO_STOP_US 1500  /* 停止脉宽 (us) */
#define SERVO_RANGE_US 1000 /* 0~全速对应的脉宽偏移 (us)：1000~2000 */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
void controller_debounce();

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim == &htim3)
  { /* 10ms trigger once */
    controller_debounce();
  }
}
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/*
0:x
1:y
*/
uint16_t debounce_con[2][3] = {0};
volatile uint16_t adc_value[2] = {0, 0};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void controller_debounce()
{
  // read adc value
  volatile uint16_t adc_value_raw[2] = {0, 0};
  adc_value_raw[0] = HAL_ADC_GetValue(&hadc1);
  adc_value_raw[1] = HAL_ADC_GetValue(&hadc2);

  // dead zone
  const int min = 50;
  const int max = 4000;
  for (int i = 0; i < 2; i++)
  {
    if (adc_value_raw[i] < min)
      adc_value_raw[i] = min;
    else if (adc_value_raw[i] > max)
      adc_value_raw[i] = max;
    // slide window filter
    for (int j = 0; j < 2; j++)
    {
      debounce_con[i][j] = debounce_con[i][j + 1];
    }
    debounce_con[i][2] = adc_value_raw[i];
    adc_value[i] = (debounce_con[i][0] + debounce_con[i][1] + debounce_con[i][2]) / 3;
  }
}

void servo_control360(uint8_t speed, bool isReverse)
{
  /* 把 0~255 映射到 0~SERVO_RANGE_US */
  uint16_t offset = ((uint32_t)speed * SERVO_RANGE_US) / 255u;

  uint16_t pulse_width;
  if (isReverse)
  {
    pulse_width = SERVO_STOP_US + offset;
  }
  else
  {
    pulse_width = SERVO_STOP_US - offset;
  }

  if (pulse_width < 500)
    pulse_width = 500;
  if (pulse_width > 2500)
    pulse_width = 2500;

  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse_width);
}

void servo_control(uint8_t angle, bool isReverse)
{
  /* 把 0~180 映射到 0~SERVO_RANGE_US */
  uint16_t offset = ((uint32_t)angle * SERVO_RANGE_US) / 180u;

  uint16_t pulse_width;
  if (isReverse)
  {
    pulse_width = SERVO_STOP_US + offset;
  }
  else
  {
    pulse_width = SERVO_STOP_US - offset;
  }

  if (pulse_width < 500)
    pulse_width = 500;
  if (pulse_width > 2500)
    pulse_width = 2500;

  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse_width);
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
  MX_TIM2_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  MX_ADC2_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start_IT(&htim3);
  /* USER CODE BEGIN 2 */
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);

  // 校准电压
  HAL_ADCEx_Calibration_Start(&hadc1);
  HAL_ADCEx_Calibration_Start(&hadc2);

  // 启动ADC转换
  HAL_ADC_Start(&hadc1);
  HAL_ADC_Start(&hadc2);
  servo_control(0, false);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t last_ctrl = 0;
  uint32_t last_uart = 0;
  servo_control(255, 0);
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t now = HAL_GetTick();
    /* ================= 舵机控制：每 20ms 一次 ================= */
    if (now - last_ctrl >= 20)
    {
      last_ctrl = now;

      int16_t y = (int16_t)adc_value[1]; /* Y 轴控制舵机 */
      int16_t diff = y - 2048;           /* 相对中心 2048 的偏移 */

      if (diff >= -100 && diff <= 100)
      {
        /* 中心死区 ±100 内：停转 */
        servo_control(0, false);
      }
      else
      {
        /* 死区外：|diff| 从 100~2048 映射到 -90-90 */
        int32_t mag = (diff > 0) ? diff : -diff;
        int32_t spd = ((mag - 100) * 180) / (2048 - 100);

        if (spd > 180)
          spd = 180;
        if (spd < 0)
          spd = 0;

        /* diff > 0 → 一个方向，diff < 0 → 另一个方向
           若舵机转向与摇杆相反，把 true/false 互换 */
        servo_control((uint8_t)spd, diff > 0);
      }
    }

    /* ================= 串口输出：每 100ms 一次 ================= */
    if (now - last_uart >= 100)
    {
      last_uart = now;

      char buffer[50];
      int len = snprintf(buffer, sizeof(buffer),
                         "ADC1: %d, ADC2: %d\r\n",
                         adc_value[0], adc_value[1]);
      HAL_UART_Transmit(&huart1, (uint8_t *)buffer, len, 100);
    }
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
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
