/*
Library for functions related to the 4 URM4 ultrasonic sensors included in the nexus 4WD holonomic robot.

TO BE USED ON ARDUINO MEGA

# Author : Robin Baran
# Date   : 2020.6.17
# Ver    : 1

# Specification
    * Detecting range: 4cm-500cm
    * Resolution     : 1cm
*/

#include "Arduino.h"
#include <sensor_msgs/Range.h>
#include <sensor_msgs/Temperature.h>
#include <std_msgs/Byte.h>
#include <std_msgs/ByteMultiArray.h>
#include <std_msgs/String.h>

//Serial port 2 is used communication with sensor
#define SerialPort Serial2
#define printByte(args) SerialPort.write(args)
#define CommMAXRetry 40

/******************** Variables ****************/
byte sensorReadingStep = 0;
double sensorStepTimer = 0; //ms
double prevSensorTime  = 0; //ms

byte cmdst[10];
char buff[10];

unsigned int sensorData[8];

byte sensorAddresses[]  = {0x11, 0x12, 0x13, 0x14}; //RS-485 addresses of respectively right, front, left and rear sensor
byte sensorTriggerCmd[] = {0x55, 0xaa, 0xff, 0x00, 0x01, 0xff};
byte sensorDistCmd[]    = {0x55, 0xaa, 0xff, 0x00, 0x02, 0xff};
byte sensorTempCmd[]    = {0x55, 0xaa, 0xff, 0x00, 0x03, 0xff};

int sensorCommDelay = 10;     //ms, minimum time to wait for communication with sensor
int sensorTriggerDelay = 40;  //ms, minimum time to wait for data to be available after triggering

sensor_msgs::Range sensorDistMsg;
sensor_msgs::Range leftSensorDistMsg;
sensor_msgs::Range rearSensorDistMsg;
sensor_msgs::Range rightSensorDistMsg;
sensor_msgs::Range frontSensorDistMsg;

sensor_msgs::Temperature leftSensorTempMsg;
sensor_msgs::Temperature rearSensorTempMsg;
sensor_msgs::Temperature rightSensorTempMsg;
sensor_msgs::Temperature frontSensorTempMsg;

std_msgs::Byte sensorStepMsg;
std_msgs::ByteMultiArray sensorCommMsg;
std_msgs::String sensorStatusMsg;

char leftSensorFrame[] = "left_sonar";
char rearSensorFrame[] = "rear_sonar";
char rightSensorFrame[] = "right_sonar";
char frontSensorFrame[] = "front_sonar";

//Setup publishers
ros::Publisher leftSensorDistPub("left_range", &sensorDistMsg);
ros::Publisher rearSensorDistPub("rear_range", &sensorDistMsg);
ros::Publisher rightSensorDistPub("right_range", &sensorDistMsg);
ros::Publisher frontSensorDistPub("front_range", &sensorDistMsg);
ros::Publisher leftSensorTempPub("left_temperature", &leftSensorTempMsg);
ros::Publisher rearSensorTempPub("rear_temperature", &rearSensorTempMsg);
ros::Publisher rightSensorTempPub("right_temperature", &rightSensorTempMsg);
ros::Publisher frontSensorTempPub("front_temperature", &frontSensorTempMsg);
//ros::Publisher sensorStepPub("sensor_step", &sensorStepMsg);
//ros::Publisher sensorStatusPub("sensor_status", &sensorStatusMsg);
//ros::Publisher sensorCommPub("sensor_serial_comm", &sensorCommMsg);

/******************** Functions ****************/
void setupSensorTopics();
void runSensor();
void triggerSensor(int id);
void getDistSensor(int id);
void getTempSensor(int id);
void transmitCommands();
void readSensorData();
void analyzeSensorData(byte cmd[]);
void publishSensorData();

