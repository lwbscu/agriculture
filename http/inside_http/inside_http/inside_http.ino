/***********************************
 *室内1区                          *
 *温湿度、光强、水箱水位、培养槽水位  *
 *水泵、风扇、天窗、LED              *
 ************************************/
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <HTTPClient.h>
#include <SimpleDHT.h>
#include <ESP32Servo.h>
#include <NTPClient.h>

/***************************
 *将ADDR模式设置为GND模式   *
 *ADDR引脚接地             *
 *其中地址为0010011(0x23)  *
 *若引脚接VCC              *
 *其中地址为1101100(0x5B)  *
 **************************/
#define ADDRESS_BH1750FVI 0x23  
#define ONE_TIME_H_RESOLUTION_MODE 0x20

//任务〇(控制)
TaskHandle_t Task0;

const char *ssid = "U NEED CRY DEAR";    //你的网络名称
const char *password = "12345678"; //你的网络密码

const int DHT_Pin=4;          //DHT传感器针脚
const int FlumePin=19;         //培养槽水位传感器针脚
//const int TankPin=41;          //水箱水位传感器
const int BumpPin=17;          //水泵针脚
const int LED_Pin=16;          //LED针脚
const int WindowPin=25;        //天窗控制(舵机) 175->OFF;70->ON
const int FanPin=26;            //风扇针脚



int LightState;
int LightSwitch;
int LightCondition;
int WindowState;
int WindowSwitch;
int WindowTemperature;
int LightIntensity;          //光强
int Temperature;             //温度
int AirHumidity;             //湿度
//int WaterTank;               //水箱水位 目前仅有三档(无法显示详细水位)-0:低水位;1:中水位;2:高水位
int Flume;                   //培养槽水位 目前仅有三档(无法显示详细水位)-0:低水位;1:中水位;2:高水位
int tiandisId = 1;
int BumpTime;
int BumpSwitch;
int BumpState;
unsigned long long BumpStartTime;
unsigned long long BumpInterval;

//声明DHT传感器对象
SimpleDHT11 dht11(DHT_Pin);

//声明ntp
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP,"ntp.aliyun.com");             //NTP地址

//声明舵机对象
Servo myServo;

//I2C高低位数据位
byte HighByte = 0;
byte LowByte = 0;

//温湿度变量
byte temperature = 0;
byte humidity = 0;

//水泵控制的相关变量
typedef struct bump{
  int cmp;                     //比较水泵状态
  int flag;                    //已经过几天
  int time;                    //现在时间
  unsigned long long TimeStamp;//时间戳
}bump;
bump Bump={3,0,-1,0};

//天窗(舵机)控制的相关变量(默认关闭)
typedef struct window{
  int AngleNow;                //舵机目前所处的角度
  int AngleSet;                //设定的角度
} window;
window Window={175,175};

//风扇控制的相关变量(默认关闭)
typedef struct fan{
  int Now;                     //风扇目前状态
  int Set;                     //设定的角度
} fan;
fan Fan={0,0};



//初始化端口
inline void Init(){
  pinMode(FlumePin,INPUT);     //水槽水位传感器

  //pinMode(TankPin,INPUT);      //水箱水位传感器
  pinMode(BumpPin,OUTPUT);     //水泵端口
  pinMode(LED_Pin,OUTPUT);     //LED端口
  pinMode(FanPin,OUTPUT);      //风扇端口
  myServo.attach(WindowPin);   //天窗端口(舵机)
  myServo.write(175);          //将天窗关闭
}


