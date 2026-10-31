#pragma once
#include <Arduino.h>

// Web UI（PROGMEM，节省 RAM）
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Bambu 打印机监视器</title>
<style>
:root{--bg:#0f1319;--card:#1a2028;--line:#2a3340;--txt:#e6edf3;--sub:#8b98a7;--accent:#4fc3f7;--ok:#2ecc71;--warn:#f1c40f;--err:#e74c3c;}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--txt);font-family:-apple-system,"Segoe UI",Roboto,"Helvetica Neue",Arial,sans-serif;}
header{padding:18px 16px;border-bottom:1px solid var(--line);display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:8px;}
h1{font-size:18px;margin:0;font-weight:600;letter-spacing:.5px;}
.badges{display:flex;gap:8px;flex-wrap:wrap;}
.badge{font-size:12px;padding:3px 10px;border-radius:999px;border:1px solid var(--line);background:#232a33;color:var(--sub);}
.badge.ok{border-color:var(--ok);color:var(--ok);}
.badge.bad{border-color:var(--err);color:var(--err);}
.badge.idle{border-color:var(--accent);color:var(--accent);}
.badge.printing{border-color:var(--ok);color:var(--ok);}
.badge.paused{border-color:var(--warn);color:var(--warn);}
.badge.error{border-color:var(--err);color:var(--err);}
main{padding:16px;max-width:1080px;margin:0 auto;}
.stats{display:grid;grid-template-columns:repeat(3,1fr);gap:14px;align-items:stretch;}
.stats .card{grid-column:span 1;margin:0;}
.stats .fill2{grid-column:span 2;}
.stats .fill3{grid-column:1/-1;}
@media(max-width:760px){.stats{grid-template-columns:1fr;}.stats .card,.stats .fill2,.stats .fill3{grid-column:auto;}}
.settings{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:14px;margin-top:14px;}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px 16px;}
.card.wide{margin:14px 0 0;}
.card h2{font-size:13px;margin:0 0 12px;color:var(--sub);font-weight:600;text-transform:uppercase;letter-spacing:1px;}
.row{display:flex;justify-content:space-between;align-items:center;padding:6px 0;border-bottom:1px dashed rgba(255,255,255,.05);gap:10px;}
.row:last-child{border-bottom:0;}
.row .k{color:var(--sub);font-size:13px;white-space:nowrap;}
.row .v{font-size:15px;font-weight:600;text-align:right;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}
.temp{font-variant-numeric:tabular-nums;}
.sub{color:var(--sub);font-weight:400;font-size:13px;}
.progress{height:8px;background:#0c1015;border-radius:6px;overflow:hidden;margin-top:10px;border:1px solid var(--line);}
.progress>span{display:block;height:100%;width:0;background:linear-gradient(90deg,#4fc3f7,#2ecc71);transition:width .4s;}
.btns{display:flex;gap:8px;flex-wrap:wrap;margin-top:6px;}
button{cursor:pointer;border:1px solid var(--line);background:#232a33;color:var(--txt);border-radius:9px;padding:9px 14px;font-size:14px;transition:all .15s;}
button:hover{background:#2c3541;}
button:disabled{opacity:.5;cursor:not-allowed;}
button.on{border-color:var(--ok);color:var(--ok);}
.led{width:12px;height:12px;border-radius:50%;background:#333b45;display:inline-block;margin-right:8px;vertical-align:middle;box-shadow:0 0 0 2px rgba(0,0,0,.3) inset;transition:all .2s;}
.led.on{background:var(--ok);box-shadow:0 0 8px var(--ok);}
label{display:block;font-size:12px;color:var(--sub);margin:8px 0 4px;}
input,select{width:100%;background:#0c1015;border:1px solid var(--line);color:var(--txt);border-radius:8px;padding:9px 10px;font-size:14px;}
input:focus,select:focus{outline:none;border-color:var(--accent);}
input[type=number]{font-variant-numeric:tabular-nums;}
.checks{display:grid;grid-template-columns:repeat(auto-fit,minmax(110px,1fr));gap:6px;margin-top:6px;}
.checks label{display:flex;align-items:center;gap:6px;margin:0;padding:8px 10px;border:1px solid var(--line);border-radius:8px;background:#0c1015;font-size:13px;color:var(--txt);}
.checks input{width:auto;margin:0;accent-color:var(--accent);}
.actions{display:flex;gap:8px;margin-top:12px;flex-wrap:wrap;}
.hint{color:var(--sub);font-size:12px;line-height:1.5;margin-top:10px;}
.foot{text-align:center;color:var(--sub);font-size:12px;padding:8px 0 22px;}
a{color:var(--accent);text-decoration:none;}
.err{color:var(--err);font-size:12px;margin-top:6px;min-height:14px;}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:10px;}
</style>
</head>
<body>
<header>
  <h1>Bambu 打印机监视器</h1>
  <div class="badges">
    <span id="modelTag" class="badge">…</span>
    <span id="stBadge" class="badge">…</span>
    <span id="mqttBadge" class="badge">…</span>
    <span id="relayBadge" class="badge">…</span>
  </div>
</header>
<main>
<div class="stats">
  <section class="card" id="cardTask">
    <h2>打印任务</h2>
    <div class="row"><span class="k">当前文件</span><span class="v" id="filename">—</span></div>
    <div class="row"><span class="k">打印进度</span><span class="v" id="percent">—</span></div>
    <div class="progress"><span id="bar"></span></div>
    <div class="row" style="margin-top:10px"><span class="k">剩余时间</span><span class="v" id="remain">—</span></div>
    <div class="row"><span class="k">当前图层</span><span class="v" id="layer">—</span></div>
  </section>

  <section class="card">
    <h2>温度</h2>
    <div class="row"><span class="k">喷嘴</span><span class="v temp" id="nozzle">—</span></div>
    <div class="row"><span class="k">热床</span><span class="v temp" id="bed">—</span></div>
    <div class="row" id="rowChamber"><span class="k">腔体</span><span class="v temp" id="chamber">—</span></div>
    <div class="row" id="rowBedType"><span class="k">热床类型</span><span class="v" id="bedType">—</span></div>
  </section>

  <section class="card">
    <h2>速度与风扇</h2>
    <div class="row"><span class="k">速度档位</span><span class="v" id="speed">—</span></div>
    <div class="row"><span class="k">部件风扇</span><span class="v" id="fanCooling">—</span></div>
    <div class="row" id="rowFanBig1"><span class="k">辅助风扇</span><span class="v" id="fanBig1">—</span></div>
    <div class="row" id="rowFanBig2"><span class="k">腔体风扇</span><span class="v" id="fanBig2">—</span></div>
    <div class="row"><span class="k">热端风扇</span><span class="v" id="fanHeatbreak">—</span></div>
    <div class="row" id="rowNozzleInfo"><span class="k">喷嘴</span><span class="v" id="nozzleInfo">—</span></div>
  </section>

  <section class="card" id="cardAms">
    <h2>耗材 / AMS</h2>
    <div class="row"><span class="k">当前槽位</span><span class="v" id="amsIndex">—</span></div>
    <div class="row"><span class="k">耗材类型</span><span class="v" id="amsType">—</span></div>
    <div class="row"><span class="k">AMS 湿度</span><span class="v" id="amsHumidity">—</span></div>
    <div class="row"><span class="k">AMS 温度</span><span class="v" id="amsTemp">—</span></div>
  </section>

  <section class="card">
    <h2>错误与系统</h2>
    <div class="row"><span class="k">错误码</span><span class="v" id="printError">—</span></div>
    <div class="row"><span class="k">失败原因</span><span class="v" id="failReason">—</span></div>
    <div class="row"><span class="k">HMS 告警</span><span class="v" id="hms">—</span></div>
    <div class="row"><span class="k">打印机 WiFi</span><span class="v" id="printerRssi">—</span></div>
    <div class="row"><span class="k">ESP WiFi</span><span class="v" id="espRssi">—</span></div>
    <div class="row"><span class="k">空闲内存</span><span class="v" id="heap">—</span></div>
    <div class="row"><span class="k">运行时间</span><span class="v" id="uptime">—</span></div>
  </section>
</div>

  <section class="card wide">
    <h2>继电器控制</h2>
    <div class="row"><span class="k"><span id="relayLed" class="led"></span><span id="relayState">…</span></span><span class="v" id="relayMode">—</span></div>
    <div class="row"><span class="k">自动目标</span><span class="v" id="relayWant">—</span></div>
    <div class="row"><span class="k">手动接管</span><span class="v" id="relayOverride">—</span></div>
    <div class="btns">
      <button class="on" onclick="relaySet(true)">开启</button>
      <button onclick="relaySet(false)">关闭</button>
      <button onclick="relayToggle()">切换</button>
    </div>
    <div class="err" id="relayErr"></div>

    <div class="grid2" style="margin-top:14px">
      <div>
        <label>工作模式</label>
        <select id="rMode">
          <option value="0">手动（仅网页开关）</option>
          <option value="1">自动（按规则）</option>
        </select>
        <label>触发条件</label>
        <div class="checks">
          <label><input type="checkbox" id="tPrinting">打印中</label>
          <label><input type="checkbox" id="tPaused">暂停</label>
          <label><input type="checkbox" id="tError">故障</label>
          <label><input type="checkbox" id="tTemp">温度阈值</label>
        </div>
      </div>
      <div>
        <label>吸合延时（秒）</label>
        <input type="number" id="rDelayOn" min="0" max="600" step="1" value="0">
        <label>断开延时（秒）</label>
        <input type="number" id="rDelayOff" min="0" max="600" step="1" value="0">
        <label>温度阈值（°C，0=禁用）</label>
        <input type="number" id="rTemp" min="0" max="300" step="1" value="0">
        <label>手动接管时长（分钟，0=不接管）</label>
        <input type="number" id="rOverride" min="0" max="120" step="1" value="5">
      </div>
    </div>
    <div class="actions">
      <button onclick="saveRelay()">保存继电器规则</button>
      <button type="button" onclick="loadConfig()">重新载入</button>
    </div>
    <div class="err" id="relayMsg"></div>
    <div class="hint">自动模式：满足任一勾选条件时吸合（可叠加延时）；条件消失后按“断开延时”释放。温度阈值取喷嘴与热床的较大值。手动开关会临时接管设定分钟数，期间忽略自动规则。上电始终为断开态。</div>
  </section>

<div class="settings">
  <section class="card">
    <h2>网络设置</h2>
    <form id="wifiForm" onsubmit="saveWifi(event)">
      <label>WiFi 名称 (SSID)</label>
      <input id="ssid" autocomplete="off">
      <label>WiFi 密码</label>
      <input id="pass" type="password" autocomplete="off">
      <div class="actions"><button type="submit">保存并重启</button></div>
      <div class="err" id="wifiMsg"></div>
    </form>
  </section>

  <section class="card">
    <h2>打印机设置</h2>
    <form id="printerForm" onsubmit="savePrinter(event)">
      <label>打印机 IP</label>
      <input id="pip" placeholder="192.168.1.100" autocomplete="off">
      <label>Device ID（序列号）</label>
      <input id="pdid" autocomplete="off">
      <label>LAN 访问码</label>
      <input id="pac" autocomplete="off">
      <div class="actions">
        <button type="submit">保存</button>
        <button type="button" onclick="doReset()">恢复出厂</button>
      </div>
      <div class="err" id="printerMsg"></div>
    </form>
  </section>
</div>
</main>
<p class="foot"><a href="/update">OTA 固件更新</a></p>
<script>
var STATE_MAP={idle:'空闲',printing:'打印中',paused:'已暂停',error:'错误',unknown:'未知',offline:'离线'};
var MODE_MAP={0:'手动',1:'自动'};
function el(id){return document.getElementById(id);}
function fmtTime(s){if(!s||s<=0)return '—';var h=Math.floor(s/3600),m=Math.floor((s%3600)/60);return h>0?h+'h '+m+'m':m+'m';}
function fmtUp(s){if(!s)return '0m';var d=Math.floor(s/86400),h=Math.floor((s%86400)/3600),m=Math.floor((s%3600)/60);return (d?d+'d ':'')+(h?h+'h ':'')+m+'m';}
function fmtFan(n){if(n===undefined||n===null)return '—';return n+'/15 ('+Math.round(n*100/15)+'%)';}
function setBadge(id,txt,cls){var b=el(id);b.textContent=txt;b.className='badge'+(cls?' '+cls:'');}
function setV(id,val){el(id).textContent=val;}
/* 末行不足 3 张时让最后一张铺满剩余列，保证每行填满且同行等高 */
function layoutStats(){
  var cards=[].slice.call(document.querySelectorAll('.stats .card'));
  cards.forEach(function(c){c.classList.remove('fill2','fill3');});
  var vis=cards.filter(function(c){return c.style.display!=='none';});
  if(!vis.length)return;
  if(window.innerWidth>760){
    var r=vis.length%3;
    if(r===1){
      /* 只剩一张时由「打印任务」整行承接，宽度观感最好 */
      var solo=el('cardTask')||vis[vis.length-1];
      solo.classList.add('fill3');
    }else if(r===2){
      vis[vis.length-1].classList.add('fill2');
    }
  }
}
window.addEventListener('resize',layoutStats);
/* 按编译期机型能力隐藏不适用的参数区块 */
function applyFeatures(f){
  if(f.model)el('modelTag').textContent=f.model;
  var show=function(id,yes){var e=el(id);if(e)e.style.display=yes?'':'none';};
  show('rowChamber',f.has_chamber);
  show('rowBedType',f.has_bed_type);
  show('rowFanBig1',f.has_aux_fan);
  show('rowFanBig2',f.has_chamber_fan);
  show('rowNozzleInfo',f.has_nozzle_info);
  show('cardAms',f.has_ams);
  layoutStats();
}
async function refresh(){
  try{
    var r=await fetch('/api/status',{cache:'no-store'});
    var d=await r.json();
    var cls=(d.state==='printing')?'printing':(d.state==='paused')?'paused':(d.state==='error')?'error':'idle';
    setBadge('stBadge',STATE_MAP[d.state]||'未知',cls);
    setBadge('mqttBadge',d.mqtt_connected?'已连接':'未连接',d.mqtt_connected?'ok':'bad');

    setV('filename',d.filename||'—');
    setV('percent',(d.percent||0)+'%');
    el('bar').style.width=Math.max(0,Math.min(100,d.percent||0))+'%';
    setV('remain',fmtTime(d.remaining));
    setV('layer',(d.total_layer>0)?((d.layer||0)+' / '+d.total_layer):'—');

    el('nozzle').innerHTML=(d.nozzle||0).toFixed(1)+'°C <span class="sub">/ '+(d.nozzle_target||0).toFixed(0)+'°C</span>';
    el('bed').innerHTML=(d.bed||0).toFixed(1)+'°C <span class="sub">/ '+(d.bed_target||0).toFixed(0)+'°C</span>';
    setV('chamber',d.chamber>0?d.chamber.toFixed(1)+'°C':'—');
    setV('bedType',d.bed_type||'—');

    setV('speed',d.spd_lvl?('档位 '+d.spd_lvl+' / 100%').replace('100%',(d.spd_mag||100)+'%'):'—');
    setV('fanCooling',fmtFan(d.fan_cooling));
    setV('fanBig1',fmtFan(d.fan_big1));
    setV('fanBig2',fmtFan(d.fan_big2));
    setV('fanHeatbreak',fmtFan(d.fan_heatbreak));
    setV('nozzleInfo',(d.nozzle_type||'—')+(d.nozzle_diameter?(' '+d.nozzle_diameter.toFixed(1)+'mm'):''));

    setV('amsIndex',(d.ams_index===undefined||d.ams_index===255)?'未使用':('槽位 '+d.ams_index));
    setV('amsType',d.ams_type||'—');
    setV('amsHumidity',(d.ams_humidity>=0)?(d.ams_humidity+'%'):'—');
    setV('amsTemp',d.ams_temp>0?d.ams_temp.toFixed(1)+'°C':'—');

    setV('printError',d.print_error?('#'+d.print_error):'无');
    setV('failReason',d.fail_reason||'—');
    setV('hms',(d.hms!==undefined)?d.hms:'0');
    setV('printerRssi',d.wifi_signal?(d.wifi_signal+' dBm'):'—');
    setV('espRssi',(d.esp_rssi||0)+' dBm');
    setV('heap',Math.round((d.heap||0)/1024)+' KB');
    setV('uptime',fmtUp(d.uptime));
  }catch(e){setBadge('mqttBadge','面板离线','bad');}
}
async function refreshRelay(){
  try{
    var r=await fetch('/api/relay',{cache:'no-store'});
    var d=await r.json();
    el('relayLed').className='led'+(d.on?' on':'');
    setV('relayState',d.on?'已闭合 (ON)':'已断开 (OFF)');
    setV('relayMode',(MODE_MAP[d.mode]||'手动')+'模式');
    setV('relayWant',d.mode===1?(d.want?'满足条件':'未满足'):'不适用');
    setV('relayOverride',d.override?('接管中，剩余 '+fmtTime(d.override_remain)):'无');
    setBadge('relayBadge',d.on?'继电器 ON':'继电器 OFF',d.on?'ok':'');
  }catch(e){}
}
async function relaySet(on){
  try{
    await fetch('/api/relay',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({on:on})});
    await refreshRelay();
  }catch(e){el('relayErr').textContent='继电器操作失败';}
}
async function relayToggle(){
  try{
    await fetch('/api/relay',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({toggle:true})});
    await refreshRelay();
  }catch(e){el('relayErr').textContent='继电器操作失败';}
}
function relayTriggerMask(){
  var m=0;
  if(el('tPrinting').checked)m|=1;
  if(el('tPaused').checked)m|=2;
  if(el('tError').checked)m|=4;
  if(el('tTemp').checked)m|=8;
  return m;
}
async function saveRelay(){
  var mask=relayTriggerMask();
  if(mask===0){el('relayMsg').textContent='至少选择一个触发条件';return;}
  var settings={
    mode:parseInt(el('rMode').value,10),
    trigger:mask,
    delay_on:parseInt(el('rDelayOn').value||'0',10),
    delay_off:parseInt(el('rDelayOff').value||'0',10),
    temp_threshold:parseInt(el('rTemp').value||'0',10),
    override_min:parseInt(el('rOverride').value||'0',10)
  };
  el('relayMsg').textContent='保存中…';
  try{
    var r=await fetch('/api/relay',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({settings:settings})});
    el('relayMsg').textContent=r.ok?'规则已保存':'保存失败';
    await refreshRelay();
  }catch(e){el('relayMsg').textContent='网络错误';}
}
async function saveWifi(ev){
  ev.preventDefault();
  var ssid=el('ssid').value.trim(),pass=el('pass').value;
  if(!ssid){el('wifiMsg').textContent='请输入 SSID';return;}
  el('wifiMsg').textContent='保存中…';
  try{
    var r=await fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:ssid,pass:pass})});
    el('wifiMsg').textContent=r.ok?'已保存，设备重启中…':'保存失败';
  }catch(e){el('wifiMsg').textContent='网络错误';}
}
async function savePrinter(ev){
  ev.preventDefault();
  var ip=el('pip').value.trim(),did=el('pdid').value.trim(),ac=el('pac').value.trim();
  if(!ip||!did||!ac){el('printerMsg').textContent='请填写完整';return;}
  el('printerMsg').textContent='保存中…';
  try{
    var r=await fetch('/api/printer',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ip:ip,device_id:did,access_code:ac})});
    el('printerMsg').textContent=r.ok?'已保存，正在重连 MQTT':'保存失败';
  }catch(e){el('printerMsg').textContent='网络错误';}
}
async function doReset(){
  if(!confirm('确认恢复出厂设置？设备将重启进入配网模式。'))return;
  try{await fetch('/api/reset',{method:'POST'});alert('已恢复，设备重启中…');}catch(e){alert('网络错误');}
}
async function loadConfig(){
  try{
    var r=await fetch('/api/config',{cache:'no-store'});
    var d=await r.json();
    el('ssid').value=d.wifi_ssid||'';
    el('pass').value=d.wifi_password||'';
    el('pip').value=d.printer_ip||'';
    el('pdid').value=d.printer_device_id||'';
    el('pac').value=d.printer_access_code||'';
    el('rMode').value=String(d.relay_mode||0);
    var t=d.relay_trigger||1;
    el('tPrinting').checked=!!(t&1);
    el('tPaused').checked=!!(t&2);
    el('tError').checked=!!(t&4);
    el('tTemp').checked=!!(t&8);
    el('rDelayOn').value=d.relay_delay_on||0;
    el('rDelayOff').value=d.relay_delay_off||0;
    el('rTemp').value=d.relay_temp_threshold||0;
    el('rOverride').value=(d.relay_override_min!==undefined)?d.relay_override_min:5;
    applyFeatures(d);
  }catch(e){}
}
loadConfig();layoutStats();refresh();refreshRelay();
setInterval(refresh,3000);
setInterval(refreshRelay,3000);
</script>
</body>
</html>
)rawliteral";

