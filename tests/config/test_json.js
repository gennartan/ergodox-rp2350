"use strict";
const assert=require("node:assert/strict"), fs=require("node:fs"), vm=require("node:vm"), path=require("node:path");
const context=vm.createContext({TextDecoder,TextEncoder});
vm.runInContext(fs.readFileSync(path.join(__dirname,"../../src/web/app.js"),"utf8"),context);
const fromJSON=vm.runInContext("layoutFromJSON",context),toJSON=vm.runInContext("layoutToJSON",context),encode=vm.runInContext("encodeLayout",context),decode=vm.runInContext("decodeLayout",context);
function fixture(){return {format:"ergodox-layout",version:1,name:"é test",default_layer:7,display_layout:"qwerty",layers:Array.from({length:8},()=>({left:Array.from({length:38},(_,i)=>({position:i+1,type:"none"})),right:Array.from({length:38},(_,i)=>({position:i+1,type:"transparent"}))}))}}
const value=fixture();
const samples=[
  {type:"key",key:"A",modifiers:["LEFT_CTRL","RIGHT_ALT"]},
  {type:"consumer",usage:"VOLUME_UP"}, {type:"consumer",usage:777},
  {type:"system",usage:"WAKE"}, {type:"mouse_button",buttons:["LEFT","RIGHT","MIDDLE","BACK","FORWARD"]},
  {type:"mouse_move",x:-127,y:127}, {type:"mouse_wheel",delta:-2},
  {type:"lighting",command:"BRIGHTNESS_DOWN"}, {type:"layer_momentary",layer:1},
  {type:"layer_toggle",layer:2}, {type:"layer_default",layer:3}
];
samples.forEach((a,i)=>value.layers[0].left[i]={position:i+1,...a});
value.layers[7].right[37]={position:38,type:"key",key:"NONE",modifiers:["RIGHT_ALT"]};
const layout=fromJSON(value),blob=encode(layout);
assert.equal(blob.length,3168);
assert.equal(layout.name,"é test");
assert.equal(layout.actions[0][0][0].modifier,65);
assert.equal(layout.actions[0][0][5].x,-127);
assert.equal(layout.actions[1][7][37].modifier,64);
assert.deepEqual(JSON.parse(JSON.stringify(toJSON(decode(blob.buffer)))),value);
const normalized=encode(fromJSON(JSON.parse(JSON.stringify(toJSON(layout)))));
assert.deepEqual(Array.from(normalized),Array.from(blob));
assert.equal(layout.displayLayout,"qwerty");
const custom=fixture(); custom.display_layout="be-azerty";
assert.equal(decode(encode(fromJSON(custom)).buffer).displayLayout,"be-azerty");
assert.equal(toJSON(fromJSON(custom)).display_layout,"be-azerty");
const legacy=fixture(); delete legacy.display_layout;
assert.equal(fromJSON(legacy).displayLayout,"qwerty");
const badCases=[
  [v=>v.display_layout="x".repeat(128),/Display layout/],
  [v=>v.display_layout="../bad",/Display layout/],
  [v=>v.version=2,/version/], [v=>v.extra=1,/unknown field/],
  [v=>v.name="é".repeat(16),/31 UTF-8/], [v=>v.name="x\0y",/NUL/],
  [v=>v.default_layer=8,/integer/], [v=>v.layers=[],/1 and 8/],
  [v=>v.layers[0].left.pop(),/38 keys/], [v=>v.layers[0].left[0].position=2,/position/],
  [v=>v.layers[0].left[0].type="typo",/unknown action/],
  [v=>v.layers[0].left[0]={position:1,type:"key",key:"BAD"},/unknown value/],
  [v=>v.layers[0].left[0]={position:1,type:"key",key:"A",modifiers:["LEFT_SHIFT","LEFT_SHIFT"]},/duplicate/],
  [v=>v.layers[0].left[0]={position:1,type:"mouse_move",x:-128,y:0},/integer/],
  [v=>v.layers[0].left[0]={position:1,type:"layer_toggle",layer:8},/integer/],
  [v=>v.layers[0].left[0]={position:1,type:"none",key:"A"},/unknown field/],
  [v=>v.layers[0].left[0]={position:1,type:"system",usage:128},/invalid system/],
  [v=>v.layers[0].left[0]={position:1,type:"consumer",usage:1024},/integer/]
];
for(const [mutate,error] of badCases){const bad=fixture();mutate(bad);assert.throws(()=>fromJSON(bad),error)}
const largest=fixture();
for(const layer of largest.layers)for(const hand of ["left","right"])for(let i=0;i<38;i++) layer[hand][i]={position:i+1,type:"key",key:"NON_US_BACKSLASH",modifiers:["LEFT_CTRL","LEFT_SHIFT","LEFT_ALT","LEFT_GUI","RIGHT_CTRL","RIGHT_SHIFT","RIGHT_ALT","RIGHT_GUI"]};
assert.ok(Buffer.byteLength(JSON.stringify(largest,null,2))<=256*1024,"valid exports exceed upload file limit");
if(process.argv[2]) {
  const file=fs.readFileSync(process.argv[2]);
  const current=decode(file.buffer.slice(file.byteOffset,file.byteOffset+file.byteLength));
  assert.deepEqual(Buffer.from(encode(fromJSON(JSON.parse(JSON.stringify(toJSON(current)))))),file);
  console.log(`Firmware layout JSON round trip passed: ${current.name}`);
}
fs.writeFileSync("/tmp/ergodox-json-test.bin",blob);
fs.writeFileSync("/tmp/ergodox-json-test.json",JSON.stringify(value,null,2));
// Locale selection stays in the browser and never creates a saveable preview.
const controls = new Map();
const storage = new Map();
context.localStorage = {setItem:(key,value)=>storage.set(key,value)};
context.document = {getElementById(id) {
  if (!controls.has(id)) controls.set(id,{value:"",classList:{toggle(){} }});
  return controls.get(id);
}};
context.fixtureLayout = layout;
vm.runInContext(`
  loadDisplayLabels = () => {};
  render = () => {};
  displayLayout = current => { layout = current; selectedDisplayLayout = current.displayLayout; updateTransferControls(); };
  installDisplayCatalog({paths:["base/qwerty.json","keyboards/fr_BE.json"],names:{}});
  currentLayout = fixtureLayout;
  layout = currentLayout;
`,context);
controls.set("display-layout",{value:"fr_be"});
vm.runInContext("changeDisplayLayout()",context);
assert.equal(vm.runInContext("pendingLayout.displayLayout",context),"fr_be");
assert.equal(vm.runInContext("currentLayout.displayLayout",context),"qwerty");
assert.equal(decode(encode(vm.runInContext("pendingLayout",context)).buffer).displayLayout,"fr_be");
assert.equal(controls.get("save-layout").disabled,false);
assert.deepEqual(Array.from(encode(layout).slice(48,60)),Array(12).fill(0));
controls.get("display-layout").value="../bad";
vm.runInContext("changeDisplayLayout()",context);
assert.equal(controls.get("display-layout").value,"fr_be");
console.log("JSON round trips, action validation, and NVM locale round-trip and staging tests passed");
// Every editor action must survive staging and wire encoding, without mutating
// the running configuration or another key/layer. No typed input is required.
context.document.querySelectorAll = selector => {
  const field = selector.includes('modifier') ? 'modifier' : 'buttons';
  return [1,64].filter(bit => (field === 'modifier' ? 65 : 1) & bit).map(value => ({value:String(value)}));
};
controls.set('key-editor',{close(){this.closed=true;}});
for (const [type, expected] of [
  [0,{type:'transparent'}], [1,{type:'none'}],
  [2,{type:'key',key:'A',modifiers:['LEFT_CTRL','RIGHT_ALT']}],
  [3,{type:'consumer',usage:777}], [4,{type:'system',usage:'WAKE'}],
  [5,{type:'mouse_button',buttons:['LEFT']}], [6,{type:'mouse_move',x:-127,y:127}],
  [7,{type:'mouse_wheel',delta:127}], [8,{type:'lighting',command:'BRIGHTNESS_DOWN'}],
  [9,{type:'layer_momentary',layer:6}], [10,{type:'layer_toggle',layer:6}], [11,{type:'layer_default',layer:6}]
]) {
  for (const [id,value] of Object.entries({'action-type':type,'edit-keycode':4,'edit-usage':type===3?777:type===4?131:4,'edit-x':-127,'edit-y':127,'edit-layer':6})) controls.set(id,{value:String(value)});
  vm.runInContext('editingKey = {hand:1,layer:7,index:37}; pendingSaved = true; applyKeyEdit({preventDefault(){}})',context);
  const pending = JSON.parse(vm.runInContext('JSON.stringify(layoutToJSON(pendingLayout))',context));
  assert.deepEqual(pending.layers[7].right[37],{position:38,...expected});
  assert.deepEqual(pending.layers[0].left,value.layers[0].left);
  assert.equal(vm.runInContext('currentLayout.actions[1][7][37].type',context),2);
  assert.equal(vm.runInContext('currentLayout.actions[1][7][37].modifier',context),64);
  assert.equal(vm.runInContext('pendingSaved',context),false);
  assert.equal(controls.get('save-layout').disabled,false);
  assert.equal(controls.get('key-editor').closed,true);
}
vm.runInContext('discardImport()',context);
assert.equal(vm.runInContext('layout === currentLayout && pendingLayout === null',context),true);
console.log('All 12 mouse editor actions, staging isolation, save re-enabling, and discard passed');
