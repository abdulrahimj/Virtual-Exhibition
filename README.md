# Virtual Exhibition Space for Student Project Showcases

## Computer Graphics Final Year Project

### Description
A 3D virtual exhibition hall built with OpenGL where users can walk
through and view student project showcases displayed on walls as
framed posters, with 3D geometric objects exhibited on pedestals.
The system features first-person camera navigation, Phong lighting,
texture mapping, and multiple geometric primitives.

### GitHub Repository
https://github.com/abdulrahimj/Virtual-Exhibition

### Directory Contents
VirtualExhibition/
├── src/
│ └── main.cpp # Main source code (single file)
├── shaders/
│ ├── vertShader.glsl # Vertex shader (transformations)
│ └── fragShader.glsl # Fragment shader (Phong lighting)
├── textures/
│ ├── project1.bmp # Student project poster 1
│ ├── project2.bmp # Student project poster 2
│ ├── project3.bmp # Student project poster 3
│ ├── project4.bmp # Student project poster 4
│ ├── project5.bmp # Student project poster 5
│ ├── project6.bmp # Student project poster 6
│ ├── floor.bmp # Floor texture
│ └── wall.bmp # Wall texture
├── Makefile # Build instructions
└── README.md # This file

### Prerequisites
- MinGW-w64 C++ compiler (g++)
- OpenGL 4.3 compatible GPU
- GLEW library
- GLFW library
- MSYS2 (for Windows builds)

### Build Instructions
```bash
make            # Compile the project
make run        # Compile and run
make clean      # Remove compiled files

### Controls
Key	       Action
- W       Walk forward
- S	      Walk backward
- A	      Strafe left
- D	      Strafe right
- Mouse	  Look around
- ESC	    Release/capture mouse

### Features
- 3D exhibition room with floor ceiling, and four walls
- 6 student project posters displayed in frames on walls
- 3 pedestals with rotating 3D objects (sphere, cube, cylinder)
- First-person camera with WASD movement and mouse look
- Phong lighting model (ambient + diffuse + specular)
- BMP texture loading and mapping
- Room boundary collision detection

### Author
Name: Group 7
ID:   Year4
Institution: University of Makeni (UniMak)