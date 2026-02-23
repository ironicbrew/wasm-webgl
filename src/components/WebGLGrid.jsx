import { useEffect, useRef, useState, useLayoutEffect } from 'react'

const CANVAS_WIDTH = 700
const CANVAS_HEIGHT = 400

export default function WebGLGrid() {
  const canvasRef = useRef(null)
  const [status, setStatus] = useState('loading')
  const [module, setModule] = useState(null)
  const [gridRows, setGridRows] = useState(8)
  const [gridCols, setGridCols] = useState(5)
  const [selectedRow, setSelectedRow] = useState(-1)
  const [selectedCol, setSelectedCol] = useState(-1)
  const [cellData, setCellData] = useState({})
  const [priceData, setPriceData] = useState({})
  const [stats, setStats] = useState('Click a cell to select it')
  const [updating, setUpdating] = useState(false)

  const priceDataRef = useRef({})
  const cellDataRef = useRef({})
  const cellElementsRef = useRef([])
  const updateIntervalRef = useRef(null)
  const updateCountRef = useRef(0)
  const lastUpdateTimeRef = useRef(0)

  cellDataRef.current = cellData
  priceDataRef.current = priceData

  // Load WASM module (only once - Emscripten globals conflict if loaded twice)
  useEffect(() => {
    const onReady = () => {
      setModule(window.Module)
      setStatus('ready')
    }

    if (window.Module?._init_webgl) {
      onReady()
      return
    }

    window.Module = {
      onRuntimeInitialized: onReady,
    }

    const script = document.createElement('script')
    script.src = '/build/webgl.js'
    script.async = true
    document.body.appendChild(script)
    // Don't remove script on unmount - causes "already declared" errors on remount
  }, [])

  // Initialize grid when module is ready or grid size changes
  useEffect(() => {
    if (!module || !canvasRef.current) return

    const canvas = canvasRef.current
    // Defer one frame so canvas is fully in DOM and ready for WebGL
    const id = requestAnimationFrame(() => {
      if (!canvas.isConnected) return
      module._init_webgl(canvas.width, canvas.height)
      module._init_grid(gridRows, gridCols)
      module._render_grid()

      const newPriceData = {}
      for (let row = 1; row < gridRows; row++) {
        for (let col = 0; col < gridCols; col++) {
          const key = `${row}-${col}`
          if (!cellDataRef.current[key]) {
            newPriceData[key] = 100 + Math.random() * 900
          }
        }
      }
      setPriceData(newPriceData)
      priceDataRef.current = newPriceData
      setStats(`${gridRows}×${gridCols} grid — Click "Start Updates" to simulate live data`)
    })

    return () => {
      cancelAnimationFrame(id)
      if (updateIntervalRef.current) {
        clearInterval(updateIntervalRef.current)
      }
    }
  }, [module, gridRows, gridCols])

  const selectCell = (row, col, element) => {
    if (!module) return

    if (selectedRow >= 0) {
      const prevRow = selectedRow
      const prevCol = selectedCol
      if (prevRow === 0) {
        module._set_cell_color(prevRow, prevCol, gridCols, 0.0, 0.5, 0.7)
      } else if (prevRow % 2 === 0) {
        module._set_cell_color(prevRow, prevCol, gridCols, 0.15, 0.15, 0.25)
      } else {
        module._set_cell_color(prevRow, prevCol, gridCols, 0.2, 0.2, 0.32)
      }
    }

    setSelectedRow(row)
    setSelectedCol(col)
    module._set_cell_color(row, col, gridCols, 0.0, 1.0, 0.5)
    module._update_grid_buffer()
    module._render_grid()

    element.contentEditable = true
    element.focus()

    const range = document.createRange()
    range.selectNodeContents(element)
    const sel = window.getSelection()
    sel.removeAllRanges()
    sel.addRange(range)

    element.onblur = () => {
      element.contentEditable = false
      const key = `${row}-${col}`
      const value = element.textContent
      setCellData((prev) => ({ ...prev, [key]: value }))
      cellDataRef.current = { ...cellDataRef.current, [key]: value }
    }

    element.onkeydown = (e) => {
      if (e.key === 'Enter') {
        e.preventDefault()
        element.blur()
      }
    }

    setStats(`Selected: Row ${row}, Col ${col} — Edit the value!`)
  }

  const toggleUpdates = () => {
    if (updating) {
      clearInterval(updateIntervalRef.current)
      setUpdating(false)
    } else {
      updateCountRef.current = 0
      lastUpdateTimeRef.current = performance.now()
      updateIntervalRef.current = setInterval(updatePrices, 333)
      setUpdating(true)
    }
  }

  const updatePrices = () => {
    const startTime = performance.now()
    let cellsUpdated = 0
    const cellData = cellDataRef.current
    const priceData = priceDataRef.current

    for (const cell of cellElementsRef.current.filter(Boolean)) {
      const { element, row, col, key } = cell
      if (row === 0) continue
      if (cellData[key] !== undefined) {
        element.style.color = '#ffff00'
        continue
      }

      const oldPrice = priceData[key] || 500
      const change = (Math.random() - 0.5) * 0.04 * oldPrice
      const newPrice = Math.max(0.01, oldPrice + change)
      priceData[key] = newPrice

      element.textContent = newPrice.toFixed(2)
      element.style.color = change > 0 ? '#00ff88' : '#ff6b6b'
      cellsUpdated++
    }

    updateCountRef.current++
    const elapsed = performance.now() - startTime
    const totalTime = performance.now() - lastUpdateTimeRef.current
    const updatesPerSec = (updateCountRef.current / (totalTime / 1000)).toFixed(1)
    setStats(
      `Updated ${cellsUpdated} cells in ${elapsed.toFixed(1)}ms | ${updatesPerSec} updates/sec | Edited cells stay fixed (yellow)`
    )
  }

  const initGrid = (rows, cols) => {
    setGridRows(rows)
    setGridCols(cols)
  }

  const cellWidth = CANVAS_WIDTH / gridCols
  const cellHeight = CANVAS_HEIGHT / gridRows
  const padding = 4

  const overlayCells = []
  for (let row = 0; row < gridRows; row++) {
    for (let col = 0; col < gridCols; col++) {
      const key = `${row}-${col}`
      let value = cellData[key]
      if (value === undefined) {
        value =
          row === 0
            ? `Col ${col + 1}`
            : (priceData[key] ?? 100 + Math.random() * 900).toFixed(2)
      }

      overlayCells.push(
        <div
          key={key}
          ref={(el) => {
            if (el) {
              const idx = row * gridCols + col
              cellElementsRef.current[idx] = { element: el, row, col, key }
            }
          }}
          style={{
            position: 'absolute',
            left: col * cellWidth + padding,
            top: row * cellHeight + padding,
            width: cellWidth - padding * 2,
            height: cellHeight - padding * 2,
            lineHeight: cellHeight - padding * 2 + 'px',
            color: '#fff',
            fontSize: 12,
            fontFamily: 'monospace',
            textAlign: 'center',
            cursor: 'pointer',
          }}
          onClick={(e) => selectCell(row, col, e.currentTarget)}
        >
          {value}
        </div>
      )
    }
  }

  useLayoutEffect(() => {
    cellElementsRef.current = cellElementsRef.current.slice(0, gridRows * gridCols).filter(Boolean)
  }, [gridRows, gridCols])

  if (status === 'loading') {
    return (
      <div
        style={{
          padding: '1rem',
          borderRadius: 4,
          marginBottom: '1rem',
          background: '#ff9f1c33',
          color: '#ff9f1c',
        }}
      >
        Loading WebAssembly module...
      </div>
    )
  }

  const buttonStyle = {
    background: '#00d4ff',
    color: '#1a1a2e',
    border: 'none',
    padding: '0.5rem 1rem',
    borderRadius: 4,
    cursor: 'pointer',
    fontWeight: 'bold',
    margin: '0.25rem',
  }

  return (
    <div
      style={{
        background: '#16213e',
        borderRadius: 8,
        padding: '1.5rem',
        margin: '1rem 0',
      }}
    >
      <p>Click cells to highlight them. GPU-rendered foundation for your data table!</p>
      <div style={{ position: 'relative', display: 'inline-block' }}>
        <canvas
          ref={canvasRef}
          id="grid-canvas"
          width={CANVAS_WIDTH}
          height={CANVAS_HEIGHT}
          style={{
            border: '2px solid #00d4ff',
            borderRadius: 4,
            display: 'block',
            margin: '1rem 0',
          }}
        />
        <div
          style={{
            position: 'absolute',
            top: 0,
            left: 0,
            width: CANVAS_WIDTH,
            height: CANVAS_HEIGHT,
            pointerEvents: 'none',
          }}
        >
          <div style={{ pointerEvents: 'auto', position: 'relative', width: '100%', height: '100%' }}>
            {overlayCells}
          </div>
        </div>
      </div>
      <div style={{ margin: '1rem 0' }}>
        <button onClick={() => initGrid(8, 5)} style={buttonStyle}>
          8×5 Grid
        </button>
        <button onClick={() => initGrid(20, 8)} style={buttonStyle}>
          20×8 Grid
        </button>
        <button onClick={() => initGrid(50, 10)} style={buttonStyle}>
          50×10 Grid
        </button>
        <button onClick={() => initGrid(100, 15)} style={buttonStyle}>
          100×15 Grid
        </button>
        <span style={{ marginLeft: '1rem' }} />
        <button
          onClick={toggleUpdates}
          style={{ ...buttonStyle, background: updating ? '#00ff88' : undefined }}
        >
          {updating ? '⏹ Stop Updates' : '▶ Start Updates'}
        </button>
      </div>
      <div style={{ fontFamily: 'monospace', color: '#00ff88', marginTop: '0.5rem' }}>
        {stats}
      </div>
    </div>
  )
}
