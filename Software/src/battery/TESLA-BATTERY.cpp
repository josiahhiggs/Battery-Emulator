#include "../include.h"
#ifdef TESLA_BATTERY
#include "../datalayer/datalayer.h"
#include "../datalayer/datalayer_extended.h"  //For Advanced Battery Insights webpage
#include "../devboard/utils/events.h"
#include "TESLA-BATTERY.h"

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

/* Do not change code below unless you are sure what you are doing */
/* Credits: Most of the code comes from Per Carlen's bms_comms_tesla_model3.py (https://gitlab.com/pelle8/batt2gen24/) */

// Define constants
#define MUX0_MESSAGE_INTERVAL 50  // 50ms for each mux message (100ms total)
static unsigned long previousMillis50 = 0;   // will store last time a 50ms CAN Message was sent
static unsigned long previousMillis100 = 0;  // will store last time a 100ms CAN Message was sent
static unsigned long previousMillis500 = 0;  // will store last time a 500ms CAN Message was sent
static unsigned long previousMillis1s = 0;   // will store last time a 1000ms CAN Message was sent

void transmit_can_frame(CAN_frame* tx_frame, int interface);

// Define the CAN_config structure
struct CAN_config {
  int bitrate;
  int mode;
};

// Global variables for thread synchronization
std::mutex mtx;
std::condition_variable cv;
bool ready = false;

// Declare and initialize the frame variable globally
CAN_frame frame = {.FD = false, .ext_ID = false, .DLC = 0, .ID = 0, .data = {.u8 = {0x00}}};

// Add this function declaration at the top of the file with other function declarations
uint8_t calculateCounter();

// Add the function implementation before initialize_msg
uint8_t calculateCounter() {
    static uint8_t counter = 0;
    counter = (counter + 1) & 0x0F;  // Keep 4-bit range (0-15)
    return counter;
}

// CAN frame definition for ID 0x221 545 VCFRONT_LVPowerState GenMsgCycleTime 50ms
CAN_frame TESLA_221 = {.FD = false,
  .ext_ID = false,
  .DLC = 8,
  .ID = 0x221,
  .data = {.u8 = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}};

// Structure to hold the signal values for the CAN frame
struct TESLA_221_Struct {
uint8_t VCFRONT_LVPowerStateIndex : 5;     // 5 bits
uint8_t vehiclePowerState : 2;             // 2 bits
uint8_t parkLVState : 2;                   // 2 bits
uint8_t espLVState : 2;                    // 2 bits
uint8_t radcLVState : 2;                   // 2 bits
uint8_t hvacCompLVState : 2;               // 2 bits
uint8_t ptcLVRequest : 2;                  // 2 bits
uint8_t sccmLVRequest : 2;                 // 2 bits
uint8_t tpmsLVRequest : 2;                 // 2 bits
uint8_t rcmLVRequest : 2;                  // 2 bits
uint8_t iBoosterLVState : 2;               // 2 bits
uint8_t tunerLVRequest : 2;                // 2 bits
uint8_t amplifierLVRequest : 2;            // 2 bits
uint8_t das1HighCurrentLVState : 2;        // 2 bits
uint8_t das2HighCurrentLVState : 2;        // 2 bits
uint8_t diLVRequest : 2;                   // 2 bits
uint8_t disLVState : 2;                    // 2 bits
uint8_t oilPumpFrontLVState : 2;           // 2 bits
uint8_t oilPumpRearLVRequest : 2;          // 2 bits
uint8_t ocsLVRequest : 2;                  // 2 bits
uint8_t vcleftHiCurrentLVState : 2;        // 2 bits
uint8_t vcrightHiCurrentLVState : 2;       // 2 bits
uint8_t uiHiCurrentLVState : 2;            // 2 bits
uint8_t uiAudioLVState : 2;                // 2 bits
uint8_t cpLVRequest : 2;                   // 2 bits
uint8_t epasLVState : 2;                   // 2 bits
uint8_t hvcLVRequest : 2;                  // 2 bits
uint8_t tasLVState : 2;                    // 2 bits
uint8_t pcsLVState : 2;                    // 2 bits
uint8_t VCFRONT_LVPowerStateCounter : 4;   // 4 bits
uint8_t VCFRONT_LVPowerStateChecksum : 8;  // 8 bits
};

// Implement the checksum calculation function
uint8_t calculate_checksum(const TESLA_221_Struct& msg) {
// Example checksum calculation (simple sum of all fields)
uint16_t checksum = 0;
checksum += msg.VCFRONT_LVPowerStateIndex & 0x1F;
checksum += (msg.vehiclePowerState & 0x03) << 5;
checksum += msg.parkLVState & 0x03;
checksum += (msg.espLVState & 0x03) << 2;
checksum += (msg.radcLVState & 0x03) << 4;
checksum += (msg.hvacCompLVState & 0x03) << 6;
checksum += msg.ptcLVRequest & 0x03;
checksum += (msg.sccmLVRequest & 0x03) << 2;
checksum += (msg.tpmsLVRequest & 0x03) << 4;
checksum += (msg.rcmLVRequest & 0x03) << 6;
checksum += msg.iBoosterLVState & 0x03;
checksum += (msg.tunerLVRequest & 0x03) << 2;
checksum += (msg.amplifierLVRequest & 0x03) << 4;
checksum += (msg.das1HighCurrentLVState & 0x03) << 6;
checksum += msg.das2HighCurrentLVState & 0x03;
checksum += (msg.diLVRequest & 0x03) << 2;
checksum += (msg.disLVState & 0x03) << 4;
checksum += (msg.oilPumpFrontLVState & 0x03) << 6;
checksum += msg.oilPumpRearLVRequest & 0x03;
checksum += (msg.ocsLVRequest & 0x03) << 2;
checksum += (msg.vcleftHiCurrentLVState & 0x03) << 4;
checksum += (msg.vcrightHiCurrentLVState & 0x03) << 6;
checksum += msg.uiHiCurrentLVState & 0x03;
checksum += (msg.uiAudioLVState & 0x03) << 2;
checksum += msg.cpLVRequest & 0x03;
checksum += (msg.epasLVState & 0x03) << 2;
checksum += (msg.hvcLVRequest & 0x03) << 4;
checksum += (msg.tasLVState & 0x03) << 6;
checksum += msg.pcsLVState & 0x03;
checksum += (msg.VCFRONT_LVPowerStateCounter & 0x0F) << 4;
return checksum;
}

void initialize_msg(TESLA_221_Struct& msg, bool mux0);
void update_CAN_frame_221(CAN_frame& frame, const TESLA_221_Struct& msg, bool mux0);
void initialize_and_update_CAN_frame_221();

// Function to initialize message structure with appropriate mux states
void initialize_msg(TESLA_221_Struct& msg, bool mux0) {
// Common fields for both mux0 and mux1
msg.VCFRONT_LVPowerStateIndex = mux0 ? 0 : 1;  // Set multiplexer value: Mux0 = 0, Mux1 = 1
msg.vehiclePowerState = 2;  // CONDITIONING = 1 (OFF = 0, CONDITIONING = 1, ACCESSORY = 2, DRIVE = 3)

if (mux0) {
// Mux0 specific signals
msg.parkLVState = 0;        // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.espLVState = 0;         // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.radcLVState = 0;        // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.hvacCompLVState = 0;    // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.ptcLVRequest = 0;       // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.sccmLVRequest = 0;      // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.tpmsLVRequest = 0;      // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.rcmLVRequest = 0;       // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.iBoosterLVState = 0;    // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.tunerLVRequest = 0;     // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.amplifierLVRequest = 0; // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.das1HighCurrentLVState = 0; // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.das2HighCurrentLVState = 0; // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.diLVRequest = 0;        // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.disLVState = 0;         // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.oilPumpFrontLVState = 0; // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.oilPumpRearLVRequest = 0; // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.ocsLVRequest = 0;       // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.vcleftHiCurrentLVState = 0; // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.vcrightHiCurrentLVState = 0; // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.uiHiCurrentLVState = 0; // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.uiAudioLVState = 0;     // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
} else {
// Mux1 specific signals - Second message for hv_up_for_drive
msg.cpLVRequest = 1;        // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.epasLVState = 0;        // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.hvcLVRequest = 1;       // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.tasLVState = 0;         // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
msg.pcsLVState = 1;         // OFF = 0, ON = 1, GOING_DOWN = 2, FAULT = 3
}

// Common trailer for both muxes
msg.VCFRONT_LVPowerStateCounter = calculateCounter();        // Counter
msg.VCFRONT_LVPowerStateChecksum = calculate_checksum(msg);  // Checksum calculation
}

// Update CAN frame data based on multiplexer
void update_CAN_frame_221(CAN_frame& frame, const TESLA_221_Struct& msg, bool mux0) {
// Common byte 0 for both muxes:
// - VCFRONT_LVPowerStateIndex (bits 0-4)
// - vehiclePowerState (bits 5-6)
frame.data.u8[0] = (msg.VCFRONT_LVPowerStateIndex & 0x1F) | 
((msg.vehiclePowerState & 0x03) << 5);

if (mux0) {
// Mux0 specific signals
frame.data.u8[1] = (msg.parkLVState & 0x03) | 
  ((msg.espLVState & 0x03) << 2) |
  ((msg.radcLVState & 0x03) << 4) | 
  ((msg.hvacCompLVState & 0x03) << 6);

frame.data.u8[2] = (msg.ptcLVRequest & 0x03) | 
  ((msg.sccmLVRequest & 0x03) << 2) |
  ((msg.tpmsLVRequest & 0x03) << 4) | 
  ((msg.rcmLVRequest & 0x03) << 6);

frame.data.u8[3] = (msg.iBoosterLVState & 0x03) | 
  ((msg.tunerLVRequest & 0x03) << 2) |
  ((msg.amplifierLVRequest & 0x03) << 4) | 
  ((msg.das1HighCurrentLVState & 0x03) << 6);

frame.data.u8[4] = (msg.das2HighCurrentLVState & 0x03) |
  ((msg.diLVRequest & 0x03) << 2) |
  ((msg.disLVState & 0x03) << 4) |
  ((msg.oilPumpFrontLVState & 0x03) << 6);

frame.data.u8[5] = (msg.oilPumpRearLVRequest & 0x03) |
  ((msg.ocsLVRequest & 0x03) << 2) |
  ((msg.vcleftHiCurrentLVState & 0x03) << 4) |
  ((msg.vcrightHiCurrentLVState & 0x03) << 6);

frame.data.u8[6] = (msg.uiHiCurrentLVState & 0x03) |
  ((msg.uiAudioLVState & 0x03) << 2);
}
else {
// Mux1 specific signals 
frame.data.u8[1] = (msg.cpLVRequest & 0x03) | 
  ((msg.epasLVState & 0x03) << 2) |
  ((msg.hvcLVRequest & 0x03) << 4) | 
  ((msg.tasLVState & 0x03) << 6);

frame.data.u8[2] = (msg.pcsLVState & 0x03);

// Clear unused bytes for mux1
frame.data.u8[3] = 0x00;
frame.data.u8[4] = 0x00;
frame.data.u8[5] = 0x00;
}

// Common trailer byte for both muxes
frame.data.u8[6] = ((msg.VCFRONT_LVPowerStateCounter & 0x0F) << 4);  // Counter in upper 4 bits of byte 6
frame.data.u8[7] = msg.VCFRONT_LVPowerStateChecksum;  // Checksum in byte 7
}

// LV_PowerStateIndex = mux0
// SG_ VCFRONT_LVPowerStateIndex M : 0|5@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_vehiclePowerState : 5|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_parkLVState m0 : 8|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_espLVState m0 : 10|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_radcLVState m0 : 12|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_hvacCompLVState m0 : 14|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_ptcLVRequest m0 : 16|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_sccmLVRequest m0 : 18|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_tpmsLVRequest m0 : 20|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_rcmLVRequest m0 : 22|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_iBoosterLVState m0 : 24|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_tunerLVRequest m0 : 26|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_amplifierLVRequest m0 : 28|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_das1HighCurrentLVState m0 : 30|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_das2HighCurrentLVState m0 : 32|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_diLVRequest m0 : 34|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_disLVState m0 : 36|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_oilPumpFrontLVState m0 : 38|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_oilPumpRearLVRequest m0 : 40|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_ocsLVRequest m0 : 42|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_vcleftHiCurrentLVState m0 : 44|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_vcrightHiCurrentLVState m0 : 46|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_uiHiCurrentLVState m0 : 48|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_uiAudioLVState m0 : 50|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_LVPowerStateChecksum : 56|8@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_LVPowerStateCounter : 52|4@1+ (1,0) [0|0] ""  X

// LV_PowerStateIndex = mux1
// SG_ VCFRONT_LVPowerStateIndex M : 0|5@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_vehiclePowerState : 5|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_cpLVRequest m1 : 8|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_epasLVState m1 : 10|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_hvcLVRequest m1 : 12|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_tasLVState m1 : 14|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_pcsLVState m1 : 16|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_LVPowerStateChecksum : 56|8@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_LVPowerStateCounter : 52|4@1+ (1,0) [0|0] ""  X

// CAN frame definition for ID 0x241 577 VCFRONT_coolant GenMsgCycleTime 100ms
CAN_frame TESLA_241 = {.FD = false,
  .ext_ID = false,
  .DLC = 8,
  .ID = 0x241,
  .data = {.u8 = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}};

// Structure to hold the signal values for the CAN frame
// Define scaling factors as constants
#define COOLANT_FLOW_SCALE 0.1
#define COOLANT_FLOW_OFFSET 0.0

struct TESLA_241_Struct {
// Battery coolant flow signals
uint16_t VCFRONT_coolantFlowBatActual : 9;    // 0-8: Actual battery coolant flow (LPM * 0.1)
uint16_t VCFRONT_coolantFlowBatTarget : 9;    // 9-17: Target battery coolant flow (LPM * 0.1)
uint8_t VCFRONT_coolantFlowBatReason : 4;     // 18-21: Reason for battery flow control

// Powertrain coolant flow signals  
uint16_t VCFRONT_coolantFlowPTActual : 9;     // 22-30: Actual PT coolant flow (LPM * 0.1)
uint16_t VCFRONT_coolantFlowPTTarget : 9;     // 31-39: Target PT coolant flow (LPM * 0.1)
uint8_t VCFRONT_coolantFlowPTReason : 4;      // 40-43: Reason for PT flow control

// Status signals
uint8_t VCFRONT_wasteHeatRequestType : 2;     // 44-45: Waste heat request type
uint8_t VCFRONT_coolantHasBeenFilled : 1;     // 46: Coolant filled status
uint8_t VCFRONT_radiatorIneffective : 1;      // 47: Radiator effectiveness
uint8_t VCFRONT_coolantAirPurgeBatState : 3;  // 48-50: Battery air purge state
};

// Function prototypes
void initialize_msg(TESLA_241_Struct& msg);
void update_CAN_frame_241(CAN_frame& frame, const TESLA_241_Struct& msg);

// Function to initialize the struct with default values 
void initialize_msg(TESLA_241_Struct& msg) {
msg.VCFRONT_coolantFlowBatActual = 50;  // example 5.0 LPM * 10 = 50 (value * 0.1)
msg.VCFRONT_coolantFlowBatTarget = 50;  // 5.0 LPM (value * 0.1)
msg.VCFRONT_coolantFlowBatReason =
0;  // 0 "NONE" 1 "COOLANT_AIR_PURGE" 2 "NO_FLOW_REQ" 3 "OVERRIDE_BATT" 4 "ACTIVE_MANAGER_BATT" 5 "PASSIVE_MANAGER_BATT" 6 "BMS_FLOW_REQ" 7 "DAS_FLOW_REQ" 8 "OVERRIDE_PT" 9 "ACTIVE_MANAGER_PT" 10 "PASSIVE_MANAGER_PT" 11 "PCS_FLOW_REQ" 12 "DI_FLOW_REQ" 13 "DIS_FLOW_REQ" ;
msg.VCFRONT_coolantFlowPTTarget = 0;  // 0.0 LPM (value * 0.1)
msg.VCFRONT_coolantFlowPTActual = 0;  // 0.0 LPM (value * 0.1)
msg.VCFRONT_coolantFlowPTReason =
0;  // 0 "NONE" 1 "COOLANT_AIR_PURGE" 2 "NO_FLOW_REQ" 3 "OVERRIDE_BATT" 4 "ACTIVE_MANAGER_BATT" 5 "PASSIVE_MANAGER_BATT" 6 "BMS_FLOW_REQ" 7 "DAS_FLOW_REQ" 8 "OVERRIDE_PT" 9 "ACTIVE_MANAGER_PT" 10 "PASSIVE_MANAGER_PT" 11 "PCS_FLOW_REQ" 12 "DI_FLOW_REQ" 13 "DIS_FLOW_REQ" ;
msg.VCFRONT_wasteHeatRequestType = 0;     // 0 "NONE" 1 "PARTIAL" 2 "FULL" ;
msg.VCFRONT_coolantHasBeenFilled = 0;     // 0 = false, 1 = true
msg.VCFRONT_radiatorIneffective = 0;      // 0 = false, 1 = true
msg.VCFRONT_coolantAirPurgeBatState = 0;  // 0 "INACTIVE" 1 "ACTIVE" 2 "COMPLETE" 3 "INTERRUPTED" 4 "PENDING" ;
}

// Function to update the CAN frame 0x241 with the signal values
void update_CAN_frame_241(CAN_frame& frame, const TESLA_241_Struct& msg) {
// Clear frame data
memset(frame.data.u8, 0, sizeof(frame.data.u8));

// Pack battery coolant flow signals
const uint16_t bat_actual = msg.VCFRONT_coolantFlowBatActual;
const uint16_t bat_target = msg.VCFRONT_coolantFlowBatTarget;
frame.data.u8[0] = bat_actual & 0xFF;
frame.data.u8[1] = ((bat_actual >> 8) & 0x01) | ((bat_target & 0x1FF) << 1);
frame.data.u8[2] = ((bat_target >> 7) & 0x03) | ((msg.VCFRONT_coolantFlowBatReason & 0x0F) << 2);

// Pack powertrain coolant flow signals
const uint16_t pt_actual = msg.VCFRONT_coolantFlowPTActual;
const uint16_t pt_target = msg.VCFRONT_coolantFlowPTTarget; 
frame.data.u8[3] = (pt_actual >> 1) & 0xFF;
frame.data.u8[4] = ((pt_actual >> 8) & 0x01) | ((pt_target & 0x1FF) << 1);
frame.data.u8[5] = ((pt_target >> 7) & 0x03) | ((msg.VCFRONT_coolantFlowPTReason & 0x0F) << 2);

// Pack status signals
frame.data.u8[6] = (msg.VCFRONT_wasteHeatRequestType << 4) |
  (msg.VCFRONT_coolantHasBeenFilled << 6) |
  (msg.VCFRONT_radiatorIneffective << 7);
frame.data.u8[7] = msg.VCFRONT_coolantAirPurgeBatState & 0x07;
}

// Function to initialize and update the CAN frame
void initialize_and_update_CAN_frame_241() {
TESLA_241_Struct msg;
initialize_msg(msg);
update_CAN_frame_241(TESLA_241, msg);
}

// BO_ 577 VCFRONT_coolant: 7 VEH
// SG_ VCFRONT_coolantAirPurgeBatState : 48|3@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_coolantFlowBatActual : 0|9@1+ (0.1,0) [0|0] "LPM"  X
// SG_ VCFRONT_coolantFlowBatReason : 18|4@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_coolantFlowBatTarget : 9|9@1+ (0.1,0) [0|0] "LPM"  X
// SG_ VCFRONT_coolantFlowPTActual : 22|9@1+ (0.1,0) [0|0] "LPM"  X
// SG_ VCFRONT_coolantFlowPTReason : 40|4@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_coolantFlowPTTarget : 31|9@1+ (0.1,0) [0|0] "LPM"  X
// SG_ VCFRONT_coolantHasBeenFilled : 46|1@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_radiatorIneffective : 47|1@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_wasteHeatRequestType : 44|2@1+ (1,0) [0|0] ""  X

// VAL_ 577 VCFRONT_coolantAirPurgeBatState 0 "INACTIVE" 1 "ACTIVE" 2 "COMPLETE" 3 "INTERRUPTED" 4 "PENDING" ;
// VAL_ 577 VCFRONT_coolantFlowBatReason 0 "NONE" 1 "COOLANT_AIR_PURGE" 2 "NO_FLOW_REQ" 3 "OVERRIDE_BATT" 4 "ACTIVE_MANAGER_BATT" 5 "PASSIVE_MANAGER_BATT" 6 "BMS_FLOW_REQ" 7 "DAS_FLOW_REQ" 8 "OVERRIDE_PT" 9 "ACTIVE_MANAGER_PT" 10 "PASSIVE_MANAGER_PT" 11 "PCS_FLOW_REQ" 12 "DI_FLOW_REQ" 13 "DIS_FLOW_REQ" ;
// VAL_ 577 VCFRONT_coolantFlowPTReason 0 "NONE" 1 "COOLANT_AIR_PURGE" 2 "NO_FLOW_REQ" 3 "OVERRIDE_BATT" 4 "ACTIVE_MANAGER_BATT" 5 "PASSIVE_MANAGER_BATT" 6 "BMS_FLOW_REQ" 7 "DAS_FLOW_REQ" 8 "OVERRIDE_PT" 9 "ACTIVE_MANAGER_PT" 10 "PASSIVE_MANAGER_PT" 11 "PCS_FLOW_REQ" 12 "DI_FLOW_REQ" 13 "DIS_FLOW_REQ" ;
// VAL_ 577 VCFRONT_wasteHeatRequestType 0 "NONE" 1 "PARTIAL" 2 "FULL" ;

// CAN frame definition for ID 0x2D1 (721) VCFRONT_okToUseHighPower GenMsgCycleTime 100ms
CAN_frame TESLA_2D1 = {
  .FD = false,
  .ext_ID = false,
  .DLC = 2,
  .ID = 0x2D1,
  .data = {.u8 = {0x00, 0x00}}
};

// Structure to hold the signal values for the CAN frame
struct TESLA_2D1_Struct {
  uint8_t vcleftOkToUseHighPower : 1;   // 1 bit
  uint8_t vcrightOkToUseHighPower : 1;  // 1 bit
  uint8_t das1OkToUseHighPower : 1;     // 1 bit
  uint8_t das2OkToUseHighPower : 1;     // 1 bit
  uint8_t uiOkToUseHighPower : 1;       // 1 bit
  uint8_t uiAudioOkToUseHighPower : 1;  // 1 bit
  uint8_t cpOkToUseHighPower : 1;       // 1 bit
  uint8_t premAudioOkToUseHiPower : 1;  // 1 bit
};

// Function prototypes
void initialize_msg(TESLA_2D1_Struct& msg);
void update_CAN_frame_2D1(CAN_frame& frame, const TESLA_2D1_Struct& msg);

// Function to initialize the TESLA_2D1_Struct
void initialize_msg(TESLA_2D1_Struct& msg) {
  msg.vcleftOkToUseHighPower = 1;   // 0 = false, 1 = true
  msg.vcrightOkToUseHighPower = 1;  // 0 = false, 1 = true
  msg.das1OkToUseHighPower = 1;     // 0 = false, 1 = true
  msg.das2OkToUseHighPower = 1;     // 0 = false, 1 = true
  msg.uiOkToUseHighPower = 1;       // 0 = false, 1 = true
  msg.uiAudioOkToUseHighPower = 1;  // 0 = false, 1 = true
  msg.cpOkToUseHighPower = 1;       // 0 = false, 1 = true
  msg.premAudioOkToUseHiPower = 1;  // 0 = false, 1 = true
}

// Function to pack struct data into CAN frame
void update_CAN_frame_2D1(CAN_frame& frame, const TESLA_2D1_Struct& msg) {
  // Pack all bits into byte 0
  frame.data.u8[0] = (msg.vcleftOkToUseHighPower) |
                     (msg.vcrightOkToUseHighPower << 1) |
                     (msg.das1OkToUseHighPower << 2) |
                     (msg.das2OkToUseHighPower << 3) |
                     (msg.uiOkToUseHighPower << 4) |
                     (msg.uiAudioOkToUseHighPower << 5) |
                     (msg.cpOkToUseHighPower << 6) |
                     (msg.premAudioOkToUseHiPower << 7);

  // Byte 1 is reserved/unused according to DBC
  frame.data.u8[1] = 0x00;
}

// Function to initialize and update CAN frame
void initialize_and_update_CAN_frame_2D1() {
  TESLA_2D1_Struct msg;
  initialize_msg(msg);
  update_CAN_frame_2D1(TESLA_2D1, msg);
}

//BO_ 721 VCFRONT_okToUseHighPower: 2 VEH
 //SG_ VCFRONT_cpOkToUseHighPower : 6|1@1+ (1,0) [0|0] ""  X
 //SG_ VCFRONT_das1OkToUseHighPower : 2|1@1+ (1,0) [0|0] ""  X
 //SG_ VCFRONT_das2OkToUseHighPower : 3|1@1+ (1,0) [0|0] ""  X
 //SG_ VCFRONT_premAudioOkToUseHiPower : 7|1@1+ (1,0) [0|0] ""  X
 //SG_ VCFRONT_uiAudioOkToUseHighPower : 5|1@1+ (1,0) [0|0] ""  X
 //SG_ VCFRONT_uiOkToUseHighPower : 4|1@1+ (1,0) [0|0] ""  X
 //SG_ VCFRONT_vcleftOkToUseHighPower : 0|1@1+ (1,0) [0|0] ""  X
 //SG_ VCFRONT_vcrightOkToUseHighPower : 1|1@1+ (1,0) [0|0] ""  X

// CAN frame definition for ID 0x3A1 929 VCFRONT_vehicleStatus GenMsgCycleTime 100ms
CAN_frame TESLA_3A1 = {.FD = false,
  .ext_ID = false,
  .DLC = 8,
  .ID = 0x3A1,
  .data = {.u8 = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}};

struct TESLA_3A1_Struct {
// Byte 0
uint8_t bmsHvChargeEnable : 1;       
uint8_t preconditionRequest : 1;      
uint8_t APGlassHeaterState : 3;       
uint8_t is12VBatterySupported : 1;    
uint8_t standbySupplySupported : 1;   
uint8_t thermalSystemType : 1;       

// Byte 1  
uint8_t LVLoadRequest : 1;            
uint8_t diPowerOnState : 3;           
uint8_t driverIsLeavingAnySpeed : 1;   
uint8_t statusForDrive : 2;            
uint8_t reserved1 : 1;      

// Bytes 2-3
uint16_t pcs12vVoltageTarget : 11;
uint8_t batterySupportRequest : 1;
uint8_t driverIsLeaving : 1;
uint8_t ota12VSupportRequest : 1;
uint8_t driverBuckleStatus : 1;
uint8_t driverDoorStatus : 1;

// Bytes 4-5
uint8_t driverUnbuckled : 2;
uint8_t passengerUnbuckled : 2;
uint8_t rowLeftUnbuckled : 2; 
uint8_t rowCenterUnbuckled : 2; 
uint8_t rowRightUnbuckled : 2;
uint16_t pcsEFuseVoltage : 10;

// Bytes 6-7  
uint8_t vehicleStatusCounter : 4;
uint8_t vehicleStatusChecksum : 8;
} __attribute__((packed));

// Function prototypes after struct definition
uint8_t calculateChecksum(const TESLA_3A1_Struct& msg);
uint8_t calculateCounter();
void update_CAN_frame_3A1(CAN_frame& frame, const TESLA_3A1_Struct& msg);
void initialize_msg(TESLA_3A1_Struct& msg);

// Define voltage scaling constants
#define PCS_12V_VOLTAGE_SCALE 0.01    // For 12V voltage (0.01V/bit)
#define PCS_EFUSE_VOLTAGE_SCALE 0.1   // For eFuse voltage (0.1V/bit)

// Helper functions for scaling and conversion
uint16_t scale_pcs12vVoltageTarget(double voltage) {
return static_cast<uint16_t>(voltage / PCS_12V_VOLTAGE_SCALE);
}

uint16_t scale_pcsEFuseVoltage(double voltage) {
return static_cast<uint16_t>(voltage / PCS_EFUSE_VOLTAGE_SCALE);
}

double unscale_pcs12vVoltageTarget(uint16_t scaled_voltage) {
return scaled_voltage * PCS_12V_VOLTAGE_SCALE;
}

double unscale_pcsEFuseVoltage(uint16_t scaled_voltage) {
return scaled_voltage * PCS_EFUSE_VOLTAGE_SCALE;
}

// Update checksum calculation to handle scaled values properly
uint8_t calculateChecksum(const TESLA_3A1_Struct& msg) {
uint16_t checksum = 0;

// Add all status bits
checksum += msg.bmsHvChargeEnable & 0x01;
checksum += msg.preconditionRequest & 0x01;
checksum += msg.APGlassHeaterState & 0x07; 
checksum += msg.is12VBatterySupported & 0x01;
checksum += msg.standbySupplySupported & 0x01;
checksum += msg.thermalSystemType & 0x01;

// Add control bits
checksum += msg.LVLoadRequest & 0x01;
checksum += msg.diPowerOnState & 0x07;
checksum += msg.driverIsLeavingAnySpeed & 0x01;
checksum += msg.statusForDrive & 0x03;

// Add scaled voltage values
uint16_t scaled_12v = scale_pcs12vVoltageTarget(msg.pcs12vVoltageTarget);
uint16_t scaled_efuse = scale_pcsEFuseVoltage(msg.pcsEFuseVoltage);
checksum += scaled_12v & 0xFF;
checksum += scaled_efuse & 0xFF;

// Add remaining status bits
checksum += msg.batterySupportRequest & 0x01;
checksum += msg.driverIsLeaving & 0x01;
checksum += msg.ota12VSupportRequest & 0x01;
checksum += msg.driverBuckleStatus & 0x01;
checksum += msg.driverDoorStatus & 0x01;

// Add unbuckled states
checksum += msg.driverUnbuckled & 0x03;
checksum += msg.passengerUnbuckled & 0x03;
checksum += msg.rowLeftUnbuckled & 0x03;
checksum += msg.rowCenterUnbuckled & 0x03;
checksum += msg.rowRightUnbuckled & 0x03;

// Add counter
checksum += msg.vehicleStatusCounter & 0x0F;

return checksum & 0xFF;
}

void update_CAN_frame_3A1(CAN_frame& frame, const TESLA_3A1_Struct& msg) {
memset(frame.data.u8, 0, sizeof(frame.data.u8));

// Byte 0: Base signals 
frame.data.u8[0] = msg.bmsHvChargeEnable |
(msg.preconditionRequest << 1) |
(msg.APGlassHeaterState << 2) |
(msg.is12VBatterySupported << 5) |
(msg.standbySupplySupported << 6) |
(msg.thermalSystemType << 7);

// Byte 1: Status signals
frame.data.u8[1] = msg.LVLoadRequest |
(msg.diPowerOnState << 2) |
(msg.driverIsLeavingAnySpeed << 5) |
(msg.statusForDrive << 6);

// Bytes 2-3: Voltage and status
uint16_t scaled_voltage = scale_pcs12vVoltageTarget(msg.pcs12vVoltageTarget);
frame.data.u8[2] = scaled_voltage & 0xFF;
frame.data.u8[3] = (scaled_voltage >> 8) & 0x07;
frame.data.u8[3] |= (msg.batterySupportRequest << 3) |
 (msg.driverIsLeaving << 4) |
 (msg.ota12VSupportRequest << 5) |
 (msg.driverBuckleStatus << 6) |
 (msg.driverDoorStatus << 7);

// Byte 4: Unbuckled states
frame.data.u8[4] = msg.driverUnbuckled |
(msg.passengerUnbuckled << 2) |
(msg.rowLeftUnbuckled << 4) |
(msg.rowCenterUnbuckled << 6);

// Byte 5: Row right and eFuse voltage
uint16_t efuse_voltage = scale_pcsEFuseVoltage(msg.pcsEFuseVoltage);
frame.data.u8[5] = msg.rowRightUnbuckled |
((efuse_voltage & 0x3F) << 2);

// Byte 6: Counter and remaining eFuse voltage
frame.data.u8[6] = ((efuse_voltage >> 6) & 0x0F) |
(msg.vehicleStatusCounter << 4);

// Byte 7: Checksum
frame.data.u8[7] = msg.vehicleStatusChecksum;
}

void initialize_msg(TESLA_3A1_Struct& msg) {
msg.bmsHvChargeEnable = 1;         // Enable HV charging (0=Disable, 1=Enable)
msg.preconditionRequest = 0;        // No preconditioning (0=No, 1=Yes)
msg.APGlassHeaterState = 2;        // OFF state (0=SNA, 1=ON, 2=OFF, 3=OFF_UNAVAILABLE, 4=FAULT)
msg.is12VBatterySupported = 1;     // 12V battery is supported (0=No, 1=Yes)
msg.standbySupplySupported = 0;    // No standby supply (0=No, 1=Yes)
msg.thermalSystemType = 0;         // Standard thermal system (0=Standard, 1=Performance)
msg.LVLoadRequest = 1;             // Request LV load (0=No, 1=Yes)
msg.diPowerOnState = 3;            // Powered for stationary heat (0=POWERED_OFF, 1=POWERED_ON_FOR_POST_RUN, 2=POWERED_ON_FOR_STATIONARY_HEAT, 3=POWERED_ON_FOR_DRIVE, 4=POWER_GOING_DOWN)
msg.driverIsLeavingAnySpeed = 0;   // Driver not leaving (0=No, 1=Yes)
msg.statusForDrive = 1;            // Ready for drive (0=NOT_READY_FOR_DRIVE_12V, 1=READY_FOR_DRIVE_12V, 2=EXIT_DRIVE_REQUESTED_12V)
msg.pcs12vVoltageTarget = 13.60;    // 13.60V target
msg.batterySupportRequest = 1;      // Request battery support (0=No, 1=Yes)
msg.driverIsLeaving = 0;           // Driver not leaving (0=No, 1=Yes)
msg.ota12VSupportRequest = 0;      // No OTA support request (0=No, 1=Yes)
msg.driverBuckleStatus = 0;        // Not buckled (0=UNBUCKLED, 1=BUCKLED)
msg.driverDoorStatus = 0;          // Door open (0=OPEN, 1=CLOSED)
msg.driverUnbuckled = 0;           // No unbuckle warning (0=NONE, 1=OCCUPIED_AND_UNBUCKLED, 2=SNA)
msg.passengerUnbuckled = 0;        // No unbuckle warning (0=NONE, 1=OCCUPIED_AND_UNBUCKLED, 2=SNA)
msg.rowLeftUnbuckled = 0;          // No unbuckle warning (0=NONE, 1=OCCUPIED_AND_UNBUCKLED, 2=SNA)
msg.rowCenterUnbuckled = 0;        // No unbuckle warning (0=NONE, 1=OCCUPIED_AND_UNBUCKLED, 2=SNA)
msg.rowRightUnbuckled = 0;         // No unbuckle warning (0=NONE, 1=OCCUPIED_AND_UNBUCKLED, 2=SNA)
msg.pcsEFuseVoltage = 13.0;         // 13.0V
msg.vehicleStatusCounter = calculateCounter(); // 4-bit rolling counter
msg.vehicleStatusChecksum = calculateChecksum(msg); // 8-bit checksum
}

