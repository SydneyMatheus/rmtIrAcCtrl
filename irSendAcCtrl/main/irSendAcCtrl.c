#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/rmt.h"

//I used a base project to write my code. I don't know if it's the RTM module best practices, but it worked for my AC

static const char* LOG_TAG = "TEST_LOG";

//To recording the ON and OFF values I used Arduino IDE with IRLibRecvPCI.h, using a Arduino UNO and I also tested it
//with the ESP32 using IRrecv.h/ IRremoteESP8266.h
//I didn't try to record the IR values using RMT, just because I already have the IR values.

//Recorded command of each value for ON.
uint16_t powerOn[] = 
{
3658, 1554, 442, 1350,422, 1350,422, 638, 
  406, 618, 422, 642, 434, 1294,478, 590, 
  454, 606, 438, 1334,434, 1298,474, 586, 
  422, 1330,478, 566, 474, 586, 458, 1294, 
  474, 1298,474, 586, 458, 1314,454, 1318, 
  454, 586, 458, 586, 454, 1294,478, 606, 
  434, 586, 458, 1294,478, 586, 454, 586, 
  458, 582, 458, 582, 458, 586, 458, 582, 
  458, 562, 482, 586, 454, 566, 478, 586, 
  454, 586, 458, 586, 454, 562, 482, 586, 
  454, 562, 482, 586, 458, 582, 458, 1314, 
  458, 586, 454, 562, 482, 1314,458, 562, 
  478, 582, 458, 1314,458, 1314,458, 586, 
  454, 586, 458, 586, 458, 562, 478, 562, 
  478, 586, 458, 1282,490, 562, 482, 582, 
  458, 1334,438, 586, 454, 562, 482, 562, 
  478, 566, 474, 562, 482, 1334,434, 566, 
  478, 1314,458, 562, 478, 582, 458, 566, 
  478, 582, 458, 606, 438, 586, 454, 586, 
  458, 582, 458, 558, 482, 562, 482, 582, 
  458, 566, 478, 582, 458, 586, 458, 602, 
  438, 582, 458, 586, 454, 566, 478, 558, 
  482, 562, 482, 1290,478, 586, 458, 562, 
  478, 562, 482, 582, 458, 602, 438, 582, 
  458, 566, 478, 602, 438, 562, 478, 1314, 
  458, 562, 482, 562, 478, 562, 478, 562, 
  478, 566, 478, 558, 482, 562, 482, 1310, 
  458, 566, 478, 1294,474, 558, 486, 1294, 
  474, 586, 458, 1000
  };
  
//Recorded command of each value for OFF.
uint16_t poweroff[] =  
{
3694, 1478, 518, 1314,458, 1294,478, 582, 
  458, 586, 458, 586, 458, 1314,458, 582, 
  458, 586, 458, 1310,458, 1294,478, 586, 
  458, 1314,458, 586, 454, 606, 438, 1314, 
  458, 1290,482, 582, 458, 1314,458, 1294, 
  478, 562, 478, 586, 458, 1290,482, 586, 
  454, 586, 458, 1294,478, 582, 458, 570, 
  474, 566, 478, 582, 458, 582, 458, 586, 
  458, 558, 482, 562, 478, 590, 454, 566, 
  478, 602, 438, 602, 442, 562, 478, 582, 
  458, 586, 458, 582, 458, 562, 482, 582, 
  458, 562, 482, 582, 458, 1314,458, 602, 
  438, 602, 438, 1334,438, 1294,478, 606, 
  434, 586, 458, 562, 478, 586, 458, 562, 
  478, 586, 458, 1310,462, 582, 458, 562, 
  478, 1290,482, 582, 458, 590, 454, 558, 
  482, 586, 458, 566, 474, 1318,454, 562, 
  478, 1290,482, 586, 458, 562, 478, 582, 
  462, 582, 458, 558, 482, 582, 458, 566, 
  478, 586, 454, 562, 482, 562, 478, 586, 
  458, 586, 458, 578, 462, 562, 478, 582, 
  462, 558, 482, 582, 458, 562, 482, 558, 
  482, 582, 458, 1314,458, 582, 458, 586, 
  458, 558, 482, 586, 458, 562, 478, 582, 
  458, 582, 458, 558, 486, 582, 458, 1314, 
  458, 582, 462, 582, 458, 582, 458, 586, 
  458, 582, 458, 566, 474, 562, 482, 582, 
  458, 562, 482, 1310,458, 562, 482, 1286, 
  482, 586, 458, 1000
  };
  
