/* Export the dual-mono SVG at its native size. No firmware build is run.
 * Usage: node export_lvgl_assets.cjs <absolute-node_modules> [chrome.exe]
 * Requires playwright and sharp. The SVG embeds the repository's OFL font. */
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const modules = process.argv[2];
const { chromium } = require(path.join(modules, 'playwright'));
const sharp = require(path.join(modules, 'sharp'));
const output = path.resolve(__dirname, '../../code/PM_Controller/main/app/ui');
const svg = fs.readFileSync(path.join(__dirname, 'home.svg'), 'utf8');
const bootSvg = fs.readFileSync(path.join(__dirname, 'boot.svg'), 'utf8');
const hash = crypto.createHash('sha256').update(svg).digest('hex');
const bootHash = crypto.createHash('sha256').update(bootSvg).digest('hex');
const fontHash = crypto.createHash('sha256').update(fs.readFileSync(path.join(output,'fonts/RobotoMono/RobotoMono[wght].ttf'))).digest('hex');
const displayFontHash = crypto.createHash('sha256').update(fs.readFileSync(path.join(output,'fonts/SpaceMono/SpaceMono-Bold.ttf'))).digest('hex');
const declarations = [];
const chunks = [`/* Generated from UI设计/PortraitUI/home.svg and boot.svg\n * Home SVG SHA-256: ${hash}\n * Boot SVG SHA-256: ${bootHash}\n * Roboto Mono TTF SHA-256: ${fontHash}\n * Space Mono TTF SHA-256: ${displayFontHash}\n * Font glyphs: Copyright 2015 The Roboto Mono Project Authors.\n * Copyright 2016 The Space Mono Project Authors.\n * Font derivatives use SIL OFL 1.1; see fonts/RobotoMono/OFL.txt and fonts/SpaceMono/OFL.txt.\n * Regenerate with UI设计/PortraitUI/export_lvgl_assets.cjs. */`, '#include "ui_home_assets.h"'];
const bytes = data => Array.from({length:Math.ceil(data.length/20)},(_,i)=>'    '+Array.from(data.subarray(i*20,(i+1)*20),v=>'0x'+v.toString(16).padStart(2,'0')).join(', ')+',').join('\n');

function image(name, data, w, h, alpha, mask=false, rowStride) {
    const format = mask?'LV_COLOR_FORMAT_A8':alpha?'LV_COLOR_FORMAT_ARGB8888':'LV_COLOR_FORMAT_RGB565';
    const stride = rowStride??w*(mask?1:alpha?4:2);
    chunks.push(`_Alignas(4) static const uint8_t ${name}_pixels[] = {\n${bytes(data)}\n};\nconst lv_image_dsc_t ${name} = {\n    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = ${format}, .w = ${w}, .h = ${h}, .stride = ${stride}},\n    .data_size = sizeof(${name}_pixels), .data = ${name}_pixels,\n};`);
    declarations.push(`extern const lv_image_dsc_t ${name};`);
}

function cropRgb(pixels, sourceWidth, x, y, w, h) {
    const cropped=Buffer.alloc(w*h*2,Buffer.from([0x41,0x08]));
    for(let row=0;row<h;row++) {
        const start=((y+row)*sourceWidth+x)*2;
        pixels.copy(cropped,row*w*2,start,start+Math.min(w,sourceWidth-x)*2);
    }
    return cropped;
}

function homeTiles(pixels,w,h) {
    // Retain the exact RGB565 pixels of each nonempty horizontal band.
    // Uniform background rows and outer margins need no stored pixels.
    const bands=[];
    let band;
    for(let y=0;y<h;y++) {
        let x0=w,x1=-1;
        for(let x=0;x<w;x++) if(pixels.readUInt16LE((y*w+x)*2)!==0x0841) {
            x0=Math.min(x0,x);x1=Math.max(x1,x);
        }
        if(x1<0) {band=null;continue;}
        if(!band) {band={x:x0,y,w:x1-x0+1,h:1};bands.push(band);}
        else {
            const right=Math.max(band.x+band.w-1,x1);
            band.x=Math.min(band.x,x0);band.w=right-band.x+1;band.h++;
        }
    }
    bands.forEach((b,i)=>{
        // Even widths give 4-byte-aligned RGB565 rows without a RAM copy.
        if(b.w%2) {if(b.x+b.w<w)b.w++;else{b.x--;b.w++;}}
        image(`ui_home_tile_${i}`,cropRgb(pixels,w,b.x,b.y,b.w,b.h),b.w,b.h,false);
    });
    declarations.push('typedef struct { int16_t x, y; const lv_image_dsc_t *image; } ui_image_tile_t;');
    declarations.push(`extern const ui_image_tile_t ui_home_tiles[${bands.length}];`);
    chunks.push(`const ui_image_tile_t ui_home_tiles[${bands.length}] = {\n`+
                bands.map((b,i)=>`    {.x=${b.x}, .y=${b.y}, .image=&ui_home_tile_${i}},`).join('\n')+'\n};');
}

