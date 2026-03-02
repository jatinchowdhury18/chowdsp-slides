import { mathjax } from 'mathjax-full/js/mathjax.js';
import { TeX } from 'mathjax-full/js/input/tex.js';
import { SVG } from 'mathjax-full/js/output/svg.js';
import { liteAdaptor } from 'mathjax-full/js/adaptors/liteAdaptor.js';
import { RegisterHTMLHandler } from 'mathjax-full/js/handlers/html.js';

// Create adaptor (virtual DOM)
const adaptor = liteAdaptor();
RegisterHTMLHandler(adaptor);

// Configure TeX input
const tex = new TeX({
  packages: ['base']   // keep minimal; add more if needed
});

// Configure SVG output
const svg = new SVG({
  fontCache: 'none'    // IMPORTANT: standalone SVG
});

// Create MathJax document
const html = mathjax.document('', {
  InputJax: tex,
  OutputJax: svg
});

// 🔴 IMPORTANT: Force synchronous initialization
html.updateDocument();

// Expose global render function
globalThis.renderLatexToSVG = function (latex) {
  try {
    const node = html.convert(latex, { display: true });
    return adaptor.outerHTML(node);
  } catch (err) {
    return `<svg xmlns="http://www.w3.org/2000/svg">
              <text x="10" y="20" fill="red">${err.toString()}</text>
            </svg>`;
  }
};