// Function to initialize and update CAN frame
void initialize_and_update_CAN_frame_3A1() {
TESLA_3A1_Struct msg;
initialize_msg(msg);
update_CAN_frame_3A1(TESLA_3A1, msg);
}

// BO_ 929 VCFRONT_vehicleStatus: 8 VEH
// SG_ VCFRONT_12vStatusForDrive: 14|2@1+ (1,0) [0|0] "" X
// SG_ VCFRONT_2RowCenterUnbuckled: 38|2@1+ (1,0) [0|0] "" X
// SG_ VCFRONT_2RowLeftUnbuckled: 36|2@1+ (1,0) [0|0] "" X
// SG_ VCFRONT_2RowRightUnbuckled: 40|2@1+ (1,0) [0|0] "" X
// SG_ VCFRONT_APGlassHeaterState: 2|3@1+ (1,0) [0|0] "" X
// SG_ VCFRONT_LVLoadRequest: 9|1@1+ (1,0) [0|0] "" X
// SG_ VCFRONT_batterySupportRequest: 27|1@1+ (1,0) [0|0] "" X
// SG_ VCFRONT_bmsHvChargeEnable: 0|1@1+ (1,0) [0|0] "" X
// SG_ VCFRONT_diPowerOnState: 10|3@1+ (1,0) [0|0] "" X
// SG_ VCFRONT_driverBuckleStatus: 30|1@1+ (1,0) [0|0] "" X
// SG_ VCFRONT_driverDoorStatus: 31|1@1+ (1,0) [0|0] "" X
// SG_ VCFRONT_driverIsLeaving: 28|1@1+ (1,0) [0|0] "" X
// SG_ VCFRONT_driverIsLeavingAnySpeed: 13|1@1+ (1,0) [0|0] "" X
// SG_ VCFRONT_driverUnbuckled: 32|2@1+ (1,0) [0|0] "" X
// SG_ VCFRONT_is12VBatterySupported: 5|1@1+ (1,0) [0|0] "" X
// SG_ VCFRONT_ota12VSupportRequest: 29|1@1+ (1,0) [0|0] "" X
// SG_ VCFRONT_passengerUnbuckled: 34|2@1+ (1,0) [0|0] "" X
// SG_ VCFRONT_pcs12vVoltageTarget: 16|11@1+ (0.01,0) [0|0] "V" X
// SG_ VCFRONT_pcsEFuseVoltage: 42|10@1+ (0.1,0) [0|0] "V" X
// SG_ VCFRONT_preconditionRequest: 1|1@1+ (1,0) [0|0] "" X
// SG_ VCFRONT_standbySupplySupported : 6|1@1+ (1,0) [0|1] "" X
// SG_ VCFRONT_thermalSystemType : 5|1@1+ (1,0) [0|1] "" X
// SG_ VCFRONT_vehicleStatusChecksum: 56|8@1+ (1,0) [0|0] "" X
// SG_ VCFRONT_vehicleStatusCounter: 52|4@1+ (1,0) [0|0] "" X

//VAL_ 929 VCFRONT_12vStatusForDrive 0 "NOT_READY_FOR_DRIVE_12V" 1 "READY_FOR_DRIVE_12V" 2 "EXIT_DRIVE_REQUESTED_12V" ;
//VAL_ 929 VCFRONT_2RowCenterUnbuckled 0 "NONE" 1 "OCCUPIED_AND_UNBUCKLED" 2 "SNA" ;
//VAL_ 929 VCFRONT_2RowLeftUnbuckled 0 "NONE" 1 "OCCUPIED_AND_UNBUCKLED" 2 "SNA" ;
//VAL_ 929 VCFRONT_2RowRightUnbuckled 0 "NONE" 1 "OCCUPIED_AND_UNBUCKLED" 2 "SNA" ;
//VAL_ 929 VCFRONT_APGlassHeaterState 0 "SNA" 1 "ON" 2 "OFF" 3 "OFF_UNAVAILABLE" 4 "FAULT" ;
//VAL_ 929 VCFRONT_diPowerOnState 0 "POWERED_OFF" 1 "POWERED_ON_FOR_POST_RUN" 2 "POWERED_ON_FOR_STATIONARY_HEAT" 3 "POWERED_ON_FOR_DRIVE" 4 "POWER_GOING_DOWN" ;
//VAL_ 929 VCFRONT_driverBuckleStatus 0 "UNBUCKLED" 1 "BUCKLED" ;
//VAL_ 929 VCFRONT_driverDoorStatus 0 "OPEN" 1 "CLOSED" ;
//VAL_ 929 VCFRONT_driverUnbuckled 0 "NONE" 1 "OCCUPIED_AND_UNBUCKLED" 2 "SNA" ;
//VAL_ 929 VCFRONT_passengerUnbuckled 0 "NONE" 1 "OCCUPIED_AND_UNBUCKLED" 2 "SNA" ;
//VAL_ 929 VCFRONT_pcsEFuseVoltage 1023 "SNA" ;

// CAN frame definition for ID 0x333 819 UI_chargeRequest GenMsgCycleTime 500ms
CAN_frame TESLA_333 = {
  .FD = false,
  .ext_ID = false,
  .DLC = 5,
  .ID = 0x333, 
  .data = {.u8 = {0x00, 0x00, 0x00, 0x00}}};

// Structure to hold the signal values for the CAN frame
struct TESLA_333_Struct {
  uint8_t UI_openChargePortDoorRequest : 1;   // 1 bit
  uint8_t UI_closeChargePortDoorRequest : 1;  // 1 bit
  uint8_t UI_chargeEnableRequest : 1;         // 1 bit
  uint8_t UI_brickVLoggingRequest : 1;        // 1 bit
  uint8_t UI_brickBalancingDisabled : 1;      // 1 bit
  uint8_t UI_acChargeCurrentLimit : 7;        // 7 bits
  uint16_t UI_chargeTerminationPct : 10;      // 10 bits
  uint8_t UI_smartAcChargingEnabled : 1;      // 1 bit
  uint8_t UI_scheduledDepartureEnabled : 1;   // 1 bit
  uint8_t UI_socSnapshotExpirationTime : 4;   // 4 bits
  uint8_t UI_cpInletHeaterRequest : 1;        // 1 bit
};

// Function prototypes
void update_CAN_frame_333(CAN_frame& frame, const TESLA_333_Struct& msg);
void initialize_msg(TESLA_333_Struct& msg);

// Function to initialize message struct with default values
void initialize_msg(TESLA_333_Struct& msg) {
  msg.UI_openChargePortDoorRequest = 1;     // 0 = Don't open, 1 = Request open
  msg.UI_closeChargePortDoorRequest = 0;    // 0 = Don't close, 1 = Request close
  msg.UI_chargeEnableRequest = 1;           // 0 = Disable charging, 1 = Enable charging
  msg.UI_brickVLoggingRequest = 0;          // 0 = Disable logging, 1 = Enable logging
  msg.UI_brickBalancingDisabled = 0;        // 0 = Enable balancing, 1 = Disable balancing
  msg.UI_acChargeCurrentLimit = 16;         // 16A default charging current
  msg.UI_chargeTerminationPct = 1023;       // Set to 100.0% (1000 * 0.1%)
  msg.UI_smartAcChargingEnabled = 0;        // 0 = Disabled, 1 = Enabled
  msg.UI_scheduledDepartureEnabled = 0;     // 0 = Disabled, 1 = Enabled
  msg.UI_socSnapshotExpirationTime = 0;     // 0 weeks
  msg.UI_cpInletHeaterRequest = 0;          // 0 = No request, 1 = Request
}

// BO_ 819 ID333UI_chargeRequest: 5 VehicleBus
// SG_ UI_openChargePortDoorRequest : 0|1@1+ (1,0) [0|1] ""  Receiver
// SG_ UI_closeChargePortDoorRequest : 1|1@1+ (1,0) [0|1] ""  Receiver
// SG_ UI_chargeEnableRequest : 2|1@1+ (1,0) [0|1] ""  Receiver
// SG_ UI_brickVLoggingRequest : 3|1@1+ (1,0) [0|1] ""  Receiver
// SG_ UI_brickBalancingDisabled : 4|1@1+ (1,0) [0|1] ""  Receiver
// SG_ UI_acChargeCurrentLimit : 8|7@1+ (1,0) [0|127] "A"  Receiver
// SG_ UI_chargeTerminationPct : 16|10@1+ (0.1,0) [25|100] "%"  Receiver
// SG_ UI_smartAcChargingEnabled : 26|1@1+ (1,0) [0|1] ""  Receiver
// SG_ UI_scheduledDepartureEnabled : 27|1@1+ (1,0) [0|1] ""  Receiver
// SG_ UI_socSnapshotExpirationTime : 28|4@1+ (1,2) [0|0] "weeks"  Receiver
// SG_ UI_cpInletHeaterRequest : 32|1@1+ (1,0) [0|0] ""  Receiver
// VAL_ 819 UI_acChargeCurrentLimit 127 "SNA" ;
// VAL_ 819 UI_brickBalancingDisabled 0 "FALSE" 1 "TRUE" ;
// VAL_ 819 UI_brickVLoggingRequest 0 "FALSE" 1 "TRUE" ;

// Function to update the CAN frame 0x333 with the signal values
void update_CAN_frame_333(CAN_frame& frame, const TESLA_333_Struct& msg) {
  // Clear frame data first
  memset(frame.data.u8, 0, frame.DLC);

  // Pack signals into CAN frame bytes
  frame.data.u8[0] = (msg.UI_openChargePortDoorRequest & 0x01) |
                     ((msg.UI_closeChargePortDoorRequest & 0x01) << 1) |
                     ((msg.UI_chargeEnableRequest & 0x01) << 2) |
                     ((msg.UI_brickVLoggingRequest & 0x01) << 3) |
                     ((msg.UI_brickBalancingDisabled & 0x01) << 4);

  frame.data.u8[1] = (msg.UI_acChargeCurrentLimit & 0x7F) << 1;

  frame.data.u8[2] = msg.UI_chargeTerminationPct & 0xFF;

  frame.data.u8[3] = ((msg.UI_chargeTerminationPct >> 8) & 0x03) |
                     ((msg.UI_smartAcChargingEnabled & 0x01) << 2) |
                     ((msg.UI_scheduledDepartureEnabled & 0x01) << 3) |
                     ((msg.UI_socSnapshotExpirationTime & 0x0F) << 4);

  frame.data.u8[4] = (msg.UI_cpInletHeaterRequest & 0x01);
}

// Function to initialize and update the CAN frame 0x333
void initialize_and_update_CAN_frame_333() {
  TESLA_333_Struct msg;
  initialize_msg(msg);
  update_CAN_frame_333(TESLA_333, msg);
}

//BO_ 505 VCSEC_requests: 1 VEH
// SG_ VCSEC_chargePortRequest : 0|2@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_driveAttemptedWithoutAuth : 2|1@1+ (1,0) [0|0] ""  X
// VAL_ 505 VCSEC_chargePortRequest 0 "NONE" 1 "OPEN" 2 "CLOSE" 3 "SNA" ;

// CAN frame definition for ID 0x1F9 505 VCSEC_requests GenMsgCycleTime 100ms
CAN_frame TESLA_1F9 = {
  .FD = false,
  .ext_ID = false, 
  .DLC = 1,
  .ID = 0x1F9,
  .data = {.u8 = {0x00}}
};

// Structure to hold the signal values for the CAN frame
struct TESLA_1F9_Struct {
  uint8_t VCSEC_chargePortRequest : 2;        // Bit 0-1, Length 2
  uint8_t VCSEC_driveAttemptedWithoutAuth : 1;  // Bit 2, Length 1
};

// Function prototypes
void update_CAN_frame_1F9(CAN_frame& frame, const TESLA_1F9_Struct& msg);
void initialize_msg(TESLA_1F9_Struct& msg);

// Function to initialize the TESLA_1F9_Struct
void initialize_msg(TESLA_1F9_Struct& msg) {
  msg.VCSEC_chargePortRequest = 1;          // Values: 0=NONE, 1=OPEN, 2=CLOSE, 3=SNA
  msg.VCSEC_driveAttemptedWithoutAuth = 0;  // Values: 0=No attempt, 1=Attempt detected
}

// Pack signal values into CAN frame bytes
void update_CAN_frame_1F9(CAN_frame& frame, const TESLA_1F9_Struct& msg) {
  // Clear frame data first
  frame.data.u8[0] = 0x00;

  // Pack signals:
  // - VCSEC_chargePortRequest in bits 0-1
  // - VCSEC_driveAttemptedWithoutAuth in bit 2
  frame.data.u8[0] = (msg.VCSEC_chargePortRequest & 0x03) |         // Bits 0-1 
                     ((msg.VCSEC_driveAttemptedWithoutAuth & 0x01) << 2); // Bit 2
}

// Initialize and update CAN frame
void initialize_and_update_CAN_frame_1F9() {
  TESLA_1F9_Struct msg;
  initialize_msg(msg);
  update_CAN_frame_1F9(TESLA_1F9, msg);
}

// BO_ 825 VCSEC_authentication: 8 VEH
// SG_ VCSEC_MCUCommandType : 36|3@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_alarmStatus : 43|4@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_authRequested : 60|1@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_authenticationStatus : 16|2@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_chargePortLockStatus : 18|1@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_frunkRequest : 34|2@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_immobilizerState : 48|3@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_keyChannelIndexed : 56|4@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_leftFrontLockStatus : 19|1@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_leftRearLockStatus : 20|1@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_lockIndicationRequest : 51|3@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_lockRequestType : 24|5@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_numberOfPubKeysOnWhitelist : 6|5@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_prsntRsnDeltaD : 3|1@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_prsntRsnDeltaP : 39|1@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_prsntRsnDeltaR : 4|1@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_prsntRsnHighThresholdC : 0|1@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_prsntRsnHighThresholdD : 1|1@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_prsntRsnHighThresholdP : 2|1@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_prsntRsnHighThresholdR : 5|1@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_rightFrontLockStatus : 21|1@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_rightRearLockStatus : 22|1@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_simpleLockStatus : 54|2@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_summonRequest : 29|3@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_trunkLockStatus : 23|1@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_trunkRequest : 32|2@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_usingModifiedMACAddress : 40|1@1+ (1,0) [0|0] ""  X
// SG_ VCSEC_vehicleLockStatus : 12|4@1+ (1,0) [0|0] ""  X
// VAL_ 825 VCSEC_MCUCommandType 0 "NONE" 1 "REMOTE_UNLOCK" 2 "REMOTE_START" 3 "COMMAND3" 4 "COMMAND4" 5 "COMMAND5" ;
// VAL_ 825 VCSEC_alarmStatus 0 "DISARMED" 1 "ARMED" 2 "PARTIAL_ARMED" 3 "TRIGGERED_FLASH_ACTIVE" 4 "ERROR" 5 "TRIGGERED_FLASH_INACTIVE" 6 "IMMINENT" 7 "DEFAULT" 15 "SNA" ;
// VAL_ 825 VCSEC_authenticationStatus 0 "NONE" 1 "AUTHENTICATED_FOR_UNLOCK" 2 "AUTHENTICATED_FOR_DRIVE" ;
// VAL_ 825 VCSEC_chargePortLockStatus 0 "UNLOCKED" 1 "LOCKED" ;
// VAL_ 825 VCSEC_frunkRequest 0 "NONE" 1 "OPEN" 2 "SNA" ;
// VAL_ 825 VCSEC_immobilizerState 0 "IDLE" 1 "PREPARE" 2 "ENCRYPT_BEGIN" 3 "ENCRYPT" 4 "SEND_AUTH_RESPONSE" 5 "SEND_NO_GO_AUTH_RESPONSE" ;
// VAL_ 825 VCSEC_keyChannelIndexed 15 "SNA" ;
// VAL_ 825 VCSEC_leftFrontLockStatus 0 "UNLOCKED" 1 "LOCKED" ;
// VAL_ 825 VCSEC_leftRearLockStatus 0 "UNLOCKED" 1 "LOCKED" ;
// VAL_ 825 VCSEC_lockIndicationRequest 0 "NONE_SNA" 1 "SINGLE" 2 "DOUBLE" 3 "TRIPLE" 4 "HOLD" ;
// VAL_ 825 VCSEC_lockRequestType 0 "NONE" 1 "PASSIVE_SHIFT_TO_P_UNLOCK" 2 "PASSIVE_PARKBUTTON_UNLOCK" 3 "PASSIVE_INTERNAL_HANDLE_UNLOCK" 4 "PASSIVE_DRIVE_AWAY_LOCK" 5 "PASSIVE_BLE_WALKUP_UNLOCK" 6 "PASSIVE_BLE_EXTERIOR_CHARGEHANDLEBUTTON_UNLOCK" 7 "PASSIVE_BLE_EXTERIOR_HANDLE_UNLOCK" 8 "PASSIVE_BLE_INTERIOR_HANDLE_UNLOCK" 9 "PASSIVE_BLE_LOCK" 10 "CRASH_UNLOCK" 11 "ACTIVE_UI_BUTTON_UNLOCK" 12 "ACTIVE_UI_BUTTON_LOCK" 13 "ACTIVE_REMOTE_UNLOCK" 14 "ACTIVE_REMOTE_LOCK" 15 "ACTIVE_NFC_UNLOCK" 16 "ACTIVE_NFC_LOCK" 17 "ACTIVE_BLE_UNLOCK" 18 "ACTIVE_BLE_LOCK" 19 "PASSIVE_INTERNAL_LOCK_PROMOTION" ;
// VAL_ 825 VCSEC_rightFrontLockStatus 0 "UNLOCKED" 1 "LOCKED" ;
// VAL_ 825 VCSEC_rightRearLockStatus 0 "UNLOCKED" 1 "LOCKED" ;
// VAL_ 825 VCSEC_simpleLockStatus 0 "SNA" 1 "UNLOCKED" 2 "LOCKED" ;
// VAL_ 825 VCSEC_summonRequest 0 "IDLE" 1 "PRIME" 2 "FORWARD" 3 "BACKWARD" 4 "STOP" 5 "SNA" ;
// VAL_ 825 VCSEC_trunkLockStatus 0 "UNLOCKED" 1 "LOCKED" ;
// VAL_ 825 VCSEC_trunkRequest 0 "NONE" 1 "OPEN" 2 "SNA" ;
// VAL_ 825 VCSEC_vehicleLockStatus 0 "SNA" 1 "ACTIVE_NFC_UNLOCKED" 2 "ACTIVE_NFC_LOCKED" 3 "PASSIVE_SELECTIVE_UNLOCKED" 4 "PASSIVE_BLE_UNLOCKED" 5 "PASSIVE_BLE_LOCKED" 6 "ACTIVE_SELECTIVE_UNLOCKED" 7 "ACTIVE_BLE_UNLOCKED" 8 "ACTIVE_BLE_LOCKED" 9 "ACTIVE_UI_UNLOCKED" 10 "ACTIVE_UI_LOCKED" 11 "ACTIVE_REMOTE_UNLOCKED" 12 "ACTIVE_REMOTE_LOCKED" 13 "CRASH_UNLOCKED" 14 "PASSIVE_INTERNAL_UNLOCKED" 15 "PASSIVE_INTERNAL_LOCKED" ;

// CAN frame definition for ID 0x339 825 VCSEC_authentication GenMsgCycleTime 100ms
CAN_frame TESLA_339 = {.FD = false,
  .ext_ID = false,
  .DLC = 8,
  .ID = 0x339,
  .data = {.u8 = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}};

// Structure to hold signal values with proper bit lengths
struct TESLA_339_Struct {
// Byte 0
uint8_t VCSEC_prsntRsnHighThresholdC : 1;    // Bit 0
uint8_t VCSEC_prsntRsnHighThresholdD : 1;    // Bit 1  
uint8_t VCSEC_prsntRsnHighThresholdP : 1;    // Bit 2
uint8_t VCSEC_prsntRsnDeltaD : 1;            // Bit 3
uint8_t VCSEC_prsntRsnHighThresholdR : 1;    // Bit 4
uint8_t VCSEC_prsntRsnDeltaR : 1;            // Bit 5
uint8_t VCSEC_numberOfPubKeysOnWhitelist : 5; // Bits 6-10
uint8_t VCSEC_prsntRsnDeltaP : 1;            // Bit 39

// Authentication and status signals
uint8_t VCSEC_vehicleLockStatus : 4;         // Bits 12-15
uint8_t VCSEC_authenticationStatus : 2;       // Bits 16-17
uint8_t VCSEC_chargePortLockStatus : 1;      // Bit 18
uint8_t VCSEC_leftFrontLockStatus : 1;       // Bit 19
uint8_t VCSEC_leftRearLockStatus : 1;        // Bit 20
uint8_t VCSEC_rightFrontLockStatus : 1;      // Bit 21
uint8_t VCSEC_rightRearLockStatus : 1;       // Bit 22
uint8_t VCSEC_trunkLockStatus : 1;           // Bit 23
uint8_t VCSEC_lockRequestType : 5;           // Bits 24-28
uint8_t VCSEC_summonRequest : 3;             // Bits 29-31
uint8_t VCSEC_trunkRequest : 2;              // Bits 32-33
uint8_t VCSEC_frunkRequest : 2;              // Bits 34-35
uint8_t VCSEC_MCUCommandType : 3;            // Bits 36-38
uint8_t VCSEC_usingModifiedMACAddress : 1;   // Bit 40
uint8_t VCSEC_alarmStatus : 4;               // Bits 43-46
uint8_t VCSEC_immobilizerState : 3;          // Bits 48-50
uint8_t VCSEC_lockIndicationRequest : 3;     // Bits 51-53
uint8_t VCSEC_simpleLockStatus : 2;          // Bits 54-55
uint8_t VCSEC_keyChannelIndexed : 4;         // Bits 56-59
uint8_t VCSEC_authRequested : 1;             // Bit 60
};

// Function prototypes
void update_CAN_frame_339(CAN_frame& frame, const TESLA_339_Struct& msg);
void initialize_msg(TESLA_339_Struct& msg);

void initialize_msg(TESLA_339_Struct& msg) {
// Initialize presence reason thresholds and deltas
msg.VCSEC_prsntRsnHighThresholdC = 0;  // Threshold C high
msg.VCSEC_prsntRsnHighThresholdD = 0;  // Threshold D high
msg.VCSEC_prsntRsnHighThresholdP = 0;  // Threshold P high
msg.VCSEC_prsntRsnDeltaD = 0;          // Delta D
msg.VCSEC_prsntRsnHighThresholdR = 0;  // Threshold R high
msg.VCSEC_prsntRsnDeltaR = 0;          // Delta R
msg.VCSEC_numberOfPubKeysOnWhitelist = 0;  // Number of public keys
msg.VCSEC_prsntRsnDeltaP = 0;          // Delta P

// Authentication and lock status
msg.VCSEC_vehicleLockStatus = 0;        // 0 "SNA" 1 "ACTIVE_NFC_UNLOCKED" 2 "ACTIVE_NFC_LOCKED" 3 "PASSIVE_SELECTIVE_UNLOCKED" 4 "PASSIVE_BLE_UNLOCKED" 5 "PASSIVE_BLE_LOCKED" 6 "ACTIVE_SELECTIVE_UNLOCKED" 7 "ACTIVE_BLE_UNLOCKED" 8 "ACTIVE_BLE_LOCKED" 9 "ACTIVE_UI_UNLOCKED" 10 "ACTIVE_UI_LOCKED" 11 "ACTIVE_REMOTE_UNLOCKED" 12 "ACTIVE_REMOTE_LOCKED" 13 "CRASH_UNLOCKED" 14 "PASSIVE_INTERNAL_UNLOCKED" 15 "PASSIVE_INTERNAL_LOCKED"
msg.VCSEC_authenticationStatus = 2;      // 0=NONE, 1=AUTHENTICATED_FOR_UNLOCK, 2=AUTHENTICATED_FOR_DRIVE
msg.VCSEC_chargePortLockStatus = 0;     // 0=UNLOCKED, 1=LOCKED

// Door lock statuses
msg.VCSEC_leftFrontLockStatus = 0;      // 0=UNLOCKED, 1=LOCKED
msg.VCSEC_leftRearLockStatus = 0;       // 0=UNLOCKED, 1=LOCKED
msg.VCSEC_rightFrontLockStatus = 0;     // 0=UNLOCKED, 1=LOCKED
msg.VCSEC_rightRearLockStatus = 0;      // 0=UNLOCKED, 1=LOCKED
msg.VCSEC_trunkLockStatus = 0;          // 0=UNLOCKED, 1=LOCKED

// Request statuses
msg.VCSEC_lockRequestType = 0;          // 0 "NONE" 1 "PASSIVE_SHIFT_TO_P_UNLOCK" 2 "PASSIVE_PARKBUTTON_UNLOCK" 3 "PASSIVE_INTERNAL_HANDLE_UNLOCK" 4 "PASSIVE_DRIVE_AWAY_LOCK" 5 "PASSIVE_BLE_WALKUP_UNLOCK" 6 "PASSIVE_BLE_EXTERIOR_CHARGEHANDLEBUTTON_UNLOCK" 7 "PASSIVE_BLE_EXTERIOR_HANDLE_UNLOCK" 8 "PASSIVE_BLE_INTERIOR_HANDLE_UNLOCK" 9 "PASSIVE_BLE_LOCK" 10 "CRASH_UNLOCK" 11 "ACTIVE_UI_BUTTON_UNLOCK" 12 "ACTIVE_UI_BUTTON_LOCK" 13 "ACTIVE_REMOTE_UNLOCK" 14 "ACTIVE_REMOTE_LOCK" 15 "ACTIVE_NFC_UNLOCK" 16 "ACTIVE_NFC_LOCK" 17 "ACTIVE_BLE_UNLOCK" 18 "ACTIVE_BLE_LOCK" 19 "PASSIVE_INTERNAL_LOCK_PROMOTION"
msg.VCSEC_summonRequest = 0;            // 0=IDLE, 1=PRIME, 2=FORWARD, 3=BACKWARD, 4=STOP, 5=SNA
msg.VCSEC_trunkRequest = 0;             // 0=NONE, 1=OPEN, 2=SNA
msg.VCSEC_frunkRequest = 0;             // 0=NONE, 1=OPEN, 2=SNA

// Command and status fields
msg.VCSEC_MCUCommandType = 0;           //0 "NONE" 1 "REMOTE_UNLOCK" 2 "REMOTE_START" 3 "COMMAND3" 4 "COMMAND4" 5 "COMMAND5" ;
msg.VCSEC_usingModifiedMACAddress = 0;  // Modified MAC address flag
msg.VCSEC_alarmStatus = 0;              // 0 "DISARMED" 1 "ARMED" 2 "PARTIAL_ARMED" 3 "TRIGGERED_FLASH_ACTIVE" 4 "ERROR" 5 "TRIGGERED_FLASH_INACTIVE" 6 "IMMINENT" 7 "DEFAULT" 15 "SNA"
msg.VCSEC_immobilizerState = 0;         //  0 "IDLE" 1 "PREPARE" 2 "ENCRYPT_BEGIN" 3 "ENCRYPT" 4 "SEND_AUTH_RESPONSE" 5 "SEND_NO_GO_AUTH_RESPONSE"
msg.VCSEC_lockIndicationRequest = 0;    // 0=NONE_SNA, 1=SINGLE, 2=DOUBLE, 3=TRIPLE, 4=HOLD
msg.VCSEC_simpleLockStatus = 0;         // 0=SNA, 1=UNLOCKED, 2=LOCKED
msg.VCSEC_keyChannelIndexed = 15;       // 15=SNA
msg.VCSEC_authRequested = 0;            // Authentication request flag
}

void update_CAN_frame_339(CAN_frame& frame, const TESLA_339_Struct& msg) {
// Clear frame data
memset(frame.data.u8, 0, sizeof(frame.data.u8));

// Pack signals into CAN frame bytes
frame.data.u8[0] = (msg.VCSEC_prsntRsnHighThresholdC) |
(msg.VCSEC_prsntRsnHighThresholdD << 1) |
(msg.VCSEC_prsntRsnHighThresholdP << 2) |
(msg.VCSEC_prsntRsnDeltaD << 3) |
(msg.VCSEC_prsntRsnHighThresholdR << 4) |
(msg.VCSEC_prsntRsnDeltaR << 5) |
((msg.VCSEC_numberOfPubKeysOnWhitelist & 0x1F) << 6);

frame.data.u8[1] = ((msg.VCSEC_numberOfPubKeysOnWhitelist >> 2) & 0x07) |
(msg.VCSEC_prsntRsnDeltaP << 3) |
(msg.VCSEC_vehicleLockStatus << 4);

frame.data.u8[2] = (msg.VCSEC_authenticationStatus) |
(msg.VCSEC_chargePortLockStatus << 2) |
(msg.VCSEC_leftFrontLockStatus << 3) |
(msg.VCSEC_leftRearLockStatus << 4) |
(msg.VCSEC_rightFrontLockStatus << 5) |
(msg.VCSEC_rightRearLockStatus << 6) |
(msg.VCSEC_trunkLockStatus << 7);

frame.data.u8[3] = (msg.VCSEC_lockRequestType) |
(msg.VCSEC_summonRequest << 5);

frame.data.u8[4] = (msg.VCSEC_trunkRequest) |
(msg.VCSEC_frunkRequest << 2) |
(msg.VCSEC_MCUCommandType << 4);

frame.data.u8[5] = (msg.VCSEC_usingModifiedMACAddress << 4) |
(msg.VCSEC_alarmStatus << 5);

frame.data.u8[6] = (msg.VCSEC_immobilizerState) |
(msg.VCSEC_lockIndicationRequest << 3) |
(msg.VCSEC_simpleLockStatus << 6);

frame.data.u8[7] = (msg.VCSEC_keyChannelIndexed) |
(msg.VCSEC_authRequested << 4);
}

// Function to initialize and update the CAN frame 0x339
void initialize_and_update_CAN_frame_339() {
TESLA_339_Struct msg;
initialize_msg(msg);
update_CAN_frame_339(TESLA_339, msg);
}

// BO_ 801 VCFRONT_sensors: 8 VEH
// SG_ VCFRONT_tempCoolantBatInlet : 0|10@1+ (0.125,-40) [0|0] "degC"  X
// SG_ VCFRONT_tempCoolantPTInlet : 10|11@1+ (0.125,-40) [0|0] "degC"  X
// SG_ VCFRONT_coolantLevel : 21|1@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_brakeFluidLevel : 22|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_tempAmbient : 24|8@1+ (0.5,-40) [0|0] "degC"  X
// SG_ VCFRONT_tempAmbientFiltered : 40|8@1+ (0.5,-40) [0|0] "degC"  X
// SG_ VCFRONT_washerFluidLevel : 32|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_battSensorIrrational : 48|1@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_ptSensorIrrational : 49|1@1+ (1,0) [0|0] ""  X
// VAL_ 801 VCFRONT_brakeFluidLevel 0 "SNA" 1 "LOW" 2 "NORMAL" ;
// VAL_ 801 VCFRONT_coolantLevel 0 "NOT_OK" 1 "FILLED" ;
// VAL_ 801 VCFRONT_tempAmbient 0 "SNA" ;
// VAL_ 801 VCFRONT_tempAmbientFiltered 0 "SNA" ;
// VAL_ 801 VCFRONT_tempCoolantBatInlet 1023 "SNA" ;
// VAL_ 801 VCFRONT_tempCoolantPTInlet 2047 "SNA" ;
// VAL_ 801 VCFRONT_washerFluidLevel 0 "SNA" 1 "LOW" 2 "NORMAL" ;

// CAN frame definition for ID 0x321 801 VCFRONT_sensors GenMsgCycleTime 1000ms
CAN_frame TESLA_321 = {.FD = false,
  .ext_ID = false,
  .DLC = 8,
  .ID = 0x321,
  .data = {.u8 = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}};

// Structure to hold the signal values for the CAN frame
struct TESLA_321_Struct {
uint8_t VCFRONT_battSensorIrrational : 1;   // 1 bit
uint8_t VCFRONT_brakeFluidLevel : 2;        // 2 bits
uint8_t VCFRONT_coolantLevel : 1;           // 1 bit
uint8_t VCFRONT_ptSensorIrrational : 1;     // 1 bit
uint8_t VCFRONT_tempAmbient : 8;            // 8 bits
uint8_t VCFRONT_tempAmbientFiltered : 8;    // 8 bits
uint16_t VCFRONT_tempCoolantBatInlet : 10;  // 10 bits
uint16_t VCFRONT_tempCoolantPTInlet : 11;   // 11 bits
uint8_t VCFRONT_washerFluidLevel : 2;       // 2 bits
};

// Function prototypes
void update_CAN_frame_321(CAN_frame& frame, const TESLA_321_Struct& msg);
void initialize_msg(TESLA_321_Struct& msg);

// Function to initialize message struct with default values
void initialize_msg(TESLA_321_Struct& msg) {
msg.VCFRONT_battSensorIrrational = 0;    // 0 = No, 1 = Yes
msg.VCFRONT_brakeFluidLevel = 2;         // 0 = SNA, 1 = LOW, 2 = NORMAL  
msg.VCFRONT_coolantLevel = 1;            // 0 = NOT_OK, 1 = FILLED
msg.VCFRONT_ptSensorIrrational = 0;      // 0 = No, 1 = Yes
msg.VCFRONT_tempAmbient = 20.0;            // Example value in °C
msg.VCFRONT_tempAmbientFiltered = 20.0;    // Example value in °C 
msg.VCFRONT_tempCoolantBatInlet = 20.0;    // Example value in °C
msg.VCFRONT_tempCoolantPTInlet = 20.0;     // Example value in °C
msg.VCFRONT_washerFluidLevel = 2;        // 0 = SNA, 1 = LOW, 2 = NORMAL
}

