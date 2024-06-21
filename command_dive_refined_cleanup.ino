// Code to connect to an arduino via BTLE and control a linear actuator while logging dive data in a dive bladder device.


// Include the nRF51 SPI Library
#include "Adafruit_BluefruitLE_SPI.h"
// wire library is for I2c connections
#include <Wire.h>
// MS5837 Library is for BlueRobotics Bar02 sensor
#include "MS5837.h"
MS5837 sensor;
// On the 32U4 Feather board, the nRF51 chip is connected to the
// 32U4 hardware SPI pins. When we create the BTLE object, we tell
// it what pins are used for CS (8), IRQ (7), and RST (4):
Adafruit_BluefruitLE_SPI btle(8, 7, 4);

// Set this to true for one time configuration
// This performs a factory reset, then changes the broadcast name.
// There is no harm in redoing this at each boot (leave true).
// You can set this to false after you have programmed the module one time.
const bool PERFORM_CONFIGURATION = true; 

// This is how the BTLE device will identify itself to your smartphone
const String BTLE_NAME = "ROVrunner VP";

// On-Board LED is connected to Pin 13
const int STATUS_LED = 13;

// Pin to be controlled is connected to Pin 5
const int CTRL_Pin = 5;

//Expand pin
const int expin = 12;

// Contract pin
const int conpin = 11;

//Array and Loop Variables
float data[120][2];       // creates array with 120 spaces for data 
int count = 0;            // sets count for array pointer
int down = 3;             // time period to contract actuator
int up = 6;               // time period to expand actuator
int down2 = 9;            //time period to contract second cycle
int up2 = 12;             // time period to expand second cycle
int wait = 15;            // time period after which actuator holds but data is still logged
float zero = 10.36;       // sets the zero offset for sensor reading

// Variables to keep track of LED state
bool Pin_state = LOW;
String cmd = "";
String reply = "";

//  variables to track current command 
bool diving = false;
bool transmiting = false;


void setup(void)
{
  // Set LEDs as outputs and turn off
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);
  pinMode(CTRL_Pin, OUTPUT);
  digitalWrite(CTRL_Pin, Pin_state);

  //Expand pin. 2 Channel relay is set to closed with signal from pin. Power draw is less from arduino and produces less heat within capsul. 
  pinMode(expin,OUTPUT);
  digitalWrite(expin,HIGH);

  // Contract pin
  pinMode(conpin, OUTPUT);
  digitalWrite(conpin,HIGH);
  
  // We'll print debug info to the Serial console.
  Serial.begin(9600);

  // Connect to the module.
  Serial.print(F("Initializing BTLE Module..."));
  if (!btle.begin())
  {
    Serial.println("");
    Serial.println(F("Couldn't connect to nRF51 Module."));
    while(1);
  }
  Serial.println(F("Ready!"));

  // Reset the BTLE chip to factory defaults if specified.
  // You can trigger this to recover from any programming errors you
  // make that render the module unresponsive.
  // After doing factory reset of the module, it sets its broadcast name
  if (PERFORM_CONFIGURATION)
  {
    // Reset to defaults
    Serial.print(F("Resetting to Defaults..."));
    if (!btle.factoryReset())
    {
      Serial.println("");
      Serial.println(F("Couldn't reset module."));
      while(1);
    }
    Serial.println(F("Done!"));

    // Set the name to be broadcast using an AT Command
    Serial.print(F("Setting Device name..."));
    btle.print(F("AT+GAPDEVNAME="));
    btle.println(BTLE_NAME);
    if (!btle.waitForOK())
    {
      Serial.println(F("Could not set name."));
      while(1);
    }
    btle.reset(); // Restart the module for new name to take effect
    Serial.println(F("Done!"));
  }
  
  //Switch to Data mode (from command mode)
  btle.setMode(BLUEFRUIT_MODE_DATA);

  // Initialize Sensor

  Serial.println("Starting Sensor");
  btle.println("Starting Sensor");
  
  Wire.begin();

  // Initialize pressure sensor
  // Returns true if initialization was successful
  // We can't continue with the rest of the program unless we can initialize the sensor
  while (!sensor.init()) {
    Serial.println("Init failed!");
    Serial.println("Are SDA/SCL connected correctly?");
    Serial.println("Blue Robotics Bar30: White=SDA, Green=SCL");
    Serial.println("\n\n\n");
    btle.println("Init failed!");
    btle.println("Are SDA/SCL connected correctly?");
    btle.println("Blue Robotics Bar30: White=SDA, Green=SCL");
    btle.println("\n\n\n");
    delay(500);
  }

  
  sensor.setModel(MS5837::MS5837_02BA);         // Line is important for Bar02 sensor. If Bar30 change to MS5837_30BA
  sensor.setFluidDensity(997);                  // kg/m^3 (freshwater, 1029 for seawater)*/
  
}

