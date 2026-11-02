"use strict";

// Physical key indices and positions match scripts/keyboard_configurator.py.
const columns = {
  left: [
    [0,30,[[26,1.5],[25,1.5],[24,1.5],[23,1.5],[22]]],
    [98,30,[[31],[30],[29],[28],[27]]],
    [172,30,[[4],[3],[2],[1],[0]]],
    [246,25,[[9],[8],[7],[6],[5]]],
    [320,20,[[14],[13],[12],[11],[10]]],
    [394,25,[[18],[17],[16],[15]]],
    [468,30,[[21,1,1.5],[20,1,1.5],[19,1,1.5]]]
  ],
  right: [
    [124,30,[[0],[1,1,1.5],[2,1,1.5]]],
    [198,30,[[3],[4],[5],[6]]],
    [272,25,[[7],[8],[9],[10],[11]]],
    [346,20,[[12],[13],[14],[15],[16]]],
    [420,25,[[17],[18],[19],[20],[21]]],
    [494,30,[[22],[23],[24],[25],[26]]],
    [568,30,[[31,1.5],[30,1.5],[29,1.5],[28,1.5],[27]]]
  ]
};
const thumbs = {
  left: [[37,566,300],[36,632,300],[35,632,366],[34,632,432],[33,566,374,1,2],[32,500,374,1,2]],
  right: [[32,66,300],[33,0,300],[34,0,366],[35,0,432],[36,66,374,1,2],[37,132,374,1,2]]
};
function placements(hand) {
  const keys = [];
  for (const [x, top, column] of columns[hand]) {
    let y = top;
    for (const [index, w = 1, h = 1] of column) {
      keys.push({index, x, y, w: w * 58, h: h * 58});
      y += h * 58 + 8;
    }
  }
  for (const [index, x, y, w = 1, h = 1] of thumbs[hand]) {
    keys.push({index, x, y, w: w * 58, h: h * 58});
  }
  return keys;
}

function crc32(bytes) {
  let crc = 0xffffffff;
  for (let i = 0; i < bytes.length; ++i) {
    crc ^= i >= 60 && i < 64 ? 0 : bytes[i];
    for (let bit = 0; bit < 8; ++bit) crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
  }
  return (crc ^ 0xffffffff) >>> 0;
}
function decodeLayout(buffer) {
  const bytes = new Uint8Array(buffer), view = new DataView(buffer);
  if (bytes.length < 64 || String.fromCharCode(...bytes.slice(0,4)) !== "EKCF") throw new Error("Invalid layout response.");
  const version = view.getUint16(4,true);
  if (![1,2].includes(version) || view.getUint16(6,true) !== 64) throw new Error("Unsupported layout format. Reload after updating firmware.");
  const layerCount = bytes[12], defaultLayer = bytes[13];
  if (layerCount < 1 || layerCount > 8 || defaultLayer >= layerCount || bytes[14] !== 2 || bytes[15] !== 38 ||
      bytes.length !== 64 + 2 * layerCount * 38 * 4 || view.getUint32(8,true) !== bytes.length ||
      (version === 1 && bytes.slice(48,60).some(byte => byte !== 0)) || view.getUint32(60,true) !== crc32(bytes)) throw new Error("The layout response is incomplete or corrupt.");
  let displayLayout = "qwerty";
  if (version === 2) {
    const field = bytes.slice(48,60), end = field.indexOf(0);
    if (end < 0) throw new Error("Invalid display layout identifier.");
    displayLayout = validateDisplayLayout(String.fromCharCode(...field.slice(0,end)));
  }
  const nameBytes = bytes.slice(16,48), terminator = nameBytes.indexOf(0);
  if (terminator < 0) throw new Error("Invalid layout name.");
  const name = new TextDecoder().decode(nameBytes.slice(0,terminator));
  let offset = 64;
  const actions = Array.from({length: 2}, () => Array.from({length: layerCount}, () => Array.from({length: 38}, () => {
    const action = {type: bytes[offset], modifier: bytes[offset+1], keycode: bytes[offset+2], layer: bytes[offset+3],
      usage: view.getUint16(offset+2,true), x: view.getInt8(offset+2), y: view.getInt8(offset+3)};
    offset += 4;
    if (action.type > 11) throw new Error("Unknown key action in layout.");
    return action;
  })));
  return {name, layerCount, defaultLayer, displayLayout, actions};
}

