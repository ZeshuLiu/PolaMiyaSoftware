/* Settings visual proposal, using the production HOME fonts and palette.
 * Usage: node render_settings_menu.cjs <node_modules> [chrome.exe]
 * No firmware or LVGL simulator is run; ON/OFF is a visual example only. */
const fs = require('fs');
const path = require('path');
const {chromium} = require(path.join(process.argv[2], 'playwright'));
const source = fs.readFileSync(path.join(__dirname, 'home.svg'), 'utf8');
const fontStyle = source.match(/<style>[\s\S]*?<\/style>/)[0];
const output = path.join(__dirname, 'proposals/2026-11-02-settings-menu');
fs.mkdirSync(output, {recursive: true});

const C = {bg:'#080f10', card:'#142720', border:'#2d463b', text:'#e0eee7',
    muted:'#91afa0', mint:'#79f5a7', ink:'#09291e', amber:'#ffbe58', blue:'#69d6ff'};
const rect = (x,y,w,h,fill,rx=0,stroke='none') =>
    `<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="${rx}" fill="${fill}" stroke="${stroke}"/>`;
const text = (x,y,value,size=14,color=C.text,weight=400,anchor='start') =>
    `<text x="${x}" y="${y}" font-family="Roboto Mono" font-size="${size}" font-weight="${weight}" text-anchor="${anchor}" fill="${color}">${value}</text>`;
const line = (d,color,width=2) =>
    `<path d="${d}" fill="none" stroke="${color}" stroke-width="${width}" stroke-linecap="round" stroke-linejoin="round"/>`;

function colors(selected, editing, busy=false) {
    if (busy) return {fill:'#10303b', title:C.blue, detail:C.blue, stroke:C.blue};
    if (selected) return {fill:editing?C.amber:C.mint, title:C.ink, detail:'#295a40', stroke:'none'};
    return {fill:C.card, title:C.text, detail:C.muted, stroke:C.border};
}

function numericCard(y,h,selected,editing,value) {
    const c=colors(selected,editing);
    const track=selected?'#477c57':C.border;
    const fill=selected?C.ink:C.mint;
    return rect(8,y,120,h,c.fill,6,c.stroke) +
        text(16,y+20,'BACKLIGHT',14,c.title,700) +
        text(16,y+50,String(value),24,c.title,700) +
        text(49,y+50,'%',14,c.title,700) +
        rect(78,y+42,40,4,track,2) + rect(78,y+42,40*value/100,4,fill,2);
}

function actionCard(y,h,selected,busy) {
    const c=colors(selected,false,busy);
    const cy=y+h/2;
    let icon=line(`M106 ${cy-7}L113 ${cy}L106 ${cy+7}`,c.title,2.5);
    if (busy) icon=[0,1,2].map(i=>`<circle cx="${103+i*6}" cy="${cy}" r="1.7" fill="${C.blue}"/>`).join('');
    return rect(8,y,120,h,c.fill,6,c.stroke) +
        text(16,y+20,'SEEK ZERO',14,c.title,700) +
        text(16,y+h-15,busy?'RUNNING':'START',14,c.detail,busy?700:400) + icon;
}

function choiceCard(y,h,value,editing) {
    const c=colors(true,editing);
    const on=value==='ON';
    return rect(8,y,120,h,c.fill,6) + text(16,y+20,'KEY TONE',14,c.title,700) +
        rect(16,y+28,104,26,'none',5,'#477c57') +
        rect(on?68:17,y+29,51,24,C.ink,4) +
        text(42,y+47,'OFF',14,on?C.ink:c.fill,700,'middle') +
        text(94,y+47,'ON',14,on?c.fill:C.ink,700,'middle');
}

function backRow(y,h,selected) {
    const c=colors(selected,false);
    const cy=y+h/2;
    return rect(8,y,120,h,c.fill,6,c.stroke) +
        line(`M28 ${cy-5}L23 ${cy}L28 ${cy+5}M23 ${cy}H34`,c.title,2) +
        text(43,cy+5,'BACK',14,c.title,700);
}

const languageOptions=['ENGLISH','GERMAN','FRENCH','ITALIAN','SPANISH','JAPANESE','KOREAN','PORTUGUESE'];

function submenuCard(y,h,value) {
    const c=colors(true,false);
    return rect(8,y,120,h,c.fill,6) + text(16,y+20,'LANGUAGE',14,c.title,700) +
        text(16,y+h-15,value,14,c.detail) +
        line(`M106 ${y+h/2-7}L113 ${y+h/2}L106 ${y+h/2+7}`,c.title,2.5);
}

