/*******************
 *2区-ESP32        *
 *室外土壤湿度      *
 *管理室外水泵      *
 *Ciallo :)        *
 *******************/
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <HTTPClient.h>

//任务〇(数据)
TaskHandle_t Task0;

int SoilMoisture;            //土壤湿度

 
int BumpState;                   //水泵操作模式 0->由单片机自主控制;1->手动控制;2->计划
int BumpSwitch;                  //水泵开关 0->关闭;1->开启
int BumpTime;                    //单次灌溉时长,默认为3s
unsigned long long BumpInterval; //计划操作时,间隔时间(单位:ms)
unsigned long long BumpStartTime;//每次的启动时间(24h)
int WaterTank;                 //水箱水位
int tiandisId = 1;             //田地区块

//WiFi
const char *ssid="zeyun";
const char *password="11235813";

//声明ntp
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP,"ntp.aliyun.com");                 //NTP地址

//I/O端口
const int BumpPin=16;           //水泵针脚
const int SoilMoisturePin=35;  //土壤湿度传感器针脚

//设置土壤湿度传感器采样数量
const int NumRead=5;           //采样数量
int MoistRead[NumRead];        //存储样本
int Point=0;                   //指示目前最新数据

//时间戳,用于控制灌溉时间及计算第一次开启时间差
unsigned long long TimeStamp=0;
//水泵计划模式所需数据
typedef struct bump{
  int flag;                    //已经过几天
  int time;                    //现在时间
}bump;
bump Bump={0,-1};
//比对以判断水泵状态是否改变
int cmp=3;

/******************
 *计算土壤湿度     *
 *使用了滤波算法   *
 *NumRead-采样数量*
 *****************/
inline double SoilMoistGet(){
  int sum=0;
  for(int i=0;i<NumRead;i++){
    sum+=MoistRead[i];
  }
  //返回湿度(百分数)
  double ave=sum/NumRead;
  double per=(1-((ave-1400)/900.0))*100;
  return per;
}


//初始化土壤湿度数据
inline void InitMoist(){
  for(int i=0;i<NumRead;i++){
    MoistRead[i]=analogRead(SoilMoisturePin);
    delay(5);
  }
  return;
}

//开启端口
inline void Connect(){
  pinMode(BumpPin,OUTPUT);
  pinMode(SoilMoisturePin,INPUT);
}

//调试代码(输出数据,检测数据发送是否成功.etc)
inline void Test(){
  /*打印数据*/
  Serial.print("土壤湿度:");
  Serial.println(SoilMoisture);
  Serial.print("水泵状态:");
  Serial.println(BumpState);
}

void setup() {
  //开启串口
  Serial.begin(115200);

  //开启端口
  Connect();

  //使用核心0, 读取土壤湿度及电池电压数据
  xTaskCreatePinnedToCore(
                    Task0code,   // Task function. 
                    "Task0",     // name of task. 
                    10000,       // Stack size of task 
                    NULL,        // parameter of the task 
                    1,           // priority of the task 
                    &Task0,      // Task handle to keep track of created task 
                    0);          // pin task to core 0                  
  delay(100);

  //初始化土壤湿度数据
  InitMoist();

  //ntp服务
  timeClient.begin();
  timeClient.setTimeOffset(28800); //+1区，偏移3600，+8区，偏移3600*8

  //将灌溉时间初始化为3s
  BumpTime=3000;
}



void Task0code(void * pvParameters){
  //读取土壤湿度数据并处理
  while(1){

  
  MoistRead[++Point]=analogRead(SoilMoisturePin);
  Point%=NumRead;
  SoilMoisture=SoilMoistGet();

  
  //调试端口(默认关闭)
  // Test();
  }
}

void loop() {

  
  //如果水箱水位为"低水位",则停止浇灌
  if(!WaterTank){
    return;
  }

  switch(BumpState){
  /*由单片机控制水泵*/
  case 0:
      //土壤较为干旱情况
      if(SoilMoisture<20){
        digitalWrite(BumpPin,HIGH);
        TimeStamp=millis();
        while(millis()-TimeStamp<BumpTime);
        digitalWrite(BumpPin,LOW);
      }
      //土壤非常干旱情况
      else if(SoilMoisture<50){
        BumpTime=5000;
        digitalWrite(BumpPin,HIGH);
        TimeStamp=millis();
        while(millis()-TimeStamp<BumpTime);
        digitalWrite(BumpPin,LOW);
        BumpTime=3000;
      }
      cmp=3; Bump.time=-1;
    break;
  /*手动控制水泵*/
  case 1:
      //比对数据并决定是否更改水泵状态
      if(BumpSwitch!=cmp){
        cmp=BumpSwitch;
        if(cmp){
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
        TimeStamp=millis();
        digitalWrite(BumpPin,HIGH);
        while(millis()-TimeStamp<BumpTime);
        digitalWrite(BumpPin,LOW);
        TimeStamp=millis();
        Bump.flag=0;
      }
      cmp=3;
    break;
  }

 //接收数据
  HTTPClient http; // 声明HTTPClient对象

  http.begin("http://192.168.16.9:8080/Control/Out/Bump/Switch"); // 水泵操作模式 0->由单片机自主控制;1->手动控制;2->计划

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

  http.begin("http://192.168.16.9:8080/Control/Out/Bump/Switch"); // 水泵开关(仅手动控制状态使用) 0->关闭;1->开启
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

  http.begin("http://192.168.16.9:8080/Control/Out/Bump/Time"); // 单次灌溉时长
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

  http.begin("http://192.168.16.9:8080/Control/Out/Bump/StartTime"); // 每次的启动时间(24h)
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

  http.begin("http://192.168.16.9:8080/Control/Out/Bump/Interval"); // 计划操作时,间隔时间(单位:ms)
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

  //发送数据
  String url1 = "http://192.168.16.9:8080/Outside/SoilMoisture";
    http.begin(url1);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String postData1 = "turangshidu=" + String(SoilMoisture) + "&tiandisId=" + String(tiandisId);
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
    delay(2000);
}
