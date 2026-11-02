"use strict";
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");
const context = vm.createContext({TextEncoder,TextDecoder,AbortSignal,
  fetch:async url=>({ok:true,text:async()=>fs.readFileSync(path.join(__dirname,"fixtures/hid-io",...(url.includes("api.github.com") ? ["repository-tree.json"] : url.split("/").slice(-2))),"utf8")})
});
vm.runInContext(fs.readFileSync(path.join(__dirname,"../../src/web/app.js"),"utf8"),context);
const run = code=>vm.runInContext(code,context);
class SvgNode {
  constructor(tag) { this.tag=tag;this.attributes={};this.children=[];this.textContent=""; }
  setAttribute(name,value) {this.attributes[name]=value;}
  append(child) {this.children.push(child);}
  replaceChildren(...children) {this.children=children;}
  addEventListener() {}
}
const escape = text=>String(text).replaceAll("&","&amp;").replaceAll("<","&lt;").replaceAll('"',"&quot;");
function serialize(node) {
  return `<${node.tag} ${Object.entries(node.attributes).map(([k,v])=>`${k}="${escape(v)}"`).join(" ")}>${escape(node.textContent)}${node.children.map(serialize).join("")}</${node.tag}>`;
}
async function main() {
  const mappings = {};
  for (const id of ["qwerty","azerty","qwertz","colemak","en_us","fr_fr","fr_be","de_de"]) mappings[id] = await run(`getHidIoMapping("${id}")`);
  const plain = value=>JSON.parse(JSON.stringify(value));
  assert.deepEqual(plain(mappings.qwerty.legends[30]),{base:"1",shift:"!",dead:[]});
  assert.deepEqual(plain(mappings.azerty.legends[30]),{base:"&",shift:"1",dead:[]});
  assert.deepEqual(plain(mappings.azerty.legends[32]),{base:'"',shift:"3",altgr:"#",dead:[]});
  assert.deepEqual(plain(mappings.azerty.legends[47]),{base:"ˆ",shift:"¨",dead:["base","shift"]});
  assert.deepEqual(plain(mappings.azerty.legends[53]),{base:"²",dead:[]},"no inherited US Shift+~ on superscript-2");
  assert.deepEqual(plain(mappings.qwertz.legends[31]),{base:"2",shift:'"',altgr:"²",dead:[]});
  assert.deepEqual(plain(mappings.colemak.legends[20]),{base:"q",shift:"Q",altgr:"ä",shiftAltgr:"Ä",dead:[]});
  assert.deepEqual(plain(mappings.colemak.legends[7]),{base:"s",shift:"S",dead:[]});
  assert.equal(mappings.colemak.legends[57],undefined,"CapsLock remapped to Backspace stays special");
  for (const [regional,base] of [["en_us","qwerty"],["fr_fr","azerty"],["de_de","qwertz"]]) assert.deepEqual(plain(mappings[regional].legends),plain(mappings[base].legends));
  const elements = {layer:{value:0},left:new SvgNode("svg"),right:new SvgNode("svg")};
  context.document = {createElementNS:(_,tag)=>new SvgNode(tag),getElementById:id=>elements[id],querySelectorAll:()=>[]};
  context.mapping = mappings.colemak;
  run('displayLabels = mapping.labels; displayLegends = mapping.legends');
  const legends = run("standardKeyLegends"), describe = run("describe"), append = run("appendKeyLabels");
  const action = {type:2,modifier:0,keycode:20};
  const key = {x:10,y:20,w:58,h:58};
  const group = new SvgNode("g");
  append(group,key,describe(action),legends(action));
  assert.deepEqual(group.children.map(n=>[n.textContent,n.attributes.x,n.attributes.y]),[
    ["q",20,68],["Q",20,43],["ä",58,68],["Ä",58,43]
  ]);
  assert.match(describe(action).detail,/Base: q.*Shift: Q.*AltGr: ä.*Shift\+AltGr: Ä/);
  for (const special of [
    {...action,modifier:2}, {...action,modifier:64}, {...action,modifier:3},
    {...action,keycode:0}, {...action,keycode:40}, {...action,keycode:44},
    {...action,keycode:57}, {...action,keycode:58}, {...action,keycode:79},
    {...action,keycode:89}, {...action,type:3,usage:233}, {...action,type:9,layer:1}
  ]) {
    assert.equal(legends(special),null);
    const centered = new SvgNode("g");
    append(centered,key,describe(special),legends(special));
    assert.ok(centered.children.every(n=>n.attributes.x===39 && !n.attributes.class),"combination/special labels stay centered");
  }
  context.mapping=mappings.azerty;
  run('displayLabels = mapping.labels; displayLegends = mapping.legends');
  const deadKey={type:2,modifier:0,keycode:47};
  const deadNode=new SvgNode("g");
  append(deadNode,key,describe(deadKey),legends(deadKey));
  assert.ok(deadNode.children.every(n=>n.attributes.class.includes("dead-key")));
  assert.match(describe(deadKey).detail,/dead key/);
  // Render both complete halves: ordinary keys use corner glyphs, combinations
  // retain action labels, and ARIA/title details include modifier characters.
  context.actions = Array.from({length:2},()=>[Array.from({length:38},(_,i)=>({type:2,modifier:i===1?2:0,keycode:i===0?32:4+i%26}))]);
  run('layout = {actions}; render()');
  assert.equal(elements.left.children.length,38);
  const normal = elements.left.children.find(n=>n.attributes["data-index"]===0);
  const shortcut = elements.left.children.find(n=>n.attributes["data-index"]===1);
  assert.match(normal.attributes["aria-label"],/AltGr: #/);
  assert.equal(normal.children.filter(n=>n.attributes.class?.includes("key-glyph")).length,3);
  assert.equal(shortcut.children.filter(n=>n.attributes.class?.includes("key-glyph")).length,0);
  run('displayLegends = null');
  assert.equal(legends(action),null,"unavailable locale keeps centered fallback");
  assert.throws(()=>run('validateLegends({4:{base:"ab",dead:[]}})'),/Invalid/);
  assert.throws(()=>run('validateLegends({4:{base:"a",dead:["altgr"]}})'),/Invalid/);
  assert.throws(()=>run('mergeHidIoLabels({}, {name:["bad"],parent:null,composition:{a:["A"]}})'),/Invalid character composition/);
  // Optional standalone visual preview uses the actual SVG label renderer.
  if (process.argv[2]) {
    let rows="";
    for (const [index,id] of ["qwerty","azerty","qwertz","colemak"].entries()) {
      context.mapping=mappings[id];run('displayLabels = mapping.labels; displayLegends = mapping.legends');
      const y=50+index*110;
      rows+=`<text x="20" y="${y+32}" font-size="15">${id.toUpperCase()}</text>`;
      for (const [i,code] of [4,20,30,31,32,47].entries()) {
        const a={type:2,modifier:0,keycode:code}, k={x:150+i*85,y,w:58,h:58}, g=new SvgNode("g");
        g.setAttribute("class","key");const rect=new SvgNode("rect");
        for (const [name,value] of Object.entries({...k,width:58,height:58,rx:7})) rect.setAttribute(name,value);
        g.append(rect);append(g,k,describe(a),legends(a));rows+=serialize(g);
      }
      const a={type:2,modifier:2,keycode:4}, k={x:660,y,w:100,h:58},g=new SvgNode("g");g.setAttribute("class","key");
      const rect=new SvgNode("rect");for(const [name,value] of Object.entries({x:k.x,y,width:k.w,height:k.h,rx:7}))rect.setAttribute(name,value);
      g.append(rect);append(g,k,describe(a),legends(a));rows+=serialize(g);
    }
    fs.writeFileSync(process.argv[2],`<svg xmlns="http://www.w3.org/2000/svg" width="800" height="510" viewBox="0 0 800 510"><style>${fs.readFileSync(path.join(__dirname,"../../src/web/app.css"),"utf8")}</style><rect width="800" height="510" fill="#f2f5f9"/><text x="20" y="24" font-size="16">Base ↙ · Shift ↖ · AltGr ↘ · Shift+AltGr ↗ · Assigned shortcut at right</text>${rows}</svg>`);
  }
  console.log("Locale keycap characters, inheritance, dead keys, SVG positions, accessibility, and special/combination exclusions passed");
}
main().catch(error=>{console.error(error);process.exitCode=1});
