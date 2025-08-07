/* USER CODE BEGIN Header */
/**
 ******************************************************************************
* @file		: main.c
* @project	: EECE8011 - Full PCB test code
* @authors	: Monica (Thi Kim Thu) Tong + Kiruthika Arumugam
* @date 	: 2025-08-06 [yyyy-mm-dd]
* @board	: Nucleo STMF441RE
* @brief	:
*    The functions in this file are used to conceptually mimic a semi-portable system
*    	that detects risk of mold growth indoors (no sunlight + high humidity):
*    	- Init each sensor separately
*    	- Read environmental values:
*    		+ Solar panel's voltage - ADC input (interrupt)
*    		+ Humidity (1-2 DHT11 sensors) - pulses
*    	- Periodically check if sensors are working correctly (watchdog timer? Check values?)
*    		+ If not working properly/disconnected, prompt user to manually restart system
*    	- Store sensor data (e.g. in big array OR circular buffer)
*    	- Display average sensor status on OLED
*    		+ But we can see more detailed values on terminal
*    		+ Screen can refresh at same rate as mold risk evaluation
*    	- Evaluate mold risk periodically (e.g. every 1 second)
*    		+ Calculate if low light + high humidity threshold reached
*    		+ Show hard-to-miss warning on OLED if mold risk is detected
*
*	Input:
*		Push button: B0 (onboard) (PC13) (this is used for DEBUGGING only, not part of the final demo)
*			* Button is Active Low (so '0' = pressed)
*
*		ADC: Non-constant input voltage (from solar panel)
*		DHT11: 3.3V, PA1 (hardcoded in DHT library), GND
*
*		VCP_RX (PuTTy input -> onboard/Nucleo)
*
*		TIM4 (just internal clock, no channel): for non-blocking delay functions
*
*	Outputs:
*		SPI2: OLED: (VCC: 3.3V)
*			NC: N/A				DIN: PC3 (07_37)		CLK: PC7 (D9)			(DIN is MOSI)
*			CS: PB2	(10_22)		DC: PB1 (10_24)			RES: PB0 (A3)
*				NOTE: Screen's mounted upside down in this case.
*
*		VCP_TX (Nucleo -> PuTTy)
*
*    Citations:
*		DHT11 library: https://controllerstech.com/using-dht11-sensor-with-stm32/
*    	Some code troubleshot and partially written with assistance from Copilot. I've reviewed and tested them to understand what they do

 ******************************************************************************
 **/
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
// Basic libs:
#include <stdio.h>
#include <string.h> // string manipulation (where necessary)
#include "userInput.h" // to get user's character input from terminal

#include "debounce.h" // push button debouncing (TEMP - to remove)

#include "DHT.h" // humidity sensor(s)

// For OLED:
#include "ssd1331.h"
#include "fonts.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// To overwrite extern vars in syscalls.c:
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#define GETCHAR_PROTOTYPE int __io_getchar (void)

// Mold risk evaluation:
#define SOLAR_HIGH 2000 // >= this val -> high sunlight. Risk = LOW sunlight
#define HUMIDITY_HIGH 75.0f // >= this val -> high humidity. Risk = HIGH humidity
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// DHT sensor:
DHT_DataTypedef DHT11_Data; // Look in the DHT.h for the definition
float Temperature, Humidity;

// ADC (solar) values for interrupt:
volatile uint32_t latestAdcValue = 0;
volatile uint8_t adcUpdated = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


/*
 * FUNCTION : printMenu
 * DESCRIPTION :
 *    Print the interactive menu to the terminal with short explanations
 *    for each option. Called when user selects an option amongst the
 *    ones below, or at startup.
 * PARAMETERS : void
 * RETURNS : void
 */
void printMenu (void) {
	printf("Choose a module to test:\n\r");
	printf("0: Show menu again\n\r");
	printf("1: Test only DHT11\n\r");
	printf("2: Test only OLED (SPI2)\n\r");
	printf("3: Test only Solar panel (ADC1 CH1)\n\r");
	return;
} // end of func


