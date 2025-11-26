# Space Explorer

A 2D OpenGL-based space navigation game built in C++ using GLUT and
Windows Multimedia APIs. The player navigates through obstacles,
collects items, activates power-ups, and races against time to reach a
moving target point controlled by a Bezier curve path.

## Features

-   Smooth 2D movement with arrow keys\
-   Collectibles, obstacles, and power-ups\
-   Speed boost & double-score power-ups\
-   Dynamic moving target with Bezier curve animation\
-   Real-time UI showing timer, score, and lives\
-   Win/Lose end screens with sound effects\
-   Mouse-based placement mode for creating obstacles, collectibles, and
    power-ups

## Tech Stack

-   **C++**\
-   **OpenGL / GLUT**\
-   **Windows.h**\
-   **winmm.lib (MCI sound system)**\
-   **Immediate-mode rendering**

## Folder / Architecture Overview

Since this is a single-file project, all logic is contained in:

    /SpaceExplorer.cpp

Internal systems: - Rendering\
- Input\
- Game loop\
- Audio engine\
- Object placement system\
- End-screen system

## Installation

### 1. Install Dependencies

You need: - FreeGLUT\
- OpenGL\
- MinGW or Visual Studio Build Tools\
- Windows SDK

### 2. Link Required Libraries

    opengl32.lib  
    freeglut.lib  
    winmm.lib

### 3. Place Audio Files

    background.mp3  
    hit.wav  
    collect.wav  
    win.wav  
    lose.wav

## Usage

### Run the Game

-   **R** → Start game\
-   **C** → Clear objects and stop music\
-   Arrow keys → Move\
-   Mouse → Place objects

### Win Condition

Reach the purple rotating target.

### Lose Conditions

-   Lives reach 0\
-   Timer reaches 0

## Configuration

    int totalTime = 120;
    int playerLives = 5;
    float baseSpeed = 200.0f;

## Running Locally

    g++ SpaceExplorer.cpp -lfreeglut -lopengl32 -lwinmm -o SpaceExplorer.exe

## Deployment

Package the .exe with DLLs and sound files.

## Testing

Manual tests: - Collision\
- Sounds\
- Boundaries\
- Power-ups\
- Bezier motion

## Troubleshooting

  Issue       Fix
  ----------- -------------------------------
  No sound    Ensure files are next to .exe
  Freeze      Update GPU drivers
  No render   Check GLUT installation

## Contributing

Fork and submit PRs.

## License

MIT (or as provided)

## Acknowledgements

GLUT instructors and OpenGL community.
