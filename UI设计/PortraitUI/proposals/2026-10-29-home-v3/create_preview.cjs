/* Design preview only. Does not export firmware assets or change the live UI. */
const fs = require('fs');
const path = require('path');
const { chromium } = require(path.join(process.argv[2], 'playwright'));
const root = path.resolve(__dirname, '../../../..');
const fontRoot = path.join(root, 'code/PM_Controller/main/app/ui/fonts');
const font = (family, file, weight) => `@font-face{font-family:'${family}';src:url(data:font/ttf;base64,${fs.readFileSync(path.join(fontRoot, file)).toString('base64')}) format('truetype');font-weight:${weight};}`;
const fonts = font('Roboto Mono', 'RobotoMono/RobotoMono[wght].ttf', '100 900') + font('Space Mono', 'SpaceMono/SpaceMono-Bold.ttf', '700');
const T = (x,y,s,text,color='#e9f0ec',anchor='start',weight=500) => `<text x="${x}" y="${y}" font-size="${s}" fill="${color}" text-anchor="${anchor}" font-weight="${weight}">${text}</text>`;
const P = (d,color,width=1.5) => `<path d="${d}" fill="none" stroke="${color}" stroke-width="${width}" stroke-linecap="round" stroke-linejoin="round"/>`;
function number(value, y, size, color) {
    const step = size*.60, dot = size*.25;
    const total = [...value].reduce((sum,c)=>sum+(c==='.'?dot:step),0) + 17;
    let x = 68 - total/2;
    let out='';
    for(const c of value){const w=c==='.'?dot:step;out+=`<text x="${x+w/2}" y="${y}" text-anchor="middle" fill="${color}" font-family="Space Mono" font-weight="700" font-size="${size}">${c}</text>`;x+=w;}
    return out + T(x+5,y,15,'m',color,'start',700);
}
function focusIcon(x,y,color,size=18){return `<g transform="translate(${x} ${y}) scale(${size/18})">${P('M0 5V0H5M13 0H18V5M18 13V18H13M5 18H0V13',color,1.8)}<circle cx="9" cy="9" r="3" fill="none" stroke="${color}" stroke-width="1.8"/></g>`;}
function gear(x,y,color){return `<g transform="translate(${x} ${y})"><circle cx="7" cy="7" r="4" fill="none" stroke="${color}" stroke-width="1.5"/>${P('M7 0V3M7 11V14M0 7H3M11 7H14M2 2L4 4M10 10L12 12M2 12L4 10M10 4L12 2',color,1.5)}</g>`;}
function eject(x,y,color){return `<g transform="translate(${x} ${y})">${P('M1 8L7 1L13 8ZM1 12H13',color,1.5)}</g>`;}
function screen(edit=false){
    const ink='#09291e', pale='#d4ffe7', green='#79f5a7', blue='#69d6ff', amber='#ffbe58';
    let s=`<svg xmlns="http://www.w3.org/2000/svg" width="172" height="320" viewBox="0 0 172 320"><title>HOME concept — ${edit?'focus edit':'browse'}</title><defs><style>${fonts}text{font-family:'Roboto Mono';}</style><clipPath id="screen"><rect width="172" height="320" rx="17"/></clipPath></defs><g clip-path="url(#screen)"><rect width="172" height="320" fill="#080f10"/>`;
    ['#f07d72','#efbd65','#d6e8c4','#81c7bb','#86b9dc'].forEach((c,i)=>{s+=`<rect x="${12+i*7}" y="13" width="6" height="5" fill="${c}"/>`;});
    s+=`<rect x="107" y="11" width="18" height="9" rx="1" fill="none" stroke="#cfdfd6"/><rect x="109" y="13" width="11" height="5" fill="#cfdfd6"/>${P('M127 14V17','#cfdfd6',2)}`;
    const accent=edit?amber:green;
    s+=`<rect x="8" y="32" width="120" height="90" rx="4" fill="${accent}"/>`;
    s+=T(16,53,14,'FOCUS',ink,'start',700);
    if(edit) s+=P('M109 44L114 39L119 44M109 49L114 54L119 49',ink,2);
    s+=number('2.5',108,50,ink);
    s+=`<rect x="8" y="130" width="120" height="85" rx="4" fill="#072b42"/>`;
    s+=T(16,151,14,'ACTUAL',blue,'start',700)+number('2.4',203,50,blue);
    const sx = v=>14+89*Math.log(1+v)/Math.log(21);
    s+=P('M14 233H103M110 233H113','#b9d0c4',1.5)+P('M121 229V237','#b9d0c4',1.5);
    [0,1,5,20].forEach(v=>{const x=sx(v);s+=P(`M${x} 230V237`,'#b9d0c4',1.5);});
    // Sparse labels keep the scale readable; the log mapping and unlabeled ticks remain.
    s+=T(14,255,11,'0','#e0eee7','middle',600)+T(sx(5),255,11,'5','#e0eee7','middle',600)+T(98,255,11,'20','#e0eee7','middle',600);
    s+=`<path d="M121 252C126 246 129 247 129 251C129 257 125 256 121 252C117 248 114 247 114 251C114 257 117 257 121 252Z" fill="none" stroke="#e0eee7" stroke-width="1.6"/>`;
    s+=`<path d="M${sx(2.5)-4} 220h8l-4 7Z" fill="${green}"/><path d="M${sx(2.4)-4} 244h8l-4-7Z" fill="${blue}"/>`;
    s+=T(12,281,13,'FILM','#e0eee7', 'start',700)+T(124,284,24,'6',green,'end',700);
    for(let i=0;i<8;i++) s+=`<rect x="${12+i*14.2}" y="291" width="11" height="15" rx="1" fill="${i<6?'#d8ffe7':'#355044'}"/><rect x="${14+i*14.2}" y="293" width="7" height="8" fill="${i<6?'#278752':'#15251f'}"/>`;
    // Uptime moves to INFO; the key centers remain 54/160/266.
    s+=`<rect x="136" y="32" width="36" height="274" rx="4" fill="${pale}"/>`;
    s+=`<rect x="140" y="38" width="28" height="34" rx="3" fill="#9ce8ba"/>`;
    s+= edit ? P('M147 54H161M154 47V61',ink,3) : P('M147 58L154 51L161 58',ink,3);
    s+=P('M142 84H166','#78ad8a',1.5);
    if(!edit)s+=`<g transform="translate(144 105) scale(1.43)">${gear(0,0,'#386b4b')}</g>`;
    s+=`<rect x="139" y="138" width="30" height="62" rx="3" fill="${edit?amber:ink}"/>`;
    s+=`<path d="M136 155L141 160L136 165Z" fill="${edit?amber:ink}"/>`;
    s+=edit ? P('M145 159L152 166L163 153',ink,3) : focusIcon(143,149,green,22);
    s+=T(154,190,9.5,edit?'DONE':'FOCUS',edit?ink:pale,'middle',700);
    if(!edit)s+=`<g transform="translate(144 217) scale(1.43)">${eject(0,0,'#386b4b')}</g>`;
    if(edit)s+=T(154,227,10,'50mm',ink,'middle',700);
    s+=P('M142 244H166','#78ad8a',1.5);
    s+=`<rect x="140" y="250" width="28" height="34" rx="3" fill="#9ce8ba"/>`;
    s+=edit ? P('M147 266H161',ink,3) : P('M147 262L154 269L161 262',ink,3);
    return s+'</g></svg>';
}
function infoScreen(){
    let s=`<svg xmlns="http://www.w3.org/2000/svg" width="172" height="320" viewBox="0 0 172 320"><title>INFO — uptime moved from HOME</title><defs><style>${fonts}text{font-family:'Roboto Mono';}</style><clipPath id="info-screen"><rect width="172" height="320" rx="17"/></clipPath></defs><g clip-path="url(#info-screen)"><rect width="172" height="320" fill="#080f10"/>`;
    s+=T(12,22,14,'DEVICE INFO','#e6fff0','start',700);
    s+=`<rect x="8" y="32" width="120" height="55" rx="4" fill="#79f5a7"/>`;
    s+=T(16,51,13,'UPTIME','#09291e','start',700)+T(16,77,20,'00:12:34','#09291e','start',700);
    const rows=[['BATTERY','3.92V / ~72%'],['MOTOR / IPROPI','0mA / 0mV'],['REBOOTS / SHOTS','R 12 / S 36'],['MOTOR TOTAL','00:04:28']];
    rows.forEach(([label,value],i)=>{const y=108+i*45;s+=T(12,y,12,label,'#bedac9','start',600)+T(12,y+21,14,value,'#f0fff6','start',600)+P(`M12 ${y+30}H124`,'#456452',1);});
    s+=T(12,304,12,'EEPROM','#bedac9','start',600)+T(124,304,14,'OK','#79f5a7','end',700);
    s+=`<rect x="136" y="32" width="36" height="274" rx="4" fill="#d4ffe7"/>`;
    s+=`<rect x="140" y="38" width="28" height="34" rx="3" fill="#9ce8ba"/>${P('M147 58L154 51L161 58','#09291e',3)}`;
    s+=P('M142 84H166','#78ad8a',1.5);
    s+=`<rect x="139" y="138" width="30" height="62" rx="3" fill="#09291e"/><g transform="translate(143 149) scale(1.57)">${gear(0,0,'#79f5a7')}</g>`+T(154,190,9.5,'SETUP','#d4ffe7','middle',700);
    s+=P('M142 244H166','#78ad8a',1.5)+`<rect x="140" y="250" width="28" height="34" rx="3" fill="#9ce8ba"/>${P('M147 262L154 269L161 262','#09291e',3)}`;
    return s+'</g></svg>';
}
async function main(){
    const browser=await chromium.launch({executablePath:process.argv[3],headless:true});
    try{
        const page=await browser.newPage({viewport:{width:172,height:320},deviceScaleFactor:4});
        const native=await browser.newPage({viewport:{width:172,height:320},deviceScaleFactor:1});
        for(const [name,svg] of [['home-concept',screen(false)],['home-focus-edit',screen(true)],['info-concept',infoScreen()]]){
            fs.writeFileSync(path.join(__dirname,name+'.svg'),svg);
            await page.setContent(`<style>html,body{margin:0}svg{display:block}</style>${svg}`);
            await page.evaluate(async()=>{for(const f of ['500 12px "Roboto Mono"','700 50px "Space Mono"']){const faces=await document.fonts.load(f);if(!faces.length)throw Error('Font missing '+f);}await document.fonts.ready;});
            await page.screenshot({path:path.join(__dirname,name+'-4x.png')});
            await native.setContent(`<style>html,body{margin:0}svg{display:block}</style>${svg}`);
            await native.evaluate(async()=>{await document.fonts.ready;});
            await native.screenshot({path:path.join(__dirname,name+'-1x.png')});
        }
        const board=`<!doctype html><meta charset="utf-8"><style>${fonts}*{box-sizing:border-box}body{margin:0;background:#e8ebe7;color:#21352b;font-family:'Roboto Mono';width:1320px;height:1040px;padding:42px 64px}.top{font-size:12px;letter-spacing:2px;color:#617366}h1{font-size:28px;font-weight:600;margin:14px 0 8px}.sub{font-size:12px;color:#617366}.screens{display:flex;gap:56px;margin-top:34px}.col{width:360px;flex-shrink:0}.tag{font-size:11px;letter-spacing:1px;margin-bottom:16px}.frame{width:360px;padding:8px;background:#1d2a23;border-radius:42px;box-shadow:0 12px 25px #172e2118}.frame svg{display:block;width:344px;height:640px;border-radius:34px}.note{margin-top:20px;font-size:11px;line-height:1.8;color:#526759}.foot{margin-top:26px;padding-top:16px;border-top:1px solid #c6cec6;font-size:11px;color:#617366;display:flex;justify-content:space-between}</style><div class="top">POLAMIYA / HOME CONCEPT 03</div><h1>Bright color. Bigger readouts.</h1><div class="sub">1.47 in · 172 × 320 px · 50 px readouts · 22 px main icons · Original fonts</div><div class="screens"><div class="col"><div class="tag">01 / HOME</div><div class="frame">${screen(false)}</div><div class="note">Bright green target / vivid blue reading<br>50 px numbers + 14 px labels</div></div><div class="col"><div class="tag">02 / FOCUS EDIT</div><div class="frame">${screen(true)}</div><div class="note">Amber target + confirmation key<br>22 px icons + stronger key strokes</div></div><div class="col"><div class="tag">03 / INFO</div><div class="frame">${infoScreen()}</div><div class="note">Uptime stays in device information<br>12–14 px detail text</div></div></div><div class="foot"><span>ENLARGED PREVIEW · SAMPLE VALUES · NO FIRMWARE CHANGES</span><span>2026.10.29</span></div>`;
        fs.writeFileSync(path.join(__dirname,'preview.html'),board);
        const bp=await browser.newPage({viewport:{width:1320,height:1040},deviceScaleFactor:1});
        await bp.setContent(board);await bp.evaluate(async()=>{await document.fonts.ready;});
        await bp.screenshot({path:path.join(__dirname,'preview.png')});
        console.log('Saved SVGs, 4× PNGs, preview.html and preview.png in '+__dirname);
    }finally{await browser.close();}
}
main().catch(e=>{console.error(e);process.exitCode=1;});