/*
 * FUNCTION : hasElapsed
 * DESCRIPTION :
 *    Non-blocking delay checker. Returns 1 if the specified number of milliseconds
 *    has passed since the given start time.
 * PARAMETERS :
 *    uint32_t startTime : the reference start time (from HAL_GetTick())
 *    uint32_t duration  : how many milliseconds to wait
 * RETURNS :
 *    1 if time has elapsed, 0 otherwise
 */
uint8_t hasElapsed(uint32_t startTime, uint32_t duration) {
	return (HAL_GetTick() - startTime >= duration);
} // end of func


/*
 * FUNCTION : runOledTest
 * DESCRIPTION : Display a fixed string on the OLED
 * PARAMETERS : void
 * RETURNS : void
*/
void runOledTest (void) {
	// Short description of the test
	printf("=== OLED Display Test ===\n\r");
	printf("This test displays a fixed message on the OLED screen.\n\r");

	const char *testString = {"Monica's OLED!"}; // the string
	ssd1331_display_string(0, 0, testString, FONT_1206, WHITE); // don't need to think about string buffer here
} // end of func


/*
 * FUNCTION : runAdcTest
 * DESCRIPTION :
 *    Read analog input from ADC1 and display the value on the terminal.
 *    This test is designed to verify that the potentiometer is wired correctly
 *    and that the ADC is functioning. Values will be printed continuously.
 *    Type 'q' to quit the test and return to the main menu.
 * PARAMETERS : void
 * RETURNS : void
 */
void runAdcTest (void) {
	// Short description of the test:
	printf("=== ADC Input Test ===\n\r");
	printf("This test reads analog input from a potentiometer via ADC1.\n\r");
	printf("Ensure the potentiometer is connected to one of the ADC pins.\n\r");
	printf("Type 'Y' to continue or any other key to cancel...\n\r");

	// Confirmation prompt:
	char confirm = 0;
	while (confirm == 0) {
		confirm = GetCharFromUART2(); // wait for user input via VCP
	}

	if (confirm != 'Y' && confirm != 'y') {
		printf("Test aborted. Returning to main menu...\n\r");
		return;
	}

	uint32_t startTime = HAL_GetTick(); // non-blocking timer for read loop
	// Begin ADC read loop:
	printf("ADC test started. Type 'q' to quit.\n\r");
	while (1) {
		char exitChar = GetCharFromUART2(); // allow exit via VCP
		if (exitChar == 'q' || exitChar == 'Q') {
			printf("Quitting ADC test. Returning to main menu...\n\r");
			break;
		}

		if (hasElapsed(startTime, 200)) { // non-blocking HAL_Delay equivalent
			startTime = HAL_GetTick(); // reset timer

			HAL_ADC_Start(&hadc1);
			if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) { // polling, not interrupt
				uint32_t adcValue = HAL_ADC_GetValue(&hadc1);
				printf("ADC Value: %lu\n\r", adcValue);
			}
			HAL_ADC_Stop(&hadc1);
		} // end of outer if

	} // end of inner while()

	return;
} // end of func


/*
 * FUNCTION : HAL_ADC_ConvCpltCallback (ADC interrupt func)
 * DESCRIPTION :
 *    Read ADC interrupt value and update them to global vars
 * PARAMETERS : ADC_HandleTypeDef *hadc (ADC typedef)
 * RETURNS : void
 */
void HAL_ADC_ConvCpltCallback (ADC_HandleTypeDef *hadc) {
	if (hadc->Instance == ADC1) {
		latestAdcValue = HAL_ADC_GetValue(hadc);
		adcUpdated = 1;
	}
} // end of func


/*
 * FUNCTION : runDhtTest
 * DESCRIPTION :
 *    Read analog input from DHT11 and display the value on the terminal & OLED.
 *    Type 'q' to quit the test and return to the main menu.
 * PARAMETERS : void
 * RETURNS : void
 */