function font(name, data) {
    const raw = Buffer.from(data.pixels);
    chunks.push(`_Alignas(4) static const uint8_t ${name}_pixels[] = {\n${bytes(raw)}\n};`);
    chunks.push(`static const lv_font_fmt_txt_glyph_dsc_t ${name}_glyphs[] = {\n    {0},\n${data.glyphs.map(g=>`    {.bitmap_index=${g.index}, .adv_w=${g.advance}, .box_w=${g.w}, .box_h=${g.h}, .ofs_x=${g.x}, .ofs_y=${g.y}},`).join('\n')}\n};`);
    chunks.push(`static const uint16_t ${name}_unicode[] = {${data.glyphs.map(g=>g.cp-32).join(', ')}};\nstatic const lv_font_fmt_txt_cmap_t ${name}_cmap[] = {{\n    .range_start=32, .range_length=95, .glyph_id_start=1,\n    .unicode_list=${name}_unicode, .list_length=${data.glyphs.length},\n    .type=LV_FONT_FMT_TXT_CMAP_SPARSE_TINY,\n}};`);
    chunks.push(`static const lv_font_fmt_txt_dsc_t ${name}_data = {\n    .glyph_bitmap=${name}_pixels, .glyph_dsc=${name}_glyphs, .cmaps=${name}_cmap,\n    .cmap_num=1, .bpp=8, .bitmap_format=LV_FONT_FMT_TXT_PLAIN,\n};\nconst lv_font_t ${name} = {\n    .get_glyph_dsc=lv_font_get_glyph_dsc_fmt_txt, .get_glyph_bitmap=lv_font_get_bitmap_fmt_txt,\n    .line_height=${data.lineHeight}, .base_line=${data.baseline}, .subpx=LV_FONT_SUBPX_NONE,\n    .dsc=&${name}_data,\n};`);
    declarations.push(`LV_FONT_DECLARE(${name});`);
}