/********* Setup messages and publisher ************/
void setupSensorTopics(){
  
  //Sensor characteristics
  sensorDistMsg.radiation_type = sensorDistMsg.ULTRASOUND;
  sensorDistMsg.field_of_view = 60;
  sensorDistMsg.min_range = 0.04;
  sensorDistMsg.max_range = 5.0;
  
  leftSensorDistMsg = sensorDistMsg;
  rearSensorDistMsg = sensorDistMsg;
  rightSensorDistMsg = sensorDistMsg;
  frontSensorDistMsg = sensorDistMsg;
  
  //Populate header of range messages
  leftSensorDistMsg.header.frame_id = leftSensorFrame;
  rearSensorDistMsg.header.frame_id = rearSensorFrame;
  rightSensorDistMsg.header.frame_id = rightSensorFrame;
  frontSensorDistMsg.header.frame_id = frontSensorFrame;
  //Populate header of temperature messages
  leftSensorTempMsg.header.frame_id = leftSensorFrame;
  rearSensorTempMsg.header.frame_id = rearSensorFrame;
  rightSensorTempMsg.header.frame_id = rightSensorFrame;
  frontSensorTempMsg.header.frame_id = frontSensorFrame;
  //Variance is unknown
  leftSensorTempMsg.variance = 0;
  rearSensorTempMsg.variance = 0;
  rightSensorTempMsg.variance = 0;
  frontSensorTempMsg.variance = 0;

  //Advertise sensors topics
  nh.advertise(leftSensorDistPub);
  nh.advertise(rearSensorDistPub);
  nh.advertise(rightSensorDistPub);
  nh.advertise(frontSensorDistPub); 
  nh.advertise(leftSensorTempPub); 
  nh.advertise(rearSensorTempPub); 
  nh.advertise(rightSensorTempPub); 
  nh.advertise(frontSensorTempPub); 
  //nh.advertise(sensorStepPub);
  //nh.advertise(sensorStatusPub); 

  for(int i = 0 ;i < 10; i++)  cmdst[i] = 0;  //init the URM04 protocol
  SerialPort.begin(19200);               // Init the RS485 interface

  //sensorCommMsg.data_length = 10;
  //sensorCommMsg.data = (byte*)malloc(sizeof(byte)*10);

  //nh.advertise(sensorCommPub);
}