const keyNames = {
  0:"Modifier",40:"Enter",41:"Esc",42:"Backspace",43:"Tab",44:"Space",45:"-",46:"=",47:"[",48:"]",49:"\\",50:"Non-US #",
  51:";",52:"'",53:"`",54:",",55:".",56:"/",57:"Caps Lock",70:"Print Screen",71:"Scroll Lock",72:"Pause",
  73:"Insert",74:"Home",75:"Page Up",76:"Delete",77:"End",78:"Page Down",79:"→",80:"←",81:"↓",82:"↑",
  83:"Num Lock",84:"KP /",85:"KP *",86:"KP -",87:"KP +",88:"KP Enter",98:"KP 0",99:"KP .",100:"Non-US backslash",101:"Menu"
};
function keyName(code) {
  if (code >= 4 && code <= 29) return String.fromCharCode(65 + code - 4);
  if (code >= 30 && code <= 39) return "1234567890"[code - 30];
  if (code >= 58 && code <= 69) return `F${code - 57}`;
  if (code >= 89 && code <= 97) return `KP ${code - 88}`;
  return keyNames[code] || `Key 0x${code.toString(16).padStart(2,"0")}`;
}
// Only identifiers are stored on the keyboard. Locale data is fetched by the
// browser from a fixed revision so saved preferences render consistently.
const hidIoRevision = "9fa4d76ecfd9bb23a10e6b32d79fc3ccb13ab69f";
const displayLayouts = {
  qwerty: {name:"QWERTY", path:"base/qwerty.json"},
  azerty: {name:"AZERTY", path:"base/azerty.json"},
  qwertz: {name:"QWERTZ", path:"base/qwertz.json"},
  colemak: {name:"Colemak", path:"keyboards/colemak.json"},
  en_us: {name:"English (US)", path:"keyboards/en_US.json"},
  fr_fr: {name:"French (France)", path:"keyboards/fr_FR.json"},
  de_de: {name:"German (Germany)", path:"keyboards/de_DE.json"}
};
const hidIoPaths = new Set(["base/base.json", "base/default.json", ...Object.values(displayLayouts).map(item => item.path)]);
const labelRequests = new Map();
let displayLabels = null, labelGeneration = 0;
function validateLabelMap(value) {
  if (!value || typeof value !== "object" || Array.isArray(value) || !Object.keys(value).length) throw new Error("Invalid label mapping.");
  const labels = Object.create(null);
  for (const [code, label] of Object.entries(value)) {
    if (!/^(0|[1-9][0-9]*)$/.test(code) || Number(code) > 101 ||
        typeof label !== "string" || !label.length || label.length > 100 || /[\u0000-\u001f\u007f]/.test(label)) throw new Error("Invalid key label.");
    labels[code] = label;
  }
  return labels;
}
function mergeHidIoLabels(labels, data) {
  if (!data || typeof data !== "object" || !Array.isArray(data.name) || !data.name.length ||
      !(data.parent === null || hidIoPaths.has(data.parent))) throw new Error("Invalid hid-io layout.");
  for (const field of ["to_hid_keyboard", "from_hid_keyboard"]) {
    if (data[field] !== undefined && (!data[field] || typeof data[field] !== "object" || Array.isArray(data[field]))) throw new Error("Invalid HID mapping table.");
  }
  const own = Object.create(null), namedKeys = new Set();
  for (const [code, name] of Object.entries(data.to_hid_keyboard ?? {})) {
    if (!/^0x[0-9a-f]{2,4}$/i.test(code) || typeof name !== "string" || !name.length || name.length > 100 || /[\u0000-\u001f\u007f]/.test(name)) throw new Error("Invalid HID mapping.");
    if (Number(code) <= 101) {
      own[Number(code)] = name;
      if ([...name].length > 1) namedKeys.add(Number(code));
    }
  }
  // Canonical names may be prose (e.g. "Acute E"). Prefer a one-character
  // alias from the same file, preserving child overrides over parent aliases.
  for (const [name, code] of Object.entries(data.from_hid_keyboard ?? {})) {
    if (typeof code !== "string" || !/^0x[0-9a-f]{2,4}$/i.test(code)) throw new Error("Invalid HID alias.");
    if (Number(code) <= 101 && [...name].length === 1 && !/[\u0000-\u001f\u007f]/.test(name) &&
        (own[Number(code)] === undefined || namedKeys.has(Number(code)))) own[Number(code)] = name;
  }
  Object.assign(labels, own);
}
async function fetchHidIoLabels(id) {
  const choice = Object.hasOwn(displayLayouts, id) && displayLayouts[id];
  if (!choice) throw new Error(`Unknown display layout: ${id}.`);
  const cacheKey = `ergodox-labels-v1:${hidIoRevision}:${id}`;
  try {
    const cached = localStorage.getItem(cacheKey);
    if (cached) return validateLabelMap(JSON.parse(cached));
  } catch (_) { /* Storage may be disabled, full, or contain invalid data. */ }
  const labels = Object.create(null), seen = new Set();
  const signal = AbortSignal.timeout(10000);
  async function load(path) {
    if (!hidIoPaths.has(path) || seen.has(path) || seen.size >= 5) throw new Error("Invalid hid-io parent chain.");
    seen.add(path);
    const response = await fetch(`https://raw.githubusercontent.com/hid-io/layouts/${hidIoRevision}/${path}`, {
      mode:"cors", credentials:"omit", referrerPolicy:"no-referrer", signal
    });
    if (!response.ok) throw new Error(`Layout download returned HTTP ${response.status}.`);
    const text = await response.text();
    if (text.length > 256 * 1024) throw new Error("Layout download is too large.");
    const data = JSON.parse(text);
    // Validate before following any remote-supplied parent path.
    mergeHidIoLabels(Object.create(null), data);
    if (data.parent !== null) await load(data.parent);
    mergeHidIoLabels(labels, data);
  }
  await load(choice.path);
  const result = validateLabelMap(labels);
  try { localStorage.setItem(cacheKey, JSON.stringify(result)); } catch (_) { /* Memory cache still works. */ }
  return result;
}
function getHidIoLabels(id) {
  if (!labelRequests.has(id)) {
    const request = fetchHidIoLabels(id).catch(error => { labelRequests.delete(id); throw error; });
    labelRequests.set(id, request);
  }
  return labelRequests.get(id);
}
async function loadDisplayLabels() {
  const generation = ++labelGeneration, id = layout.displayLayout ?? "qwerty";
  const status = document.getElementById("display-layout-status");
  const retry = document.getElementById("retry-labels");
  displayLabels = null;
  status.textContent = "Loading display labels…";
  retry.hidden = true;
  render();
  try {
    const labels = await getHidIoLabels(id);
    if (generation !== labelGeneration) return;
    displayLabels = labels;
    status.textContent = `${displayLayouts[id].name} labels`;
  } catch (error) {
    if (generation !== labelGeneration) return;
    status.textContent = `Using QWERTY labels. ${error.message}`;
    retry.hidden = !Object.hasOwn(displayLayouts, id);
  }
  render();
}
function displayKeyName(code) {
  return displayLabels?.[code] ?? keyName(code);
}
const consumerNames = {
  0x6f:"Brightness +",0x70:"Brightness −",0xb2:"Record",0xb3:"Fast forward",0xb4:"Rewind",0xb5:"Next track",0xb6:"Previous track",
  0xb7:"Stop",0xb8:"Eject",0xcd:"Play / pause",0xe2:"Mute",0xe9:"Volume +",0xea:"Volume −",0x18a:"Mail",0x192:"Calculator",
  0x221:"Search",0x223:"Browser home",0x224:"Browser back",0x225:"Browser forward",0x226:"Browser stop",0x227:"Refresh",0x22a:"Bookmarks"
};
function describe(action) {
  const mods = ["Ctrl","Shift","Alt","GUI","RCtrl","RShift","RAlt","RGUI"].filter((_,i) => action.modifier & (1 << i));
  switch (action.type) {
    case 0: return {lines:["↳"], detail:"Transparent — falls through to a lower active layer.", kind:"empty"};
    case 1: return {lines:["—"], detail:"No action — this key is disabled on this layer.", kind:"empty"};
    case 2: {
      const key = action.keycode ? displayKeyName(action.keycode) : "";
      const text = [...mods, key].filter(Boolean).join(" + ") || "No key";
      return {lines: mods.length && key ? [mods.join("+"), key] : [text], detail:`${text} · HID keycode 0x${action.keycode.toString(16).padStart(2,"0")}`, kind:""};
    }
    case 3: { const text = consumerNames[action.usage] || `Consumer 0x${action.usage.toString(16)}`; return {lines:[text], detail:`${text} · consumer usage 0x${action.usage.toString(16)}`, kind:"special"}; }
    case 4: { const text = ({0:"System release",0x81:"Power",0x82:"Sleep",0x83:"Wake"})[action.usage] || "System"; return {lines:[text], detail:text, kind:"special"}; }
    case 5: { const text = ["Left","Right","Middle","Back","Forward"].filter((_,i) => action.usage & (1 << i)).join(" + ") || "Release"; return {lines:["Mouse",text], detail:`Mouse buttons: ${text}`, kind:"special"}; }
    case 6: return {lines:["Mouse move",`${action.x}, ${action.y}`], detail:`Move mouse: X ${action.x}, Y ${action.y}`, kind:"special"};
    case 7: return {lines:["Scroll",String(action.y)], detail:`Mouse wheel: ${action.y}`, kind:"special"};
    case 8: { const text = ["Toggle","On","Off","Brighter","Dimmer"][action.usage] || String(action.usage); return {lines:["Lighting",text], detail:`Lighting: ${text}`, kind:"special"}; }
    default: { const text = ({9:"Hold",10:"Toggle",11:"Default"})[action.type]; return {lines:[text,`Layer ${action.layer}`], detail:`${text} layer ${action.layer}`, kind:"layer"}; }
  }
}

