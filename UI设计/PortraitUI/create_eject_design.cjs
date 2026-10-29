/* Maintain the approved EJECT SVG. Dynamic parts are drawn by LVGL on-board. */
const fs = require('fs');
const path = require('path');
const home = fs.readFileSync(path.join(__dirname, 'home.svg'), 'utf8');
const fonts = home.match(/@font-face\{[^}]*\}/g).join('\n');
// Independent header: keep the EJECT battery position and current brand.
const header = `<g>${['#f07d72','#efbd65','#d6e8c4','#81c7bb','#86b9dc'].map((c,i)=>`<rect x="${12+i*7}" y="13" width="6" height="5" fill="${c}"/>`).join('')}</g><g fill="none" stroke="#9eb7ab" stroke-width="0.9"><rect x="131" y="11" width="19" height="10" rx="1.7"/><path d="M152 14v4" stroke-width="1.7"/></g><rect x="133" y="13" width="9" height="6" rx="0.6" fill="#b7e6cb"/>`;
const point = (a, r=57) => {a*=Math.PI/180;return [78+r*Math.cos(a),165+r*Math.sin(a)];};
const arc = end => {const p=point(135),q=point(end);return `M${p[0]} ${p[1]}A57 57 0 ${end-135>180?1:0} 1 ${q[0]} ${q[1]}`;};
let ticks='';
for(let i=0;i<=30;i++){
    const p=point(135+9*i,65),q=point(135+9*i,i%5===0?69:67);
    ticks+=`<path d="M${p[0]} ${p[1]}L${q[0]} ${q[1]}" stroke="#10291b" stroke-opacity="${i%5===0?.6:.28}" stroke-width=".7"/>`;
}
const staticBody=`<text x="12" y="49" fill="#28673b" font-size="9" font-weight="500" letter-spacing="1.1">EJECT TIME</text>
<path d="M12 98H144" stroke="#10291b" stroke-opacity=".17" stroke-width=".6"/>
${ticks}
<text x="78" y="147" text-anchor="middle" fill="#28673b" font-size="9" letter-spacing="1.2">CURRENT</text>
<text x="78" y="202" text-anchor="middle" fill="#28673b" font-size="12" font-weight="500">A</text>
<text x="30" y="225" text-anchor="middle" fill="#28673b" font-size="8">0</text>
<text x="126" y="225" text-anchor="middle" fill="#28673b" font-size="8">1.5</text>`;
const tip=point(135+270*.620/1.5);
/* Pixel steps match the two exported Space Mono fonts, including a narrow dot. */
function digits(value, baseline, size, digitStep, dotStep){
    const step=c=>c==='.'?dotStep:digitStep;
    let x=(156-[...value].reduce((n,c)=>n+step(c),0))/2;
    return [...value].map(c=>{const cell=step(c),t=`<text x="${x+cell/2}" y="${baseline}" text-anchor="middle" fill="#10291b" font-family="Space Mono" font-size="${size}" font-weight="700">${c}</text>`;x+=cell;return t;}).join('');
}
const svg=`<svg xmlns="http://www.w3.org/2000/svg" width="172" height="320" viewBox="0 0 172 320">
<title>EJECT — independent green page, black status header</title>
<defs><style>text{font-family:'Roboto Mono'}${fonts}</style>
<g id="eject-paper"><rect width="42" height="50" rx="1.5" fill="#10291b"/><rect x="2" y="2" width="38" height="46" rx=".7" fill="#d5ffe1"/><rect x="6" y="6" width="30" height="28.5" rx=".4" fill="#28673b"/><circle cx="28" cy="13" r="3.5" fill="#8ceaa5"/><path d="M6 34.5V28.5L15 18L24 27L30 22.5L36 30V34.5Z" fill="#68c584"/></g>
<clipPath id="eject-paper-window"><rect x="43" y="246" width="70" height="67"/></clipPath>
</defs>
<rect width="172" height="320" fill="#47f56c"/>
<g data-eject-static="true">${staticBody}</g>
<g data-eject-live="true">
<path d="${arc(405)}" fill="none" stroke="#3bd95f" stroke-width="7" stroke-linecap="round"/>
<path d="${arc(135+270*.620/1.5)}" fill="none" stroke="#10291b" stroke-width="7" stroke-linecap="round"/>
<circle cx="${tip[0]}" cy="${tip[1]}" r="2" fill="#b9ffd0"/>
${digits('00:03.2',83,28,17,17)}${digits('.620',184,42,25,10)}
</g>
<rect x="155" y="31" width="17" height="289" fill="#10291b" fill-opacity=".047"/>
<path d="M157 84.5H170M157 236.5H170" stroke="#2c9e47"/>
<g data-eject-live="true"><rect x="159" y="155" width="10" height="10" rx="1" fill="none" stroke="#10291b" stroke-width="2"/>
</g>
<rect x="43" y="240" width="70" height="6" rx="3" fill="#10291b"/>
<g data-eject-live="true" clip-path="url(#eject-paper-window)"><use href="#eject-paper" transform="translate(57 263)"/></g>
<rect width="172" height="31" fill="#080b0c"/>${header}
</svg>`;
fs.writeFileSync(path.join(__dirname,'eject.svg'),svg);
