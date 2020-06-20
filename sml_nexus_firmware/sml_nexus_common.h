/*
Library for the nexus 4WD holonomic robot.

TO BE USED ON ARDUINO MEGA

# Author : Robin Baran
# Date   : 2020.6.17
# Ver    : 1

*/

#include <PID_v1.h>
#include <ros.h>
#include <geometry_msgs/Twist.h>
#include <std_msgs/Float32MultiArray.h>
#include <Wire.h>
#include <Adafruit_MotorShield.h>


/******************** Variables ****************/

// Create the motor shield object with the default I2C address
Adafruit_MotorShield AFMS = Adafruit_MotorShield(); 

unsigned long prevUpdateTime;
double long updateOldness;
double long now;

//UR wheel motor
int intCount1 = 0;
#define MOTOR1_ENC_A 3
#define MOTOR1_ENC_B 13
Adafruit_DCMotor *URMotor = AFMS.getMotor(1);

//LR wheel motor
int intCount2 = 0;
#define MOTOR2_ENC_A 19
#define MOTOR2_ENC_B A12
Adafruit_DCMotor *LRMotor = AFMS.getMotor(2);

//LL wheel motor
int intCount3 = 0;
#define MOTOR3_ENC_A 18
#define MOTOR3_ENC_B A14
Adafruit_DCMotor *LLMotor = AFMS.getMotor(3);

//UL wheel motor
int intCount4 = 0;
#define MOTOR4_ENC_A 2
#define MOTOR4_ENC_B 12
Adafruit_DCMotor *ULMotor = AFMS.getMotor(4);

/************ ROS Node Handle ************/
ros::NodeHandle_<ArduinoHardware, 25, 25, 1024, 1024> nh;

double outputPIDUL = 0;
double outputPIDUR = 0;
double outputPIDLL = 0;
double outputPIDLR = 0;

float control_signal;

double measUL, measUR, measLL, measLR;

double L1 = 150;
double L2 = 150;

double long lastReceivedCommTimeout;
double long commTimeout = 500; // ms
double long updateRate = 50.0; // ms
double long vx, vy, w;

float errorUL = 0;
float errorUR = 0;
float errorLL = 0;
float errorLR = 0;

int pwmUL = 0;
int pwmUR = 0;
int pwmLL = 0;
int pwmLR = 0;

double ULspeed = 0;
double URspeed = 0;
double LLspeed = 0;
double LRspeed = 0;

/************ Robot-specific constants ************/
const int encoder_CPR = 1536;
const float wheel_radius = 0.05;  //Wheel radius (in m)
const double speed_to_pwm_ratio = 120;     //Ratio to convert speed (in m/s) to PWM value. It was obtained by plotting the wheel speed in relation to the PWM motor command.

double max_speed = 0.5; //max speed per wheel in m/s
double min_speed = 0.005; //minimum that will stop the motor command if reached, in m/s

//-----------------------------------
// Setup a PID object for each wheel
//-----------------------------------
// PID Parameters, respectively Kp, Ki and Kd
float PID_default_params[] = { 160, 90, 0.5 };
// Feedforward default gain
float feedForwardGainDefault = 80;

float PID_UL_params[3];
float PID_UR_params[3];
float PID_LL_params[3];
float PID_LR_params[3];

float feedForwardGainUL;
float feedForwardGainUR;
float feedForwardGainLL;
float feedForwardGainLR;

//PID(&Input, &Output, &Setpoint, Kp, Ki, Kd, Direction) 
PID PID_UL(&measUL, &outputPIDUL, &ULspeed, PID_default_params[0], PID_default_params[1], PID_default_params[2], DIRECT);
PID PID_UR(&measUR, &outputPIDUR, &URspeed, PID_default_params[0], PID_default_params[1], PID_default_params[2], DIRECT);
PID PID_LL(&measLL, &outputPIDLL, &LLspeed, PID_default_params[0], PID_default_params[1], PID_default_params[2], DIRECT);
PID PID_LR(&measLR, &outputPIDLR, &LRspeed, PID_default_params[0], PID_default_params[1], PID_default_params[2], DIRECT);


////-------------------------------
//// PWM command callback function
////-------------------------------
//void pwmSubCb(const std_msgs::Float32MultiArray& msg){
//  pwmUL = (int)msg.data[0];
//  pwmUR = (int)msg.data[1];
//  pwmLL = (int)msg.data[2];
//  pwmLR = (int)msg.data[3];
//}


/************ Velocity command callback function ************/
void messageCb( const geometry_msgs::Twist& msg){
  vx = (float)msg.linear.x;
  vy = (float)msg.linear.y;
  w = (float)msg.angular.z;

  lastReceivedCommTimeout = millis() + commTimeout;
}

/************ PID tuning callback function ************/
void pidCb( const std_msgs :: Float32MultiArray& msg){
//Receives Kp, Ki and Kd for UL, UR, LL and LR respectively
  //if message has length 12
//  if (msg.data_lenght == 12){
//    PID_UL.SetTunings(msg.data[0], msg.data[1], msg.data[2]);
//    PID_UR.SetTunings(msg.data[3], msg.data[4], msg.data[5]);
//    PID_LL.SetTunings(msg.data[6], msg.data[7], msg.data[8]);
//    PID_LR.SetTunings(msg.data[9], msg.data[10], msg.data[11]);
//  }
}

