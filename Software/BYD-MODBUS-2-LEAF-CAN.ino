#include <Arduino.h>
#include "HardwareSerial.h"
#include "config.h"
#include "logging.h"
#include "mbServerFCs.h"
#include "ModbusServerRTU.h"
#include "ESP32CAN.h"
#include "CAN_config.h"

//Battery size
#define BATTERY_WH_MAX 30000

//CAN parameters
CAN_device_t CAN_cfg; // CAN Config
unsigned long previousMillis10 = 0; // will store last time a 10ms CAN Message was send
unsigned long previousMillis100 = 0; // will store last time a 100ms CAN Message was send
const int interval10 = 10; // interval (ms) at which send CAN Messages
const int interval100 = 100; // interval (ms) at which send CAN Messages
const int rx_queue_size = 10; // Receive Queue size
byte mprun10 = 0; //counter 0-3
byte mprun100 = 0; //counter 0-3

CAN_frame_t LEAF_1F2 = {.MsgID = 0x1F2, LEAF_1F2.FIR.B.DLC = 8, LEAF_1F2.FIR.B.FF = CAN_frame_std, LEAF_1F2.data.u8[0] = 0x64, LEAF_1F2.data.u8[1] = 0x64,LEAF_1F2.data.u8[2] = 0x32, LEAF_1F2.data.u8[3] = 0xA0,LEAF_1F2.data.u8[4] = 0x00,LEAF_1F2.data.u8[5] = 0x0A};
CAN_frame_t LEAF_50B = {.MsgID = 0x50B, LEAF_50B.FIR.B.DLC = 8, LEAF_50B.FIR.B.FF = CAN_frame_std, LEAF_50B.data.u8[0] = 0x00, LEAF_50B.data.u8[1] = 0x00,LEAF_50B.data.u8[2] = 0x06, LEAF_50B.data.u8[3] = 0xC0,LEAF_50B.data.u8[4] = 0x00,LEAF_50B.data.u8[5] = 0x00};
CAN_frame_t LEAF_50C = {.MsgID = 0x50C, LEAF_50C.FIR.B.DLC = 8, LEAF_50C.FIR.B.FF = CAN_frame_std, LEAF_50C.data.u8[0] = 0x00, LEAF_50C.data.u8[1] = 0x00,LEAF_50C.data.u8[2] = 0x00, LEAF_50C.data.u8[3] = 0x00,LEAF_50C.data.u8[4] = 0x00,LEAF_50C.data.u8[5] = 0x00};

//Nissan LEAF battery parameters from CAN
#define WH_PER_GID 77 //One GID is this amount of Watt hours
int LB_Discharge_Power_Limit = 0; //Limit in kW
int LB_MAX_POWER_FOR_CHARGER = 0; //Limit in kW
int LB_SOC = 0; //0 - 100.0 % (0-1000)
long LB_Wh_Remaining = 0; //Amount of energy in battery, in Wh
int LB_GIDS = 0;
int LB_MAX = 0;
int LB_Max_GIDS = 273; //Startup in 24kWh mode
int LB_Current = 0; //Current in kW going in/out of battery
int LB_Total_Voltage = 0; //Battery voltage (0-450V)