#define DataLength 228    						//Length of powerOn (or powerOff, It has the same size)

const rmt_channel_t RMT_CHANNEL = RMT_CHANNEL_0;
const gpio_num_t IR_PIN = GPIO_NUM_5;			//Using GPIO 5 of ESP32
const int  RMT_TX_CARRIER_EN = 1;   			//Enable carrier for IR transmitter test with IR led

//The setup I left "default" of the code I used as a base, just changing "rmtConfig.tx_config.carrier_freq_hz = 36000;" which was 
//38000 but I don't know if is necessary to change it
void setup_rmt_config() 
{
	//put your setup code here, to run once:
	rmt_config_t rmtConfig;

	rmtConfig.rmt_mode = RMT_MODE_TX;  								//transmit mode
	rmtConfig.channel = RMT_CHANNEL;  								//channel to use 0 - 7
	rmtConfig.clk_div = 80;  										//clock divider 1 - 255. source clock is 80MHz -> 80MHz/80 = 1MHz -> 1 tick = 1 us
	rmtConfig.gpio_num = IR_PIN; 									//pin to use
	rmtConfig.mem_block_num = 1; 									//memory block size

	rmtConfig.tx_config.loop_en = 0; 								//no loop
	rmtConfig.tx_config.carrier_freq_hz = 36000;  					//36kHz carrier frequency
	rmtConfig.tx_config.carrier_duty_percent = 33; 					//duty
	rmtConfig.tx_config.carrier_level =  RMT_CARRIER_LEVEL_HIGH; 	//carrier level
	rmtConfig.tx_config.carrier_en =RMT_TX_CARRIER_EN ;  			//carrier enable
	rmtConfig.tx_config.idle_level =  RMT_IDLE_LEVEL_LOW ; 			//signal level at idle
	rmtConfig.tx_config.idle_output_en = true;  					//output if idle

	rmt_config(&rmtConfig);
	rmt_driver_install(rmtConfig.channel, 0, 0);
}

static void rmtAcCtrlTask()
{
    vTaskDelay(10);
    setup_rmt_config();
    esp_log_level_set(LOG_TAG, ESP_LOG_INFO);

	const int sendDataLength = DataLength/2;        //sendDataLength is the range of the array powerOn and powerOff It is divided by two
													//because each index of object sendDataOn holds High and Low values and the powerOn/powerOff
													//array uses an index for High and other for Low
													
	rmt_item32_t sendDataOn[sendDataLength]; 		//Data to send the Ac turn On 

	for(int i = 0, t = 0; i < (sendDataLength*2)-1; i += 2, t++)
	{
								//Odd bit High
		sendDataOn[t].duration0 = powerOn[i];		//The patern is odd bits to High on IR LED and even bits to Low  
		sendDataOn[t].level0 = 1;					//looking at the powerOn and Off array index. So this is mapped on 
								//Even bit Low		//sendDataOn for every index
		sendDataOn[t].duration1 = powerOn[i+1];
		sendDataOn[t].level1 = 0;
	}
	
	rmt_item32_t sendDataOff[sendDataLength]; 		//Data to send the Ac turn Off 

	for(int i = 0, t = 0; i < (sendDataLength*2)-1; i += 2, t++)
	{
								//Odd bit High
		sendDataOff[t].duration0 = poweroff[i];
		sendDataOff[t].level0 = 1;
								//Even bit Low
		sendDataOff[t].duration1 = poweroff[i+1];
		sendDataOff[t].level1 = 0;
	}

    while(1) 
	{
		ESP_LOGI(LOG_TAG, "SEND RMT DATA");
		
		//Just to see it working I set this task to every 5 seconds send the AC turn On and Off
		rmt_write_items(RMT_CHANNEL, sendDataOn, sendDataLength, false);
		vTaskDelay(5000 / portTICK_PERIOD_MS); //5 sec delay
		
		rmt_write_items(RMT_CHANNEL, sendDataOff, sendDataLength, false);
		vTaskDelay(5000 / portTICK_PERIOD_MS); //5 sec delay
    }
    vTaskDelete(NULL);
}

void app_main()
{
	xTaskCreate(rmtAcCtrlTask, "rmtAcCtrlTask", 10000, NULL, 10, NULL);
}