#include "stm32f4xx.h"
#include <string.h>
#include <stdio.h>

#define BUFFER_SIZE 256


void USART2_config(void);
void USART3_config(void);
void USART2_SendText(char *str);
void USART3_SendText(char *str);
void USART2_IRQHandler(void);
void delay_ms(uint32_t ms);

char BUFFER_REC[BUFFER_SIZE];
volatile unsigned int i=0;
volatile uint8_t responseReceived = 0;



void USART2_config(void) 
 {
	//Configuration GPIO (PA2-TX, PA3-RX)
	RCC->AHB1ENR |= 0x1; 
  GPIOA->MODER |= GPIO_MODER_MODER2_1 | GPIO_MODER_MODER3_1;  // Alternate function
  GPIOA->AFR[0] |= (7 << (2 * 4)) | (7 << (3 * 4));  // AF7 for USART2
	//Configuration  USART2
	RCC->APB1ENR |= RCC_APB1ENR_USART2EN; 
	USART2->BRR= 0x8A; // baud rate 115200 16MHz
	USART2->CR1|=USART_CR1_RE | USART_CR1_TE | USART_CR1_IDLEIE | USART_CR1_RXNEIE |USART_CR1_UE ; // Enable RE (bit 2) & TE (bit 3) & IDLE (bit 4) & RXNE (bit 5) & USART (bit 13)
	NVIC_SetPriority(USART2_IRQn, 1);
	NVIC_EnableIRQ(USART2_IRQn);
}
void USART2_SendText(char *str)
{
	 while (*str)
 	  	 {  
         while(!(USART2->SR & (1 << 7))) ;
         USART2->DR = *str++; 
       }  
}

void USART3_SendText(char *str)
{
	 while (*str)
 	  	 {  
         while(!(USART3->SR & USART_SR_TXE)); 
         USART3->DR = *str++; 
       }  
}

void USART2_IRQHandler(void)
{ // Handle RXNE interrupt
  if (USART2->SR & (1 << 5)) {
		//flag RXNE=1 
	if (i < BUFFER_SIZE - 1) {
            BUFFER_REC[i++] = USART2->DR;
        } else {
            // reset index
            i = 0;
					  BUFFER_REC[i++] = USART2->DR;
        }
	}
	// Handle IDLE line interrupt
  if (USART2->SR & (1 << 4)) 
		//flag IDLE=1 
	{
		 volatile uint32_t temp = USART2->SR;  // Read SR
     temp = USART2->DR;                    // Read DR to clear IDLE
     (void)temp;
        
     BUFFER_REC[i] = '\0';  // Null-terminate string
		 USART3_SendText("ESP: ");
     USART3_SendText(BUFFER_REC);
       // Set flag that a response has been received
     responseReceived = 1;
	}		
		
}
void USART3_config(void) 
{
	// (PB10-TX, PB11-RX)
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN; 
  GPIOB->MODER |= GPIO_MODER_MODER10_1 | GPIO_MODER_MODER11_1; // Alternate function
  GPIOB->AFR[1] |= (7 << ((10 - 8) * 4)) | (7 << ((11 - 8) * 4)); // AF7
	
	RCC->APB1ENR |= RCC_APB1ENR_USART3EN; 
	USART3->BRR=0x8A; 
	USART3->CR1|= USART_CR1_TE | USART_CR1_UE  ; // Enable  TE (bit 3) & USART (bit 13)
}