// global Modbus memory registers
const int intervalModbusTask = 10000; //Interval at which to refresh modbus registers
unsigned long previousMillisModbus = 0; //will store last time a modbus register refresh
uint16_t mbPV[30000]; // process variable memory: produced by sensors, etc., cyclic read by PLC via modbus TCP
uint16_t capacity_Wh_startup = BATTERY_WH_MAX;
uint16_t MaxPower = 40960; //41kW TODO, fetch from LEAF battery (or does it matter, polled after startup?)
uint16_t MaxVoltage = 4672; //(467.2V), if higher charging is not possible (goes into forced discharge)
uint16_t MinVoltage = 3200; //Min Voltage (320.0V), if lower Gen24 disables battery
uint16_t Status = 3; //ACTIVE - [0..5]<>[STANDBY,INACTIVE,DARKSTART,ACTIVE,FAULT,UPDATING]
uint16_t SOC = 5000; //SOC 0-100.00% //Updates later on from CAN
uint16_t capacity_Wh = BATTERY_WH_MAX; //Updates later on from CAN
uint16_t remaining_capacity_Wh = BATTERY_WH_MAX; //Updates later on from CAN
uint16_t max_target_discharge_power = 0; //0W (0W > restricts to no discharge) //Updates later on from CAN
uint16_t max_target_charge_power = 4312;
//4.3kW (during charge), both 307&308 can be set (>0) at the same time //Updates later on from CAN
uint16_t TemperatureMax = 50; //Todo, read from LEAF pack, uint not ok
uint16_t TemperatureMin = 60; //Todo, read from LEAF pack, uint not ok

