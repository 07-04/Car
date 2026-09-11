// ============================================
// 肉牛养殖场巡检机器人 - WiFi 遥控 (ESP32 + TB6612)
// 接法（两片 TB6612，每片管一侧，PWM/IN 引脚组内并联）：
//   左组(TB6612#1): PWM=4  IN1=16  IN2=17
//   右组(TB6612#2): PWM=5  IN1=18  IN2=19
// 使用：上电后 ESP32 自建热点 "Robot"，密码 "12345678"
//       手机连上该 WiFi → 浏览器打开 192.168.4.1 → 屏幕按钮/键盘 WASD 遥控
// ============================================
#include <WiFi.h>
#include <WebServer.h>

// 左组（TB6612 #1）
#define L_PWM 4
#define L_IN1 16
#define L_IN2 17

// 右组（TB6612 #2）
#define R_PWM 5
#define R_IN1 18
#define R_IN2 19

WebServer server(80);

// 运动状态：0停 1前进 2后退 3左转 4右转
volatile int  cmd     = 0;
volatile unsigned long lastCmd = 0;
const unsigned long TIMEOUT_MS = 800;   // 失联/松手 800ms 自动停车
int speed = 180;                          // 默认速度 0~255

void setMotor(int pwm, int in1, int in2, int spd) {
  spd = constrain(spd, -255, 255);
  if (spd > 0)      { digitalWrite(in1, HIGH); digitalWrite(in2, LOW);  analogWrite(pwm, spd); }
  else if (spd < 0) { digitalWrite(in1, LOW);  digitalWrite(in2, HIGH); analogWrite(pwm, -spd); }
  else              { digitalWrite(in1, LOW);  digitalWrite(in2, LOW);  analogWrite(pwm, 0); }
}

void drive(int dir, int spd) {
  switch (dir) {
    case 1:  // 前进：左右同向正转
      setMotor(L_PWM, L_IN1, L_IN2,  spd);
      setMotor(R_PWM, R_IN1, R_IN2,  spd);
      break;
    case 2:  // 后退：左右同向反转
      setMotor(L_PWM, L_IN1, L_IN2, -spd);
      setMotor(R_PWM, R_IN1, R_IN2, -spd);
      break;
    case 3:  // 原地左转：左组反转、右组正转
      setMotor(L_PWM, L_IN1, L_IN2, -spd);
      setMotor(R_PWM, R_IN1, R_IN2,  spd);
      break;
    case 4:  // 原地右转：左组正转、右组反转
      setMotor(L_PWM, L_IN1, L_IN2,  spd);
      setMotor(R_PWM, R_IN1, R_IN2, -spd);
      break;
    default: // 停车
      setMotor(L_PWM, L_IN1, L_IN2, 0);
      setMotor(R_PWM, R_IN1, R_IN2, 0);
  }
}

const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no">
<title>巡检机器人遥控</title>
<style>
 body{margin:0;background:#111;color:#eee;font-family:sans-serif;text-align:center;user-select:none;-webkit-user-select:none;touch-action:none}
 h1{font-size:20px;margin:10px 0}
 #speed{width:80%}
 .pad{display:grid;grid-template-columns:repeat(3,90px);grid-template-rows:repeat(3,90px);gap:8px;justify-content:center;margin-top:24px}
 .btn{background:#333;border:2px solid #555;border-radius:12px;font-size:26px;color:#eee}
 .btn:active{background:#0a6}
 .stop{background:#a00;border-color:#f66}
</style></head>
<body>
<h1>巡检机器人遥控</h1>
<div>速度：<span id="sv">180</span></div>
<input id="speed" type="range" min="80" max="255" value="180">
<div class="pad">
 <span></span><button class="btn" data-c="1">&#9650;</button><span></span>
 <button class="btn" data-c="3">&#9664;</button><button class="btn stop" data-c="0">&#9632;</button><button class="btn" data-c="4">&#9654;</button>
 <span></span><button class="btn" data-c="2">&#9660;</button><span></span>
</div>
<script>
function send(c){fetch('/cmd?c='+c).catch(()=>{});}
var timer=null;
function press(c){send(c);if(timer)clearInterval(timer);timer=setInterval(function(){send(c);},300);}
function release(){if(timer){clearInterval(timer);timer=null;}send(0);}
document.querySelectorAll('.btn').forEach(function(b){
  b.addEventListener('touchstart',function(e){e.preventDefault();press(b.dataset.c);});
  b.addEventListener('mousedown',function(){press(b.dataset.c);});
  b.addEventListener('touchend',function(e){e.preventDefault();release();});
  b.addEventListener('touchcancel',function(e){e.preventDefault();release();});
  b.addEventListener('mouseup',release);
  b.addEventListener('mouseleave',release);
});
document.getElementById('speed').addEventListener('input',function(e){
  document.getElementById('sv').textContent=e.target.value;
  fetch('/spd?s='+e.target.value).catch(()=>{});
});
document.addEventListener('keydown',function(e){var m={'w':1,'s':2,'a':3,'d':4};if(m[e.key])press(m[e.key]);});
document.addEventListener('keyup',function(e){var m={'w':1,'s':2,'a':3,'d':4};if(m[e.key])release();});
</script></body></html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", PAGE);
}
void handleCmd() {
  cmd = server.arg("c").toInt();
  lastCmd = millis();
  server.send(200, "text/plain", "ok");
}
void handleSpd() {
  speed = server.arg("s").toInt();
  speed = constrain(speed, 0, 255);
  server.send(200, "text/plain", "ok");
}

void setup() {
  pinMode(L_PWM, OUTPUT);
  pinMode(L_IN1, OUTPUT);
  pinMode(L_IN2, OUTPUT);
  pinMode(R_PWM, OUTPUT);
  pinMode(R_IN1, OUTPUT);
  pinMode(R_IN2, OUTPUT);
  drive(0, 0);

  WiFi.softAP("Robot", "12345678");   // 热点名 / 密码(≥8位)

  server.on("/",    handleRoot);
  server.on("/cmd", handleCmd);
  server.on("/spd", handleSpd);
  server.begin();
}

void loop() {
  server.handleClient();

  // 松手/断网超时自动停车
  if (cmd != 0 && millis() - lastCmd > TIMEOUT_MS) {
    cmd = 0;
  }

  drive(cmd, speed);
  delay(10);
}
