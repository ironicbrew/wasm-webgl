import { useEffect } from 'react'
import type { WebGLModule } from '../wasm'

type UseWasmOptions = {
  setModule: (mod: WebGLModule | null) => void
  setStatus: (status: 'loading' | 'ready' | 'error') => void
}

export default function useWasm({ setModule, setStatus }: UseWasmOptions) {
  useEffect(() => {
    let cancelled = false
    import('../wasm/webgl.js')
      .then(({ default: createWebGLModule }) =>
        createWebGLModule({
          locateFile: () => new URL('../wasm/webgl.wasm', import.meta.url).href,
        })
      )
      .then((mod) => {
        if (!cancelled) {
          setModule(mod)
          setStatus('ready')
        }
      })
      .catch((err) => {
        if (!cancelled) setStatus('error')
        console.error('WASM load failed:', err)
      })
    return () => {
      cancelled = true
    }
  }, [])
}
