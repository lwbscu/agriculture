#include <Servo.h>                        //舵机头文件
#include <ld3320.h>                       //LD3320语音识别模块的头文件
 
Servo myservo;                                  //声明一个舵机对象
VoiceRecognition Voice;                         //声明一个语音识别对象
 
#define Led 8                                   //定义LED控制引脚
 
void setup()                                   
{
    myservo.attach(6);                          //定义6号引脚为舵机的脉冲输入引脚
    pinMode(Led,OUTPUT);                        //初始化LED引脚为输出模式
    digitalWrite(Led,LOW);                      //LED引脚低电平
    
    Voice.init();                               //初始化VoiceRecognition模块
    Voice.noiseTime(0x10);                      //上电噪声略过
    Voice.micVol(0x30);                         //调整ADC增益
    Voice.voiceMaxLength(0x14);                 //最长语音段时间
    Voice.addCommand("kai",0);             //"开"添加指令，参数（指令内容，指令标签（可重复））
    Voice.addCommand("guan",1);            //"关"添加指令，参数（指令内容，指令标签（可重复））
    Voice.addCommand("jiu shi du",2);           //90度
    Voice.addCommand("yi bai ba shi du",3);     //180度
    Voice.start();                              //开始识别
}
void loop() 
{
  switch(Voice.read())                          //判断识别
  {
    case 0:                                     //若是指令“kai deng”
    digitalWrite(Led,HIGH);                       //点亮LED
    break;
 
    case 1:                                     //若是指令“guan deng”
    digitalWrite(Led,LOW);                        //熄灭LED
    break;
 
    case 2:
    myservo.write(90);                            //90度
    break;
 
    case 3:
    myservo.write(180);                           //180度
    break;
  
    default:
    break;
  }
}