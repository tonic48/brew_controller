#include <DallasTemperature.h>
#include <OneWire.h>
#include <EEPROM.h>
#include <Arduino.h>


#define OUTHEAT 2 // Relays output 
#define OUTCOOL 3
#define OUTFAN 4
#define OUTLAMP 5

int led = 13; 

float targetTemperature;
unsigned int deltaValue;
unsigned int compressorDelay;
float tempCalibration;
int onOff=1;
int lamp=0;
String mode="P";
unsigned long lastCompressorOffTime=0;
boolean allowCooling();

const int numberParms=10;
int eepromValues[numberParms];




OneWire  ds(10); // 11 output sensor 18b20

//Functions declaration
void sendData(double temperature);
void readConfiguration();
float  getTemp(); 
void  processInput(String line);  
void setLow();
void controlLamp();
void coolingOn();
void coolingOff();
void heatingOn();
void heatingOff();
void fanOn();
void fanOff();

void setup() {
  Serial.begin(9600); 
  Serial.flush();
  
  pinMode(led, OUTPUT); 
  pinMode(OUTHEAT, OUTPUT);
  pinMode(OUTCOOL, OUTPUT);
  pinMode(OUTFAN, OUTPUT);
  pinMode(OUTLAMP, OUTPUT);

} 

void loop() {  
  //reset mode "Parking"
  mode="P";
  //Read configuration from eprom
  readConfiguration();
  
  float temperature = getTemp();
  controlLamp();

  if(onOff==1){
    //Check the temperature
    if (temperature <= targetTemperature-deltaValue) {
      //start heating + fan
      coolingOff();
      heatingOn();
      mode="H";
    }else{
      heatingOff();
    }
    if (temperature >= targetTemperature+deltaValue) {
      //start cooling + FAN
      if(allowCooling()){
        heatingOff();
        coolingOn();
        //Mode cooling   
        mode="C";
      }else{
          //Mode Lock (Compressor protection)
          mode="L";
      }   
    }  else {
      coolingOff();

    }
  }else{
    setLow();
  }

  sendData(temperature);
  
  //read from the port
  if (Serial.available()!=0){
  
    delay(50);//waiting untill data will be ready
    String incomingChar = Serial.readString();  
    processInput(incomingChar);
   
  }
  delay(2000);
}

float getTempDummy(){ 
  return 19.2;
}

boolean allowCooling(){
   // compessor delay protection
  boolean retCode=true;
  unsigned long curMills=millis();
   //Serial.println(curMills);
  if(lastCompressorOffTime>0){
    if(curMills/1000-lastCompressorOffTime/1000<compressorDelay){
        retCode=false;
    }
  }

  return retCode;

}

void error(){ // stop the program
  digitalWrite(OUTHEAT, LOW); // switch off the relays
  digitalWrite(OUTCOOL, LOW);
  digitalWrite(OUTFAN, LOW);
  digitalWrite(OUTLAMP, LOW);
    while(1){ // endless loop
      digitalWrite(led, !digitalRead(led));
      delay(500);
    }  
}
////

float getTemp(){  // get temperature from the sensor
  byte data[12];   
  byte addr[8];  
  
  if (!ds.search(addr)) {
    // Serial.println("No more addresses.");
    //error(); 
  }
  
  ds.reset_search(); 
 
  if (OneWire::crc8(addr, 7) != addr[7]) {
     Serial.println("CRC is not valid!");
     //error();   
  }
  
  ds.reset();
  ds.select(addr);        
  ds.write(0x44);      
  delay(1000);   
  
  ds.reset();
  ds.select(addr);    
  ds.write(0xBE);          

  for (int i = 0; i < 9; i++) data[i] = ds.read(); 
  int raw = (data[1] << 8) | data[0]; //transform into temperature 
  if (data[7] == 0x10) raw = (raw & 0xFFF0) + 12 - data[6];  
  
  return round(raw / 16.0 *10.0) / 10.0 + tempCalibration;
} 


void sendData(double temperature){
    //send string to the port
  String line="ARDUINO BREWCONTROL;";
  line+=String(onOff);
  line+=";";
  line+=String(temperature);
  line+=";";
  line+=String(targetTemperature);
  line+=";";
  line+=String(deltaValue);
  line+=";";
  line+=String(compressorDelay);
  line+=";";
  line+=String(tempCalibration);
  line+=";";
  line+=mode;
  line+=";";
  line+=String(lamp);
  line+=";";
  Serial.println(line);
  delay(2000);
}
 
