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

// Noms de fichiers fixes et chemins relatifs : l'interface est embarquée dans le plugin
// (juce_add_binary_data) et servie par le fournisseur de ressources de JUCE.
export default defineConfig({
  plugins: [react(), asciiCss()],
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
