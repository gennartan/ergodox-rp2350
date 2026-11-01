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
  if (view.getUint16(4,true) !== 1 || view.getUint16(6,true) !== 64) throw new Error("Unsupported layout format. Reload after updating firmware.");
  const layerCount = bytes[12], defaultLayer = bytes[13];
  if (layerCount < 1 || layerCount > 8 || defaultLayer >= layerCount || bytes[14] !== 2 || bytes[15] !== 38 ||
      bytes.length !== 64 + 2 * layerCount * 38 * 4 || view.getUint32(8,true) !== bytes.length ||
      bytes.slice(48,60).some(byte => byte !== 0) || view.getUint32(60,true) !== crc32(bytes)) throw new Error("The layout response is incomplete or corrupt.");
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
  return {name, layerCount, defaultLayer, actions};
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
      const key = action.keycode ? keyName(action.keycode) : "";
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
  if (document.hidden || !layout) return;
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
async function loadLayout() {
  stopKeyPolling();
  const status = document.getElementById("status"), retry = document.getElementById("retry");
  status.hidden = false; status.className = ""; status.textContent = "Loading the keyboard’s layout…";
  retry.hidden = true;
  document.getElementById("layout").hidden = true;
  try {
    const response = await fetch("/api/layout", {cache:"no-store", signal:AbortSignal.timeout(10000)});
    if (!response.ok) throw new Error(`Keyboard returned HTTP ${response.status}.`);
    layout = decodeLayout(await response.arrayBuffer());
    selectedKey = null;
    document.getElementById("detail-title").textContent = "Key details";
    document.getElementById("detail").textContent = "Select a key to inspect its assigned action.";
    document.getElementById("layout-name").textContent = layout.name || "Unnamed layout";
    document.getElementById("summary").textContent = `${layout.layerCount} layers · 38 keys per hand · Default: layer ${layout.defaultLayer}`;
    const selector = document.getElementById("layer");
    selector.replaceChildren(...Array.from({length:layout.layerCount}, (_,i) => new Option(`Layer ${i}${i === layout.defaultLayer ? " (default)" : ""}`,i)));
    selector.value = String(layout.defaultLayer);
    render();
    document.getElementById("layout").hidden = false;
    status.hidden = true;
    startKeyPolling();
  } catch (error) {
    status.textContent = `Unable to load layout. ${error.message} Check the USB connection and try again.`;
    status.className = "error"; retry.hidden = false;
  }
}
if (typeof document !== "undefined") {
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
