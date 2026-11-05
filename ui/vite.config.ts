// Groovebox — OFAD — AGPL-3.0-or-later
import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import type { Plugin } from "vite";

// La WebView ne garantit pas l'encodage des feuilles de style servies par le plugin :
// tout caractère non ASCII du CSS final est écrit en échappement CSS (\B7 , \E9 ...).
function asciiCss(): Plugin {
  return {
    name: "groovebox-ascii-css",
    generateBundle(_options, bundle) {
      for (const file of Object.values(bundle)) {
        if (file.type === "asset" && file.fileName.endsWith(".css") && typeof file.source === "string") {
          file.source = file.source.replace(/[^\x00-\x7f]/gu, (c) => `\\${c.codePointAt(0)!.toString(16)} `);
        }
      }
    },
  };
}

// v0.46.1 (audit F7) : politique de sécurité du contenu de la page embarquée (version compilée
// seulement : le serveur de développement de Vite a besoin de scripts en ligne). Aucun script venu
// d'ailleurs que l'interface elle-même, aucun objet, aucun cadre, aucune connexion extérieure.
// 'unsafe-eval' : exigé par JUCE sous Android (check_native_interop.js évalue ses scripts
// d'intégration) ; 'unsafe-inline' pour les styles : positions et couleurs calculées par React.
const CSP = [
  "default-src 'self'",
  "script-src 'self' 'unsafe-eval'",
  "style-src 'self' 'unsafe-inline'",
  "img-src 'self' data: blob:",
  "font-src 'self'",
  "connect-src 'self'",
  "media-src 'self' blob:",
  "object-src 'none'",
  "frame-src 'none'",
  "base-uri 'none'",
  "form-action 'none'",
].join("; ");

function contentSecurityPolicy(): Plugin {
  return {
    name: "groovebox-csp",
    apply: "build",
    transformIndexHtml(html) {
      return html.replace('<meta charset="UTF-8" />', `<meta charset="UTF-8" />\n    <meta http-equiv="Content-Security-Policy" content="${CSP}" />`);
    },
  };
}

// Noms de fichiers fixes et chemins relatifs : l'interface est embarquée dans le plugin
// (juce_add_binary_data) et servie par le fournisseur de ressources de JUCE.
export default defineConfig({
  plugins: [react(), asciiCss(), contentSecurityPolicy()],
  base: "./",
  build: {
    outDir: "dist",
    emptyOutDir: true,
    assetsInlineLimit: 0,
    sourcemap: false,
    rollupOptions: {
      output: {
        entryFileNames: "assets/app.js",
        chunkFileNames: "assets/[name].js",
        assetFileNames: "assets/[name][extname]",
      },
    },
  },
  server: { port: 5173, strictPort: true },
});