// Function to update the CAN frame 0x321 with signal values
void update_CAN_frame_321(CAN_frame& frame, const TESLA_321_Struct& msg) {
// Apply temperature scaling and offset according to DBC:
// - Battery/PT coolant temps: 0.125°C/bit, -40°C offset
// - Ambient temps: 0.5°C/bit, -40°C offset
uint16_t tempCoolantBatInlet = (uint16_t)((msg.VCFRONT_tempCoolantBatInlet + 40) * 8);  // Scale to 0.125°C/bit
uint16_t tempCoolantPTInlet = (uint16_t)((msg.VCFRONT_tempCoolantPTInlet + 40) * 8);    // Scale to 0.125°C/bit
uint8_t tempAmbient = (uint8_t)((msg.VCFRONT_tempAmbient + 40) * 2);                    // Scale to 0.5°C/bit 
uint8_t tempAmbientFiltered = (uint8_t)((msg.VCFRONT_tempAmbientFiltered + 40) * 2);    // Scale to 0.5°C/bit

// Clear frame data
memset(frame.data.u8, 0, sizeof(frame.data.u8));

// Pack signals into CAN frame data bytes
frame.data.u8[0] = (msg.VCFRONT_tempCoolantBatInlet & 0x03FF); // bits 0-9
frame.data.u8[1] = ((msg.VCFRONT_tempCoolantBatInlet >> 8) & 0x03) | ((msg.VCFRONT_tempCoolantPTInlet & 0x07FF) << 2); // bits 10-20
frame.data.u8[2] = (msg.VCFRONT_tempCoolantPTInlet >> 6) & 0xFF; // bits 21-28
frame.data.u8[3] = ((msg.VCFRONT_tempCoolantPTInlet >> 14) & 0x01) | ((msg.VCFRONT_coolantLevel & 0x01) << 1) | ((msg.VCFRONT_brakeFluidLevel & 0x03) << 2); // bits 29-31
frame.data.u8[4] = (msg.VCFRONT_tempAmbient & 0xFF); // bits 32-39
frame.data.u8[5] = (msg.VCFRONT_washerFluidLevel & 0x03); // bits 40-41
frame.data.u8[6] = (msg.VCFRONT_tempAmbientFiltered & 0xFF); // bits 42-49
frame.data.u8[7] = (msg.VCFRONT_battSensorIrrational & 0x01) | ((msg.VCFRONT_ptSensorIrrational & 0x01) << 1); // bits 50-51
}

// Function to initialize and update the CAN frame 0x321
void initialize_and_update_CAN_frame_321() {
TESLA_321_Struct msg;
initialize_msg(msg);
update_CAN_frame_321(TESLA_321, msg);
}

// BO_ 801 VCFRONT_sensors: 8 VEH
// SG_ VCFRONT_tempCoolantBatInlet : 0|10@1+ (0.125,-40) [0|0] "degC"  X
// SG_ VCFRONT_tempCoolantPTInlet : 10|11@1+ (0.125,-40) [0|0] "degC"  X
// SG_ VCFRONT_coolantLevel : 21|1@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_brakeFluidLevel : 22|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_tempAmbient : 24|8@1+ (0.5,-40) [0|0] "degC"  X
// SG_ VCFRONT_tempAmbientFiltered : 40|8@1+ (0.5,-40) [0|0] "degC"  X
// SG_ VCFRONT_washerFluidLevel : 32|2@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_battSensorIrrational : 48|1@1+ (1,0) [0|0] ""  X
// SG_ VCFRONT_ptSensorIrrational : 49|1@1+ (1,0) [0|0] ""  X
// VAL_ 801 VCFRONT_brakeFluidLevel 0 "SNA" 1 "LOW" 2 "NORMAL" ;
// VAL_ 801 VCFRONT_coolantLevel 0 "NOT_OK" 1 "FILLED" ;
// VAL_ 801 VCFRONT_tempAmbient 0 "SNA" ;
// VAL_ 801 VCFRONT_tempAmbientFiltered 0 "SNA" ;
// VAL_ 801 VCFRONT_tempCoolantBatInlet 1023 "SNA" ;
// VAL_ 801 VCFRONT_tempCoolantPTInlet 2047 "SNA" ;
// VAL_ 801 VCFRONT_washerFluidLevel 0 "SNA" 1 "LOW" 2 "NORMAL" ;

// Add a main function to start the process
int main() {
  initialize_and_update_CAN_frame_221();
  initialize_and_update_CAN_frame_2D1();
  initialize_and_update_CAN_frame_3A1();
  initialize_and_update_CAN_frame_333();
  initialize_and_update_CAN_frame_1F9();
  initialize_and_update_CAN_frame_339();
  initialize_and_update_CAN_frame_321();
  initialize_and_update_CAN_frame_241();
  return 0;
}

CAN_frame TESLA_602 = {.FD = false,
                       .ext_ID = false,
                       .DLC = 8,
                       .ID = 0x602,
                       .data = {0x02, 0x27, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00}};  //Diagnostic request

static uint8_t stateMachineClearIsolationFault = 0xFF;
static uint16_t sendContactorClosingMessagesStill = 300;
static uint16_t battery_cell_max_v = 3300;
static uint16_t battery_cell_min_v = 3300;
static uint16_t battery_cell_deviation_mV = 0;  //contains the deviation between highest and lowest cell in mV
static bool cellvoltagesRead = false;
//0x3d2: 978 BMS_kwhCounter
static uint32_t battery_total_discharge = 0;
static uint32_t battery_total_charge = 0;
//0x352: 850 BMS_energyStatus
static bool BMS352_mux = false;                            // variable to store when 0x352 mux is present
static uint16_t battery_energy_buffer = 0;                 // kWh
static uint16_t battery_energy_buffer_m1 = 0;              // kWh
static uint16_t battery_energy_to_charge_complete = 0;     // kWh
static uint16_t battery_energy_to_charge_complete_m1 = 0;  // kWh
static uint16_t battery_expected_energy_remaining = 0;     // kWh
static uint16_t battery_expected_energy_remaining_m1 = 0;  // kWh
static bool battery_full_charge_complete = false;          // Changed to bool
static bool battery_fully_charged = false;                 // Changed to bool
static uint16_t battery_ideal_energy_remaining = 0;        // kWh
static uint16_t battery_ideal_energy_remaining_m0 = 0;     // kWh
static uint16_t battery_nominal_energy_remaining = 0;      // kWh
static uint16_t battery_nominal_energy_remaining_m0 = 0;   // kWh
static uint16_t battery_nominal_full_pack_energy = 0;      // Kwh
static uint16_t battery_nominal_full_pack_energy_m0 = 0;   // Kwh
//0x132 306 HVBattAmpVolt
static uint16_t battery_volts = 0;                  // V
static int16_t battery_amps = 0;                    // A
static int16_t battery_raw_amps = 0;                // A
static uint16_t battery_charge_time_remaining = 0;  // Minutes
//0x252 594 BMS_powerAvailable
static uint16_t BMS_maxRegenPower = 0;           //rename from battery_regenerative_limit
static uint16_t BMS_maxDischargePower = 0;       // rename from battery_discharge_limit
static uint16_t BMS_maxStationaryHeatPower = 0;  //rename from battery_max_heat_park
static uint16_t BMS_hvacPowerBudget = 0;         //rename from battery_hvac_max_power
static uint8_t BMS_notEnoughPowerForHeatPump = 0;
static uint8_t BMS_powerLimitState = 0;
static uint8_t BMS_inverterTQF = 0;
//0x2d2: 722 BMSVAlimits
static uint16_t battery_max_discharge_current = 0;
static uint16_t battery_max_charge_current = 0;
static uint16_t battery_bms_max_voltage = 0;
static uint16_t battery_bms_min_voltage = 0;
//0x2b4: 692 PCS_dcdcRailStatus
static uint16_t battery_dcdcHvBusVolt = 0;  // Change name from battery_high_voltage to battery_dcdcHvBusVolt
static uint16_t battery_dcdcLvBusVolt = 0;  // Change name from battery_low_voltage to battery_dcdcLvBusVolt
static uint16_t battery_dcdcLvOutputCurrent =
    0;  // Change name from battery_output_current to battery_dcdcLvOutputCurrent
//0x292: 658 BMS_socStatus
static uint16_t battery_beginning_of_life = 0;  // kWh
static uint16_t battery_soc_min = 0;
static uint16_t battery_soc_max = 0;
static uint16_t battery_soc_ui = 0;  //Change name from battery_soc_vi to reflect DBC battery_soc_ui
static uint16_t battery_soc_ave = 0;
static uint8_t battery_battTempPct = 0;
//0x392: BMS_packConfig
static uint32_t battery_packMass = 0;
static uint32_t battery_platformMaxBusVoltage = 0;
static uint32_t battery_packConfigMultiplexer = 0;
static uint32_t battery_moduleType = 0;
static uint32_t battery_reservedConfig = 0;
//0x332: 818 BattBrickMinMax:BMS_bmbMinMax
static int16_t battery_max_temp = 0;  // C*
static int16_t battery_min_temp = 0;  // C*
static uint16_t battery_BrickVoltageMax = 0;
static uint16_t battery_BrickVoltageMin = 0;
static uint8_t battery_BrickTempMaxNum = 0;
static uint8_t battery_BrickTempMinNum = 0;
static uint8_t battery_BrickModelTMax = 0;
static uint8_t battery_BrickModelTMin = 0;
static uint8_t battery_BrickVoltageMaxNum = 0;  //rename from battery_max_vno
static uint8_t battery_BrickVoltageMinNum = 0;  //rename from battery_min_vno
//0x20A: 522 HVP_contactorState
static uint8_t battery_contactor = 0;  //State of contactor
static uint8_t battery_hvil_status = 0;
static uint8_t battery_packContNegativeState = 0;
static uint8_t battery_packContPositiveState = 0;
static uint8_t battery_packContactorSetState = 0;
static bool battery_packCtrsClosingAllowed = false;    // Change to bool
static bool battery_pyroTestInProgress = false;        // Change to bool
static bool battery_packCtrsOpenNowRequested = false;  // Change to bool
static bool battery_packCtrsOpenRequested = false;     // Change to bool
static uint8_t battery_packCtrsRequestStatus = 0;
static bool battery_packCtrsResetRequestRequired = false;  // Change to bool
static bool battery_dcLinkAllowedToEnergize = false;       // Change to bool
static bool battery_fcContNegativeAuxOpen = false;         // Change to bool
static uint8_t battery_fcContNegativeState = 0;
static bool battery_fcContPositiveAuxOpen = false;  // Change to bool
static uint8_t battery_fcContPositiveState = 0;
static uint8_t battery_fcContactorSetState = 0;
static bool battery_fcCtrsClosingAllowed = false;    // Change to bool
static bool battery_fcCtrsOpenNowRequested = false;  // Change to bool
static bool battery_fcCtrsOpenRequested = false;     // Change to bool
static uint8_t battery_fcCtrsRequestStatus = 0;
static bool battery_fcCtrsResetRequestRequired = false;  // Change to bool
static bool battery_fcLinkAllowedToEnergize = false;     // Change to bool
//0x72A: BMS_serialNumber
static uint8_t BMS_SerialNumber[14] = {0};  // Stores raw HEX values for ASCII chars
//0x212: 530 BMS_status
static bool battery_BMS_hvacPowerRequest = false;          //Change to bool
static bool battery_BMS_notEnoughPowerForDrive = false;    //Change to bool
static bool battery_BMS_notEnoughPowerForSupport = false;  //Change to bool
static bool battery_BMS_preconditionAllowed = false;       //Change to bool
static bool battery_BMS_updateAllowed = false;             //Change to bool
static bool battery_BMS_activeHeatingWorthwhile = false;   //Change to bool
static bool battery_BMS_cpMiaOnHvs = false;                //Change to bool
static uint8_t battery_BMS_contactorState = 0;
static uint8_t battery_BMS_state = 0;
static uint8_t battery_BMS_hvState = 0;
static uint16_t battery_BMS_isolationResistance = 0;
static bool battery_BMS_chargeRequest = false;    //Change to bool
static bool battery_BMS_keepWarmRequest = false;  //Change to bool
static uint8_t battery_BMS_uiChargeStatus = 0;
static bool battery_BMS_diLimpRequest = false;   //Change to bool
static bool battery_BMS_okToShipByAir = false;   //Change to bool
static bool battery_BMS_okToShipByLand = false;  //Change to bool
static uint32_t battery_BMS_chgPowerAvailable = 0;
static uint8_t battery_BMS_chargeRetryCount = 0;
static bool battery_BMS_pcsPwmEnabled = false;        //Change to bool
static bool battery_BMS_ecuLogUploadRequest = false;  //Change to bool
static uint8_t battery_BMS_minPackTemperature = 0;
// 0x224:548 PCS_dcdcStatus
static uint8_t battery_PCS_dcdcPrechargeStatus = 0;
static uint8_t battery_PCS_dcdc12VSupportStatus = 0;
static uint8_t battery_PCS_dcdcHvBusDischargeStatus = 0;
static uint16_t battery_PCS_dcdcMainState = 0;
static uint8_t battery_PCS_dcdcSubState = 0;
static bool battery_PCS_dcdcFaulted = false;          //Change to bool
static bool battery_PCS_dcdcOutputIsLimited = false;  //Change to bool
static uint32_t battery_PCS_dcdcMaxOutputCurrentAllowed = 0;
static uint8_t battery_PCS_dcdcPrechargeRtyCnt = 0;
static uint8_t battery_PCS_dcdc12VSupportRtyCnt = 0;
static uint8_t battery_PCS_dcdcDischargeRtyCnt = 0;
static uint8_t battery_PCS_dcdcPwmEnableLine = 0;
static uint8_t battery_PCS_dcdcSupportingFixedLvTarget = 0;
static uint8_t battery_PCS_ecuLogUploadRequest = 0;
static uint8_t battery_PCS_dcdcPrechargeRestartCnt = 0;
static uint8_t battery_PCS_dcdcInitialPrechargeSubState = 0;
//0x312: 786 BMS_thermalStatus
static uint16_t BMS_powerDissipation = 0;
static uint16_t BMS_flowRequest = 0;
static uint16_t BMS_inletActiveCoolTargetT = 0;
static uint16_t BMS_inletPassiveTargetT = 0;
static uint16_t BMS_inletActiveHeatTargetT = 0;
static uint16_t BMS_packTMin = 0;
static uint16_t BMS_packTMax = 0;
static bool BMS_pcsNoFlowRequest = false;
static bool BMS_noFlowRequest = false;
//0x2A4; 676 PCS_thermalStatus
static int16_t PCS_chgPhATemp = 0;
static int16_t PCS_chgPhBTemp = 0;
static int16_t PCS_chgPhCTemp = 0;
static int16_t PCS_dcdcTemp = 0;
static int16_t PCS_ambientTemp = 0;
//0x2C4; 708 PCS_logging
static uint16_t PCS_logMessageSelect = 0;
static uint16_t PCS_dcdcMaxLvOutputCurrent = 0;
static uint16_t PCS_dcdcCurrentLimit = 0;
static uint16_t PCS_dcdcLvOutputCurrentTempLimit = 0;
static uint16_t PCS_dcdcUnifiedCommand = 0;
static uint16_t PCS_dcdcCLAControllerOutput = 0;
static int16_t PCS_dcdcTankVoltage = 0;
static uint16_t PCS_dcdcTankVoltageTarget = 0;
static uint16_t PCS_dcdcClaCurrentFreq = 0;
static int16_t PCS_dcdcTCommMeasured = 0;
static uint16_t PCS_dcdcShortTimeUs = 0;
static uint16_t PCS_dcdcHalfPeriodUs = 0;
static uint16_t PCS_dcdcIntervalMaxFrequency = 0;
static uint16_t PCS_dcdcIntervalMaxHvBusVolt = 0;
static uint16_t PCS_dcdcIntervalMaxLvBusVolt = 0;
static uint16_t PCS_dcdcIntervalMaxLvOutputCurr = 0;
static uint16_t PCS_dcdcIntervalMinFrequency = 0;
static uint16_t PCS_dcdcIntervalMinHvBusVolt = 0;
static uint16_t PCS_dcdcIntervalMinLvBusVolt = 0;
static uint16_t PCS_dcdcIntervalMinLvOutputCurr = 0;
static uint32_t PCS_dcdc12vSupportLifetimekWh = 0;
static uint16_t PCS_chgPhAInputIrms = 0;
static uint16_t PCS_chgPhAIntBusV = 0;
static uint16_t PCS_chgPhAIntBusVTarget = 0;
static uint8_t PCS_chgPhAOutputI = 0;
static uint16_t PCS_chgPhBInputIrms = 0;
static uint16_t PCS_chgPhBIntBusV = 0;
static uint16_t PCS_chgPhBIntBusVTarget = 0;
static uint8_t PCS_chgPhBOutputI = 0;
static uint16_t PCS_chgPhCInputIrms = 0;
static uint16_t PCS_chgPhCIntBusV = 0;
static uint16_t PCS_chgPhCIntBusVTarget = 0;
static uint8_t PCS_chgPhCOutputI = 0;
static uint16_t PCS_chgInputL1NVrms = 0;
static uint16_t PCS_chgInputL2NVrms = 0;
static uint16_t PCS_chgInputL3NVrms = 0;
static uint16_t PCS_chgInputL1L2Vrms = 0;
static uint16_t PCS_chgInputNGVrms = 0;
static uint16_t PCS_chgInputFrequencyL1N = 0;
static uint16_t PCS_chgInputFrequencyL2N = 0;
static uint16_t PCS_chgInputFrequencyL3N = 0;
static uint16_t PCS_dLogPhAChannel1Content = 0;
static uint16_t PCS_dLogPhBChannel1Content = 0;
static uint16_t PCS_dLogPhCChannel1Content = 0;
static uint16_t PCS_dLogPhAChannel1Data = 0;
static uint16_t PCS_dLogPhBChannel1Data = 0;
static uint16_t PCS_dLogPhCChannel1Data = 0;
static uint16_t PCS_dLogPhAChannel2Content = 0;
static uint16_t PCS_dLogPhBChannel2Content = 0;
static uint16_t PCS_dLogPhCChannel2Content = 0;
static uint16_t PCS_dLogPhAChannel2Data = 0;
static uint16_t PCS_dLogPhBChannel2Data = 0;
static uint16_t PCS_dLogPhCChannel2Data = 0;
static uint16_t PCS_dLogPhAChannel3Content = 0;
static uint16_t PCS_dLogPhBChannel3Content = 0;
static uint16_t PCS_dLogPhCChannel3Content = 0;
static uint16_t PCS_dLogPhAChannel3Data = 0;
static uint16_t PCS_dLogPhBChannel3Data = 0;
static uint16_t PCS_dLogPhCChannel3Data = 0;
static uint16_t PCS_dLogPhAChannel4Content = 0;
static uint16_t PCS_dLogPhBChannel4Content = 0;
static uint16_t PCS_dLogPhCChannel4Content = 0;
static uint16_t PCS_dLogPhAChannel4Data = 0;
static uint16_t PCS_dLogPhBChannel4Data = 0;
static uint16_t PCS_dLogPhCChannel4Data = 0;
static uint16_t PCS_chgPhANoFlowBucket = 0;
static uint16_t PCS_chgPhBNoFlowBucket = 0;
static uint16_t PCS_chgPhCNoFlowBucket = 0;
static uint16_t PCS_chgInputL1NVdc = 0;
static uint16_t PCS_chgInputL2NVdc = 0;
static uint16_t PCS_chgInputL3NVdc = 0;
static uint16_t PCS_chgInputL1L2Vdc = 0;
static uint16_t PCS_chgInternalPhaseConfig = 0;
static uint16_t PCS_chgOutputV = 0;
static uint16_t PCS_chgPhAState = 0;
static uint16_t PCS_chgPhBState = 0;
static uint16_t PCS_chgPhCState = 0;
static uint16_t PCS_chgPhALastShutdownReason = 0;
static uint16_t PCS_chgPhBLastShutdownReason = 0;
static uint16_t PCS_chgPhCLastShutdownReason = 0;
static uint16_t PCS_chgPhARetryCount = 0;
static uint16_t PCS_chgPhBRetryCount = 0;
static uint16_t PCS_chgPhCRetryCount = 0;
static uint16_t PCS_chgRetryCount = 0;
static uint16_t PCS_chgPhManCurrentToDist = 0;
static uint16_t PCS_chgL1NPllLocked = 0;
static uint16_t PCS_chgL2NPllLocked = 0;
static uint16_t PCS_chgL3NPllLocked = 0;
static uint16_t PCS_chgL1L2PllLocked = 0;
static uint16_t PCS_chgNgPllLocked = 0;
static uint16_t PCS_chgPhManOptimalPhsToUse = 0;
static uint16_t PCS_chg5VL1Enable = 0;
static uint16_t PCS_chgInputNGVdc = 0;
static uint8_t PCS_cpu2BootState = 0;
static uint8_t PCS_acChargeSelfTestState = 0;
static uint16_t PCS_1V5Min10s = 0;
static uint16_t PCS_1V5Max10s = 0;
static uint16_t PCS_1V2Min10s = 0;
static uint16_t PCS_1V2Max10s = 0;
static uint8_t PCS_numAlertsSet = 0;
static uint16_t PCS_chgPhAIntBusVMin10s = 0;
static uint16_t PCS_chgPhAIntBusVMax10s = 0;
static uint8_t PCS_chgPhAPchgVoltDeltaMax10s = 0;
static uint32_t PCS_chgPhALifetimekWh = 0;
static uint16_t PCS_chgPhATransientRetryCount = 0;
static uint16_t PCS_chgPhBIntBusVMin10s = 0;
static uint16_t PCS_chgPhBIntBusVMax10s = 0;
static uint8_t PCS_chgPhBPchgVoltDeltaMax10s = 0;
static uint32_t PCS_chgPhBLifetimekWh = 0;
static uint16_t PCS_chgPhBTransientRetryCount = 0;
static uint16_t PCS_chgPhCIntBusVMin10s = 0;
static uint16_t PCS_chgPhCIntBusVMax10s = 0;
static uint8_t PCS_chgPhCPchgVoltDeltaMax10s = 0;
static uint32_t PCS_chgPhCLifetimekWh = 0;
static uint16_t PCS_chgPhCTransientRetryCount = 0;
static uint16_t PCS_chgInputL1NVPeak10s = 0;
static uint16_t PCS_chgInputL2NVPeak10s = 0;
static uint16_t PCS_chgInputL3NVPeak10s = 0;
static uint8_t PCS_chgInputFreqWobblePHAPeak = 0;
static uint8_t PCS_chgInputFreqWobblePHBPeak = 0;
static uint8_t PCS_chgInputFreqWobblePHCPeak = 0;
//0x7AA: //1962 HVP_debugMessage:
static uint8_t HVP_debugMessageMultiplexer = 0;
static bool HVP_gpioPassivePyroDepl = false;       //Change to bool
static bool HVP_gpioPyroIsoEn = false;             //Change to bool
static bool HVP_gpioCpFaultIn = false;             //Change to bool
static bool HVP_gpioPackContPowerEn = false;       //Change to bool
static bool HVP_gpioHvCablesOk = false;            //Change to bool
static bool HVP_gpioHvpSelfEnable = false;         //Change to bool
static bool HVP_gpioLed = false;                   //Change to bool
static bool HVP_gpioCrashSignal = false;           //Change to bool
static bool HVP_gpioShuntDataReady = false;        //Change to bool
static bool HVP_gpioFcContPosAux = false;          //Change to bool
static bool HVP_gpioFcContNegAux = false;          //Change to bool
static bool HVP_gpioBmsEout = false;               //Change to bool
static bool HVP_gpioCpFaultOut = false;            //Change to bool
static bool HVP_gpioPyroPor = false;               //Change to bool
static bool HVP_gpioShuntEn = false;               //Change to bool
static bool HVP_gpioHvpVerEn = false;              //Change to bool
static bool HVP_gpioPackCoontPosFlywheel = false;  //Change to bool
static bool HVP_gpioCpLatchEnable = false;         //Change to bool
static bool HVP_gpioPcsEnable = false;             //Change to bool
static bool HVP_gpioPcsDcdcPwmEnable = false;      //Change to bool
static bool HVP_gpioPcsChargePwmEnable = false;    //Change to bool
static bool HVP_gpioFcContPowerEnable = false;     //Change to bool
static bool HVP_gpioHvilEnable = false;            //Change to bool
static bool HVP_gpioSecDrdy = false;               //Change to bool
static uint16_t HVP_hvp1v5Ref = 0;
static int16_t HVP_shuntCurrentDebug = 0;
static bool HVP_packCurrentMia = false;           //Change to bool
static bool HVP_auxCurrentMia = false;            //Change to bool
static bool HVP_currentSenseMia = false;          //Change to bool
static bool HVP_shuntRefVoltageMismatch = false;  //Change to bool
static bool HVP_shuntThermistorMia = false;       //Change to bool
static bool HVP_shuntHwMia = false;               //Change to bool
static int16_t HVP_dcLinkVoltage = 0;
static int16_t HVP_packVoltage = 0;
static int16_t HVP_fcLinkVoltage = 0;
static uint16_t HVP_packContVoltage = 0;
static int16_t HVP_packNegativeV = 0;
static int16_t HVP_packPositiveV = 0;
static uint16_t HVP_pyroAnalog = 0;
static int16_t HVP_dcLinkNegativeV = 0;
static int16_t HVP_dcLinkPositiveV = 0;
static int16_t HVP_fcLinkNegativeV = 0;
static uint16_t HVP_fcContCoilCurrent = 0;
static uint16_t HVP_fcContVoltage = 0;
static uint16_t HVP_hvilInVoltage = 0;
static uint16_t HVP_hvilOutVoltage = 0;
static int16_t HVP_fcLinkPositiveV = 0;
static uint16_t HVP_packContCoilCurrent = 0;
static uint16_t HVP_battery12V = 0;
static int16_t HVP_shuntRefVoltageDbg = 0;
static int16_t HVP_shuntAuxCurrentDbg = 0;
static int16_t HVP_shuntBarTempDbg = 0;
static int16_t HVP_shuntAsicTempDbg = 0;
static uint8_t HVP_shuntAuxCurrentStatus = 0;
static uint8_t HVP_shuntBarTempStatus = 0;
static uint8_t HVP_shuntAsicTempStatus = 0;
//0x3aa: HVP_alertMatrix1 Fault codes   // Change to bool
static bool battery_WatchdogReset = false;   //Warns if the processor has experienced a reset due to watchdog reset.
static bool battery_PowerLossReset = false;  //Warns if the processor has experienced a reset due to power loss.
static bool battery_SwAssertion = false;     //An internal software assertion has failed.
static bool battery_CrashEvent = false;      //Warns if the crash signal is detected by HVP
static bool battery_OverDchgCurrentFault = false;  //Warns if the pack discharge is above max discharge current limit
static bool battery_OverChargeCurrentFault =
    false;  //Warns if the pack discharge current is above max charge current limit
static bool battery_OverCurrentFault =
    false;  //Warns if the pack current (discharge or charge) is above max current limit.
static bool battery_OverTemperatureFault = false;  //A pack module temperature is above maximum temperature limit
static bool battery_OverVoltageFault = false;      //A brick voltage is above maximum voltage limit
static bool battery_UnderVoltageFault = false;     //A brick voltage is below minimum voltage limit
static bool battery_PrimaryBmbMiaFault =
    false;  //Warns if the voltage and temperature readings from primary BMB chain are mia
static bool battery_SecondaryBmbMiaFault =
    false;  //Warns if the voltage and temperature readings from secondary BMB chain are mia
static bool battery_BmbMismatchFault =
    false;  //Warns if the primary and secondary BMB chain readings don't match with each other
static bool battery_BmsHviMiaFault = false;   //Warns if the BMS node is mia on HVS or HVI CAN
static bool battery_CpMiaFault = false;       //Warns if the CP node is mia on HVS CAN
static bool battery_PcsMiaFault = false;      //The PCS node is mia on HVS CAN
static bool battery_BmsFault = false;         //Warns if the BMS ECU has faulted
static bool battery_PcsFault = false;         //Warns if the PCS ECU has faulted
static bool battery_CpFault = false;          //Warns if the CP ECU has faulted
static bool battery_ShuntHwMiaFault = false;  //Warns if the shunt current reading is not available
static bool battery_PyroMiaFault = false;     //Warns if the pyro squib is not connected
static bool battery_hvsMiaFault = false;      //Warns if the pack contactor hw fault
static bool battery_hviMiaFault = false;      //Warns if the FC contactor hw fault
static bool battery_Supply12vFault = false;  //Warns if the low voltage (12V) battery is below minimum voltage threshold
static bool battery_VerSupplyFault =
    false;                              //Warns if the Energy reserve voltage supply is below minimum voltage threshold
static bool battery_HvilFault = false;  //Warn if a High Voltage Inter Lock fault is detected
static bool battery_BmsHvsMiaFault = false;  //Warns if the BMS node is mia on HVS or HVI CAN
static bool battery_PackVoltMismatchFault =
    false;  //Warns if the pack voltage doesn't match approximately with sum of brick voltages
static bool battery_EnsMiaFault = false;         //Warns if the ENS line is not connected to HVC
static bool battery_PackPosCtrArcFault = false;  //Warns if the HVP detectes series arc at pack contactor
static bool battery_packNegCtrArcFault = false;  //Warns if the HVP detectes series arc at FC contactor
static bool battery_ShuntHwAndBmsMiaFault = false;
static bool battery_fcContHwFault = false;
static bool battery_robinOverVoltageFault = false;
static bool battery_packContHwFault = false;
static bool battery_pyroFuseBlown = false;
static bool battery_pyroFuseFailedToBlow = false;
static bool battery_CpilFault = false;
static bool battery_PackContactorFellOpen = false;
static bool battery_FcContactorFellOpen = false;
static bool battery_packCtrCloseBlocked = false;
static bool battery_fcCtrCloseBlocked = false;
static bool battery_packContactorForceOpen = false;
static bool battery_fcContactorForceOpen = false;
static bool battery_dcLinkOverVoltage = false;
static bool battery_shuntOverTemperature = false;
static bool battery_passivePyroDeploy = false;
static bool battery_logUploadRequest = false;
static bool battery_packCtrCloseFailed = false;
static bool battery_fcCtrCloseFailed = false;
static bool battery_shuntThermistorMia = false;
//0x320: 800 BMS_alertMatrix
static uint8_t battery_BMS_matrixIndex = 0;  // Changed to bool
static bool battery_BMS_a061_robinBrickOverVoltage = false;
static bool battery_BMS_a062_SW_BrickV_Imbalance = false;
static bool battery_BMS_a063_SW_ChargePort_Fault = false;
static bool battery_BMS_a064_SW_SOC_Imbalance = false;
static bool battery_BMS_a127_SW_shunt_SNA = false;
static bool battery_BMS_a128_SW_shunt_MIA = false;
static bool battery_BMS_a069_SW_Low_Power = false;
static bool battery_BMS_a130_IO_CAN_Error = false;
static bool battery_BMS_a071_SW_SM_TransCon_Not_Met = false;
static bool battery_BMS_a132_HW_BMB_OTP_Uncorrctbl = false;
static bool battery_BMS_a134_SW_Delayed_Ctr_Off = false;
static bool battery_BMS_a075_SW_Chg_Disable_Failure = false;
static bool battery_BMS_a076_SW_Dch_While_Charging = false;
static bool battery_BMS_a017_SW_Brick_OV = false;
static bool battery_BMS_a018_SW_Brick_UV = false;
static bool battery_BMS_a019_SW_Module_OT = false;
static bool battery_BMS_a021_SW_Dr_Limits_Regulation = false;
static bool battery_BMS_a022_SW_Over_Current = false;
static bool battery_BMS_a023_SW_Stack_OV = false;
static bool battery_BMS_a024_SW_Islanded_Brick = false;
static bool battery_BMS_a025_SW_PwrBalance_Anomaly = false;
static bool battery_BMS_a026_SW_HFCurrent_Anomaly = false;
static bool battery_BMS_a087_SW_Feim_Test_Blocked = false;
static bool battery_BMS_a088_SW_VcFront_MIA_InDrive = false;
static bool battery_BMS_a089_SW_VcFront_MIA = false;
static bool battery_BMS_a090_SW_Gateway_MIA = false;
static bool battery_BMS_a091_SW_ChargePort_MIA = false;
static bool battery_BMS_a092_SW_ChargePort_Mia_On_Hv = false;
static bool battery_BMS_a034_SW_Passive_Isolation = false;
static bool battery_BMS_a035_SW_Isolation = false;
static bool battery_BMS_a036_SW_HvpHvilFault = false;
static bool battery_BMS_a037_SW_Flood_Port_Open = false;
static bool battery_BMS_a158_SW_HVP_HVI_Comms = false;
static bool battery_BMS_a039_SW_DC_Link_Over_Voltage = false;
static bool battery_BMS_a041_SW_Power_On_Reset = false;
static bool battery_BMS_a042_SW_MPU_Error = false;
static bool battery_BMS_a043_SW_Watch_Dog_Reset = false;
static bool battery_BMS_a044_SW_Assertion = false;
static bool battery_BMS_a045_SW_Exception = false;
static bool battery_BMS_a046_SW_Task_Stack_Usage = false;
static bool battery_BMS_a047_SW_Task_Stack_Overflow = false;
static bool battery_BMS_a048_SW_Log_Upload_Request = false;
static bool battery_BMS_a169_SW_FC_Pack_Weld = false;
static bool battery_BMS_a050_SW_Brick_Voltage_MIA = false;
static bool battery_BMS_a051_SW_HVC_Vref_Bad = false;
static bool battery_BMS_a052_SW_PCS_MIA = false;
static bool battery_BMS_a053_SW_ThermalModel_Sanity = false;
static bool battery_BMS_a054_SW_Ver_Supply_Fault = false;
static bool battery_BMS_a176_SW_GracefulPowerOff = false;
static bool battery_BMS_a059_SW_Pack_Voltage_Sensing = false;
static bool battery_BMS_a060_SW_Leakage_Test_Failure = false;
static bool battery_BMS_a077_SW_Charger_Regulation = false;
static bool battery_BMS_a081_SW_Ctr_Close_Blocked = false;
static bool battery_BMS_a082_SW_Ctr_Force_Open = false;
static bool battery_BMS_a083_SW_Ctr_Close_Failure = false;
static bool battery_BMS_a084_SW_Sleep_Wake_Aborted = false;
static bool battery_BMS_a094_SW_Drive_Inverter_MIA = false;
static bool battery_BMS_a099_SW_BMB_Communication = false;
static bool battery_BMS_a105_SW_One_Module_Tsense = false;
static bool battery_BMS_a106_SW_All_Module_Tsense = false;
static bool battery_BMS_a107_SW_Stack_Voltage_MIA = false;
static bool battery_BMS_a121_SW_NVRAM_Config_Error = false;
static bool battery_BMS_a122_SW_BMS_Therm_Irrational = false;
static bool battery_BMS_a123_SW_Internal_Isolation = false;
static bool battery_BMS_a129_SW_VSH_Failure = false;
static bool battery_BMS_a131_Bleed_FET_Failure = false;
static bool battery_BMS_a136_SW_Module_OT_Warning = false;
static bool battery_BMS_a137_SW_Brick_UV_Warning = false;
static bool battery_BMS_a138_SW_Brick_OV_Warning = false;
static bool battery_BMS_a139_SW_DC_Link_V_Irrational = false;
static bool battery_BMS_a141_SW_BMB_Status_Warning = false;
static bool battery_BMS_a144_Hvp_Config_Mismatch = false;
static bool battery_BMS_a145_SW_SOC_Change = false;
static bool battery_BMS_a146_SW_Brick_Overdischarged = false;
static bool battery_BMS_a149_SW_Missing_Config_Block = false;
static bool battery_BMS_a151_SW_external_isolation = false;
static bool battery_BMS_a156_SW_BMB_Vref_bad = false;
static bool battery_BMS_a157_SW_HVP_HVS_Comms = false;
static bool battery_BMS_a159_SW_HVP_ECU_Error = false;
static bool battery_BMS_a161_SW_DI_Open_Request = false;
static bool battery_BMS_a162_SW_No_Power_For_Support = false;
static bool battery_BMS_a163_SW_Contactor_Mismatch = false;
static bool battery_BMS_a164_SW_Uncontrolled_Regen = false;
static bool battery_BMS_a165_SW_Pack_Partial_Weld = false;
static bool battery_BMS_a166_SW_Pack_Full_Weld = false;
static bool battery_BMS_a167_SW_FC_Partial_Weld = false;
static bool battery_BMS_a168_SW_FC_Full_Weld = false;
static bool battery_BMS_a170_SW_Limp_Mode = false;
static bool battery_BMS_a171_SW_Stack_Voltage_Sense = false;
static bool battery_BMS_a174_SW_Charge_Failure = false;
static bool battery_BMS_a179_SW_Hvp_12V_Fault = false;
static bool battery_BMS_a180_SW_ECU_reset_blocked = false;