// OTA 上传页
const char OTA_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>OTA 固件更新</title>
<style>
body{margin:0;background:#0f1319;color:#e6edf3;font-family:-apple-system,"Segoe UI",Roboto,Arial,sans-serif;padding:24px;}
.card{max-width:520px;margin:0 auto;background:#1a2028;border:1px solid #2a3340;border-radius:12px;padding:20px;}
h1{font-size:18px;margin:0 0 6px;}p{color:#8b98a7;font-size:13px;}
input[type=file]{display:block;margin:16px 0;color:#e6edf3;}
button{cursor:pointer;border:1px solid #2a3340;background:#232a33;color:#e6edf3;border-radius:9px;padding:10px 16px;}
progress{width:100%;height:16px;margin-top:12px;}
#msg{margin-top:12px;font-size:13px;}
a{color:#4fc3f7;}
</style></head><body>
<div class="card">
<h1>OTA 固件更新</h1>
<p>选择 Arduino IDE 导出的 .bin（项目 → 导出已编译的二进制文件），上传后设备自动重启。</p>
<p>可用 OTA 空间：<b id="slot">%OTA_FREE%</b>（仅接受 ESP8266 应用固件，文件头 0xE9）</p>
<form id="f"><input id="file" type="file" accept=".bin"><button type="submit">上传并更新</button></form>
<progress id="p" value="0" max="1"></progress>
<div id="msg"></div>
<p><a href="/">← 返回面板</a></p>
</div>
<script>
var OTA_MAX=%OTA_MAX%;
function el(i){return document.getElementById(i);}
document.getElementById('f').addEventListener('submit',function(e){
  e.preventDefault();
  var file=el('file').files[0];
  if(!file){el('msg').textContent='请选择 .bin 文件';return;}
  if(!/\.bin$/i.test(file.name)){el('msg').textContent='仅支持 .bin 固件文件';return;}
  if(file.size>OTA_MAX){el('msg').textContent='文件 '+Math.round(file.size/1024)+'KB 超出可用 OTA 空间（'+(OTA_MAX/1024)+'KB），已阻止上传';return;}
  var fd=new FormData();fd.append('firmware',file);
  var xhr=new XMLHttpRequest();
  xhr.open('POST','/upload');
  xhr.upload.onprogress=function(ev){if(ev.lengthComputable){el('p').max=ev.total;el('p').value=ev.loaded;el('msg').textContent='上传中 '+Math.round(ev.loaded*100/ev.total)+'%';}};
  xhr.onload=function(){el('msg').textContent=xhr.status===200?'上传成功，设备重启中…':'上传失败（'+(xhr.responseText||xhr.status)+'）';};
  xhr.onerror=function(){el('msg').textContent='网络错误';};
  xhr.send(fd);
});
</script>
</body></html>
)rawliteral";