function keyRail(mode) {
    const edit=mode==='numeric-edit'||mode==='choice';
    let result=rect(136,32,36,274,'#d4ffe7',4) +
        rect(140,38,28,34,'#9ce8ba',3) + rect(140,250,28,34,'#9ce8ba',3) +
        line('M142 84H166M142 244H166','#78ad8a',1) +
        rect(139,142,30,44,mode==='busy'?'#365148':edit?C.amber:C.ink,3);
    if(mode==='numeric-edit') result+=line('M147 55H161M154 48V62M147 267H161',C.ink,3);
    else result+=line('M147 58L154 51L161 58M147 262L154 269L161 262',C.ink,3);
    if(mode==='busy') result+=[0,1,2].map(i=>`<circle cx="${147+i*7}" cy="164" r="2" fill="#9ce8ba"/>`).join('');
    else if(mode==='action') result+=`<path d="M150 155L160 164L150 173Z" fill="${C.mint}"/>`;
    else if(mode==='submenu') result+=line('M150 155L159 164L150 173',C.mint,3);
    else result+=line('M146 165L152 171L163 159',edit?C.ink:C.mint,3);
    return result;
}

function wrapScreen(title,body) {
    return `<svg xmlns="http://www.w3.org/2000/svg" width="172" height="320" viewBox="0 0 172 320"><title>${title}</title><defs>${fontStyle}<clipPath id="settings-screen"><rect width="172" height="320" rx="20"/></clipPath></defs><g clip-path="url(#settings-screen)">${body}</g></svg>`;
}

function optionListScreen(state) {
    const visible=5;
    const start=Math.max(0,Math.min(state.cursor-2,languageOptions.length-visible));
    let body=rect(0,0,172,320,C.bg) + text(12,22,'LANGUAGE',16,C.text,700) +
        text(128,22,`${state.cursor+1}/${languageOptions.length}`,14,C.muted,400,'end') +
        line('M12 31H128',C.border,1) +
        text(12,55,languageOptions[state.applied],14,C.mint) +
        line('M108 50L112 54L119 46',C.mint,2);
    for(let row=0;row<visible;++row) {
        const index=start+row, y=72+row*40;
        const focused=index===state.cursor&&!state.backSelected;
        const applied=index===state.applied;
        const c=colors(focused,true);
        body+=rect(8,y,114,32,c.fill,5,c.stroke) +
            text(16,y+21,languageOptions[index],14,c.title,focused?700:400);
        if(applied) body+=line(`M105 ${y+16}L109 ${y+20}L116 ${y+12}`,focused?C.ink:C.mint,2);
    }
    const thumb=192*visible/languageOptions.length;
    body+=rect(126,72,2,192,C.border,1) +
        rect(126,72+(192-thumb)*start/(languageOptions.length-visible),2,thumb,C.mint,1) +
        backRow(276,28,!!state.backSelected) + keyRail(state.backSelected?'browse':'choice');
    return wrapScreen(state.title,body);
}

function screen(state) {
    if(state.child) return optionListScreen(state);
    const submenu=state.selected==='submenu';
    const future=state.selected==='choice'||submenu;
    const busy=!!state.busy;
    let body=rect(0,0,172,320,C.bg) + text(12,22,'SETTINGS',16,C.text,700) +
        line('M12 31H128',C.border,1);
    body+=numericCard(42,future?60:68,state.selected==='numeric',state.editing,state.percent);
    body+=actionCard(future?110:120,future?56:64,state.selected==='action',busy);
    if(submenu) body+=submenuCard(174,64,languageOptions[state.applied]);
    else if(future) body+=choiceCard(174,64,state.choice,true);
    body+=backRow(future?246:196,future?36:40,state.selected==='back');
    if(future) body+=text(12,305,submenu?(state.changed?'UNSAVED':'PRESS TO OPEN'):'PRESS TO SET',14,state.changed?C.amber:C.muted);
    else {
        let status='SAVED', hint='HOLD TO SAVE', color=C.mint;
        if(state.editing) {status='UNSAVED';color=C.amber;}
        if(state.selected==='action') {status=busy?'HOMING...':'FOCUS READY';hint=busy?'PLEASE WAIT':'PRESS TO RUN';color=busy?C.blue:C.muted;}
        if(state.selected==='back') {status='RETURN HOME';hint='PRESS OK';color=C.muted;}
        body+=rect(8,250,120,56,'#0e1d18',6) + text(16,270,status,14,color,700) +
            text(16,296,hint,14,C.muted);
    }
    const mode=busy?'busy':submenu?'submenu':future?'choice':state.selected==='action'?'action':state.editing?'numeric-edit':'browse';
    body+=keyRail(mode);
    return wrapScreen(state.title,body);
}

