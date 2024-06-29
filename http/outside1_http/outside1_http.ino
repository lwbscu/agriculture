/*************************************
 *3区-ESP32                          *
 *室外温湿度、降雨、气压、电池电压与电量*
 *愿意和我一辈子搞嵌入式吗?            *
 *************************************/
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <HTTPClient.h>
#include <BMP280.h>
#include <Adafruit_AHTX0.h>
/*
//初始化I2C引脚-第一个设备
#define SDA_PIN_1 21
#define SCL_PIN_1 22
*/

//设置降雨传感器采样数量
const int NumRead=5;           //采样数量
int RainRead[NumRead];         //存储样本
int Point=0;                   //指示目前最新数据
int Temperature = 20;    //温度
int AirHumidity = 20;    //空气湿度
int Rain = 20 ;           //降雨
int AtomPre = 20 ;        //气压
double Voltage;     //电池电压
int BatteryLevel = 20 ;   //电池电量
int tiandisId = 1;
int FanState = 0;
int FanSwitch = 1;
int FanTemperature = 20;



//定义端口
const int RainPin=4;           //降雨传感器
const int VoltagePin=34;       //电池电压针脚
const int FanPin=13;            //风扇针脚


//声明Aht20&Bmp20
Adafruit_AHTX0 aht;
BMP280 bmp280;


//风扇控制的相关变量(默认关闭)
typedef struct fan{
  int Now;                     //风扇目前状态
  int Set;                     //设定的角度
} fan;
fan Fan={0,0};

const char *ssid = "U NEED CRY DEAR";    //你的网络名称
const char *password = "12345678"; //你的网络密码
//const char *ssid = "zeyun";    //你的网络名称
//const char *password = "11235813"; //你的网络密码
/************************************
 *计算电池电压及电量                  *
 *使用esp32测量电池电压的20%并进行处理 *
 *得到实际电压与电池电量              *
 ************************************/
inline void VolGet(double V){
  Voltage = -3.3126889904968743e-18*V*V*V*V*V*V*V  \
                     +1.623217605075953e-14*V*V*V*V*V*V     \
                     -3.3719331192434486e-11*V*V*V*V*V      \
                     +3.847999653156838e-8*V*V*V*V          \
                     -0.00002604499123955503*V*V*V          \
                     +0.010452452154782833*V*V              \
                     -2.3016342267679173*V                  \
                     +214.85012707853883;
  Voltage *= 5;
  BatteryLevel = 100*(Voltage-3)/1.2;
}


//初始化降雨传感器数据
void InitRain(){
  for(int i=0;i<NumRead;i++){
    RainRead[i]=analogRead(RainPin);
    delay(5);
  }
  return;
}

//读取降雨传感器数据
double RainGet(){
  int sum=0;
  for(int i=0;i<NumRead;i++){
    sum+=RainRead[i];
  }
  //返回降雨值(百分数)
  double ave=sum/NumRead;
  double per=(1-((ave-900)/1300.0))*100;
  return per;
}

//检测是否初始化成功
void Detect(){
  //Aht20初始化
  while(!aht.begin()){
    Serial.println("Aht20 initial error!");
    delay(500);
  }
/*
  //Bmp初始化
  while(!bmp280.begin()){
    Serial.println("Bmp280 initial error!");
    delay(500);
  }
*/
  Serial.println("Aht20&Bmp280 initial successfully!");
}

//端口初始化
inline void Connect(){
  //初始化雨水传感器
  pinMode(RainPin,INPUT);
  //初始化电压读取端口
  pinMode(VoltagePin,INPUT);
}