/************ Sign function ************/
static inline int8_t sgn(int val) {
 if (val < 0) return -1;
 if (val==0) return 0;
 return 1;
}

//------------------------------------
// Setup ROS publishers & subscribers
//------------------------------------
ros::Subscriber<geometry_msgs::Twist> cmd_sub("cmdvel", &messageCb );
//ros::Subscriber<std_msgs :: Float32MultiArray> pid_sub("pid_tuning", &pidCb );
std_msgs :: Float32MultiArray meas_msg;
ros::Publisher meas_speed("velocity", &meas_msg);
//std_msgs :: Float32MultiArray output_msg;
std_msgs :: Float32MultiArray pwm_msg;
//ros::Publisher output_pub("output", &output_msg);
ros::Publisher pwm_pub("pwm", &pwm_msg);


/************ PID parameters and setup function ************/
void setupPIDParams(){
  //------------------------------------------------------------------------------------
  // Get PID parameters from ROS parameter server. If not available, set default params
  //-------------------------------------------------------------------------------------
  if(!nh.getParam("PID_UL", PID_UL_params, 3, 2000)){
    PID_UL_params[0] = PID_default_params[0];
    PID_UL_params[1] = PID_default_params[1];
    PID_UL_params[2] = PID_default_params[2];
    nh.logwarn("UL motor PID: loading default values;");
  }
  if(!nh.getParam("feedforward_UL", &feedForwardGainUL)){
    feedForwardGainUL = feedForwardGainDefault;
    nh.logwarn("UL motor feedforward gain: loading default values;");
  }
  if(!nh.getParam("PID_UR", PID_UR_params, 3, 2000)){
    PID_UR_params[0] = PID_default_params[0];
    PID_UR_params[1] = PID_default_params[1];
    PID_UR_params[2] = PID_default_params[2];
    nh.logwarn("UR motor PID: loading default values;");
  }
  if(!nh.getParam("feedforward_UR", &feedForwardGainUR)){
    feedForwardGainUR = feedForwardGainDefault;
    nh.logwarn("UR motor feedforward gain: loading default values;");
  }
  if(!nh.getParam("PID_LL", PID_LL_params, 3, 2000)){
    PID_LL_params[0] = PID_default_params[0];
    PID_LL_params[1] = PID_default_params[1];
    PID_LL_params[2] = PID_default_params[2];
    nh.logwarn("LL motor PID: loading default values;");
  }
  if(!nh.getParam("feedforward_LL", &feedForwardGainLL)){
    feedForwardGainLL = feedForwardGainDefault;
    nh.logwarn("LL motor feedforward gain: loading default values;");
  }
  if(!nh.getParam("PID_LR", PID_LR_params, 3, 2000)){
    PID_LR_params[0] = PID_default_params[0];
    PID_LR_params[1] = PID_default_params[1];
    PID_LR_params[2] = PID_default_params[2];
    nh.logwarn("LR motor PID: loading default values;");
  }
  if(!nh.getParam("feedforward_LR", &feedForwardGainLR)){
    feedForwardGainLR = feedForwardGainDefault;
    nh.logwarn("LR motor feedforward gain: loading default values;");
  }
    
  //------------------------
  // Setting PID parameters
  //------------------------
  PID_UL.SetTunings(PID_UL_params[0], PID_UL_params[1], PID_UL_params[2]);
  PID_UR.SetTunings(PID_UR_params[0], PID_UR_params[1], PID_UR_params[2]);
  PID_LL.SetTunings(PID_LL_params[0], PID_LL_params[1], PID_LL_params[2]);
  PID_LR.SetTunings(PID_LR_params[0], PID_LR_params[1], PID_LR_params[2]);
  PID_UL.SetSampleTime(50);
  PID_UR.SetSampleTime(50);
  PID_LL.SetSampleTime(50);
  PID_LR.SetSampleTime(50);
  PID_UL.SetOutputLimits(-200, 200);
  PID_UR.SetOutputLimits(-200, 200);
  PID_LL.SetOutputLimits(-200, 200);
  PID_LR.SetOutputLimits(-200, 200);
  PID_UL.SetMode(AUTOMATIC);
  PID_UR.SetMode(AUTOMATIC);
  PID_LL.SetMode(AUTOMATIC);
  PID_LR.SetMode(AUTOMATIC);
}

/************ Setup topics over ROS ************/
void setupCommonTopics(){
  nh.subscribe(cmd_sub);
 // nh.subscribe(pid_sub);
  nh.advertise(meas_speed);
 //nh.advertise(output_pub);
  nh.advertise(pwm_pub);
}
  

/************ Encoders interrupt functions ************/
void encoderM1A(){
  if (digitalRead(MOTOR1_ENC_A) == digitalRead(MOTOR1_ENC_B)) --intCount1;
  else ++intCount1;
}

void encoderM2A(){
  if (digitalRead(MOTOR2_ENC_A) == digitalRead(MOTOR2_ENC_B)) --intCount2;
  else ++intCount2;
}

void encoderM3A(){
  if (digitalRead(MOTOR3_ENC_A) == digitalRead(MOTOR3_ENC_B)) --intCount3;
  else ++intCount3;
}

void encoderM4A(){
  if (digitalRead(MOTOR4_ENC_A) == digitalRead(MOTOR4_ENC_B)) --intCount4;
  else ++intCount4;
}