void dive()
{
  Pin_state = HIGH;  //Changes state of CTRL_Pin
  diving = true;
}

void transmit()
{
  Pin_state = LOW; //Changes state of CTRL_Pin
  transmiting = true; 
}


// creates function that reads commands from phone
// "dive" command enters VP into dive loop which logs data over two expand contract cycles.
// "transmit" command enters VP into loop which transmits array of data logged over the course of "dive" loop.
void processBLECommand()
{
  while (btle.available() > 0)
  {
    // Blink the Status LED when we receive a request
   digitalWrite(STATUS_LED, HIGH);
  
    // Read the receive buffer until there is a newline
    cmd = btle.readStringUntil('\n');
    
    Serial.print(F("Received Command: "));
    Serial.println(cmd);

    // Makes it lower case so we recognize the command regardless of caps
   cmd.toLowerCase();

    // Parse commands with the word "dive" or "transmit"
    if (cmd.indexOf(F("dive")) != -1 || cmd.indexOf(F("transmit")) != -1)
    {
      // Command contains "dive"
      if (cmd.indexOf(F("dive")) != -1)
      {
        dive();
        transmiting = false;
        reply = F("OK! The VP will dive.");
      }
    
      // Command contains "transmit"
      else if (cmd.indexOf(F("transmit")) != -1)
      {
        transmit();
        diving = false;
        reply = F("OK! The VP will transmit data.");
      }

      else 
      {
        if (Pin_state) reply = F("The LED is currently on.");
        else reply = F("The LED is currently off.");
      }

      // Set the LED state
      digitalWrite(CTRL_Pin, Pin_state);    // writes changes to CTRL_Pin from commands.
    }
    else
    {
      reply = F("Command not understood.");
    }

    // Acknowledge Command
    btle.println(reply);
    Serial.print(F("Replied With: "));
    Serial.println(reply);
    btle.waitForOK();
    digitalWrite(STATUS_LED, LOW);
  }
}
 
