import { useEffect, useRef, useState, useCallback } from 'react'
import useWasm from '../hooks/useWasm'
import type { WebGLModule } from '../wasm'

const CANVAS_WIDTH = 1200
const CANVAS_HEIGHT = 800
const CURSOR_BLINK_MS = 530

function setCellText(mod: WebGLModule, row: number, col: number, text: string) {
  mod.ccall('set_cell_text', null, ['number', 'number', 'string'], [row, col, text])
}

function defaultCellColor(row: number): [number, number, number] {
  if (row === 0) return [0.0, 0.5, 0.7]
  return row % 2 === 0 ? [0.15, 0.15, 0.25] : [0.2, 0.2, 0.32]
}

export default function WebGLGrid() {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const [status, setStatus] = useState<'loading' | 'ready' | 'error'>('loading')
  const [module, setModule] = useState<WebGLModule | null>(null)
  const [gridRows, setGridRows] = useState(8)
  const [gridCols, setGridCols] = useState(5)
  const [stats, setStats] = useState('Click a cell or press arrow keys to navigate')
  const [updating, setUpdating] = useState(false)

  const priceDataRef = useRef<Record<string, number>>({})
  const cellDataRef = useRef<Record<string, string>>({})
  const updateIntervalRef = useRef<ReturnType<typeof setInterval> | null>(null)
  const updateCountRef = useRef(0)
  const lastUpdateTimeRef = useRef(0)

  const selRef = useRef({ row: -1, col: -1 })
  const editRef = useRef({ active: false, buffer: '', cursorPos: 0 })
  const blinkRef = useRef<ReturnType<typeof setInterval> | null>(null)
  const blinkOn = useRef(true)
  const moduleRef = useRef<WebGLModule | null>(null)
  const gridRef = useRef({ rows: 8, cols: 5 })

  moduleRef.current = module
  gridRef.current = { rows: gridRows, cols: gridCols }

  useWasm({ setModule, setStatus })

  const getCellValue = useCallback((row: number, col: number): string => {
    const key = `${row}-${col}`
    const edited = cellDataRef.current[key]
    if (edited !== undefined) return edited
    if (row === 0) return `Col ${col + 1}`
    return priceDataRef.current[key]?.toFixed(2) ?? ''
  }, [])

  const syncAllText = useCallback((mod: WebGLModule, prices: Record<string, number>, rows: number, cols: number) => {
    const cd = cellDataRef.current
    for (let row = 0; row < rows; row++) {
      for (let col = 0; col < cols; col++) {
        const key = `${row}-${col}`
        const edited = cd[key]
        const value =
          edited !== undefined
            ? edited
            : row === 0
              ? `Col ${col + 1}`
              : (prices[key] ?? 100 + Math.random() * 900).toFixed(2)
        setCellText(mod, row, col, value)
      }
    }
  }, [])

  const selectCell = useCallback((row: number, col: number) => {
    const mod = moduleRef.current
    if (!mod) return
    const { cols } = gridRef.current
    const prev = selRef.current

    if (editRef.current.active) {
      commitEdit()
    }

    if (prev.row >= 0 && prev.col >= 0) {
      const [r, g, b] = defaultCellColor(prev.row)
      mod._set_cell_color(prev.row, prev.col, cols, r, g, b)
    }

    selRef.current = { row, col }
    mod._set_cell_color(row, col, cols, 0.0, 1.0, 0.5)
    mod._update_grid_buffer()
    mod._set_cursor(row, col, 0, 0)
    mod._render_grid()
    setStats(`Selected: Row ${row}, Col ${col} — press Enter to edit, arrows to move`)
  }, [])

  const startEdit = useCallback(() => {
    const mod = moduleRef.current
    const { row, col } = selRef.current
    if (!mod || row < 0) return

    const value = getCellValue(row, col)
    editRef.current = { active: true, buffer: value, cursorPos: value.length }
    startBlink()
    setStats(`Editing [${row},${col}]: "${value}" — type to replace, Esc to cancel`)
  }, [getCellValue])

  const commitEdit = useCallback(() => {
    const mod = moduleRef.current
    const { row, col } = selRef.current
    if (!mod || row < 0) return

    const { buffer } = editRef.current
    const key = `${row}-${col}`
    cellDataRef.current = { ...cellDataRef.current, [key]: buffer }
    setCellText(mod, row, col, buffer)
    editRef.current = { active: false, buffer: '', cursorPos: 0 }
    stopBlink()
    mod._set_cursor(row, col, 0, 0)
    mod._render_grid()
    setStats(`Committed [${row},${col}] = "${buffer}"`)
  }, [])

  const cancelEdit = useCallback(() => {
    const mod = moduleRef.current
    const { row, col } = selRef.current
    if (!mod || row < 0) return

    const originalValue = getCellValue(row, col)
    setCellText(mod, row, col, originalValue)
    editRef.current = { active: false, buffer: '', cursorPos: 0 }
    stopBlink()
    mod._set_cursor(row, col, 0, 0)
    mod._render_grid()
    setStats(`Cancelled edit on [${row},${col}]`)
  }, [getCellValue])

  const startBlink = useCallback(() => {
    stopBlink()
    blinkOn.current = true
    blinkRef.current = setInterval(() => {
      blinkOn.current = !blinkOn.current
      const mod = moduleRef.current
      const { row, col } = selRef.current
      if (mod && row >= 0) {
        mod._set_cursor(row, col, editRef.current.cursorPos, blinkOn.current ? 1 : 0)
        mod._render_grid()
      }
    }, CURSOR_BLINK_MS)
  }, [])

  const stopBlink = useCallback(() => {
    if (blinkRef.current) {
      clearInterval(blinkRef.current)
      blinkRef.current = null
    }
  }, [])

  const resetBlink = useCallback(() => {
    blinkOn.current = true
    const mod = moduleRef.current
    const { row, col } = selRef.current
    if (mod && row >= 0) {
      mod._set_cursor(row, col, editRef.current.cursorPos, 1)
      mod._render_grid()
    }
    startBlink()
  }, [startBlink])

  // Init grid when module loads or grid size changes
  useEffect(() => {
    if (!module || !canvasRef.current) return

    const canvas = canvasRef.current
    const id = requestAnimationFrame(() => {
      if (!canvas.isConnected) return
      module._init_webgl(canvas.width, canvas.height)
      module._init_grid(gridRows, gridCols)

      const newPrices: Record<string, number> = {}
      for (let row = 1; row < gridRows; row++) {
        for (let col = 0; col < gridCols; col++) {
          const key = `${row}-${col}`
          if (!cellDataRef.current[key]) {
            newPrices[key] = 100 + Math.random() * 900
          }
        }
      }
      priceDataRef.current = newPrices

      syncAllText(module, newPrices, gridRows, gridCols)
      module._render_grid()

      selRef.current = { row: -1, col: -1 }
      editRef.current = { active: false, buffer: '', cursorPos: 0 }
      stopBlink()
      setStats(`${gridRows}×${gridCols} grid — click a cell or use arrow keys`)
      canvas.focus()
    })

    return () => {
      cancelAnimationFrame(id)
      if (updateIntervalRef.current) clearInterval(updateIntervalRef.current)
      stopBlink()
    }
  }, [module, gridRows, gridCols, syncAllText, stopBlink])

  const handleCanvasClick = useCallback((e: React.MouseEvent<HTMLCanvasElement>) => {
    const mod = moduleRef.current
    const canvas = canvasRef.current
    if (!mod || !canvas) return

    canvas.focus()

    const rect = canvas.getBoundingClientRect()
    const clipX = ((e.clientX - rect.left) / rect.width) * 2 - 1
    const clipY = 1 - ((e.clientY - rect.top) / rect.height) * 2

    const encoded = mod._get_cell_at(clipX, clipY)
    if (encoded < 0) return

    const row = Math.floor(encoded / 256)
    const col = encoded % 256

    if (selRef.current.row === row && selRef.current.col === col) {
      if (!editRef.current.active) startEdit()
      return
    }

    selectCell(row, col)
  }, [selectCell, startEdit])

  const handleKeyDown = useCallback((e: React.KeyboardEvent<HTMLCanvasElement>) => {
    const mod = moduleRef.current
    if (!mod) return

    const { rows, cols } = gridRef.current
    const { row, col } = selRef.current
    const editing = editRef.current.active

    if (!editing) {
      switch (e.key) {
        case 'ArrowUp':
        case 'ArrowDown':
        case 'ArrowLeft':
        case 'ArrowRight': {
          e.preventDefault()
          let nr = row < 0 ? 0 : row
          let nc = col < 0 ? 0 : col
          if (e.key === 'ArrowUp') nr = Math.max(0, nr - 1)
          if (e.key === 'ArrowDown') nr = Math.min(rows - 1, nr + 1)
          if (e.key === 'ArrowLeft') nc = Math.max(0, nc - 1)
          if (e.key === 'ArrowRight') nc = Math.min(cols - 1, nc + 1)
          selectCell(nr, nc)
          return
        }
        case 'Enter':
        case 'F2':
          e.preventDefault()
          if (row >= 0) startEdit()
          return
        case 'Delete': {
          e.preventDefault()
          if (row >= 0) {
            const key = `${row}-${col}`
            cellDataRef.current = { ...cellDataRef.current }
            delete cellDataRef.current[key]
            const original = row === 0 ? `Col ${col + 1}` : priceDataRef.current[key]?.toFixed(2) ?? ''
            setCellText(mod, row, col, original)
            mod._render_grid()
            setStats(`Cleared [${row},${col}]`)
          }
          return
        }
        case 'Tab': {
          e.preventDefault()
          if (row < 0) { selectCell(0, 0); return }
          const nc = e.shiftKey
            ? (col - 1 + cols) % cols
            : (col + 1) % cols
          const nr = e.shiftKey
            ? (nc === cols - 1 ? (row - 1 + rows) % rows : row)
            : (nc === 0 ? (row + 1) % rows : row)
          selectCell(nr, nc)
          return
        }
        default:
          if (e.key.length === 1 && !e.ctrlKey && !e.metaKey && !e.altKey && row >= 0) {
            editRef.current = { active: true, buffer: e.key, cursorPos: 1 }
            setCellText(mod, row, col, e.key)
            startBlink()
            mod._render_grid()
            setStats(`Editing [${row},${col}]: "${e.key}"`)
            e.preventDefault()
          }
          return
      }
    }

    // --- Editing mode ---
    const { buffer, cursorPos } = editRef.current

    switch (e.key) {
      case 'Enter':
        e.preventDefault()
        commitEdit()
        return
      case 'Escape':
        e.preventDefault()
        cancelEdit()
        return
      case 'Backspace': {
        e.preventDefault()
        if (cursorPos > 0) {
          const next = buffer.slice(0, cursorPos - 1) + buffer.slice(cursorPos)
          editRef.current.buffer = next
          editRef.current.cursorPos = cursorPos - 1
          setCellText(mod, row, col, next)
          resetBlink()
          setStats(`Editing [${row},${col}]: "${next}"`)
        }
        return
      }
      case 'Delete': {
        e.preventDefault()
        if (cursorPos < buffer.length) {
          const next = buffer.slice(0, cursorPos) + buffer.slice(cursorPos + 1)
          editRef.current.buffer = next
          setCellText(mod, row, col, next)
          resetBlink()
          setStats(`Editing [${row},${col}]: "${next}"`)
        }
        return
      }
      case 'ArrowLeft':
        e.preventDefault()
        if (cursorPos > 0) {
          editRef.current.cursorPos = cursorPos - 1
          resetBlink()
        }
        return
      case 'ArrowRight':
        e.preventDefault()
        if (cursorPos < buffer.length) {
          editRef.current.cursorPos = cursorPos + 1
          resetBlink()
        }
        return
      case 'Home':
        e.preventDefault()
        editRef.current.cursorPos = 0
        resetBlink()
        return
      case 'End':
        e.preventDefault()
        editRef.current.cursorPos = buffer.length
        resetBlink()
        return
      case 'Tab': {
        e.preventDefault()
        commitEdit()
        const nc = e.shiftKey
          ? (col - 1 + cols) % cols
          : (col + 1) % cols
        const nr = e.shiftKey
          ? (nc === cols - 1 ? (row - 1 + rows) % rows : row)
          : (nc === 0 ? (row + 1) % rows : row)
        selectCell(nr, nc)
        return
      }
      default:
        if (e.key.length === 1 && !e.ctrlKey && !e.metaKey && !e.altKey) {
          e.preventDefault()
          const next = buffer.slice(0, cursorPos) + e.key + buffer.slice(cursorPos)
          editRef.current.buffer = next
          editRef.current.cursorPos = cursorPos + 1
          setCellText(mod, row, col, next)
          resetBlink()
          setStats(`Editing [${row},${col}]: "${next}"`)
        }
        return
    }
  }, [selectCell, startEdit, commitEdit, cancelEdit, resetBlink])

  // Live price updates
  const toggleUpdates = () => {
    if (updating) {
      if (updateIntervalRef.current) clearInterval(updateIntervalRef.current)
      setUpdating(false)
    } else {
      updateCountRef.current = 0
      lastUpdateTimeRef.current = performance.now()
      updateIntervalRef.current = setInterval(updatePrices, 333)
      setUpdating(true)
    }
  }

  const updatePrices = () => {
    const mod = moduleRef.current
    if (!mod) return
    const { rows, cols } = gridRef.current
    const startTime = performance.now()
    let cellsUpdated = 0
    const cd = cellDataRef.current
    const pd = priceDataRef.current

    const editing = editRef.current.active
    const editRow = selRef.current.row
    const editCol = selRef.current.col

    for (let row = 1; row < rows; row++) {
      for (let col = 0; col < cols; col++) {
        const key = `${row}-${col}`
        if (cd[key] !== undefined) continue
        if (editing && row === editRow && col === editCol) continue

        const oldPrice = pd[key] || 500
        const change = (Math.random() - 0.5) * 0.04 * oldPrice
        const newPrice = Math.max(0.01, oldPrice + change)
        pd[key] = newPrice

        setCellText(mod, row, col, newPrice.toFixed(2))
        cellsUpdated++
      }
    }

    priceDataRef.current = pd
    mod._render_grid()

    updateCountRef.current++
    const elapsed = performance.now() - startTime
    const totalTime = performance.now() - lastUpdateTimeRef.current
    const updatesPerSec = (updateCountRef.current / (totalTime / 1000)).toFixed(1)
    setStats(
      `Updated ${cellsUpdated} cells in ${elapsed.toFixed(1)}ms | ${updatesPerSec} updates/sec`
    )
  }

  if (status === 'loading') {
    return (
      <div style={{ padding: '1rem', borderRadius: 4, background: '#ff9f1c33', color: '#ff9f1c' }}>
        Loading WebAssembly module...
      </div>
    )
  }

  if (status === 'error') {
    return (
      <div style={{ padding: '1rem', borderRadius: 4, background: '#ff6b6b33', color: '#ff6b6b' }}>
        Failed to load WebAssembly module. Run <code>pnpm run build:wasm</code> to compile.
      </div>
    )
  }

  const buttonStyle: React.CSSProperties = {
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
    <div style={{ background: '#16213e', borderRadius: 8, padding: '1.5rem', margin: '1rem 0' }}>
      <p>
        Grid backgrounds + text + cursor are all rendered via WebGL shaders.
        Click a cell to select, click again or press Enter to edit.
        Arrow keys navigate, Tab moves between cells.
      </p>
      <canvas
        ref={canvasRef}
        id="grid-canvas"
        width={CANVAS_WIDTH}
        height={CANVAS_HEIGHT}
        tabIndex={0}
        style={{
          border: '2px solid #00d4ff',
          borderRadius: 4,
          display: 'block',
          margin: '1rem 0',
          cursor: 'pointer',
          outline: 'none',
        }}
        onClick={handleCanvasClick}
        onKeyDown={handleKeyDown}
      />
      <div style={{ margin: '1rem 0' }}>
        <button onClick={() => { setGridRows(8); setGridCols(5) }} style={buttonStyle}>8×5</button>
        <button onClick={() => { setGridRows(20); setGridCols(8) }} style={buttonStyle}>20×8</button>
        <button onClick={() => { setGridRows(50); setGridCols(10) }} style={buttonStyle}>50×10</button>
        <button onClick={() => { setGridRows(100); setGridCols(15) }} style={buttonStyle}>100×15</button>
        <span style={{ marginLeft: '1rem' }} />
        <button
          onClick={toggleUpdates}
          style={{ ...buttonStyle, background: updating ? '#00ff88' : undefined }}
        >
          {updating ? '⏹ Stop Updates' : '▶ Start Updates'}
        </button>
      </div>
      <div style={{
        fontFamily: 'monospace',
        color: '#00ff88',
        marginTop: '0.5rem',
        fontSize: '0.85rem',
        lineHeight: 1.8,
      }}>
        <div>{stats}</div>
        <div style={{ color: '#778899', fontSize: '0.75rem' }}>
          Arrows: move | Enter/F2: edit | Esc: cancel | Tab: next cell | Delete: clear | Type: overwrite
        </div>
      </div>
    </div>
  )
}