void  processInput(String line){
      //Serial.println("Reading data");

     char ch[50]="";

     String out[numberParms];
     line.toCharArray(ch, 50) ;
     char *p = ch;
     char *str;
     int i=0;
     while ((str = strtok_r(p, ";", &p)) != NULL){ // delimiter is the semicolon
        out[i]=str;    
        i++;
     }

    /*EEPROM addresses
      0 -  main switch status (0 - OFF, 1 - ON)
      1 -  target temperature sign (1 => '-' , 0 => '+')
      2 -  target temperature integers (50-99)
      3 -  target temperarure decimals  (0-9)
      4 -  difference set value (1-10) C
      5 -  compressor delay value (1-10) minutes
      6 -  temperature callibration value sign (0 => '-' , 1 => '+')
      7 -  temperature callibration value integers (0-10)
      8 -  temperature callibration value decimals (0-10)
      9 -  lamp switch status  (0 - OFF, 1 - ON)
    */

   for (unsigned int i = 0; i < numberParms; i++){
     int inputVal=out[i].toInt();
     if(inputVal!=eepromValues[i]){
       //Write eeprom only when value has chanded
       EEPROM.write(i, inputVal);

     }
   }   
}

void readConfiguration(){

  String tempSignString="";
  int tempSign=0;  
  int tempNbr=0;
  int tempDec=0;

  String tempCalibrSignString="";
  int tempCalibrNbr=0;
  int tempCalibrDec=0;

  for (int i = 0; i < numberParms; i++){

    switch (i) {
      case 0:
        onOff =  EEPROM.read(i);
        if(onOff==255) {
          onOff=0;
          EEPROM.write(i, onOff);
        }
        eepromValues[i]=onOff;
        break;
      case 1:
          tempSign= EEPROM.read(i);
          if(tempSign==255) { 
            tempSign=0;
            EEPROM.write(i, tempSign); 
          }

        if(tempSign==1)tempSignString="-";
        eepromValues[i]=tempSign;
         break;
   
      case 2:
        tempNbr=  EEPROM.read(i);
        if(tempNbr==255) {
          tempNbr=20;
          EEPROM.write(i, tempNbr); 
        }
        eepromValues[i]=tempNbr;
        break;
      case 3:
        tempDec=  EEPROM.read(i);
        if(tempDec==255) {
          tempDec=0;
          EEPROM.write(i, tempDec); 
        }
        eepromValues[i]=tempDec;
        break;
      case 4:
        deltaValue =  EEPROM.read(i);
          if(deltaValue==255) {
            deltaValue=0;
            EEPROM.write(i, deltaValue); 
          }
        eepromValues[i]=deltaValue;
        break;
      case 5:
        compressorDelay=  EEPROM.read(i);
          if(compressorDelay>10 || compressorDelay<1) {
            compressorDelay=3;
            EEPROM.write(i, compressorDelay); 
          }
          eepromValues[i]=compressorDelay;
        break;
      case 6:
          tempSign= EEPROM.read(i);
          if(tempSign==255) { 
            tempSign=0;
            EEPROM.write(i, tempSign); 
          }

        if(tempSign==1)tempCalibrSignString="-";
        eepromValues[i]=tempSign;
        break;
      case 7:
        tempCalibrNbr=  EEPROM.read(i);
        if(tempCalibrNbr==255) {
          tempCalibrNbr=0;
          EEPROM.write(i, tempCalibrNbr); 
        }
        eepromValues[i]=tempCalibrNbr;
        break;
      case 8:
        tempCalibrDec=  EEPROM.read(i);
        if(tempCalibrDec==255) {
          tempCalibrDec=0;
          EEPROM.write(i, tempCalibrDec); 
        }
        eepromValues[i]=tempCalibrDec;
        break;
      case 9:
        lamp =  EEPROM.read(i);
        if(lamp==255) {
          lamp=0;
          EEPROM.write(i, lamp);
        }
         eepromValues[i]=lamp;
        break;
      default:
        // if nothing else matches, do the default
        // default is optional
        break;
    }

  }
  //String  temperatureString=;
  targetTemperature=(tempSignString+tempNbr+"."+tempDec).toFloat();
  tempCalibration=(tempCalibrSignString+tempCalibrNbr+"."+tempCalibrDec).toFloat();
   
}

void coolingOff(){
   digitalWrite(OUTCOOL, LOW);
   if(OUTCOOL==HIGH){
      
      lastCompressorOffTime=millis();
    }
    fanOff();
}

void coolingOn(){
   digitalWrite(OUTCOOL, HIGH);
  if(OUTCOOL==LOW){
               
  }
  fanOn();
}

void heatingOff(){
  digitalWrite(OUTHEAT, LOW);
  if(OUTHEAT==HIGH){
      
  }
  fanOff();
}
void heatingOn(){
   digitalWrite(OUTHEAT, HIGH);
   if(OUTHEAT==LOW){
     
  }
  fanOn();
}

void fanOff(){
 if(OUTFAN==HIGH){
      digitalWrite(OUTFAN, LOW);
  }
}

void fanOn(){
 if(OUTFAN==LOW){
    digitalWrite(OUTFAN, HIGH); 
  }
}

void setLow(){
  coolingOff();
  heatingOff();
}

void controlLamp(){
  //Lamp can be controlled every time
  if(lamp==1){
    if(OUTLAMP==LOW){
        digitalWrite(OUTLAMP, HIGH);
    }
    
  }else{
    if(OUTLAMP==HIGH){
      digitalWrite(OUTLAMP, LOW);
    }
    
  }

}