// Function definitions
inline const char* getContactorText(int index) {
  static const char* contactorTexts[] = {"UNKNOWN(0)",  "OPEN",        "CLOSING",    "BLOCKED", "OPENING",
                                         "CLOSED",      "UNKNOWN(6)",  "WELDED",     "POS_CL",  "NEG_CL",
                                         "UNKNOWN(10)", "UNKNOWN(11)", "UNKNOWN(12)"};
  return (index >= 0 && index < sizeof(contactorTexts) / sizeof(contactorTexts[0])) ? contactorTexts[index] : "UNKNOWN";
}

inline const char* getContactorState(int index) {
  static const char* contactorStates[] = {"SNA",        "OPEN",       "PRECHARGE",   "BLOCKED",
                                          "PULLED_IN",  "OPENING",    "ECONOMIZED",  "WELDED",
                                          "UNKNOWN(8)", "UNKNOWN(9)", "UNKNOWN(10)", "UNKNOWN(11)"};
  return (index >= 0 && index < sizeof(contactorStates) / sizeof(contactorStates[0])) ? contactorStates[index]
                                                                                      : "UNKNOWN";
}

inline const char* getHvilStatusState(int index) {
  static const char* hvilStatusStates[] = {"NOT OK",
                                           "STATUS_OK",
                                           "CURRENT_SOURCE_FAULT",
                                           "INTERNAL_OPEN_FAULT",
                                           "VEHICLE_OPEN_FAULT",
                                           "PENTHOUSE_LID_OPEN_FAULT",
                                           "UNKNOWN_LOCATION_OPEN_FAULT",
                                           "VEHICLE_NODE_FAULT",
                                           "NO_12V_SUPPLY",
                                           "VEHICLE_OR_PENTHOUSE_LID_OPENFAULT",
                                           "UNKNOWN(10)",
                                           "UNKNOWN(11)",
                                           "UNKNOWN(12)",
                                           "UNKNOWN(13)",
                                           "UNKNOWN(14)",
                                           "UNKNOWN(15)"};
  return (index >= 0 && index < sizeof(hvilStatusStates) / sizeof(hvilStatusStates[0])) ? hvilStatusStates[index]
                                                                                        : "UNKNOWN";
}

inline const char* getBMSState(int index) {
  static const char* bmsStates[] = {"STANDBY",     "DRIVE", "SUPPORT", "CHARGE", "FEIM",
                                    "CLEAR_FAULT", "FAULT", "WELD",    "TEST",   "SNA"};
  return (index >= 0 && index < sizeof(bmsStates) / sizeof(bmsStates[0])) ? bmsStates[index] : "UNKNOWN";
}

inline const char* getBMSContactorState(int index) {
  static const char* bmsContactorStates[] = {"SNA", "OPEN", "OPENING", "CLOSING", "CLOSED", "WELDED", "BLOCKED"};
  return (index >= 0 && index < sizeof(bmsContactorStates) / sizeof(bmsContactorStates[0])) ? bmsContactorStates[index]
                                                                                            : "UNKNOWN";
}

inline const char* getBMSHvState(int index) {
  static const char* bmsHvStates[] = {"DOWN",          "COMING_UP",        "GOING_DOWN", "UP_FOR_DRIVE",
                                      "UP_FOR_CHARGE", "UP_FOR_DC_CHARGE", "UP"};
  return (index >= 0 && index < sizeof(bmsHvStates) / sizeof(bmsHvStates[0])) ? bmsHvStates[index] : "UNKNOWN";
}

inline const char* getBMSUiChargeStatus(int index) {
  static const char* bmsUiChargeStatuses[] = {"DISCONNECTED", "NO_POWER",        "ABOUT_TO_CHARGE",
                                              "CHARGING",     "CHARGE_COMPLETE", "CHARGE_STOPPED"};
  return (index >= 0 && index < sizeof(bmsUiChargeStatuses) / sizeof(bmsUiChargeStatuses[0]))
             ? bmsUiChargeStatuses[index]
             : "UNKNOWN";
}

inline const char* getPCS_DcdcStatus(int index) {
  static const char* pcsDcdcStatuses[] = {"IDLE", "ACTIVE", "FAULTED"};
  return (index >= 0 && index < sizeof(pcsDcdcStatuses) / sizeof(pcsDcdcStatuses[0])) ? pcsDcdcStatuses[index]
                                                                                      : "UNKNOWN";
}

inline const char* getPCS_DcdcMainState(int index) {
  static const char* pcsDcdcMainStates[] = {"STANDBY",          "12V_SUPPORT_ACTIVE", "PRECHARGE_STARTUP",
                                            "PRECHARGE_ACTIVE", "DIS_HVBUS_ACTIVE",   "SHUTDOWN",
                                            "FAULTED"};
  return (index >= 0 && index < sizeof(pcsDcdcMainStates) / sizeof(pcsDcdcMainStates[0])) ? pcsDcdcMainStates[index]
                                                                                          : "UNKNOWN";
}

inline const char* getPCS_DcdcSubState(int index) {
  static const char* pcsDcdcSubStates[] = {"PWR_UP_INIT",
                                           "STANDBY",
                                           "12V_SUPPORT_ACTIVE",
                                           "DIS_HVBUS",
                                           "PCHG_FAST_DIS_HVBUS",
                                           "PCHG_SLOW_DIS_HVBUS",
                                           "PCHG_DWELL_CHARGE",
                                           "PCHG_DWELL_WAIT",
                                           "PCHG_DI_RECOVERY_WAIT",
                                           "PCHG_ACTIVE",
                                           "PCHG_FLT_FAST_DIS_HVBUS",
                                           "SHUTDOWN",
                                           "12V_SUPPORT_FAULTED",
                                           "DIS_HVBUS_FAULTED",
                                           "PCHG_FAULTED",
                                           "CLEAR_FAULTS",
                                           "FAULTED",
                                           "NUM"};
  return (index >= 0 && index < sizeof(pcsDcdcSubStates) / sizeof(pcsDcdcSubStates[0])) ? pcsDcdcSubStates[index]
                                                                                        : "UNKNOWN";
}

inline const char* getBMSPowerLimitState(int index) {
  static const char* bmsPowerLimitStates[] = {"NOT_CALCULATED_FOR_DRIVE", "CALCULATED_FOR_DRIVE"};
  return (index >= 0 && index < sizeof(bmsPowerLimitStates) / sizeof(bmsPowerLimitStates[0]))
             ? bmsPowerLimitStates[index]
             : "UNKNOWN";
}

inline const char* getHVPStatus(int index) {
  static const char* hvpStatuses[] = {"INVALID", "NOT_AVAILABLE", "STALE", "VALID"};
  return (index >= 0 && index < sizeof(hvpStatuses) / sizeof(hvpStatuses[0])) ? hvpStatuses[index] : "UNKNOWN";
}

inline const char* getHVPContactor(int index) {
  static const char* hvpContactors[] = {"NOT_ACTIVE", "ACTIVE", "COMPLETED"};
  return (index >= 0 && index < sizeof(hvpContactors) / sizeof(hvpContactors[0])) ? hvpContactors[index] : "UNKNOWN";
}

inline const char* getFalseTrue(bool value) {
  return value ? "True" : "False";
}

inline const char* getNoYes(bool value) {
  return value ? "Yes" : "No";
}

inline const char* getFault(bool value) {
  return value ? "ACTIVE" : "NOT_ACTIVE";
}

inline const char* getPCSLogMessageSelect(int index) {
  static const char* pcsLogMessageSelect[] = {
      "PHA_1",           "PHB_1",           "PHC_1",          "CHG_1",   "CHG_2",  "CHG_3",  "DCDC_1",
      "DCDC_2",          "DCDC_3",          "SYSTEM_1",       "PHA_2",   "PHB_2",  "PHC_2",  "CHG_4",
      "DLOG_1",          "DLOG_2",          "DLOG_3",         "DLOG_4",  "DCDC_4", "DCDC_5", "CHG_NO_FLOW",
      "CHG_LINE_OFFSET", "DCDC_STATISTICS", "CHG_STATISTICS", "NUM_MSGS"};
  return (index >= 0 && index < sizeof(pcsLogMessageSelect) / sizeof(pcsLogMessageSelect[0]))
             ? pcsLogMessageSelect[index]
             : "UNKNOWN";
}

inline const char* getPCSChgPhState(int index) {
  static const char* pcsChgPhStates[] = {"INIT",  "IDLE",         "PRECHARGE",    "ENABLE",
                                         "FAULT", "CLEAR_FAULTS", "SHUTTING_DOWN"};
  return (index >= 0 && index < sizeof(pcsChgPhStates) / sizeof(pcsChgPhStates[0])) ? pcsChgPhStates[index] : "UNKNOWN";
}

inline const char* getPCSChgPhLastShutdownReason(int index) {
  static const char* pcsChgPhLastShutdownReasons[] = {"REASON_NONE",
                                                      "SW_ENABLE",
                                                      "HW_ENABLE",
                                                      "SW_FAULT",
                                                      "HW_FAULT",
                                                      "PLL_NOT_LOCKED",
                                                      "INPUT_UV",
                                                      "INPUT_OV",
                                                      "OUTPUT_UV",
                                                      "OUTPUT_OV",
                                                      "PRECHARGE_TIMEOUT",
                                                      "INT_BUS_UV",
                                                      "CONTROL_REGULATION_FAULT",
                                                      "OVER_TEMPERATURE",
                                                      "TEMP_IRRATIONAL",
                                                      "SENSOR_IRRATIONAL",
                                                      "FREQ_OUT_OF_RANGE",
                                                      "LINE_TRANSIENT_FAULT"};
  return (index >= 0 && index < sizeof(pcsChgPhLastShutdownReasons) / sizeof(pcsChgPhLastShutdownReasons[0]))
             ? pcsChgPhLastShutdownReasons[index]
             : "UNKNOWN";
}

inline const char* getPCSChgInputFrequencyLN(int index) {
  static const char* pcsChgInputFrequencyLN[] = {"FREQUENCY_UNKNOWN"};
  return (index >= 0 && index < sizeof(pcsChgInputFrequencyLN) / sizeof(pcsChgInputFrequencyLN[0]))
             ? pcsChgInputFrequencyLN[index]
             : "UNKNOWN";
}

inline const char* getPCSChgInternalPhaseConfig(int index) {
  static const char* pcsChgInternalPhaseConfigs[] = {"SNA",
                                                     "SINGLE_PHASE",
                                                     "THREE_PHASE",
                                                     "THREE_PHASE_DELTA",
                                                     "SINGLE_PHASE_IEC_GB",
                                                     "MFG_TEST_CONFIG_1",
                                                     "MFG_TEST_CONFIG_2",
                                                     "TOTAL_NUM"};
  return (index >= 0 && index < sizeof(pcsChgInternalPhaseConfigs) / sizeof(pcsChgInternalPhaseConfigs[0]))
             ? pcsChgInternalPhaseConfigs[index]
             : "UNKNOWN";
}

inline const char* getPCS_dLogContent(int index) {
  static const char* pcsDLogContents[] = {"INPUT_VOLTAGE", "INPUT_CURRENT", "OUTPUT_CURRENT", "INT_BUS_VOLTAGE"};
  return (index >= 0 && index < sizeof(pcsDLogContents) / sizeof(pcsDLogContents[0])) ? pcsDLogContents[index]
                                                                                      : "UNKNOWN";
}

void update_values_battery() {  //This function maps all the values fetched via CAN to the correct parameters used for modbus
  //After values are mapped, we perform some safety checks, and do some serial printouts

  datalayer.battery.status.soh_pptt = 9900;  //Tesla batteries do not send a SOH% value on bus. Hardcode to 99%

  datalayer.battery.status.real_soc = (battery_soc_ui * 10);  //increase SOC range from 0-100.0 -> 100.00

  datalayer.battery.status.voltage_dV = (battery_volts * 10);  //One more decimal needed (370 -> 3700)

  datalayer.battery.status.current_dA = battery_amps;  //13.0A

  //Calculate the remaining Wh amount from SOC% and max Wh value.
  datalayer.battery.status.remaining_capacity_Wh = static_cast<uint32_t>(
      (static_cast<double>(datalayer.battery.status.real_soc) / 10000) * datalayer.battery.info.total_capacity_Wh);

  // Define the allowed discharge power
  datalayer.battery.status.max_discharge_power_W = (battery_max_discharge_current * battery_volts);
  // Cap the allowed discharge power if higher than the maximum discharge power allowed
  if (datalayer.battery.status.max_discharge_power_W > MAXDISCHARGEPOWERALLOWED) {
    datalayer.battery.status.max_discharge_power_W = MAXDISCHARGEPOWERALLOWED;
  }

  //The allowed charge power behaves strangely. We instead estimate this value
  if (battery_soc_ui > 990) {
    datalayer.battery.status.max_charge_power_W = FLOAT_MAX_POWER_W;
  } else if (battery_soc_ui >
             RAMPDOWN_SOC) {  // When real SOC is between RAMPDOWN_SOC-99%, ramp the value between Max<->0
    datalayer.battery.status.max_charge_power_W =
        RAMPDOWNPOWERALLOWED * (1 - (battery_soc_ui - RAMPDOWN_SOC) / (1000.0 - RAMPDOWN_SOC));
    //If the cellvoltages start to reach overvoltage, only allow a small amount of power in
    if (datalayer.battery.info.chemistry == battery_chemistry_enum::LFP) {
      if (battery_cell_max_v > (MAX_CELL_VOLTAGE_LFP - FLOAT_START_MV)) {
        datalayer.battery.status.max_charge_power_W = FLOAT_MAX_POWER_W;
      }
    } else {  //NCM/A
      if (battery_cell_max_v > (MAX_CELL_VOLTAGE_NCA_NCM - FLOAT_START_MV)) {
        datalayer.battery.status.max_charge_power_W = FLOAT_MAX_POWER_W;
      }
    }
  } else {  // No limits, max charging power allowed
    datalayer.battery.status.max_charge_power_W = MAXCHARGEPOWERALLOWED;
  }

  datalayer.battery.status.temperature_min_dC = battery_min_temp;

  datalayer.battery.status.temperature_max_dC = battery_max_temp;

  datalayer.battery.status.cell_max_voltage_mV = battery_cell_max_v;

  datalayer.battery.status.cell_min_voltage_mV = battery_cell_min_v;

  battery_cell_deviation_mV = (battery_cell_max_v - battery_cell_min_v);

  /* Value mapping is completed. Start to check all safeties */

  //INTERNAL_OPEN_FAULT - Someone disconnected a high voltage cable while battery was in use
  if (battery_hvil_status == 3) {
    set_event(EVENT_INTERNAL_OPEN_FAULT, 0);
  } else {
    clear_event(EVENT_INTERNAL_OPEN_FAULT);
  }
  //Voltage missing, pyrofuse most likely blown
  if (datalayer.battery.status.voltage_dV == 10) {
    set_event(EVENT_BATTERY_FUSE, 0);
  } else {
    clear_event(EVENT_BATTERY_FUSE);
  }

#ifdef TESLA_MODEL_3Y_BATTERY
  // Autodetect algoritm for chemistry on 3/Y packs.
  // NCM/A batteries have 96s, LFP has 102-108s
  // Drawback with this check is that it takes 3-5minutes before all cells have been counted!
  if (datalayer.battery.info.number_of_cells > 101) {
    datalayer.battery.info.chemistry = battery_chemistry_enum::LFP;
  }

  //Once cell chemistry is determined, set maximum and minimum total pack voltage safety limits
  if (datalayer.battery.info.chemistry == battery_chemistry_enum::LFP) {
    datalayer.battery.info.max_design_voltage_dV = MAX_PACK_VOLTAGE_3Y_LFP;
    datalayer.battery.info.min_design_voltage_dV = MIN_PACK_VOLTAGE_3Y_LFP;
    datalayer.battery.info.max_cell_voltage_mV = MAX_CELL_VOLTAGE_LFP;
    datalayer.battery.info.min_cell_voltage_mV = MIN_CELL_VOLTAGE_LFP;
    datalayer.battery.info.max_cell_voltage_deviation_mV = MAX_CELL_DEVIATION_LFP;
  } else {  // NCM/A chemistry
    datalayer.battery.info.max_design_voltage_dV = MAX_PACK_VOLTAGE_3Y_NCMA;
    datalayer.battery.info.min_design_voltage_dV = MIN_PACK_VOLTAGE_3Y_NCMA;
    datalayer.battery.info.max_cell_voltage_mV = MAX_CELL_VOLTAGE_NCA_NCM;
    datalayer.battery.info.min_cell_voltage_mV = MIN_CELL_VOLTAGE_NCA_NCM;
    datalayer.battery.info.max_cell_voltage_deviation_mV = MAX_CELL_DEVIATION_NCA_NCM;
  }

  // During forced balancing request via webserver, we allow the battery to exceed normal safety parameters
  if (datalayer.battery.settings.user_requests_balancing) {
    datalayer.battery.status.real_soc = 9900;  //Force battery to show up as 99% when balancing
    datalayer.battery.info.max_design_voltage_dV = datalayer.battery.settings.balancing_max_pack_voltage_dV;
    datalayer.battery.info.max_cell_voltage_mV = datalayer.battery.settings.balancing_max_cell_voltage_mV;
    datalayer.battery.info.max_cell_voltage_deviation_mV =
        datalayer.battery.settings.balancing_max_deviation_cell_voltage_mV;
    datalayer.battery.status.max_charge_power_W = datalayer.battery.settings.balancing_float_power_W;
  }
#endif  // TESLA_MODEL_3Y_BATTERY

  // Check if user requests some action
  if (datalayer.battery.settings.user_requests_isolation_clear) {
    stateMachineClearIsolationFault = 0;  //Start the statemachine
    datalayer.battery.settings.user_requests_isolation_clear = false;
  }

  // Update webserver datalayer
  //0x20A
  datalayer_extended.tesla.status_contactor = battery_contactor;
  datalayer_extended.tesla.hvil_status = battery_hvil_status;
  datalayer_extended.tesla.packContNegativeState = battery_packContNegativeState;
  datalayer_extended.tesla.packContPositiveState = battery_packContPositiveState;
  datalayer_extended.tesla.packContactorSetState = battery_packContactorSetState;
  datalayer_extended.tesla.packCtrsClosingAllowed = battery_packCtrsClosingAllowed;
  datalayer_extended.tesla.pyroTestInProgress = battery_pyroTestInProgress;
  datalayer_extended.tesla.battery_packCtrsOpenNowRequested = battery_packCtrsOpenNowRequested;
  datalayer_extended.tesla.battery_packCtrsOpenRequested = battery_packCtrsOpenRequested;
  datalayer_extended.tesla.battery_packCtrsRequestStatus = battery_packCtrsRequestStatus;
  datalayer_extended.tesla.battery_packCtrsResetRequestRequired = battery_packCtrsResetRequestRequired;
  datalayer_extended.tesla.battery_dcLinkAllowedToEnergize = battery_dcLinkAllowedToEnergize;
  //0x72A
  memcpy(datalayer_extended.tesla.BMS_SerialNumber, BMS_SerialNumber, sizeof(BMS_SerialNumber));
  //0x2B4
  datalayer_extended.tesla.battery_dcdcLvBusVolt = battery_dcdcLvBusVolt;
  datalayer_extended.tesla.battery_dcdcHvBusVolt = battery_dcdcHvBusVolt;
  datalayer_extended.tesla.battery_dcdcLvOutputCurrent = battery_dcdcLvOutputCurrent;
  //0x352
  datalayer_extended.tesla.BMS352_mux = BMS352_mux;
  datalayer_extended.tesla.battery_nominal_full_pack_energy = battery_nominal_full_pack_energy;
  datalayer_extended.tesla.battery_nominal_full_pack_energy_m0 = battery_nominal_full_pack_energy_m0;
  datalayer_extended.tesla.battery_nominal_energy_remaining = battery_nominal_energy_remaining;
  datalayer_extended.tesla.battery_nominal_energy_remaining_m0 = battery_nominal_energy_remaining_m0;
  datalayer_extended.tesla.battery_ideal_energy_remaining = battery_ideal_energy_remaining;
  datalayer_extended.tesla.battery_ideal_energy_remaining_m0 = battery_ideal_energy_remaining_m0;
  datalayer_extended.tesla.battery_energy_to_charge_complete = battery_energy_to_charge_complete;
  datalayer_extended.tesla.battery_energy_to_charge_complete_m1 = battery_energy_to_charge_complete_m1;
  datalayer_extended.tesla.battery_energy_buffer = battery_energy_buffer;
  datalayer_extended.tesla.battery_energy_buffer_m1 = battery_energy_buffer_m1;
  datalayer_extended.tesla.battery_expected_energy_remaining_m1 = battery_expected_energy_remaining_m1;
  datalayer_extended.tesla.battery_full_charge_complete = battery_full_charge_complete;
  datalayer_extended.tesla.battery_fully_charged = battery_fully_charged;
  //0x3D2
  datalayer_extended.tesla.battery_total_discharge = battery_total_discharge;
  datalayer_extended.tesla.battery_total_charge = battery_total_charge;
  //0x392
  datalayer_extended.tesla.battery_moduleType = battery_moduleType;
  datalayer_extended.tesla.battery_packMass = battery_packMass;
  datalayer_extended.tesla.battery_platformMaxBusVoltage = battery_platformMaxBusVoltage;
  //0x2D2
  datalayer_extended.tesla.battery_bms_min_voltage = battery_bms_min_voltage;
  datalayer_extended.tesla.battery_bms_max_voltage = battery_bms_max_voltage;
  datalayer_extended.tesla.battery_max_charge_current = battery_max_charge_current;
  datalayer_extended.tesla.battery_max_discharge_current = battery_max_discharge_current;
  //0x292
  datalayer_extended.tesla.battery_beginning_of_life = battery_beginning_of_life;
  datalayer_extended.tesla.battery_battTempPct = battery_battTempPct;
  datalayer_extended.tesla.battery_soc_ave = battery_soc_ave;
  datalayer_extended.tesla.battery_soc_max = battery_soc_max;
  datalayer_extended.tesla.battery_soc_min = battery_soc_min;
  datalayer_extended.tesla.battery_soc_ui = battery_soc_ui;
  //0x332
  datalayer_extended.tesla.battery_BrickVoltageMax = battery_BrickVoltageMax;
  datalayer_extended.tesla.battery_BrickVoltageMin = battery_BrickVoltageMin;
  datalayer_extended.tesla.battery_BrickTempMaxNum = battery_BrickTempMaxNum;
  datalayer_extended.tesla.battery_BrickTempMinNum = battery_BrickTempMinNum;
  datalayer_extended.tesla.battery_BrickModelTMax = battery_BrickModelTMax;
  datalayer_extended.tesla.battery_BrickModelTMin = battery_BrickModelTMin;
  //0x212
  datalayer_extended.tesla.battery_BMS_isolationResistance = battery_BMS_isolationResistance;
  datalayer_extended.tesla.battery_BMS_contactorState = battery_BMS_contactorState;
  datalayer_extended.tesla.battery_BMS_state = battery_BMS_state;
  datalayer_extended.tesla.battery_BMS_hvState = battery_BMS_hvState;
  datalayer_extended.tesla.battery_BMS_uiChargeStatus = battery_BMS_uiChargeStatus;
  datalayer_extended.tesla.battery_BMS_diLimpRequest = battery_BMS_diLimpRequest;
  datalayer_extended.tesla.battery_BMS_chgPowerAvailable = battery_BMS_chgPowerAvailable;
  datalayer_extended.tesla.battery_BMS_pcsPwmEnabled = battery_BMS_pcsPwmEnabled;
  //0x224
  datalayer_extended.tesla.battery_PCS_dcdcPrechargeStatus = battery_PCS_dcdcPrechargeStatus;
  datalayer_extended.tesla.battery_PCS_dcdc12VSupportStatus = battery_PCS_dcdc12VSupportStatus;
  datalayer_extended.tesla.battery_PCS_dcdcHvBusDischargeStatus = battery_PCS_dcdcHvBusDischargeStatus;
  datalayer_extended.tesla.battery_PCS_dcdcMainState = battery_PCS_dcdcMainState;
  datalayer_extended.tesla.battery_PCS_dcdcSubState = battery_PCS_dcdcSubState;
  datalayer_extended.tesla.battery_PCS_dcdcFaulted = battery_PCS_dcdcFaulted;
  datalayer_extended.tesla.battery_PCS_dcdcOutputIsLimited = battery_PCS_dcdcOutputIsLimited;
  datalayer_extended.tesla.battery_PCS_dcdcMaxOutputCurrentAllowed = battery_PCS_dcdcMaxOutputCurrentAllowed;
  datalayer_extended.tesla.battery_PCS_dcdcPrechargeRtyCnt = battery_PCS_dcdcPrechargeRtyCnt;
  datalayer_extended.tesla.battery_PCS_dcdc12VSupportRtyCnt = battery_PCS_dcdc12VSupportRtyCnt;
  datalayer_extended.tesla.battery_PCS_dcdcDischargeRtyCnt = battery_PCS_dcdcDischargeRtyCnt;
  datalayer_extended.tesla.battery_PCS_dcdcPwmEnableLine = battery_PCS_dcdcPwmEnableLine;
  datalayer_extended.tesla.battery_PCS_dcdcSupportingFixedLvTarget = battery_PCS_dcdcSupportingFixedLvTarget;
  datalayer_extended.tesla.battery_PCS_dcdcPrechargeRestartCnt = battery_PCS_dcdcPrechargeRestartCnt;
  datalayer_extended.tesla.battery_PCS_dcdcInitialPrechargeSubState = battery_PCS_dcdcInitialPrechargeSubState;
  //0x252
  datalayer_extended.tesla.BMS_maxRegenPower = BMS_maxRegenPower;
  datalayer_extended.tesla.BMS_maxDischargePower = BMS_maxDischargePower;
  datalayer_extended.tesla.BMS_maxStationaryHeatPower = BMS_maxStationaryHeatPower;
  datalayer_extended.tesla.BMS_hvacPowerBudget = BMS_hvacPowerBudget;
  datalayer_extended.tesla.BMS_notEnoughPowerForHeatPump = BMS_notEnoughPowerForHeatPump;
  datalayer_extended.tesla.BMS_powerLimitState = BMS_powerLimitState;
  datalayer_extended.tesla.BMS_inverterTQF = BMS_inverterTQF;
  //0x312
  datalayer_extended.tesla.BMS_powerDissipation = BMS_powerDissipation;
  datalayer_extended.tesla.BMS_flowRequest = BMS_flowRequest;
  datalayer_extended.tesla.BMS_inletActiveCoolTargetT = BMS_inletActiveCoolTargetT;
  datalayer_extended.tesla.BMS_inletPassiveTargetT = BMS_inletPassiveTargetT;
  datalayer_extended.tesla.BMS_inletActiveHeatTargetT = BMS_inletActiveHeatTargetT;
  datalayer_extended.tesla.BMS_packTMin = BMS_packTMin;
  datalayer_extended.tesla.BMS_packTMax = BMS_packTMax;
  datalayer_extended.tesla.BMS_pcsNoFlowRequest = BMS_pcsNoFlowRequest;
  datalayer_extended.tesla.BMS_noFlowRequest = BMS_noFlowRequest;
  //0x2A4
  datalayer_extended.tesla.PCS_dcdcTemp = PCS_dcdcTemp;
  datalayer_extended.tesla.PCS_ambientTemp = PCS_ambientTemp;
  datalayer_extended.tesla.PCS_chgPhATemp = PCS_chgPhATemp;
  datalayer_extended.tesla.PCS_chgPhBTemp = PCS_chgPhBTemp;
  datalayer_extended.tesla.PCS_chgPhCTemp = PCS_chgPhCTemp;
  //0x2C4
  datalayer_extended.tesla.PCS_dcdcMaxLvOutputCurrent = PCS_dcdcMaxLvOutputCurrent;
  datalayer_extended.tesla.PCS_dcdcCurrentLimit = PCS_dcdcCurrentLimit;
  datalayer_extended.tesla.PCS_dcdcLvOutputCurrentTempLimit = PCS_dcdcLvOutputCurrentTempLimit;
  datalayer_extended.tesla.PCS_dcdcUnifiedCommand = PCS_dcdcUnifiedCommand;
  datalayer_extended.tesla.PCS_dcdcCLAControllerOutput = PCS_dcdcCLAControllerOutput;
  datalayer_extended.tesla.PCS_dcdcTankVoltage = PCS_dcdcTankVoltage;
  datalayer_extended.tesla.PCS_dcdcTankVoltageTarget = PCS_dcdcTankVoltageTarget;
  datalayer_extended.tesla.PCS_dcdcClaCurrentFreq = PCS_dcdcClaCurrentFreq;
  datalayer_extended.tesla.PCS_dcdcTCommMeasured = PCS_dcdcTCommMeasured;
  datalayer_extended.tesla.PCS_dcdcShortTimeUs = PCS_dcdcShortTimeUs;
  datalayer_extended.tesla.PCS_dcdcHalfPeriodUs = PCS_dcdcHalfPeriodUs;
  datalayer_extended.tesla.PCS_dcdcIntervalMaxFrequency = PCS_dcdcIntervalMaxFrequency;
  datalayer_extended.tesla.PCS_dcdcIntervalMaxHvBusVolt = PCS_dcdcIntervalMaxHvBusVolt;
  datalayer_extended.tesla.PCS_dcdcIntervalMaxLvBusVolt = PCS_dcdcIntervalMaxLvBusVolt;
  datalayer_extended.tesla.PCS_dcdcIntervalMaxLvOutputCurr = PCS_dcdcIntervalMaxLvOutputCurr;
  datalayer_extended.tesla.PCS_dcdcIntervalMinFrequency = PCS_dcdcIntervalMinFrequency;
  datalayer_extended.tesla.PCS_dcdcIntervalMinHvBusVolt = PCS_dcdcIntervalMinHvBusVolt;
  datalayer_extended.tesla.PCS_dcdcIntervalMinLvBusVolt = PCS_dcdcIntervalMinLvBusVolt;
  datalayer_extended.tesla.PCS_dcdcIntervalMinLvOutputCurr = PCS_dcdcIntervalMinLvOutputCurr;
  datalayer_extended.tesla.PCS_dcdc12vSupportLifetimekWh = PCS_dcdc12vSupportLifetimekWh;
  //0x7AA
  datalayer_extended.tesla.HVP_gpioPassivePyroDepl = HVP_gpioPassivePyroDepl;
  datalayer_extended.tesla.HVP_gpioPyroIsoEn = HVP_gpioPyroIsoEn;
  datalayer_extended.tesla.HVP_gpioCpFaultIn = HVP_gpioCpFaultIn;
  datalayer_extended.tesla.HVP_gpioPackContPowerEn = HVP_gpioPackContPowerEn;
  datalayer_extended.tesla.HVP_gpioHvCablesOk = HVP_gpioHvCablesOk;
  datalayer_extended.tesla.HVP_gpioHvpSelfEnable = HVP_gpioHvpSelfEnable;
  datalayer_extended.tesla.HVP_gpioLed = HVP_gpioLed;
  datalayer_extended.tesla.HVP_gpioCrashSignal = HVP_gpioCrashSignal;
  datalayer_extended.tesla.HVP_gpioShuntDataReady = HVP_gpioShuntDataReady;
  datalayer_extended.tesla.HVP_gpioFcContPosAux = HVP_gpioFcContPosAux;
  datalayer_extended.tesla.HVP_gpioFcContNegAux = HVP_gpioFcContNegAux;
  datalayer_extended.tesla.HVP_gpioBmsEout = HVP_gpioBmsEout;
  datalayer_extended.tesla.HVP_gpioCpFaultOut = HVP_gpioCpFaultOut;
  datalayer_extended.tesla.HVP_gpioPyroPor = HVP_gpioPyroPor;
  datalayer_extended.tesla.HVP_gpioShuntEn = HVP_gpioShuntEn;
  datalayer_extended.tesla.HVP_gpioHvpVerEn = HVP_gpioHvpVerEn;
  datalayer_extended.tesla.HVP_gpioPackCoontPosFlywheel = HVP_gpioPackCoontPosFlywheel;
  datalayer_extended.tesla.HVP_gpioCpLatchEnable = HVP_gpioCpLatchEnable;
  datalayer_extended.tesla.HVP_gpioPcsEnable = HVP_gpioPcsEnable;
  datalayer_extended.tesla.HVP_gpioPcsDcdcPwmEnable = HVP_gpioPcsDcdcPwmEnable;
  datalayer_extended.tesla.HVP_gpioPcsChargePwmEnable = HVP_gpioPcsChargePwmEnable;
  datalayer_extended.tesla.HVP_gpioFcContPowerEnable = HVP_gpioFcContPowerEnable;
  datalayer_extended.tesla.HVP_gpioHvilEnable = HVP_gpioHvilEnable;
  datalayer_extended.tesla.HVP_gpioSecDrdy = HVP_gpioSecDrdy;
  datalayer_extended.tesla.HVP_hvp1v5Ref = HVP_hvp1v5Ref;
  datalayer_extended.tesla.HVP_shuntCurrentDebug = HVP_shuntCurrentDebug;
  datalayer_extended.tesla.HVP_packCurrentMia = HVP_packCurrentMia;
  datalayer_extended.tesla.HVP_auxCurrentMia = HVP_auxCurrentMia;
  datalayer_extended.tesla.HVP_currentSenseMia = HVP_currentSenseMia;
  datalayer_extended.tesla.HVP_shuntRefVoltageMismatch = HVP_shuntRefVoltageMismatch;
  datalayer_extended.tesla.HVP_shuntThermistorMia = HVP_shuntThermistorMia;
  datalayer_extended.tesla.HVP_shuntHwMia = HVP_shuntHwMia;
  datalayer_extended.tesla.HVP_dcLinkVoltage = HVP_dcLinkVoltage;
  datalayer_extended.tesla.HVP_packVoltage = HVP_packVoltage;
  datalayer_extended.tesla.HVP_fcLinkVoltage = HVP_fcLinkVoltage;
  datalayer_extended.tesla.HVP_packContVoltage = HVP_packContVoltage;
  datalayer_extended.tesla.HVP_packNegativeV = HVP_packNegativeV;
  datalayer_extended.tesla.HVP_packPositiveV = HVP_packPositiveV;
  datalayer_extended.tesla.HVP_pyroAnalog = HVP_pyroAnalog;
  datalayer_extended.tesla.HVP_dcLinkNegativeV = HVP_dcLinkNegativeV;
  datalayer_extended.tesla.HVP_dcLinkPositiveV = HVP_dcLinkPositiveV;
  datalayer_extended.tesla.HVP_fcLinkNegativeV = HVP_fcLinkNegativeV;
  datalayer_extended.tesla.HVP_fcContCoilCurrent = HVP_fcContCoilCurrent;
  datalayer_extended.tesla.HVP_fcContVoltage = HVP_fcContVoltage;
  datalayer_extended.tesla.HVP_hvilInVoltage = HVP_hvilInVoltage;
  datalayer_extended.tesla.HVP_hvilOutVoltage = HVP_hvilOutVoltage;
  datalayer_extended.tesla.HVP_fcLinkPositiveV = HVP_fcLinkPositiveV;
  datalayer_extended.tesla.HVP_packContCoilCurrent = HVP_packContCoilCurrent;
  datalayer_extended.tesla.HVP_battery12V = HVP_battery12V;
  datalayer_extended.tesla.HVP_shuntRefVoltageDbg = HVP_shuntRefVoltageDbg;
  datalayer_extended.tesla.HVP_shuntAuxCurrentDbg = HVP_shuntAuxCurrentDbg;
  datalayer_extended.tesla.HVP_shuntBarTempDbg = HVP_shuntBarTempDbg;
  datalayer_extended.tesla.HVP_shuntAsicTempDbg = HVP_shuntAsicTempDbg;
  datalayer_extended.tesla.HVP_shuntAuxCurrentStatus = HVP_shuntAuxCurrentStatus;
  datalayer_extended.tesla.HVP_shuntBarTempStatus = HVP_shuntBarTempStatus;
  datalayer_extended.tesla.HVP_shuntAsicTempStatus = HVP_shuntAsicTempStatus;

#ifdef DEBUG_LOG

  printFaultCodesIfActive();

  logging.print(getContactorText(battery_contactor));  // Display what state the contactor is in
  logging.print(", HVIL: ");
  logging.print(getHvilStatusState(battery_hvil_status));
  logging.print(", NegativeState: ");
  logging.print(getContactorState(battery_packContNegativeState));
  logging.print(", PositiveState: ");
  logging.print(getContactorState(battery_packContPositiveState));
  logging.print(", setState: ");
  logging.print(getContactorState(battery_packContactorSetState));
  logging.print(", close allowed: ");
  logging.print(getNoYes(battery_packCtrsClosingAllowed));
  logging.print(", Pyrotest: ");
  logging.println(getNoYes(battery_pyroTestInProgress));

  logging.print("Battery values: ");
  logging.print("Real SOC: ");
  logging.print(battery_soc_ui / 10.0, 1);
  print_int_with_units(", Battery voltage: ", battery_volts, "V");
  print_int_with_units(", Battery HV current: ", (battery_amps * 0.1), "A");
  logging.print(", Fully charged?: ");
  if (battery_full_charge_complete)
    logging.print("YES, ");
  else
    logging.print("NO, ");
  if (datalayer.battery.info.chemistry == battery_chemistry_enum::LFP) {
    logging.print("LFP chemistry detected!");
  }
  logging.println("");
  logging.print("Cellstats, Max: ");
  logging.print(battery_cell_max_v);
  logging.print("mV (cell ");
  logging.print(battery_BrickVoltageMaxNum);
  logging.print("), Min: ");
  logging.print(battery_cell_min_v);
  logging.print("mV (cell ");
  logging.print(battery_BrickVoltageMinNum);
  logging.print("), Imbalance: ");
  logging.print(battery_cell_deviation_mV);
  logging.println("mV.");

  logging.printf("High Voltage Output Pins: %.2f V, Low Voltage: %.2f V, DC/DC 12V current: %.2f A.\n",
                 (battery_dcdcHvBusVolt * 0.146484), (battery_dcdcLvBusVolt * 0.0390625),
                 (battery_dcdcLvOutputCurrent * 0.1));

  logging.printf("PCS_ambientTemp: %.2f°C, DCDC_Temp: %.2f°C, ChgPhA: %.2f°C, ChgPhB: %.2f°C, ChgPhC: %.2f°C.\n",
                 PCS_ambientTemp * 0.1 + 40, PCS_dcdcTemp * 0.1 + 40, PCS_chgPhATemp * 0.1 + 40,
                 PCS_chgPhBTemp * 0.1 + 40, PCS_chgPhCTemp * 0.1 + 40);

  logging.println("Values passed to the inverter: ");
  print_SOC(" SOC: ", datalayer.battery.status.reported_soc);
  print_int_with_units(" Max discharge power: ", datalayer.battery.status.max_discharge_power_W, "W");
  logging.print(", ");
  print_int_with_units(" Max charge power: ", datalayer.battery.status.max_charge_power_W, "W");
  logging.println("");
  print_int_with_units(" Max temperature: ", ((int16_t)datalayer.battery.status.temperature_min_dC * 0.1), "°C");
  logging.print(", ");
  print_int_with_units(" Min temperature: ", ((int16_t)datalayer.battery.status.temperature_max_dC * 0.1), "°C");
  logging.println("");
#endif  //DEBUG_LOG
}