void loop(void)
{
  // If there is incoming data available, read and parse it
  while (btle.available() > 0)
  {
    processBLECommand();
   //DIVE loop
   if (diving)
    { 
      int dly = 30000;                         // sets delay time (ms)

      sensor.read();                          // sensor reads incoming data
        
      float depth = (sensor.depth()+zero );   // Get the Value of the sensor

      float pressure = (sensor.pressure());   // get the pressure value from the sensor
        
      data[count][0] = depth;                 // logs value of sensor to next space in data array [count: row] [depth: column]

      data[count][1] = pressure;              // logs the value of the sensor to next space in data array [count: row] [pressure: column]
        
      data[count][2] = millis();              // logs time to next space in data array [count: row] [time: column]
      
      for (int i = 0; i <= count; i++) // sets up for loop to move through all logged points of data
      {  
        // Send the data to the BTLE module to be sent via BTLE Serial 
        float x = data[i][0];       //Depth data   

        float y = data[i][1];        //pressure data in mbar
        
        float z = data[i][2]/1000;  //Division by 1000 converts milliseconds to seconds.

        btle.print("EX11:  ");
      
        btle.print("depth:  ");

        btle.print(x);
        
        btle.print(" , pressure(mbar):  ");
        
        btle.print(y);

        btle.print(" , time(s):  ");

        btle.print(z);
        
        //Serial.print is for debugging. Comment out or delete during application
        /*Serial.print("Data: ");
        Serial.print(x);
        Serial.print(" , ");
        Serial.println(y);*/
        btle.waitForOK();
      }
      count = count + 1;                      // increases count number

      //  dive sequence contract  
      while(count < down)                       
      {   
        digitalWrite(conpin,LOW);               // sends signal to relay that contracts linear actuator
        
        delay(dly);                            // delay 30 seconds

        sensor.read();                          // sensor reads incoming data
        
        float depth = (sensor.depth()+zero );   // Get the Value of the sensor

        float pressure = (sensor.pressure());   // get the pressure value from the sensor
        
        data[count][0] = depth;                 // logs value of sensor to next space in data array [count: row] [depth: column]

        data[count][1] = pressure;              // logs the value of the sensor to next space in data array [count: row] [pressure: column]
        
        data[count][2] = millis();              // logs time to next space in data array [count: row] [time: column]
        
        count = count + 1;                      // increases count number
        
       
        
        //Serial.println("EX10 " + String(data[count][2]) + " miliseconds " + String(data[count][1]) ); //line left for debugging purposes. No purpose in BTLE ops.
      }
      
      // expand linear actuator
      while (count >= down && count < up)        
      {
        digitalWrite(conpin,HIGH);               // ends contract signal

        digitalWrite(expin,LOW);                 //sends signal to relay to expand linear actuator

        delay(dly);                             // delay 1 seconds

        sensor.read();                           // sensor reads incoming data

        float depth = (sensor.depth()+zero );   // Get the Value of the sensor

        float pressure = (sensor.pressure());   // get the pressure value from the sensor

        data[count][0] = depth;                 // logs value of sensor to next space in data array [count: row] [depth: column]

        data[count][1] = pressure;              // logs the value of the sensor to next space in data array [count: row] [pressure: column]

        data[count][2] = millis();              // logs time to next space in data array [count: row] [time: column]

        count = count + 1;                       // increases count number 

 

        //Serial.println("EX10 " + String(data[count][2]) + " miliseconds " + String(data[count][1]) ); //line left for debugging purposes. No purpose in BTLE ops.
      }   
      
      // contract linear actuator
      while (count >= up && count < down2)        
      {
        digitalWrite(conpin,LOW);                 //sends signal to relay that contracts linear actuator

        digitalWrite(expin,HIGH);                 //ends expand signal

        delay(dly);                              // delay 1 seconds

        sensor.read();                            // sensor reads incoming data
      
        float depth = (sensor.depth()+zero );   // Get the Value of the sensor

        float pressure = (sensor.pressure());   // get the pressure value from the sensor

        data[count][0] = depth;                 // logs value of sensor to next space in data array [count: row] [depth: column]

        data[count][1] = pressure;              // logs the value of the sensor to next space in data array [count: row] [pressure: column]

        data[count][2] = millis();              // logs time to next space in data array [count: row] [time: column]

        count = count + 1;                        // increases count number

       
      
        //Serial.println("EX10 " + String(data[count][2]) + " miliseconds " + String(data[count][1]) ); //line left for debugging purposes. No purpose in BTLE ops.
      }
      
      // expand linear actuator
      while (count >= down2 && count < up2)       
      {
        digitalWrite(conpin,HIGH);                // ends contract signal

        digitalWrite(expin,LOW);                  //sends signal to relay to expand linear actuator

        delay(dly);                              // delay 1 seconds

        sensor.read();                            // sensor reads incoming data

        float depth = (sensor.depth()+zero );   // Get the Value of the sensor

        float pressure = (sensor.pressure());   // get the pressure value from the sensor

        data[count][0] = depth;                 // logs value of sensor to next space in data array [count: row] [depth: column]

        data[count][1] = pressure;              // logs the value of the sensor to next space in data array [count: row] [pressure: column]

        data[count][2] = millis();              // logs time to next space in data array [count: row] [time: column]

        count = count + 1;                        // increases count number

        

        //Serial.println("EX10 " + String(data[count][2]) + " miliseconds " + String(data[count][1]) );  //line left for debugging purposes. No purpose in BTLE ops.    
      }   
      while (count >= up2 && count < wait)        //log data until wait time
      {
        digitalWrite(expin,HIGH);                 // relay recieves no signal. Sets to neutral position and prevents battery drain.
        
        delay(dly);                              // delay 1 seconds

        sensor.read();                            // sensor reads incoming data
        
        float depth = (sensor.depth()+zero );   // Get the Value of the sensor

        float pressure = (sensor.pressure());   // get the pressure value from the sensor

        data[count][0] = depth;                 // logs value of sensor to next space in data array [count: row] [depth: column]

        data[count][1] = pressure;              // logs the value of the sensor to next space in data array [count: row] [pressure: column]

        data[count][2] = millis();              // logs time to next space in data array [count: row] [time: column]

        count = count + 1;                        // increases count number

        

        //Serial.println("EX10 " + String(data[count][2]) + " miliseconds " + String(data[count][1]) ); //line left for debugging purposes. No purpose in BTLE ops.
      }
      
      // Hold until connection is re-established
      while(count>= wait)             
      {
        if(btle.isConnected())
        {
         delay(500);
         
         break;
        }
      }
    }

    // Transmit loop to send data
    if(transmiting)
    {
      for (int i = 0; i <= count; i++) // sets up for loop to move through all logged points of data
      {  
        // Send the data to the BTLE module to be sent via BTLE Serial 
        float x = data[i][1];       //Depth data   
        
        float y = data[i][2]/1000;  //Division by 1000 converts milliseconds to seconds.
      
        btle.print("EX11:  ");
      
        btle.print(x);
        
        btle.print(" , ");
        
        btle.print(y);
        
        //Serial.print is for debugging. Comment out or delete during application
        /*Serial.print("Data: ");
        Serial.print(x);
        Serial.print(" , ");
        Serial.println(y);*/
        btle.waitForOK();
      }
      btle.println("End of Data");

      Serial.println("End of Data");
      
      count = 0;      //resets count to zero for next dive sequence
    }
  }
}
