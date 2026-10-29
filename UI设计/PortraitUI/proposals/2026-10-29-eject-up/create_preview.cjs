/* Preview only: preserve the current EJECT source and all firmware assets. */
const fs = require('fs');
const path = require('path');
const {chromium} = require(path.join(process.argv[2], 'playwright'));
const original = fs.readFileSync(path.join(__dirname, '../../eject.svg'), 'utf8');

(async () => {
    const browser = await chromium.launch({executablePath:process.argv[3], headless:true});
    try {
        const page = await browser.newPage({viewport:{width:172,height:320},deviceScaleFactor:1});
        await page.setContent(`<style>html,body{margin:0}svg{display:block}</style>${original}`);
        await page.evaluate(async()=>{
            for(const font of ['700 28px "Space Mono"','700 12px "Roboto Mono"']) {
                const faces=await document.fonts.load(font);
                if(!faces.length)throw Error('Missing font '+font);
            }
            await document.fonts.ready;
        });
        const template = await page.$eval('svg', root => {
            root.querySelector('title').textContent='EJECT preview - upward paper, bottom timer';
            root.querySelector('#eject-paper').setAttribute('transform','rotate(180 21 25)');
            const label=[...root.querySelectorAll('text')].find(n=>n.textContent==='EJECT TIME');
            for(const [k,v] of Object.entries({x:'78',y:'267','font-size':'12','font-weight':'700','text-anchor':'middle','letter-spacing':'0'})) label.setAttribute(k,v);
            for(const n of root.querySelectorAll('text[y="83"]')) n.setAttribute('y','302');
            const divider=[...root.querySelectorAll('path')].find(n=>n.getAttribute('d')==='M12 98H144');
            divider.setAttribute('d','M12 248H144');
            // Give the upper slot a clear gap above the current-gauge ticks.
            for (const parent of [root.querySelector('[data-eject-static]'),root.querySelector('[data-eject-live]')]) {
                const gauge=document.createElementNS('http://www.w3.org/2000/svg','g');
                gauge.setAttribute('transform','translate(0 8)');
                for(const n of [...parent.children]) {
                    if(n===label || n===divider || (n.localName==='text' && n.getAttribute('y')==='302'))continue;
                    gauge.appendChild(n);
                }
                parent.appendChild(gauge);
            }
            const window=root.querySelector('#eject-paper-window rect');
            window.setAttribute('y','34');window.setAttribute('height','60');
            const paper=root.querySelector('g[clip-path="url(#eject-paper-window)"]');
            paper.id='upward-paper';
            paper.querySelector('use').setAttribute('transform','translate(57 40)');
            const slot=root.querySelector('rect[x="43"][y="240"]');
            slot.setAttribute('y','94');
            // Paint the slot after the paper; hidden portions stay below its lip.
            slot.parentNode.insertBefore(paper,slot);
            return root.outerHTML;
        });
        const frame = (y,opacity=1) => template.replace('translate(57 40)',`translate(57 ${y})`).replace('id="upward-paper"',`id="upward-paper" opacity="${opacity}"`);
        const shots=[['start',82],['middle',65],['out',40]];
        for(const [name,y] of shots) {
            const svg=frame(y);
            fs.writeFileSync(path.join(__dirname,`eject-${name}.svg`),svg);
            await page.setContent(`<style>html,body{margin:0}svg{display:block}</style>${svg}`);
            await page.evaluate(async()=>{await document.fonts.ready;});
            await page.screenshot({path:path.join(__dirname,`eject-${name}-1x.png`)});
        }
        const fonts=original.match(/@font-face\{[^}]*\}/g).join('\n');
        const board=`<!doctype html><meta charset="utf-8"><style>${fonts}*{box-sizing:border-box}body{margin:0;padding:38px 60px;background:#eaf0e9;color:#18392a;font-family:'Roboto Mono';width:1312px;height:906px}h1{font-size:27px;margin:0 0 10px}.sub{font-size:12px;color:#5b7562}.row{display:flex;gap:56px;margin-top:30px}.col{width:360px}.label{font-size:12px;margin-bottom:12px}.frame{width:360px;padding:8px;border-radius:40px;background:#173120}.frame svg{display:block;width:344px;height:640px;border-radius:32px}.foot{font-size:11px;color:#5b7562;margin-top:24px}</style><h1>EJECT / UPWARD MOTION</h1><div class="sub">Paper at the top · Current in the middle · Elapsed time at the bottom</div><div class="row">${shots.map(([name,y],i)=>`<div class="col"><div class="label">${['01 / EMERGING','02 / MOVING UP','03 / FULLY OUT'][i]}</div><div class="frame">${frame(y)}</div></div>`).join('')}</div><div class="foot">172 × 320 px · Design preview only · Existing fonts and colors</div>`;
        fs.writeFileSync(path.join(__dirname,'preview.html'),board);
        const overview=await browser.newPage({viewport:{width:1312,height:906},deviceScaleFactor:1});
        await overview.setContent(board);await overview.evaluate(async()=>{await document.fonts.ready;});
        await overview.screenshot({path:path.join(__dirname,'preview.png')});
        // 26 frames over the existing 2.6 s cycle: rise, pause, fade, repeat.
        const framesDir=path.join(__dirname,'frames');fs.mkdirSync(framesDir,{recursive:true});
        await page.setContent(`<style>html,body{margin:0}svg{display:block}</style>${template}`);
        await page.evaluate(async()=>{await document.fonts.ready;});
        for(let i=0;i<26;i++) {
            const phase=i/26,y=94-54*Math.min(phase/.72,1),opacity=phase<.82?1:(1-phase)/.18;
            await page.evaluate(({y,opacity})=>{
                document.querySelector('#upward-paper use').setAttribute('transform',`translate(57 ${y})`);
                document.querySelector('#upward-paper').setAttribute('opacity',String(opacity));
            },{y,opacity});
            await page.screenshot({path:path.join(framesDir,`${String(i).padStart(2,'0')}.png`)});
        }
        console.log('Saved upward EJECT layout, three phases and 26 animation frames.');
    } finally { await browser.close(); }
})().catch(error=>{console.error(error);process.exitCode=1;});
