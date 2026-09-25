import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import path from 'node:path';

export default defineConfig({
  plugins: [react()],
  define: { 'process.env.NODE_ENV': JSON.stringify('production') },
  resolve: {
    alias: {
      '@artemis-os/on-screen-keyboard': path.resolve(
        __dirname,
        '../../onscreen-keyboard/src/index.ts'
      ),
      react: path.resolve(__dirname, 'node_modules/react'),
      'react-dom': path.resolve(__dirname, 'node_modules/react-dom'),
    },
    dedupe: ['react', 'react-dom'],
  },
  build: {
    target: ['safari13'],
    emptyOutDir: false,
    outDir: '../plugin/webview',
    lib: {
      entry: path.resolve(__dirname, 'src/artemis-osk.tsx'),
      formats: ['iife'],
      name: 'Tone3000ArtemisOsk',
      fileName: () => 'artemis-osk.js',
      cssFileName: 'artemis-osk',
    },
  },
});
