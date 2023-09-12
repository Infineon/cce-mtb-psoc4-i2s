/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for the PSoC4 I2S Application
*              for ModusToolbox.
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2020-2023, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

#include "cy_pdl.h"
#include "cybsp.h"
#include "cyhal.h"
#include "cy_retarget_io.h"


/*******************************************************************************
* Macros
*******************************************************************************/

/* Buffer size */
#define BUFFER_SIZE 64u

/*******************************************************************************
* Global Variables
********************************************************************************/
/*HAL Objects*/

cyhal_i2s_t   i2s;
cy_rslt_t     rslt;
cyhal_clock_t clock_pll;
cyhal_clock_t clock_hf;
cyhal_clock_t clock_sys;

/* I2S pin configuration */
const cyhal_i2s_pins_t i2s_tx_pins = {
    .sck  = P8_1,
    .ws   = P8_2,
    .data = P8_3,
    .mclk = NC
};


/* I2S Configuration */
cyhal_i2s_config_t config  =
    {
        .is_tx_slave    = false,
        .is_rx_slave    = false,
        .mclk_hz        = 0,
        .channel_length = 32,
        .word_length    = 16,
        .sample_rate_hz = 18750
    };

/* Initialize the Transmit buffer */
uint16_t  tx_buffer0[BUFFER_SIZE];
uint16_t  tx_buffer1[BUFFER_SIZE];
const uint16_t* active_tx_buffer;
const uint16_t* next_tx_buffer;

/*******************************************************************************
* Function Prototypes
********************************************************************************/
void i2s_event_handler_transmit_streaming(void *arg, cyhal_i2s_event_t event);


/*******************************************************************************
* Function Name: i2s_event_handler_transmit_streamin
********************************************************************************
* Summary:
*  Initializes the clocks in the system.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void i2s_event_handler_transmit_streaming(void* arg, cyhal_i2s_event_t event)
{
    /* When we registered the callback, we set 'arg' to point to the i2s object */
    cyhal_i2s_t* i2s = (cyhal_i2s_t*)arg;
    if (0u != (event & CYHAL_I2S_ASYNC_TX_COMPLETE))
    {
        /* Flip the active and the next tx buffers */
        const uint16_t* temp = active_tx_buffer;
        active_tx_buffer = next_tx_buffer;
        next_tx_buffer   = temp;


        /* Start writing the next buffer while the just-freed one is repopulated */
        cy_rslt_t result = cyhal_i2s_write_async(i2s, active_tx_buffer, BUFFER_SIZE);
        if (CY_RSLT_SUCCESS == result)
        {
            printf("Transfer Completed\r\n");
        }
    }
}

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* The main function performs the following actions:
*    1. Initializes the BSP.
*    2. Initializes the I2S Block
*    3. Registers I2S interrupt handler and starts the I2S transmission
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;

    for(int16_t i=0; i<=63; i++)
    {
    tx_buffer0[i] = i;
    tx_buffer1[i] = i;
    }

    /* Initialize the device and board peripherals */
    result = cybsp_init() ;
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }


    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    result = cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX,
                                     CY_RETARGET_IO_BAUDRATE);

    /* retarget-io init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    printf("\x1b[2J\x1b[;H");
    printf("****************** "
           "HAL: PSoC 4 I2S Example "
           "****************** \r\n\n");


    /* Initialize the i2s block */
    result = cyhal_i2s_init(&i2s, &i2s_tx_pins, NULL, &config, NULL);
    if (CY_RSLT_SUCCESS != result)
    {
        /* Handle error */
        while(1);
    }

    /* Register callback function */
    cyhal_i2s_register_callback(&i2s, &i2s_event_handler_transmit_streaming, &i2s);

    /* Enable the Asynchronous transfer event */
    cyhal_i2s_enable_event(&i2s, CYHAL_I2S_ASYNC_TX_COMPLETE, CYHAL_ISR_PRIORITY_DEFAULT, true);

    /* Configure asynchronous transfers to use DMA to free up the CPU during transfers */
    result = cyhal_i2s_set_async_mode(&i2s, CYHAL_ASYNC_DMA , CYHAL_DMA_PRIORITY_DEFAULT);

    if (CY_RSLT_SUCCESS == result)
    {
            /* Populate initial data in the two tx buffers (e.g. by streaming over the network) */
            active_tx_buffer = tx_buffer0;
            next_tx_buffer   = tx_buffer1;
            result = cyhal_i2s_write_async(&i2s, active_tx_buffer, BUFFER_SIZE);
    }

    if (CY_RSLT_SUCCESS == result)
    {
        result = cyhal_i2s_start_tx(&i2s);
    }

    for (;;)
    {
    }
}

/* [] END OF FILE */
