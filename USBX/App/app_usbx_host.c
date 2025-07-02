/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    app_usbx_host.c
 * @author  MCD Application Team
 * @brief   USBX host applicative file
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
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
#include "app_usbx_host.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "ux_hcd_stm32.h"
#include "ux_host_class_cdc_acm.h"
#include "ux_host_cdc_acm.h"
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
static TX_THREAD ux_host_app_thread;
/* USER CODE BEGIN PV */
extern UART_HandleTypeDef huart3;
extern HCD_HandleTypeDef hhcd_USB_DRD_FS;

UX_HOST_CLASS_CDC_ACM        *cdc_acm;

TX_THREAD                    cdc_acm_send_thread;
TX_THREAD                    cdc_acm_recieve_thread;

TX_EVENT_FLAGS_GROUP         ux_app_EventFlag;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
static VOID app_ux_host_thread_entry(ULONG thread_input);
static UINT ux_host_event_callback(ULONG event, UX_HOST_CLASS *current_class, VOID *current_instance);
static VOID ux_host_error_callback(UINT system_level, UINT system_context, UINT error_code);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/**
 * @brief  Application USBX Host Initialization.
 * @param  memory_ptr: memory pointer
 * @retval status
 */
UINT MX_USBX_Host_Init(VOID *memory_ptr)
{
	UINT ret = UX_SUCCESS;
	UCHAR *pointer;
	TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;

	/* USER CODE BEGIN MX_USBX_Host_Init0 */
	printf("Initializing the Host Stack...\r\n");
	printf("Allocating bytes for the pool for the USBX Stack...\r\n");
	printf("Initializing the system...\r\n");
	printf("Initializing the Host Stack...\r\n");
	printf("Registering the CDC Host Class...\r\n");
	printf("Allocating bytes for the main application thread...\r\n");
	printf("Creating the main application thread...\r\n");
	/* USER CODE END MX_USBX_Host_Init0 */

	/* Allocate the stack for USBX Memory */
	if (tx_byte_allocate(byte_pool, (VOID **) &pointer,
			USBX_HOST_MEMORY_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
	{
		/* USER CODE BEGIN USBX_ALLOCATE_STACK_ERROR */
		printf("Error while allocating bytes!\r\n");
		return TX_POOL_ERROR;
		/* USER CODE END USBX_ALLOCATE_STACK_ERROR */
	}

	/* Initialize USBX Memory */
	if (ux_system_initialize(pointer, USBX_HOST_MEMORY_STACK_SIZE, UX_NULL, 0) != UX_SUCCESS)
	{
		/* USER CODE BEGIN USBX_SYSTEM_INITIALIZE_ERROR */
		printf("Error while initializing the system!\r\n");
		return UX_ERROR;
		/* USER CODE END USBX_SYSTEM_INITIALIZE_ERROR */
	}

	/* Install the host portion of USBX */
	if (ux_host_stack_initialize(ux_host_event_callback) != UX_SUCCESS)
	{
		/* USER CODE BEGIN USBX_HOST_INITIALIZE_ERROR */
		printf("Error while initializing the Host Stack!\r\n");
		return UX_ERROR;
		/* USER CODE END USBX_HOST_INITIALIZE_ERROR */
	}

	/* Register a callback error function */
	ux_utility_error_callback_register(&ux_host_error_callback);

	/* Initialize the host cdc acm class */
	if ((ux_host_stack_class_register(_ux_system_host_class_cdc_acm_name,
			ux_host_class_cdc_acm_entry)) != UX_SUCCESS)
	{
		/* USER CODE BEGIN USBX_HOST_CDC_ACM_REGISTER_ERROR */
		printf("Error while initializing the CDC Host Class!\r\n");
		return UX_ERROR;
		/* USER CODE END USBX_HOST_CDC_ACM_REGISTER_ERROR */
	}

	/* Allocate the stack for host application main thread */
	if (tx_byte_allocate(byte_pool, (VOID **) &pointer, UX_HOST_APP_THREAD_STACK_SIZE,
			TX_NO_WAIT) != TX_SUCCESS)
	{
		/* USER CODE BEGIN MAIN_THREAD_ALLOCATE_STACK_ERROR */
		printf("Error while allocating the bytes for the main application thread!\r\n");
		return TX_POOL_ERROR;
		/* USER CODE END MAIN_THREAD_ALLOCATE_STACK_ERROR */
	}

	/* Create the host application main thread */
	if (tx_thread_create(&ux_host_app_thread, UX_HOST_APP_THREAD_NAME, app_ux_host_thread_entry,
			0, pointer, UX_HOST_APP_THREAD_STACK_SIZE, UX_HOST_APP_THREAD_PRIO,
			UX_HOST_APP_THREAD_PREEMPTION_THRESHOLD, UX_HOST_APP_THREAD_TIME_SLICE,
			UX_HOST_APP_THREAD_START_OPTION) != TX_SUCCESS)
	{
		/* USER CODE BEGIN MAIN_THREAD_CREATE_ERROR */
		printf("Error while creating the main application thread!\r\n");
		return TX_THREAD_ERROR;
		/* USER CODE END MAIN_THREAD_CREATE_ERROR */
	}

	/* USER CODE BEGIN MX_USBX_Host_Init1 */
	/* Allocate the stack for cdc_acm receive thread */
	if (tx_byte_allocate(byte_pool, (VOID **) &pointer,
			UX_HOST_APP_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
	{
		return TX_POOL_ERROR;
	}

	/* Create the cdc_acm_recieve thread */
	if (tx_thread_create(&cdc_acm_recieve_thread, "CDC Receive App thread",
			cdc_acm_recieve_app_thread_entry, 0, pointer,
			UX_HOST_APP_THREAD_STACK_SIZE, 30, 30, 0,
			TX_AUTO_START) != TX_SUCCESS)
	{
		return TX_THREAD_ERROR;
	}

	/* Allocate the stack for cdc_acm send thread */
	if (tx_byte_allocate(byte_pool, (VOID **) &pointer,
			UX_HOST_APP_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
	{
		return TX_POOL_ERROR;
	}

	/* Create the cdc_acm_send thread */
	if (tx_thread_create(&cdc_acm_send_thread, "CDC Read App thread",
			cdc_acm_send_app_thread_entry, 0, pointer,
			UX_HOST_APP_THREAD_STACK_SIZE, 30, 30, 0,
			TX_AUTO_START) != TX_SUCCESS)
	{
		return TX_THREAD_ERROR;
	}

	/* Create the event flags group */
	if (tx_event_flags_create(&ux_app_EventFlag, "Event Flag") != TX_SUCCESS)
	{
		return TX_GROUP_ERROR;
	}

	printf("Initialization done Successfully!\n\r\n");
	/* USER CODE END MX_USBX_Host_Init1 */

	return ret;
}

/**
 * @brief  Function implementing app_ux_host_thread_entry.
 * @param  thread_input: User thread input parameter.
 * @retval none
 */
static VOID app_ux_host_thread_entry(ULONG thread_input)
{
	/* USER CODE BEGIN app_ux_host_thread_entry */
	printf("Initializing the USB Peripheral in host mode...\r\n");

	/* Initialize the LL driver */
	MX_USB_HCD_Init();

	printf("USB Peripheral initialized successfully!\r\n");

	printf("Initializing the host controller driver...");
	/* Initialize the host controller driver */
	if (ux_host_stack_hcd_register(_ux_system_host_hcd_stm32_name,
			_ux_hcd_stm32_initialize, (ULONG)USB_DRD_FS,
			(ULONG)&hhcd_USB_DRD_FS) != UX_SUCCESS)
	{
		printf("Error registering host controller driver!\r\n");
		return;
	}

	printf("Host controller driver initialized!\r\n");

	/* Start USB Host */
	if (HAL_HCD_Start(&hhcd_USB_DRD_FS) != HAL_OK)
	{
		printf("Error starting USB Host!\r\n");
		return;
	}

	printf("******** USB DRD CDC HOST ********\r\n");
	printf("USB Host library started!\r\n");

	printf("Starting CDC Application!\r\n");
	printf("Connect your CDC Device...\n\r\n");
	/* USER CODE END app_ux_host_thread_entry */
}

/**
 * @brief  ux_host_event_callback
 *         This callback is invoked to notify application of instance changes.
 * @param  event: event code.
 * @param  current_class: Pointer to class.
 * @param  current_instance: Pointer to class instance.
 * @retval status
 */
UINT ux_host_event_callback(ULONG event, UX_HOST_CLASS *current_class, VOID *current_instance)
{
	UINT status = UX_SUCCESS;

	/* USER CODE BEGIN ux_host_event_callback0 */
	UX_PARAMETER_NOT_USED(current_class);
	UX_PARAMETER_NOT_USED(current_instance);
	/* USER CODE END ux_host_event_callback0 */

	switch (event)
	{
	case UX_DEVICE_INSERTION:

		/* USER CODE BEGIN UX_DEVICE_INSERTION */
		/* Get current CDC Class */
		if (current_class -> ux_host_class_entry_function == ux_host_class_cdc_acm_entry)
		{
			if (cdc_acm == UX_NULL)
			{
				/* Get current CDC Instance */
				cdc_acm = (UX_HOST_CLASS_CDC_ACM *)current_instance;

				/* Check if this is CDC DATA instance */
				if (cdc_acm -> ux_host_class_cdc_acm_bulk_in_endpoint == UX_NULL)
				{
					cdc_acm = UX_NULL;
				}
				else
				{
					printf("\nUSB CDC Device Found\r\n");
					printf("PID: %#x \r\n", (UINT)cdc_acm -> ux_host_class_cdc_acm_device -> ux_device_descriptor.idProduct);
					printf("VID: %#x \r\n", (UINT)cdc_acm -> ux_host_class_cdc_acm_device -> ux_device_descriptor.idVendor);
					printf("Data Interface initialized\r\n");
				}
			}
		}

		/* USER CODE END UX_DEVICE_INSERTION */

		break;

	case UX_DEVICE_REMOVAL:

		/* USER CODE BEGIN UX_DEVICE_REMOVAL */
		if ((VOID*)cdc_acm == current_instance)
		{
			/* Clear cdc instance */
			cdc_acm = UX_NULL;
			printf("\nUSB CDC ACM Device Removal!\r\n");
		}

		/* USER CODE END UX_DEVICE_REMOVAL */

		break;

	case UX_DEVICE_CONNECTION:

		/* USER CODE BEGIN UX_DEVICE_CONNECTION */
		printf("USB Device Connected!\r\n");
		/* USER CODE END UX_DEVICE_CONNECTION */

		break;

	case UX_DEVICE_DISCONNECTION:

		/* USER CODE BEGIN UX_DEVICE_DISCONNECTION */
		printf("USB Device Disconnected!\r\n");
		/* USER CODE END UX_DEVICE_DISCONNECTION */

		break;

	default:

		/* USER CODE BEGIN EVENT_DEFAULT */

		/* USER CODE END EVENT_DEFAULT */

		break;
	}

	/* USER CODE BEGIN ux_host_event_callback1 */

	/* USER CODE END ux_host_event_callback1 */

	return status;
}

/**
 * @brief ux_host_error_callback
 *         This callback is invoked to notify application of error changes.
 * @param  system_level: system level parameter.
 * @param  system_context: system context code.
 * @param  error_code: error event code.
 * @retval Status
 */
VOID ux_host_error_callback(UINT system_level, UINT system_context, UINT error_code)
{
	/* USER CODE BEGIN ux_host_error_callback0 */
	printf("USB Error - Level: %d, Context: %d, Code: 0x%x\r\n", system_level, system_context, error_code);
	/* USER CODE END ux_host_error_callback0 */

	switch (error_code)
	{
	case UX_DEVICE_ENUMERATION_FAILURE:

		/* USER CODE BEGIN UX_DEVICE_ENUMERATION_FAILURE */
		printf("Device Enumeration Failure!\r\n");
		/* USER CODE END UX_DEVICE_ENUMERATION_FAILURE */

		break;

	case  UX_NO_DEVICE_CONNECTED:

		/* USER CODE BEGIN UX_NO_DEVICE_CONNECTED */
		printf("USB Device disconnected!\r\n");
		/* USER CODE END UX_NO_DEVICE_CONNECTED */

		break;

	default:

		/* USER CODE BEGIN ERROR_DEFAULT */

		/* USER CODE END ERROR_DEFAULT */

		break;
	}

	/* USER CODE BEGIN ux_host_error_callback1 */

	/* USER CODE END ux_host_error_callback1 */
}

/* USER CODE BEGIN 1 */
int __io_putchar(int ch)
{
	HAL_UART_Transmit(&huart3, (uint8_t *)&ch, 1, 0xFF);
	return ch;
}
/* USER CODE END 1 */