// JSON interchange stays in the browser; the device still receives compact bytes.
const actionTypes = ["transparent","none","key","consumer","system","mouse_button","mouse_move","mouse_wheel","lighting","layer_momentary","layer_toggle","layer_default"];
const modifierTokens = ["LEFT_CTRL","LEFT_SHIFT","LEFT_ALT","LEFT_GUI","RIGHT_CTRL","RIGHT_SHIFT","RIGHT_ALT","RIGHT_GUI"];
const buttonTokens = ["LEFT","RIGHT","MIDDLE","BACK","FORWARD"];
const lightingTokens = ["TOGGLE","ON","OFF","BRIGHTNESS_UP","BRIGHTNESS_DOWN"];
const keyTokens = Object.assign({},
  Object.fromEntries(Array.from({length:26},(_,i)=>[i+4,String.fromCharCode(65+i)])),
  Object.fromEntries(Array.from({length:10},(_,i)=>[i+30,"1234567890"[i]])),
  Object.fromEntries(Array.from({length:12},(_,i)=>[i+58,`F${i+1}`])),
  Object.fromEntries(Array.from({length:9},(_,i)=>[i+89,`KP_${i+1}`])),
  {0:"NONE",1:"ERROR_ROLLOVER",2:"POST_FAIL",3:"ERROR_UNDEFINED",40:"ENTER",41:"ESCAPE",42:"BACKSPACE",43:"TAB",44:"SPACE",45:"MINUS",46:"EQUAL",47:"LEFT_BRACKET",48:"RIGHT_BRACKET",49:"BACKSLASH",50:"NON_US_HASH",51:"SEMICOLON",52:"APOSTROPHE",53:"GRAVE",54:"COMMA",55:"DOT",56:"SLASH",57:"CAPS_LOCK",70:"PRINT_SCREEN",71:"SCROLL_LOCK",72:"PAUSE",73:"INSERT",74:"HOME",75:"PAGE_UP",76:"DELETE",77:"END",78:"PAGE_DOWN",79:"RIGHT",80:"LEFT",81:"DOWN",82:"UP",83:"NUM_LOCK",84:"KP_DIVIDE",85:"KP_MULTIPLY",86:"KP_MINUS",87:"KP_PLUS",88:"KP_ENTER",98:"KP_0",99:"KP_DOT",100:"NON_US_BACKSLASH",101:"MENU"});
