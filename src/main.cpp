#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <WiFiUdp.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>

ESP8266WebServer server;
int LED = 5;
MDNSResponder mdns;

WiFiUDP UDPInstance;               // 实例化WiFiUDP
unsigned int localUDPPort = 5181;  // 本地监听端口
unsigned int remoteUDPPort = 4321; // 远程监听端口
char incomingPacket[255];          // 保存Udp工具发过来的消息

#define MAX_RETRY 10

void smart_config(void)
{
  // Init WiFi as Station, start SmartConfig
  WiFi.mode(WIFI_AP_STA);
  WiFi.beginSmartConfig();

  // Wait for SmartConfig packet from mobile
  Serial.println("Waiting for SmartConfig.");
  while (!WiFi.smartConfigDone())
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("SmartConfig received.");

  // Wait for WiFi to connect to AP
  Serial.println("Waiting for WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  WiFi.setAutoConnect(true); // 设置自动连接
}

bool connect_wifi(void)
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(); // 启动WIFI连接
  Serial.println("Connection WIFI");

  int retry_count = 0;
  while (retry_count < MAX_RETRY)
  {
    delay(500);
    Serial.print(".");
    retry_count++;
    if (WiFi.status() == WL_CONNECTED) // 检查连接状态
    {
      return true;
    }
  }
  return false;
}

void setup_wifi(void)
{
  if (!connect_wifi())
  {
    smart_config();
  }

  Serial.println("");
  Serial.print("WiFi connected: ");
  Serial.println(WiFi.SSID());
  Serial.printf("PSW:%s\r\n", WiFi.psk().c_str());
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (mdns.begin("onlinetest", WiFi.localIP()))
  {
    Serial.println("MDNS responder started");
    mdns.addService("http", "tcp", 80);

    server.on("/", HTTP_GET,[]()
              { server.send(200, "text/plain", WiFi.localIP().toString()); });
    server.begin();

    
  }

  

  if (UDPInstance.begin(localUDPPort))
  { // 启动Udp监听服务
    Serial.println("监听成功");
    Serial.printf("现在收听IP：%s, UDP端口：%d\n", WiFi.localIP().toString().c_str(), localUDPPort);
  }
  else
  {
    Serial.println("监听失败");
  }
}

// 向udp工具发送消息
void sendCallBack(const char *buffer)
{
  UDPInstance.beginPacket(UDPInstance.remoteIP(), remoteUDPPort); // 配置远端ip地址和端口
  UDPInstance.write(buffer);                                      // 把数据写入发送缓冲区
  UDPInstance.endPacket();                                        // 发送数据
}

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  setup_wifi();
  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);
}

void loop()
{
  // put your main code here, to run repeatedly:
  mdns.update();
  server.handleClient();
  // 解析Udp数据包
  int packetSize = UDPInstance.parsePacket(); // 获得解析包
  if (packetSize != 0)
  {
    if (packetSize) // 解析包不为空
    {
      // 读取Udp数据包并存放在incomingPacket
      int len = UDPInstance.read(incomingPacket, 255); // 返回数据包字节数
      if (len > 0)
      {
        incomingPacket[len] = 0;
        if (strcmp(incomingPacket, "ON") == 0) // 命令ON
        {
          digitalWrite(LED, HIGH); // 控制实际的设备
          Serial.println("信号灯已打开");
          sendCallBack("灯已打开");
        }
        else
        {
          digitalWrite(LED, LOW);
          Serial.println("信号灯已关闭");
          sendCallBack("灯已关闭");
        }
      }
    }
  }
}