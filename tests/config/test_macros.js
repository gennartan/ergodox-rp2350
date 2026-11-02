"use strict";
const assert = require("node:assert/strict"), fs = require("node:fs"), vm = require("node:vm"), path = require("node:path");
const context = vm.createContext({TextDecoder,TextEncoder});
vm.runInContext(fs.readFileSync(path.join(__dirname,"../../src/web/app.js"),"utf8"),context);
const run = code => vm.runInContext(code,context);
const fromJSON=run('layoutFromJSON'), toJSON=run('layoutToJSON'), encode=run('encodeLayout'), decode=run('decodeLayout');
const value = {format:"ergodox-layout",version:1,name:"Macro test",default_layer:0,display_layout:"qwerty",
  macros:Array.from({length:8},()=>[]), layers:[Object.fromEntries(["left","right"].map(hand => [hand,Array.from({length:38},(_,i)=>({position:i+1,type:"none"}))]))]};
value.macros[0] = [{key:"NON_US_HASH",modifiers:["RIGHT_ALT"],delay_ms:20},{key:"SPACE",modifiers:[],delay_ms:0}];
value.layers[0].left[0] = {position:1,type:"macro",macro:0};
value.layers[0].right[37] = {position:38,type:"macro",macro:0};
let current = fromJSON(value), bytes = encode(current);
assert.equal(bytes[4],5); assert.equal(bytes.length,1040);
assert.deepEqual(JSON.parse(JSON.stringify(toJSON(decode(bytes.buffer)))),value);
assert.equal(current.macros[0][0].modifier,64); assert.equal(current.macros[0][0].keycode,50);
for (const change of [
  v=>v.macros.pop(), v=>v.macros[0]=[], v=>v.macros[0]=Array(17).fill(v.macros[0][0]),
  v=>v.macros[0][0].delay_ms=5001, v=>v.macros[0][0].delay_ms=-1,
  v=>v.macros[0][0].key="BAD", v=>v.macros[0][0].extra=true,
  v=>v.macros[0][0].modifiers=["RIGHT_ALT","RIGHT_ALT"],
  v=>v.layers[0].left[0].macro=8, v=>v.layers[0].left[0].macro=1
]) {
  const bad=JSON.parse(JSON.stringify(value)); change(bad); assert.throws(()=>fromJSON(bad));
}
// Valid CRC, invalid extension headers/steps/references must still fail.
for (const [offset,byte] of [[368,17],[369,1],[373,102],[375,255],[380,1],[66,8]]) {
  const bad=bytes.slice(); bad[offset]=byte;
  context.badBytes=bad;
  const crc=run('crc32(badBytes)'); new DataView(bad.buffer).setUint32(60,crc,true);
  assert.throws(()=>decode(bad.buffer));
}
// All slots and maximum length fit the single-sector NVS record limit.
const maximum=JSON.parse(JSON.stringify(value));
maximum.layers=Array.from({length:8},()=>JSON.parse(JSON.stringify(value.layers[0])));
maximum.macros=Array.from({length:8},()=>Array.from({length:16},()=>({key:"A",modifiers:["LEFT_CTRL"],delay_ms:5000})));
assert.equal(encode(fromJSON(maximum)).length,3168);
fs.writeFileSync('/tmp/ergodox-js-macro-test.bin',encode(fromJSON(maximum)));
// Optional interoperability with the firmware's encoder.
if (process.argv[2]) {
  const file=fs.readFileSync(process.argv[2]);
  const parsed=decode(file.buffer.slice(file.byteOffset,file.byteOffset+file.byteLength));
  assert.equal(parsed.macros[0][0].keycode,50);
  assert.equal(parsed.macros[0][0].modifier,64);
  assert.equal(parsed.macros[0][2].delayMs,5000);
  assert.deepEqual(Buffer.from(encode(fromJSON(toJSON(parsed)))),file);
}
// Stage an edited shared slot; cancellation/discard never changes live data.
const controls=new Map();
context.document={getElementById(id){if(!controls.has(id)) controls.set(id,{value:"",classList:{toggle(){}},close(){}}); return controls.get(id);}};
context.currentFixture=current;
run('render=()=>{}; displayLayout=value=>{layout=value}; currentLayout=currentFixture; layout=currentLayout; editorMacros=layout.macros.map(m=>m.map(s=>({...s}))); editorMacros[0][0].delayMs=200; editingKey={hand:0,layer:0,index:1}');
controls.set('action-type',{value:'12'});controls.set('edit-macro-slot',{value:'0'});
run('applyKeyEdit({preventDefault(){}})');
assert.equal(run('pendingLayout.macros[0][0].delayMs'),200);
assert.equal(run('currentLayout.macros[0][0].delayMs'),20);
assert.equal(run('pendingLayout.actions[0][0][1].type'),12);
assert.equal(controls.get('save-layout').disabled,false);
run('discardImport()');
assert.equal(run('layout===currentLayout && pendingLayout===null'),true);
console.log('Macro JSON/binary interoperability, limits, invalid inputs, shared-slot staging, and discard passed');
