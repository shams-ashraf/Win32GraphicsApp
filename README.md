Win32GraphicsApp
A Windows-based graphics application developed in C++ using the Win32 API. This project allows users to draw various shapes (lines, circles, ellipses, polygons, and curves) using different algorithms, apply filling techniques, and perform clipping operations. It features an interactive GUI with menu options for selecting shapes, colors, and fill types.
Features

Drawing Shapes:
Lines (DDA, Midpoint, Parametric algorithms)
Circles (Direct, Polar, Iterative Polar, Midpoint, Modified Midpoint algorithms)
Ellipses (Direct, Polar, Midpoint algorithms)
Polygons (Filled and Outline)
Curves (Hermite, Bezier, Cardinal Spline)


Filling Options:
Circle quarter filling with lines or arcs
Square filling with Hermite curves
Rectangle filling with Bezier curves
Recursive and iterative flood fill


Clipping:
Point, line, and polygon clipping within a user-defined clipping window


Additional Features:
Save and load drawings to/from .gfx files
Customizable background colors and cursor types
Interactive drawing with mouse input


Requirements

Operating System: Windows (tested on Windows 10/11)
Compiler: C++ compiler with Windows SDK (e.g., MSVC in Visual Studio)
Libraries: Win32 API (included in Windows SDK)
Development Environment: Visual Studio or any IDE supporting C++ and Win32 API

Build Instructions

Clone the repository:git clone https://github.com/<your-username>/Win32GraphicsApp.git


Open the project in Visual Studio:
Open MAIN.cpp in Visual Studio or your preferred IDE.
Ensure the Windows SDK is installed and configured.

Build the project:
Set the solution configuration to Release or Debug.
Build the project (e.g., Build > Build Solution in Visual Studio).


Run the executable:
The compiled .exe will be in the Debug or Release folder of your project directory.

Usage

Launch the application.
Use the menu bar to:
Select a shape or curve from the Shapes menu.
Choose a color from the Colors menu.
Select filling options from the Fills menu.
Define a clipping window or apply clipping from the Clipping menu.
Change the cursor type from the Cursor menu.
Save or load drawings from the File menu.

Draw shapes:
Click and drag the left mouse button to draw most shapes.
For polygons and splines, left-click to add points and right-click to finalize.
For flood fill, select the fill type and click inside a shape.

Define a clipping window:
Select Define Clipping Window from the Clipping menu and drag to create a rectangle.
Apply clipping using the Point Clipping, Line Clipping, or Polygon Clipping options.

File Structure

MAIN.cpp: The main source file containing the entire application logic.
README.md: This file, providing project documentation.

Notes

The application uses the Win32 API for rendering and requires a Windows environment.
Ensure you have sufficient permissions to write files when saving drawings.
The .gfx file format is custom and used to store shape data in binary format.
Clipping functionality requires defining a clipping window before applying clipping operations.
