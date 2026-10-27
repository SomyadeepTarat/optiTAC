import { nativeCompiler } from './server/native.ts'
import react from '@vitejs/plugin-react'
import { defineConfig } from 'vite'

// https://vite.dev/config/
export default defineConfig({
  plugins: [react(), nativeCompiler()],
})
