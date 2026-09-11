### Pico_development_notes
This file is a dumping ground for all things related to pico development, especially using the pico sdk and the vs code extension. 

# starting a project
Best to use the vs code extension and start a project using the tools provided there. A word of caution - it might be problematic to put the new project in a git repo. 

# Cmake tools extension
When creating a project with the pico extension, you can check a box to allow Cmake tools integration. I had no success with this, so leave it unchecked. ALSO, you must disable the CMake tools extension for each pico sdk project as they don't play well. 

# Using with code in a git repo
As of 8/30/2026 when I started working with the pico again, i kept running into a bunch of compiler errors when my project was in a git repo. 

# how to get debugging prints on USB
The UART is the default stdio, but you can select the stdio when you do the following. Find the CMakeLists.txt file in your project dir (which is created when you use the extension to create a new project). Add the lines below in the file:
```
pico_enable_stdio_uart(<target name> 0)
pico_enable_stdio_usb(<target name> 1)
```
Note that these need to be added for each 'target' output, and must be only after the target is declared. 

# Link to 2024 projects
C:\Users\ryanc\Documents\Engineering\Embedded\pico-projects

# TODO list
1. setup interrupt on lis3dh

2. test dormant mode with interrupt
    works fine

3. test dormant mode with wifi connection - is the connection maintained throughout the dormant mode
    Done. This does not appear to work. 