void setup()
{
  Serial.begin(115200);
  Serial.println();
//初始化端口
  Init();

  WiFi.begin(ssid, password);

 //初始化I2C
  Wire.begin();

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected!");

  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  //使用核心0, 控制
  xTaskCreatePinnedToCore(
                    Task0code,   /* Task function. */
                    "Task0",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task0,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */                  
  delay(100);

  //ntp服务
  timeClient.begin();
  timeClient.setTimeOffset(28800); //+1区，偏移3600，+8区，偏移3600*8

  //将培养槽放水时间初始化为5s
  BumpTime=5000;
}


//控制部分
void Task0code(void * pvParameters){
while(1){

/*水泵控制*/
  switch(BumpState){
  /*由单片机控制水泵*/
  case 0:
      //培养槽水量较少
      if(Flume==0){
        digitalWrite(BumpPin,HIGH);
        Bump.TimeStamp=millis();
        while(millis()-Bump.TimeStamp<BumpTime);
        digitalWrite(BumpPin,LOW);
      }
      //培养槽极其缺水
      else if(Flume<50){
        BumpTime=8000;
        digitalWrite(BumpPin,HIGH);
        Bump.TimeStamp=millis();
        while(millis()-Bump.TimeStamp<BumpTime);
        digitalWrite(BumpPin,LOW);
        BumpTime=5000;
      }
      Bump.cmp=3;
      Bump.time=-1;
    break;
  /*手动控制水泵*/
  case 1:
      //比对数据并决定是否更改水泵状态
      if(BumpSwitch!=Bump.cmp){
        Bump.cmp=BumpSwitch;
        if(Bump.cmp){
          digitalWrite(BumpPin,HIGH);
        }
        else{
          digitalWrite(BumpPin,LOW);
        }
      }
      Bump.time=-1;
    break;
  /*计划灌溉*/
  case 2:
      if(Bump.time==-1){
        Bump.flag==BumpInterval;
      }
      int currentHour = timeClient.getHours();
      // Serial.print("Hour:");
      // Serial.println(currentHour);
      int currentMinute = timeClient.getMinutes();
      // Serial.print("Hour:");
      // Serial.println(currentHour);
      //计算当前时间并判断是否需要浇水
      Bump.time=currentHour*60+currentMinute;
      if(Bump.time>=BumpStartTime-1||Bump.time<=BumpStartTime+1){
        Bump.flag++;
      }
      if(Bump.flag>=BumpInterval){
        Bump.TimeStamp=millis();
        digitalWrite(BumpPin,HIGH);
        while(millis()-Bump.TimeStamp<BumpTime);
        digitalWrite(BumpPin,LOW);
        Bump.TimeStamp=millis();
        Bump.flag=0;
      }
      Bump.cmp=3;
    break;
  }

NextModule:
/*LED控制*/
  switch(LightState){
  /*基于光强开启*/
  case 0:
    //光强低于设定值时开启LED
    if(LightIntensity<LightCondition){
      digitalWrite(LED_Pin,HIGH);
    }
    else{
      digitalWrite(LED_Pin,LOW);
    }
    delay(10);
    break;
  /*手动控制*/  
  case 1:
    if(LightSwitch){
      digitalWrite(LED_Pin,HIGH);
    }
    else{
      digitalWrite(LED_Pin,LOW);
    }
    delay(10);
    break;
  }

/*天窗(舵机)控制*/
  switch(WindowState){
  /*基于温度开启*/
  case 0:
    //不低于设定温度时开启
    if(Temperature>=WindowTemperature){
      Window.AngleSet=70;
      if(Window.AngleNow>Window.AngleSet){
        Window.AngleNow--;
        myServo.write(Window.AngleNow);
      }
    }
    else{
      Window.AngleSet=175;
      if(Window.AngleNow<Window.AngleSet){
        Window.AngleNow++;
        myServo.write(Window.AngleNow);
      }
    }
    break;
  /*手动控制*/
  case 1:
    //天窗开
    if(WindowSwitch){
      Window.AngleSet=70;
      if(Window.AngleNow>Window.AngleSet){
        Window.AngleNow--;
        myServo.write(Window.AngleNow);
      }
    }
    //天窗关
    else{
      Window.AngleSet=175;
      if(Window.AngleNow<Window.AngleSet){
        Window.AngleNow++;
        myServo.write(Window.AngleNow);
      }
    }
    break;
  }

/*风扇控制
  switch(ReceiveData.Fan.State){
  //基于温度开启
  case 0:
    //不低于设定温度时开启
    if(SendData.Temperature>=ReceiveData.Fan.Temperature){
      Fan.Set=1;
      if(Fan.Set!=Fan.Now){
        Fan.Now=1;
        digitalWrite(FanPin,HIGH);
      }
    }
    else{
      Fan.Set=0;
      if(Fan.Set!=Fan.Now){
        Fan.Now=0;
        digitalWrite(FanPin,LOW);
      }
    }
    break;
  //手动控制
  case 1:
    //风扇开
    if(ReceiveData.Fan.Switch){
      digitalWrite(FanPin,HIGH);
    }
    else{
      digitalWrite(FanPin,LOW);
    }
    break;
  }*/
 }
}


void loop()
{
  Serial.print("loopbegin!");
//设置GY-30为单次L分辨率模式
  
  Wire.beginTransmission(ADDRESS_BH1750FVI);
  Wire.write(ONE_TIME_H_RESOLUTION_MODE);
  Wire.endTransmission();
  delay(180);

  //读取I2C总线传输的数据并处理(光强数据)
  Wire.requestFrom(ADDRESS_BH1750FVI,2);
  HighByte = Wire.read();
  LowByte = Wire.read();
  LightIntensity=(HighByte<<8)|LowByte;
  LightIntensity/=1.2;

  //读取温湿度数据
  dht11.read(&temperature, &humidity, NULL);
  Temperature=temperature;
  AirHumidity=humidity;

  //读取水箱及培养槽水位
  int temp=analogRead(FlumePin);
  if(temp>3000){
    /*高水位*/
    Flume=2;
  }
  else if(temp<2300){
    /*低水位*/
    Flume=0;
  }
  else{
    /*中水位*/
    Flume=1;
  }
  /*temp=analogRead(TankPin);
  if(temp>3000){
    //高水位
    SendData.WaterTank=2;
  }
  else if(temp<2300){
    //低水位
    SendData.WaterTank=0;
  }
  else{
    中水位
    SendData.WaterTank=1;
  }*/

  Serial.print("httpbegin");
  HTTPClient http; // 声明HTTPClient对象

  http.begin("http-localhost:8080/Control/In/Bump/State"); // 水泵操作模式 0->由单片机自主控制;1->手动控制;2->计划

  int httpCode = http.GET(); // 发起GET请求

  if (httpCode > 0) // 如果状态码大于0说明请求过程无异常
  {
    if (httpCode == HTTP_CODE_OK) // 请求被服务器正常响应，等同于httpCode == 200
    {
      String BState = http.getString(); // 读取服务器返回的响应正文数据
      BumpState = BState.toInt();
      Serial.print("BState=");
      Serial.println(BState);
    }
  }
  else
  {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
  Serial.print("httpon");

  http.begin("http://192.168.16.9:8080/Control/In/Bump/Switch"); // 水泵开关(仅手动控制状态使用) 0->关闭;1->开启
  httpCode = http.GET(); // 发起GET请求

  if (httpCode > 0) // 如果状态码大于0说明请求过程无异常
  {
    if (httpCode == HTTP_CODE_OK) // 请求被服务器正常响应，等同于httpCode == 200
    {
      String BSwitch = http.getString(); // 读取服务器返回的响应正文数据
      BumpSwitch = BSwitch.toInt();
      Serial.print("BSwitch=");
      Serial.println(BSwitch);
    }
  }
  else
  {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();


  http.begin("http://192.168.16.9:8080/Control/In/Bump/Time"); // 单次灌溉时长
  httpCode = http.GET(); // 发起GET请求

  if (httpCode > 0) // 如果状态码大于0说明请求过程无异常
  {
    if (httpCode == HTTP_CODE_OK) // 请求被服务器正常响应，等同于httpCode == 200
    {
      String BTime = http.getString(); // 读取服务器返回的响应正文数据
      BumpTime = BTime.toInt();
      Serial.print("BTime=");
      Serial.println(BTime);
    }
  }
  else
  {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();


  http.begin("http://192.168.16.9:8080/Control/In/Bump/StartTime"); // 每次的启动时间(24h)
  httpCode = http.GET(); // 发起GET请求

  if (httpCode > 0) // 如果状态码大于0说明请求过程无异常
  {
    if (httpCode == HTTP_CODE_OK) // 请求被服务器正常响应，等同于httpCode == 200
    {
      String BStartTime = http.getString(); // 读取服务器返回的响应正文数据
      unsigned long long BumpStartTime = BStartTime.toInt();
      Serial.print("BStartTime=");
      Serial.println(BStartTime);
    }
  }
  else
  {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();

  http.begin("http://192.168.16.9:8080/Control/In/Bump/Interval"); // 计划操作时,间隔时间(单位:ms)
  httpCode = http.GET(); // 发起GET请求

  if (httpCode > 0) // 如果状态码大于0说明请求过程无异常
  {
    if (httpCode == HTTP_CODE_OK) // 请求被服务器正常响应，等同于httpCode == 200
    {
      String BInterval = http.getString(); // 读取服务器返回的响应正文数据
      unsigned long long BumpInterval = BInterval.toInt();
      Serial.print("BInterval=");
      Serial.println(BInterval);
    }
  }
  else
  {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
  
  


  http.begin("http://192.168.16.9:8080/Control/In/LED/State"); // LED操作模式 0->基于光强进行控制;1->手动控制
  httpCode = http.GET(); // 发起GET请求

  if (httpCode > 0) // 如果状态码大于0说明请求过程无异常
  {
    if (httpCode == HTTP_CODE_OK) // 请求被服务器正常响应，等同于httpCode == 200
    {
      String LState = http.getString(); // 读取服务器返回的响应正文数据
      LightState = LState.toInt();
      Serial.print("LState=");
      Serial.println(LState);
    }
  }
  else
  {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();


 http.begin("http://192.168.16.9:8080/Control/In/LED/Switch"); // LED开关(仅手动控制状态使用) 0->关闭;1->开启
  httpCode = http.GET(); // 发起GET请求

  if (httpCode > 0) // 如果状态码大于0说明请求过程无异常
  {
    if (httpCode == HTTP_CODE_OK) // 请求被服务器正常响应，等同于httpCode == 200
    {
      String LSwitch = http.getString(); // 读取服务器返回的响应正文数据
      LightSwitch = LSwitch.toInt();
      Serial.print("LSwitch=");
      Serial.println(LSwitch);
    }
  }
  else
  {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  } 
  http.end();

  http.begin("http://192.168.16.9:8080/Control/In/LED/Condition"); // 设定的开启标准(低于该值时开启)
   httpCode = http.GET(); // 发起GET请求

  if (httpCode > 0) // 如果状态码大于0说明请求过程无异常
  {
    if (httpCode == HTTP_CODE_OK) // 请求被服务器正常响应，等同于httpCode == 200
    {
      String LCondition = http.getString(); // 读取服务器返回的响应正文数据
      LightCondition = LCondition.toInt();
      Serial.print("LCondition=");
      Serial.println(LCondition);
    }
  }
  else
  {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  } 
  http.end();



  http.begin("http://192.168.16.9:8080/Control/In/Window/State"); // 天窗操作模式 1->基于温度进行控制;1->手动控制
  httpCode = http.GET(); // 发起GET请求

  if (httpCode > 0) // 如果状态码大于0说明请求过程无异常
  {
    if (httpCode == HTTP_CODE_OK) // 请求被服务器正常响应，等同于httpCode == 200
    {
      String WState = http.getString(); // 读取服务器返回的响应正文数据
      WindowState = WState.toInt();
      Serial.print("WState=");
      Serial.println(WState);
    }
  }
  else
  {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  } 
  http.end();


  http.begin("http://192.168.16.9:8080/Control/In/Window/Switch"); // 天窗开关(仅手动控制状态使用) 0->关闭;1->开启
  httpCode = http.GET(); // 发起GET请求

  if (httpCode > 0) // 如果状态码大于0说明请求过程无异常
  {
    if (httpCode == HTTP_CODE_OK) // 请求被服务器正常响应，等同于httpCode == 200
    {
      String WSwitch = http.getString(); // 读取服务器返回的响应正文数据
      WindowSwitch = WSwitch.toInt();
      Serial.print("WSwitch=");
      Serial.println(WSwitch);
    }
  }
  else
  {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  } 
  http.end();


  http.begin("http://192.168.16.9:8080/Control/In/Window/Temperature"); // 设定的开启标准(高于此值时开启)
  httpCode = http.GET(); // 发起GET请求
  if (httpCode > 0) // 如果状态码大于0说明请求过程无异常
  {
    if (httpCode == HTTP_CODE_OK) // 请求被服务器正常响应，等同于httpCode == 200
    {
      String WTemperature = http.getString(); // 读取服务器返回的响应正文数据
      WindowTemperature = WTemperature.toInt();
      Serial.print("WTemperature=");
      Serial.println(WTemperature);
    }
  }
  else
  {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  } 
  http.end();



    //int Flume = 2;
    //int tiandisId = 1;
    String url1 = "http://192.168.16.9:8080/Inside/Flume";
    http.begin(url1);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String postData1 = "Flume=" + String(Flume) + "&tiandisId=" + String(tiandisId);
    httpCode = http.POST(postData1); // 发送POST请求
    if (httpCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpCode);
        String response = http.getString();
        Serial.println(response);
    } else {
        Serial.print("Error code: ");
        Serial.println(httpCode);
    }
     http.end();
  
    //int  LightIntensity= 100;
    String url2 = "http://192.168.16.9:8080/Inside/LightIntensity";
    http.begin(url2);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String postData2 = "gzqiangdu=" + String(LightIntensity) + "&tiandisId=" + String(tiandisId);
    httpCode = http.POST(postData2); // 发送POST请求
    if (httpCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpCode);
        String response = http.getString();
        Serial.println(response);
    } else {
        Serial.print("Error code: ");
        Serial.println(httpCode);
    }
    http.end();

    //int Temperature = 25;
    
    String url3 = "http://192.168.16.9:8080/Inside/Temperature";
    http.begin(url3);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String postData3 = "wendu=" + String(Temperature) + "&tiandisId=" + String(tiandisId);
    httpCode = http.POST(postData3); // 发送POST请求
    if (httpCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpCode);
        String response = http.getString();
        Serial.println(response);
    } else {
        Serial.print("Error code: ");
        Serial.println(httpCode);
    }
     http.end();
    //int AirHumidity = 25;
    
    String url4 = "http://192.168.16.9:8080/Inside/AirHumidity";
    http.begin(url4);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String postData4 = "kongqishidu=" + String(AirHumidity) + "&tiandisId=" + String(tiandisId);
    httpCode = http.POST(postData4); // 发送POST请求
    if (httpCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpCode);
        String response = http.getString();
        Serial.println(response);
    } else {
        Serial.print("Error code: ");
        Serial.println(httpCode);
    }

 
  http.end(); // 结束当前连接

////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
Serial.println(Temperature);
delay(5000);
}