const states=[
    {name:'numeric-browse',title:'Numeric / selected',selected:'numeric',editing:false,percent:80},
    {name:'numeric-edit',title:'Numeric / editing',selected:'numeric',editing:true,percent:80},
    {name:'action-ready',title:'Action / selected',selected:'action',editing:false,percent:80},
    {name:'action-running',title:'Action / running',selected:'action',editing:false,percent:80,busy:true},
    {name:'choice-on',title:'ON-OFF / example',selected:'choice',editing:true,percent:80,choice:'ON'},
    {name:'choice-off',title:'ON-OFF / example',selected:'choice',editing:true,percent:80,choice:'OFF'},
    {name:'back',title:'Back / selected',selected:'back',editing:false,percent:80},
    {name:'submenu-entry',title:'More choices / enter',selected:'submenu',percent:80,applied:0},
    {name:'submenu-current',title:'Current value / check',child:true,cursor:0,applied:0},
    {name:'submenu-select',title:'Choose / not applied yet',child:true,cursor:4,applied:0},
    {name:'submenu-scroll',title:'Scroll / 8 choices',child:true,cursor:7,applied:0},
    {name:'submenu-cancel',title:'Back / keep old value',child:true,cursor:7,applied:0,backSelected:true},
    {name:'submenu-returned',title:'Confirm / return',selected:'submenu',percent:80,applied:4,changed:true},
];

(async()=>{
    const browser=await chromium.launch({executablePath:process.argv[3]||'C:/Program Files/Google/Chrome/Application/chrome.exe',headless:true});
    try {
        const page=await browser.newPage({viewport:{width:172,height:320},deviceScaleFactor:1});
        const metrics=[];
        for(const state of states) {
            state.svg=screen(state);
            fs.writeFileSync(path.join(output,state.name+'.svg'),state.svg);
            await page.setContent('<style>html,body{margin:0;padding:0}svg{display:block}</style>'+state.svg);
            await page.evaluate(async()=>{
                for(const font of ['400 14px "Roboto Mono"','700 16px "Roboto Mono"','700 24px "Roboto Mono"']) {
                    const faces=await document.fonts.load(font);
                    if(!faces.length||faces.some(f=>f.status!=='loaded'))throw new Error('Missing font '+font);
                }
                await document.fonts.ready;
            });
            const textBoxes=await page.evaluate(()=>[...document.querySelectorAll('svg > g text')].map(t=>{
                const b=t.getBBox();return {text:t.textContent,size:parseFloat(getComputedStyle(t).fontSize),x:b.x,y:b.y,width:b.width,height:b.height};
            }));
            for(const t of textBoxes) {
                if(t.size<14||t.x<7||t.x+t.width>130||t.y<0||t.y+t.height>315)
                    throw new Error(`Text outside design bounds in ${state.name}: ${JSON.stringify(t)}`);
            }
            metrics.push({state:state.name,minimumFontPx:Math.min(...textBoxes.map(t=>t.size)),text:textBoxes});
            await page.screenshot({path:path.join(output,state.name+'-1x.png')});
        }
        fs.writeFileSync(path.join(output,'layout-metrics.json'),JSON.stringify(metrics,null,2)+'\n');
        await page.close();
        async function renderOverview(names,filename) {
            const items=names.map(n=>states.find(s=>s.name===n));
            const html=`<!doctype html><meta charset="utf-8"><title>Settings menu design</title><style>body{margin:0;padding:24px;background:#14211e;color:#e0eee7;font:14px monospace}main{display:flex;gap:24px}figure{margin:0}figcaption{height:28px;font-weight:bold}.screen,svg{display:block;width:344px;height:640px}footer{margin-top:18px;color:#b9d0c4}</style><main>${items.map(s=>`<figure><figcaption>${s.title}</figcaption><div class="screen">${s.svg}</div></figure>`).join('')}</main><footer>172 x 320 / shown at 2x | text >= 14 px | options are style examples; sample values</footer>`;
            fs.writeFileSync(path.join(output,filename+'.html'),html);
            const overview=await browser.newPage({viewport:{width:1128,height:752}});
            await overview.setContent(html);
            await overview.evaluate(async()=>{
                for(const f of ['400 14px "Roboto Mono"','700 16px "Roboto Mono"','700 24px "Roboto Mono"'])await document.fonts.load(f);
                await document.fonts.ready;
            });
            await overview.screenshot({path:path.join(output,filename+'.png')});
            await overview.close();
        }
        await renderOverview(['numeric-edit','action-ready','choice-on'],'preview');
        await renderOverview(['numeric-browse','action-running','choice-off'],'states');
        await renderOverview(['submenu-entry','submenu-select','submenu-returned'],'submenu-preview');
        await renderOverview(['submenu-current','submenu-scroll','submenu-cancel'],'submenu-states');
        console.log('Rendered settings menu proposal:',output);
    } finally {await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
