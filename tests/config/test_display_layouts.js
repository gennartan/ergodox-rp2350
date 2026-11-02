"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");
const fixtures = path.join(__dirname,"fixtures/hid-io");
const source = fs.readFileSync(path.join(__dirname,"../../src/web/app.js"),"utf8");
const revision = "8a391fd41a2b2b77cfab16d7f2e53da09bfe747a";
function environment(storage = new Map()) {
  const calls = [];
  const context = vm.createContext({TextDecoder,TextEncoder,AbortSignal,
    localStorage:{getItem:key=>storage.get(key) ?? null,setItem:(key,value)=>storage.set(key,value)},
    fetch:async (url,options)=>{
      const prefix = `https://raw.githubusercontent.com/gennartan/layouts/${revision}/`;
      const catalogURL = `https://api.github.com/repos/gennartan/layouts/git/trees/${revision}?recursive=1`;
      assert.ok(url.startsWith(prefix) || url === catalogURL);
      assert.equal(options.credentials,"omit");
      assert.equal(options.referrerPolicy,"no-referrer");
      const file = url === catalogURL ? "repository-tree.json" : url.slice(prefix.length);
      calls.push(file);
      return {ok:true,text:async()=>fs.readFileSync(path.join(fixtures,file),"utf8")};
    }
  });
  vm.runInContext(source,context);
  return {context,calls,storage,run:code=>vm.runInContext(code,context)};
}
async function main() {
  const preference = environment(new Map([["ergodox-display-layout", "fr_be"]]));
  assert.equal(preference.run("selectedDisplayLayout"), "qwerty", "keyboard preference must override browser storage");
  const env = environment();
  await env.run("ensureDisplayCatalog()");
  assert.equal(env.run("Object.keys(displayLayouts).length"),710);
  assert.equal(env.run('displayLayouts.xkb_be__oss.path'),"keyboards/xkb_be__oss.json");
  assert.match(env.run('displayLayouts.xkb_be__oss.name'),/Belgian/);
  assert.equal(env.run('hidIoPaths.has("base/azerty-keys.json")'),true);
  assert.equal(env.run('Object.hasOwn(displayLayouts,"azerty-keys")'),false);
  assert.equal((await env.run('getHidIoMapping("xkb_be__oss")')).legends[30].base,"&");
  const expected = {
    qwerty:{4:"A",20:"Q",30:"1",31:"2",45:"-"},
    azerty:{4:"Q",20:"A",30:"&",31:"é",52:"ù",54:";"},
    qwertz:{28:"Z",29:"Y",45:"ß",47:"Ü",51:"Ö"},
    colemak:{7:"S",8:"F",19:";",57:"Backspace"},
    fr_be:{4:"Q",35:"§",37:"!",56:"="}, en_us:{4:"A",30:"1"}, fr_fr:{4:"Q",31:"é"}, de_de:{28:"Z",47:"Ü"}
  };
  for (const [id, keys] of Object.entries(expected)) {
    const {labels} = await env.run(`getHidIoMapping("${id}")`);
    for (const [code,label] of Object.entries(keys)) assert.equal(labels[code],label,`${id} HID ${code}`);
    env.context.testLabels = labels;
    assert.equal(env.run("(displayLabels = testLabels, describe({type:2,keycode:4,modifier:0}).lines[0])"),labels[4]);
  }
  env.run('selectedDisplayLayout = "fr_be"');
  env.context.belgianLabels = (await env.run('getHidIoMapping("fr_be")')).labels;
  assert.equal(env.run('(displayLabels = belgianLabels, keyIdentity(4))'), "0x04 HID_KEY_A fr_BE Q");
  env.run('selectedDisplayLayout = "azerty"');
  assert.match(env.run('keyIdentity(4)'), /HID_KEY_A azerty Q$/);
  assert.ok(env.calls.includes("keyboards/fr_BE.json"), "Belgian data must be downloaded");
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
  const broken = environment(new Map([[`ergodox-labels-v3:gennartan/layouts:${revision}:azerty`,"invalid json"]]));
  const realFetch = broken.context.fetch;
  broken.context.fetch = async()=>{throw new Error("offline")};
  await assert.rejects(broken.run('getHidIoMapping("azerty")'),/offline/);
  broken.context.fetch = realFetch;
  assert.equal((await broken.run('getHidIoMapping("azerty")')).labels[31],"é");
  broken.context.localStorage = {getItem(){throw new Error("blocked")},setItem(){throw new Error("blocked")}};
  assert.equal((await broken.run('getHidIoMapping("qwertz")')).labels[28],"Z");
  const cycle = environment();
  await cycle.run("ensureDisplayCatalog()");
  cycle.context.fetch = async()=>({ok:true,text:async()=>JSON.stringify({name:["cycle"],parent:"base/qwerty.json"})});
  await assert.rejects(cycle.run('getHidIoMapping("qwerty")'),/parent chain/);
  const malformed = environment();
  await malformed.run("ensureDisplayCatalog()");
  malformed.context.fetch = async()=>({ok:true,text:async()=>'{"name":["bad"],"parent":null,"to_hid_keyboard":{"oops":"A"}}'});
  await assert.rejects(malformed.run('getHidIoMapping("qwerty")'),/Invalid HID/);
  // A slow result for a previous selection must not replace the new labels.
  const racing = environment();
  await racing.run("ensureDisplayCatalog()");
  racing.context.Option = function(text,value){this.text=text;this.value=value};
  const elements = new Map();
  racing.context.document = {getElementById(id){
    if (!elements.has(id)) elements.set(id,{replaceChildren(){},append(){}});
    return elements.get(id);
  }};
  racing.run('render = () => {}; selectedDisplayLayout = "azerty";');
  let releaseOld;
  racing.context.oldLabels = new Promise(resolve=>{releaseOld=resolve});
  racing.run('getHidIoMapping = id => id === "azerty" ? oldLabels : Promise.resolve({labels:{4:"A"},legends:{}});');
  const oldRequest = racing.run('loadDisplayLabels()');
  racing.run('selectedDisplayLayout = "qwerty"');
  await racing.run('loadDisplayLabels()');
  releaseOld({labels:{4:"Q"},legends:{}});
  await oldRequest;
  assert.equal(racing.run('displayKeyName(4)'),"A");
  assert.equal(elements.get("display-layout-status").textContent,"qwerty labels");
  racing.run('getHidIoMapping = async () => { throw new Error("offline"); }; selectedDisplayLayout = "azerty"');
  await racing.run('loadDisplayLabels()');
  assert.equal(racing.run('displayKeyName(4)'),"A");
  assert.match(elements.get("display-layout-status").textContent,/Using en_US labels.*offline/);
  assert.equal(elements.get("retry-labels").hidden,false);
  // Catalog failures are retryable, and incomplete GitHub responses are never
  // accepted as the set of available locales.
  const unavailable = environment();
  const repositoryFetch = unavailable.context.fetch;
  unavailable.context.fetch = async()=>({ok:false,status:403});
  await assert.rejects(unavailable.run("ensureDisplayCatalog()"),/403/);
  unavailable.context.fetch = repositoryFetch;
  await unavailable.run("ensureDisplayCatalog()");
  assert.equal(unavailable.run("Object.keys(displayLayouts).length"),710);
  const truncated = environment();
  truncated.context.fetch = async()=>({ok:true,text:async()=>JSON.stringify({sha:revision,truncated:true,tree:[]})});
  await assert.rejects(truncated.run("ensureDisplayCatalog()"),/Incomplete/);
  assert.throws(()=>env.run('installDisplayCatalog({paths:["../evil.json"],names:{}})'),/Invalid/);
  // Persist a catalog-discovered variant through the real config codecs, then
  // open it in a fresh browser with no stored locale preference or catalog.
  function ui(instance) {
    const controls = new Map();
    instance.context.Option = function(text,value){this.text=text;this.value=value};
    instance.context.document = {getElementById(id){
      if (!controls.has(id)) controls.set(id,{replaceChildren(...items){this.options=items},append(item){this.options.push(item)}});
      return controls.get(id);
    }};
    instance.run("render = () => {}");
    return controls;
  }
  const choosing = environment();
  await choosing.run("ensureDisplayCatalog()");
  const controls = ui(choosing);
  choosing.run('currentLayout = {name:"Saved locale",displayLayout:"qwerty",layerCount:1,defaultLayer:0,actions:Array.from({length:2},()=>[Array.from({length:38},()=>({type:1}))])}; displayLayout(currentLayout)');
  controls.get("display-layout").value="xkb_be__oss";
  choosing.run("changeDisplayLayout()");
  await choosing.run("loadDisplayLabels()");
  assert.equal(choosing.run("currentLayout.displayLayout"),"qwerty");
  assert.equal(choosing.run("pendingLayout.displayLayout"),"xkb_be__oss");
  assert.equal(controls.get("save-layout").disabled,false);
  const saved = choosing.run("encodeLayout(pendingLayout)");
  const reopening = environment();
  reopening.context.savedConfig = saved;
  const reopenedControls = ui(reopening);
  reopening.run("displayLayout(decodeLayout(savedConfig.buffer))");
  await reopening.run("loadDisplayLabels()");
  assert.equal(reopening.run("selectedDisplayLayout"),"xkb_be__oss");
  assert.equal(reopenedControls.get("display-layout").value,"xkb_be__oss");
  assert.equal(reopenedControls.get("display-layout").options.length,710);
  assert.match(reopenedControls.get("display-layout-status").textContent,/Belgian/);
  assert.equal(reopening.run("displayLegends[30].base"),"&");
  console.log("hid-io inheritance, all layouts, display labels, caching, failures, retries, and stale-download tests passed");
}
main().catch(error=>{console.error(error);process.exitCode=1});