void runDhtTest (void) {
	printf("=== DHT11 Sensor Test ===\n\r");
	printf("This test reads temperature and humidity from the DHT11 sensor.\n\r");
	printf("Type 'q' to quit.\n\r");

	char tempStr[20] = {0}; // format output to readable text (OLED prefers string) & init 1st byte to \0
	char humStr[20] = {0};

	uint32_t startTime = HAL_GetTick(); // non-blocking timer

	while (1) {
		char exitChar = GetCharFromUART2();
		if (exitChar == 'q' || exitChar == 'Q') {
			printf("Quitting DHT11 test. Returning to main menu...\n\r");
			break;
		}
		if ( hasElapsed(startTime, 1100) ) { // non-blocking delay. IMPORTANT: DHT11 can't handle delays lower than 1000 ms...
			startTime = HAL_GetTick(); // reset timer
			// Read & update values to some vars:
			DHT_GetData(&DHT11_Data);
			Temperature = DHT11_Data.Temperature;
			Humidity = DHT11_Data.Humidity;
			snprintf(tempStr, sizeof(tempStr), "Temp: %d C", (int)Temperature); // cast to int instead of (uint16_t) for simplicity
			snprintf(humStr, sizeof(humStr), "Humidity: %d %%", (int)Humidity); // cast to int instead of (uint16_t) for simplicity

			printf("T: %s, H: %s\n\r", tempStr, humStr);

			// Clear top half of screen by drawing a black rectangle:
			ssd1331_fill_rect(0, 0, 96, 32, BLACK); // clear top half of screen
			ssd1331_display_string(0, 0, tempStr, FONT_1206, WHITE);
			ssd1331_display_string(0, 16, humStr, FONT_1206, WHITE);

			// Display values on OLED:
			ssd1331_display_string(0, 0, tempStr, FONT_1206, WHITE);
			ssd1331_display_string(0, 16, humStr, FONT_1206, WHITE);
		} // end of if

	} // end of while()

	return;
} // end of func


/*
 * FUNCTION: readSensors
 * DESCRIPTION: Reads humidity and light level from sensors
 * PARAMETERS: float* humidity, uint32_t* lightLevel
 * RETURNS: int8_t - 0 if success, -1 if sensor error
 */
int8_t readSensors(float* humidity, uint32_t* lightLevel) {
	// to write
	return 0;
}


/*
 * FUNCTION: evaluateMoldRisk
 * DESCRIPTION: Checks if mold risk is present based on humidity and light level
 * PARAMETERS: float humidity, uint32_t lightLevel
 * RETURNS: int8_t - 1 if mold risk detected, 0 if not (normal values)
 */
int8_t evaluateMoldRisk (float humidity, uint32_t lightLevel) {
	int8_t riskFound = ( humidity >= HUMIDITY_HIGH && lightLevel <= SOLAR_HIGH );
		/* NOTE 1: using int instead of uint here to potentially return errors in the future
		 * NOTE 2: still assuming humidity's a float in case we switch sensor model */
	return riskFound;
} // end of func


/*
 * FUNCTION: showMoldWarning
 * DESCRIPTION: Displays mold risk warning on OLED
 * PARAMETERS: void
 * RETURNS: void
 */
void showMoldWarning (void) {
	ssd1331_fill_rect(0, 32, 96, 32, RED); // clear bottom half with red
	ssd1331_display_string(0, 32, "MOLD RISK!", FONT_1206, WHITE); // white text
    return;
} // end of func


/*
 * FUNCTION: evaluateAndDisplayRisk
 * DESCRIPTION: Evaluates mold risk and displays warning if needed
 * PARAMETERS: float humidity, uint32_t lightLevel
 * RETURNS: void
 */
void evaluateAndDisplayRisk(float humidity, uint32_t lightLevel) {
	if (evaluateMoldRisk(humidity, lightLevel)) {
		showMoldWarning();
	}
	return;
} // end of func


