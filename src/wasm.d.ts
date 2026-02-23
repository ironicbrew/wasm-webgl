export interface WebGLModule {
  _init_webgl: (width: number, height: number) => number
  _init_grid: (rows: number, cols: number) => number
  _render_grid: () => void
  _set_cell_color: (
    row: number,
    col: number,
    totalCols: number,
    r: number,
    g: number,
    b: number
  ) => void
  _update_grid_buffer: () => void
  _get_cell_at: (clipX: number, clipY: number) => number
  _set_cursor: (row: number, col: number, pos: number, visible: number) => void
  ccall: (
    ident: string,
    returnType: string | null,
    argTypes: string[],
    args: unknown[]
  ) => unknown
}

export type CreateWebGLModule = (
  overrides?: { locateFile?: (path: string) => string }
) => Promise<WebGLModule>

declare module '../wasm/webgl.js' {
  const createWebGLModule: CreateWebGLModule
  export default createWebGLModule
}
