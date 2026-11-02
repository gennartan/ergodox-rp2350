"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");
const fixtures = path.join(__dirname,"fixtures/hid-io");
const source = fs.readFileSync(path.join(__dirname,"../../src/web/app.js"),"utf8");
const revision = "9fa4d76ecfd9bb23a10e6b32d79fc3ccb13ab69f";
function environment(storage = new Map()) {
  const calls = [];
  const context = vm.createContext({TextDecoder,TextEncoder,AbortSignal,
    localStorage:{getItem:key=>storage.get(key) ?? null,setItem:(key,value)=>storage.set(key,value)},
    fetch:async (url,options)=>{
      const prefix = `https://raw.githubusercontent.com/hid-io/layouts/${revision}/`;
      assert.ok(url.startsWith(prefix));
      assert.equal(options.credentials,"omit");
      assert.equal(options.referrerPolicy,"no-referrer");
      const file = url.slice(prefix.length);
      calls.push(file);
      return {ok:true,text:async()=>fs.readFileSync(path.join(fixtures,file),"utf8")};
    }
  });
  vm.runInContext(source,context);
  return {context,calls,storage,run:code=>vm.runInContext(code,context)};
}
async function main() {
  const env = environment();
  const expected = {
    qwerty:{4:"A",20:"Q",30:"1",31:"2",45:"-"},
    azerty:{4:"Q",20:"A",30:"&",31:"é",52:"ù",54:";"},
    qwertz:{28:"Z",29:"Y",45:"ß",47:"Ü",51:"Ö"},
    colemak:{7:"S",8:"F",19:";",57:"Backspace"},
    en_us:{4:"A",30:"1"}, fr_fr:{4:"Q",31:"é"}, de_de:{28:"Z",47:"Ü"}
  };
  for (const [id, keys] of Object.entries(expected)) {
    const {labels} = await env.run(`getHidIoMapping("${id}")`);
    for (const [code,label] of Object.entries(keys)) assert.equal(labels[code],label,`${id} HID ${code}`);
    env.context.testLabels = labels;
    assert.equal(env.run("(displayLabels = testLabels, describe({type:2,keycode:4,modifier:0}).lines[0])"),labels[4]);
  }
  const count = env.calls.length;
  await env.run('getHidIoMapping("fr_fr")');
  assert.equal(env.calls.length,count,"same-page reuse should not redownload");
  const cached = environment(env.storage);
  assert.equal((await cached.run('getHidIoMapping("fr_fr")')).labels[31],"é");
  assert.equal(cached.calls.length,0,"new page should use persistent browser cache");
  cached.run('displayLabels = null');
  assert.equal(cached.run('describe({type:2,keycode:4,modifier:2}).lines.join(" ")'),"Shift A");
  await assert.rejects(env.run('getHidIoMapping("be-azerty")'),/Unknown/);
  await assert.rejects(env.run('getHidIoMapping("constructor")'),/Unknown/);
  assert.throws(()=>env.run('validateLabelMap({4:"bad\\u0000label"})'),/Invalid/);
  assert.throws(()=>env.run('validateLabelMap({102:"invalid usage"})'),/Invalid/);
  assert.throws(()=>env.run('mergeHidIoLabels({}, {name:["bad"],parent:"https://example.com/evil.json"})'),/Invalid/);
  // A broken cache is ignored. Failed downloads can be retried.
  const broken = environment(new Map([[`ergodox-labels-v2:${revision}:azerty`,"invalid json"]]));
  const realFetch = broken.context.fetch;
  broken.context.fetch = async()=>{throw new Error("offline")};
  await assert.rejects(broken.run('getHidIoMapping("azerty")'),/offline/);
  broken.context.fetch = realFetch;
  assert.equal((await broken.run('getHidIoMapping("azerty")')).labels[31],"é");
  broken.context.localStorage = {getItem(){throw new Error("blocked")},setItem(){throw new Error("blocked")}};
  assert.equal((await broken.run('getHidIoMapping("qwertz")')).labels[28],"Z");
  const cycle = environment();
  cycle.context.fetch = async()=>({ok:true,text:async()=>JSON.stringify({name:["cycle"],parent:"base/qwerty.json"})});
  await assert.rejects(cycle.run('getHidIoMapping("qwerty")'),/parent chain/);
  const malformed = environment();
  malformed.context.fetch = async()=>({ok:true,text:async()=>'{"name":["bad"],"parent":null,"to_hid_keyboard":{"oops":"A"}}'});
  await assert.rejects(malformed.run('getHidIoMapping("qwerty")'),/Invalid HID/);
  // A slow result for a previous selection must not replace the new labels.
  const racing = environment();
  const elements = new Map();
  racing.context.document = {getElementById(id){
    if (!elements.has(id)) elements.set(id,{});
    return elements.get(id);
  }};
  racing.run('render = () => {}; layout = {displayLayout:"azerty"};');
  let releaseOld;
  racing.context.oldLabels = new Promise(resolve=>{releaseOld=resolve});
  racing.run('getHidIoMapping = id => id === "azerty" ? oldLabels : Promise.resolve({labels:{4:"A"},legends:{}});');
  const oldRequest = racing.run('loadDisplayLabels()');
  racing.run('layout = {displayLayout:"qwerty"}');
  await racing.run('loadDisplayLabels()');
  releaseOld({labels:{4:"Q"},legends:{}});
  await oldRequest;
  assert.equal(racing.run('displayKeyName(4)'),"A");
  assert.equal(elements.get("display-layout-status").textContent,"QWERTY labels");
  racing.run('getHidIoMapping = async () => { throw new Error("offline"); }; layout = {displayLayout:"azerty"}');
  await racing.run('loadDisplayLabels()');
  assert.equal(racing.run('displayKeyName(4)'),"A");
  assert.match(elements.get("display-layout-status").textContent,/Using QWERTY labels.*offline/);
  assert.equal(elements.get("retry-labels").hidden,false);
  console.log("hid-io inheritance, all layouts, display labels, caching, failures, retries, and stale-download tests passed");
}
main().catch(error=>{console.error(error);process.exitCode=1});
