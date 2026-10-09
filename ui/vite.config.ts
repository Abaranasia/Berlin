import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  base: './',
  plugins: [react()],
  server: { port: 5173, strictPort: true },
  test: {
    include: ['scripts/**/*.test.mjs', 'src/**/*.test.{ts,tsx}'],
  },
});
