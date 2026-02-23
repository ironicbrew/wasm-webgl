import WebGLGrid from './components/WebGLGrid'

function App() {
  return (
    <div style={{ maxWidth: 1000, margin: '0 auto' }}>
      <h1 style={{ color: '#00d4ff', borderBottom: '2px solid #00d4ff', paddingBottom: '0.5rem' }}>
        WebGL + WASM Interactive Grid
      </h1>
      <WebGLGrid />
    </div>
  )
}

export default App
