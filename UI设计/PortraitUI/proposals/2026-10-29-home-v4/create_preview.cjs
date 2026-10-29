/* Render the current HOME source and its edit state; no firmware is run. */
const fs = require('fs');
const path = require('path');
const {chromium} = require(path.join(process.argv[2], 'playwright'));
const source = fs.readFileSync(path.join(__dirname, '../../home.svg'), 'utf8');

(async () => {
    const browser = await chromium.launch({executablePath: process.argv[3], headless: true});
    try {
        const page = await browser.newPage({viewport: {width: 172, height: 320}, deviceScaleFactor: 4});
        await page.setContent(`<style>html,body{margin:0}svg{display:block}</style>${source}`);
        await page.evaluate(async () => {
            await document.fonts.load('700 14px "Roboto Mono"');
            await document.fonts.load('700 50px "Space Mono"');
            await document.fonts.ready;
        });
        fs.writeFileSync(path.join(__dirname, 'home-current.svg'), source);
        await page.screenshot({path: path.join(__dirname, 'home-current-4x.png')});
        const edit = await page.$eval('svg', root => {
            root.querySelector('[data-ui-role="focus-card"]').setAttribute('fill', '#ffbe58');
            root.querySelector('[data-ui-role="active-key"]').setAttribute('fill', '#ffbe58');
            const g = [...root.children].find(n => n.localName === 'g');
            for (const n of [...g.children]) {
                const transform = n.getAttribute('transform') || '';
                if (transform.startsWith('translate(143 109)') || transform.startsWith('translate(143 197)')) n.remove();
                if (transform.startsWith('translate(143 153)')) {
                    const check = document.createElementNS('http://www.w3.org/2000/svg', 'path');
                    for (const [key, value] of Object.entries({d:'M145 163L152 170L163 157',fill:'none',stroke:'#09291e','stroke-width':'3','stroke-linecap':'round','stroke-linejoin':'round','data-ui-live':'true'})) check.setAttribute(key,value);
                    n.replaceWith(check);
                }
                if (n.getAttribute('d') === 'M136 159L141 164L136 169Z') n.setAttribute('fill','#ffbe58');
                if (n.getAttribute('d') === 'M147 58L154 51L161 58') n.setAttribute('d','M147 54H161M154 47V61');
                if (n.getAttribute('d') === 'M147 262L154 269L161 262') n.setAttribute('d','M147 266H161');
            }
            return root.outerHTML;
        });
        fs.writeFileSync(path.join(__dirname,'home-edit.svg'), edit);
        await page.screenshot({path: path.join(__dirname,'home-edit-4x.png')});
        const board = `<!doctype html><meta charset="utf-8"><style>*{box-sizing:border-box}body{margin:0;padding:34px 52px;background:#edf1ec;color:#18372a;font-family:'Roboto Mono';width:920px;height:890px}h1{font-size:24px;margin:0 0 10px}p{font-size:12px;color:#597164;margin:0}.screens{display:flex;gap:64px;margin-top:28px}.screen{width:376px}.tag{font-size:12px;margin:0 0 12px}.frame{padding:8px;background:#17271f;border-radius:42px;width:360px}.frame svg{display:block;width:344px;height:640px;border-radius:34px}.foot{margin-top:24px;font-size:11px;color:#597164}</style><h1>ACTUAL above FOCUS</h1><p>Matching labels · Icon-only controls · Battery at the right</p><div class="screens"><div class="screen"><div class="tag">HOME</div><div class="frame">${source}</div></div><div class="screen"><div class="tag">FOCUS EDIT</div><div class="frame">${edit}</div></div></div><div class="foot">172 × 320 px · Enlarged design preview · Sample readings</div>`;
        fs.writeFileSync(path.join(__dirname,'preview.html'), board);
        const overview = await browser.newPage({viewport:{width:920,height:890},deviceScaleFactor:1});
        await overview.setContent(board);
        await overview.evaluate(async()=>{await document.fonts.ready;});
        await overview.screenshot({path:path.join(__dirname,'preview.png')});
        console.log('Rendered updated HOME and FOCUS edit previews.');
    } finally { await browser.close(); }
})().catch(error=>{console.error(error);process.exitCode=1;});
