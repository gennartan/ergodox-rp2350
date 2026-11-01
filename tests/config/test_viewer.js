"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const vm = require("node:vm");
const path = require("node:path");
const context = vm.createContext({TextDecoder});
vm.runInContext(fs.readFileSync(path.join(__dirname,"../../src/web/app.js"),"utf8"),context);
const decode = vm.runInContext("decodeLayout",context);
const describe = vm.runInContext("describe",context);
const placements = vm.runInContext("placements",context);

// Independent fixture builder: explicit bytes from the documented wire format.
const bytes = new Uint8Array(2496), view = new DataView(bytes.buffer);
bytes.set(Buffer.from("EKCF"));
view.setUint16(4,1,true); view.setUint16(6,64,true); view.setUint32(8,bytes.length,true);
bytes.set([8,7,2,38],12); bytes.set(Buffer.from("Viewer test"),16);
bytes.set([2,2,4,0],64); // left, layer 0, key 0: Shift+A
bytes.set([6,0,240,16],68); // signed movement
bytes.set([9,0,0,7],72); // layer 7
bytes.set([3,0,233,0],64+(8+7)*38*4); // right, layer 7, key 0: Volume+
let crc = 0xffffffff;
for (const byte of bytes) {
  crc ^= byte;
  for (let bit=0; bit<8; bit++) crc = crc & 1 ? (crc >>> 1) ^ 0xedb88320 : crc >>> 1;
}
view.setUint32(60,(~crc)>>>0,true);
const layout = decode(bytes.buffer);
assert.equal(layout.name,"Viewer test");
assert.equal(layout.defaultLayer,7);
assert.equal(layout.actions[0][0][0].keycode,4);
assert.match(describe(layout.actions[0][0][0]).detail,/Shift \+ A/);
assert.equal(layout.actions[0][0][1].x,-16);
assert.equal(layout.actions[0][0][2].layer,7);
assert.match(describe(layout.actions[1][7][0]).detail,/Volume/);
for (const hand of ["left","right"]) {
  const keys = Array.from(placements(hand));
  assert.equal(keys.length,38);
  assert.deepEqual(keys.map(key=>key.index).sort((a,b)=>a-b),Array.from({length:38},(_,i)=>i));
  for (const key of keys) assert.ok(key.x>=0 && key.y>=0 && key.x+key.w<=702 && key.y+key.h<=510);
}
for (let type=0; type<=11; type++) {
  const description = describe({type,modifier:0,keycode:4,layer:7,usage:0,x:-16,y:16});
  assert.equal(typeof description.detail,"string");
  assert.ok(description.lines.every(line=>typeof line==="string"));
}
const corrupt=bytes.slice(); corrupt[70]^=1;
assert.throws(()=>decode(corrupt.buffer),/corrupt/);
assert.throws(()=>decode(bytes.slice(0,63).buffer),/Invalid/);
assert.throws(()=>decode(bytes.slice(0,-1).buffer),/corrupt/);
// Verify interoperability with an optional record encoded by the firmware C codec.
if (process.argv[2]) {
  const input=fs.readFileSync(process.argv[2]);
  const parsed=decode(input.buffer.slice(input.byteOffset,input.byteOffset+input.byteLength));
  assert.ok(parsed.layerCount>=1 && parsed.layerCount<=8);
  console.log(`Firmware record decoded: ${parsed.name}, ${parsed.layerCount} layers`);
}
console.log("Viewer decoder, all action labels, both physical geometries, and corrupt-input tests passed");
