# Golden/BLAS

A bytecode compiler and virtual machine written in C.

## Quick Start

### Build
```bash
cd client && make
cd ../vm && make
```

### Create Project
```bash
./bin/gld new myproject
```

### Build & Run
```bash
./bin/gld build myproject
./bin/gldvm run myproject/myproject.gld
```

## Commands

### Compiler (gld)
```bash
./bin/gld new <name>           # Create a new project
./bin/gld build <directory>    # Compile to bytecode
./bin/gld clean                # Clean compiled files
./bin/gld --version            # Show version
./bin/gld --help               # Show help
```

### Virtual Machine (gldvm)
```bash
./bin/gldvm run <file.gld>              # Run bytecode
./bin/gldvm run <file.gld> --debug      # Run with debug info
./bin/gldvm run <file.gld> --renderer opengl  # Use OpenGL
./bin/gldvm run <file.gld> --renderer none    # Console mode
./bin/gldvm --version                   # Show version
./bin/gldvm --help                      # Show help
```

## Project Configuration

Add `project.conf` to your project:

```properties
[project]
name = myapp
version = 1.0

[graphics]
window_title = My Application
window_width = 1280
window_height = 720
window_resizable = yes
window_mode = windowed
renderer = opengl
fps = 60
```

## Features

- Bytecode compilation and execution
- OpenGL rendering support
- Console mode
- Configurable frame rate (1-240 fps)
- Arrays with `.len` and `.clear()` methods
- Global variables
- Class support