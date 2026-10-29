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
    const ink='#10281f', pale='#d5eddf', green='#c7f2d3', blue='#86ceef', amber='#ffbe64';
    let s=`<svg xmlns="http://www.w3.org/2000/svg" width="172" height="320" viewBox="0 0 172 320"><title>HOME concept — ${edit?'focus edit':'browse'}</title><defs><style>${fonts}text{font-family:'Roboto Mono';}</style><clipPath id="screen"><rect width="172" height="320" rx="17"/></clipPath></defs><g clip-path="url(#screen)"><rect width="172" height="320" fill="#080f10"/>`;
    ['#f07d72','#efbd65','#d6e8c4','#81c7bb','#86b9dc'].forEach((c,i)=>{s+=`<rect x="${12+i*7}" y="13" width="6" height="5" fill="${c}"/>`;});
    s+=`<rect x="107" y="11" width="18" height="9" rx="1" fill="none" stroke="#cfdfd6"/><rect x="109" y="13" width="11" height="5" fill="#cfdfd6"/>${P('M127 14V17','#cfdfd6',2)}`;
    s+=`<rect x="8" y="32" width="120" height="85" rx="7" fill="${edit?amber:green}"/>`;
    s+=T(16,51,12,'FOCUS',ink,'start',700);
    if(edit) s+=T(119,50,8,'EDIT',ink,'end',700);
    else s+=focusIcon(108,40,ink,10);
    s+=number('2.5',100,44,ink);
    s+=`<rect x="8" y="125" width="120" height="77" rx="7" fill="#11242d"/>`;
    s+=T(16,145,12,'ACTUAL',blue,'start',600)+number('2.4',187,40,blue);
    const sx = v=>14+89*Math.log(1+v)/Math.log(21);
    s+=P('M14 221H103M108 221H112','#799389',1)+P('M119 217V225','#799389',1);
    [0,1,5,20].forEach(v=>{const x=sx(v);s+=P(`M${x} 218V224`,'#799389',1)+T(x,239,8,String(v),'#b4c6bd','middle');});
    s+=T(119,239,7,'INF','#b4c6bd','middle');
    s+=`<path d="M${sx(2.5)-3} 210h6l-3 6Z" fill="${green}"/><path d="M${sx(2.4)-3} 231h6l-3-6Z" fill="${blue}"/>`;
    s+=P('M12 250H124','#34473e',.7)+T(12,266,10,'FILM','#b4c6bd', 'start',600)+T(124,267,15,'6',green,'end',700);
    for(let i=0;i<8;i++) s+=`<rect x="${12+i*14.2}" y="275" width="11" height="10" rx="1" fill="${i<6?green:'#2b4036'}"/><rect x="${14+i*14.2}" y="277" width="7" height="5" fill="${i<6?'#547d66':'#15251f'}"/>`;
    s+=T(12,307,9,'UP','#92aa9c')+T(124,307,11,'00:12:34','#d4e1d9','end');
    // Full-height rail is aligned to the existing physical key centers, 54/160/266.
    s+=`<rect x="136" y="0" width="36" height="320" fill="${pale}"/>${P('M136.5 15V305','#819b8b',1)}`;
    s+=T(154,25,7,edit?'EDIT':'MENU','#4c6959','middle',700);
    s+=`<rect x="141" y="38" width="26" height="32" rx="5" fill="#b3cdbd"/>`;
    s+= edit ? P('M149 54H159M154 49V59',ink,2) : P('M149 57L154 52L159 57',ink,2);
    s+=P('M143 84H165','#8ba696',1);
    if(!edit)s+=gear(147,105,'#557963')+T(154,130,7,'SETUP','#557963','middle',600);
    s+=`<rect x="139" y="139" width="30" height="61" rx="5" fill="${edit?amber:ink}"/>`;
    s+=`<path d="M136 154L141 160L136 166Z" fill="#080f10"/>`;
    s+=edit ? P('M148 159L152 163L160 154',ink,2) : focusIcon(146,152,green,16);
    s+=T(154,187,8,edit?'DONE':'FOCUS',edit?ink:green,'middle',700);
    if(!edit)s+=eject(147,210,'#557963')+T(154,234,7,'EJECT','#557963','middle',600);
    if(edit)s+=T(154,221,7,'50mm',ink,'middle',600);
    s+=P('M143 243H165','#8ba696',1);
    s+=`<rect x="141" y="250" width="26" height="32" rx="5" fill="#b3cdbd"/>`;
    s+=edit ? P('M149 266H159',ink,2) : P('M149 263L154 268L159 263',ink,2);
    for(let i=0;i<4;i++)s+=`<rect x="${143+i*6}" y="298" width="4" height="3" rx="1" fill="${i===1?ink:'#9cb8a7'}"/>`;
    return s+'</g></svg>';
}
async function main(){
    const browser=await chromium.launch({executablePath:process.argv[3],headless:true});
    try{
        const page=await browser.newPage({viewport:{width:172,height:320},deviceScaleFactor:4});
        for(const [name,edit] of [['home-concept',false],['home-focus-edit',true]]){
            const svg=screen(edit);fs.writeFileSync(path.join(__dirname,name+'.svg'),svg);
            await page.setContent(`<style>html,body{margin:0}svg{display:block}</style>${svg}`);
            await page.evaluate(async()=>{for(const f of ['500 12px "Roboto Mono"','700 44px "Space Mono"']){const faces=await document.fonts.load(f);if(!faces.length)throw Error('Font missing '+f);}await document.fonts.ready;});
            await page.screenshot({path:path.join(__dirname,name+'-4x.png')});
            await page.setViewportSize({width:172,height:320});
        }
        const board=`<!doctype html><meta charset="utf-8"><style>${fonts}*{box-sizing:border-box}body{margin:0;background:#e9eee9;color:#183026;font-family:'Roboto Mono';width:1120px;height:1230px;padding:48px 70px}.top{font-size:13px;letter-spacing:2px;color:#607468}h1{font-size:31px;font-weight:600;margin:15px 0 8px}.sub{font-size:13px;color:#607468}.screens{display:flex;gap:80px;margin-top:40px}.col{width:450px}.tag{font-size:12px;letter-spacing:1px;margin-bottom:18px}.frame{width:450px;padding:10px;background:#15271f;border-radius:52px;box-shadow:0 18px 32px #172e2120}.frame svg{display:block;width:430px;height:800px;border-radius:42px}.note{margin-top:23px;font-size:12px;line-height:1.8;color:#4b6557}.foot{margin-top:32px;padding-top:16px;border-top:1px solid #c4d0c6;font-size:11px;color:#607468;display:flex;justify-content:space-between}</style><div class="top">POLAMIYA / HOME CONCEPT 01</div><h1>Distance first. Controls in sight.</h1><div class="sub">172 × 320 px · Space Mono + Roboto Mono · 36 px control rail</div><div class="screens"><div class="col"><div class="tag">01 / BROWSE</div><div class="frame">${screen(false)}</div><div class="note">Previous / current / next action<br>Three keys aligned at 54 / 160 / 266 px</div></div><div class="col"><div class="tag">02 / FOCUS EDIT</div><div class="frame">${screen(true)}</div><div class="note">Amber target + amber confirmation<br>Increase / confirm / decrease</div></div></div><div class="foot"><span>DESIGN PREVIEW · SAMPLE VALUES</span><span>2026.10.29</span></div>`;
        fs.writeFileSync(path.join(__dirname,'preview.html'),board);
        const bp=await browser.newPage({viewport:{width:1120,height:1230},deviceScaleFactor:1});
        await bp.setContent(board);await bp.evaluate(async()=>{await document.fonts.ready;});
        await bp.screenshot({path:path.join(__dirname,'preview.png')});
        console.log('Saved SVGs, 4× PNGs, preview.html and preview.png in '+__dirname);
    }finally{await browser.close();}
}
main().catch(e=>{console.error(e);process.exitCode=1;});