/*
 * FUNCTION: runMoldRiskTest
 * DESCRIPTION: Runs mold risk evaluation loop
 * 	NOTE:for now I'm implementing this way, but will be updated with circular buffer implementation
 * PARAMETERS: void
 * RETURNS: void
 */
void runMoldRiskTest(void) {
	printf("=== Mold Risk Evaluation ===\n\r");
	printf("Press 'q' to quit.\n\r");

	// Define vars again:
	char humStr[20] = {0};
	char lightStr[20] = {0};
	float humidity = 0;
	uint32_t lightLevel = 0;

	// Main eval loop
	while (1) {
		// Prompt to escape to main menu:
		char exitChar = GetCharFromUART2();
		if (exitChar == 'q' || exitChar == 'Q') {
			printf("Exiting mold risk test.\n\r");
			break;
		}

		uint32_t startTime = HAL_GetTick(); // non-blocking timer

		// non-blocking delay:
		if ( hasElapsed(startTime, 1000) ) { // check every 1 second
			startTime = HAL_GetTick(); // reset timer

			// Check if sensor outputs make sense:
			if (readSensors(&humidity, &lightLevel) == -1) {
				printf("ERROR: DHT sensor not responding.\n\r");
				ssd1331_display_string(0, 0, "DHT ERROR!", FONT_1206, RED);
				continue;
			}

			// Show results on OLED:
			snprintf(humStr, sizeof(humStr), "Humidity: %d %%", (int)humidity);
			snprintf(lightStr, sizeof(lightStr), "Light: %lu", lightLevel);

			ssd1331_fill_rect(0, 0, 96, 32, BLACK); // clear top half
			ssd1331_display_string(0, 0, humStr, FONT_1206, WHITE);
			ssd1331_display_string(0, 16, lightStr, FONT_1206, WHITE);

			evaluateAndDisplayRisk(humidity, lightLevel);
		} // end of hasElapsed() if loop

	} // end of inner while(1)
	return;
} // end of func



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
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_SPI2_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */
  printf("\n\rGroup 3's Demo:\n\r===\n\r");

  ssd1331_init(); // Init OLED
  HAL_ADC_Start_IT(&hadc1); // Start ADC interrupt

  // Declare vars:
  uint8_t showMenu = 1; // flag that when set will output the menu prompt

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
//	  runDhtTest();
	  // If menu hasn't been printed:
	  if(showMenu != 0)	{	// meaning menu flag isn't 0
		  printMenu();
		  showMenu = 0;		// reset flag
	  	  }

	  // Show user input:
	  char userInput = GetCharFromUART2(); // using VCP here to avoid module failure
										  /* this declaration needs to be here to restrict
										   * its scope only to this loop */
	  if (userInput != 0) {
		  printf("You entered: %c\n\r", userInput);
	  }

	  // Handle the menu input:
	  switch (userInput) {
	  	  case 0: // (note this is 0 not '0') Nothing received from GetCharFromUART2()
	  		  break;

	  	  case '0': // show menu again
	  		  showMenu = 1;
	  		  break;

	  	  case '1': // test DHT11 measurements
	  		  runDhtTest();
	  		  break;

	  	  case '2': // test OLED
	  		  runOledTest();
	  		  break;

	  	  case '3': // test ADC
	  		  runAdcTest();
	  		  break;

	  	  default:
	  		  printf("ERROR: invalid menu option!\n\rShowing menu again...\n\r");
	  		  showMenu = 1; // show menu again
	  		  break;
	  } // end of switch
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  } // end of while(1)
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 100;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
PUTCHAR_PROTOTYPE
{
  /* Place your implementation of fputc here */
  /* e.g. write a character to the USART1 and Loop until the end of transmission */
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);
  return ch;
}

GETCHAR_PROTOTYPE
{
  uint8_t ch = 0;
  // Clear the Overrun flag just before receiving the first character
  __HAL_UART_CLEAR_OREFLAG(&huart2);

  HAL_UART_Receive(&huart2, (uint8_t *)&ch, 1, 5);
  return ch;
}

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