const consumerTokens = {0:"NONE",0x6f:"BRIGHTNESS_UP",0x70:"BRIGHTNESS_DOWN",0xb2:"RECORD",0xb3:"FAST_FORWARD",0xb4:"REWIND",0xb5:"NEXT_TRACK",0xb6:"PREVIOUS_TRACK",0xb7:"STOP",0xb8:"EJECT",0xcd:"PLAY_PAUSE",0xe2:"MUTE",0xe9:"VOLUME_UP",0xea:"VOLUME_DOWN",0x18a:"MAIL",0x192:"CALCULATOR",0x221:"SEARCH",0x223:"BROWSER_HOME",0x224:"BROWSER_BACK",0x225:"BROWSER_FORWARD",0x226:"BROWSER_STOP",0x227:"BROWSER_REFRESH",0x22a:"BOOKMARKS"};
const systemTokens = {0:"NONE",0x81:"POWER",0x82:"SLEEP",0x83:"WAKE"};

function validateDisplayLayout(value) {
  if (typeof value !== "string" || !/^[a-z0-9][a-z0-9_-]{0,10}$/.test(value)) {
    throw new Error("Display layout must be 1–11 lowercase letters, digits, hyphens or underscores, starting with a letter or digit.");
  }
  return value;
}
function layoutToJSON(current) {
  return {format:"ergodox-layout",version:1,name:current.name,default_layer:current.defaultLayer,display_layout:current.displayLayout ?? "qwerty",
    layers:Array.from({length:current.layerCount},(_,layer)=>Object.fromEntries(["left","right"].map((hand,handIndex)=>[hand,
      current.actions[handIndex][layer].map((a,index)=>{
        const out={position:index+1,type:actionTypes[a.type]};
        switch(a.type) {
          case 2: out.key=keyTokens[a.keycode];out.modifiers=modifierTokens.filter((_,i)=>a.modifier&(1<<i));break;
          case 3: out.usage=consumerTokens[a.usage] ?? a.usage;break;
          case 4: out.usage=systemTokens[a.usage];break;
          case 5: out.buttons=buttonTokens.filter((_,i)=>a.usage&(1<<i));break;
          case 6: out.x=a.x;out.y=a.y;break;
          case 7: out.delta=a.y;break;
          case 8: out.command=lightingTokens[a.usage];break;
          case 9: case 10: case 11: out.layer=a.layer;break;
        }
        return out;
      })
    ])))};
}
function checkFields(value, allowed, path) {
  if (!value || typeof value!=="object" || Array.isArray(value)) throw new Error(`${path}: expected an object.`);
  for(const key of Object.keys(value)) if(!allowed.includes(key)) throw new Error(`${path}: unknown field "${key}".`);
}
function integer(value,min,max,path) {
  if(!Number.isInteger(value) || value<min || value>max) throw new Error(`${path}: expected an integer from ${min} to ${max}.`);
  return value;
}
function namedNumber(value,names,min,max,path) {
  if(typeof value==="number") return integer(value,min,max,path);
  const found=Object.entries(names).find(([,name])=>name===value);
  if(!found) throw new Error(`${path}: unknown value "${String(value)}".`);
  return Number(found[0]);
}
function maskFromNames(value,names,path) {
  if(!Array.isArray(value)) throw new Error(`${path}: expected an array of names.`);
  let mask=0;
  for(const name of value) {
    const bit=names.indexOf(name);
    if(bit<0 || (mask&(1<<bit))) throw new Error(`${path}: unknown or duplicate name "${String(name)}".`);
    mask|=1<<bit;
  }
  return mask;
}
function layoutFromJSON(value) {
  checkFields(value,["format","version","name","default_layer","display_layout","layers"],"Layout");
  if(value.format!=="ergodox-layout" || value.version!==1) throw new Error("Expected ergodox-layout JSON version 1.");
  if(typeof value.name!=="string" || value.name.includes("\0") || new TextEncoder().encode(value.name).length>31) throw new Error("Layout name must contain at most 31 UTF-8 bytes and no NUL character.");
  if(!Array.isArray(value.layers) || value.layers.length<1 || value.layers.length>8) throw new Error("Expected between 1 and 8 layers.");
  const result={name:value.name,displayLayout:validateDisplayLayout(value.display_layout === undefined ? "qwerty" : value.display_layout),layerCount:value.layers.length,defaultLayer:integer(value.default_layer,0,value.layers.length-1,"default_layer"),actions:[[],[]]};
  value.layers.forEach((layer,layerIndex)=>{
    checkFields(layer,["left","right"],`Layer ${layerIndex}`);
    ["left","right"].forEach((hand,handIndex)=>{
      const entries=layer[hand];
      if(!Array.isArray(entries) || entries.length!==38) throw new Error(`Layer ${layerIndex}, ${hand}: expected exactly 38 keys.`);
      result.actions[handIndex][layerIndex]=entries.map((entry,index)=>{
        const path=`Layer ${layerIndex}, ${hand} key ${index+1}`;
        if(!entry || entry.position!==index+1) throw new Error(`${path}: position must be ${index+1}.`);
        const type=actionTypes.indexOf(entry.type);
        if(type<0) throw new Error(`${path}: unknown action type.`);
        const fields=[[],[],["key","modifiers"],["usage"],["usage"],["buttons"],["x","y"],["delta"],["command"],["layer"],["layer"],["layer"]][type];
        checkFields(entry,["position","type",...fields],path);
        const a={type,modifier:0,keycode:0,usage:0,x:0,y:0,layer:0};
        switch(type) {
          case 2: a.keycode=namedNumber(entry.key,keyTokens,0,101,`${path}, key`);a.modifier=maskFromNames(entry.modifiers ?? [],modifierTokens,path);break;
          case 3: a.usage=namedNumber(entry.usage,consumerTokens,0,1023,path);break;
          case 4: a.usage=namedNumber(entry.usage,systemTokens,0,131,path);if(![0,129,130,131].includes(a.usage)) throw new Error(`${path}: invalid system usage.`);break;
          case 5: a.usage=maskFromNames(entry.buttons,buttonTokens,path);break;
          case 6: a.x=integer(entry.x,-127,127,path);a.y=integer(entry.y,-127,127,path);break;
          case 7: a.y=integer(entry.delta,-127,127,path);break;
          case 8: a.usage=namedNumber(entry.command,lightingTokens,0,4,path);break;
          case 9: case 10: case 11: a.layer=integer(entry.layer,0,result.layerCount-1,path);break;
        }
        return a;
      });
    });
  });
  return decodeLayout(encodeLayout(result).buffer);
}
function encodeLayout(current) {
  const bytes=new Uint8Array(64+2*current.layerCount*38*4),view=new DataView(bytes.buffer);
  bytes.set([69,75,67,70,2,0,64,0]);view.setUint32(8,bytes.length,true);
  bytes.set([current.layerCount,current.defaultLayer,2,38],12);
  bytes.set(new TextEncoder().encode(current.name),16);
  bytes.set(new TextEncoder().encode(validateDisplayLayout(current.displayLayout ?? "qwerty")),48);
  let offset=64;
  for(const hand of current.actions) for(const layer of hand) for(const a of layer) {
    bytes[offset]=a.type;
    switch(a.type) {
      case 2: bytes[offset+1]=a.modifier;bytes[offset+2]=a.keycode;break;
      case 3: case 4: case 5: case 8: view.setUint16(offset+2,a.usage,true);break;
      case 6: view.setInt8(offset+2,a.x);view.setInt8(offset+3,a.y);break;
      case 7: view.setInt8(offset+3,a.y);break;
      case 9: case 10: case 11: bytes[offset+3]=a.layer;break;
    }
    offset+=4;
  }
  view.setUint32(60,crc32(bytes),true);
  return bytes;
}

