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
    const total = [...value].reduce((sum,c)=>sum+(c==='.'?dot:step),0) + 15;
    let x = 68 - total/2;
    let out='';
    for(const c of value){const w=c==='.'?dot:step;out+=`<text x="${x+w/2}" y="${y}" text-anchor="middle" fill="${color}" font-family="Space Mono" font-weight="700" font-size="${size}">${c}</text>`;x+=w;}
    return out + T(x+5,y,13,'m',color);
}
function focusIcon(x,y,color,size=18){return `<g transform="translate(${x} ${y}) scale(${size/18})">${P('M0 5V0H5M13 0H18V5M18 13V18H13M5 18H0V13',color,1.8)}<circle cx="9" cy="9" r="3" fill="none" stroke="${color}" stroke-width="1.8"/></g>`;}
function gear(x,y,color){return `<g transform="translate(${x} ${y})"><circle cx="7" cy="7" r="4" fill="none" stroke="${color}" stroke-width="1.5"/>${P('M7 0V3M7 11V14M0 7H3M11 7H14M2 2L4 4M10 10L12 12M2 12L4 10M10 4L12 2',color,1.5)}</g>`;}
function eject(x,y,color){return `<g transform="translate(${x} ${y})">${P('M1 8L7 1L13 8ZM1 12H13',color,1.5)}</g>`;}
function screen(edit=false){
    const ink='#10251e', pale='#293b35', green='#a8ddbd', blue='#8dbcd3', amber='#e8b779';
    let s=`<svg xmlns="http://www.w3.org/2000/svg" width="172" height="320" viewBox="0 0 172 320"><title>HOME concept — ${edit?'focus edit':'browse'}</title><defs><style>${fonts}text{font-family:'Roboto Mono';}</style><clipPath id="screen"><rect width="172" height="320" rx="17"/></clipPath></defs><g clip-path="url(#screen)"><rect width="172" height="320" fill="#080f10"/>`;
    ['#f07d72','#efbd65','#d6e8c4','#81c7bb','#86b9dc'].forEach((c,i)=>{s+=`<rect x="${12+i*7}" y="13" width="6" height="5" fill="${c}"/>`;});
    s+=`<rect x="107" y="11" width="18" height="9" rx="1" fill="none" stroke="#cfdfd6"/><rect x="109" y="13" width="11" height="5" fill="#cfdfd6"/>${P('M127 14V17','#cfdfd6',2)}`;
    const accent=edit?amber:green;
    s+=`<rect x="8.5" y="32.5" width="119" height="84" rx="4" fill="${edit?'#2b251e':'#162820'}" stroke="${edit?'#796044':'#385646'}" stroke-width="1"/>`;
    s+=`<rect x="16" y="32" width="28" height="2" fill="${accent}"/>`;
    s+=T(16,51,12,'FOCUS',accent,'start',600);
    if(edit) s+=T(119,50,8,'EDIT',accent,'end',700);
    else s+=focusIcon(108,40,accent,10);
    s+=number('2.5',100,44,accent);
    s+=`<rect x="8.5" y="125.5" width="119" height="76" rx="4" fill="#14232b" stroke="#2d4450" stroke-width="1"/>`;
    s+=T(16,145,12,'ACTUAL',blue,'start',600)+number('2.4',187,40,blue);
    const sx = v=>14+89*Math.log(1+v)/Math.log(21);
    s+=P('M14 221H103M108 221H112','#799389',1)+P('M119 217V225','#799389',1);
    [0,1,5,20].forEach(v=>{const x=sx(v);s+=P(`M${x} 218V224`,'#799389',1)+T(x,239,8,String(v),'#b4c6bd','middle');});
    s+=T(119,239,7,'INF','#b4c6bd','middle');
    s+=`<path d="M${sx(2.5)-3} 210h6l-3 6Z" fill="${green}"/><path d="M${sx(2.4)-3} 231h6l-3-6Z" fill="${blue}"/>`;
    s+=P('M12 255H124','#34473e',.7)+T(12,275,10,'FILM','#b4c6bd', 'start',600)+T(124,276,17,'6',green,'end',700);
    for(let i=0;i<8;i++) s+=`<rect x="${12+i*14.2}" y="285" width="11" height="15" rx="1" fill="${i<6?'#b4c8bc':'#2b4036'}"/><rect x="${14+i*14.2}" y="287" width="7" height="8" fill="${i<6?'#3b5b4a':'#15251f'}"/>`;
    // Uptime moves to INFO; the key centers remain 54/160/266.
    s+=`<rect x="136" y="32" width="36" height="269" rx="4" fill="${pale}"/>${P('M136.5 36V297','#63816f',1)}`;
    s+=T(154,25,7,edit?'EDIT':'MENU','#a5b9ad','middle',700);
    s+=`<rect x="141" y="38" width="26" height="32" rx="3" fill="#3b5246"/>`;
    s+= edit ? P('M149 54H159M154 49V59','#d8e7dd',2) : P('M149 57L154 52L159 57','#d8e7dd',2);
    s+=P('M143 84H165','#5c7565',1);
    if(!edit)s+=gear(147,105,'#a5bfae')+T(154,130,7,'SETUP','#a5bfae','middle',600);
    s+=`<rect x="139" y="139" width="30" height="61" rx="3" fill="${accent}"/>`;
    s+=`<path d="M136 156L140 160L136 164Z" fill="${accent}"/>`;
    s+=edit ? P('M148 159L152 163L160 154',ink,2) : focusIcon(146,152,ink,16);
    s+=T(154,187,8,edit?'DONE':'FOCUS',ink,'middle',700);
    if(!edit)s+=eject(147,210,'#a5bfae')+T(154,234,7,'EJECT','#a5bfae','middle',600);
    if(edit)s+=T(154,221,7,'50mm','#d8e7dd','middle',600);
    s+=P('M143 243H165','#5c7565',1);
    s+=`<rect x="141" y="250" width="26" height="32" rx="3" fill="#3b5246"/>`;
    s+=edit ? P('M149 266H159','#d8e7dd',2) : P('M149 263L154 268L159 263','#d8e7dd',2);
    for(let i=0;i<4;i++)s+=`<rect x="${143+i*6}" y="291" width="4" height="3" rx="1" fill="${i===1?green:'#587261'}"/>`;
    return s+'</g></svg>';
}
function infoScreen(){
    let s=`<svg xmlns="http://www.w3.org/2000/svg" width="172" height="320" viewBox="0 0 172 320"><title>INFO — uptime moved from HOME</title><defs><style>${fonts}text{font-family:'Roboto Mono';}</style><clipPath id="info-screen"><rect width="172" height="320" rx="17"/></clipPath></defs><g clip-path="url(#info-screen)"><rect width="172" height="320" fill="#080f10"/>`;
    s+=T(12,22,11,'DEVICE INFO','#d9e5dd','start',600);
    s+=`<rect x="8.5" y="32.5" width="119" height="55" rx="4" fill="#17251f" stroke="#385646"/>`;
    s+=T(16,50,10,'UPTIME','#a8ddbd','start',600)+T(16,74,18,'00:12:34','#e1ebe5','start',600);
    const rows=[['BATTERY','3.92V / ~72%'],['MOTOR / IPROPI','0mA / 0mV'],['REBOOTS / SHOTS','R 12 / S 36'],['MOTOR TOTAL','00:04:28']];
    rows.forEach(([label,value],i)=>{const y=108+i*45;s+=T(12,y,9,label,'#91a99c','start',600)+T(12,y+19,12,value,'#d9e5dd')+P(`M12 ${y+29}H124`,'#2d4035',.7);});
    s+=T(12,304,9,'EEPROM','#91a99c')+T(124,304,10,'OK','#a8ddbd','end',600);
    s+=`<rect x="136" y="32" width="36" height="269" rx="4" fill="#293b35"/>${P('M136.5 36V297','#63816f',1)}`;
    s+=T(154,25,7,'INFO','#a5b9ad','middle',700);
    s+=`<rect x="141" y="38" width="26" height="32" rx="3" fill="#3b5246"/>${P('M149 57L154 52L159 57','#d8e7dd',2)}`;
    s+=P('M143 84H165','#5c7565',1);
    s+=`<rect x="139" y="139" width="30" height="61" rx="3" fill="#a8ddbd"/>${gear(147,153,'#10251e')}`+T(154,187,8,'SETUP','#10251e','middle',700);
    s+=P('M143 243H165','#5c7565',1)+`<rect x="141" y="250" width="26" height="32" rx="3" fill="#3b5246"/>${P('M149 263L154 268L159 263','#d8e7dd',2)}`;
    return s+'</g></svg>';
}
async function main(){
    const browser=await chromium.launch({executablePath:process.argv[3],headless:true});
    try{
        const page=await browser.newPage({viewport:{width:172,height:320},deviceScaleFactor:4});
        for(const [name,svg] of [['home-concept',screen(false)],['home-focus-edit',screen(true)],['info-concept',infoScreen()]]){
            fs.writeFileSync(path.join(__dirname,name+'.svg'),svg);
            await page.setContent(`<style>html,body{margin:0}svg{display:block}</style>${svg}`);
            await page.evaluate(async()=>{for(const f of ['500 12px "Roboto Mono"','700 44px "Space Mono"']){const faces=await document.fonts.load(f);if(!faces.length)throw Error('Font missing '+f);}await document.fonts.ready;});
            await page.screenshot({path:path.join(__dirname,name+'-4x.png')});
            await page.setViewportSize({width:172,height:320});
        }
        const board=`<!doctype html><meta charset="utf-8"><style>${fonts}*{box-sizing:border-box}body{margin:0;background:#e8ebe7;color:#21352b;font-family:'Roboto Mono';width:1320px;height:1040px;padding:42px 64px}.top{font-size:12px;letter-spacing:2px;color:#617366}h1{font-size:28px;font-weight:600;margin:14px 0 8px}.sub{font-size:12px;color:#617366}.screens{display:flex;gap:56px;margin-top:34px}.col{width:360px;flex-shrink:0}.tag{font-size:11px;letter-spacing:1px;margin-bottom:16px}.frame{width:360px;padding:8px;background:#1d2a23;border-radius:42px;box-shadow:0 12px 25px #172e2118}.frame svg{display:block;width:344px;height:640px;border-radius:34px}.note{margin-top:20px;font-size:11px;line-height:1.8;color:#526759}.foot{margin-top:26px;padding-top:16px;border-top:1px solid #c6cec6;font-size:11px;color:#617366;display:flex;justify-content:space-between}</style><div class="top">POLAMIYA / HOME CONCEPT 02</div><h1>Quieter surfaces. Clearer actions.</h1><div class="sub">172 × 320 px · Space Mono + Roboto Mono · 4 px cards / 3 px controls</div><div class="screens"><div class="col"><div class="tag">01 / HOME</div><div class="frame">${screen(false)}</div><div class="note">Sage focus / muted blue measurement<br>Selected action stays bright</div></div><div class="col"><div class="tag">02 / FOCUS EDIT</div><div class="frame">${screen(true)}</div><div class="note">Amber marks the editable target<br>Increase / confirm / decrease</div></div><div class="col"><div class="tag">03 / INFO</div><div class="frame">${infoScreen()}</div><div class="note">Uptime moved to device information<br>HOME keeps distance + film only</div></div></div><div class="foot"><span>DESIGN PREVIEW · SAMPLE VALUES · NO FIRMWARE CHANGES</span><span>2026.10.29</span></div>`;
        fs.writeFileSync(path.join(__dirname,'preview.html'),board);
        const bp=await browser.newPage({viewport:{width:1320,height:1040},deviceScaleFactor:1});
        await bp.setContent(board);await bp.evaluate(async()=>{await document.fonts.ready;});
        await bp.screenshot({path:path.join(__dirname,'preview.png')});
        console.log('Saved SVGs, 4× PNGs, preview.html and preview.png in '+__dirname);
    }finally{await browser.close();}
}
main().catch(e=>{console.error(e);process.exitCode=1;});