void handle_incoming_can_frame_battery(CAN_frame rx_frame) {
  static uint8_t mux = 0;
  static uint16_t temp = 0;
  static bool mux0_read = false;
  static bool mux1_read = false;
  static uint16_t volts;
  static uint8_t mux_zero_counter = 0u;
  static uint8_t mux_max = 0u;

  switch (rx_frame.ID) {
    case 0x352:                              // 850 BMS_energyStatus newer BMS
      mux = ((rx_frame.data.u8[0]) & 0x03);  //BMS_energyStatusIndex M : 0|2@1+ (1,0) [0|0] ""  X
      if (mux == 0) {
        battery_nominal_full_pack_energy_m0 =
            (((rx_frame.data.u8[3]) << 8) |
             rx_frame.data.u8[2]);  //16|16@1+ (0.02,0) [0|0] "kWh"//to datalayer_extended
        battery_nominal_energy_remaining_m0 =
            (((rx_frame.data.u8[5]) << 8) |
             rx_frame.data.u8[4]);  //32|16@1+ (0.02,0) [0|0] "kWh"//to datalayer_extended
        battery_ideal_energy_remaining_m0 =
            (((rx_frame.data.u8[7]) << 8) |
             rx_frame.data.u8[6]);  //48|16@1+ (0.02,0) [0|0] "kWh"//to datalayer_extended
        mux0_read = true;           //Set flag to true
      }
      if (mux == 1) {
        battery_fully_charged = (rx_frame.data.u8[1] & 0x01);  //15|1@1+ (1,0) [0|1]//noYes
        battery_energy_buffer_m1 =
            ((rx_frame.data.u8[3] << 8) | rx_frame.data.u8[2]);  //16|16@1+ (0.01,0) [0|0] "kWh"//to datalayer_extended
        battery_expected_energy_remaining_m1 =
            ((rx_frame.data.u8[5] << 8) | rx_frame.data.u8[4]);  //32|16@1+ (0.02,0) [0|0] "kWh"//to datalayer_extended
        battery_energy_to_charge_complete_m1 =
            ((rx_frame.data.u8[7] << 8) | rx_frame.data.u8[6]);  //48|16@1+ (0.02,0) [0|0] "kWh"//to datalayer_extended
        mux1_read = true;                                        //Set flag to true
      }
      if (mux == 2) {
      }  // Additional information needed on this mux 2, example frame: 02 26 02 20 02 80 00 00 doesn't change
      if (mux0_read && mux1_read) {
        mux0_read = false;
        mux1_read = false;
        BMS352_mux = true;  //Set flag to true
      }
      // older BMS <2021 without mux
      battery_nominal_full_pack_energy =  //BMS_nominalFullPackEnergy : 0|11@1+ (0.1,0) [0|204.6] "KWh" //((_d[1] & (0x07U)) << 8) | (_d[0] & (0xFFU));
          (((rx_frame.data.u8[1] & 0x07) << 8) | (rx_frame.data.u8[0]));  //Example 752 (75.2kWh)
      battery_nominal_energy_remaining =  //BMS_nominalEnergyRemaining : 11|11@1+ (0.1,0) [0|204.6] "KWh" //((_d[2] & (0x3FU)) << 5) | ((_d[1] >> 3) & (0x1FU));
          (((rx_frame.data.u8[2] & 0x3F) << 5) | ((rx_frame.data.u8[1] & 0x1F) >> 3));  //Example 1247 * 0.1 = 124.7kWh
      battery_expected_energy_remaining =  //BMS_expectedEnergyRemaining : 22|11@1+ (0.1,0) [0|204.6] "KWh"// ((_d[4] & (0x01U)) << 10) | ((_d[3] & (0xFFU)) << 2) | ((_d[2] >> 6) & (0x03U));
          (((rx_frame.data.u8[4] & 0x01) << 10) | (rx_frame.data.u8[3] << 2) |
           ((rx_frame.data.u8[2] & 0x03) >> 6));  //Example 622 (62.2kWh)
      battery_ideal_energy_remaining =  //BMS_idealEnergyRemaining : 33|11@1+ (0.1,0) [0|204.6] "KWh" //((_d[5] & (0x0FU)) << 7) | ((_d[4] >> 1) & (0x7FU));
          (((rx_frame.data.u8[5] & 0x0F) << 7) | ((rx_frame.data.u8[4] & 0x7F) >> 1));  //Example 311 * 0.1 = 31.1kWh
      battery_energy_to_charge_complete =  // BMS_energyToChargeComplete : 44|11@1+ (0.1,0) [0|204.6] "KWh"// ((_d[6] & (0x7FU)) << 4) | ((_d[5] >> 4) & (0x0FU));
          (((rx_frame.data.u8[6] & 0x7F) << 4) | ((rx_frame.data.u8[5] & 0x0F) << 4));  //Example 147 * 0.1 = 14.7kWh
      battery_energy_buffer =  //BMS_energyBuffer : 55|8@1+ (0.1,0) [0|25.4] "KWh"// ((_d[7] & (0x7FU)) << 1) | ((_d[6] >> 7) & (0x01U));
          (((rx_frame.data.u8[7] & 0xFE) >> 1) | ((rx_frame.data.u8[6] & 0x80) >> 7));  //Example 1 * 0.1 = 0
      battery_full_charge_complete =  //BMS_fullChargeComplete : 63|1@1+ (1,0) [0|1] ""//((_d[7] >> 7) & (0x01U));
          ((rx_frame.data.u8[7] >> 7) & 0x01);  //noYes
      break;
    case 0x20A:  //522 HVP_contactorState:
      battery_packContNegativeState =
          (rx_frame.data.u8[0] & 0x07);  //(_d[0] & (0x07U)); 0|3@1+ (1,0) [0|7] //contactorState
      battery_packContPositiveState =
          (rx_frame.data.u8[0] & 0x38) >> 3;             //((_d[0] >> 3) & (0x07U)); 3|3@1+ (1,0) [0|7] //contactorState
      battery_contactor = (rx_frame.data.u8[1] & 0x0F);  // 8|4@1+ (1,0) [0|9] //contactorText
      battery_packContactorSetState =
          (rx_frame.data.u8[1] & 0x0F);  //(_d[1] & (0x0FU)); 8|4@1+ (1,0) [0|9] //contactorState
      battery_packCtrsClosingAllowed =
          (rx_frame.data.u8[4] & 0x08) >> 3;  //((_d[4] >> 3) & (0x01U)); 35|1@1+ (1,0) [0|1] //noYes
      battery_pyroTestInProgress =
          (rx_frame.data.u8[4] & 0x20) >> 5;  //((_d[4] >> 5) & (0x01U));//37|1@1+ (1,0) [0|1] //noYes
      battery_hvil_status =
          (rx_frame.data.u8[5] & 0x0F);  //(_d[5] & (0x0FU));   //40|4@1+ (1,0) [0|9] //hvilStatusState
      battery_packCtrsOpenNowRequested = ((rx_frame.data.u8[4] >> 1) & (0x01U));  //33|1@1+ (1,0) [0|1] //noYes
      battery_packCtrsOpenRequested = ((rx_frame.data.u8[4] >> 2) & (0x01U));     //34|1@1+ (1,0) [0|1] //noYes
      battery_packCtrsRequestStatus = ((rx_frame.data.u8[3] >> 6) & (0x03U));     //30|2@1+ (1,0) [0|2] //HVP_contactor
      battery_packCtrsResetRequestRequired = (rx_frame.data.u8[4] & (0x01U));     //32|1@1+ (1,0) [0|1] //noYes
      battery_dcLinkAllowedToEnergize = ((rx_frame.data.u8[4] >> 4) & (0x01U));   //36|1@1+ (1,0) [0|1] //noYes
      battery_fcContNegativeAuxOpen = ((rx_frame.data.u8[0] >> 7) & (0x01U));     //7|1@1+ (1,0) [0|1] ""  Receiver
      battery_fcContNegativeState = ((rx_frame.data.u8[1] >> 4) & (0x07U));       //12|3@1+ (1,0) [0|7] ""  Receiver
      battery_fcContPositiveAuxOpen = ((rx_frame.data.u8[0] >> 6) & (0x01U));     //6|1@1+ (1,0) [0|1] ""  Receiver
      battery_fcContPositiveState = (rx_frame.data.u8[2] & (0x07U));              //16|3@1+ (1,0) [0|7] ""  Receiver
      battery_fcContactorSetState = ((rx_frame.data.u8[2] >> 3) & (0x0FU));       //19|4@1+ (1,0) [0|9] ""  Receiver
      battery_fcCtrsClosingAllowed = ((rx_frame.data.u8[3] >> 5) & (0x01U));      //29|1@1+ (1,0) [0|1] ""  Receiver
      battery_fcCtrsOpenNowRequested = ((rx_frame.data.u8[3] >> 3) & (0x01U));    //27|1@1+ (1,0) [0|1] ""  Receiver
      battery_fcCtrsOpenRequested = ((rx_frame.data.u8[3] >> 4) & (0x01U));       //28|1@1+ (1,0) [0|1] ""  Receiver
      battery_fcCtrsRequestStatus = (rx_frame.data.u8[3] & (0x03U));              //24|2@1+ (1,0) [0|2] ""  Receiver
      battery_fcCtrsResetRequestRequired = ((rx_frame.data.u8[3] >> 2) & (0x01U));  //26|1@1+ (1,0) [0|1] ""  Receiver
      battery_fcLinkAllowedToEnergize = ((rx_frame.data.u8[5] >> 4) & (0x03U));     //44|2@1+ (1,0) [0|2] ""  Receiver
      break;
    case 0x212:  //530 BMS_status: 8
      battery_BMS_hvacPowerRequest = (rx_frame.data.u8[0] & (0x01U));
      battery_BMS_notEnoughPowerForDrive = ((rx_frame.data.u8[0] >> 1) & (0x01U));
      battery_BMS_notEnoughPowerForSupport = ((rx_frame.data.u8[0] >> 2) & (0x01U));
      battery_BMS_preconditionAllowed = ((rx_frame.data.u8[0] >> 3) & (0x01U));
      battery_BMS_updateAllowed = ((rx_frame.data.u8[0] >> 4) & (0x01U));
      battery_BMS_activeHeatingWorthwhile = ((rx_frame.data.u8[0] >> 5) & (0x01U));
      battery_BMS_cpMiaOnHvs = ((rx_frame.data.u8[0] >> 6) & (0x01U));
      battery_BMS_contactorState =
          (rx_frame.data.u8[1] &
           (0x07U));  //0 "SNA" 1 "OPEN" 2 "OPENING" 3 "CLOSING" 4 "CLOSED" 5 "WELDED" 6 "BLOCKED" ;
      battery_BMS_state =
          ((rx_frame.data.u8[1] >> 3) &
           (0x0FU));  //0 "STANDBY" 1 "DRIVE" 2 "SUPPORT" 3 "CHARGE" 4 "FEIM" 5 "CLEAR_FAULT" 6 "FAULT" 7 "WELD" 8 "TEST" 9 "SNA" ;
      battery_BMS_hvState =
          (rx_frame.data.u8[2] &
           (0x07U));  //0 "DOWN" 1 "COMING_UP" 2 "GOING_DOWN" 3 "UP_FOR_DRIVE" 4 "UP_FOR_CHARGE" 5 "UP_FOR_DC_CHARGE" 6 "UP" ;
      battery_BMS_isolationResistance =
          ((rx_frame.data.u8[3] & (0x1FU)) << 5) |
          ((rx_frame.data.u8[2] >> 3) & (0x1FU));  //19|10@1+ (10,0) [0|0] "kOhm"/to datalayer_extended
      battery_BMS_chargeRequest = ((rx_frame.data.u8[3] >> 5) & (0x01U));
      battery_BMS_keepWarmRequest = ((rx_frame.data.u8[3] >> 6) & (0x01U));
      battery_BMS_uiChargeStatus =
          (rx_frame.data.u8[4] &
           (0x07U));  // 0 "DISCONNECTED" 1 "NO_POWER" 2 "ABOUT_TO_CHARGE" 3 "CHARGING" 4 "CHARGE_COMPLETE" 5 "CHARGE_STOPPED" ;
      battery_BMS_diLimpRequest = ((rx_frame.data.u8[4] >> 3) & (0x01U));
      battery_BMS_okToShipByAir = ((rx_frame.data.u8[4] >> 4) & (0x01U));
      battery_BMS_okToShipByLand = ((rx_frame.data.u8[4] >> 5) & (0x01U));
      battery_BMS_chgPowerAvailable = ((rx_frame.data.u8[6] & (0x01U)) << 10) | ((rx_frame.data.u8[5] & (0xFFU)) << 2) |
                                      ((rx_frame.data.u8[4] >> 6) & (0x03U));  //38|11@1+ (0.125,0) [0|0] "kW"
      battery_BMS_chargeRetryCount = ((rx_frame.data.u8[6] >> 1) & (0x0FU));
      battery_BMS_pcsPwmEnabled = ((rx_frame.data.u8[6] >> 5) & (0x01U));
      battery_BMS_ecuLogUploadRequest = ((rx_frame.data.u8[6] >> 6) & (0x03U));
      battery_BMS_minPackTemperature = (rx_frame.data.u8[7] & (0xFFU));  //56|8@1+ (0.5,-40) [0|0] "DegC
      break;
    case 0x224:                                                                   //548 PCS_dcdcStatus:
      battery_PCS_dcdcPrechargeStatus = (rx_frame.data.u8[0] & (0x03U));          //0 "IDLE" 1 "ACTIVE" 2 "FAULTED" ;
      battery_PCS_dcdc12VSupportStatus = ((rx_frame.data.u8[0] >> 2) & (0x03U));  //0 "IDLE" 1 "ACTIVE" 2 "FAULTED"
      battery_PCS_dcdcHvBusDischargeStatus = ((rx_frame.data.u8[0] >> 4) & (0x03U));  //0 "IDLE" 1 "ACTIVE" 2 "FAULTED"
      battery_PCS_dcdcMainState =
          ((rx_frame.data.u8[1] & (0x03U)) << 2) |
          ((rx_frame.data.u8[0] >> 6) &
           (0x03U));  //0 "STANDBY" 1 "12V_SUPPORT_ACTIVE" 2 "PRECHARGE_STARTUP" 3 "PRECHARGE_ACTIVE" 4 "DIS_HVBUS_ACTIVE" 5 "SHUTDOWN" 6 "FAULTED" ;
      battery_PCS_dcdcSubState =
          ((rx_frame.data.u8[1] >> 2) &
           (0x1FU));  //0 "PWR_UP_INIT" 1 "STANDBY" 2 "12V_SUPPORT_ACTIVE" 3 "DIS_HVBUS" 4 "PCHG_FAST_DIS_HVBUS" 5 "PCHG_SLOW_DIS_HVBUS" 6 "PCHG_DWELL_CHARGE" 7 "PCHG_DWELL_WAIT" 8 "PCHG_DI_RECOVERY_WAIT" 9 "PCHG_ACTIVE" 10 "PCHG_FLT_FAST_DIS_HVBUS" 11 "SHUTDOWN" 12 "12V_SUPPORT_FAULTED" 13 "DIS_HVBUS_FAULTED" 14 "PCHG_FAULTED" 15 "CLEAR_FAULTS" 16 "FAULTED" 17 "NUM" ;
      battery_PCS_dcdcFaulted = ((rx_frame.data.u8[1] >> 7) & (0x01U));
      battery_PCS_dcdcOutputIsLimited = ((rx_frame.data.u8[3] >> 4) & (0x01U));
      battery_PCS_dcdcMaxOutputCurrentAllowed = ((rx_frame.data.u8[5] & (0x01U)) << 11) |
                                                ((rx_frame.data.u8[4] & (0xFFU)) << 3) |
                                                ((rx_frame.data.u8[3] >> 5) & (0x07U));  //29|12@1+ (0.1,0) [0|0] "A"
      battery_PCS_dcdcPrechargeRtyCnt = ((rx_frame.data.u8[5] >> 1) & (0x07U));          //Retry Count
      battery_PCS_dcdc12VSupportRtyCnt = ((rx_frame.data.u8[5] >> 4) & (0x0FU));         //Retry Count
      battery_PCS_dcdcDischargeRtyCnt = (rx_frame.data.u8[6] & (0x0FU));                 //Retry Count
      battery_PCS_dcdcPwmEnableLine = ((rx_frame.data.u8[6] >> 4) & (0x01U));
      battery_PCS_dcdcSupportingFixedLvTarget = ((rx_frame.data.u8[6] >> 5) & (0x01U));
      battery_PCS_ecuLogUploadRequest = ((rx_frame.data.u8[6] >> 6) & (0x03U));
      battery_PCS_dcdcPrechargeRestartCnt = (rx_frame.data.u8[7] & (0x07U));
      battery_PCS_dcdcInitialPrechargeSubState =
          ((rx_frame.data.u8[7] >> 3) &
           (0x1FU));  //0 "PWR_UP_INIT" 1 "STANDBY" 2 "12V_SUPPORT_ACTIVE" 3 "DIS_HVBUS" 4 "PCHG_FAST_DIS_HVBUS" 5 "PCHG_SLOW_DIS_HVBUS" 6 "PCHG_DWELL_CHARGE" 7 "PCHG_DWELL_WAIT" 8 "PCHG_DI_RECOVERY_WAIT" 9 "PCHG_ACTIVE" 10 "PCHG_FLT_FAST_DIS_HVBUS" 11 "SHUTDOWN" 12 "12V_SUPPORT_FAULTED" 13 "DIS_HVBUS_FAULTED" 14 "PCHG_FAULTED" 15 "CLEAR_FAULTS" 16 "FAULTED" 17 "NUM" ;
      break;
    case 0x252:  //Limit //594 BMS_powerAvailable:
      BMS_maxRegenPower = ((rx_frame.data.u8[1] << 8) |
                           rx_frame.data.u8[0]);  //0|16@1+ (0.01,0) [0|655.35] "kW"  //Example 4715 * 0.01 = 47.15kW
      BMS_maxDischargePower =
          ((rx_frame.data.u8[3] << 8) |
           rx_frame.data.u8[2]);  //16|16@1+ (0.013,0) [0|655.35] "kW"  //Example 2009 * 0.013 = 26.117???
      BMS_maxStationaryHeatPower =
          (((rx_frame.data.u8[5] & 0x03) << 8) |
           rx_frame.data.u8[4]);  //32|10@1+ (0.01,0) [0|10.23] "kW"  //Example 500 * 0.01 = 5kW
      BMS_hvacPowerBudget =
          (((rx_frame.data.u8[7] << 6) |
            ((rx_frame.data.u8[6] & 0xFC) >> 2)));  //50|10@1+ (0.02,0) [0|20.46] "kW"  //Example 1000 * 0.02 = 20kW?
      BMS_notEnoughPowerForHeatPump =
          ((rx_frame.data.u8[5] >> 2) & (0x01U));  //BMS_notEnoughPowerForHeatPump : 42|1@1+ (1,0) [0|1] ""  Receiver
      BMS_powerLimitState =
          (rx_frame.data.u8[6] &
           (0x01U));  //BMS_powerLimitsState : 48|1@1+ (1,0) [0|1] 0 "NOT_CALCULATED_FOR_DRIVE" 1 "CALCULATED_FOR_DRIVE"
      BMS_inverterTQF = ((rx_frame.data.u8[7] >> 4) & (0x03U));  //BMS_inverterTQF : 56|2@1+ (1,0) [0|3] ""  Receiver
      break;
    case 0x132:  //battery amps/volts //HVBattAmpVolt
      battery_volts = ((rx_frame.data.u8[1] << 8) | rx_frame.data.u8[0]) *
                      0.01;  //0|16@1+ (0.01,0) [0|655.35] "V"  //Example 37030mv * 0.01 = 370V
      battery_amps =
          ((rx_frame.data.u8[3] << 8) |
           rx_frame.data.u8
               [2]);  //SmoothBattCurrent : 16|16@1- (-0.1,0) [-3276.7|3276.7] "A"//Example 65492 (-4.3A) OR 225 (22.5A)
      battery_raw_amps =
          ((rx_frame.data.u8[5] << 8) | rx_frame.data.u8[4]) * -0.05 +
          822;  //RawBattCurrent : 32|16@1- (-0.05,822) [-1138.35|2138.4] "A"  //Example 10425 * -0.05 = ?
      battery_charge_time_remaining =
          (((rx_frame.data.u8[7] & 0x0F) << 8) |
           rx_frame.data.u8[6]);  //ChargeHoursRemaining : 48|12@1+ (1,0) [0|4095] "Min"  //Example 228 * 0.1 = 22.8min
      if (battery_charge_time_remaining == 4095) {
        battery_charge_time_remaining = 0;
      }
      break;
    case 0x3D2:  //TotalChargeDischarge:
      battery_total_discharge = ((rx_frame.data.u8[3] << 24) | (rx_frame.data.u8[2] << 16) |
                                 (rx_frame.data.u8[1] << 8) | rx_frame.data.u8[0]) *
                                0.001;  //0|32@1+ (0.001,0) [0|4294970] "kWh"
      battery_total_charge = ((rx_frame.data.u8[7] << 24) | (rx_frame.data.u8[6] << 16) | (rx_frame.data.u8[5] << 8) |
                              rx_frame.data.u8[4]) *
                             0.001;  //32|32@1+ (0.001,0) [0|4294970] "kWh"
      break;
    case 0x332:                            //min/max hist values //BattBrickMinMax:
      mux = (rx_frame.data.u8[0] & 0x03);  //BattBrickMultiplexer M : 0|2@1+ (1,0) [0|0] ""

      if (mux == 1)  //Cell voltages
      {
        temp = ((rx_frame.data.u8[1] << 6) | (rx_frame.data.u8[0] >> 2));
        temp = (temp & 0xFFF);
        battery_cell_max_v = temp * 2;
        temp = ((rx_frame.data.u8[3] << 8) | rx_frame.data.u8[2]);
        temp = (temp & 0xFFF);
        battery_cell_min_v = temp * 2;
        cellvoltagesRead = true;
        //BattBrickVoltageMax m1 : 2|12@1+ (0.002,0) [0|0] "V"  Receiver ((_d[1] & (0x3FU)) << 6) | ((_d[0] >> 2) & (0x3FU));
        battery_BrickVoltageMax =
            ((rx_frame.data.u8[1] & (0x3F)) << 6) | ((rx_frame.data.u8[0] >> 2) & (0x3F));  //to datalayer_extended
        //BattBrickVoltageMin m1 : 16|12@1+ (0.002,0) [0|0] "V"  Receiver ((_d[3] & (0x0FU)) << 8) | (_d[2] & (0xFFU));
        battery_BrickVoltageMin =
            ((rx_frame.data.u8[3] & (0x0F)) << 8) | (rx_frame.data.u8[2] & (0xFF));  //to datalayer_extended
        //BattBrickVoltageMaxNum m1 : 32|7@1+ (1,1) [0|0] ""  Receiver
        battery_BrickVoltageMaxNum =
            1 + (rx_frame.data.u8[4] & 0x7F);  //(_d[4] & (0x7FU)); //This cell has highest voltage
        //BattBrickVoltageMinNum m1 : 40|7@1+ (1,1) [0|0] ""  Receiver
        battery_BrickVoltageMinNum =
            1 + (rx_frame.data.u8[5] & 0x7F);  //(_d[5] & (0x7FU)); //This cell has lowest voltage
      }
      if (mux == 0)  //Temperature sensors
      {              //BattBrickTempMax m0 : 16|8@1+ (0.5,-40) [0|0] "C" (_d[2] & (0xFFU));
        battery_max_temp = (rx_frame.data.u8[2] * 5) - 400;  //Temperature values have 40.0*C offset, 0.5*C /bit
        //BattBrickTempMin m0 : 24|8@1+ (0.5,-40) [0|0] "C" (_d[3] & (0xFFU));
        battery_min_temp =
            (rx_frame.data.u8[3] * 5) - 400;  //Multiply by 5 and remove offset to get C+1 (0x61*5=485-400=8.5*C)
        //BattBrickTempMaxNum m0 : 2|4@1+ (1,0) [0|0] "" ((_d[0] >> 2) & (0x0FU));
        battery_BrickTempMaxNum = ((rx_frame.data.u8[0] >> 2) & (0x0F));  //to datalayer_extended
        //BattBrickTempMinNum m0 : 8|4@1+ (1,0) [0|0] "" (_d[1] & (0x0FU));
        battery_BrickTempMinNum = (rx_frame.data.u8[1] & (0x0F));  //to datalayer_extended
        //BattBrickModelTMax m0 : 32|8@1+ (0.5,-40) [0|0] "C" (_d[4] & (0xFFU));
        battery_BrickModelTMax = (rx_frame.data.u8[4] & (0xFFU));  //to datalayer_extended
        //BattBrickModelTMin m0 : 40|8@1+ (0.5,-40) [0|0] "C" (_d[5] & (0xFFU));
        battery_BrickModelTMin = (rx_frame.data.u8[5] & (0xFFU));  //to datalayer_extended
      }
      break;
    case 0x312:  // 786 BMS_thermalStatus
      BMS_powerDissipation =
          ((rx_frame.data.u8[1] & (0x03U)) << 8) | (rx_frame.data.u8[0] & (0xFFU));  //0|10@1+ (0.02,0) [0|0] "kW"
      BMS_flowRequest = ((rx_frame.data.u8[2] & (0x01U)) << 6) |
                        ((rx_frame.data.u8[1] >> 2) & (0x3FU));  //10|7@1+ (0.3,0) [0|0] "LPM"
      BMS_inletActiveCoolTargetT = ((rx_frame.data.u8[3] & (0x03U)) << 7) |
                                   ((rx_frame.data.u8[2] >> 1) & (0x7FU));  //17|9@1+ (0.25,-25) [0|0] "DegC"
      BMS_inletPassiveTargetT = ((rx_frame.data.u8[4] & (0x07U)) << 6) |
                                ((rx_frame.data.u8[3] >> 2) & (0x3FU));  //26|9@1+ (0.25,-25) [0|0] "DegC"  X
      BMS_inletActiveHeatTargetT = ((rx_frame.data.u8[5] & (0x0FU)) << 5) |
                                   ((rx_frame.data.u8[4] >> 3) & (0x1FU));  //35|9@1+ (0.25,-25) [0|0] "DegC"
      BMS_packTMin = ((rx_frame.data.u8[6] & (0x1FU)) << 4) |
                     ((rx_frame.data.u8[5] >> 4) & (0x0FU));  //44|9@1+ (0.25,-25) [-25|100] "DegC"
      BMS_packTMax = ((rx_frame.data.u8[7] & (0x3FU)) << 3) |
                     ((rx_frame.data.u8[6] >> 5) & (0x07U));          //53|9@1+ (0.25,-25) [-25|100] "DegC"
      BMS_pcsNoFlowRequest = ((rx_frame.data.u8[7] >> 6) & (0x01U));  // 62|1@1+ (1,0) [0|0] ""
      BMS_noFlowRequest = ((rx_frame.data.u8[7] >> 7) & (0x01U));     //63|1@1+ (1,0) [0|0] ""
      break;
    case 0x2A4:                                                                             //676 PCS_thermalStatus
      PCS_chgPhATemp = (rx_frame.data.u8[0] & 0xFF) | ((rx_frame.data.u8[1] & 0x07) << 8);  //0|11@1- (0.1,40) [0|0] "C"
      PCS_chgPhBTemp =
          ((rx_frame.data.u8[1] & 0xF8) >> 3) | ((rx_frame.data.u8[2] & 0x3F) << 5);  //11|11@1- (0.1,40) [0|0] "C"
      PCS_chgPhCTemp = ((rx_frame.data.u8[2] & 0xC0) >> 6) | (rx_frame.data.u8[3] << 2) |
                       ((rx_frame.data.u8[4] & 0x01) << 10);  //24|11@1- (0.1,40) [0|0] "C"
      PCS_dcdcTemp =
          ((rx_frame.data.u8[4] & 0xFE) >> 1) | ((rx_frame.data.u8[5] & 0x0F) << 7);       //35|11@1- (0.1,40) [0|0] "C"
      PCS_ambientTemp = ((rx_frame.data.u8[5] & 0xF0) >> 4) | (rx_frame.data.u8[6] << 4);  //48|11@1- (0.1,40) [0|0] "C"
      break;
    case 0x2C4:  // 708 PCS_logging:
      mux = (rx_frame.data.u8[0] & (0x1FU));
      PCS_logMessageSelect = (rx_frame.data.u8[0] & (0x1FU));  //0|5@1+ (1,0) [0|0] ""
      if (mux == 0) {
        PCS_chgPhAInputIrms = ((rx_frame.data.u8[1] & (0xFFU)) << 1) |
                              ((rx_frame.data.u8[0] >> 7) & (0x01U));  // m0 : 5|9@1+ (0.1,0) [0|0] "A"  X
        PCS_chgPhAIntBusV = ((rx_frame.data.u8[2] & (0xFFU)) << 1) |
                            ((rx_frame.data.u8[1] >> 7) & (0x01U));  // m0 : 14|9@1+ (1,0) [0|0] "V"  X
        PCS_chgPhAIntBusVTarget = ((rx_frame.data.u8[3] & (0xFFU)) << 1) |
                                  ((rx_frame.data.u8[2] >> 7) & (0x01U));  // m0 : 23|9@1+ (1,0) [0|0] "V"  X
        PCS_chgPhAOutputI = (rx_frame.data.u8[4] & (0xFFU));               // m0 : 32|8@1+ (0.1,0) [0|0] "A"  X
      }
      if (mux == 1) {
        PCS_chgPhBInputIrms = ((rx_frame.data.u8[1] & (0xFFU)) << 1) |
                              ((rx_frame.data.u8[0] >> 7) & (0x01U));  // m1 : 5|9@1+ (0.1,0) [0|0] "A"  X
        PCS_chgPhBIntBusV = ((rx_frame.data.u8[2] & (0xFFU)) << 1) |
                            ((rx_frame.data.u8[1] >> 7) & (0x01U));  // m1 : 14|9@1+ (1,0) [0|0] "V"  X
        PCS_chgPhBIntBusVTarget = ((rx_frame.data.u8[3] & (0xFFU)) << 1) |
                                  ((rx_frame.data.u8[2] >> 7) & (0x01U));  // m1 : 23|9@1+ (1,0) [0|0] "V"  X
        PCS_chgPhBOutputI = (rx_frame.data.u8[4] & (0xFFU));               // m1 : 32|8@1+ (0.1,0) [0|0] "A"  X
      }
      if (mux == 2) {
        PCS_chgPhCInputIrms = ((rx_frame.data.u8[1] & (0xFFU)) << 1) |
                              ((rx_frame.data.u8[0] >> 7) & (0x01U));  // m2 : 5|9@1+ (0.1,0) [0|0] "A"  X
        PCS_chgPhCIntBusV = ((rx_frame.data.u8[2] & (0xFFU)) << 1) |
                            ((rx_frame.data.u8[1] >> 7) & (0x01U));  // m2 : 14|9@1+ (1,0) [0|0] "V"  X
        PCS_chgPhCIntBusVTarget = ((rx_frame.data.u8[3] & (0xFFU)) << 1) |
                                  ((rx_frame.data.u8[2] >> 7) & (0x01U));  // m2 : 23|9@1+ (1,0) [0|0] "V"  X
        PCS_chgPhCOutputI = (rx_frame.data.u8[4] & (0xFFU));               // m2 : 32|8@1+ (0.1,0) [0|0] "A"  X
      }
      if (mux == 3) {
        PCS_chgInputL1NVrms = ((rx_frame.data.u8[1] & (0xFFU)) << 4) |
                              ((rx_frame.data.u8[0] >> 4) & (0x0FU));  // m3 : 5|12@1+ (0.2,0) [0|0] "V"  X
        PCS_chgInputL2NVrms = ((rx_frame.data.u8[2] & (0xFFU)) << 4) |
                              ((rx_frame.data.u8[1] >> 4) & (0x0FU));  // m3 : 17|12@1+ (0.2,0) [0|0] "V"  X
        PCS_chgInputL3NVrms = ((rx_frame.data.u8[3] & (0xFFU)) << 4) |
                              ((rx_frame.data.u8[2] >> 4) & (0x0FU));  // m3 : 29|12@1+ (0.2,0) [0|0] "V"  X
        PCS_chgInputL1L2Vrms = ((rx_frame.data.u8[4] & (0xFFU)) << 4) |
                               ((rx_frame.data.u8[3] >> 4) & (0x0FU));  // m3 : 41|12@1+ (0.2,0) [0|0] "V"  X
        PCS_chgInputNGVrms = ((rx_frame.data.u8[5] & (0xFFU)) << 1) |
                             ((rx_frame.data.u8[4] >> 7) & (0x01U));  // m3 : 53|9@1+ (1,0) [0|0] "V"  X
      }
      if (mux == 4) {
        PCS_chgInputFrequencyL1N = ((rx_frame.data.u8[1] & (0xFFU)) << 4) |
                                   ((rx_frame.data.u8[0] >> 4) & (0x0FU));  // m4 : 8|12@1+ (0.01,40) [0|0] "Hz"  X
        PCS_chgInputFrequencyL2N = ((rx_frame.data.u8[2] & (0xFFU)) << 4) |
                                   ((rx_frame.data.u8[1] >> 4) & (0x0FU));  // m4 : 20|12@1+ (0.01,40) [0|0] "Hz"  X
        PCS_chgInputFrequencyL3N = ((rx_frame.data.u8[3] & (0xFFU)) << 4) |
                                   ((rx_frame.data.u8[2] >> 4) & (0x0FU));  // m4 : 32|12@1+ (0.01,40) [0|0] "Hz"  X
        PCS_chgInternalPhaseConfig = ((rx_frame.data.u8[4] & (0x07U)));     // m4 : 44|3@1+ (1,0) [0|0] ""  X
        PCS_chgOutputV = ((rx_frame.data.u8[5] & (0xFFU)) << 4) |
                         ((rx_frame.data.u8[4] >> 4) & (0x0FU));  // m4 : 48|12@1+ (0.146484,0) [0|0] "V"  X
      }
      if (mux == 5) {
        PCS_chgPhAState = ((rx_frame.data.u8[0] & (0x1FU)) >> 1);               // m5 : 5|4@1+ (1,0) [0|0] ""  X
        PCS_chgPhBState = ((rx_frame.data.u8[1] & (0x1FU)) >> 1);               // m5 : 9|4@1+ (1,0) [0|0] ""  X
        PCS_chgPhCState = ((rx_frame.data.u8[2] & (0x1FU)) >> 1);               // m5 : 13|4@1+ (1,0) [0|0] ""  X
        PCS_chgPhALastShutdownReason = ((rx_frame.data.u8[3] & (0x1FU)) >> 1);  // m5 : 17|5@1+ (1,0) [0|0] ""  X
        PCS_chgPhBLastShutdownReason = ((rx_frame.data.u8[4] & (0x1FU)) >> 1);  // m5 : 22|5@1+ (1,0) [0|0] ""  X
        PCS_chgPhCLastShutdownReason = ((rx_frame.data.u8[5] & (0x1FU)) >> 1);  // m5 : 27|5@1+ (1,0) [0|0] ""  X
        PCS_chgPhARetryCount = ((rx_frame.data.u8[6] & (0x1FU)) >> 1);          // m5 : 32|3@1+ (1,0) [0|0] ""  X
        PCS_chgPhBRetryCount = ((rx_frame.data.u8[6] & (0x1FU)) >> 1);          // m5 : 35|3@1+ (1,0) [0|0] ""  X
        PCS_chgPhCRetryCount = ((rx_frame.data.u8[6] & (0x1FU)) >> 1);          // m5 : 38|3@1+ (1,0) [0|0] ""  X
        PCS_chgRetryCount = ((rx_frame.data.u8[6] & (0x1FU)) >> 1);             // m5 : 41|3@1+ (1,0) [0|0] ""  X
        PCS_chgPhManCurrentToDist = ((rx_frame.data.u8[7] & (0xFFU)) << 2) |
                                    ((rx_frame.data.u8[6] >> 6) & (0x03U));    // m5 : 44|10@1+ (0.1,0) [0|0] "A"  X
        PCS_chgL1NPllLocked = ((rx_frame.data.u8[7] & (0x01U)) >> 6);          // m5 : 54|1@1+ (1,0) [0|0] ""  X
        PCS_chgL2NPllLocked = ((rx_frame.data.u8[7] & (0x01U)) >> 6);          // m5 : 55|1@1+ (1,0) [0|0] ""  X
        PCS_chgL3NPllLocked = ((rx_frame.data.u8[7] & (0x01U)) >> 6);          // m5 : 56|1@1+ (1,0) [0|0] ""  X
        PCS_chgL1L2PllLocked = ((rx_frame.data.u8[7] & (0x01U)) >> 6);         // m5 : 57|1@1+ (1,0) [0|0] ""  X
        PCS_chgNgPllLocked = ((rx_frame.data.u8[7] & (0x01U)) >> 6);           // m5 : 58|1@1+ (1,0) [0|0] ""  X
        PCS_chgPhManOptimalPhsToUse = ((rx_frame.data.u8[7] & (0x01U)) >> 6);  // m5 : 59|2@1+ (1,0) [0|0] ""  X
        PCS_chg5VL1Enable = ((rx_frame.data.u8[7] & (0x01U)) >> 6);            // m5 : 61|1@1+ (1,0) [0|0] ""  X
      }
      if (mux == 6) {
        PCS_dcdcMaxLvOutputCurrent = ((rx_frame.data.u8[4] & (0xFFU)) << 4) |
                                     ((rx_frame.data.u8[3] >> 4) & (0x0FU));  //m6 : 28|12@1+ (0.1,0) [0|0] "A"  X
        PCS_dcdcCurrentLimit = ((rx_frame.data.u8[6] & (0x0FU)) << 8) |
                               (rx_frame.data.u8[5] & (0xFFU));  //m6 : 40|12@1+ (0.1,0) [0|0] "A"  X
        PCS_dcdcLvOutputCurrentTempLimit = ((rx_frame.data.u8[7] & (0xFFU)) << 4) |
                                           ((rx_frame.data.u8[6] >> 4) & (0x0FU));  //m6 : 52|12@1+ (0.1,0) [0|0] "A"  X
      }
      if (mux == 7) {
        PCS_dcdcUnifiedCommand = ((rx_frame.data.u8[1] & (0x7FU)) << 3) |
                                 ((rx_frame.data.u8[0] >> 5) & (0x07U));  //m7 : 5|10@1+ (0.001,0) [0|0] "1"  X
        PCS_dcdcCLAControllerOutput = ((rx_frame.data.u8[3] & (0x03U)) << 8) |
                                      (rx_frame.data.u8[2] & (0xFFU));  //m7 : 16|10@1+ (0.001,0) [0|0] "1"  X
        PCS_dcdcTankVoltage = ((rx_frame.data.u8[4] & (0x1FU)) << 6) |
                              ((rx_frame.data.u8[3] >> 2) & (0x3FU));  //m7 : 26|11@1- (1,0) [0|0] "V"  X
        PCS_dcdcTankVoltageTarget = ((rx_frame.data.u8[5] & (0x7FU)) << 3) |
                                    ((rx_frame.data.u8[4] >> 5) & (0x07U));  // m7 : 37|10@1+ (1,0) [0|0] "V"  X
        PCS_dcdcClaCurrentFreq = ((rx_frame.data.u8[7] & (0x0FU)) << 8) |
                                 (rx_frame.data.u8[6] & (0xFFU));  //P m7 : 48|12@1+ (0.0976563,0) [0|0] "kHz"  X
      }
      if (mux == 8) {
        PCS_dcdcTCommMeasured = ((rx_frame.data.u8[2] & (0xFFU)) << 8) |
                                (rx_frame.data.u8[1] & (0xFFU));  // m8 : 8|16@1- (0.00195313,0) [0|0] "us"  X
        PCS_dcdcShortTimeUs = ((rx_frame.data.u8[4] & (0xFFU)) << 8) |
                              (rx_frame.data.u8[3] & (0xFFU));  // m8 : 24|16@1+ (0.000488281,0) [0|0] "us"  X
        PCS_dcdcHalfPeriodUs = ((rx_frame.data.u8[6] & (0xFFU)) << 8) |
                               (rx_frame.data.u8[5] & (0xFFU));  // m8 : 40|16@1+ (0.000488281,0) [0|0] "us"  X
      }
      if (mux == 9) {
        PCS_cpu2BootState = (rx_frame.data.u8[0] >> 5) & 0x03;          // m9 : 5|2@1+ (1,0) [0|0] ""  X
        PCS_acChargeSelfTestState = (rx_frame.data.u8[0] >> 7) & 0x01;  // m9 : 7|1@1+ (1,0) [0|0] ""  X
        PCS_1V5Min10s = ((rx_frame.data.u8[2] & 0xFF) << 3) |
                        ((rx_frame.data.u8[1] >> 5) & 0x07);  // m9 : 8|11@1+ (0.001,0) [0|0] "V"  X
        PCS_1V5Max10s = ((rx_frame.data.u8[3] & 0xFF) << 3) |
                        ((rx_frame.data.u8[2] >> 5) & 0x07);  // m9 : 19|11@1+ (0.001,0) [0|0] "V"  X
        PCS_1V2Min10s = ((rx_frame.data.u8[5] & 0xFF) << 3) |
                        ((rx_frame.data.u8[4] >> 5) & 0x07);  // m9 : 32|11@1+ (0.001,0) [0|0] "V"  X
        PCS_1V2Max10s = ((rx_frame.data.u8[6] & 0xFF) << 3) |
                        ((rx_frame.data.u8[5] >> 5) & 0x07);  // m9 : 43|11@1+ (0.001,0) [0|0] "V"  X
        PCS_numAlertsSet = rx_frame.data.u8[7] & 0x7F;        // m9 : 56|7@1+ (1,0) [0|0] ""  X
      }
      if (mux == 10) {
        PCS_chgPhAIntBusVMin10s = ((rx_frame.data.u8[1] & 0xFF) << 1) |
                                  ((rx_frame.data.u8[0] >> 7) & 0x01);  // m10 : 5|9@1+ (1,0) [0|0] "V"  X
        PCS_chgPhAIntBusVMax10s = ((rx_frame.data.u8[2] & 0xFF) << 1) |
                                  ((rx_frame.data.u8[1] >> 7) & 0x01);  // m10 : 14|9@1+ (1,0) [0|0] "V"  X
        PCS_chgPhAPchgVoltDeltaMax10s = rx_frame.data.u8[3] & 0xFF;     // m10 : 23|8@1+ (0.5,0) [0|0] "V"  X
        PCS_chgPhALifetimekWh = ((rx_frame.data.u8[6] & 0xFF) << 16) | ((rx_frame.data.u8[5] & 0xFF) << 8) |
                                (rx_frame.data.u8[4] & 0xFF);  // m10 : 31|24@1+ (0.01,0) [0|0] "kWh"  X
        PCS_chgPhATransientRetryCount = ((rx_frame.data.u8[7] & 0xFF) << 1) |
                                        ((rx_frame.data.u8[6] >> 7) & 0x01);  // m10 : 55|9@1+ (0.1,0) [0|0] "-"  X
      }
      if (mux == 11) {
        PCS_chgPhBIntBusVMin10s = ((rx_frame.data.u8[1] & 0xFF) << 1) |
                                  ((rx_frame.data.u8[0] >> 7) & 0x01);  // m11 : 5|9@1+ (1,0) [0|0] "V"  X
        PCS_chgPhBIntBusVMax10s = ((rx_frame.data.u8[2] & 0xFF) << 1) |
                                  ((rx_frame.data.u8[1] >> 7) & 0x01);  // m11 : 14|9@1+ (1,0) [0|0] "V"  X
        PCS_chgPhBPchgVoltDeltaMax10s = rx_frame.data.u8[3] & 0xFF;     // m11 : 23|8@1+ (0.5,0) [0|0] "V"  X
        PCS_chgPhBLifetimekWh = ((rx_frame.data.u8[6] & 0xFF) << 16) | ((rx_frame.data.u8[5] & 0xFF) << 8) |
                                (rx_frame.data.u8[4] & 0xFF);  // m11 : 31|24@1+ (0.01,0) [0|0] "kWh"  X
        PCS_chgPhBTransientRetryCount = ((rx_frame.data.u8[7] & 0xFF) << 1) |
                                        ((rx_frame.data.u8[6] >> 7) & 0x01);  // m11 : 55|9@1+ (0.1,0) [0|0] "-"  X
      }
      if (mux == 12) {
        PCS_chgPhCIntBusVMin10s = ((rx_frame.data.u8[1] & 0xFF) << 1) |
                                  ((rx_frame.data.u8[0] >> 7) & 0x01);  // m12 : 5|9@1+ (1,0) [0|0] "V"  X
        PCS_chgPhCIntBusVMax10s = ((rx_frame.data.u8[2] & 0xFF) << 1) |
                                  ((rx_frame.data.u8[1] >> 7) & 0x01);  // m12 : 14|9@1+ (1,0) [0|0] "V"  X
        PCS_chgPhCPchgVoltDeltaMax10s = rx_frame.data.u8[3] & 0xFF;     // m12 : 23|8@1+ (0.5,0) [0|0] "V"  X
        PCS_chgPhCLifetimekWh = ((rx_frame.data.u8[6] & 0xFF) << 16) | ((rx_frame.data.u8[5] & 0xFF) << 8) |
                                (rx_frame.data.u8[4] & 0xFF);  // m12 : 31|24@1+ (0.01,0) [0|0] "kWh"  X
        PCS_chgPhCTransientRetryCount = ((rx_frame.data.u8[7] & 0xFF) << 1) |
                                        ((rx_frame.data.u8[6] >> 7) & 0x01);  // m12 : 55|9@1+ (0.1,0) [0|0] "-"  X
      }
      if (mux == 13) {
        PCS_chgInputL1NVPeak10s = ((rx_frame.data.u8[1] & 0xFF) << 1) |
                                  ((rx_frame.data.u8[0] >> 7) & 0x01);  // m13 : 5|9@1+ (1,0) [0|0] "V"  X
        PCS_chgInputL2NVPeak10s = ((rx_frame.data.u8[2] & 0xFF) << 1) |
                                  ((rx_frame.data.u8[1] >> 7) & 0x01);  // m13 : 14|9@1+ (1,0) [0|0] "V"  X
        PCS_chgInputL3NVPeak10s = ((rx_frame.data.u8[3] & 0xFF) << 1) |
                                  ((rx_frame.data.u8[2] >> 7) & 0x01);  // m13 : 23|9@1+ (1,0) [0|0] "V"  X
        PCS_chgInputFreqWobblePHAPeak = rx_frame.data.u8[4] & 0xFF;     // m13 : 32|8@1+ (0.01,0) [0|0] "Hz"  X
        PCS_chgInputFreqWobblePHBPeak = rx_frame.data.u8[5] & 0xFF;     // m13 : 40|8@1+ (0.01,0) [0|0] "Hz"  X
        PCS_chgInputFreqWobblePHCPeak = rx_frame.data.u8[6] & 0xFF;     // m13 : 48|8@1+ (0.01,0) [0|0] "Hz"  X
      }
      if (mux == 14) {
        PCS_dLogPhAChannel1Content = (rx_frame.data.u8[0] >> 5) & 0x03;  // m14 : 5|2@1+ (1,0) [0|0] ""  X
        PCS_dLogPhBChannel1Content = (rx_frame.data.u8[1] >> 0) & 0x03;  // m14 : 8|2@1+ (1,0) [0|0] ""  X
        PCS_dLogPhCChannel1Content = (rx_frame.data.u8[1] >> 2) & 0x03;  // m14 : 10|2@1+ (1,0) [0|0] ""  X
        PCS_dLogPhAChannel1Data =
            ((rx_frame.data.u8[3] & 0xFF) << 8) | (rx_frame.data.u8[2] & 0xFF);  // m14 : 16|16@1- (0.02,0) [0|0] "-"  X
        PCS_dLogPhBChannel1Data =
            ((rx_frame.data.u8[5] & 0xFF) << 8) | (rx_frame.data.u8[4] & 0xFF);  // m14 : 32|16@1- (0.02,0) [0|0] "-"  X
        PCS_dLogPhCChannel1Data =
            ((rx_frame.data.u8[7] & 0xFF) << 8) | (rx_frame.data.u8[6] & 0xFF);  // m14 : 48|16@1- (0.02,0) [0|0] "-"  X
      }
      if (mux == 15) {
        PCS_dLogPhAChannel2Content = (rx_frame.data.u8[0] >> 5) & 0x03;  // m15 : 5|2@1+ (1,0) [0|0] ""  X
        PCS_dLogPhBChannel2Content = (rx_frame.data.u8[1] >> 0) & 0x03;  // m15 : 8|2@1+ (1,0) [0|0] ""  X
        PCS_dLogPhCChannel2Content = (rx_frame.data.u8[1] >> 2) & 0x03;  // m15 : 10|2@1+ (1,0) [0|0] ""  X
        PCS_dLogPhAChannel2Data =
            ((rx_frame.data.u8[3] & 0xFF) << 8) | (rx_frame.data.u8[2] & 0xFF);  // m15 : 16|16@1- (0.02,0) [0|0] "-"  X
        PCS_dLogPhBChannel2Data =
            ((rx_frame.data.u8[5] & 0xFF) << 8) | (rx_frame.data.u8[4] & 0xFF);  // m15 : 32|16@1- (0.02,0) [0|0] "-"  X
        PCS_dLogPhCChannel2Data =
            ((rx_frame.data.u8[7] & 0xFF) << 8) | (rx_frame.data.u8[6] & 0xFF);  // m15 : 48|16@1- (0.02,0) [0|0] "-"  X
      }
      if (mux == 16) {
        PCS_dLogPhAChannel3Content = (rx_frame.data.u8[0] >> 5) & 0x03;  // m16 : 5|2@1+ (1,0) [0|0] ""  X
        PCS_dLogPhBChannel3Content = (rx_frame.data.u8[1] >> 0) & 0x03;  // m16 : 8|2@1+ (1,0) [0|0] ""  X
        PCS_dLogPhCChannel3Content = (rx_frame.data.u8[1] >> 2) & 0x03;  // m16 : 10|2@1+ (1,0) [0|0] ""  X
        PCS_dLogPhAChannel3Data =
            ((rx_frame.data.u8[3] & 0xFF) << 8) | (rx_frame.data.u8[2] & 0xFF);  // m16 : 16|16@1- (0.02,0) [0|0] "-"  X
        PCS_dLogPhBChannel3Data =
            ((rx_frame.data.u8[5] & 0xFF) << 8) | (rx_frame.data.u8[4] & 0xFF);  // m16 : 32|16@1- (0.02,0) [0|0] "-"  X
        PCS_dLogPhCChannel3Data =
            ((rx_frame.data.u8[7] & 0xFF) << 8) | (rx_frame.data.u8[6] & 0xFF);  // m16 : 48|16@1- (0.02,0) [0|0] "-"  X
      }
      if (mux == 17) {
        PCS_dLogPhAChannel4Content = (rx_frame.data.u8[0] >> 5) & 0x03;  // m17 : 5|2@1+ (1,0) [0|0] ""  X
        PCS_dLogPhBChannel4Content = (rx_frame.data.u8[1] >> 0) & 0x03;  // m17 : 8|2@1+ (1,0) [0|0] ""  X
        PCS_dLogPhCChannel4Content = (rx_frame.data.u8[1] >> 2) & 0x03;  // m17 : 10|2@1+ (1,0) [0|0] ""  X
        PCS_dLogPhAChannel4Data =
            ((rx_frame.data.u8[3] & 0xFF) << 8) | (rx_frame.data.u8[2] & 0xFF);  // m17 : 16|16@1- (0.02,0) [0|0] "-"  X
        PCS_dLogPhBChannel4Data =
            ((rx_frame.data.u8[5] & 0xFF) << 8) | (rx_frame.data.u8[4] & 0xFF);  // m17 : 32|16@1- (0.02,0) [0|0] "-"  X
        PCS_dLogPhCChannel4Data =
            ((rx_frame.data.u8[7] & 0xFF) << 8) | (rx_frame.data.u8[6] & 0xFF);  // m17 : 48|16@1- (0.02,0) [0|0] "-"  X
      }
      if (mux == 18) {
        PCS_dcdcIntervalMaxFrequency = ((rx_frame.data.u8[2] & (0x0FU)) << 8) |
                                       (rx_frame.data.u8[1] & (0xFFU));  // m18 : 8|12@1+ (1,0) [0|0] "kHz"  X
        PCS_dcdcIntervalMaxHvBusVolt = ((rx_frame.data.u8[4] & (0x1FU)) << 8) |
                                       (rx_frame.data.u8[3] & (0xFFU));  //m18 : 24|13@1+ (0.1,0) [0|0] "V"  X
        PCS_dcdcIntervalMaxLvBusVolt = ((rx_frame.data.u8[5] & (0x3FU)) << 3) |
                                       ((rx_frame.data.u8[4] >> 5) & (0x07U));  // m18 : 37|9@1+ (0.1,0) [0|0] "V"  X
        PCS_dcdcIntervalMaxLvOutputCurr = ((rx_frame.data.u8[7] & (0x0FU)) << 8) |
                                          (rx_frame.data.u8[6] & (0xFFU));  //m18 : 48|12@1+ (1,0) [0|0] "A"  X
      }
      if (mux == 19) {
        PCS_dcdcIntervalMinFrequency = ((rx_frame.data.u8[2] & (0x0FU)) << 8) |
                                       (rx_frame.data.u8[1] & (0xFFU));  //m19 : 8|12@1+ (1,0) [0|0] "kHz"  X
        PCS_dcdcIntervalMinHvBusVolt = ((rx_frame.data.u8[4] & (0x1FU)) << 8) |
                                       (rx_frame.data.u8[3] & (0xFFU));  //m19 : 24|13@1+ (0.1,0) [0|0] "V"  X
        PCS_dcdcIntervalMinLvBusVolt = ((rx_frame.data.u8[5] & (0x3FU)) << 3) |
                                       ((rx_frame.data.u8[4] >> 5) & (0x07U));  //m19 : 37|9@1+ (0.1,0) [0|0] "V"  X
        PCS_dcdcIntervalMinLvOutputCurr = ((rx_frame.data.u8[7] & (0x0FU)) << 8) |
                                          (rx_frame.data.u8[6] & (0xFFU));  // m19 : 48|12@1+ (1,0) [0|0] "A"  X
      }
      if (mux == 20) {
        PCS_chgPhANoFlowBucket = ((rx_frame.data.u8[1] & 0xFF) << 1) |
                                 ((rx_frame.data.u8[0] >> 7) & 0x01);  // m20 : 5|9@1+ (0.01,0) [0|0] "-"  X
        PCS_chgPhBNoFlowBucket = ((rx_frame.data.u8[2] & 0xFF) << 1) |
                                 ((rx_frame.data.u8[1] >> 7) & 0x01);  // m20 : 14|9@1+ (0.01,0) [0|0] "-"  X
        PCS_chgPhCNoFlowBucket = ((rx_frame.data.u8[3] & 0xFF) << 1) |
                                 ((rx_frame.data.u8[2] >> 7) & 0x01);  // m20 : 23|9@1+ (0.01,0) [0|0] "-"  X
      }
      if (mux == 21) {
        PCS_chgInputL1NVdc = ((rx_frame.data.u8[1] & 0xFF) << 2) |
                             ((rx_frame.data.u8[0] >> 6) & 0x03);  // m21 : 5|10@1- (1,0) [0|0] "V"  X
        PCS_chgInputL2NVdc = ((rx_frame.data.u8[2] & 0xFF) << 2) |
                             ((rx_frame.data.u8[1] >> 6) & 0x03);  // m21 : 16|10@1- (1,0) [0|0] "V"  X
        PCS_chgInputL3NVdc = ((rx_frame.data.u8[3] & 0xFF) << 2) |
                             ((rx_frame.data.u8[2] >> 6) & 0x03);  // m21 : 26|10@1- (1,0) [0|0] "V"  X
        PCS_chgInputL1L2Vdc = ((rx_frame.data.u8[4] & 0xFF) << 2) |
                              ((rx_frame.data.u8[3] >> 6) & 0x03);  // m21 : 36|10@1- (1,0) [0|0] "V"  X
        PCS_chgInputNGVdc = ((rx_frame.data.u8[5] & 0xFF) << 2) |
                            ((rx_frame.data.u8[4] >> 6) & 0x03);  // m21 : 46|10@1- (1,0) [0|0] "V"  X
      }
      if (mux == 22) {
        PCS_dcdc12vSupportLifetimekWh = ((rx_frame.data.u8[3] & (0xFFU)) << 16) |
                                        ((rx_frame.data.u8[2] & (0xFFU)) << 8) |
                                        (rx_frame.data.u8[1] & (0xFFU));  // m22 : 8|24@1+ (0.01,0) [0|0] "kWh"  X
      }
      break;
    case 0x401: {                   // Cell stats  //BrickVoltages
      mux = (rx_frame.data.u8[0]);  //MultiplexSelector M : 0|8@1+ (1,0) [0|0] ""
                                    //StatusFlags : 8|8@1+ (1,0) [0|0] ""
                                    //Brick0 m0 : 16|16@1+ (0.0001,0) [0|0] "V"
                                    //Brick1 m0 : 32|16@1+ (0.0001,0) [0|0] "V"
                                    //Brick2 m0 : 48|16@1+ (0.0001,0) [0|0] "V"

      if (rx_frame.data.u8[1] == 0x2A)  // status byte must be 0x2A to read cellvoltages
      {
        // Example, frame3=0x89,frame2=0x1D = 35101 / 10 = 3510mV
        volts = ((rx_frame.data.u8[3] << 8) | rx_frame.data.u8[2]) / 10;
        datalayer.battery.status.cell_voltages_mV[mux * 3] = volts;
        volts = ((rx_frame.data.u8[5] << 8) | rx_frame.data.u8[4]) / 10;
        datalayer.battery.status.cell_voltages_mV[1 + mux * 3] = volts;
        volts = ((rx_frame.data.u8[7] << 8) | rx_frame.data.u8[6]) / 10;
        datalayer.battery.status.cell_voltages_mV[2 + mux * 3] = volts;

        // Track the max value of mux. If we've seen two 0 values for mux, we've probably gathered all
        // cell voltages. Then, 2 + mux_max * 3 + 1 is the number of cell voltages.
        mux_max = (mux > mux_max) ? mux : mux_max;
        if (mux_zero_counter < 2 && mux == 0u) {
          mux_zero_counter++;
          if (mux_zero_counter == 2u) {
            // The max index will be 2 + mux_max * 3 (see above), so "+ 1" for the number of cells
            datalayer.battery.info.number_of_cells = 2 + 3 * mux_max + 1;
            // Increase the counter arbitrarily another time to make the initial if-statement evaluate to false
            mux_zero_counter++;
          }
        }
      }
      break;
    } break;
    case 0x2d2:  //BMSVAlimits:
      battery_bms_min_voltage =
          ((rx_frame.data.u8[1] << 8) |
           rx_frame.data.u8[0]);  //0|16@1+ (0.01,0) [0|430] "V"  //Example 24148mv * 0.01 = 241.48 V
      battery_bms_max_voltage =
          ((rx_frame.data.u8[3] << 8) |
           rx_frame.data.u8[2]);  //16|16@1+ (0.01,0) [0|430] "V"  //Example 40282mv * 0.01 = 402.82 V
      battery_max_charge_current = (((rx_frame.data.u8[5] & 0x3F) << 8) | rx_frame.data.u8[4]) *
                                   0.1;  //32|14@1+ (0.1,0) [0|1638.2] "A"  //Example 1301? * 0.1 = 130.1?
      battery_max_discharge_current = (((rx_frame.data.u8[7] & 0x3F) << 8) | rx_frame.data.u8[6]) *
                                      0.128;  //48|14@1+ (0.128,0) [0|2096.9] "A"  //Example 430? * 0.128 = 55.4?
      break;
    case 0x2b4:  //PCS_dcdcRailStatus:
      battery_dcdcLvBusVolt =
          (((rx_frame.data.u8[1] & 0x03) << 8) | rx_frame.data.u8[0]);  //0|10@1+ (0.0390625,0) [0|39.9609] "V"
      battery_dcdcHvBusVolt = (((rx_frame.data.u8[2] & 0x3F) << 6) |
                               ((rx_frame.data.u8[1] & 0xFC) >> 2));  //10|12@1+ (0.146484,0) [0|599.854] "V"
      battery_dcdcLvOutputCurrent =
          (((rx_frame.data.u8[4] & 0x0F) << 8) | rx_frame.data.u8[3]);  //24|12@1+ (0.1,0) [0|400] "A"
      break;
    case 0x292:                                                            //BMS_socStatus
      datalayer.battery.status.CAN_battery_still_alive = CAN_STILL_ALIVE;  //We are getting CAN messages from the BMS
      battery_beginning_of_life =
          (((rx_frame.data.u8[6] & 0x03) << 8) | rx_frame.data.u8[5]) * 0.1;          //40|10@1+ (0.1,0) [0|102.3] "kWh"
      battery_soc_min = (((rx_frame.data.u8[1] & 0x03) << 8) | rx_frame.data.u8[0]);  //0|10@1+ (0.1,0) [0|102.3] "%"
      battery_soc_ui =
          (((rx_frame.data.u8[2] & 0x0F) << 6) | ((rx_frame.data.u8[1] & 0xFC) >> 2));  //10|10@1+ (0.1,0) [0|102.3] "%"
      battery_soc_max =
          (((rx_frame.data.u8[3] & 0x3F) << 4) | ((rx_frame.data.u8[2] & 0xF0) >> 4));  //20|10@1+ (0.1,0) [0|102.3] "%"
      battery_soc_ave =
          ((rx_frame.data.u8[4] << 2) | ((rx_frame.data.u8[3] & 0xC0) >> 6));  //30|10@1+ (0.1,0) [0|102.3] "%"
      battery_battTempPct =
          (((rx_frame.data.u8[7] & 0x03) << 6) | (rx_frame.data.u8[6] & 0x3F) >> 2);  //50|8@1+ (0.4,0) [0|100] "%"
      break;
    case 0x392:  //BMS_packConfig
      mux = (rx_frame.data.u8[0] & (0xFF));
      if (mux == 1) {
        battery_packConfigMultiplexer = (rx_frame.data.u8[0] & (0xff));  //0|8@1+ (1,0) [0|1] ""
        battery_moduleType =
            (rx_frame.data.u8[1] &
             (0x07));  //8|3@1+ (1,0) [0|4] ""//0 "UNKNOWN" 1 "E3_NCT" 2 "E1_NCT" 3 "E3_CT" 4 "E1_CT" 5 "E1_CP" ;//to datalayer_extended
        battery_packMass = (rx_frame.data.u8[2]) + 300;  //16|8@1+ (1,300) [342|469] "kg"
        battery_platformMaxBusVoltage =
            (((rx_frame.data.u8[4] & 0x03) << 8) | (rx_frame.data.u8[3]));  //24|10@1+ (0.1,375) [0|0] "V"
      }
      if (mux == 0) {
        battery_reservedConfig =
            (rx_frame.data.u8[1] &
             (0x1F));  //8|5@1+ (1,0) [0|31] ""//0 "BMS_CONFIG_0" 1 "BMS_CONFIG_1" 10 "BMS_CONFIG_10" 11 "BMS_CONFIG_11" 12 "BMS_CONFIG_12" 13 "BMS_CONFIG_13" 14 "BMS_CONFIG_14" 15 "BMS_CONFIG_15" 16 "BMS_CONFIG_16" 17 "BMS_CONFIG_17" 18 "BMS_CONFIG_18" 19 "BMS_CONFIG_19" 2 "BMS_CONFIG_2" 20 "BMS_CONFIG_20" 21 "BMS_CONFIG_21" 22 "BMS_CONFIG_22" 23 "BMS_CONFIG_23" 24 "BMS_CONFIG_24" 25 "BMS_CONFIG_25" 26 "BMS_CONFIG_26" 27 "BMS_CONFIG_27" 28 "BMS_CONFIG_28" 29 "BMS_CONFIG_29" 3 "BMS_CONFIG_3" 30 "BMS_CONFIG_30" 31 "BMS_CONFIG_31" 4 "BMS_CONFIG_4" 5 "BMS_CONFIG_5" 6 "BMS_CONFIG_6" 7 "BMS_CONFIG_7" 8 "BMS_CONFIG_8" 9 "BMS_CONFIG_9" ;
      }
      break;
    case 0x7AA:  //1962 HVP_debugMessage:
      mux = (rx_frame.data.u8[0] & (0x0FU));
      //HVP_debugMessageMultiplexer = (rx_frame.data.u8[0] & (0x0FU));  //0|4@1+ (1,0) [0|6] ""
      if (mux == 0) {
        HVP_gpioPassivePyroDepl = ((rx_frame.data.u8[0] >> 4) & (0x01U));       //: 4|1@1+ (1,0) [0|1] ""  Receiver
        HVP_gpioPyroIsoEn = ((rx_frame.data.u8[0] >> 5) & (0x01U));             //: 5|1@1+ (1,0) [0|1] ""  Receiver
        HVP_gpioCpFaultIn = ((rx_frame.data.u8[0] >> 6) & (0x01U));             //: 6|1@1+ (1,0) [0|1] ""  Receiver
        HVP_gpioPackContPowerEn = ((rx_frame.data.u8[0] >> 7) & (0x01U));       //: 7|1@1+ (1,0) [0|1] ""  Receiver
        HVP_gpioHvCablesOk = (rx_frame.data.u8[1] & (0x01U));                   //: 8|1@1+ (1,0) [0|1] ""  Receiver
        HVP_gpioHvpSelfEnable = ((rx_frame.data.u8[1] >> 1) & (0x01U));         //: 9|1@1+ (1,0) [0|1] ""  Receiver
        HVP_gpioLed = ((rx_frame.data.u8[1] >> 2) & (0x01U));                   //: 10|1@1+ (1,0) [0|1] ""  Receiver
        HVP_gpioCrashSignal = ((rx_frame.data.u8[1] >> 3) & (0x01U));           //: 11|1@1+ (1,0) [0|1] ""  Receiver
        HVP_gpioShuntDataReady = ((rx_frame.data.u8[1] >> 4) & (0x01U));        //: 12|1@1+ (1,0) [0|1] ""  Receiver
        HVP_gpioFcContPosAux = ((rx_frame.data.u8[1] >> 5) & (0x01U));          //: 13|1@1+ (1,0) [0|1] ""  Receiver
        HVP_gpioFcContNegAux = ((rx_frame.data.u8[1] >> 6) & (0x01U));          //: 14|1@1+ (1,0) [0|1] ""  Receiver
        HVP_gpioBmsEout = ((rx_frame.data.u8[1] >> 7) & (0x01U));               //: 15|1@1+ (1,0) [0|1] ""  Receiver
        HVP_gpioCpFaultOut = (rx_frame.data.u8[2] & (0x01U));                   //: 16|1@1+ (1,0) [0|1] ""  Receiver
        HVP_gpioPyroPor = ((rx_frame.data.u8[2] >> 1) & (0x01U));               //: 17|1@1+ (1,0) [0|1] ""  Receiver
        HVP_gpioShuntEn = ((rx_frame.data.u8[2] >> 2) & (0x01U));               //: 18|1@1+ (1,0) [0|1] ""  Receiver
        HVP_gpioHvpVerEn = ((rx_frame.data.u8[2] >> 3) & (0x01U));              //: 19|1@1+ (1,0) [0|1] ""  Receiver
        HVP_gpioPackCoontPosFlywheel = ((rx_frame.data.u8[2] >> 4) & (0x01U));  //: 20|1@1+ (1,0) [0|1] ""  Receiver
        HVP_gpioCpLatchEnable = ((rx_frame.data.u8[2] >> 5) & (0x01U));         //: 21|1@1+ (1,0) [0|1] ""  Receiver
        HVP_gpioPcsEnable = ((rx_frame.data.u8[2] >> 6) & (0x01U));             //: 22|1@1+ (1,0) [0|1] ""  Receiver
        HVP_gpioPcsDcdcPwmEnable = ((rx_frame.data.u8[2] >> 7) & (0x01U));      //: 23|1@1+ (1,0) [0|1] ""  Receiver
        HVP_gpioPcsChargePwmEnable = (rx_frame.data.u8[3] & (0x01U));           //: 24|1@1+ (1,0) [0|1] ""  Receiver
        HVP_gpioFcContPowerEnable = ((rx_frame.data.u8[3] >> 1) & (0x01U));     //: 25|1@1+ (1,0) [0|1] ""  Receiver
        HVP_gpioHvilEnable = ((rx_frame.data.u8[3] >> 2) & (0x01U));            //: 26|1@1+ (1,0) [0|1] ""  Receiver
        HVP_gpioSecDrdy = ((rx_frame.data.u8[3] >> 3) & (0x01U));               //: 27|1@1+ (1,0) [0|1] ""  Receiver
        HVP_hvp1v5Ref = ((rx_frame.data.u8[4] & (0xFFU)) << 4) |
                        ((rx_frame.data.u8[3] >> 4) & (0x0FU));  //: 28|12@1+ (0.1,0) [0|3] "V"  Receiver
        HVP_shuntCurrentDebug = ((rx_frame.data.u8[6] & (0xFFU)) << 8) |
                                (rx_frame.data.u8[5] & (0xFFU));     //: 40|16@1- (0.1,0) [-3276.8|3276.7] "A"  Receiver
        HVP_packCurrentMia = (rx_frame.data.u8[7] & (0x01U));        //: 56|1@1+ (1,0) [0|1] ""  Receiver
        HVP_auxCurrentMia = ((rx_frame.data.u8[7] >> 1) & (0x01U));  //: 57|1@1+ (1,0) [0|1] ""  Receiver
        HVP_currentSenseMia = ((rx_frame.data.u8[7] >> 2) & (0x03U));          //: 58|1@1+ (1,0) [0|1] ""  Receiver
        HVP_shuntRefVoltageMismatch = ((rx_frame.data.u8[7] >> 3) & (0x01U));  //: 59|1@1+ (1,0) [0|1] ""  Receiver
        HVP_shuntThermistorMia = ((rx_frame.data.u8[7] >> 4) & (0x01U));       //: 60|1@1+ (1,0) [0|1] ""  Receiver
        HVP_shuntHwMia = ((rx_frame.data.u8[7] >> 5) & (0x01U));               //: 61|1@1+ (1,0) [0|1] ""  Receiver
      }
      if (mux == 1) {
        HVP_dcLinkVoltage = ((rx_frame.data.u8[2] & (0xFFU)) << 8) |
                            (rx_frame.data.u8[1] & (0xFFU));  //: 8|16@1- (0.1,0) [-3276.8|3276.7] "V"  Receiver
        HVP_packVoltage = ((rx_frame.data.u8[4] & (0xFFU)) << 8) |
                          (rx_frame.data.u8[3] & (0xFFU));  //: 24|16@1- (0.1,0) [-3276.8|3276.7] "V"  Receiver
        HVP_fcLinkVoltage = ((rx_frame.data.u8[6] & (0xFFU)) << 8) |
                            (rx_frame.data.u8[5] & (0xFFU));  //: 40|16@1- (0.1,0) [-3276.8|3276.7] "V"  Receiver
      }
      if (mux == 2) {
        HVP_packContVoltage = ((rx_frame.data.u8[1] & (0xFFU)) << 4) |
                              ((rx_frame.data.u8[0] >> 4) & (0x0FU));  //: 4|12@1+ (0.1,0) [0|30] "V"  Receiver
        HVP_packNegativeV = ((rx_frame.data.u8[3] & (0xFFU)) << 8) |
                            (rx_frame.data.u8[2] & (0xFFU));  //: 16|16@1- (0.1,0) [-550|550] "V"  Receiver
        HVP_packPositiveV = ((rx_frame.data.u8[5] & (0xFFU)) << 8) |
                            (rx_frame.data.u8[4] & (0xFFU));  //: 32|16@1- (0.1,0) [-550|550] "V"  Receiver
        HVP_pyroAnalog = ((rx_frame.data.u8[7] & (0x0FU)) << 8) |
                         (rx_frame.data.u8[6] & (0xFFU));  //: 48|12@1+ (0.1,0) [0|3] "V"  Receiver
      }
      if (mux == 3) {
        HVP_dcLinkNegativeV = ((rx_frame.data.u8[2] & (0xFFU)) << 8) |
                              (rx_frame.data.u8[1] & (0xFFU));  //: 8|16@1- (0.1,0) [-550|550] "V"  Receiver
        HVP_dcLinkPositiveV = ((rx_frame.data.u8[4] & (0xFFU)) << 8) |
                              (rx_frame.data.u8[3] & (0xFFU));  //: 24|16@1- (0.1,0) [-550|550] "V"  Receiver
        HVP_fcLinkNegativeV = ((rx_frame.data.u8[6] & (0xFFU)) << 8) |
                              (rx_frame.data.u8[5] & (0xFFU));  //: 40|16@1- (0.1,0) [-550|550] "V"  Receiver
      }
      if (mux == 4) {
        HVP_fcContCoilCurrent = ((rx_frame.data.u8[1] & (0xFFU)) << 4) |
                                ((rx_frame.data.u8[0] >> 4) & (0x0FU));  //: 4|12@1+ (0.1,0) [0|7.5] "A"  Receiver
        HVP_fcContVoltage = ((rx_frame.data.u8[3] & (0x0FU)) << 8) |
                            (rx_frame.data.u8[2] & (0xFFU));  //: 16|12@1+ (0.1,0) [0|30] "V"  Receiver
        HVP_hvilInVoltage = ((rx_frame.data.u8[4] & (0xFFU)) << 4) |
                            ((rx_frame.data.u8[3] >> 4) & (0x0FU));  //: 28|12@1+ (0.1,0) [0|30] "V"  Receiver
        HVP_hvilOutVoltage = ((rx_frame.data.u8[6] & (0x0FU)) << 8) |
                             (rx_frame.data.u8[5] & (0xFFU));  //: 40|12@1+ (0.1,0) [0|30] "V"  Receiver
      }
      if (mux == 5) {
        HVP_fcLinkPositiveV = ((rx_frame.data.u8[2] & (0xFFU)) << 8) |
                              (rx_frame.data.u8[1] & (0xFFU));  //: 8|16@1- (0.1,0) [-550|550] "V"  Receiver
        HVP_packContCoilCurrent = ((rx_frame.data.u8[4] & (0x0FU)) << 8) |
                                  (rx_frame.data.u8[3] & (0xFFU));  //: 24|12@1+ (0.1,0) [0|7.5] "A"  Receiver
        HVP_battery12V = ((rx_frame.data.u8[5] & (0xFFU)) << 4) |
                         ((rx_frame.data.u8[4] >> 4) & (0x0FU));  //: 36|12@1+ (0.1,0) [0|30] "V"  Receiver
        HVP_shuntRefVoltageDbg = ((rx_frame.data.u8[7] & (0xFFU)) << 8) |
                                 (rx_frame.data.u8[6] & (0xFFU));  //: 48|16@1- (0.001,0) [-32.768|32.767] "V"  Receiver
      }
      if (mux == 6) {
        HVP_shuntAuxCurrentDbg = ((rx_frame.data.u8[2] & (0xFFU)) << 8) |
                                 (rx_frame.data.u8[1] & (0xFFU));  //: 8|16@1- (0.1,0) [-3276.8|3276.7] "A"  Receiver
        HVP_shuntBarTempDbg = ((rx_frame.data.u8[4] & (0xFFU)) << 8) |
                              (rx_frame.data.u8[3] & (0xFFU));  //: 24|16@1- (0.01,0) [-327.67|327.67] "C"  Receiver
        HVP_shuntAsicTempDbg = ((rx_frame.data.u8[6] & (0xFFU)) << 8) |
                               (rx_frame.data.u8[5] & (0xFFU));  //: 40|16@1- (0.01,0) [-327.67|327.67] "C"  Receiver
        HVP_shuntAuxCurrentStatus = (rx_frame.data.u8[7] & (0x03U));       //: 56|2@1+ (1,0) [0|3] ""  Receiver
        HVP_shuntBarTempStatus = ((rx_frame.data.u8[7] >> 2) & (0x03U));   //: 58|2@1+ (1,0) [0|3] ""  Receiver
        HVP_shuntAsicTempStatus = ((rx_frame.data.u8[7] >> 4) & (0x03U));  //: 60|2@1+ (1,0) [0|3] ""  Receiver
      }
      break;
    case 0x3aa:  //HVP_alertMatrix1
      battery_WatchdogReset = (rx_frame.data.u8[0] & 0x01);
      battery_PowerLossReset = ((rx_frame.data.u8[0] & 0x02) >> 1);
      battery_SwAssertion = ((rx_frame.data.u8[0] & 0x04) >> 2);
      battery_CrashEvent = ((rx_frame.data.u8[0] & 0x08) >> 3);
      battery_OverDchgCurrentFault = ((rx_frame.data.u8[0] & 0x10) >> 4);
      battery_OverChargeCurrentFault = ((rx_frame.data.u8[0] & 0x20) >> 5);
      battery_OverCurrentFault = ((rx_frame.data.u8[0] & 0x40) >> 6);
      battery_OverTemperatureFault = ((rx_frame.data.u8[1] & 0x80) >> 7);
      battery_OverVoltageFault = (rx_frame.data.u8[1] & 0x01);
      battery_UnderVoltageFault = ((rx_frame.data.u8[1] & 0x02) >> 1);
      battery_PrimaryBmbMiaFault = ((rx_frame.data.u8[1] & 0x04) >> 2);
      battery_SecondaryBmbMiaFault = ((rx_frame.data.u8[1] & 0x08) >> 3);
      battery_BmbMismatchFault = ((rx_frame.data.u8[1] & 0x10) >> 4);
      battery_BmsHviMiaFault = ((rx_frame.data.u8[1] & 0x20) >> 5);
      battery_CpMiaFault = ((rx_frame.data.u8[1] & 0x40) >> 6);
      battery_PcsMiaFault = ((rx_frame.data.u8[1] & 0x80) >> 7);
      battery_BmsFault = (rx_frame.data.u8[2] & 0x01);
      battery_PcsFault = ((rx_frame.data.u8[2] & 0x02) >> 1);
      battery_CpFault = ((rx_frame.data.u8[2] & 0x04) >> 2);
      battery_ShuntHwMiaFault = ((rx_frame.data.u8[2] & 0x08) >> 3);
      battery_PyroMiaFault = ((rx_frame.data.u8[2] & 0x10) >> 4);
      battery_hvsMiaFault = ((rx_frame.data.u8[2] & 0x20) >> 5);
      battery_hviMiaFault = ((rx_frame.data.u8[2] & 0x40) >> 6);
      battery_Supply12vFault = ((rx_frame.data.u8[2] & 0x80) >> 7);
      battery_VerSupplyFault = (rx_frame.data.u8[3] & 0x01);
      battery_HvilFault = ((rx_frame.data.u8[3] & 0x02) >> 1);
      battery_BmsHvsMiaFault = ((rx_frame.data.u8[3] & 0x04) >> 2);
      battery_PackVoltMismatchFault = ((rx_frame.data.u8[3] & 0x08) >> 3);
      battery_EnsMiaFault = ((rx_frame.data.u8[3] & 0x10) >> 4);
      battery_PackPosCtrArcFault = ((rx_frame.data.u8[3] & 0x20) >> 5);
      battery_packNegCtrArcFault = ((rx_frame.data.u8[3] & 0x40) >> 6);
      battery_ShuntHwAndBmsMiaFault = ((rx_frame.data.u8[3] & 0x80) >> 7);
      battery_fcContHwFault = (rx_frame.data.u8[4] & 0x01);
      battery_robinOverVoltageFault = ((rx_frame.data.u8[4] & 0x02) >> 1);
      battery_packContHwFault = ((rx_frame.data.u8[4] & 0x04) >> 2);
      battery_pyroFuseBlown = ((rx_frame.data.u8[4] & 0x08) >> 3);
      battery_pyroFuseFailedToBlow = ((rx_frame.data.u8[4] & 0x10) >> 4);
      battery_CpilFault = ((rx_frame.data.u8[4] & 0x20) >> 5);
      battery_PackContactorFellOpen = ((rx_frame.data.u8[4] & 0x40) >> 6);
      battery_FcContactorFellOpen = ((rx_frame.data.u8[4] & 0x80) >> 7);
      battery_packCtrCloseBlocked = (rx_frame.data.u8[5] & 0x01);
      battery_fcCtrCloseBlocked = ((rx_frame.data.u8[5] & 0x02) >> 1);
      battery_packContactorForceOpen = ((rx_frame.data.u8[5] & 0x04) >> 2);
      battery_fcContactorForceOpen = ((rx_frame.data.u8[5] & 0x08) >> 3);
      battery_dcLinkOverVoltage = ((rx_frame.data.u8[5] & 0x10) >> 4);
      battery_shuntOverTemperature = ((rx_frame.data.u8[5] & 0x20) >> 5);
      battery_passivePyroDeploy = ((rx_frame.data.u8[5] & 0x40) >> 6);
      battery_logUploadRequest = ((rx_frame.data.u8[5] & 0x80) >> 7);
      battery_packCtrCloseFailed = (rx_frame.data.u8[6] & 0x01);
      battery_fcCtrCloseFailed = ((rx_frame.data.u8[6] & 0x02) >> 1);
      battery_shuntThermistorMia = ((rx_frame.data.u8[6] & 0x04) >> 2);
      break;
    case 0x320:  //800 BMS_alertMatrix                                                //BMS_alertMatrix 800 BMS_alertMatrix: 8 VEH
      mux = (rx_frame.data.u8[0] & (0x0F));
      if (mux == 0) {                                                                     //mux0
        battery_BMS_matrixIndex = (rx_frame.data.u8[0] & (0x0F));                         // 0|4@1+ (1,0) [0|0] ""  X
        battery_BMS_a017_SW_Brick_OV = ((rx_frame.data.u8[2] >> 4) & (0x01));             //20|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a018_SW_Brick_UV = ((rx_frame.data.u8[2] >> 5) & (0x01));             //21|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a019_SW_Module_OT = ((rx_frame.data.u8[2] >> 6) & (0x01));            //22|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a021_SW_Dr_Limits_Regulation = (rx_frame.data.u8[3] & (0x01U));       //24|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a022_SW_Over_Current = ((rx_frame.data.u8[3] >> 1) & (0x01U));        //25|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a023_SW_Stack_OV = ((rx_frame.data.u8[3] >> 2) & (0x01U));            //26|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a024_SW_Islanded_Brick = ((rx_frame.data.u8[3] >> 3) & (0x01U));      //27|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a025_SW_PwrBalance_Anomaly = ((rx_frame.data.u8[3] >> 4) & (0x01U));  //28|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a026_SW_HFCurrent_Anomaly = ((rx_frame.data.u8[3] >> 5) & (0x01U));   //29|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a034_SW_Passive_Isolation = ((rx_frame.data.u8[4] >> 5) & (0x01U));   //37|1@1+ (1,0) [0|0] ""  X ?
        battery_BMS_a035_SW_Isolation = ((rx_frame.data.u8[4] >> 6) & (0x01U));           //38|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a036_SW_HvpHvilFault = ((rx_frame.data.u8[4] >> 6) & (0x01U));        //39|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a037_SW_Flood_Port_Open = (rx_frame.data.u8[5] & (0x01U));            //40|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a039_SW_DC_Link_Over_Voltage = ((rx_frame.data.u8[5] >> 2) & (0x01U));  //42|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a041_SW_Power_On_Reset = ((rx_frame.data.u8[5] >> 4) & (0x01U));        //44|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a042_SW_MPU_Error = ((rx_frame.data.u8[5] >> 5) & (0x01U));             //45|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a043_SW_Watch_Dog_Reset = ((rx_frame.data.u8[5] >> 6) & (0x01U));       //46|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a044_SW_Assertion = ((rx_frame.data.u8[5] >> 7) & (0x01U));             //47|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a045_SW_Exception = (rx_frame.data.u8[6] & (0x01U));                    //48|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a046_SW_Task_Stack_Usage = ((rx_frame.data.u8[6] >> 1) & (0x01U));      //49|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a047_SW_Task_Stack_Overflow = ((rx_frame.data.u8[6] >> 2) & (0x01U));   //50|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a048_SW_Log_Upload_Request = ((rx_frame.data.u8[6] >> 3) & (0x01U));    //51|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a050_SW_Brick_Voltage_MIA = ((rx_frame.data.u8[6] >> 5) & (0x01U));     //53|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a051_SW_HVC_Vref_Bad = ((rx_frame.data.u8[6] >> 6) & (0x01U));          //54|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a052_SW_PCS_MIA = ((rx_frame.data.u8[6] >> 7) & (0x01U));               //55|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a053_SW_ThermalModel_Sanity = (rx_frame.data.u8[7] & (0x01U));          //56|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a054_SW_Ver_Supply_Fault = ((rx_frame.data.u8[7] >> 1) & (0x01U));      //57|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a059_SW_Pack_Voltage_Sensing = ((rx_frame.data.u8[7] >> 6) & (0x01U));  //62|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a060_SW_Leakage_Test_Failure = ((rx_frame.data.u8[7] >> 7) & (0x01U));  //63|1@1+ (1,0) [0|0] ""  X
      }
      if (mux == 1) {                                                                       //mux1
        battery_BMS_a061_robinBrickOverVoltage = ((rx_frame.data.u8[0] >> 4) & (0x01U));    //4|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a062_SW_BrickV_Imbalance = ((rx_frame.data.u8[0] >> 5) & (0x01U));      //5|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a063_SW_ChargePort_Fault = ((rx_frame.data.u8[0] >> 6) & (0x01U));      //6|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a064_SW_SOC_Imbalance = ((rx_frame.data.u8[0] >> 7) & (0x01U));         //7|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a069_SW_Low_Power = ((rx_frame.data.u8[1] >> 4) & (0x01U));             //12|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a071_SW_SM_TransCon_Not_Met = ((rx_frame.data.u8[1] >> 6) & (0x01U));   //14|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a075_SW_Chg_Disable_Failure = ((rx_frame.data.u8[2] >> 2) & (0x01U));   //18|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a076_SW_Dch_While_Charging = ((rx_frame.data.u8[2] >> 3) & (0x01U));    //19|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a077_SW_Charger_Regulation = ((rx_frame.data.u8[2] >> 4) & (0x01U));    //20|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a081_SW_Ctr_Close_Blocked = (rx_frame.data.u8[3] & (0x01U));            //24|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a082_SW_Ctr_Force_Open = ((rx_frame.data.u8[3] >> 1) & (0x01U));        //25|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a083_SW_Ctr_Close_Failure = ((rx_frame.data.u8[3] >> 2) & (0x01U));     //26|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a084_SW_Sleep_Wake_Aborted = ((rx_frame.data.u8[3] >> 3) & (0x01U));    //27|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a087_SW_Feim_Test_Blocked = ((rx_frame.data.u8[3] >> 6) & (0x01U));     //30|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a088_SW_VcFront_MIA_InDrive = ((rx_frame.data.u8[3] >> 7) & (0x01U));   //31|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a089_SW_VcFront_MIA = (rx_frame.data.u8[4] & (0x01U));                  //32|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a090_SW_Gateway_MIA = ((rx_frame.data.u8[4] >> 1) & (0x01U));           //33|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a091_SW_ChargePort_MIA = ((rx_frame.data.u8[4] >> 2) & (0x01U));        //34|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a092_SW_ChargePort_Mia_On_Hv = ((rx_frame.data.u8[4] >> 3) & (0x01U));  //35|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a094_SW_Drive_Inverter_MIA = ((rx_frame.data.u8[4] >> 5) & (0x01U));    //37|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a099_SW_BMB_Communication = ((rx_frame.data.u8[5] >> 2) & (0x01U));     //42|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a105_SW_One_Module_Tsense = (rx_frame.data.u8[6] & (0x01U));            //48|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a106_SW_All_Module_Tsense = ((rx_frame.data.u8[6] >> 1) & (0x01U));     //49|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a107_SW_Stack_Voltage_MIA = ((rx_frame.data.u8[6] >> 2) & (0x01U));     //50|1@1+ (1,0) [0|0] ""  X
      }
      if (mux == 2) {                                                                       //mux2
        battery_BMS_a121_SW_NVRAM_Config_Error = ((rx_frame.data.u8[0] >> 4) & (0x01U));    // 4|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a122_SW_BMS_Therm_Irrational = ((rx_frame.data.u8[0] >> 5) & (0x01U));  //5|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a123_SW_Internal_Isolation = ((rx_frame.data.u8[0] >> 6) & (0x01U));    //6|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a127_SW_shunt_SNA = ((rx_frame.data.u8[1] >> 2) & (0x01U));             //10|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a128_SW_shunt_MIA = ((rx_frame.data.u8[1] >> 3) & (0x01U));             //11|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a129_SW_VSH_Failure = ((rx_frame.data.u8[1] >> 4) & (0x01U));           //12|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a130_IO_CAN_Error = ((rx_frame.data.u8[1] >> 5) & (0x01U));             //13|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a131_Bleed_FET_Failure = ((rx_frame.data.u8[1] >> 6) & (0x01U));        //14|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a132_HW_BMB_OTP_Uncorrctbl = ((rx_frame.data.u8[1] >> 7) & (0x01U));    //15|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a134_SW_Delayed_Ctr_Off = ((rx_frame.data.u8[2] >> 1) & (0x01U));       //17|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a136_SW_Module_OT_Warning = ((rx_frame.data.u8[2] >> 3) & (0x01U));     //19|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a137_SW_Brick_UV_Warning = ((rx_frame.data.u8[2] >> 4) & (0x01U));      //20|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a138_SW_Brick_OV_Warning = ((rx_frame.data.u8[2] >> 5) & (0x01U));      //21|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a139_SW_DC_Link_V_Irrational = ((rx_frame.data.u8[2] >> 6) & (0x01U));  //22|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a141_SW_BMB_Status_Warning = (rx_frame.data.u8[3] & (0x01U));           //24|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a144_Hvp_Config_Mismatch = ((rx_frame.data.u8[3] >> 3) & (0x01U));      //27|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a145_SW_SOC_Change = ((rx_frame.data.u8[3] >> 4) & (0x01U));            //28|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a146_SW_Brick_Overdischarged = ((rx_frame.data.u8[3] >> 5) & (0x01U));  //29|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a149_SW_Missing_Config_Block = (rx_frame.data.u8[4] & (0x01U));         //32|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a151_SW_external_isolation = ((rx_frame.data.u8[4] >> 2) & (0x01U));    //34|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a156_SW_BMB_Vref_bad = ((rx_frame.data.u8[4] >> 7) & (0x01U));          //39|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a157_SW_HVP_HVS_Comms = (rx_frame.data.u8[5] & (0x01U));                //40|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a158_SW_HVP_HVI_Comms = ((rx_frame.data.u8[5] >> 1) & (0x01U));         //41|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a159_SW_HVP_ECU_Error = ((rx_frame.data.u8[5] >> 2) & (0x01U));         //42|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a161_SW_DI_Open_Request = ((rx_frame.data.u8[5] >> 4) & (0x01U));       //44|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a162_SW_No_Power_For_Support = ((rx_frame.data.u8[5] >> 5) & (0x01U));  //45|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a163_SW_Contactor_Mismatch = ((rx_frame.data.u8[5] >> 6) & (0x01U));    //46|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a164_SW_Uncontrolled_Regen = ((rx_frame.data.u8[5] >> 7) & (0x01U));    //47|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a165_SW_Pack_Partial_Weld = (rx_frame.data.u8[6] & (0x01U));            //48|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a166_SW_Pack_Full_Weld = ((rx_frame.data.u8[6] >> 1) & (0x01U));        //49|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a167_SW_FC_Partial_Weld = ((rx_frame.data.u8[6] >> 2) & (0x01U));       //50|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a168_SW_FC_Full_Weld = ((rx_frame.data.u8[6] >> 3) & (0x01U));          //51|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a169_SW_FC_Pack_Weld = ((rx_frame.data.u8[6] >> 4) & (0x01U));          //52|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a170_SW_Limp_Mode = ((rx_frame.data.u8[6] >> 5) & (0x01U));             //53|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a171_SW_Stack_Voltage_Sense = ((rx_frame.data.u8[6] >> 6) & (0x01U));   //54|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a174_SW_Charge_Failure = ((rx_frame.data.u8[7] >> 1) & (0x01U));        //57|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a176_SW_GracefulPowerOff = ((rx_frame.data.u8[7] >> 3) & (0x01U));      //59|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a179_SW_Hvp_12V_Fault = ((rx_frame.data.u8[7] >> 6) & (0x01U));         //62|1@1+ (1,0) [0|0] ""  X
        battery_BMS_a180_SW_ECU_reset_blocked = ((rx_frame.data.u8[7] >> 7) & (0x01U));     //63|1@1+ (1,0) [0|0] ""  X
      }
      break;
    case 0x72A:  //1834 ID72ABMS_serialNumber
      //Work in progress to display BMS Serial Number in ASCII: 00 54 47 33 32 31 32 30 (mux 0) .TG32120 + 01 32 30 30 33 41 48 58 (mux 1) .2003AHX = TG321202003AHX
      if (rx_frame.data.u8[0] == 0x00) {
        BMS_SerialNumber[0] = rx_frame.data.u8[1];
        BMS_SerialNumber[1] = rx_frame.data.u8[2];
        BMS_SerialNumber[2] = rx_frame.data.u8[3];
        BMS_SerialNumber[3] = rx_frame.data.u8[4];
        BMS_SerialNumber[4] = rx_frame.data.u8[5];
        BMS_SerialNumber[5] = rx_frame.data.u8[6];
        BMS_SerialNumber[6] = rx_frame.data.u8[7];
      }
      if (rx_frame.data.u8[0] == 0x01) {
        BMS_SerialNumber[7] = rx_frame.data.u8[1];
        BMS_SerialNumber[8] = rx_frame.data.u8[2];
        BMS_SerialNumber[9] = rx_frame.data.u8[3];
        BMS_SerialNumber[10] = rx_frame.data.u8[4];
        BMS_SerialNumber[11] = rx_frame.data.u8[5];
        BMS_SerialNumber[12] = rx_frame.data.u8[6];
        BMS_SerialNumber[13] = rx_frame.data.u8[7];
      }
      break;
    default:
      break;
  }
}

