'use strict';
/* Browser-only localization. No setting or HomeKit name changes. */
const I18n=(()=>{
 const chinese={
  "Gateway is busy. Please retry.": "网关繁忙，请稍后重试",
  "Connecting": "连接中",
  "Waiting": "等待读数",
  "Daily fridge controls": "冰箱日常控制",
  "Inside temperature": "当前箱温",
  "Device target": "设备设定",
  "No device reading yet": "尚未收到设备状态",
  "Reading device status…": "正在读取设备状态…",
  "Set temperature": "设定温度",
  "Decrease by 1 degree Celsius": "降低1摄氏度",
  "Increase by 1 degree Celsius": "提高1摄氏度",
  "Target temperature; release to apply": "目标温度滑块，松手后提交",
  "Apply": "应用",
  "Cooling mode": "运行模式",
  "Power": "电源",
  "Device controls unavailable": "设备控制暂不可用",
  "More": "更多功能",
  "Backup · HomeKit · Log": "备用 · HomeKit · 记录",
  "Daily backup": "每日备用",
  "Loading…": "正在读取…",
  "Beijing time (UTC+8)": "北京时间",
  "Next action": "下个动作",
  "Manual override": "手动覆盖",
  "Saved sample": "备用样本",
  "No sample yet": "尚无样本",
  "Today’s learning": "今日学习",
  "Learns confirmed fixed-time actions sent to the Automation accessories. Repeats the latest complete day. Incomplete days keep the old sample; a complete empty day makes no changes.": "只学习固定时间、通过「自动化温控／电源／节能」下发且已获设备确认的动作。使用最近一个完整自然日，每日循环；不完整日保留旧样本，完整空白日不主动控制。",
  "The first partial day is excluded. Backup can run offline while powered; after a cold start, wait for clock sync or set time from this device.": "首次开启当天不算完整日。持续供电时断网继续；彻底断电后等待联网或本机校时。",
  "Enable learning and backup": "启用学习和备用",
  "Settings changed in another page. Reload before saving.": "配置已在其他页面更新，请重新载入。",
  "Discard and reload": "放弃修改并载入",
  "Preview & save": "预览并保存",
  "Set time from this device": "用本机时间校时",
  "No changes": "未修改",
  "Saved sample actions": "备用样本记录",
  "Previous": "上一页",
  "Next": "下一页",
  "Actions received today": "今日收到的动作",
  "Apple Home": "Apple 家庭",
  "Use Fridge Temperature / Power / ECO for manual control, and the Automation accessories for fixed-time automations. ECO on = ECO; off = MAX.": "日常使用「冰箱温控／冰箱电源／冰箱节能」；固定时间自动化使用对应「自动化」入口。节能开为ECO，关为MAX。",
  "Set Connectivity Probe → Single Press to turn on Probe Reply. The reply resets automatically.": "连通检测：单次按下配置为打开「检测回应」，回应会自动复位。",
  "Web language does not rename accessories in Apple Home. Names follow the installation language.": "网页语言不会更改家庭中的配件名称；名称以安装时选择为准。",
  "Recent activity": "最近记录",
  "This boot": "本次启动",
  "No activity yet": "暂无记录",
  "Connection & device": "连接与设备",
  "Mode": "模式",
  "Voltage": "电压",
  "Release the slider to apply, or enter a temperature and tap Apply. Device readback confirms each setting.": "拖动滑块松手即提交，输入温度后点“应用”。设置以设备回读为准。",
  "Confirm daily backup": "确认每日备用设置",
  "Saving settings and executing actions are separate. Only fresh device readback confirms an action.": "保存设置与设备执行分别确认。设备动作仍需新鲜回读才算成功。",
  "Back": "返回编辑",
  "Save settings": "确认保存",
  "Switch to English": "切换到英语",
  "Queued": "已排队",
  "Processing": "处理中",
  "Waiting for readback": "等待设备回读",
  "Confirmed by device": "设备已确认",
  "Completed": "已完成",
  "Failed": "失败",
  "Readback timed out": "回读超时",
  "Cancelled": "已取消",
  "Device writes are disabled in this build.": "当前仅开放查看，冰箱设置尚未完成实机验收。",
  "Fridge is disconnected.": "冰箱未连接。",
  "No usable device state yet.": "尚未收到可用设备状态。",
  "Device readings are stale. Waiting for an update.": "设备读数已过期，等待新状态。",
  "The fridge controls are locked.": "冰箱按键已锁定。",
  "Select Celsius on the fridge first.": "请先在冰箱上选择摄氏单位。",
  "This device layout does not support this control.": "当前协议暂不支持这项控制。",
  "State changed. Preview again before saving.": "状态已变化，请重新预览后保存。",
  "Save not confirmed. Reload, check the settings and retry.": "保存未确认，请刷新核对后重试。",
  "Records changed. Reloading.": "记录已更新，正在重新载入。",
  "No timely device readback confirmation.": "未及时收到设备回读确认。",
  "Result expired. Check the current device state.": "控制结果已过期，请查看设备当前状态。",
  "Device write was not confirmed.": "设备写入结果未确认。",
  "Request waited too long. Please submit again.": "请求排队过久，请重新提交。",
  "The queue is full. Please retry shortly.": "请求较多，请稍后重试。",
  "Record expired or gateway restarted. Check the current state.": "记录已过期或网关已重启，请查看当前状态。",
  "Request failed. Please retry.": "请求失败，请稍后重试。",
  "Gateway unavailable. Check your local network. Consult activity and device readings before retrying.": "网关暂时无法访问，请检查本地网络。请求结果请以最近记录和设备读数为准。",
  "The gateway returned an invalid operation record.": "网关返回了无效的请求记录。",
  "Queued, waiting…": "已排队，等待处理…",
  "Result is still unconfirmed. Check recent activity before resubmitting.": "请求结果仍未确认，请查看最近记录，不要重复提交。",
  "Gateway offline. Showing the last readings.": "网关离线，显示最后收到的读数。",
  "Readings are stale. Controls are unavailable.": "读数已过期，控制暂不可用。",
  "Temperature": "温度",
  "Not available": "尚未接入",
  "Paired": "已配对",
  "Not paired": "等待配对",
  "Waiting for the first automation probe reply": "自动化连通检测待配置或待首次回应",
  "Automation round-trip is healthy": "自动化往返正常",
  "Automation reply has expired": "自动化回应已超时",
  "Probe state unknown": "检测状态未知",
  "{health} · {count} pending controls": "{health} · {count} 条控制等待确认",
  "This boot: {received} automation requests, {confirmed} confirmed by the device.": "本次启动自动化入口收到 {received} 次请求，{confirmed} 次取得设备确认。",
  "Daily learning and backup are enabled.": "每日学习与备用已开启。",
  "Daily learning and backup are disabled.": "每日学习与备用已关闭。",
  "Fridge Power": "冰箱电源",
  "Fridge ECO": "冰箱节能",
  "Automation Power": "自动化电源",
  "Automation ECO": "自动化节能",
  "Fridge Temperature": "冰箱温控",
  "Automation Temperature": "自动化温控",
  "Gateway online": "网关在线",
  "Gateway offline": "网关离线",
  "Offline": "冰箱离线",
  "Stale": "读数过期",
  "Online": "已连接",
  "Updated {seconds}s ago": "读数更新于 {seconds} 秒前",
  "Last reading {seconds}s ago": "最后读数 {seconds} 秒前",
  "Unknown": "未知",
  "On": "开机",
  "Off": "关机",
  "Power unknown": "电源未知",
  "Turn off": "关闭冰箱",
  "Turn on": "开启冰箱",
  "Saved backup failed validation. Backup is stopped.": "备用存储校验失败，已停止备用。",
  "Backup storage is unavailable.": "备用存储不可用。",
  "Gateway unavailable. Last readings are retained.": "网关暂时无法访问，最后读数已保留。",
  "Device readings are unavailable. Controls are paused.": "设备读数暂不可用，控制已暂停。",
  "Daily backup: {message}": "每日备用：{message}",
  "Enter a whole-number temperature from {low} to {high}.": "请输入 {low} 至 {high} 的整数温度。",
  "Unsaved changes": "有未保存修改",
  "Disabled": "已关闭",
  "Waiting for time": "等待校时",
  "HomeKit primary": "HomeKit 主控",
  "Learning": "等待完整日样本",
  "Checking HomeKit": "正在检测 HomeKit",
  "Manual control takes priority": "手动操作优先",
  "Backup active": "每日备用接管",
  "Complete": "完整",
  "Partial day (not recorded from midnight)": "未从零点连续记录",
  "Time is not trusted": "时间尚不可靠",
  "Clock correction gap": "校时发生跳变",
  "Recording gap": "记录有缺口",
  "HomeKit or network interruption": "HomeKit 或网络中断",
  "Save unconfirmed": "保存未确认",
  "Action rejected": "动作被拒绝",
  "Action failed": "动作执行失败",
  "More than 64 daily actions": "超过每日64条上限",
  "Actions still pending at midnight": "午夜仍有动作待确认",
  "Fridge connection or reading gap": "冰箱连接或读数中断",
  "Unknown state": "状态未知",
  "Learning and backup are off. Manual controls remain available.": "学习和备用均已关闭；日常控制照常可用。",
  "The sample is an empty day. Backup will make no changes.": "备用样本为空白日，接管时不主动改变冰箱。",
  "{mode} · {count} backup actions": "{mode} · {count} 条备用动作",
  "No valid sample yet. Record one complete calendar day first; backup cannot take over yet.": "尚无可用样本，等待连续记录一个完整自然日；此时不会接管。",
  "Until {time}": "至 {time}",
  "Waiting for the next automation boundary": "等待下个自动化边界",
  "Waiting for trusted time": "等待可靠时间",
  "None": "无",
  "{days} days old": "{days} 天前",
  "{date} · {count} actions{age}": "{date} · {count} 条{age}",
  "No valid sample": "尚无可用样本",
  "{date} · {count} actions · {gap}": "{date} · {count} 条 · {gap}",
  "Learning is off": "未开启学习",
  "{summary} Sample not updated for {date}: {reason}.": "{summary}。{date} 未更新样本：{reason}。",
  "Disable learning and backup, leaving current fridge settings unchanged.": "将关闭学习和备用调度，不改变冰箱当前设置。",
  "Set to {temperature}": "设为 {temperature}",
  "Enable learning and backup, ending the previous manual override.": "将启用学习和备用，结束旧的手动覆盖。",
  "Align the current profile: {actions}.": "当前需要对齐：{actions}。",
  "{mode}. No backup action will be sent now.": "{mode}，当前不向冰箱发送备用动作。",
  "Waiting for state confirmation": "等待状态确认",
  "Settings saved. Check state and device readback for action results.": "设置已保存；设备执行情况以状态和回读为准。",
  "No valid sample yet.": "尚无可用样本。",
  "No records for today.": "尚无今日记录。",
  "{start}–{end} / {total} actions": "{start}–{end} / {total} 条",
  "Empty day; no changes": "空白日，不主动控制",
  "Pending": "待确认",
  "Confirmed": "已确认",
  "Manual control": "手动控制",
  "Backup preview": "备用预览",
  "Save backup settings": "保存备用设置",
  "Clock sync": "校时",
  "Backup control": "每日备用控制",
  "Home manual control": "家庭手动控制",
  "Home automation": "家庭自动化",
  "Web control": "网页控制",
  "USB control": "USB控制",
  "Gateway restarted. Check the current state.": "网关已重启，请查看当前状态。",
  "Time synchronized.": "时间已校准。",
  "· Using an IP address": "· 当前通过 IP 访问",
  "Switch to Chinese": "切换到中文",
  "Invalid request.": "请求格式不正确",
  "Use the gateway’s own address.": "请使用网关自己的地址访问",
  "Request origin is not allowed.": "请求来源不受支持",
  "Send JSON content.": "请发送JSON格式",
  "Request is too large or complex.": "请求过大或内容过于复杂",
  "Gateway is starting. Please wait.": "网关正在启动，请稍候",
  "Time is outside the supported range.": "校时时间超出支持范围",
  "Temperature is out of range.": "温度超出允许范围",
  "Connection closed. Please submit again.": "连接已断开，请重新提交",
  "A newer operation replaced this request.": "请求已被较新的操作替代",
  "Wait for a fresh device reading before resubmitting.": "需等待新的设备状态后重新提交",
  "Run the USB pair command on your computer, then scan its local QR code in Apple Home. This does not set up Wi-Fi.": "配对时请在电脑运行 pair 命令，使用家庭App扫描本地二维码；这不用于Wi-Fi配网。"
};
 let language='en';
 try{const saved=localStorage.getItem('frostbridge.language');if(saved==='en' || saved==='zh-CN')language=saved;}catch{}
 function t(message,values={}){return (language==='zh-CN'?(chinese[message]||message):message).replace(/\{(\w+)\}/g,(match,key)=>Object.prototype.hasOwnProperty.call(values,key)?String(values[key]):match);}
 function apply(){document.documentElement.lang=language;for(const e of document.querySelectorAll('[data-i18n]'))e.textContent=t(e.dataset.i18n);for(const e of document.querySelectorAll('[data-i18n-aria]'))e.setAttribute('aria-label',t(e.dataset.i18nAria));const button=document.getElementById('language');button.textContent=language==='en'?'中':'EN';button.setAttribute('aria-label',t(language==='en'?"Switch to Chinese":"Switch to English"));button.title=button.getAttribute('aria-label');}
 function set(next){language=next==='zh-CN'?'zh-CN':'en';try{localStorage.setItem('frostbridge.language',language);}catch{}apply();}
 return {t,apply,set,get language(){return language;}};
})();