(async()=>{
    const browser=await chromium.launch({executablePath:process.argv[3]||'C:/Program Files/Google/Chrome/Application/chrome.exe',headless:true});
    try {
        const page=await browser.newPage({viewport:{width:172,height:320},deviceScaleFactor:1});
        await page.setContent('<style>html,body{margin:0;padding:0;overflow:hidden}svg{display:block}</style>'+svg);
        async function loadFont() {
            await page.evaluate(async()=>{
                for(const font of ['14px "Roboto Mono"','700 34px "Space Mono"']) {
                    const faces=await document.fonts.load(font);
                    if(!faces.length||faces.some(face=>face.status!=='loaded')) throw new Error('Required font did not load: '+font);
                }
                await document.fonts.ready;
            });
        }
        await loadFont();
        const definitions=await page.$eval('svg',e=>e.querySelector('defs').outerHTML);
        const staticBody=await page.$eval('svg',root=>{
            root=root.cloneNode(true);
            for(const node of [...root.children]) {
                if(['title','desc','defs'].includes(node.localName)) {node.remove();continue;}
                if(node.localName==='use'||node.querySelector('use')) {node.remove();continue;}
                if(node.hasAttribute('data-ui-distance')||node.hasAttribute('data-ui-distance-unit')) {node.remove();continue;}
                if(node.hasAttribute('data-ui-battery-fill')||node.hasAttribute('data-ui-live')) {node.remove();continue;}
                if(node.localName==='text'&&['2.50','00:12:34','6'].includes(node.textContent)) node.remove();
                if(node.localName==='rect') {
                    const x=Number(node.getAttribute('x')),y=Number(node.getAttribute('y'));
                    if(y===267||y===268.5) node.remove();
                }
                if(node.localName==='path'&&(/^M50\.5/.test(node.getAttribute('d'))||node.getAttribute('fill')==='#6b9e7d')) node.remove();
                if(node.localName==='g'&&node.getAttribute('transform')==='translate(0 -26)') {
                    for(const child of [...node.children]) {
                        if((child.localName==='text'&&['3','2'].includes(child.textContent))||child.localName==='circle'||(child.localName==='path'&&child.getAttribute('d').startsWith('M34 228'))) child.remove();
                    }
                }
            }
            return root.innerHTML;
        });
        async function raster(name,body,w,h,{alpha=false,mask=false,tiles=false,emit=true,viewBox=`0 0 ${w} ${h}`,assetDefs=definitions}={}) {
            await page.setViewportSize({width:w,height:h});
            await page.setContent(`<style>html,body{margin:0;padding:0;background:transparent;overflow:hidden}svg{display:block}</style><svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="${viewBox}">${assetDefs}${body}</svg>`);
            await loadFont();
            const png=await page.screenshot({omitBackground:alpha});
            const {data}=await sharp(png).ensureAlpha().raw().toBuffer({resolveWithObject:true});
            const pixels=Buffer.alloc(w*h*(mask?1:alpha?4:2));
            for(let i=0;i<w*h;i++) {
                // A black mask over a white card preserves every grayscale edge.
                if(mask) pixels[i]=255-data[i*4];
                else if(alpha) { pixels[i*4]=data[i*4+2];pixels[i*4+1]=data[i*4+1];pixels[i*4+2]=data[i*4];pixels[i*4+3]=data[i*4+3]; }
                else { const rgb=((data[i*4]>>3)<<11)|((data[i*4+1]>>2)<<5)|(data[i*4+2]>>3);pixels.writeUInt16LE(rgb,i*2); }
            }
            if(tiles) homeTiles(pixels,w,h);
            else if(emit) image(name,pixels,w,h,alpha,mask);
            return pixels;
        }
        await raster('home','<rect width="172" height="320" fill="#080b0c"/>'+staticBody,172,320,{tiles:true});
        await page.setViewportSize({width:172,height:320});
        await page.setContent('<style>html,body{margin:0;padding:0;overflow:hidden}</style>'+bootSvg);
        await loadFont();
        const bootParts=await page.$eval('svg',root=>{
            const defs=root.querySelector('defs').outerHTML;
            const copy=root.cloneNode(true);
            for(const child of [...copy.children]) {
                if(['title','desc','defs'].includes(child.localName)||child.hasAttribute('data-boot-live'))child.remove();
            }
            return {defs,body:copy.innerHTML};
        });
        // Keep only the small color header and the dog's alpha mask in Flash.
        // The boot module draws the white card, black background and rules.
        await raster('ui_boot_header','<rect width="172" height="320" fill="#000"/>'+bootParts.body,78,17,
                     {viewBox:'12 8 78 17',assetDefs:bootParts.defs});
        await raster('ui_boot_dog',bootParts.body,100,48,
                     {mask:true,viewBox:'36 56 100 48',assetDefs:bootParts.defs});
        await raster('ui_key_rail','<rect width="172" height="320" fill="#080b0c"/><use href="#rail"/>',17,280,{viewBox:'155 20 17 280'});
        for(const [name,size,offset] of [['current',14,0],['candidate',11,.5]]) {
            for(const icon of ['gear','focus','eject','info']) await raster(`ui_icon_${icon}_${name}`,`<g color="#ffffff" transform="translate(0 ${offset}) scale(${size/14})"><use href="#ico-${icon}"/></g>`,size,size+(offset?1:0),{alpha:true});
        }
        await raster('ui_icon_up','<g color="#ffffff"><use href="#ico-up"/></g>',14,14,{alpha:true});
        await raster('ui_icon_down','<g color="#ffffff"><use href="#ico-down"/></g>',14,14,{alpha:true});
        await raster('ui_pointer_focus','<path d="M0.5 0.5h7l-3.5 6z" fill="#b1ffd3"/>',8,7,{alpha:true});
        await raster('ui_pointer_actual','<path d="M0.5 6.5h7l-3.5-6z" fill="#72c9ff"/>',8,7,{alpha:true});
        for(const n of [0,8]) {
            let body='<rect width="132" height="19" fill="#080b0c"/>';
            for(let i=0;i<8;i++) {
                const x=i*16.8,bright=i<n;
                body+=`<rect x="${x}" width="13.5" height="19" rx="0.8" fill="${bright?'#c3ecd4':'#294137'}" stroke="none"/><rect x="${x+1.5}" y="1.5" width="10.5" height="10.5" rx="0.3" fill="${bright?'#234c3b':'#101c17'}" stroke="none"/>`;
                if(bright) body+=`<path d="M${x+2.1} 11.2l3.2-3.8 2 1.8 3-4.1v6.1z" fill="#6b9e7d"/>`;
            }
            const pixels=await raster('film',body,132,19,{emit:false});
            const state=n?'lit':'dim';
            // Crop each original fractional position, deduplicating only exact
            // pixel matches. Browser rasterization can differ by one gray level
            // even when the mathematical fractional positions repeat.
            const unique=new Map(),frames=[];
            // Partition in the gaps, retaining a leading antialias pixel.
            const edges=Array.from({length:8},(_,i)=>Math.max(0,Math.floor(i*16.8)-1)).concat(132);
            for(let i=0;i<8;i++) {
                const width=edges[i+1]-edges[i],storedWidth=(width+1)&~1;
                const tile=cropRgb(pixels,132,edges[i],0,storedWidth,19);
                const key=width+':'+tile.toString('base64');
                let name=unique.get(key);
                if(!name) {
                    name=`ui_film_${state}_${unique.size}`;
                    unique.set(key,name);
                    image(name,tile,width,19,false,false,storedWidth*2);
                }
                frames.push(name);
            }
            chunks.push(`const lv_image_dsc_t * const ui_film_${state}[8] = {`+
                        frames.map(name=>`&${name}`).join(', ')+'};');
            declarations.push(`extern const lv_image_dsc_t * const ui_film_${state}[8];`);
        }
        for(const [name,size,weight,lineHeight,baseline,ascii,family] of [
            ['body',14,400,20,4,true,'Roboto Mono'],['small',10,400,16,4,true,'Roboto Mono'],
            ['distance',48,700,56,10,'-.0123456789','Space Mono'],
            ['distance_status',30,600,40,8,' -AEFINR','Roboto Mono'],['uptime',12,500,18,4,false,'Roboto Mono'],
            ['unit',16,500,22,5,'m','Roboto Mono'],
            ['count',14,600,20,4,false,'Roboto Mono'],
            ['boot_label',8,400,12,3,' ./:0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-','Roboto Mono'],
            ['boot_log',9,500,14,3,' .ABCDEFGHIJKLMNOPQRSTUVWXYZ[]','Roboto Mono'],
        ]) {
            const data=await page.evaluate(({size,weight,lineHeight,baseline,ascii,family,compactDecimal})=>{
                const canvas=document.createElement('canvas');canvas.width=96;canvas.height=80;
                const ctx=canvas.getContext('2d',{willReadFrequently:true});
                ctx.font=`${weight} ${size}px "${family}"`;ctx.textBaseline='alphabetic';ctx.fillStyle='#fff';
                const pixels=[],glyphs=[];
                const chars=typeof ascii==='string'?ascii:ascii?Array.from({length:95},(_,i)=>String.fromCharCode(i+32)).join(''):' -.0123456789:AEFINRm';
                for(const cp of [...new Set(chars)].map(c=>c.codePointAt(0)).sort((a,b)=>a-b)) {
                    const ch=String.fromCodePoint(cp);ctx.clearRect(0,0,96,80);ctx.fillText(ch,16,48);
                    const rgba=ctx.getImageData(0,0,96,80).data;
                    let x1=96,x2=-1,y1=80,y2=-1;
                    for(let y=0;y<80;y++)for(let x=0;x<96;x++)if(rgba[(y*96+x)*4+3]){x1=Math.min(x1,x);x2=Math.max(x2,x);y1=Math.min(y1,y);y2=Math.max(y2,y);}
                    const w=Math.max(0,x2-x1+1),h=Math.max(0,y2-y1+1),index=pixels.length;
                    for(let y=y1;y<=y2;y++)for(let x=x1;x<=x2;x++)pixels.push(rgba[(y*96+x)*4+3]);
                    const narrowDot=compactDecimal&&ch==='.';
                    const advance=narrowDot?12*16:compactDecimal?Math.round(ctx.measureText(ch).width)*16:Math.round(ctx.measureText(ch).width*16);
                    glyphs.push({cp,index,advance,w,h,x:narrowDot?Math.round((12-w)/2):w?x1-16:0,y:h?48-y2-1:0});
                }
                return {pixels,glyphs,lineHeight,baseline};
            },{size,weight,lineHeight,baseline,ascii,family,compactDecimal:name==='distance'});
            chunks.push(`/* ${name}: ${family}, ${size} px, weight ${weight}; SIL OFL 1.1. */`);
            font(`ui_font_${name}`,data);
        }
        fs.writeFileSync(path.join(output,'ui_home_assets.c'),chunks.join('\n\n')+'\n');
        fs.writeFileSync(path.join(output,'ui_home_assets.h'),'/* Generated from dual-mono design; font glyphs use SIL OFL 1.1. */\n#ifndef PM_UI_HOME_ASSETS_H\n#define PM_UI_HOME_ASSETS_H\n#include "lvgl.h"\n'+declarations.join('\n')+'\n#endif\n');
        console.log('Exported UI graphics and both OFL font glyphs:',hash);
    } finally {await browser.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