// Store the data into the array
//16-bit int in these modbus-register, two letters at a time. Example p101[1]....(ascii for S) * 256 + (ascii for I) = 21321
//uint16_t p101_data[] = {21321, 1, 16985, 17408, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16985, 17408, 16993, 29812, 25970, 31021, 17007, 30720, 20594, 25965, 26997, 27904, 18518, 0, 0, 0, 13614, 12288, 0, 0, 0, 0, 0, 0, 13102, 12598, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0};
//Delete the following lines once we know this works :)
uint16_t p101_data[] = {21321, 1}; //SI
uint16_t p103_data[] = {16985, 17408, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; //BY D 
uint16_t p119_data[] = {
	16985, 17440, 16993, 29812, 25970, 31021, 17007, 30752, 20594, 25965, 26997, 27936, 18518, 0, 0, 0
}; //BY D  Ba tt er y- Bo x  Pr em iu m  HV
uint16_t p135_data[] = {13614, 12288, 0, 0, 0, 0, 0, 0, 13102, 12598, 0, 0, 0, 0, 0, 0}; //5.0 3.16
uint16_t p151_data[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; //Serial number for battery
uint16_t p167_data[] = {1, 0};
uint16_t p201_data[] = {
	0, 0, capacity_Wh_startup, MaxPower, MaxPower, MaxVoltage, MinVoltage, 53248, 10, 53248, 10, 0, 0
};
uint16_t p301_data[] = {
	Status, 0, 128, SOC, capacity_Wh, remaining_capacity_Wh, max_target_discharge_power, max_target_charge_power, 0, 0,
	2058, 0, TemperatureMin, TemperatureMax, 0, 0, 16, 22741, 0, 0, 13, 52064, 80, 9900
};
//These registers get written to
uint16_t p401_data[] = {1, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
uint16_t p1001_data[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
uint16_t i;

// Create a ModbusRTU server instance listening on Serial2 with 2000ms timeout
ModbusServerRTU MBserver(Serial2, 2000);

// Setup() - initialization happens here
void setup()
{
	//CAN pins
	pinMode(CAN_SE_PIN, OUTPUT);
	digitalWrite(CAN_SE_PIN, LOW);
	CAN_cfg.speed = CAN_SPEED_500KBPS;
	CAN_cfg.tx_pin_id = GPIO_NUM_27;
	CAN_cfg.rx_pin_id = GPIO_NUM_26;
	CAN_cfg.rx_queue = xQueueCreate(rx_queue_size, sizeof(CAN_frame_t));
	// Init CAN Module
	ESP32Can.CANInit();
	Serial.println(CAN_cfg.speed);

	//Modbus pins
	pinMode(RS485_EN_PIN, OUTPUT);
	digitalWrite(RS485_EN_PIN, HIGH);

	pinMode(RS485_SE_PIN, OUTPUT);
	digitalWrite(RS485_SE_PIN, HIGH);

	pinMode(PIN_5V_EN, OUTPUT);
	digitalWrite(PIN_5V_EN, HIGH);
	// Init Serial monitor
	Serial.begin(9600);
	while (!Serial)
	{
	}
	Serial.println("__ OK __");
	// Init Static data to the RTU Modbus
	handle_StaticDataModbus();
	// Init Serial2 connected to the RTU Modbus
	// (Fill in your data here!)
	Serial2.begin(9600, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
	// Register served function code worker for server id 21, FC 0x03
	MBserver.registerWorker(MBTCP_ID, READ_HOLD_REGISTER, &FC03);
	MBserver.registerWorker(MBTCP_ID, WRITE_HOLD_REGISTER, &FC06);
	MBserver.registerWorker(MBTCP_ID, WRITE_MULT_REGISTERS, &FC16);
	MBserver.registerWorker(MBTCP_ID, R_W_MULT_REGISTERS, &FC23);
	// Start ModbusRTU background task
	MBserver.start();
}

// perform main program functions
void loop()
{
	handle_can();
  //every 10s
	if (millis() - previousMillisModbus >= intervalModbusTask)
	{
		previousMillisModbus = millis();
    update_values();
		handle_UpdateDataModbus();
	}
}

void update_values()
{
	MaxPower = (LB_Discharge_Power_Limit * 1000); //kW to W
	SOC = (LB_SOC * 10); //increase range from 0-100.0 -> 100.00
	capacity_Wh = (LB_Max_GIDS * WH_PER_GID);
	remaining_capacity_Wh = LB_Wh_Remaining;
	max_target_discharge_power = (LB_Discharge_Power_Limit * 1000); //kW to W
	max_target_charge_power = (LB_MAX_POWER_FOR_CHARGER * 1000); //kW to W
	TemperatureMin = 50; //hardcoded, todo, read from 5C0
	TemperatureMax = 60; //hardcoded, todo, read from 5C0
}

void handle_StaticDataModbus()
{
	i = 100;
	// --- Copy the contents of the static data from the original arrays to the new modbus array ---
	for (uint16_t j = 0; j < sizeof(p101_data) / sizeof(uint16_t); j++)
	{
		mbPV[i] = p101_data[j];
		i++;
	}
	for (uint16_t j = 0; j < sizeof(p103_data) / sizeof(uint16_t); j++)
	{
		mbPV[i] = p103_data[j];
		i++;
	}
	for (uint16_t j = 0; j < sizeof(p119_data) / sizeof(uint16_t); j++)
	{
		mbPV[i] = p119_data[j];
		i++;
	}
	for (uint16_t j = 0; j < sizeof(p135_data) / sizeof(uint16_t); j++)
	{
		mbPV[i] = p135_data[j];
		i++;
	}
	for (uint16_t j = 0; j < sizeof(p151_data) / sizeof(uint16_t); j++)
	{
		mbPV[i] = p151_data[j];
		i++;
	}
	for (uint16_t j = 0; j < sizeof(p167_data) / sizeof(uint16_t); j++)
	{
		mbPV[i] = p167_data[j];
		i++;
	}
}

void handle_UpdateDataModbus()
{
	i = 200;
	// --- Copy the data contents arrays to the new modbus array ---
	for (uint16_t j = 0; j < sizeof(p201_data) / sizeof(uint16_t); j++)
	{
		mbPV[i] = p201_data[j];
		i++;
	}
	i = 300;
	for (uint16_t j = 0; j < sizeof(p301_data) / sizeof(uint16_t); j++)
	{
		mbPV[i] = p301_data[j];
		i++;
	}
}

void handle_can()
{
  CAN_frame_t rx_frame;
  unsigned long currentMillis = millis();

  // Receive next CAN frame from queue
  if (xQueueReceive(CAN_cfg.rx_queue, &rx_frame, 3 * portTICK_PERIOD_MS) == pdTRUE)
  {
    if (rx_frame.FIR.B.FF == CAN_frame_std)
    {
      //printf("New standard frame");
      switch (rx_frame.MsgID)
			{
      case 0x1DB:
        //printf("1DB \n");
				LB_Current = (rx_frame.data.u8[0] << 3) | (rx_frame.data.u8[1] & 0xe0) >> 5;
				LB_Total_Voltage = ((rx_frame.data.u8[2] << 2) | (rx_frame.data.u8[3] & 0xc0) >> 6) / 2;
				break;
			case 0x1DC:
				LB_Discharge_Power_Limit = ((rx_frame.data.u8[0] << 2 | rx_frame.data.u8[1] >> 6) / 4.0);
				LB_MAX_POWER_FOR_CHARGER = ((((rx_frame.data.u8[2] & 0x0F) << 6 | rx_frame.data.u8[3] >> 2) / 10.0) -
					10); //check if -10 is correct offset
				break;
			case 0x55B:
				LB_SOC = (rx_frame.data.u8[0] << 2 | rx_frame.data.u8[1] >> 6);
				break;
			case 0x5BC:
				LB_MAX = ((rx_frame.data.u8[5] & 0x10) >> 4);
				if (LB_MAX)
				{
					LB_Max_GIDS = (rx_frame.data.u8[0] << 2) | ((rx_frame.data.u8[1] & 0xC0) >> 6);
					//Max gids active, do nothing
					//Only the 30/40/62kWh packs have this mux
				}
				else
				{
					//Normal current GIDS value is transmitted
					LB_GIDS = (rx_frame.data.u8[0] << 2) | ((rx_frame.data.u8[1] & 0xC0) >> 6);
					LB_Wh_Remaining = (LB_GIDS * WH_PER_GID);
				}
				break;
      default:
				break;
      }      
    }
    else
    {
      //printf("New extended frame");
    }
  }
	// Send 100ms CAN Message
	if (currentMillis - previousMillis100 >= interval100)
	{
		previousMillis100 = currentMillis;

    ESP32Can.CANWriteFrame(&LEAF_50B); //Always send 50B as a static message

		mprun100++;
		if (mprun100 > 3)
		{
			mprun100 = 0;
		}

		if (mprun100 == 0)
		{
			LEAF_50C.data.u8[5] = 0x00;
			LEAF_50C.data.u8[6] = 0x5D;
			LEAF_50C.data.u8[7] = 0xC8;
		}
		else if(mprun100 == 1)
		{
			LEAF_50C.data.u8[5] = 0x01;
			LEAF_50C.data.u8[6] = 0x5D;
			LEAF_50C.data.u8[7] = 0x5F;
		}
		else if(mprun100 == 2)
		{
			LEAF_50C.data.u8[5] = 0x02;
			LEAF_50C.data.u8[6] = 0x5D;
			LEAF_50C.data.u8[7] = 0x63;
		}
		else if(mprun100 == 3)
		{
			LEAF_50C.data.u8[5] = 0x03;
			LEAF_50C.data.u8[6] = 0x5D;
			LEAF_50C.data.u8[7] = 0xF4;
		}
		ESP32Can.CANWriteFrame(&LEAF_50C);
	}
  //Send 10ms message
	if (currentMillis - previousMillis10 >= interval10)
	{ 
		previousMillis10 = currentMillis;

    if(mprun10 == 0)
    {
      LEAF_1F2.data.u8[6] = 0x00;
      LEAF_1F2.data.u8[7] = 0x8F;
    }
    else if(mprun10 == 1)
    {
      LEAF_1F2.data.u8[6] = 0x01;
      LEAF_1F2.data.u8[7] = 0x80;
    }
    else if(mprun10 == 2)
    {
      LEAF_1F2.data.u8[6] = 0x02;
      LEAF_1F2.data.u8[7] = 0x81;
    }
    else if(mprun10 == 3)
    {
      LEAF_1F2.data.u8[6] = 0x03;
      LEAF_1F2.data.u8[7] = 0x82;
    }
    ESP32Can.CANWriteFrame(&LEAF_1F2);

		mprun10++;
		if (mprun10 > 3)
		{
			mprun10 = 0;
		}
		//Serial.println("CAN 10ms done");
	}
}