#if defined(TESLA_MODEL_SX_BATTERY) || defined(EXP_TESLA_BMS_DIGITAL_HVIL)
CAN_frame can_msg_1CF[] = {
    {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x1CF, .data = {0x01, 0x00, 0x00, 0x1A, 0x1C, 0x02, 0x60, 0x69}},
    {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x1CF, .data = {0x01, 0x00, 0x00, 0x1A, 0x1C, 0x02, 0x80, 0x89}},
    {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x1CF, .data = {0x01, 0x00, 0x00, 0x1A, 0x1C, 0x02, 0xA0, 0xA9}},
    {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x1CF, .data = {0x01, 0x00, 0x00, 0x1A, 0x1C, 0x02, 0xC0, 0xC9}},
    {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x1CF, .data = {0x01, 0x00, 0x00, 0x1A, 0x1C, 0x02, 0xE0, 0xE9}},
    {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x1CF, .data = {0x01, 0x00, 0x00, 0x1A, 0x1C, 0x02, 0x00, 0x09}},
    {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x1CF, .data = {0x01, 0x00, 0x00, 0x1A, 0x1C, 0x02, 0x20, 0x29}},
    {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x1CF, .data = {0x01, 0x00, 0x00, 0x1A, 0x1C, 0x02, 0x40, 0x49}}};

