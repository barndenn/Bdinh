#include <PID_v1.h>
#include <SW_MCP4017.h>
#include <Wire.h>

// Pinouts for ATmega32U4
// Motor Driver Definitions
#define AIN1 12
#define AIN2 4
#define PWMA 6
#define BIN1 9
#define BIN2 5
#define PWMB 10
#define STBY 8 

// Shaft Encoder Definitions (These need external interrupt pins) 
#define outAR 2 // Right Motor Output
#define outAL 3 // Left Motor Output

// Gear Position Sensor
#define phaR 14 // Right Gear 
#define phaL 15 // Left Gear works

uint8_t gearLeft, gearRight, pastGearLeft, pastGearRight;
uint8_t setupTickL, setupTickR;

// Phase Variables
float totalTicks= 720;
float phaseRight, phaseLeft, phaseError;

int rightPWM = 180; 
int leftPWM = 180;
int maxRightPWM = 200;
int maxLeftPWM = 200;
int minRightPWM = 160;
int minLeftPWM = 160;

volatile long ticksR, ticksL; 

// --------------- PID Controller --------------- 
double SetpointR, InputR, OutputR;
//Specify the links and initial tuning parameters | P,I,D = 2,6,1
PID myPID_R(&InputR, &OutputR, &SetpointR, 2, 6, 1, DIRECT);

uint8_t dpMaxSteps = 128; //remember even thought the the digital pot has 128 steps it looses one on either end (usually cant go all the way to last tick)
int maxRangeOhms = 100000; //this is a 5K potentiometer
MCP4017 i2cDP(MCP4017ADDRESS, dpMaxSteps, maxRangeOhms);

void setup() {
  //pinMode(A4, OUTPUT);
  //i2cDP.setSteps(115);

  Init3DoT();
  attachInterrupt(digitalPinToInterrupt(outAR), countTicksR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(outAL), countTicksL, CHANGE);

  //Sets the left motor to 0^
  do{
   gearLeft = digitalRead(phaL);
   left_forward();
  }while(gearLeft);
  left_stop();
  ticksL = 0;

  //Set the right motor to 0^
  do{
   gearRight = digitalRead(phaR);
   right_forward();
  }while(gearRight);
  right_stop(); //Stop after in position
  ticksR = 0;   //Reset the tick counter

  //Set the right motor to 180^
  do{
   gearRight = digitalRead(phaR);
   right_forward();
  }while(ticksR < 360); //Totalticks/2 = 720/2 = 360
  right_stop();

  InputR = phaseError;
  SetpointR = 128;        //128 is 50% phase or 180^

  // Turn the PID on
  myPID_R.SetMode(AUTOMATIC);
  //Set min and max output limits on the motor
  myPID_R.SetOutputLimits(maxRightPWM, minRightPWM);
  
  delay(1000);


}

void loop() {
  all_forward();
  gearRight = digitalRead(phaR);
  gearLeft = digitalRead(phaL);

  check_phase();

  if (phaseError < 0) phaseError++;

  phaseError = phaseError*255;

  Serial.println(phaseError);

  
  InputR = phaseError; 
  myPID_R.Compute();
  rightPWM = OutputR;

  //Increasing right motor increases the phase error
  //Want phase error to be |0.5|

//  //if phase is greater then 0 increase right speed & decrease left to get to 0.5
//  if (phaseError > 0){
//    rightPWM++;
//    leftPWM--;
//    if (rightPWM > maxRightPWM) rightPWM = maxRightPWM;
//    if (leftPWM < minLeftPWM) leftPWM = minLeftPWM;
//  }
//  
//
//  //if phase is less then 0 increase left speed & decrease right to get to -0.5
//  if (phaseError < 0){
//    rightPWM--;
//    leftPWM++;
//    if (leftPWM > maxLeftPWM) leftPWM = maxLeftPWM;
//    if (rightPWM < minRightPWM) rightPWM = minRightPWM;
//  }
  
  
//  Input = phaseError;
//  myPID_R.Compute();
//
//  rightPWM = Output;

  if (gearRight == 0 && pastGearRight != gearRight){
    Serial.println("Right Phase is zero.");
    Serial.print("Right ticks is:");
    Serial.println(ticksR);
    ticksR = 0;
  }
  if (gearLeft == 0 && pastGearLeft != gearLeft){
    Serial.println("Left Phase is zero.");
    Serial.print("Left ticks is:");
    Serial.println(ticksL);
    ticksL = 0;
  }

  pastGearRight = gearRight;
  pastGearLeft = gearLeft;
}

void check_phase(){
  phaseRight = ticksR/totalTicks;
  phaseLeft = ticksL/totalTicks;

  if (abs(phaseLeft - phaseRight) > 0.5){
    if (phaseLeft > phaseRight){
      phaseRight += 1;
    }
    else{
      phaseLeft += 1;
    }
  }
  phaseError = phaseRight - phaseLeft;
  
//  Serial.println(phaseRight);
//  Serial.println(phaseLeft);
//  phaseError = (abs(phaseLeft-phaseRight) - 0.5) / abs(phaseLeft-phaseRight);
//  phaseError = ((phaseLeft-phaseRight) - 0.5);
//  phaseError = (phaseError+1) * rightPWM;
//
//  if (phaseError > 255) phaseError = 255;
//  if (phaseError < 0) phaseError = 0;

//  Serial.println(phaseError);
}

void all_forward() {
  // Right Motor
  digitalWrite(AIN1,LOW);
  digitalWrite(AIN2,HIGH);
  analogWrite(PWMA,rightPWM);  // Use computed OutputR for PWMA

  // Left Motor
  digitalWrite(BIN1,LOW);
  digitalWrite(BIN2,HIGH);
  analogWrite(PWMB,leftPWM);
  
  digitalWrite(STBY,HIGH);
}

void left_forward(){
  digitalWrite(BIN1,LOW);
  digitalWrite(BIN2,HIGH);
  analogWrite(PWMB,leftPWM);
  digitalWrite(STBY,HIGH);
}

void left_stop(){
  digitalWrite(BIN1,LOW);
  digitalWrite(BIN2,LOW);
  analogWrite(PWMB,leftPWM);
  digitalWrite(STBY,HIGH);
}

void right_forward(){
  digitalWrite(AIN1,LOW);
  digitalWrite(AIN2,HIGH);
  analogWrite(PWMA,rightPWM);
  digitalWrite(STBY,HIGH);
}


void right_stop(){
  digitalWrite(AIN1,LOW);
  digitalWrite(AIN2,LOW);
  analogWrite(PWMA,rightPWM);
  digitalWrite(STBY,HIGH);
}

void countTicksR() {
  ticksR++;
}

void countTicksL() {
  ticksL++;
}