uint8_t waitForResponse(const char* expectedResponse)
{
    uint32_t startTime = 0;
    uint32_t currentTime = 0;
    
    // Reset response flag
    responseReceived = 0;
    i = 0;
    memset(BUFFER_REC, 0, BUFFER_SIZE);
    
    // Configure TIM6 for timeout
    RCC->APB1ENR |= (1 << 4);  // Enable TIM6 clock
    TIM6->CR1 = 0;
    TIM6->CNT = 0;
    TIM6->PSC = 15999;         // 16MHz/16000 = 1KHz
    TIM6->ARR = 0xFFFF;        // Max value to count up to
    TIM6->SR = 0;              // Clear flags
    TIM6->CR1 |= TIM_CR1_CEN;  // Start timer
    
    startTime = TIM6->CNT;
    
    while (1) {
        currentTime = TIM6->CNT;
        
        // Handle timer overflow
        if (currentTime < startTime) {
            currentTime += 0xFFFF;
        }
        
        // Check if a response was received (set in USART2_IRQHandler)
        if (responseReceived) {
            // Check if the expected response is in buffer
            if (strstr(BUFFER_REC, expectedResponse) != NULL) {
                TIM6->CR1 &= ~TIM_CR1_CEN;  // Stop timer
                return 1;  // Success
            }
            
            // Response received but not the one we're waiting for
            responseReceived = 0;
            i = 0;
            memset(BUFFER_REC, 0, BUFFER_SIZE);
        }
        
        // Small delay to prevent tight loop
        for(volatile int j = 0; j < 1000; j++);
    }
}

int main(void)
{
	
	USART2_config();
	USART3_config();
	USART3_SendText("STM32 Debug UART OK\r\n"); // Send test message
	
	//Send AT commands to configure the ESP8266 in STA (client) mode

	USART3_SendText("Sending: AT\r\n");
  USART2_SendText("AT\r\n");
  if (!waitForResponse("OK")) {
        USART3_SendText("Failed to receive OK response\r\n");
    }
	
	
	USART3_SendText("Sending: AT+RST\r\n");
  USART2_SendText("AT+RST\r\n");
  if (!waitForResponse("ready")) {
        USART3_SendText("Failed to reset module\r\n");
    }
	
	
	
	 USART3_SendText("Sending: AT+CWMODE=1\r\n");
   USART2_SendText("AT+CWMODE=1\r\n"); // Station mode
   if (!waitForResponse("OK")) {
        USART3_SendText("Failed to set station mode\r\n");
    }
	
	
    USART3_SendText("Sending: AT+CWJAP\r\n");
    USART2_SendText("AT+CWJAP=\"*******\",\"*******\"\r\n"); // Connect to WiFi : write the SSID and PASSWORD
    if (!waitForResponse("OK")) {
        USART3_SendText("Failed to connect to WiFi\r\n");
    } else {
        USART3_SendText("Successfully connected to WiFi\r\n");
    }	
	
	
    USART3_SendText("Sending: AT+CIFSR\r\n");
    USART2_SendText("AT+CIFSR\r\n"); // Check IP address
    if (!waitForResponse("OK")) {
        USART3_SendText("Failed to get IP information\r\n");
    }	
		
		/////////////////////////Sending to clouds/////////////////////
		
		      /* To use ThingSpeak, you need to:

          1)Sign up for a ThingSpeak account
          2)Create a new channel in your account 
		
		      */
		
		USART3_SendText("AT+CIPSTART=TCP,api.thingspeak.com\r\n");
    USART2_SendText("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",80\r\n");// OPEN CNX TCP WITH SERVER 
    if (!waitForResponse("OK")) {
        USART3_SendText("Failed to connect\r\n");
    }	
		char *request = "GET /update?api_key=********&field1=******* HTTP/1.1\r\nHost: api.thingspeak.com\r\n\r\n";
		// you need to add the api_key for write a Channel Feed and in front of field1= you add the desired value to add 
		//each channel has its own unique API key
		//you can add more fields
    int requestLength = strlen(request);
		
		char lengthStr[20];
    sprintf(lengthStr, "AT+CIPSEND=%d\r\n", requestLength);
		
		USART3_SendText("Preparing to send data with length: ");
    USART3_SendText(lengthStr);
		USART2_SendText(lengthStr);
		
    if (!waitForResponse(">")) {
    USART3_SendText("Failed to enter send mode\r\n");
    return 0;
    }
		USART3_SendText("Sending minimal request...\r\n");
    USART2_SendText(request);

    if (!waitForResponse("SEND OK")) {
    USART3_SendText("Send failed\r\n");
    }  else {
         USART3_SendText("Data sent\r\n");
         }

	while(1);
}