CAN_frame can_msg_118[] = {
    {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x118, .data = {0x61, 0x80, 0x30, 0x10, 0x00, 0x08, 0x00, 0x80}},
    {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x118, .data = {0x62, 0x81, 0x30, 0x10, 0x00, 0x08, 0x00, 0x80}},
    {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x118, .data = {0x63, 0x82, 0x30, 0x10, 0x00, 0x08, 0x00, 0x80}},
    {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x118, .data = {0x64, 0x83, 0x30, 0x10, 0x00, 0x08, 0x00, 0x80}},
    {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x118, .data = {0x65, 0x84, 0x30, 0x10, 0x00, 0x08, 0x00, 0x80}},
    {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x118, .data = {0x66, 0x85, 0x30, 0x10, 0x00, 0x08, 0x00, 0x80}},
    {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x118, .data = {0x67, 0x86, 0x30, 0x10, 0x00, 0x08, 0x00, 0x80}},
    {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x118, .data = {0x68, 0x87, 0x30, 0x10, 0x00, 0x08, 0x00, 0x80}},
    {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x118, .data = {0x69, 0x88, 0x30, 0x10, 0x00, 0x08, 0x00, 0x80}},
    {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x118, .data = {0x6A, 0x89, 0x30, 0x10, 0x00, 0x08, 0x00, 0x80}},
    {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x118, .data = {0x6B, 0x8A, 0x30, 0x10, 0x00, 0x08, 0x00, 0x80}},
    {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x118, .data = {0x6C, 0x8B, 0x30, 0x10, 0x00, 0x08, 0x00, 0x80}},
    {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x118, .data = {0x6D, 0x8C, 0x30, 0x10, 0x00, 0x08, 0x00, 0x80}},
    {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x118, .data = {0x6E, 0x8D, 0x30, 0x10, 0x00, 0x08, 0x00, 0x80}},
    {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x118, .data = {0x6F, 0x8E, 0x30, 0x10, 0x00, 0x08, 0x00, 0x80}},
    {.FD = false, .ext_ID = false, .DLC = 8, .ID = 0x118, .data = {0x70, 0x8F, 0x30, 0x10, 0x00, 0x08, 0x00, 0x80}}};