//================================================
// Main function, to call at every loop iteration
//================================================
void runSensor(){
  //sensorStepMsg.data = sensorReadingStep;
  //sensorStepPub.publish(&sensorStepMsg);
  if (millis() - prevSensorTime >= sensorStepTimer){
    switch(sensorReadingStep){
      
      //-----------------
      // Trigger sensors
      //-----------------
      case 0:
        //sensorStatusMsg.data = "Trigger";
        //sensorStatusPub.publish(&sensorStatusMsg);
        //Reset data
        for (int i=0; i<8; i++) sensorData[i] = 65535; //default value is considered erronous and will be treated as such (if no data received from sensor)
        triggerSensor(0);
        triggerSensor(1);
        triggerSensor(2);
        triggerSensor(3);
        sensorStepTimer = sensorTriggerDelay; //Wait for sensor data to be available
        break;

      //--------------
      // Get distance
      //--------------
      // Get distance from right sensor
      case 1:
        //sensorStatusMsg.data = "Dist sensor 1";
        //sensorStatusPub.publish(&sensorStatusMsg);
        getDistSensor(0);
        sensorStepTimer = sensorCommDelay; //Wait for communication with sensor
        break;

      // Get distance from front sensor
      case 2:
        //sensorStatusMsg.data = "Dist sensor 2";
        //sensorStatusPub.publish(&sensorStatusMsg);
        getDistSensor(1);
        sensorStepTimer = sensorCommDelay; //Wait for communication with sensor
        break;

      // Get distance from left sensor
      case 3:
        //sensorStatusMsg.data = "Dist sensor 3";
        //sensorStatusPub.publish(&sensorStatusMsg);
        getDistSensor(2);
        sensorStepTimer = sensorCommDelay; //Wait for communication with sensor
        break;

      // Get distance from rear sensor
      case 4:
        //sensorStatusMsg.data = "Dist sensor 4";
        //sensorStatusPub.publish(&sensorStatusMsg);
        getDistSensor(3);
        sensorStepTimer = sensorCommDelay; //Wait for communication with sensor
        break;

      case 5:
        //sensorStatusMsg.data = "Read dist";
        //sensorStatusPub.publish(&sensorStatusMsg);
        readSensorData();
        sensorStepTimer = 0;
        break;

      //-----------------
      // Get temperature
      //-----------------
      // Get temperature from right sensor
      case 6:
        //sensorStatusMsg.data = "Temp sensor 1";
        //sensorStatusPub.publish(&sensorStatusMsg);
        getTempSensor(0);
        sensorStepTimer = sensorCommDelay; //Wait for communication with sensor
        break;

      // Get temperature from front sensor
      case 7:
        //sensorStatusMsg.data = "Temp sensor 2";
        //sensorStatusPub.publish(&sensorStatusMsg);
        getTempSensor(1);
        sensorStepTimer = sensorCommDelay; //Wait for communication with sensor
        break;

      // Get temperature from left sensor
      case 8:
        //sensorStatusMsg.data = "Temp sensor 3";
        //sensorStatusPub.publish(&sensorStatusMsg);
        getTempSensor(2);
        sensorStepTimer = sensorCommDelay; //Wait for communication with sensor
        break;

      // Get temperature from rear sensor
      case 9:
        //sensorStatusMsg.data = "Temp sensor 4";
        //sensorStatusPub.publish(&sensorStatusMsg);
        getTempSensor(3);
        sensorStepTimer = sensorCommDelay; //Wait for communication with sensor
        break;

      case 10:
        //sensorStatusMsg.data = "Read temp and pub";
        //sensorStatusPub.publish(&sensorStatusMsg);
        readSensorData();
        sensorStepTimer = 0;
      //------------------------------
      // Publish sensor data over ROS
      //------------------------------
        publishSensorData();
        break;

      default:
        sensorReadingStep = 0;   // Finish reading the distance and start a new measuring for the sensor
        break;
      }
    
      if(sensorReadingStep < 10)  sensorReadingStep++;  //step manager
      else sensorReadingStep = 0;

      prevSensorTime = millis();
      
  }
}

/********************* Transmit Command via the RS485 interface ***************/

void triggerSensor(int id){  // The function is used to trigger the measuring
  cmdst[0] = sensorTriggerCmd[0];
  cmdst[1] = sensorTriggerCmd[1];
  cmdst[2] = sensorAddresses[id];
  cmdst[3] = sensorTriggerCmd[3];
  cmdst[4] = sensorTriggerCmd[4];
  transmitCommands();
}
void getDistSensor(int id){  // The function is used to read the distance
  cmdst[0] = sensorDistCmd[0];
  cmdst[1] = sensorDistCmd[1];
  cmdst[2] = sensorAddresses[id];
  cmdst[3] = sensorDistCmd[3];
  cmdst[4] = sensorDistCmd[4];
  transmitCommands();
}
void getTempSensor(int id){  // The function is used to read the temperature
  cmdst[0] = sensorTempCmd[0];
  cmdst[1] = sensorTempCmd[1];
  cmdst[2] = sensorAddresses[id];
  cmdst[3] = sensorTempCmd[3];
  cmdst[4] = sensorTempCmd[4];
  transmitCommands();
}

void transmitCommands(){  // Send protocol via RS485 interface
  cmdst[5]=cmdst[0]+cmdst[1]+cmdst[2]+cmdst[3]+cmdst[4];
  delay(1);
  for(int j = 0; j < 6; j++){
    printByte(cmdst[j]);
  }
  delay(3);
}

/********************* Receive the data and get the distance value from the RS485 interface ***************/