const svgNS = "http://www.w3.org/2000/svg";
function svgElement(tag, attrs, text) {
  const element = document.createElementNS(svgNS, tag);
  for (const [key,value] of Object.entries(attrs)) element.setAttribute(key, value);
  if (text !== undefined) element.textContent = text;
  return element;
}
let layout, selectedKey = null;
let pressedKeys = new Uint8Array(10);
let keyPollTimer = null, keyPollController = null, keyPollGeneration = 0;

function applyPressedKeys(bitmap) {
  if (bitmap.length !== 10 || (bitmap[9] & 0xf0)) throw new Error("Invalid key-state response.");
  pressedKeys = bitmap;
  for (const [hand, handIndex] of [["left",0],["right",1]]) {
    for (const key of document.querySelectorAll(`#${hand} .key`)) {
      const bit = handIndex * 38 + Number(key.dataset.index);
      key.classList.toggle("pressed", Boolean(bitmap[bit >> 3] & (1 << (bit & 7))));
    }
  }
}
function stopKeyPolling() {
  ++keyPollGeneration;
  clearTimeout(keyPollTimer);
  keyPollTimer = null;
  if (keyPollController) keyPollController.abort();
  keyPollController = null;
  applyPressedKeys(new Uint8Array(10));
}
function startKeyPolling() {
  stopKeyPolling();
  if (document.hidden || !layout || layoutTransferBusy) return;
  const generation = keyPollGeneration;
  let failures = 0;
  async function poll() {
    if (generation !== keyPollGeneration) return;
    const started = performance.now();
    const controller = new AbortController();
    keyPollController = controller;
    const timeout = setTimeout(() => controller.abort(), 1500);
    let delay = 50;
    try {
      const response = await fetch("/api/keys", {cache:"no-store", signal:controller.signal});
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      const bitmap = new Uint8Array(await response.arrayBuffer());
      if (generation !== keyPollGeneration) return;
      applyPressedKeys(bitmap);
      failures = 0;
      const status = document.getElementById("live-status");
      const message = "Live key display · press keys on either half";
      if (status.textContent !== message) status.textContent = message;
      status.classList.remove("offline");
      delay = Math.max(0, 50 - (performance.now() - started));
    } catch (error) {
      if (generation !== keyPollGeneration) return;
      applyPressedKeys(new Uint8Array(10));
      const status = document.getElementById("live-status");
      const message = "Live key display disconnected · reconnecting…";
      if (status.textContent !== message) status.textContent = message;
      status.classList.add("offline");
      delay = Math.min(2000, 250 * 2 ** Math.min(failures++, 3));
    } finally {
      clearTimeout(timeout);
      if (generation === keyPollGeneration) keyPollController = null;
    }
    if (generation === keyPollGeneration) keyPollTimer = setTimeout(poll, delay);
  }
  poll();
}
function showDetails(hand, index) {
  selectedKey = {hand,index};
  document.querySelectorAll(".key.selected").forEach(key => key.classList.remove("selected"));
  document.querySelector(`#${hand} [data-index="${index}"]`).classList.add("selected");
  const layer = Number(document.getElementById("layer").value);
  document.getElementById("detail-title").textContent = `${hand === "left" ? "Left" : "Right"} hand · Key ${index + 1} · Layer ${layer}`;
  document.getElementById("detail").textContent = describe(layout.actions[hand === "left" ? 0 : 1][layer][index]).detail;
}
function render() {
  const layer = Number(document.getElementById("layer").value);
  for (const [hand, handIndex] of [["left",0],["right",1]]) {
    const svg = document.getElementById(hand);
    svg.replaceChildren();
    for (const key of placements(hand)) {
      const action = layout.actions[handIndex][layer][key.index], description = describe(action);
      const group = svgElement("g", {class:`key ${description.kind}`, "data-index":key.index, tabindex:0, role:"button",
        "aria-label":`${hand} key ${key.index+1}: ${description.detail}`});
      group.append(svgElement("rect", {x:key.x,y:key.y,width:key.w,height:key.h,rx:7}));
      group.append(svgElement("title", {}, description.detail));
      group.append(svgElement("text", {x:key.x+key.w/2,y:key.y+11,class:"key-index"}, String(key.index+1)));
      description.lines.forEach((line,i) => {
        const text = svgElement("text", {x:key.x+key.w/2,y:key.y+key.h/2+4+(i-(description.lines.length-1)/2)*13}, line);
        if (line.length > 9) { text.setAttribute("textLength", key.w-10); text.setAttribute("lengthAdjust","spacingAndGlyphs"); }
        group.append(text);
      });
      group.addEventListener("click", () => showDetails(hand,key.index));
      group.addEventListener("keydown", event => {
        if (event.key === "Enter" || event.key === " ") { event.preventDefault(); showDetails(hand,key.index); }
      });
      svg.append(group);
    }
  }
  if (selectedKey) showDetails(selectedKey.hand, selectedKey.index);
  applyPressedKeys(pressedKeys);
}
let currentLayout = null, pendingLayout = null, pendingSaved = false, layoutTransferBusy = false;
function transferMessage(message, error = false) {
  const status=document.getElementById("transfer-status");
  status.textContent=message;status.classList.toggle("error",error);
}
function updateTransferControls() {
  document.getElementById("display-layout").disabled=layoutTransferBusy || !layout;
  document.getElementById("download-json").disabled=layoutTransferBusy || !layout;
  document.getElementById("upload-json").disabled=layoutTransferBusy || !currentLayout;
  document.getElementById("save-layout").disabled=layoutTransferBusy || !pendingLayout || pendingSaved;
  document.getElementById("discard-import").disabled=layoutTransferBusy;
}
function displayLayout(current) {
  layout=current;selectedKey=null;
  const displaySelector = document.getElementById("display-layout");
  const id = current.displayLayout ?? "qwerty";
  displaySelector.replaceChildren(...Object.entries(displayLayouts).map(([value, item]) => new Option(item.name, value)));
  if (!Object.hasOwn(displayLayouts, id)) displaySelector.append(new Option(`Unavailable (${id})`, id));
  displaySelector.value = id;
  document.getElementById("detail-title").textContent="Key details";
  document.getElementById("detail").textContent="Select a key to inspect its assigned action.";
  document.getElementById("layout-name").textContent=layout.name || "Unnamed layout";
  document.getElementById("summary").textContent=`${layout.layerCount} layers · 38 keys per hand · Default: layer ${layout.defaultLayer}`;
  const selector=document.getElementById("layer");
  selector.replaceChildren(...Array.from({length:layout.layerCount},(_,i)=>new Option(`Layer ${i}${i===layout.defaultLayer ? " (default)" : ""}`,i)));
  selector.value=String(layout.defaultLayer);updateTransferControls();loadDisplayLabels();
}
function changeDisplayLayout() {
  if (!layout || layoutTransferBusy) return;
  const input = document.getElementById("display-layout");
  try {
    const value = validateDisplayLayout(input.value);
    pendingLayout = {...layout, displayLayout:value};
    pendingSaved = false;
    displayLayout(pendingLayout);
    document.getElementById("mode-badge").textContent = "Unsaved configuration";
    document.getElementById("import-preview").hidden = false;
    document.getElementById("preview-message").textContent = "Display layout preference changed. Save to keyboard to keep it after reboot.";
    document.getElementById("save-layout").textContent = "Save to keyboard";
    document.getElementById("discard-import").textContent = "Discard preview";
    transferMessage("");
  } catch (error) {
    input.value = layout.displayLayout ?? "qwerty";
    transferMessage(error.message, true);
  }
}
function discardImport() {
  pendingLayout=null;pendingSaved=false;
  document.getElementById("import-preview").hidden=true;
  document.getElementById("mode-badge").textContent="Current layout";
  document.getElementById("layout-file").value="";
  if(currentLayout) displayLayout(currentLayout);
}
function downloadLayout() {
  if(!layout || layoutTransferBusy) return;
  const data=JSON.stringify(layoutToJSON(layout),null,2)+"\n";
  const url=URL.createObjectURL(new Blob([data],{type:"application/json"}));
  const link=document.createElement("a");link.href=url;
  link.download=(layout.name.replace(/[^a-zA-Z0-9_-]/g,"_") || "layout")+".json";
  document.body.append(link);link.click();link.remove();
  setTimeout(()=>URL.revokeObjectURL(url),1000);
}
async function selectLayoutFile() {
  const input=document.getElementById("layout-file"),file=input.files[0];
  if(!file || layoutTransferBusy) return;
  discardImport();
  layoutTransferBusy=true;updateTransferControls();
  transferMessage("Reading layout file…");
  try {
    if(file.size>256*1024) throw new Error("Layout file exceeds 256 KiB.");
    const parsed=JSON.parse(await file.text());
    pendingLayout=layoutFromJSON(parsed);pendingSaved=false;
    displayLayout(pendingLayout);
    document.getElementById("mode-badge").textContent="Import preview";
    document.getElementById("import-preview").hidden=false;
    document.getElementById("preview-message").textContent=`Previewing ${file.name}. Nothing has been saved. Save to this keyboard to apply after reboot.`;
    document.getElementById("save-layout").textContent="Save to keyboard";
    document.getElementById("discard-import").textContent="Discard preview";
    transferMessage("File validated. Review both halves and layers before saving.");
  } catch(error) {
    pendingLayout=null;
    transferMessage(`Unable to import: ${error.message}`,true);
  } finally {
    input.value="";layoutTransferBusy=false;updateTransferControls();
  }
}
async function saveImportedLayout() {
  if(!pendingLayout || pendingSaved || layoutTransferBusy) return;
  layoutTransferBusy=true;updateTransferControls();stopKeyPolling();
  transferMessage("Saving layout to keyboard… Keep USB connected.");
  try {
    const response=await fetch("/api/layout/import",{method:"POST",headers:{"Content-Type":"application/octet-stream"},
      body:encodeLayout(pendingLayout),cache:"no-store",signal:AbortSignal.timeout(15000)});
    const result=await response.json();
    if(!response.ok || result.saved!==true) throw new Error(result.error || `HTTP ${response.status}`);
    pendingSaved=true;
    document.getElementById("mode-badge").textContent="Saved for reboot";
    document.getElementById("preview-message").textContent="Saved to this USB-connected half. Release the boot chord and reconnect USB to use this layout. The other half's NVM is unchanged.";
    document.getElementById("save-layout").textContent="Saved";
    document.getElementById("discard-import").textContent="Show current layout";
    transferMessage("Layout saved. Reboot the keyboard to apply it.");
  } catch(error) {
    transferMessage(`Save not confirmed: ${error.message}. You can retry saving the same file.`,true);
  } finally {
    layoutTransferBusy=false;updateTransferControls();
    if(!document.hidden) startKeyPolling();
  }
}

