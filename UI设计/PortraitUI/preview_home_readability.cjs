/* Render HOME readability states from the production SVG. No firmware or LVGL
 * simulator is run. Usage: node preview_home_readability.cjs <node_modules> [chrome.exe] */
const fs = require('fs');
const path = require('path');
const {chromium} = require(path.join(process.argv[2], 'playwright'));
const source = fs.readFileSync(path.join(__dirname, 'home.svg'), 'utf8');
const output = path.join(__dirname, 'proposals/2026-11-02-home-readable');
fs.mkdirSync(output, {recursive: true});

(async () => {
    const browser = await chromium.launch({
        executablePath: process.argv[3] || 'C:/Program Files/Google/Chrome/Application/chrome.exe',
        headless: true,
    });
    try {
        const page = await browser.newPage({viewport: {width: 172, height: 320}, deviceScaleFactor: 1});
        const states = [
            {name: 'home', title: 'NORMAL', marker: '', notice: ''},
            {name: 'home-init', title: 'INITIALIZE', marker: 'I', notice: 'INIT NEEDED'},
            {name: 'home-save-error', title: 'SAVE ERROR', marker: 'E', notice: 'SAVE ERROR'},
            {name: 'home-charging-low', title: 'CHARGING / LOW', marker: '', notice: '', charging: true, batteryPercent: 25},
            {name: 'home-charging-high', title: 'CHARGING / HIGH', marker: '', notice: '', charging: true, batteryPercent: 75},
        ];
        const rendered = [];
        const metrics = [];
        for (const state of states) {
            await page.setContent(`<style>html,body{margin:0;padding:0;overflow:hidden}svg{display:block}</style>${source}`);
            await page.evaluate(async state => {
                for (const font of ['400 14px "Roboto Mono"', '600 30px "Roboto Mono"', '700 50px "Space Mono"']) {
                    const faces = await document.fonts.load(font);
                    if (!faces.length || faces.some(f => f.status !== 'loaded')) throw new Error(`Missing font: ${font}`);
                }
                await document.fonts.ready;
                if (state.charging) {
                    document.querySelector('[data-ui-role="charging-bolt"]').setAttribute('display', 'inline');
                    document.querySelector('[data-ui-role="battery-fill"]')
                        .setAttribute('width', String(Math.ceil(15 * state.batteryPercent / 100)));
                }
                if (state.marker) {
                    const numbers = [...document.querySelectorAll('[data-ui-role="focus-value"]')];
                    const marker = numbers.shift();
                    for (const element of numbers) element.remove();
                    for (const [name, value] of Object.entries({
                        x: '68', y: '195', 'font-size': '30', 'font-family': 'Roboto Mono',
                        'font-weight': '600', 'text-anchor': 'middle',
                    })) marker.setAttribute(name, value);
                    marker.textContent = state.marker;
                    document.querySelector('[data-ui-role="focus-notice"]').textContent = state.notice;
                    const pointer = [...document.querySelectorAll('path[data-ui-live]')]
                        .find(p => p.getAttribute('d').includes(' 220h8'));
                    if (pointer) pointer.remove();
                }
            }, state);
            const audit = await page.evaluate(() => {
                const group = document.querySelector('svg > g[clip-path]');
                return [...group.querySelectorAll('text')].filter(t => t.textContent.trim()).map(t => {
                    const box = t.getBBox();
                    return {text: t.textContent, size: parseFloat(getComputedStyle(t).fontSize),
                        x: box.x, y: box.y, width: box.width, height: box.height};
                });
            });
            const tooSmall = audit.filter(t => t.size < 14);
            if (tooSmall.length) throw new Error(`HOME text below 14 px: ${JSON.stringify(tooSmall)}`);
            metrics.push({state: state.name, minimumFontPx: Math.min(...audit.map(t => t.size)), text: audit});
            const svg = await page.$eval('svg', node => node.outerHTML);
            fs.writeFileSync(path.join(output, state.name + '.svg'), svg);
            await page.screenshot({path: path.join(output, state.name + '-1x.png')});
            if (state.name === 'home') await page.screenshot({path: path.join(__dirname, 'home.png')});
            rendered.push({...state, svg});
        }
        fs.writeFileSync(path.join(output, 'layout-metrics.json'), JSON.stringify(metrics, null, 2) + '\n');
        await page.close();
        async function overviewImage(items, filename) {
        const html = `<!doctype html><meta charset="utf-8"><title>HOME minimum font 14 px</title>
<style>body{margin:0;padding:24px;background:#14211e;color:#e0eee7;font:14px monospace}
main{display:flex;gap:24px}figure{margin:0}figcaption{height:28px;font-weight:bold}
.screen{width:344px;height:640px;overflow:hidden}svg{display:block;width:344px;height:640px}
footer{margin-top:18px;color:#b9d0c4}</style><main>${items.map(s => `<figure><figcaption>${s.title}</figcaption><div class="screen">${s.svg}</div></figure>`).join('')}</main>
<footer>172 x 320 display / shown at 2x | minimum HOME text: 14 px | design preview, sample values</footer>`;
        fs.writeFileSync(path.join(output, filename + '.html'), html);
        const overview = await browser.newPage({viewport: {width: 1128, height: 752}, deviceScaleFactor: 1});
        await overview.setContent(html);
        await overview.evaluate(async () => {
            await document.fonts.load('400 14px "Roboto Mono"');
            await document.fonts.load('600 30px "Roboto Mono"');
            await document.fonts.load('700 50px "Space Mono"');
            await document.fonts.ready;
        });
        await overview.screenshot({path: path.join(output, filename + '.png')});
        await overview.close();
        }
        await overviewImage(rendered.slice(0, 3), 'preview');
        await overviewImage([rendered[0], ...rendered.slice(3)], 'charging-preview');
        console.log(`HOME previews saved to ${output}; all visible HOME text >= 14 px.`);
    } finally {
        await browser.close();
    }
})().catch(error => {console.error(error); process.exitCode = 1;});
