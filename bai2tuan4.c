#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "FreeRTOS.h"
#include "task.h"
/* Hàm xử lý khi xảy ra tràn Stack trong FreeRTOS */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    /* Tràn stack xảy ra ở task pcTaskName */
    (void)xTask;
    (void)pcTaskName;
    
    /* Vòng lặp vô tận hoặc nháy LED báo lỗi */
    taskDISABLE_INTERRUPTS();
    for (;;);
}

// Cấu trúc dữ liệu truyền vào Task
typedef struct {
    GPIO_TypeDef* GPIOx;
    uint16_t GPIO_Pin;
    uint32_t toggle_ms; // 1/2 chu kỳ (ms) = 1000 / (2 * Frequency)
} LED_Config_t;

// Cấu hình ngoại vi GPIO bằng SPL
void GPIO_Configuration(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

// 01 Hàm Task duy nhất dùng chung cho cả 3 LED
void vLEDTask(void *pvParameters) {
    LED_Config_t *led = (LED_Config_t *)pvParameters;

    while (1) {
        // Đảo trạng thái chân GPIO thông qua thanh ghi ODR của SPL
        led->GPIOx->ODR ^= led->GPIO_Pin;
        
        // Trễ nhường CPU cho Task khác
        vTaskDelay(pdMS_TO_TICKS(led->toggle_ms));
    }
}

int main(void) {
    SystemInit();
    GPIO_Configuration();

    // Cấu hình tham số cho 3 LED
    // 0.1Hz -> T = 10s -> Nửa chu kỳ = 5000ms
    // 1Hz   -> T = 1s  -> Nửa chu kỳ = 500ms
    // 10Hz  -> T = 0.1s-> Nửa chu kỳ = 50ms
    static LED_Config_t led_01Hz = {GPIOA, GPIO_Pin_0, 5000};
    static LED_Config_t led_1Hz  = {GPIOA, GPIO_Pin_1, 500};
    static LED_Config_t led_10Hz = {GPIOA, GPIO_Pin_2, 50};

    // Khởi tạo 3 Task từ cùng 01 hàm vLEDTask
    xTaskCreate(vLEDTask, "LED_01Hz", configMINIMAL_STACK_SIZE, &led_01Hz, 1, NULL);
    xTaskCreate(vLEDTask, "LED_1Hz",  configMINIMAL_STACK_SIZE, &led_1Hz,  1, NULL);
    xTaskCreate(vLEDTask, "LED_10Hz", configMINIMAL_STACK_SIZE, &led_10Hz, 1, NULL);

    // Bắt đầu Bộ điều phối RTOS
    vTaskStartScheduler();

    while (1) {
        // Không bao giờ chạy vào đây nếu RTOS khởi chạy thành công
    }
}