const $=id=>document.getElementById(id), t=(message,values)=>I18n.t(message,values);
const labels={queued:"Queued",running:"Processing",awaiting_readback:"Waiting for readback",confirmed:"Confirmed by device",completed:"Completed",failed:"Failed",timed_out:"Readback timed out",cancelled:"Cancelled"};
const errors={gateway_busy:"Gateway is busy. Please retry.",writes_disabled:"Device writes are disabled in this build.",device_offline:"Fridge is disconnected.",state_unknown:"No usable device state yet.",state_stale:"Device readings are stale. Waiting for an update.",locked:"The fridge controls are locked.",unsupported_unit:"Select Celsius on the fridge first.",unsupported_layout:"This device layout does not support this control.",stale_preview:"State changed. Preview again before saving.",persistence_failed:"Save not confirmed. Reload, check the settings and retry.",records_changed:"Records changed. Reloading.",readback_timeout:"No timely device readback confirmation.",result_expired:"Result expired. Check the current device state.",transport_failed:"Device write was not confirmed.",request_expired:"Request waited too long. Please submit again.",queue_full:"The queue is full. Please retry shortly.",not_found:"Record expired or gateway restarted. Check the current state.",bad_request:"Invalid request.",forbidden_host:"Use the gateway’s own address.",forbidden_origin:"Request origin is not allowed.",unsupported_content_type:"Send JSON content.",payload_too_large:"Request is too large or complex.",gateway_not_ready:"Gateway is starting. Please wait.",invalid_time:"Time is outside the supported range.",out_of_range:"Temperature is out of range.",disconnected:"Connection closed. Please submit again.",superseded:"A newer operation replaced this request.",readback_required:"Wait for a fresh device reading before resubmitting."};
let state=null,online=false,receivedAt=0,refreshing=false,generation=null,dirty=false,conflict=false,expectedGeneration=0;
let targetDirty=false,commandBusy=false,planBusy=false,applying=false,previewId=null,previewView=null,lastActivity=null;
const messages=new Map();
const show=(id,message,values={})=>{messages.set(id,{message,values});$(id).textContent=t(message,values);};
const errorText=e=>errors[e.code]||(I18n.language==='zh-CN'?e.message:null)||"Request failed. Please retry.";
const fail=(message,values={})=>Object.assign(new Error(t(message,values)),{displayKey:message,displayValues:values});
const showError=(id,e)=>show(id,e.displayKey||e.message,e.displayValues);
const temperature=value=>value===null||value===undefined?'—':`${value} °C`;
const date=value=>value?new Intl.DateTimeFormat(I18n.language==='en'?'en-GB':'zh-CN',{timeZone:'Asia/Shanghai',month:'2-digit',day:'2-digit',hour:'2-digit',minute:'2-digit',hourCycle:'h23'}).format(new Date(value*1000)):'—';
async function request(path,body){
 const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),4000);
 try{
  const response=await fetch(path,{method:body===undefined?'GET':'POST',headers:body===undefined?{}:{'Content-Type':'application/json'},body:body===undefined?undefined:JSON.stringify(body),cache:'no-store',signal:controller.signal});
  const data=await response.json();if(!response.ok)throw Object.assign(fail(errorText(data.error||{})),{code:data.error?.code,status:response.status});return data;
 }catch(e){if(e.name==='AbortError'||e instanceof TypeError)throw fail("Gateway unavailable. Check your local network. Consult activity and device readings before retrying.");throw e;}finally{clearTimeout(timer);}
}
async function operation(path,body,progress){
 const accepted=await request(path,body);if(!accepted.poll||!accepted.poll.startsWith('/api/v1/'))throw fail("The gateway returned an invalid operation record.");
 progress?.("Queued, waiting…");const started=Date.now();
 while(Date.now()-started<16000){await new Promise(resolve=>setTimeout(resolve,350));const result=await request(accepted.poll);progress?.(labels[result.state]||result.state);if(result.state==='completed'||result.state==='confirmed')return result;if(['failed','timed_out','cancelled'].includes(result.state))throw fail(errorText({code:result.error_code,message:result.message}));}
 throw fail("Result is still unconfirmed. Check recent activity before resubmitting.");
}
function stale(){return !state||state.device.stale||!online||(state.device.age_ms!==null&&state.device.age_ms+Date.now()-receivedAt>30000);}
function availability(){
 const usable=online&&!!state,fresh=usable&&!stale(),writable=fresh&&state.gateway.writes_enabled&&!commandBusy;
 const temp=writable&&state.controls.temperature==='none',mode=writable&&state.controls.mode==='none',power=writable&&state.controls.power==='none';
 for(const id of ['minus','plus','target','temperature-range','send-temperature'])$(id).disabled=!temp;
 for(const id of ['eco','max'])$(id).disabled=!mode;
 $('power').disabled=!power;$('sync-clock').disabled=!usable||planBusy;$('backup-enabled').disabled=!usable||planBusy;$('preview-backup').disabled=!usable||generation===null||conflict||planBusy;
 const names={temperature:"Temperature",power:"Power",mode:"Mode"};
 const reason=!online?t("Gateway offline. Showing the last readings."):!state.gateway.writes_enabled?t(errors.writes_disabled):stale()?t("Readings are stale. Controls are unavailable."):commandBusy?'':Object.entries(state.controls).filter(([,error])=>error!=='none').map(([kind,error])=>`${t(names[kind])}: ${t(errors[error]||"Request failed. Please retry.")}`).join(' ');
 show('control-help',reason);$('control-help').hidden=!reason;
}
function renderStatus(){
 if(!state)return;const d=state.device,s=state.backup,hk=state.homekit;
 if(hk){
  show('homekit-state',!hk.enabled?"Not available":hk.paired?"Paired":"Not paired");
  const health={unconfigured:"Waiting for the first automation probe reply",healthy:"Automation round-trip is healthy",expired:"Automation reply has expired"};
  const healthy=t(health[hk.automation_health]||"Probe state unknown");show('homekit-summary',hk.pending?t("{health} · {count} pending controls",{health:healthy,count:hk.pending}):healthy);
  show('homekit-learning',t("This boot: {received} automation requests, {confirmed} confirmed by the device.",{received:hk.automation_requests,confirmed:hk.automation_confirmed})+' '+t(s.enabled?"Daily learning and backup are enabled.":"Daily learning and backup are disabled."));
  const names=["Fridge Power","Fridge ECO","Automation Power","Automation ECO","Fridge Temperature","Automation Temperature"];show('homekit-error',(hk.errors||[]).map((e,i)=>e!=='none'?`${t(names[i])}: ${t(errors[e]||"Request failed. Please retry.")}`:'').filter(Boolean).join(' '));
 }
 show('gateway',online?"Gateway online":"Gateway offline");const age=d.age_ms===null?null:d.age_ms+Date.now()-receivedAt;
 show('device-state',!d.connected?"Offline":stale()?"Stale":"Online");
 $('current').replaceChildren(document.createTextNode(d.current_c===null?'—':String(d.current_c)),Object.assign(document.createElement('span'),{textContent:' °C'}));
 show('actual-target',temperature(d.target_c));show('age',age===null?"No device reading yet":stale()?"Last reading {seconds}s ago":"Updated {seconds}s ago",{seconds:Math.max(0,Math.floor(age/1000))});
 show('power-state',d.power===null?"Unknown":d.power?"On":"Off");show('mode-state',d.mode===null?"Unknown":d.mode.toUpperCase());show('voltage',d.voltage_v===null?'—':`${d.voltage_v.toFixed(1)} V`);
 $('eco').setAttribute('aria-pressed',String(d.mode==='eco'));$('max').setAttribute('aria-pressed',String(d.mode==='max'));show('power',d.power===null?"Power unknown":d.power?"Turn off":"Turn on");
 const low=Math.max(-20,d.min_c??-20),high=Math.min(20,d.max_c??20);for(const id of ['target','temperature-range']){$(id).min=low;$(id).max=high;}
 if(!targetDirty&&d.target_c!==null){$('target').value=d.target_c;$('temperature-range').value=d.target_c;}
 renderBackup(s);
 const issues=[s.error_code,s.storage_error].filter(x=>x&&x!=='none').map(x=>t(errors[x]||"Request failed. Please retry."));
 if(s.store_status==='invalid')issues.push(t("Saved backup failed validation. Backup is stopped."));if(s.store_status==='unavailable')issues.push(t("Backup storage is unavailable."));show('backup-error',[...new Set(issues)].join(' '));
 const homekitIssues=(hk?.errors||[]).filter(e=>e&&e!=='none').map(e=>t(errors[e]||"Request failed. Please retry."));
 const notice=!online?t("Gateway unavailable. Last readings are retained."):!state.gateway.writes_enabled?t(errors.writes_disabled):stale()?t("Device readings are unavailable. Controls are paused."):issues.length?t("Daily backup: {message}",{message:[...new Set(issues)].join(' ')}):homekitIssues.length?t('HomeKit：{message}',{message:[...new Set(homekitIssues)].join(' ')}):'';
 show('notice',notice);$('notice').classList.toggle('warn',!!notice);$('notice').hidden=!notice;availability();
}
async function control(kind,value){
 if(commandBusy)return;commandBusy=true;availability();
 try{await operation('/api/v1/commands',{kind,value},text=>show('command-message',text));show('command-message',"Confirmed by device");if(kind==='set_temperature')targetDirty=false;}catch(e){showError('command-message',e);}finally{commandBusy=false;await refresh();availability();}
}
function submitTemperature(value){
 const n=Number(value),low=Number($('target').min),high=Number($('target').max);if(value===''||!Number.isInteger(n)||n<low||n>high){show('command-message',"Enter a whole-number temperature from {low} to {high}.",{low,high});return;}
 $('target').value=n;$('temperature-range').value=n;targetDirty=true;control('set_temperature',n);
}
function changed(){dirty=true;previewId=null;show('draft-state',"Unsaved changes");}
const modes={disabled:"Disabled",waiting_for_time:"Waiting for time",homekit_primary:"HomeKit primary",learning:"Learning",checking_link:"Checking HomeKit",manual_override:"Manual control takes priority",replaying:"Backup active"};
const gaps={none:"Complete",partial_start:"Partial day (not recorded from midnight)",time_unknown:"Time is not trusted",clock_changed:"Clock correction gap",input_gap:"Recording gap",homekit_unavailable:"HomeKit or network interruption",persistence:"Save unconfirmed",rejected:"Action rejected",failed:"Action failed",overflow:"More than 64 daily actions",pending_at_midnight:"Actions still pending at midnight",device_unavailable:"Fridge connection or reading gap"};
const dayText=day=>day===null||day===undefined?'—':new Date(day*86400000).toISOString().slice(0,10);
const pages={sample:{url:null,next:null,previous:null,key:null,busy:false,data:null},today:{url:null,next:null,previous:null,key:null,busy:false,data:null}};
function acceptBackup(s){generation=s.generation;$('backup-enabled').checked=s.enabled;dirty=false;conflict=false;previewId=null;$('backup-conflict').hidden=true;show('draft-state',"No changes");}
function renderBackup(s){
 if(s.generation>=expectedGeneration){if(generation===null||(!dirty&&!planBusy&&generation!==s.generation))acceptBackup(s);else if(generation!==s.generation){conflict=true;$('backup-conflict').hidden=false;}}
 show('backup-state',modes[s.current.state]||"Unknown state");show('backup-summary',!s.enabled?t("Learning and backup are off. Manual controls remain available."):s.sample?s.sample.count===0?t("The sample is an empty day. Backup will make no changes."):t("{mode} · {count} backup actions",{mode:t(modes[s.current.state]||"Unknown state"),count:s.sample.count}):t("No valid sample yet. Record one complete calendar day first; backup cannot take over yet."));
 show('clock',s.time_trusted?date(s.unix_seconds+Math.floor((Date.now()-receivedAt)/1000)):"Waiting for time");show('boundary',date(s.current.next_action_ms/1000));
 show('override',s.manual_override?(s.override_until_ms?t("Until {time}",{time:date(s.override_until_ms/1000)}):s.time_trusted?"Waiting for the next automation boundary":"Waiting for trusted time"):"None");
 show('sample-summary',s.sample?t("{date} · {count} actions{age}",{date:dayText(s.sample.day),count:s.sample.count,age:s.sample.age_days===null?'':` · ${t("{days} days old",{days:s.sample.age_days})}`}):"No valid sample");
 show('today-summary',s.enabled?t("{date} · {count} actions · {gap}",{date:dayText(s.today.day),count:s.today.count,gap:t(gaps[s.today.gap]||"Unknown state")}):"Learning is off");
 if(s.last_closed&&s.last_closed.gap!=='none')show('backup-summary',t("{summary} Sample not updated for {date}: {reason}.",{summary:$('backup-summary').textContent,date:dayText(s.last_closed.day),reason:t(gaps[s.last_closed.gap]||"Unknown state")}));
}
function previewEffect(effect,enabled){
 if(!enabled)return t("Disable learning and backup, leaving current fridge settings unchanged.");
 const actions=[];if(effect.target_c!==null)actions.push(t("Set to {temperature}",{temperature:temperature(effect.target_c)}));if(effect.mode!==null)actions.push(effect.mode.toUpperCase());if(effect.power!==null)actions.push(t(effect.power?"On":"Off"));
 return t("Enable learning and backup, ending the previous manual override.")+' '+(effect.state==='replaying'&&actions.length?t("Align the current profile: {actions}.",{actions:actions.join(I18n.language==='en'?', ':'、')}):t("{mode}. No backup action will be sent now.",{mode:t(modes[effect.state]||"Waiting for state confirmation")}));
}
async function preview(){
 try{planBusy=true;availability();const enabled=$('backup-enabled').checked,result=await operation('/api/v1/backup/preview',{base_generation:generation,enabled},text=>show('backup-message',text));previewId=result.preview_id;previewView={effect:result.effect,enabled};show('preview-effect',previewEffect(result.effect,enabled));$('preview-dialog').showModal();}catch(e){showError('backup-message',e);planBusy=false;availability();}
}
async function apply(){
 if(!previewId||applying)return;applying=true;$('apply-backup').disabled=true;$('cancel-preview').disabled=true;
 try{const result=await operation('/api/v1/backup/apply',{preview_id:previewId},text=>show('backup-message',text));expectedGeneration=result.generation;dirty=false;show('backup-message',"Settings saved. Check state and device readback for action results.");}catch(e){showError('backup-message',e);}finally{$('preview-dialog').close();previewId=null;previewView=null;planBusy=false;applying=false;$('apply-backup').disabled=false;$('cancel-preview').disabled=false;await refresh();availability();}
}
function renderRecords(kind,data){
 const container=$(kind+'-records');container.replaceChildren();const heading=document.createElement('p');heading.className='hint';heading.textContent=`${dayText(data.day)} · ${data.total?t("{start}–{end} / {total} actions",{start:data.offset+1,end:data.offset+data.records.length,total:data.total}):t("Empty day; no changes")}`;container.append(heading);
 for(const row of data.records){const li=document.createElement('p'),time=new Date(row.second*1000).toISOString().slice(11,19);const action=row.setting==='temperature'?temperature(row.value):row.setting==='power'?t(row.value?"On":"Off"):String(row.value).toUpperCase();li.textContent=`${time} · ${action} · ${t(({pending:"Pending",confirmed:"Confirmed",failed:"Failed"})[row.outcome]||"Unknown state")}`;container.append(li);}
}
async function records(kind,url){
 const page=pages[kind];if(page.busy||!state||!$('more').open||!$('backup-details').open||!$(kind+'-details').open)return;
 const meta=kind==='sample'?state.backup.sample:state.backup.today,key=`${state.boot}/${meta?.revision}`;if(!url&&page.key===key)return;url=url||meta?.records_url;
 if(!url){show(kind+'-records',kind==='sample'?"No valid sample yet.":"No records for today.");page.key=key;page.next=page.previous=page.data=null;for(const side of ['next','previous'])$(kind+'-'+side).disabled=true;return;}
 page.busy=true;
 try{const data=await request(url);if(state.boot!==data.boot||(kind==='sample'?state.backup.sample:state.backup.today)?.revision!==data.revision)return;messages.delete(kind+'-records');renderRecords(kind,data);Object.assign(page,{key,url,next:data.next,previous:data.previous,data});}catch(e){page.key=page.data=null;showError(kind+'-records',e);if(e.code==='records_changed')await refresh();}finally{page.busy=false;for(const side of ['next','previous'])$(kind+'-'+side).disabled=!online||!page[side];}
}
function renderActivity(data){
 $('activity').replaceChildren();const rows=[...(data.operations||[]),...(data.automatic||[])].sort((a,b)=>(b.created_ms??b.recorded_ms)-(a.created_ms??a.recorded_ms)).slice(0,16);
 if(!rows.length){const li=document.createElement('li');li.className='hint';li.textContent=t("No activity yet");$('activity').append(li);}
 for(const row of rows){const li=document.createElement('li'),title=document.createElement('strong'),detail=document.createElement('span');const types={command:"Manual control",preview:"Backup preview",backup_save:"Save backup settings",clock:"Clock sync",local_replay:"Backup control"},sources={homekit_manual:"Home manual control",homekit_automation:"Home automation",web:"Web control",usb:"USB control"};title.textContent=`${t(row.type==='command'?(sources[row.source]||types.command):(types[row.type]||row.type))} · ${t(labels[row.state]||row.state)}`;detail.textContent=row.error_code&&row.error_code!=='none'?t(errors[row.error_code]||"Request failed. Please retry."):row.id;li.append(title,detail);$('activity').append(li);}
}
async function activity(){try{lastActivity=await request('/api/v1/activity');renderActivity(lastActivity);}catch(e){lastActivity=null;throw e;}}
async function refresh(){
 if(refreshing)return;refreshing=true;
 try{const next=await request('/api/v1/status');if(state&&state.boot!==next.boot){previewId=previewView=null;generation=null;expectedGeneration=0;lastActivity=null;for(const p of Object.values(pages)){p.key=p.data=null;}if($('preview-dialog').open){$('preview-dialog').close();planBusy=false;}show('command-message',"Gateway restarted. Check the current state.");}state=next;online=true;receivedAt=Date.now();renderStatus();if($('more').open&&$('records-details').open)await activity();for(const kind of ['sample','today'])await records(kind);}catch(e){online=false;show('gateway',"Gateway offline");showError('notice',e);if(state)renderStatus();else availability();}finally{refreshing=false;}
}
$('target').addEventListener('input',()=>{targetDirty=true;if($('target').validity.valid)$('temperature-range').value=$('target').value;});
$('target').addEventListener('keydown',e=>{if(e.key==='Enter')submitTemperature($('target').value);});
$('temperature-range').addEventListener('input',()=>{targetDirty=true;$('target').value=$('temperature-range').value;});$('temperature-range').addEventListener('change',()=>submitTemperature($('temperature-range').value));
$('send-temperature').onclick=()=>submitTemperature($('target').value);$('minus').onclick=()=>submitTemperature(Number($('target').value)-1);$('plus').onclick=()=>submitTemperature(Number($('target').value)+1);
$('eco').onclick=()=>control('set_mode','eco');$('max').onclick=()=>control('set_mode','max');$('power').onclick=()=>{if(state.device.power!==null)control('set_power',!state.device.power);};
$('backup-enabled').onchange=()=>{changed();availability();};$('reload-backup').onclick=()=>{if(state){acceptBackup(state.backup);availability();}};$('preview-backup').onclick=preview;$('apply-backup').onclick=apply;
function cancelPreview(){previewId=previewView=null;planBusy=false;availability();}
$('cancel-preview').onclick=()=>{$('preview-dialog').close();cancelPreview();};$('preview-dialog').addEventListener('cancel',e=>{if(applying)e.preventDefault();else cancelPreview();});
$('sync-clock').onclick=async()=>{planBusy=true;availability();try{await operation('/api/v1/clock',{unix_seconds:Math.floor(Date.now()/1000)},text=>show('backup-message',text));show('backup-message',"Time synchronized.");}catch(e){showError('backup-message',e);}finally{planBusy=false;await refresh();availability();}};
for(const kind of ['sample','today']){$(kind+'-details').addEventListener('toggle',()=>records(kind));for(const side of ['next','previous'])$(kind+'-'+side).onclick=()=>records(kind,pages[kind][side]);}
$('records-details').addEventListener('toggle',()=>{if($('more').open&&$('records-details').open)activity().catch(e=>showError('activity',e));});$('more').addEventListener('toggle',()=>{if($('more').open)refresh();});$('backup-details').addEventListener('toggle',()=>{for(const kind of ['sample','today'])records(kind);});
$('language').onclick=()=>{I18n.set(I18n.language==='en'?'zh-CN':'en');for(const [id,value] of messages)show(id,value.message,value.values);if(state)renderStatus();if(previewView&&$('preview-dialog').open)show('preview-effect',previewEffect(previewView.effect,previewView.enabled));for(const kind of ['sample','today']){const page=pages[kind],meta=kind==='sample'?state?.backup.sample:state?.backup.today;if(page.data&&meta?.revision===page.data.revision)renderRecords(kind,page.data);}if(lastActivity)renderActivity(lastActivity);};
I18n.apply();if(location.hostname!=='frostbridge.local')show('ip-hint',"· Using an IP address");refresh();setInterval(refresh,2500);setInterval(()=>{if(state)renderStatus();},1000);
