#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>

QueueHandle_t xCountQueue = NULL;
uint32_t buttonClickCount = 0;

TaskHandle_t xButtonTaskHandle = NULL;
TaskHandle_t xUartTaskHandle = NULL;

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    for (;;);
}

void UART2_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    // PA2: TX
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // PA3: RX
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART2, &USART_InitStructure);

    USART_Cmd(USART2, ENABLE);
}

void UART2_SendChar(char c) {
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    USART_SendData(USART2, (uint8_t)c);
}

void Button_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

void vButtonTask(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(50);
    xLastWakeTime = xTaskGetTickCount();

    uint8_t btn_last = 1;

    for (;;) {
        uint8_t btn_curr = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0);

        if (btn_last == 1 && btn_curr == 0) {
            buttonClickCount++;
            xQueueSend(xCountQueue, &buttonClickCount, 0);
        }

        btn_last = btn_curr;
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void vUartTransmitTask(void *pvParameters) {
    uint32_t receivedCount = 0;
    char buffer[50];

    for (;;) {
        if (xQueueReceive(xCountQueue, &receivedCount, portMAX_DELAY) == pdPASS) {
            int len = sprintf(buffer, "So lan nhan nut: %lu\r\n", receivedCount);
            for (int i = 0; i < len; i++) {
                UART2_SendChar(buffer[i]);
            }
        }
    }
}

int main(void) {
    UART2_Init();
    Button_Init();

    xCountQueue = xQueueCreate(10, sizeof(uint32_t));

    if (xCountQueue != NULL) {
        // Tăng stack size lên 256 cho cả 2 task
        xTaskCreate(vButtonTask, "ButtonTask", 256, NULL, 2, &xButtonTaskHandle);
        xTaskCreate(vUartTransmitTask, "UartTask", 256, NULL, 1, &xUartTaskHandle);

        vTaskStartScheduler();
    }

    for (;;);
}