void readSensorData(){
  
  while(SerialPort.available()){
    
    unsigned long timerPoint = millis();
    
    int RetryCounter = 0;
    byte cmdrd[10];
    for(int i = 0 ;i < 10; i++)  cmdrd[i] = 0;
    int i=0;

    boolean flag = true;
    boolean valid = false;
    byte headerNo = 0;

    while(RetryCounter < CommMAXRetry && flag)
    {
      
      if(SerialPort.available()){
        cmdrd[i]= SerialPort.read();

        if(i > 7){
          flag=false;
          SerialPort.flush();
          break;
        }
        if(cmdrd[i] == 0x55){
          headerNo = i;
          valid = true;
        }
        if(valid && i == 7){
          flag = false;
          break;
        }
        if(valid){
          i ++;
        }
        RetryCounter = 0;
      }
      else{
        RetryCounter++;
        delayMicroseconds(15);
      }
    }
    
    if(valid)  analyzeSensorData(cmdrd);   
  }
  
}

void analyzeSensorData(byte cmd[]){
  byte sumCheck = 0;
  byte id = 255;
  for(int h = 0;h < 7; h ++)  sumCheck += cmd[h];

  //If message sum check is passed
  if(sumCheck == cmd[7]){
    //Get sensor id
    if(cmd[2] == sensorAddresses[0]){
      id = 0;
    }
    else if(cmd[2] == sensorAddresses[1]){
      id = 1;
    }
    else if(cmd[2] == sensorAddresses[2]){
      id = 2;
    }
    else if(cmd[2] == sensorAddresses[3]){
      id = 3;
    }
    //unknown sensor, leave id 255

    if(id != 255){
      //If distance message
      if(sumCheck == cmd[7] && cmd[3] == 2 && cmd[4] == 2){
        //Get distance value from message
        sensorData[id] = cmd[5] * 256 + cmd[6];
      }
      //If temperature message
      else if (sumCheck == cmd[7] && cmd[3] == 2 && cmd[4] == 3){
        //Get temperature value from message
        sensorData[id+4] = (cmd[5]*256 + cmd[6])/10;
      }
      //If unknown message, leave 65535 value
    }
  }
}
  
void publishSensorData(){
  //If data from the sensor is available, send message over ROS
  //sensorData array from 0 to 3 is distance
  if(sensorData[0] != 65535){
  //if(true){ 
    rightSensorDistMsg.header.stamp = nh.now();
    rightSensorDistMsg.range = sensorData[0];
    rightSensorDistPub.publish(&rightSensorDistMsg);
  }
  if(sensorData[1] != 65535){
  //if(true){
    frontSensorDistMsg.header.stamp = nh.now();
    frontSensorDistMsg.range = sensorData[1];
    frontSensorDistPub.publish(&frontSensorDistMsg);
  }
  if(sensorData[2] != 65535){
  //if(true){
    leftSensorDistMsg.header.stamp = nh.now();
    leftSensorDistMsg.range = sensorData[2];
    leftSensorDistPub.publish(&leftSensorDistMsg);
  }
  if(sensorData[3] != 65535){
  //if(true){
    rearSensorDistMsg.header.stamp = nh.now();
    rearSensorDistMsg.range = sensorData[3];
    rearSensorDistPub.publish(&rearSensorDistMsg);
  }
  //sensorData array from 4 to 7 is temperature
  if(sensorData[4] != 65535){
  //if(true){
    rightSensorTempMsg.header.stamp = nh.now();
    rightSensorTempMsg.temperature = sensorData[4];
    rightSensorTempPub.publish(&rightSensorTempMsg);
  }
  if(sensorData[5] != 65535){
  //if(true){
    frontSensorTempMsg.header.stamp = nh.now();
    frontSensorTempMsg.temperature = sensorData[5];
    frontSensorTempPub.publish(&frontSensorTempMsg);
  }
  if(sensorData[6] != 65535){
  //if(true){
    leftSensorTempMsg.header.stamp = nh.now();
    leftSensorTempMsg.temperature = sensorData[6];
    leftSensorTempPub.publish(&leftSensorTempMsg);
  }
  if(sensorData[7] != 65535){
  //if(true){
    rearSensorTempMsg.header.stamp = nh.now();
    rearSensorTempMsg.temperature = sensorData[7];
    rearSensorTempPub.publish(&rearSensorTempMsg);
  }
  
}
