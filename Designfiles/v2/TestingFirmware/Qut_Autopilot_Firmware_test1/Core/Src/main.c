/* USER CODE BEGIN Header */
/**
 * *****************************************************************************
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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
SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;

TIM_HandleTypeDef htim1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
int16_t x_offset = 0;
int16_t y_offset = 0;

extern TIM_HandleTypeDef htim1; // Reference the handle created by MX
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_USART1_UART_Init();
  MX_TIM1_Init();
  /* USER CODE BEGIN 2 */


  /* 1. SPI BUS SILENCE & POWER-UP */
  // Silence SPI1 (Main Sensors)
  HAL_GPIO_WritePin(GPIOC, ICM_CS_Pin, GPIO_PIN_SET);        // PC2 [cite: 152]
  HAL_GPIO_WritePin(GPIOC, ICM2_CS_Pin, GPIO_PIN_SET);       // PC15 [cite: 217]
  HAL_GPIO_WritePin(GPIOE, MAG_CS_Pin, GPIO_PIN_SET);        // PE14 [cite: 284]

  // Silence SPI2 (Baro & FRAM) - CRITICAL to prevent bus noise
  HAL_GPIO_WritePin(GPIOD, BARO_CS_Pin, GPIO_PIN_SET);       // PD7 [cite: 177]
  HAL_GPIO_WritePin(GPIOD, FRAM_CS_Pin, GPIO_PIN_SET);       // PD10 (Corrected FRAM_CS)

  // LEDs OFF (Active-Low)
  HAL_GPIO_WritePin(GPIOB, GREEN_LED_Pin|RED_LED_Pin|BLUE_LED_Pin, GPIO_PIN_SET);

  // Enable sensor power rails
  HAL_GPIO_WritePin(GPIOE, VDD_3V3_SENSORS_EN_Pin, GPIO_PIN_SET); // PE3 [cite: 233]
  HAL_GPIO_WritePin(GPIOC, VDD_3V3_PERIPH_EN_Pin, GPIO_PIN_SET);  // PC4 [cite: 165]
  HAL_Delay(200); // Stabilization time

  uint8_t id_reg, id_val;
  uint16_t C[7]; // MS5611 Calibration coefficients
  /* --- SPI1 TESTS --- */
  // ICM-42688-P Test
  uint8_t reset_42688[2] = {0x11, 0x01}; // Soft Reset
  HAL_GPIO_WritePin(GPIOC, ICM_CS_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi1, reset_42688, 2, 10);
  HAL_GPIO_WritePin(GPIOC, ICM_CS_Pin, GPIO_PIN_SET);
  HAL_Delay(50);

  id_reg = 0x75 | 0x80;
  HAL_GPIO_WritePin(GPIOC, ICM_CS_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi1, &id_reg, 1, 10);
  HAL_SPI_Receive(&hspi1, &id_val, 1, 10);
  HAL_GPIO_WritePin(GPIOC, ICM_CS_Pin, GPIO_PIN_SET);
  if (id_val != 0x47) { while(1) { HAL_GPIO_TogglePin(GPIOB, RED_LED_Pin|GREEN_LED_Pin); HAL_Delay(200); } }

  // ICM-20608 Test
  HAL_GPIO_WritePin(GPIOC, ICM2_CS_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi1, &id_reg, 1, 10);
  HAL_SPI_Receive(&hspi1, &id_val, 1, 10);
  HAL_GPIO_WritePin(GPIOC, ICM2_CS_Pin, GPIO_PIN_SET);
  if (id_val != 0xAF) { while(1) { HAL_GPIO_TogglePin(GPIOB, GREEN_LED_Pin|BLUE_LED_Pin); HAL_Delay(200); } }

  /* 4. SENSOR 3: LIS3MDL MAGNETOMETER (PE14) */

  // Step A: Soft Reset & Reboot Memory
  uint8_t mag_reset[2] = {0x21, 0x04 | 0x08}; // SOFT_RST + REBOOT [cite: 1611, 1612]
  HAL_GPIO_WritePin(GPIOE, MAG_CS_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi1, mag_reset, 2, 10);
  HAL_GPIO_WritePin(GPIOE, MAG_CS_Pin, GPIO_PIN_SET);
  HAL_Delay(50);

  // Step B: WAKE UP - Set Operating Mode to Continuous [CRITICAL]
  // CTRL_REG3 (22h), MD[1:0] = 00
  uint8_t mag_wakeup[2] = {0x22, 0x00};
  HAL_GPIO_WritePin(GPIOE, MAG_CS_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi1, mag_wakeup, 2, 10);
  HAL_GPIO_WritePin(GPIOE, MAG_CS_Pin, GPIO_PIN_SET);
  HAL_Delay(10); // Small settle time

  // Step C: Read WHO_AM_I (0Fh)
  id_reg = 0x0F | 0x80; // bit 0 = 1 for READ [cite: 1490]
  id_val = 0;
  HAL_GPIO_WritePin(GPIOE, MAG_CS_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi1, &id_reg, 1, 10);
  HAL_SPI_Receive(&hspi1, &id_val, 1, 10);
  HAL_GPIO_WritePin(GPIOE, MAG_CS_Pin, GPIO_PIN_SET);

  if (id_val != 0x3D) { // Expected value is 0x3D [cite: 1558, 1584]
      while(1) {
          HAL_GPIO_TogglePin(GPIOB, RED_LED_Pin | BLUE_LED_Pin);
          HAL_Delay(200);
      }
  }

  /* --- SPI2 TESTS (BARO & FRAM) --- */
  // MS5611 Barometer Reset & PROM Check
  /* 5. MS5611 BAROMETER (SPI2) */
  uint8_t baro_reset = 0x1E;
  HAL_GPIO_WritePin(GPIOD, BARO_CS_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi2, &baro_reset, 1, 10);
  HAL_GPIO_WritePin(GPIOD, BARO_CS_Pin, GPIO_PIN_SET);
  HAL_Delay(20);

  // Read PROM Calibration Coefficients C1-C6
  for (uint8_t i = 1; i <= 6; i++) {
      uint8_t cmd = 0xA0 + (i * 2);
      uint8_t data[2];
      HAL_GPIO_WritePin(GPIOD, BARO_CS_Pin, GPIO_PIN_RESET);
      HAL_SPI_Transmit(&hspi2, &cmd, 1, 10);
      HAL_SPI_Receive(&hspi2, data, 2, 10);
      HAL_GPIO_WritePin(GPIOD, BARO_CS_Pin, GPIO_PIN_SET);
      C[i] = (data[0] << 8) | data[1];
  }
  if (C[1] == 0 || C[1] == 0xFFFF) { HAL_GPIO_WritePin(GPIOB, RED_LED_Pin, GPIO_PIN_RESET); while(1); }
  /* --- FM25V02A FRAM SYSTEM TEST (PD10) --- */

  // 1. Recovery & Wakeup
  // Ensures the chip exits Sleep Mode (B9h) or a mid-transaction hang.
  HAL_GPIO_WritePin(GPIOD, FRAM_CS_Pin, GPIO_PIN_RESET);
  HAL_Delay(1);
  HAL_GPIO_WritePin(GPIOD, FRAM_CS_Pin, GPIO_PIN_SET);
  HAL_Delay(5); // tREC recovery time

  // 2. Identity Verification (RDID)
  // Scans for Manufacturer ID 0x04 (Cypress) or 0xC2 (Ramtron)
  uint8_t rdid_tx[12] = {0x9F};
  uint8_t rdid_rx[12] = {0};
  uint8_t fram_identified = 0;

  HAL_GPIO_WritePin(GPIOD, FRAM_CS_Pin, GPIO_PIN_RESET);
  HAL_SPI_TransmitReceive(&hspi2, rdid_tx, rdid_rx, 12, 50);
  HAL_GPIO_WritePin(GPIOD, FRAM_CS_Pin, GPIO_PIN_SET);

  for (int i = 1; i < 12; i++) {
      if (rdid_rx[i] == 0x04 || rdid_rx[i] == 0xC2) {
          fram_identified = 1;
          break;
      }
  }

  if (!fram_identified) {
      while(1) { // Error: Fast Red Flash (ID Mismatch)
          HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_11); // RED_LED
          HAL_Delay(100);
      }
  }

  // 3. Memory Integrity Check (Write/Read)
  // Verifies that the memory cells are non-volatile and writable.
  uint8_t test_addr[2] = {0x00, 0x01}; // Address 0x0001
  uint8_t test_data = 0xBC;            // Test pattern
  uint8_t read_back = 0;
  uint8_t cmd_wren = 0x06;
  uint8_t cmd_write[4] = {0x02, test_addr[0], test_addr[1], test_data};
  uint8_t cmd_read[3] = {0x03, test_addr[0], test_addr[1]};

  // Write Sequence
  HAL_GPIO_WritePin(GPIOD, FRAM_CS_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi2, &cmd_wren, 1, 10);
  HAL_GPIO_WritePin(GPIOD, FRAM_CS_Pin, GPIO_PIN_SET);

  HAL_GPIO_WritePin(GPIOD, FRAM_CS_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi2, cmd_write, 4, 10);
  HAL_GPIO_WritePin(GPIOD, FRAM_CS_Pin, GPIO_PIN_SET);

  HAL_Delay(5);

  // Read Sequence
  HAL_GPIO_WritePin(GPIOD, FRAM_CS_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi2, cmd_read, 3, 10);
  HAL_SPI_Receive(&hspi2, &read_back, 1, 10);
  HAL_GPIO_WritePin(GPIOD, FRAM_CS_Pin, GPIO_PIN_SET);

  if (read_back != test_data) {
      while(1) { // Error: Flash Blue (Write/Read Failure)
          HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0); // BLUE_LED
          HAL_Delay(200);
      }
  }


  // SUCCESS: SOLID WHITE
  HAL_GPIO_WritePin(GPIOB, RED_LED_Pin|GREEN_LED_Pin|BLUE_LED_Pin, GPIO_PIN_RESET);
  HAL_Delay(2000);
  HAL_GPIO_WritePin(GPIOB, RED_LED_Pin|GREEN_LED_Pin|BLUE_LED_Pin, GPIO_PIN_SET);

  /* --- BLHELI_32 ESC INITIALIZATION (400Hz Protocol) --- */
  /**
    * @brief Sets Motor PWM for BLHeli_32 (1000-2000us Range)
    * @param channel: TIM_CHANNEL_x
    * @param pulse_us: 1000 (Stop) to 2000 (Full)
    */
  void Set_Motor_PWM(uint32_t channel, uint16_t pulse_us)
  {
      // Safety Clamp: BLHeli range is strictly 1000-2000us [cite: 1184]
      if (pulse_us < 1000) pulse_us = 1000;
      if (pulse_us > 2000) pulse_us = 2000;

      __HAL_TIM_SET_COMPARE(&htim1, channel, pulse_us);
  }

  MX_TIM1_Init(); // Ensure this is configured with ARR=2499 for 400Hz

  // 1. Start PWM Generation
  // The ESC detects the input signal type automatically at power up.
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);

  // 2. ARMING SEQUENCE [cite: 1246]
  // The manual specifies:
  // - Power on: 3 Beeps
  // - Signal Detected: 1 Low Beep [cite: 1255]
  // - Zero Throttle Detected: 1 High Beep [cite: 1256]

  // Step A: Send "Zero Throttle" (1000us) to satisfy the "Zero throttle detected" condition.
  // Range is 1000us to 2000us.
  Set_Motor_PWM(TIM_CHANNEL_1, 1000);
  Set_Motor_PWM(TIM_CHANNEL_2, 1000);
  Set_Motor_PWM(TIM_CHANNEL_3, 1000);
  Set_Motor_PWM(TIM_CHANNEL_4, 1000);

  // Step B: Wait for ESC Arming Beeps
  // Manual suggests the sequence takes a few seconds.
  // We wait 3000ms to allow the "Low Beep" (Signal Found) and "High Beep" (Armed).
  HAL_Delay(3000);

  /* --- MAIN FLIGHT LOOP TEST --- */
  // Spin motors at low RPM (Idling)
  // Note: Min throttle must be > 1000us + 70us deadband
  uint16_t idle_throttle = 1200;

  Set_Motor_PWM(TIM_CHANNEL_1, idle_throttle);
  Set_Motor_PWM(TIM_CHANNEL_2, idle_throttle);
  Set_Motor_PWM(TIM_CHANNEL_3, idle_throttle);
  Set_Motor_PWM(TIM_CHANNEL_4, idle_throttle);

  /* USER CODE END 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */


    /* USER CODE BEGIN 3 */
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 12;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_HIGH;
  hspi2.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 179;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 2499;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.Pulse = 1000;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, VDD_3V3_SENSORS_EN_Pin|MAG_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, PD_8266_Pin|RST_8266_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, ICM2_CS_Pin|ICM_CS_Pin|VDD_3V3_PERIPH_EN_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GREEN_LED_Pin|RED_LED_Pin|BLUE_LED_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, FRAM_CS_Pin|BARO_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : VDD_3V3_SENSORS_EN_Pin PD_8266_Pin RST_8266_Pin MAG_CS_Pin */
  GPIO_InitStruct.Pin = VDD_3V3_SENSORS_EN_Pin|PD_8266_Pin|RST_8266_Pin|MAG_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : ICM2_CS_Pin ICM_CS_Pin VDD_3V3_PERIPH_EN_Pin */
  GPIO_InitStruct.Pin = ICM2_CS_Pin|ICM_CS_Pin|VDD_3V3_PERIPH_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : GREEN_LED_Pin RED_LED_Pin BLUE_LED_Pin */
  GPIO_InitStruct.Pin = GREEN_LED_Pin|RED_LED_Pin|BLUE_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : FRAM_CS_Pin BARO_CS_Pin */
  GPIO_InitStruct.Pin = FRAM_CS_Pin|BARO_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
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
     *     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
