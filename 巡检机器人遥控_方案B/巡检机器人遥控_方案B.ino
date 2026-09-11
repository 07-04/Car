// ============================================
// 肉牛养殖场巡检机器人 - WiFi 遥控（方案B: ESP32 → UART → STM32）
// ESP32 只负责：WiFi 网页遥控 + 串口转发 + 传感器数据显示
// 不再直接驱动 TB6612，改由 STM32 执行
// ============================================
// Serial2: RX=GPIO16 ← STM32 TX(PA9), TX=GPIO17 → STM32 RX(PA10)
// 协议：方向按钮 → "F/B/L/R/S\n"；速度滑杆 → "V<n>\n"
//       STM32 每秒上报一行 JSON → 网页 /data 轮询显示
// ============================================
#include <WiFi.h>
#include <WebServer.h>

WebServer server(80);

String sensorJson = "{}";   // 最近一次 STM32 上报的 JSON
String uartBuf = "";

void sendCmd(int c) {
  switch (c) {
    case 1: Serial2.print("F\n"); break;   // 前进
    case 2: Serial2.print("B\n"); break;   // 后退
    case 3: Serial2.print("L\n"); break;   // 左转
    case 4: Serial2.print("R\n"); break;   // 右转
    default: Serial2.print("S\n"); break;  // 停
  }
}

void sendSpd(int s) {
  Serial2.print("V" + String(s) + "\n");
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
 .pad{display:grid;grid-template-columns:repeat(3,90px);grid-template-rows:repeat(3,90px);gap:8px;justify-content:center;margin-top:20px}
 .btn{background:#333;border:2px solid #555;border-radius:12px;font-size:26px;color:#eee}
 .btn:active{background:#0a6}
 .stop{background:#a00;border-color:#f66}
 .sensor{margin:18px auto;width:86%;max-width:420px;background:#1a1a1a;border:1px solid #333;border-radius:10px;padding:12px}
 .sensor table{width:100%;font-size:14px;border-collapse:collapse}
 .sensor td{padding:6px 8px;text-align:left}
 .sensor td:last-child{text-align:right;font-family:monospace;color:#0cf}
</style></head>
<body>
<h1>巡检机器人遥控</h1>
<div>速度：<span id="sv">70</span></div>
<input id="speed" type="range" min="0" max="100" value="70">
<div class="pad">
 <span></span><button class="btn" data-c="1">&#9650;</button><span></span>
 <button class="btn" data-c="3">&#9664;</button><button class="btn stop" data-c="0">&#9632;</button><button class="btn" data-c="4">&#9654;</button>
 <span></span><button class="btn" data-c="2">&#9660;</button><span></span>
</div>
<div class="sensor">
 <table>
  <tr><td>空气温度</td><td id="d_temp">--</td></tr>
  <tr><td>空气湿度</td><td id="d_humi">--</td></tr>
  <tr><td>红外体温</td><td id="d_ir">--</td></tr>
  <tr><td>前方距离</td><td id="d_df">--</td></tr>
  <tr><td>左方距离</td><td id="d_dl">--</td></tr>
  <tr><td>右方距离</td><td id="d_dr">--</td></tr>
 </table>
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
function fmt(v,u){if(v==null||isNaN(v)||v<0)return '--';return v.toFixed(1)+u;}
setInterval(function(){
  fetch('/data').then(function(r){return r.json();}).then(function(d){
    document.getElementById('d_temp').textContent=fmt(d.temp,'℃');
    document.getElementById('d_humi').textContent=fmt(d.humi,'%');
    document.getElementById('d_ir').textContent=fmt(d.ir,'℃');
    document.getElementById('d_df').textContent=fmt(d.dist_f,' cm');
    document.getElementById('d_dl').textContent=fmt(d.dist_l,' cm');
    document.getElementById('d_dr').textContent=fmt(d.dist_r,' cm');
  }).catch(()=>{});
},2000);
</script></body></html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", PAGE);
}
void handleCmd() {
  sendCmd(server.arg("c").toInt());
  server.send(200, "text/plain", "ok");
}
void handleSpd() {
  sendSpd(constrain(server.arg("s").toInt(), 0, 100));
  server.send(200, "text/plain", "ok");
}
void handleData() {
  server.send(200, "application/json", sensorJson);
}

void setup() {
  Serial2.begin(115200, SERIAL_8N1, 16, 17);   // RX=16, TX=17 → STM32

  WiFi.softAP("Robot", "12345678");            // 热点名 / 密码

  server.on("/",     handleRoot);
  server.on("/cmd",  handleCmd);
  server.on("/spd",  handleSpd);
  server.on("/data", handleData);
  server.begin();
}

void loop() {
  server.handleClient();

  // 收 STM32 上报的 JSON（一行一条，换行分隔）
  while (Serial2.available()) {
    char c = (char)Serial2.read();
    if (c == '\n') {
      uartBuf.trim();
      if (uartBuf.startsWith("{")) sensorJson = uartBuf;
      uartBuf = "";
    } else {
      uartBuf += c;
    }
  }

  delay(5);
}
