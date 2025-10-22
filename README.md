# Fractal Curve Generation (OpenGL)

A small, self-contained C++/OpenGL tool for drawing line segments, grouping them into polygons, applying recursive fractal transforms (Quadratic Type-2 Koch, Heighway Dragon), and exporting exactly what you see on the canvas to PNG. Scenes can be saved/loaded as JSON.

<img width="1919" height="1079" alt="Ekran görüntüsü 2025-10-22 190902" src="https://github.com/user-attachments/assets/8aa47a85-4e2d-4fee-b703-a9924da01c2e" />

## Features

- **Drawing**
  - Line tool (click–drag–release).
  - Polygon tool (chained edges with snap-to-first for closure).
  - Regular polygon tool (click to set center, drag to set radius); created as a single undoable action.

- **Selection & Editing**
  - Click selects; **Ctrl+Click** toggles.
  - Drag endpoints to edit; drag in the middle to move all selected lines.
  - **Regular polys:** clicking the cyan center selects the whole shape; clicking an edge only selects that edge.
  - **Arbitrary polys:** clicking an edge only selects that edge.

- **Transforms**
  - Quadratic Type-2 Koch and Heighway Dragon (iterative).
  - Apply per selected line(s); cached until endpoints change.

- **Styling**
  - Per-line color and thickness (thick lines rendered as quads, not “GL line width”).

- **Export & Saves**
  - **PNG export** mirrors the canvas view using an off-screen framebuffer (no corner squashing).
  - **Scene save/load** as JSON.
  - Separate folders: `output/images/` for PNGs, `output/saves/` for JSON.

- **Undo/Redo**
  - Command-based history (per drag / per operation).

---

## Build (Windows, Visual Studio 2022)

1. Install **Visual Studio 2022** with “Desktop development with C++”.
2. Open the solution: `Fractal-Curve-Generation.sln`.
3. Select **x64** and **Debug** or **Release**.
4. Build & Run.

All third-party dependencies are vendored in the **`external/`** directory and are already wired up in the solution. Opening and building the solution in VS2022 is sufficient—no extra setup.

### Dependencies (vendored under `external/`)
- **GLFW** (windowing, input)
- **GLAD** (OpenGL loader)
- **ImGui** (+ GLFW/OpenGL3 backends)
- **GLM** (math)
- **nlohmann/json** (scene I/O)
- **stb_image_write** (PNG output)

The app creates an **OpenGL 3.3 Core** context (`#version 330`).

---

## Run-time Usage

### Canvas controls
- **LMB**: select / drag endpoints / drag middle to move selection
- **Ctrl+LMB**: toggle selection
- **MMB drag**: pan
- **Mouse wheel**: zoom about cursor

### Tools (ImGui tabs)
- **Select/Move**: selection, edit, and quick style; regular polygon group parameters (center, radius, rotation).
- **Create**:
  - **Line**: click–drag–release.
  - **Poly**: chained edges; snap to first point (10px) to close and form a group.
  - **Regular Poly**: click = center, drag = radius; creates N edges + group as one undo step.
- **Style**: apply color/thickness to selection.
- **Transforms**: set Koch-Type2 and Dragon iteration counts for the selection.
- **Canvas**: zoom and center readouts, Undo/Redo.
- **Export & Saves**:
  - **PNG**: writes to `output/images/<base>.png` (directory is created if missing).
  - **State JSON**: save/load `output/saves/<base>.json`.

> The base filename is shared for convenience; paths are separate (`images/` vs `saves/`).

---
