#!/usr/bin/env python3
"""
Molecular HTML Viewer Generator
Automatically creates and launches interactive 3D molecular viewers
"""

import sys
import os
import webbrowser
import subprocess
from pathlib import Path
import json

class MolecularViewer:
    """Auto-generates interactive HTML viewers for molecular structures"""
    
    # Element colors (CPK coloring scheme)
    ELEMENT_COLORS = {
        'H': '#FFFFFF', 'C': '#909090', 'N': '#3050F8', 'O': '#FF0D0D',
        'F': '#90E050', 'Cl': '#1FF01F', 'Br': '#A62929', 'I': '#940094',
        'S': '#FFFF30', 'P': '#FF8000', 'B': '#FFB5B5', 'Si': '#F0C8A0',
        'Xe': '#429EB0', 'Kr': '#5CB8D1', 'Ar': '#80D1E3', 'Ne': '#B3E3F5',
        'He': '#D9FFFF', 'Ca': '#3DFF00', 'Mg': '#8AFF00', 'Na': '#AB5CF2',
        'K': '#8F40D4', 'Fe': '#E06633', 'Cu': '#C88033', 'Zn': '#7D80B0',
        'Al': '#BFA6A6', 'Li': '#CC80FF', 'Be': '#C2FF00'
    }
    
    # Van der Waals radii (Å)
    VDW_RADII = {
        'H': 1.20, 'C': 1.70, 'N': 1.55, 'O': 1.52, 'F': 1.47, 'Cl': 1.75,
        'Br': 1.85, 'I': 1.98, 'S': 1.80, 'P': 1.80, 'B': 1.92, 'Si': 2.10,
        'Xe': 2.16, 'Kr': 2.02, 'Ar': 1.88, 'Ne': 1.54, 'He': 1.40,
        'Ca': 2.31, 'Mg': 1.73, 'Na': 2.27, 'K': 2.75, 'Fe': 2.00,
        'Cu': 1.40, 'Zn': 1.39, 'Al': 1.84, 'Li': 1.82, 'Be': 1.53
    }
    
    def __init__(self, xyz_file):
        self.xyz_file = Path(xyz_file)
        self.atoms = []
        self.formula = ""
        self.comment = ""
        self.parse_xyz()
        
    def parse_xyz(self):
        """Parse XYZ file format"""
        with open(self.xyz_file, 'r') as f:
            lines = f.readlines()
            
        num_atoms = int(lines[0].strip())
        self.comment = lines[1].strip()
        
        # Extract formula from comment if available
        if '-' in self.comment:
            self.formula = self.comment.split('-')[0].strip()
        else:
            self.formula = self.comment.strip()
        
        for i in range(2, 2 + num_atoms):
            parts = lines[i].split()
            element = parts[0]
            x, y, z = float(parts[1]), float(parts[2]), float(parts[3])
            
            color = self.ELEMENT_COLORS.get(element, '#FF1493')  # Default: hot pink
            radius = self.VDW_RADII.get(element, 1.70)
            
            self.atoms.append({
                'element': element,
                'x': x, 'y': y, 'z': z,
                'color': color,
                'radius': radius
            })
    
    def detect_bonds(self):
        """Detect bonds based on distance (1.6x covalent radii threshold)"""
        bonds = []
        for i, atom1 in enumerate(self.atoms):
            for j, atom2 in enumerate(self.atoms):
                if i >= j:
                    continue
                    
                dx = atom1['x'] - atom2['x']
                dy = atom1['y'] - atom2['y']
                dz = atom1['z'] - atom2['z']
                dist = (dx*dx + dy*dy + dz*dz) ** 0.5
                
                # Bond threshold: 1.6x sum of VDW radii
                r1 = atom1['radius']
                r2 = atom2['radius']
                threshold = 1.6 * (r1 + r2) / 2
                
                if dist < threshold:
                    bonds.append([i, j])
        
        return bonds
    
    def generate_html(self, output_file=None):
        """Generate interactive HTML viewer"""
        if output_file is None:
            output_file = self.xyz_file.stem + '_viewer.html'
        
        bonds = self.detect_bonds()
        
        # Convert atoms to JavaScript
        atoms_js = json.dumps(self.atoms, indent=12)
        bonds_js = json.dumps(bonds, indent=12)
        
        html_template = f'''<!DOCTYPE html>
<html>
<head>
    <title>{self.formula} - VSEPR-Sim Viewer</title>
    <meta charset="utf-8">
    <style>
        body {{ margin: 0; overflow: hidden; font-family: 'Segoe UI', Arial, sans-serif; background: #0a0a1a; color: #eee; }}
        #info {{ position: absolute; top: 10px; left: 10px; background: rgba(0,0,0,0.85); padding: 20px; border-radius: 8px; max-width: 350px; border: 1px solid #333; box-shadow: 0 4px 6px rgba(0,0,0,0.5); }}
        #info h2 {{ margin: 0 0 15px 0; color: #4CAF50; font-size: 24px; border-bottom: 2px solid #4CAF50; padding-bottom: 8px; }}
        #info p {{ margin: 8px 0; font-size: 14px; line-height: 1.6; }}
        .label {{ color: #FFD700; font-weight: bold; display: inline-block; width: 100px; }}
        .value {{ color: #FFFFFF; }}
        .controls {{ margin-top: 15px; padding-top: 15px; border-top: 1px solid #444; font-size: 12px; color: #888; }}
        .controls kbd {{ background: #333; padding: 2px 6px; border-radius: 3px; font-family: monospace; color: #aaa; }}
        #stats {{ position: absolute; bottom: 10px; left: 10px; background: rgba(0,0,0,0.7); padding: 10px; border-radius: 5px; font-size: 12px; }}
        .watermark {{ position: absolute; bottom: 10px; right: 10px; color: #444; font-size: 11px; }}
    </style>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/three@0.128.0/examples/js/controls/OrbitControls.js"></script>
</head>
<body>
    <div id="info">
        <h2>{self.formula}</h2>
        <p><span class="label">Formula:</span> <span class="value">{self.formula}</span></p>
        <p><span class="label">Atoms:</span> <span class="value">{len(self.atoms)}</span></p>
        <p><span class="label">Bonds:</span> <span class="value">{len(bonds)}</span></p>
        <p><span class="label">Source:</span> <span class="value">{self.xyz_file.name}</span></p>
        
        <div class="controls">
            <p><kbd>Left Drag</kbd> Rotate • <kbd>Right Drag</kbd> Pan • <kbd>Scroll</kbd> Zoom</p>
            <p><kbd>R</kbd> Reset View • <kbd>F</kbd> Toggle Fullscreen</p>
        </div>
    </div>
    
    <div id="stats">FPS: <span id="fps">--</span></div>
    <div class="watermark">VSEPR-Sim Molecular Viewer • Generated from {self.xyz_file.name}</div>
    
    <script>
        const atoms = {atoms_js};
        const bonds = {bonds_js};
        
        // Scene setup
        const scene = new THREE.Scene();
        scene.background = new THREE.Color(0x0a0a1a);
        scene.fog = new THREE.Fog(0x0a0a1a, 10, 50);
        
        const camera = new THREE.PerspectiveCamera(60, window.innerWidth / window.innerHeight, 0.1, 1000);
        camera.position.set(5, 5, 8);
        camera.lookAt(0, 0, 0);
        
        const renderer = new THREE.WebGLRenderer({{ antialias: true }});
        renderer.setSize(window.innerWidth, window.innerHeight);
        renderer.setPixelRatio(window.devicePixelRatio);
        document.body.appendChild(renderer.domElement);
        
        const controls = new THREE.OrbitControls(camera, renderer.domElement);
        controls.enableDamping = true;
        controls.dampingFactor = 0.05;
        controls.minDistance = 2;
        controls.maxDistance = 50;
        
        // Lighting
        const ambientLight = new THREE.AmbientLight(0xffffff, 0.5);
        scene.add(ambientLight);
        
        const keyLight = new THREE.DirectionalLight(0xffffff, 0.8);
        keyLight.position.set(10, 10, 10);
        scene.add(keyLight);
        
        const fillLight = new THREE.DirectionalLight(0x4488ff, 0.3);
        fillLight.position.set(-10, 0, -10);
        scene.add(fillLight);
        
        const backLight = new THREE.DirectionalLight(0xff8844, 0.2);
        backLight.position.set(0, -10, -10);
        scene.add(backLight);
        
        // Draw atoms
        const atomMeshes = [];
        atoms.forEach((atom, i) => {{
            const geometry = new THREE.SphereGeometry(atom.radius / 10, 32, 32);
            const material = new THREE.MeshPhongMaterial({{
                color: atom.color,
                shininess: 80,
                specular: 0x666666,
                emissive: atom.color,
                emissiveIntensity: 0.1
            }});
            const sphere = new THREE.Mesh(geometry, material);
            sphere.position.set(atom.x, atom.y, atom.z);
            sphere.userData = {{ element: atom.element, index: i }};
            scene.add(sphere);
            atomMeshes.push(sphere);
        }});
        
        // Draw bonds
        bonds.forEach(([i, j]) => {{
            const start = new THREE.Vector3(atoms[i].x, atoms[i].y, atoms[i].z);
            const end = new THREE.Vector3(atoms[j].x, atoms[j].y, atoms[j].z);
            const mid = start.clone().lerp(end, 0.5);
            
            // Half-bond coloring (each half gets atom's color)
            const points1 = [start, mid];
            const geometry1 = new THREE.BufferGeometry().setFromPoints(points1);
            const material1 = new THREE.LineBasicMaterial({{ 
                color: atoms[i].color, 
                linewidth: 2,
                transparent: true,
                opacity: 0.9
            }});
            const bond1 = new THREE.Line(geometry1, material1);
            scene.add(bond1);
            
            const points2 = [mid, end];
            const geometry2 = new THREE.BufferGeometry().setFromPoints(points2);
            const material2 = new THREE.LineBasicMaterial({{ 
                color: atoms[j].color, 
                linewidth: 2,
                transparent: true,
                opacity: 0.9
            }});
            const bond2 = new THREE.Line(geometry2, material2);
            scene.add(bond2);
        }});
        
        // Add subtle coordinate grid
        const gridHelper = new THREE.GridHelper(10, 10, 0x222244, 0x111122);
        gridHelper.position.y = -3;
        scene.add(gridHelper);
        
        // FPS counter
        let frameCount = 0;
        let lastTime = performance.now();
        
        // Animation loop
        function animate() {{
            requestAnimationFrame(animate);
            controls.update();
            
            // Gentle atom pulsing
            const time = Date.now() * 0.001;
            atomMeshes.forEach((mesh, i) => {{
                const scale = 1.0 + 0.05 * Math.sin(time * 2 + i * 0.5);
                mesh.scale.set(scale, scale, scale);
            }});
            
            renderer.render(scene, camera);
            
            // Update FPS
            frameCount++;
            const now = performance.now();
            if (now - lastTime >= 1000) {{
                document.getElementById('fps').textContent = frameCount;
                frameCount = 0;
                lastTime = now;
            }}
        }}
        
        // Keyboard controls
        document.addEventListener('keydown', (e) => {{
            if (e.key === 'r' || e.key === 'R') {{
                camera.position.set(5, 5, 8);
                camera.lookAt(0, 0, 0);
                controls.reset();
            }}
            if (e.key === 'f' || e.key === 'F') {{
                if (!document.fullscreenElement) {{
                    document.documentElement.requestFullscreen();
                }} else {{
                    document.exitFullscreen();
                }}
            }}
        }});
        
        // Responsive resize
        window.addEventListener('resize', () => {{
            camera.aspect = window.innerWidth / window.innerHeight;
            camera.updateProjectionMatrix();
            renderer.setSize(window.innerWidth, window.innerHeight);
        }});
        
        animate();
    </script>
</body>
</html>'''
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write(html_template)
        
        return output_file
    
    @staticmethod
    def auto_open(html_file):
        """Automatically open HTML in browser (Chrome preferred, fallback to default)"""
        html_path = Path(html_file).absolute()
        url = f'file:///{html_path.as_posix()}'
        
        # Try Chrome first (preferred for best WebGL performance)
        chrome_paths = [
            r'C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe',
            r'C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe',
            os.path.expanduser('~/AppData/Local/Google/Chrome/Application/chrome.exe'),
            '/usr/bin/google-chrome',
            '/usr/bin/chromium-browser',
            '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'
        ]
        
        for chrome_path in chrome_paths:
            if os.path.exists(chrome_path):
                try:
                    subprocess.Popen([chrome_path, '--new-window', str(html_path)])
                    print(f"✓ Opened in Chrome: {html_file}")
                    return True
                except Exception:
                    continue
        
        # Fallback to default browser
        try:
            webbrowser.open(url, new=2)  # new=2 opens in new tab
            print(f"✓ Opened in default browser: {html_file}")
            return True
        except Exception as e:
            print(f"✗ Failed to open browser: {e}")
            return False

def main():
    if len(sys.argv) < 2:
        print("Usage: python viewer_generator.py <molecule.xyz> [--open]")
        print("       python viewer_generator.py <molecule.xyz> --output custom_name.html --open")
        sys.exit(1)
    
    xyz_file = sys.argv[1]
    auto_open_browser = '--open' in sys.argv or '-o' in sys.argv
    
    # Check for custom output name
    output_file = None
    if '--output' in sys.argv:
        idx = sys.argv.index('--output')
        if idx + 1 < len(sys.argv):
            output_file = sys.argv[idx + 1]
    
    print(f"📊 Generating molecular viewer for: {xyz_file}")
    
    try:
        viewer = MolecularViewer(xyz_file)
        html_file = viewer.generate_html(output_file)
        print(f"✓ Generated: {html_file}")
        print(f"  Atoms: {len(viewer.atoms)}, Bonds: {len(viewer.detect_bonds())}")
        
        if auto_open_browser:
            MolecularViewer.auto_open(html_file)
        else:
            print(f"\nTo view, run: python {sys.argv[0]} {xyz_file} --open")
            print(f"Or open manually: {html_file}")
        
    except Exception as e:
        print(f"✗ Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

if __name__ == '__main__':
    main()
