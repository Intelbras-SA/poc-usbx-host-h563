/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    ux_host_cdc_acm.c
 * @author  MCD Application Team
 * @brief   USBX Host applicative file
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
#include "ux_host_cdc_acm.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "main.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define APP_RX_DATA_SIZE             2048U
#define BLOCK_SIZE                   64U

#define NEW_RECEIVED_DATA    0x01
#define NEW_DATA_TO_SEND     0x02
#define REMOVED_CDC_INSTANCE 0x03
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
extern UX_HOST_CLASS_CDC_ACM    *cdc_acm;
extern TX_EVENT_FLAGS_GROUP     ux_app_EventFlag;

UX_HOST_CLASS_CDC_ACM_RECEPTION cdc_acm_reception;
ULONG                           block_reception_count;
uint8_t                         block_reception_size[APP_RX_DATA_SIZE / BLOCK_SIZE];
uint16_t                        RxSzeIdx;
static UCHAR                    UserRxBuffer[APP_RX_DATA_SIZE];

static UCHAR					UserTxBuffer1 = '1';
static UCHAR					UserTxBuffer2 = '0';
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
VOID cdc_acm_reception_callback(struct UX_HOST_CLASS_CDC_ACM_STRUCT *cdc_acm, UINT status,
                                UCHAR *reception_buffer, ULONG reception_size);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* USER CODE BEGIN 1 */
VOID cdc_acm_send_app_thread_entry(ULONG thread_input)
{
	/* Local Variables */
	ULONG tx_actual_length;

	/* Infinite Looping */
	while(1)
	{
		/* Check the CDC class state */
		if ((cdc_acm != NULL) && (cdc_acm -> ux_host_class_cdc_acm_state ==  UX_HOST_CLASS_INSTANCE_LIVE))
		{
			_ux_host_class_cdc_acm_write(cdc_acm, &UserTxBuffer1, 1, &tx_actual_length);
			printf("tx> 1\r\n");
			tx_thread_sleep(MS_TO_TICK(500));
			_ux_host_class_cdc_acm_write(cdc_acm, &UserTxBuffer2, 1, &tx_actual_length);
			printf("tx> 0\r\n");
			tx_thread_sleep(MS_TO_TICK(500));
		}
		else
		{
			tx_thread_sleep(MS_TO_TICK(10));
		}
	}
}

VOID cdc_acm_recieve_app_thread_entry(ULONG thread_input)
{
	/* Local Variables */
	UINT status;
	ULONG    receive_dataflag = 0;
	UCHAR    *read_data_pointer = NULL;
	ULONG    read_block_count = 0;
	uint16_t read_data_block_count = 0;
	uint16_t count = 0;

	/* Infinite Looping */
	while(1)
	{
		/* Check the CDC class state */
		if ((cdc_acm != NULL) && (cdc_acm -> ux_host_class_cdc_acm_state ==  UX_HOST_CLASS_INSTANCE_LIVE))
		{
			if (cdc_acm_reception.ux_host_class_cdc_acm_reception_state != UX_HOST_CLASS_CDC_ACM_RECEPTION_STATE_STARTED)
			{
				/* Get a pointer to the USB user buffer reception */
				read_data_pointer = UserRxBuffer;

				/* Set the block size parameter reception */
				cdc_acm_reception.ux_host_class_cdc_acm_reception_block_size = BLOCK_SIZE;

				/* Set the buffer for reception */
				cdc_acm_reception.ux_host_class_cdc_acm_reception_data_buffer = (UCHAR *)UserRxBuffer;

				/* Set the size of the data reception buffer */
				cdc_acm_reception.ux_host_class_cdc_acm_reception_data_buffer_size = APP_RX_DATA_SIZE;

				/* Set the callback for each reception transfer completion */
				cdc_acm_reception.ux_host_class_cdc_acm_reception_callback = cdc_acm_reception_callback;

				/* Start reception */
				status = ux_host_class_cdc_acm_reception_start(cdc_acm, &cdc_acm_reception);

				/* Check status to starting data reception */
				if (status != UX_SUCCESS)
				{
					tx_thread_sleep(MS_TO_TICK(10));
				}
			}
			else if (cdc_acm_reception.ux_host_class_cdc_acm_reception_state == UX_HOST_CLASS_CDC_ACM_RECEPTION_STATE_STARTED)
			{
				/* Wait until the requested flag NEW_RECEIVED_DATA is received */
				if (tx_event_flags_get(&ux_app_EventFlag, NEW_RECEIVED_DATA, TX_OR_CLEAR,
						&receive_dataflag, TX_WAIT_FOREVER) != TX_SUCCESS)
				{
					Error_Handler();
				}

				while ((read_block_count < block_reception_count) && (cdc_acm != NULL))
				{
					/* Check if read_data_pointer reached end of user buffer */
					if (read_data_pointer >= cdc_acm_reception.ux_host_class_cdc_acm_reception_data_buffer +
							cdc_acm_reception.ux_host_class_cdc_acm_reception_data_buffer_size)
					{
						read_data_pointer = cdc_acm_reception.ux_host_class_cdc_acm_reception_data_buffer;

						/* Reinitialize block reception size index */
						read_data_block_count = 0;
					}

					printf("rx> ");

					/* Display the received data */
					for (count = 0; count < block_reception_size[read_data_block_count]; count++)
					{
						printf("%c", *read_data_pointer);
						read_data_pointer++;
					}

					printf("\r\n");

					/* Move to next block reception */
					read_block_count++;

					/* Move to the next block reception buffer */
					read_data_pointer += (BLOCK_SIZE - count);

					/* Move to the next block reception size */
					read_data_block_count++;
				}
			}
		}
		else
		{
			tx_thread_sleep(MS_TO_TICK(10));
		}
	}
}

VOID cdc_acm_reception_callback(struct UX_HOST_CLASS_CDC_ACM_STRUCT *cdc_acm,
		UINT status, UCHAR *reception_buffer, ULONG reception_size)
{
	/* Block reception count */
	block_reception_count++;

	/* Save block reception size */
	block_reception_size[RxSzeIdx] = reception_size;

	/* Move to the next block reception size */
	RxSzeIdx++;

	/* check if tail has reached end of user buffer */
	if (cdc_acm_reception.ux_host_class_cdc_acm_reception_data_tail + cdc_acm_reception.ux_host_class_cdc_acm_reception_block_size >=
			cdc_acm_reception.ux_host_class_cdc_acm_reception_data_buffer + cdc_acm_reception.ux_host_class_cdc_acm_reception_data_buffer_size)
	{
		/* Move back to the beginning */
		cdc_acm_reception.ux_host_class_cdc_acm_reception_data_tail = cdc_acm_reception.ux_host_class_cdc_acm_reception_data_buffer;

		/* Reinitialize reception block size index */
		RxSzeIdx = 0U;
	}
	else
	{
		/* Program the tail to be after the current buffer */
		cdc_acm_reception.ux_host_class_cdc_acm_reception_data_tail += cdc_acm_reception.ux_host_class_cdc_acm_reception_block_size;
	}

	/* Set NEW_RECEIVED_DATA flag */
	if (tx_event_flags_set(&ux_app_EventFlag, NEW_RECEIVED_DATA, TX_OR) != TX_SUCCESS)
	{
		Error_Handler();
	}
}
/* USER CODE END 1 */