unsigned long lastSend1CF = 0;
unsigned long lastSend118 = 0;

int index_1CF = 0;
int index_118 = 0;
#endif  //defined(TESLA_MODEL_SX_BATTERY) || defined(EXP_TESLA_BMS_DIGITAL_HVIL)

// Modify the initialize_and_update_CAN_frame_221() function:
void initialize_and_update_CAN_frame_221() {
  unsigned long currentMillis = millis();
  
  // Create and initialize message structure
  TESLA_221_Struct msg;

  // Handle Mux0 message timing
  if (currentMillis - previousMux0Time >= MUX0_MESSAGE_INTERVAL) {
    previousMux0Time = currentMillis;
    
    // Initialize and send Mux0 message
    initialize_msg(msg, true);  // true for Mux0
    update_CAN_frame_221(TESLA_221, msg, true);
    transmit_can_frame(&TESLA_221, can_config.battery);
  }

  // Handle Mux1 message timing
  if (currentMillis - previousMux1Time >= MUX0_MESSAGE_INTERVAL) {
    previousMux1Time = currentMillis;
    
    // Initialize and send Mux1 message
    initialize_msg(msg, false);  // false for Mux1
    update_CAN_frame_221(TESLA_221, msg, false);
    transmit_can_frame(&TESLA_221, can_config.battery);
  }
}

void transmit_can_battery() {
  /*From bielec: My fist 221 message, to close the contactors is 0x41, 0x11, 0x01, 0x00, 0x00, 0x00, 0x20, 0x96 and then, 
to cause "hv_up_for_drive" I send an additional 221 message 0x61, 0x15, 0x01, 0x00, 0x00, 0x00, 0x20, 0xBA  so 
two 221 messages are being continuously transmitted.   When I want to shut down, I stop the second message and only send 
the first, for a few cycles, then stop all  messages which causes the contactor to open. */

  unsigned long currentMillis = millis();

  if (!cellvoltagesRead) {
    return;  //All cellvoltages not read yet, do not proceed with contactor closing
  }

  // Declare message variables
  TESLA_221_Struct msg_221;
  TESLA_2D1_Struct msg_2D1;
  TESLA_3A1_Struct msg_3A1;
  TESLA_1F9_Struct msg_1F9;
  TESLA_339_Struct msg_339;
  TESLA_333_Struct msg_333;
  TESLA_321_Struct msg_321;
  TESLA_241_Struct msg_241;

  // Declare and initialize mux0
  bool mux0 = true;

#if defined(TESLA_MODEL_SX_BATTERY) || defined(EXP_TESLA_BMS_DIGITAL_HVIL)
  if ((datalayer.system.status.inverter_allows_contactor_closing) && (datalayer.battery.status.bms_status != FAULT)) {
    if (currentMillis - lastSend1CF >= 10) {
      transmit_can_frame(&can_msg_1CF[index_1CF], can_config.battery);

      index_1CF = (index_1CF + 1) % 8;
      lastSend1CF = currentMillis;
    }

    if (currentMillis - lastSend118 >= 10) {
      transmit_can_frame(&can_msg_118[index_118], can_config.battery);

      index_118 = (index_118 + 1) % 16;
      lastSend118 = currentMillis;
    }
  } else {
    index_1CF = 0;
    index_118 = 0;
  }
#endif  //defined(TESLA_MODEL_SX_BATTERY) || defined(EXP_TESLA_BMS_DIGITAL_HVIL)

  //Send 50ms message
  if (currentMillis - previousMillis50 >= INTERVAL_50_MS) {
    // Check if sending of CAN messages has been delayed too much.
    if ((currentMillis - previousMillis50 >= INTERVAL_50_MS_DELAYED) && (currentMillis > BOOTUP_TIME)) {
      set_event(EVENT_CAN_OVERRUN, (currentMillis - previousMillis50));
    } else {
      clear_event(EVENT_CAN_OVERRUN);
    }
    previousMillis50 = currentMillis;

    if ((datalayer.system.status.inverter_allows_contactor_closing == true) &&
        (datalayer.battery.status.bms_status != FAULT)) {
      sendContactorClosingMessagesStill = 300;
      initialize_msg(msg_221, true);                   // Initialize the message structure
      update_CAN_frame_221(TESLA_221, msg_221, mux0);  // Update the CAN frame data
      transmit_can_frame(&TESLA_221, can_config.battery);
    } else {  // Faulted state, or inverter blocks contactor closing
      if (sendContactorClosingMessagesStill > 0) {
        initialize_msg(msg_221, true);                   // Initialize the message structure
        update_CAN_frame_221(TESLA_221, msg_221, mux0);  // Update the CAN frame data
        transmit_can_frame(&TESLA_221, can_config.battery);
        sendContactorClosingMessagesStill--;
      }
    }
  }

  //Send 100ms message
  if (currentMillis - previousMillis100 >= INTERVAL_100_MS) {
    previousMillis100 = currentMillis;

    initialize_msg(msg_2D1);                   // Initialize the message structure
    update_CAN_frame_2D1(TESLA_2D1, msg_2D1);  // Update the CAN frame data
    transmit_can_frame(&TESLA_2D1, can_config.battery);

    initialize_msg(msg_3A1);                   // Initialize the message structure
    update_CAN_frame_3A1(TESLA_3A1, msg_3A1);  // Update the CAN frame data
    transmit_can_frame(&TESLA_3A1, can_config.battery);

    initialize_msg(msg_1F9);                   // Initialize the message structure
    update_CAN_frame_1F9(TESLA_1F9, msg_1F9);  // Update the CAN frame data
    transmit_can_frame(&TESLA_1F9, can_config.battery);

    initialize_msg(msg_339);                   // Initialize the message structure
    update_CAN_frame_339(TESLA_339, msg_339);  // Update the CAN frame data
    transmit_can_frame(&TESLA_339, can_config.battery);

    initialize_msg(msg_241);                   // Initialize the message structure
    update_CAN_frame_241(TESLA_241, msg_241);  // Update the CAN frame data
    transmit_can_frame(&TESLA_241, can_config.battery);
  }

  // Send 500ms message
  if (currentMillis - previousMillis500 >= INTERVAL_500_MS) {
    previousMillis500 = currentMillis;

    initialize_msg(msg_333);                   // Initialize the message structure
    update_CAN_frame_333(TESLA_333, msg_333);  // Update the CAN frame data
    transmit_can_frame(&TESLA_333, can_config.battery);
  }

  // Send 1s message
  if (currentMillis - previousMillis1s >= INTERVAL_1_S) {
    previousMillis1s = currentMillis;

    initialize_msg(msg_321);                   // Initialize the message structure
    update_CAN_frame_321(TESLA_321, msg_321);  // Update the CAN frame data
    transmit_can_frame(&TESLA_321, can_config.battery);
  }

  if (stateMachineClearIsolationFault != 0xFF) {
    //This implementation should be rewritten to actually replying to the UDS replied sent by the BMS
    //While this may work, it is not the correct way to implement this clearing logic
    switch (stateMachineClearIsolationFault) {
      case 0:
        TESLA_602.data = {0x02, 0x27, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00};
        transmit_can_frame(&TESLA_602, can_config.battery);
        stateMachineClearIsolationFault = 1;
        break;
      case 1:
        TESLA_602.data = {0x30, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00};
        transmit_can_frame(&TESLA_602, can_config.battery);
        // BMS should reply 02 50 C0 FF FF FF FF FF
        stateMachineClearIsolationFault = 2;
        break;
      case 2:
        TESLA_602.data = {0x10, 0x12, 0x27, 0x06, 0x35, 0x34, 0x37, 0x36};
        transmit_can_frame(&TESLA_602, can_config.battery);
        // BMS should reply 7E FF FF FF FF FF FF
        stateMachineClearIsolationFault = 3;
        break;
      case 3:
        TESLA_602.data = {0x21, 0x31, 0x30, 0x33, 0x32, 0x3D, 0x3C, 0x3F};
        transmit_can_frame(&TESLA_602, can_config.battery);
        stateMachineClearIsolationFault = 4;
        break;
      case 4:
        TESLA_602.data = {0x22, 0x3E, 0x39, 0x38, 0x3B, 0x3A, 0x00, 0x00};
        transmit_can_frame(&TESLA_602, can_config.battery);
        stateMachineClearIsolationFault = 5;
        break;
      case 5:
        TESLA_602.data = {0x04, 0x31, 0x01, 0x04, 0x0A, 0x00, 0x00, 0x00};
        transmit_can_frame(&TESLA_602, can_config.battery);
        stateMachineClearIsolationFault = 0xFF;
        break;
      default:
        //Something went wrong. Reset all and cancel
        stateMachineClearIsolationFault = 0xFF;
        break;
    }
  }
}

void print_int_with_units(char* header, int value, char* units) {
  logging.print(header);
  logging.print(value);
  logging.print(units);
}

void print_SOC(char* header, int SOC) {
  logging.print(header);
  logging.print(SOC / 100);
  logging.print(".");
  int hundredth = SOC % 100;
  if (hundredth < 10)
    logging.print(0);
  logging.print(hundredth);
  logging.println("%");
}

void printFaultCodesIfActive() {
  if (battery_packCtrsClosingAllowed == 0) {
    logging.println(
        "ERROR: Check high voltage connectors and interlock circuit! Closing contactor not allowed! Values: ");
  }
  if (battery_pyroTestInProgress == 1) {
    logging.println("ERROR: Please wait for Pyro Connection check to finish, HV cables successfully seated!");
  }
  if (datalayer.system.status.inverter_allows_contactor_closing == false) {
    logging.println(
        "ERROR: Solar inverter does not allow for contactor closing. Check communication connection to the "
        "inverter "
        "OR "
        "disable the inverter protocol to proceed with contactor closing");
  }
  // Check each symbol and print debug information if its value is 1
  // 0X3AA: 938 HVP_alertMatrix1
  //printDebugIfActive(battery_WatchdogReset, "ERROR: The processor has experienced a reset due to watchdog reset"); //Uncommented due to not affecting usage
  printDebugIfActive(battery_PowerLossReset, "ERROR: The processor has experienced a reset due to power loss");
  printDebugIfActive(battery_SwAssertion, "ERROR: An internal software assertion has failed");
  printDebugIfActive(battery_CrashEvent, "ERROR: crash signal is detected by HVP");
  printDebugIfActive(battery_OverDchgCurrentFault,
                     "ERROR: Pack discharge current is above the safe max discharge current limit!");
  printDebugIfActive(battery_OverChargeCurrentFault,
                     "ERROR: Pack charge current is above the safe max charge current limit!");
  printDebugIfActive(battery_OverCurrentFault, "ERROR: Pack current (discharge or charge) is above max current limit!");
  printDebugIfActive(battery_OverTemperatureFault,
                     "ERROR: A pack module temperature is above the max temperature limit!");
  printDebugIfActive(battery_OverVoltageFault, "ERROR: A brick voltage is above maximum voltage limit");
  printDebugIfActive(battery_UnderVoltageFault, "ERROR: A brick voltage is below minimum voltage limit");
  printDebugIfActive(battery_PrimaryBmbMiaFault,
                     "ERROR: voltage and temperature readings from primary BMB chain are mia");
  printDebugIfActive(battery_SecondaryBmbMiaFault,
                     "ERROR: voltage and temperature readings from secondary BMB chain are mia");
  printDebugIfActive(battery_BmbMismatchFault,
                     "ERROR: primary and secondary BMB chain readings don't match with each other");
  printDebugIfActive(battery_BmsHviMiaFault, "ERROR: BMS node is mia on HVS or HVI CAN");
  //printDebugIfActive(battery_CpMiaFault, "ERROR: CP node is mia on HVS CAN"); //Uncommented due to not affecting usage
  printDebugIfActive(battery_PcsMiaFault, "ERROR: PCS node is mia on HVS CAN");
  //printDebugIfActive(battery_BmsFault, "ERROR: BmsFault is active"); //Uncommented due to not affecting usage
  printDebugIfActive(battery_PcsFault, "ERROR: PcsFault is active");
  //printDebugIfActive(battery_CpFault, "ERROR: CpFault is active"); //Uncommented due to not affecting usage
  printDebugIfActive(battery_ShuntHwMiaFault, "ERROR: shunt current reading is not available");
  printDebugIfActive(battery_PyroMiaFault, "ERROR: pyro squib is not connected");
  printDebugIfActive(battery_hvsMiaFault, "ERROR: pack contactor hw fault");
  printDebugIfActive(battery_hviMiaFault, "ERROR: FC contactor hw fault");
  printDebugIfActive(battery_Supply12vFault, "ERROR: Low voltage (12V) battery is below minimum voltage threshold");
  printDebugIfActive(battery_VerSupplyFault, "ERROR: Energy reserve voltage supply is below minimum voltage threshold");
  printDebugIfActive(battery_HvilFault, "ERROR: High Voltage Inter Lock fault is detected");
  printDebugIfActive(battery_BmsHvsMiaFault, "ERROR: BMS node is mia on HVS or HVI CAN");
  printDebugIfActive(battery_PackVoltMismatchFault,
                     "ERROR: Pack voltage doesn't match approximately with sum of brick voltages");
  //printDebugIfActive(battery_EnsMiaFault, "ERROR: ENS line is not connected to HVC"); //Uncommented due to not affecting usage
  printDebugIfActive(battery_PackPosCtrArcFault, "ERROR: HVP detectes series arc at pack contactor");
  printDebugIfActive(battery_packNegCtrArcFault, "ERROR: HVP detectes series arc at FC contactor");
  printDebugIfActive(battery_ShuntHwAndBmsMiaFault, "ERROR: ShuntHwAndBmsMiaFault is active");
  printDebugIfActive(battery_fcContHwFault, "ERROR: fcContHwFault is active");
  printDebugIfActive(battery_robinOverVoltageFault, "ERROR: robinOverVoltageFault is active");
  printDebugIfActive(battery_packContHwFault, "ERROR: packContHwFault is active");
  printDebugIfActive(battery_pyroFuseBlown, "ERROR: pyroFuseBlown is active");
  printDebugIfActive(battery_pyroFuseFailedToBlow, "ERROR: pyroFuseFailedToBlow is active");
  //printDebugIfActive(battery_CpilFault, "ERROR: CpilFault is active"); //Uncommented due to not affecting usage
  printDebugIfActive(battery_PackContactorFellOpen, "ERROR: PackContactorFellOpen is active");
  printDebugIfActive(battery_FcContactorFellOpen, "ERROR: FcContactorFellOpen is active");
  printDebugIfActive(battery_packCtrCloseBlocked, "ERROR: packCtrCloseBlocked is active");
  printDebugIfActive(battery_fcCtrCloseBlocked, "ERROR: fcCtrCloseBlocked is active");
  printDebugIfActive(battery_packContactorForceOpen, "ERROR: packContactorForceOpen is active");
  printDebugIfActive(battery_fcContactorForceOpen, "ERROR: fcContactorForceOpen is active");
  printDebugIfActive(battery_dcLinkOverVoltage, "ERROR: dcLinkOverVoltage is active");
  printDebugIfActive(battery_shuntOverTemperature, "ERROR: shuntOverTemperature is active");
  printDebugIfActive(battery_passivePyroDeploy, "ERROR: passivePyroDeploy is active");
  printDebugIfActive(battery_logUploadRequest, "ERROR: logUploadRequest is active");
  printDebugIfActive(battery_packCtrCloseFailed, "ERROR: packCtrCloseFailed is active");
  printDebugIfActive(battery_fcCtrCloseFailed, "ERROR: fcCtrCloseFailed is active");
  printDebugIfActive(battery_shuntThermistorMia, "ERROR: shuntThermistorMia is active");
  // 0x320 800 BMS_alertMatrix
  printDebugIfActive(battery_BMS_a017_SW_Brick_OV, "ERROR: BMS_a017_SW_Brick_OV");
  printDebugIfActive(battery_BMS_a018_SW_Brick_UV, "ERROR: BMS_a018_SW_Brick_UV");
  printDebugIfActive(battery_BMS_a019_SW_Module_OT, "ERROR: BMS_a019_SW_Module_OT");
  printDebugIfActive(battery_BMS_a021_SW_Dr_Limits_Regulation, "ERROR: BMS_a021_SW_Dr_Limits_Regulation");
  //printDebugIfActive(battery_BMS_a022_SW_Over_Current, "ERROR: BMS_a022_SW_Over_Current");
  printDebugIfActive(battery_BMS_a023_SW_Stack_OV, "ERROR: BMS_a023_SW_Stack_OV");
  printDebugIfActive(battery_BMS_a024_SW_Islanded_Brick, "ERROR: BMS_a024_SW_Islanded_Brick");
  printDebugIfActive(battery_BMS_a025_SW_PwrBalance_Anomaly, "ERROR: BMS_a025_SW_PwrBalance_Anomaly");
  printDebugIfActive(battery_BMS_a026_SW_HFCurrent_Anomaly, "ERROR: BMS_a026_SW_HFCurrent_Anomaly");
  printDebugIfActive(battery_BMS_a034_SW_Passive_Isolation, "ERROR: BMS_a034_SW_Passive_Isolation");
  printDebugIfActive(battery_BMS_a035_SW_Isolation, "ERROR: BMS_a035_SW_Isolation");
  printDebugIfActive(battery_BMS_a036_SW_HvpHvilFault, "ERROR: BMS_a036_SW_HvpHvilFault");
  printDebugIfActive(battery_BMS_a037_SW_Flood_Port_Open, "ERROR: BMS_a037_SW_Flood_Port_Open");
  printDebugIfActive(battery_BMS_a039_SW_DC_Link_Over_Voltage, "ERROR: BMS_a039_SW_DC_Link_Over_Voltage");
  printDebugIfActive(battery_BMS_a041_SW_Power_On_Reset, "ERROR: BMS_a041_SW_Power_On_Reset");
  printDebugIfActive(battery_BMS_a042_SW_MPU_Error, "ERROR: BMS_a042_SW_MPU_Error");
  printDebugIfActive(battery_BMS_a043_SW_Watch_Dog_Reset, "ERROR: BMS_a043_SW_Watch_Dog_Reset");
  printDebugIfActive(battery_BMS_a044_SW_Assertion, "ERROR: BMS_a044_SW_Assertion");
  printDebugIfActive(battery_BMS_a045_SW_Exception, "ERROR: BMS_a045_SW_Exception");
  printDebugIfActive(battery_BMS_a046_SW_Task_Stack_Usage, "ERROR: BMS_a046_SW_Task_Stack_Usage");
  printDebugIfActive(battery_BMS_a047_SW_Task_Stack_Overflow, "ERROR: BMS_a047_SW_Task_Stack_Overflow");
  printDebugIfActive(battery_BMS_a048_SW_Log_Upload_Request, "ERROR: BMS_a048_SW_Log_Upload_Request");
  //printDebugIfActive(battery_BMS_a050_SW_Brick_Voltage_MIA, "ERROR: BMS_a050_SW_Brick_Voltage_MIA");
  printDebugIfActive(battery_BMS_a051_SW_HVC_Vref_Bad, "ERROR: BMS_a051_SW_HVC_Vref_Bad");
  printDebugIfActive(battery_BMS_a052_SW_PCS_MIA, "ERROR: BMS_a052_SW_PCS_MIA");
  printDebugIfActive(battery_BMS_a053_SW_ThermalModel_Sanity, "ERROR: BMS_a053_SW_ThermalModel_Sanity");
  printDebugIfActive(battery_BMS_a054_SW_Ver_Supply_Fault, "ERROR: BMS_a054_SW_Ver_Supply_Fault");
  printDebugIfActive(battery_BMS_a059_SW_Pack_Voltage_Sensing, "ERROR: BMS_a059_SW_Pack_Voltage_Sensing");
  printDebugIfActive(battery_BMS_a060_SW_Leakage_Test_Failure, "ERROR: BMS_a060_SW_Leakage_Test_Failure");
  printDebugIfActive(battery_BMS_a061_robinBrickOverVoltage, "ERROR: BMS_a061_robinBrickOverVoltage");
  printDebugIfActive(battery_BMS_a062_SW_BrickV_Imbalance, "ERROR: BMS_a062_SW_BrickV_Imbalance");
  printDebugIfActive(battery_BMS_a063_SW_ChargePort_Fault, "ERROR: BMS_a063_SW_ChargePort_Fault");
  printDebugIfActive(battery_BMS_a064_SW_SOC_Imbalance, "ERROR: BMS_a064_SW_SOC_Imbalance");
  printDebugIfActive(battery_BMS_a069_SW_Low_Power, "ERROR: BMS_a069_SW_Low_Power");
  printDebugIfActive(battery_BMS_a071_SW_SM_TransCon_Not_Met, "ERROR: BMS_a071_SW_SM_TransCon_Not_Met");
  printDebugIfActive(battery_BMS_a075_SW_Chg_Disable_Failure, "ERROR: BMS_a075_SW_Chg_Disable_Failure");
  printDebugIfActive(battery_BMS_a076_SW_Dch_While_Charging, "ERROR: BMS_a076_SW_Dch_While_Charging");
  printDebugIfActive(battery_BMS_a077_SW_Charger_Regulation, "ERROR: BMS_a077_SW_Charger_Regulation");
  printDebugIfActive(battery_BMS_a081_SW_Ctr_Close_Blocked, "ERROR: BMS_a081_SW_Ctr_Close_Blocked");
  printDebugIfActive(battery_BMS_a082_SW_Ctr_Force_Open, "ERROR: BMS_a082_SW_Ctr_Force_Open");
  printDebugIfActive(battery_BMS_a083_SW_Ctr_Close_Failure, "ERROR: BMS_a083_SW_Ctr_Close_Failure");
  printDebugIfActive(battery_BMS_a084_SW_Sleep_Wake_Aborted, "ERROR: BMS_a084_SW_Sleep_Wake_Aborted");
  printDebugIfActive(battery_BMS_a087_SW_Feim_Test_Blocked, "ERROR: BMS_a087_SW_Feim_Test_Blocked");
  printDebugIfActive(battery_BMS_a088_SW_VcFront_MIA_InDrive, "ERROR: BMS_a088_SW_VcFront_MIA_InDrive");
  printDebugIfActive(battery_BMS_a089_SW_VcFront_MIA, "ERROR: BMS_a089_SW_VcFront_MIA");
  //printDebugIfActive(battery_BMS_a090_SW_Gateway_MIA, "ERROR: BMS_a090_SW_Gateway_MIA");
  //printDebugIfActive(battery_BMS_a091_SW_ChargePort_MIA, "ERROR: BMS_a091_SW_ChargePort_MIA");
  //printDebugIfActive(battery_BMS_a092_SW_ChargePort_Mia_On_Hv, "ERROR: BMS_a092_SW_ChargePort_Mia_On_Hv");
  //printDebugIfActive(battery_BMS_a094_SW_Drive_Inverter_MIA, "ERROR: BMS_a094_SW_Drive_Inverter_MIA");
  printDebugIfActive(battery_BMS_a099_SW_BMB_Communication, "ERROR: BMS_a099_SW_BMB_Communication");
  printDebugIfActive(battery_BMS_a105_SW_One_Module_Tsense, "ERROR: BMS_a105_SW_One_Module_Tsense");
  printDebugIfActive(battery_BMS_a106_SW_All_Module_Tsense, "ERROR: BMS_a106_SW_All_Module_Tsense");
  printDebugIfActive(battery_BMS_a107_SW_Stack_Voltage_MIA, "ERROR: BMS_a107_SW_Stack_Voltage_MIA");
  printDebugIfActive(battery_BMS_a121_SW_NVRAM_Config_Error, "ERROR: BMS_a121_SW_NVRAM_Config_Error");
  printDebugIfActive(battery_BMS_a122_SW_BMS_Therm_Irrational, "ERROR: BMS_a122_SW_BMS_Therm_Irrational");
  printDebugIfActive(battery_BMS_a123_SW_Internal_Isolation, "ERROR: BMS_a123_SW_Internal_Isolation");
  printDebugIfActive(battery_BMS_a127_SW_shunt_SNA, "ERROR: BMS_a127_SW_shunt_SNA");
  printDebugIfActive(battery_BMS_a128_SW_shunt_MIA, "ERROR: BMS_a128_SW_shunt_MIA");
  printDebugIfActive(battery_BMS_a129_SW_VSH_Failure, "ERROR: BMS_a129_SW_VSH_Failure");
  printDebugIfActive(battery_BMS_a130_IO_CAN_Error, "ERROR: BMS_a130_IO_CAN_Error");
  printDebugIfActive(battery_BMS_a131_Bleed_FET_Failure, "ERROR: BMS_a131_Bleed_FET_Failure");
  printDebugIfActive(battery_BMS_a132_HW_BMB_OTP_Uncorrctbl, "ERROR: BMS_a132_HW_BMB_OTP_Uncorrctbl");
  printDebugIfActive(battery_BMS_a134_SW_Delayed_Ctr_Off, "ERROR: BMS_a134_SW_Delayed_Ctr_Off");
  printDebugIfActive(battery_BMS_a136_SW_Module_OT_Warning, "ERROR: BMS_a136_SW_Module_OT_Warning");
  printDebugIfActive(battery_BMS_a137_SW_Brick_UV_Warning, "ERROR: BMS_a137_SW_Brick_UV_Warning");
  printDebugIfActive(battery_BMS_a139_SW_DC_Link_V_Irrational, "ERROR: BMS_a139_SW_DC_Link_V_Irrational");
  printDebugIfActive(battery_BMS_a141_SW_BMB_Status_Warning, "ERROR: BMS_a141_SW_BMB_Status_Warning");
  printDebugIfActive(battery_BMS_a144_Hvp_Config_Mismatch, "ERROR: BMS_a144_Hvp_Config_Mismatch");
  printDebugIfActive(battery_BMS_a145_SW_SOC_Change, "ERROR: BMS_a145_SW_SOC_Change");
  printDebugIfActive(battery_BMS_a146_SW_Brick_Overdischarged, "ERROR: BMS_a146_SW_Brick_Overdischarged");
  printDebugIfActive(battery_BMS_a149_SW_Missing_Config_Block, "ERROR: BMS_a149_SW_Missing_Config_Block");
  printDebugIfActive(battery_BMS_a151_SW_external_isolation, "ERROR: BMS_a151_SW_external_isolation");
  printDebugIfActive(battery_BMS_a156_SW_BMB_Vref_bad, "ERROR: BMS_a156_SW_BMB_Vref_bad");
  printDebugIfActive(battery_BMS_a157_SW_HVP_HVS_Comms, "ERROR: BMS_a157_SW_HVP_HVS_Comms");
  printDebugIfActive(battery_BMS_a158_SW_HVP_HVI_Comms, "ERROR: BMS_a158_SW_HVP_HVI_Comms");
  printDebugIfActive(battery_BMS_a159_SW_HVP_ECU_Error, "ERROR: BMS_a159_SW_HVP_ECU_Error");
  printDebugIfActive(battery_BMS_a161_SW_DI_Open_Request, "ERROR: BMS_a161_SW_DI_Open_Request");
  printDebugIfActive(battery_BMS_a162_SW_No_Power_For_Support, "ERROR: BMS_a162_SW_No_Power_For_Support");
  printDebugIfActive(battery_BMS_a163_SW_Contactor_Mismatch, "ERROR: BMS_a163_SW_Contactor_Mismatch");
  printDebugIfActive(battery_BMS_a164_SW_Uncontrolled_Regen, "ERROR: BMS_a164_SW_Uncontrolled_Regen");
  printDebugIfActive(battery_BMS_a165_SW_Pack_Partial_Weld, "ERROR: BMS_a165_SW_Pack_Partial_Weld");
  printDebugIfActive(battery_BMS_a166_SW_Pack_Full_Weld, "ERROR: BMS_a166_SW_Pack_Full_Weld");
  printDebugIfActive(battery_BMS_a167_SW_FC_Partial_Weld, "ERROR: BMS_a167_SW_FC_Partial_Weld");
  printDebugIfActive(battery_BMS_a168_SW_FC_Full_Weld, "ERROR: BMS_a168_SW_FC_Full_Weld");
  printDebugIfActive(battery_BMS_a169_SW_FC_Pack_Weld, "ERROR: BMS_a169_SW_FC_Pack_Weld");
  //printDebugIfActive(battery_BMS_a170_SW_Limp_Mode, "ERROR: BMS_a170_SW_Limp_Mode");
  printDebugIfActive(battery_BMS_a171_SW_Stack_Voltage_Sense, "ERROR: BMS_a171_SW_Stack_Voltage_Sense");
  printDebugIfActive(battery_BMS_a174_SW_Charge_Failure, "ERROR: BMS_a174_SW_Charge_Failure");
  printDebugIfActive(battery_BMS_a176_SW_GracefulPowerOff, "ERROR: BMS_a176_SW_GracefulPowerOff");
  printDebugIfActive(battery_BMS_a179_SW_Hvp_12V_Fault, "ERROR: BMS_a179_SW_Hvp_12V_Fault");
  printDebugIfActive(battery_BMS_a180_SW_ECU_reset_blocked, "ERROR: BMS_a180_SW_ECU_reset_blocked");
}

void printDebugIfActive(uint8_t symbol, const char* message) {
  if (symbol == 1) {
    logging.println(message);
  }
}

void setup_battery(void) {  // Performs one time setup at startup
  datalayer.system.status.battery_allows_contactor_closing = true;

#ifdef TESLA_MODEL_SX_BATTERY  // Always use NCM/A mode on S/X packs
  strncpy(datalayer.system.info.battery_protocol, "Tesla Model S/X", 63);
  datalayer.system.info.battery_protocol[63] = '\0';
  datalayer.battery.info.max_design_voltage_dV = MAX_PACK_VOLTAGE_SX_NCMA;
  datalayer.battery.info.min_design_voltage_dV = MIN_PACK_VOLTAGE_SX_NCMA;
  datalayer.battery.info.max_cell_voltage_mV = MAX_CELL_VOLTAGE_NCA_NCM;
  datalayer.battery.info.min_cell_voltage_mV = MIN_CELL_VOLTAGE_NCA_NCM;
  datalayer.battery.info.max_cell_voltage_deviation_mV = MAX_CELL_DEVIATION_NCA_NCM;
#endif  // TESLA_MODEL_SX_BATTERY

#ifdef TESLA_MODEL_3Y_BATTERY  // Model 3/Y can be either LFP or NCM/A
  strncpy(datalayer.system.info.battery_protocol, "Tesla Model 3/Y", 63);
  datalayer.system.info.battery_protocol[63] = '\0';
#ifdef LFP_CHEMISTRY
  datalayer.battery.info.chemistry = battery_chemistry_enum::LFP;
  datalayer.battery.info.max_design_voltage_dV = MAX_PACK_VOLTAGE_3Y_LFP;
  datalayer.battery.info.min_design_voltage_dV = MIN_PACK_VOLTAGE_3Y_LFP;
  datalayer.battery.info.max_cell_voltage_mV = MAX_CELL_VOLTAGE_LFP;
  datalayer.battery.info.min_cell_voltage_mV = MIN_CELL_VOLTAGE_LFP;
  datalayer.battery.info.max_cell_voltage_deviation_mV = MAX_CELL_DEVIATION_LFP;
#else   // Startup in NCM/A mode
  datalayer.battery.info.max_design_voltage_dV = MAX_PACK_VOLTAGE_3Y_NCMA;
  datalayer.battery.info.min_design_voltage_dV = MIN_PACK_VOLTAGE_3Y_NCMA;
  datalayer.battery.info.max_cell_voltage_mV = MAX_CELL_VOLTAGE_NCA_NCM;
  datalayer.battery.info.min_cell_voltage_mV = MIN_CELL_VOLTAGE_NCA_NCM;
  datalayer.battery.info.max_cell_voltage_deviation_mV = MAX_CELL_DEVIATION_NCA_NCM;
#endif  // !LFP_CHEMISTRY
#endif  // TESLA_MODEL_3Y_BATTERY
}

#endif  // TESLA_BATTERY
