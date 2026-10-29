/* Render the current dual-font design; no firmware or LVGL simulator is run. */
const fs=require('fs');
const path=require('path');
const {chromium}=require(path.join(process.argv[2],'playwright'));
(async()=>{
    const browser=await chromium.launch({executablePath:process.argv[3]||'C:/Program Files/Google/Chrome/Application/chrome.exe',headless:true});
    try {
        const page=await browser.newPage({viewport:{width:172,height:320},deviceScaleFactor:1});
        for(const name of ['home','boot','eject']) {
            const svg=fs.readFileSync(path.join(__dirname,name+'.svg'),'utf8');
            await page.setContent('<style>html,body{margin:0;padding:0;overflow:hidden}svg{display:block}</style>'+svg);
            await page.evaluate(async()=>{
                for(const font of ['14px "Roboto Mono"','700 34px "Space Mono"']) {
                    const faces=await document.fonts.load(font);
                    if(!faces.length||faces.some(face=>face.status!=='loaded'))throw new Error('Required font missing: '+font);
                }
                await document.fonts.ready;
            });
            await page.screenshot({path:path.join(__dirname,name+'.png')});
        }
        console.log('Rendered home.png, boot.png and eject.png at 172x320.');
    } finally {await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
