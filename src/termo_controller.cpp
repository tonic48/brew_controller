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
float deltaValue;
int compressorDelay;
int onOff=1;
int lamp=0;
String mode="P";


OneWire  ds(10); // 11 output sensor 18b20

//Functions declaration
void sendData(float temperature);
void readConfiguration();
float getTemp(); 
void  processInput(String line);   

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

  //Read configuration from eprom
  readConfiguration();
  
  float temperature = getTemp();
  mode="P";
  //Lamp can be controlled every time
  if(lamp==1){
    digitalWrite(OUTLAMP, HIGH);
  }else{
    digitalWrite(OUTLAMP, LOW);
  }

  if(onOff==1){
    //Check the temperature
    if (temperature < targetTemperature-deltaValue) {
      //start heating + fan
      digitalWrite(OUTHEAT, HIGH);
      digitalWrite(OUTFAN, HIGH);
      mode="H";
    }else{
      digitalWrite(OUTCOOL, LOW);
      digitalWrite(OUTFAN, LOW); 
    }
    if (temperature > targetTemperature+deltaValue) {
      //start cooling + FAN
      digitalWrite(OUTCOOL, HIGH);
      digitalWrite(OUTFAN, HIGH);
      mode="C";
    }  else {
      digitalWrite(OUTCOOL, LOW);
      digitalWrite(OUTFAN, LOW);
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
  delay(500);
}

float getTempDummy(){ 
  return 19.2;
}

void error(){ // stop the program
  digitalWrite(OUTHEAT, LOW); // switch off the relays
  digitalWrite(OUTCOOL, LOW);
  digitalWrite(OUTFAN, LOW);
  digitalWrite(OUTLAMP, LOW);
    while(1){ // endless loop
      digitalWrite(13, !digitalRead(13));
      delay(500);
    }  
}
////

float getTemp(){   // get temperature from the sensor
  byte data[12];   // sleep for one second???
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
  
  return raw / 16.0;
}


void sendData(float temperature){
    //send string to the port
  String line="ARDUINO BREWCONTROL;";
  line+=temperature;
  line+=";";
  line+=targetTemperature;
  line+=";";
  line+=deltaValue;
  line+=";";
  line+=compressorDelay;
  line+=";";
  line+=mode;
  line+=";";
  line+=onOff;
  line+=";";
  line+=lamp;
  line+=";";
  
  Serial.println(line);
  delay(2000);
}

void  processInput(String line){
      //Serial.println(line);

     char ch[50]="";

     String out[6];
     line.toCharArray(ch, 50) ;
     char *p = ch;
     char *str;
     int i=0;
     while ((str = strtok_r(p, ";", &p)) != NULL){ // delimiter is the semicolon
        out[i]=str;    
        i++;
     }

     EEPROM.write(1, out[0].toInt());
     EEPROM.write(2, out[1].toInt());
     EEPROM.write(3, out[2].toInt());
     EEPROM.write(4, out[3].toInt());
     EEPROM.write(5, out[4].toInt());
     EEPROM.write(6, out[5].toInt());
     EEPROM.write(7, out[6].toInt());
     EEPROM.write(8, out[7].toInt());
     
     if(out[0]!=NULL){
        float newTemperature=out[0].toFloat();
        //EEPROM.write(1, tempSign);
     }
}

void readConfiguration(){
  
  String tempSignString="";
  int address=1;
  //default temperature 20,0
  int tempSign= EEPROM.read(1);
  if(tempSign==255) {
    tempSign=0;
    EEPROM.write(1, tempSign); 
  }
  if(tempSign==1)tempSignString="-";
  
  address=2; 
  int tempNbr=  EEPROM.read(address);
  if(tempNbr==255) {
    tempNbr=20;
    EEPROM.write(address, tempNbr); 
  }

  address=3 ;
  int tempDec=  EEPROM.read(address);
  if(tempDec==255) {
    tempDec=0;
    EEPROM.write(address, tempDec); 
  }
  String  temperatureString=tempSignString+tempNbr+"."+tempDec;
  targetTemperature=temperatureString.toFloat();

  //read delta

  address=4; 
  int deltaNbr=  EEPROM.read(address);
  if(deltaNbr==255) {
    deltaNbr=0;
    EEPROM.write(address, deltaNbr); 
  }

  address=5 ;
  int deltaDec=  EEPROM.read(address);
  if(deltaDec==255) {
    deltaDec=5;
    EEPROM.write(address, deltaDec); 
  }

   String  deltaString="";
   deltaString=deltaString+deltaNbr+"."+deltaDec;
   deltaValue=deltaString.toFloat();

  //read compressor delay
   address=6; 
  compressorDelay=  EEPROM.read(address);
  if(compressorDelay>10 || compressorDelay<1) {
    compressorDelay=3;
    EEPROM.write(address, compressorDelay); 
  }

   address=7;
   onOff =  EEPROM.read(address);
   if(onOff==255) {
     onOff=1;
     EEPROM.write(address, onOff);
   }

   address=8;
   lamp =  EEPROM.read(address);
   if(lamp==255) {
     lamp=0;
     EEPROM.write(address, lamp);
   }
   
}

void setLow(){

   if(OUTHEAT==HIGH){
      digitalWrite(OUTHEAT, LOW);
    }
    if(OUTCOOL==HIGH){
         digitalWrite(OUTCOOL, LOW);
    }
    if(OUTFAN==HIGH){
         digitalWrite(OUTFAN, LOW);
    }
}