void setup()
{
  Serial.begin(115200);
  Serial.println();
//初始化端口
  WiFi.begin(ssid, password);

  //初始化I2C总线
  // Wire.begin(SDA_PIN_2, SCL_PIN_2);
  Wire.begin();
  //(SDA_PIN_1, SCL_PIN_1);
  /**意义不明的代码(待确认)
   *Wire.beigin;
  **/
  bmp280.begin();//初始化BMP280
  //初始化Aht20&Bmp280
  Detect();

  //初始化引脚
  Connect();

  //初始化雨水数据
  InitRain();

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected!");

  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop()
{
  
  
  //读取气压&空气温湿度
  uint32_t Pressure = bmp280.getPressure();
  sensors_event_t Humidity, Temp;
  aht.getEvent(&Humidity, &Temp);
  AtomPre=Pressure/1000;
  Temperature=Temp.temperature;
  AirHumidity=Humidity.relative_humidity;

  //读取雨水传感器数值
  RainRead[++Point]=analogRead(RainPin);
  Point%=NumRead;
  Rain=RainGet();

  //读取电池电压数据并处理
  double V=analogRead(VoltagePin);
  VolGet(V);


 //风扇控制
  switch(FanState){
   //基于温度开启
   case 0:
    //不低于设定温度时开启
    if(Temperature>=FanTemperature){
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
    if(FanSwitch){
      digitalWrite(FanPin,HIGH);
    }
    else{
      digitalWrite(FanPin,LOW);
    }
    break;
  }
 


  //数据指令接收
  HTTPClient http; // 声明HTTPClient对象
  //////////
  http.begin("http://192.168.16.8:8080/Control/In/Fan/State"); // 风扇操作模式 0->基于温度进行控制;1—>手动控制
  int httpCode = http.GET(); // 发起GET请求

  if (httpCode > 0) // 如果状态码大于0说明请求过程无异常
  {
    if (httpCode == HTTP_CODE_OK) // 请求被服务器正常响应，等同于httpCode == 200
    {
      String FState = http.getString(); // 读取服务器返回的响应正文数据
      FanState = FState.toInt();
      Serial.print("FState=");
      Serial.println(FState);
    }
  }
  else
  {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  //////////
  http.begin("http://192.168.16.9:8080/Control/In/Fan/Switch");  //风扇开关(仅手动控制状态使用) 0->关闭;1—>开启
  httpCode = http.GET(); // 发起GET请求

  if (httpCode > 0) // 如果状态码大于0说明请求过程无异常
  {
    if (httpCode == HTTP_CODE_OK) // 请求被服务器正常响应，等同于httpCode == 200
    {
      String FSwitch = http.getString(); //设定的开启标准(高于此值时开启)
      FanSwitch = FSwitch.toInt();
      Serial.print("FSwitch=");
      Serial.println(FSwitch);
    }
  }
  else
  {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  ///////////
  http.begin("http://192.168.16.9:8080/Control/In/Fan/Temperature"); // 单次灌溉时长
  httpCode = http.GET(); // 发起GET请求

  if (httpCode > 0) // 如果状态码大于0说明请求过程无异常
  {
    if (httpCode == HTTP_CODE_OK) // 请求被服务器正常响应，等同于httpCode == 200
    {
      String FTemperature = http.getString(); // 读取服务器返回的响应正文数据
      FanTemperature = FTemperature.toInt();
      Serial.print("FTemperature=");
      Serial.println(FTemperature);
    }
  }
  else
  {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
    
    /*发送数据部分*/
    /////////
    String url1 = "http://192.168.16.8:8080/Outside/Temperature";
    http.begin(url1);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String postData1 = "wendu=" + String(Temperature) + "&tiandisId=" + String(tiandisId);
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

    //////////
    String url2 = "http://192.168.16.8:8080/Outside/AirHumidity";
    http.begin(url2);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String postData2 = "kongqishidu=" + String(AirHumidity) + "&tiandisId=" + String(tiandisId);
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

    /////////
    String url3 = "http://192.168.16.8:8080/Outside/AtomPre";
    http.begin(url3);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String postData3 = "qiya=" + String(AtomPre) + "&tiandisId=" + String(tiandisId);
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

    /////////
    String url4 = "http://192.168.16.8:8080/Outside/BatteryLevel";
    http.begin(url4);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String postData4 = "dianchidianliang=" + String(BatteryLevel) + "&tiandisId=" + String(tiandisId);
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

    /////////
    String url5 = "http://192.168.16.8:8080/Outside/Rain";
    http.begin(url5);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String postData5 = "jiangyuliang=" + String(Rain) + "&tiandisId=" + String(tiandisId);
    httpCode = http.POST(postData5); // 发送POST请求
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
  Serial.print("温度:");
  Serial.println(Temperature);
  Serial.print("湿度:");
  Serial.println(AirHumidity);
  Serial.print("降雨:");
  Serial.println(Rain);
  Serial.print("气压:");
  Serial.println(AtomPre);
  Serial.print("电池电压:");
  Serial.println(Voltage);
  Serial.print("电池电量:");
  Serial.println(BatteryLevel);
  delay(5000);
}
