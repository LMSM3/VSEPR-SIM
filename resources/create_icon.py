#!/usr/bin/env python3
"""
Create a simple icon for VSEPR-Sim using PIL/Pillow
This creates a molecular structure icon
"""

try:
    from PIL import Image, ImageDraw, ImageFont
    import os
    
    def create_vsepr_icon():
        """Create multi-resolution .ico file"""
        sizes = [16, 32, 48, 64, 128, 256]
        images = []
        
        for size in sizes:
            # Create image with transparency
            img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
            draw = ImageDraw.Draw(img)
            
            # Colors
            bg_color = (41, 128, 185, 255)  # Blue
            atom_color = (255, 255, 255, 255)  # White
            bond_color = (236, 240, 241, 200)  # Light gray
            
            # Scale factors
            center = size // 2
            radius = max(1, size // 8)
            bond_width = max(1, size // 16)
            
            # Draw background circle
            padding = size // 6
            draw.ellipse(
                [padding, padding, size - padding, size - padding],
                fill=bg_color,
                outline=(52, 73, 94, 255),
                width=max(1, size // 32)
            )
            
            # Draw molecular structure (tetrahedral-like)
            # Center atom
            draw.ellipse(
                [center - radius, center - radius, center + radius, center + radius],
                fill=atom_color,
                outline=(189, 195, 199, 255),
                width=max(1, size // 64)
            )
            
            # Outer atoms (4 positions)
            positions = [
                (center - size // 4, center - size // 5),  # Top-left
                (center + size // 4, center - size // 5),  # Top-right
                (center - size // 5, center + size // 4),  # Bottom-left
                (center + size // 5, center + size // 4),  # Bottom-right
            ]
            
            small_radius = max(1, size // 12)
            
            for pos in positions:
                # Draw bond
                draw.line(
                    [center, center, pos[0], pos[1]],
                    fill=bond_color,
                    width=bond_width
                )
                # Draw atom
                draw.ellipse(
                    [pos[0] - small_radius, pos[1] - small_radius,
                     pos[0] + small_radius, pos[1] + small_radius],
                    fill=(231, 76, 60, 255),  # Red
                    outline=(192, 57, 43, 255),
                    width=max(1, size // 64)
                )
            
            images.append(img)
        
        # Save as .ico
        output_path = os.path.join(os.path.dirname(__file__), 'vsepr.ico')
        images[0].save(
            output_path,
            format='ICO',
            sizes=[(img.width, img.height) for img in images],
            append_images=images[1:]
        )
        
        print(f"✓ Icon created: {output_path}")
        print(f"  Sizes: {', '.join(f'{s}x{s}' for s in sizes)}")
        
        # Also save largest as PNG for preview
        png_path = os.path.join(os.path.dirname(__file__), 'vsepr_256.png')
        images[-1].save(png_path, format='PNG')
        print(f"✓ Preview created: {png_path}")
    
    if __name__ == '__main__':
        create_vsepr_icon()

except ImportError:
    print("ERROR: Pillow library not installed")
    print("Install with: pip install Pillow")
    print("\nAlternatively, download an icon manually:")
    print("  - https://www.flaticon.com (search 'molecule')")
    print("  - https://icons8.com (search 'chemistry')")
    print("  - Use IcoFX or GIMP to create custom icons")
    exit(1)
