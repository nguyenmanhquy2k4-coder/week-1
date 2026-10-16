#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// Biến trạng thái toàn cục
uint8_t currentSelectedLed = 0;         // LED đang được chọn (0: LED1, 1: LED2, 2: LED3)
uint16_t ledDutyCycle[3] = {0, 0, 0};   // Độ sáng 3 LED (0 -> 1000)

// Khai báo Task Handle
TaskHandle_t xButtonTaskHandle = NULL;
TaskHandle_t xLed1TaskHandle = NULL;
TaskHandle_t xLed2TaskHandle = NULL;
TaskHandle_t xLed3TaskHandle = NULL;

// Hook bắt lỗi Stack Overflow của FreeRTOS
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    for (;;);
}

// Khởi tạo phần cứng
void System_Init(void) {
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    // 1. Bật clock cho GPIOA và TIM2
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    // 2. Cấu hình chân PWM cho 3 LED (PA0 - TIM2_CH1, PA1 - TIM2_CH2, PA2 - TIM2_CH3)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // Cài đặt Timer 2: Tần số PWM = 1kHz (72MHz / 72 / 1000)
    TIM_TimeBaseStructure.TIM_Period = 1000 - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = 72 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);

    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;

    TIM_OC1Init(TIM2, &TIM_OCInitStructure);
    TIM_OC1PreloadConfig(TIM2, TIM_OCPreload_Enable);

    TIM_OC2Init(TIM2, &TIM_OCInitStructure);
    TIM_OC2PreloadConfig(TIM2, TIM_OCPreload_Enable);

    TIM_OC3Init(TIM2, &TIM_OCInitStructure);
    TIM_OC3PreloadConfig(TIM2, TIM_OCPreload_Enable);

    TIM_Cmd(TIM2, ENABLE);

    // 3. Cấu hình 3 nút nhấn trên Port A: PA3 (Chuyển LED), PA4 (Tăng sáng), PA5 (Giảm sáng) - Pull-up nội
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; 
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

// --- TASK ĐỌC NÚT NHẤN (PA3, PA4, PA5) ---
void vButtonTask(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(50); // Quét phím mỗi 50ms chống dội
    xLastWakeTime = xTaskGetTickCount();

    uint8_t btn3_last = 1, btn4_last = 1, btn5_last = 1;

    for (;;) {
        uint8_t btn3_curr = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_3);
        uint8_t btn4_curr = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_4);
        uint8_t btn5_curr = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_5);

        // Nút 1 (PA3): Chuyển đổi qua lại giữa 3 LED (0 -> 1 -> 2 -> 0)
        if (btn3_last == 1 && btn3_curr == 0) {
            currentSelectedLed = (currentSelectedLed + 1) % 3;
        }

        // Nút 2 (PA4): Tăng độ sáng LED đang chọn (tăng 100, tối đa 1000)
        if (btn4_last == 1 && btn4_curr == 0) {
            if (ledDutyCycle[currentSelectedLed] <= 900) {
                ledDutyCycle[currentSelectedLed] += 100;
            } else {
                ledDutyCycle[currentSelectedLed] = 1000;
            }
        }

        // Nút 3 (PA5): Giảm độ sáng LED đang chọn (giảm 100, tối thiểu 0)
        if (btn5_last == 1 && btn5_curr == 0) {
            if (ledDutyCycle[currentSelectedLed] >= 100) {
                ledDutyCycle[currentSelectedLed] -= 100;
            } else {
                ledDutyCycle[currentSelectedLed] = 0;
            }
        }

        btn3_last = btn3_curr;
        btn4_last = btn4_curr;
        btn5_last = btn5_curr;

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

// --- TASK ĐIỀU KHIỂN LED 1 ---
void vLed1Task(void *pvParameters) {
    for (;;) {
        TIM_SetCompare1(TIM2, ledDutyCycle[0]);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// --- TASK ĐIỀU KHIỂN LED 2 ---
void vLed2Task(void *pvParameters) {
    for (;;) {
        TIM_SetCompare2(TIM2, ledDutyCycle[1]);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// --- TASK ĐIỀU KHIỂN LED 3 ---
void vLed3Task(void *pvParameters) {
    for (;;) {
        TIM_SetCompare3(TIM2, ledDutyCycle[2]);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

int main(void) {
    System_Init();

    // Tạo các Task FreeRTOS
    xTaskCreate(vButtonTask, "ButtonTask", 128, NULL, 2, &xButtonTaskHandle);
    xTaskCreate(vLed1Task,   "Led1Task",   128, NULL, 1, &xLed1TaskHandle);
    xTaskCreate(vLed2Task,   "Led2Task",   128, NULL, 1, &xLed2TaskHandle);
    xTaskCreate(vLed3Task,   "Led3Task",   128, NULL, 1, &xLed3TaskHandle);

    vTaskStartScheduler();

    for (;;);
}