async function loadLayout() {
  stopKeyPolling();
  const status = document.getElementById("status"), retry = document.getElementById("retry");
  status.hidden = false; status.className = ""; status.textContent = "Loading the keyboard’s layout…";
  retry.hidden = true;
  document.getElementById("layout").hidden = true;
  try {
    const response = await fetch("/api/layout", {cache:"no-store", signal:AbortSignal.timeout(10000)});
    if (!response.ok) throw new Error(`Keyboard returned HTTP ${response.status}.`);
    currentLayout = decodeLayout(await response.arrayBuffer());
    discardImport();
    transferMessage("");
    document.getElementById("layout").hidden = false;
    status.hidden = true;
    startKeyPolling();
  } catch (error) {
    status.textContent = `Unable to load layout. ${error.message} Check the USB connection and try again.`;
    status.className = "error"; retry.hidden = false;
  }
}
if (typeof document !== "undefined") {
  document.getElementById("retry-labels").addEventListener("click",loadDisplayLabels);
  document.getElementById("display-layout").addEventListener("change",changeDisplayLayout);
  document.getElementById("download-json").addEventListener("click",downloadLayout);
  document.getElementById("upload-json").addEventListener("click",()=>document.getElementById("layout-file").click());
  document.getElementById("layout-file").addEventListener("change",selectLayoutFile);
  document.getElementById("save-layout").addEventListener("click",saveImportedLayout);
  document.getElementById("discard-import").addEventListener("click",()=>{ discardImport();transferMessage(""); });
  document.getElementById("layer").addEventListener("change",render);
  document.getElementById("retry").addEventListener("click",loadLayout);
  document.addEventListener("visibilitychange", () => {
    if (document.hidden) {
      stopKeyPolling();
      document.getElementById("live-status").textContent = "Live key display paused";
    } else if (!document.getElementById("layout").hidden) {
      startKeyPolling();
    }
  });
  window.addEventListener("pagehide", stopKeyPolling);
  window.addEventListener("pageshow", event => {
    if (event.persisted && !document.getElementById("layout").hidden) startKeyPolling();
  });
  loadLayout();
}
