#define _USE_MATH_DEFINES
#include <windows.h>
#include <commdlg.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <iostream>
#include <string>
#include <fstream>
#include <stack>
#include <queue>

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "kernel32.lib")

using namespace std;

string GetColorName(COLORREF color) {
    switch (color) {
    case RGB(255, 0, 0): return "Red";
    case RGB(0, 255, 0): return "Green";
    case RGB(0, 0, 255): return "Blue";
    case RGB(255, 255, 0): return "Yellow";
    case RGB(0, 0, 0): return "Black";
    case RGB(255, 255, 255): return "White";
    default: {
        char hex[10];
        sprintf_s(hex, "#%06X", color & 0xFFFFFF);
        return string(hex);
    }
    }
}

struct Vector2D {
    double x, y;
    Vector2D(double x = 0, double y = 0) : x(x), y(y) {}
};

struct Point {
    int x, y;
    Point(int x = 0, int y = 0) : x(x), y(y) {}
};

struct ClipRect {
    int xmin, ymin, xmax, ymax;
    ClipRect(int x1 = 0, int y1 = 0, int x2 = 0, int y2 = 0) : xmin(x1), ymin(y1), xmax(x2), ymax(y2) {}
    operator RECT() const { RECT r = { xmin, ymin, xmax }; return r; }
};

struct Shape {
    int type;
    int subtype;
    Point center;
    int size1, size2;
    COLORREF color;
    vector<Point> points;
    double tension;
    string algorithm;
    unsigned int filledQuartersMask;
};

HBRUSH g_hbrBackground = NULL;
HCURSOR g_hCursor = NULL;
COLORREF g_selectedColor = RGB(0, 0, 0);
HMENU g_hMenu = NULL;
int g_selectedShape = 0;
int g_selectedSubtype = 0;
bool g_isDrawing = false;
Point g_startPoint;
vector<Shape> g_shapes;
vector<Point> g_tempControlPoints;
vector<Point> g_tempPolygonPoints;
float g_tension = 0.5f;
int g_numSegments = 30;
bool g_colorSelected = false;
ClipRect g_clipRect(100, 100, 400, 300);
Point g_floodFillSeed;
bool g_floodFillMode = false;
bool g_clippingMode = false;
bool g_stopFloodFill = false;

bool g_isDefiningClipRect = false;
ClipRect g_tempClipRect;
ClipRect g_userDefinedClipRect(0, 0, 0, 0);
bool g_userClipRectDefined = false;

vector<Point> currentPolygon;
vector<vector<Point>> drawnPolygons;

enum ClipEdgeType { CLIP_EDGE_LEFT, CLIP_EDGE_RIGHT, CLIP_EDGE_TOP, CLIP_EDGE_BOTTOM };

bool g_fillMode = false;
int g_fillQuarter = 0;
int g_fillShapeIndex = -1;
int g_selectedFillType = 0;

#define FILL_TYPE_NONE 0
#define FILL_TYPE_LINES 1
#define FILL_TYPE_ARCS 2

#define FILL_LINE_Q1_MASK  (1 << 0)
#define FILL_LINE_Q2_MASK  (1 << 1)
#define FILL_LINE_Q3_MASK  (1 << 2)
#define FILL_LINE_Q4_MASK  (1 << 3)
#define FILL_ARC_Q1_MASK   (1 << 4)
#define FILL_ARC_Q2_MASK   (1 << 5)
#define FILL_ARC_Q3_MASK   (1 << 6)
#define FILL_ARC_Q4_MASK   (1 << 7)

#define ID_COLOR_RED        1001
#define ID_COLOR_GREEN      1002
#define ID_COLOR_BLUE       1003
#define ID_COLOR_YELLOW     1004
#define ID_BG_WHITE         1005
#define ID_BG_BLACK         1006
#define ID_CLEAR_SCREEN     1007
#define ID_SAVE_FILE        1008
#define ID_LOAD_FILE        1009

#define ID_LINE_DDA         1010
#define ID_LINE_MIDPOINT    1011
#define ID_LINE_PARAMETRIC  1012

#define ID_CIRCLE_DIRECT    1020
#define ID_CIRCLE_POLAR     1021
#define ID_CIRCLE_ITER_POLAR 1022
#define ID_CIRCLE_MIDPOINT  1023
#define ID_CIRCLE_MODIFIED  1024

#define ID_CIRCLE_FILL_LINES_Q1  1030
#define ID_CIRCLE_FILL_LINES_Q2  1031
#define ID_CIRCLE_FILL_LINES_Q3  1032
#define ID_CIRCLE_FILL_LINES_Q4  1033
#define ID_CIRCLE_FILL_ARCS_Q1 1034
#define ID_CIRCLE_FILL_ARCS_Q2 1035
#define ID_CIRCLE_FILL_ARCS_Q3 1036
#define ID_CIRCLE_FILL_ARCS_Q4 1037

#define ID_SQUARE_HERMITE   1040
#define ID_RECT_BEZIER      1041
#define ID_POLYGON_CONVEX   1042
#define ID_FLOOD_RECURSIVE  1043
#define ID_FLOOD_ITERATIVE  1044
#define ID_CARDINAL_SPLINE  1045

#define ID_ELLIPSE_DIRECT   1050
#define ID_ELLIPSE_POLAR    1051
#define ID_ELLIPSE_MIDPOINT 1052

#define ID_DEFINE_CLIP_RECT   1066
#define ID_APPLY_POINT_CLIP   1067
#define ID_APPLY_LINE_CLIP    1068
#define ID_APPLY_POLYGON_CLIP 1069
#define ID_CLEAR_CLIPPING     1070

#define ID_SHAPE_POINT            1080
#define ID_SHAPE_POLYGON_OUTLINE  1081

#define ID_CURSOR_CROSS 1090
#define ID_CURSOR_ARROW 1091
#define ID_CURSOR_HAND  1092

void DrawLineDDA(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color);
void DrawCircleMidpoint(HDC hdc, int xc, int yc, int r, COLORREF color);
void DrawCircleFillArcs(HDC hdc, int xc, int yc, int r, COLORREF color, int quarter);
void FloodFillRecursive(HDC hdc, int x, int y, COLORREF newColor, COLORREF oldColor, HWND hwnd);
bool ClipLine(Point& p1, Point& p2, ClipRect rect);
vector<Point> ClipPolygon(const vector<Point>& polygon, const RECT& clipRect);

void DrawSquare(HDC hdc, int x, int y, int size, COLORREF c) {
    HPEN pen = CreatePen(PS_SOLID, 1, c);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, x, y, x + size, y + size);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void DrawRectangle(HDC hdc, int x, int y, int width, int height, COLORREF c) {
    HPEN pen = CreatePen(PS_SOLID, 1, c);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Rectangle(hdc, x, y, x + width, y + height);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void DrawCircleFillLines(HDC hdc, int xc, int yc, int r, COLORREF color, int quarter) {
    for (int y = yc - r; y <= yc + r; y++) {
        int dx = (int)(sqrt(max(0, r * r - (y - yc) * (y - yc))) + 0.5);
        switch (quarter) {
        case 1: if (y <= yc) DrawLineDDA(hdc, xc, y, xc + dx, y, color); break;
        case 2: if (y <= yc) DrawLineDDA(hdc, xc - dx, y, xc, y, color); break;
        case 3: if (y >= yc) DrawLineDDA(hdc, xc - dx, y, xc, y, color); break;
        case 4: if (y >= yc) DrawLineDDA(hdc, xc, y, xc + dx, y, color); break;
        }
    }
}

void DrawCircleFillArcs(HDC hdc, int xc, int yc, int r, COLORREF color, int quarter) {
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    for (int current_r = 5; current_r <= r; current_r += 4) {
        int x1 = xc - current_r;
        int y1 = yc - current_r;
        int x2 = xc + current_r;
        int y2 = yc + current_r;
        Point start, end;
        switch (quarter) {
        case 1: start = { xc + current_r, yc }; end = { xc, yc - current_r }; break;
        case 2: start = { xc, yc - current_r }; end = { xc - current_r, yc }; break;
        case 3: start = { xc - current_r, yc }; end = { xc, yc + current_r }; break;
        case 4: start = { xc, yc + current_r }; end = { xc + current_r, yc }; break;
        }
        Arc(hdc, x1, y1, x2, y2, start.x, start.y, end.x, end.y);
    }
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

void DrawLineDDA(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color) {
    int dx = x2 - x1;
    int dy = y2 - y1;
    int steps = max(abs(dx), abs(dy));
    if (steps == 0) { SetPixel(hdc, x1, y1, color); return; }
    float xIncrement = (float)dx / steps;
    float yIncrement = (float)dy / steps;
    float x = (float)x1;
    float y = (float)y1;
    for (int i = 0; i <= steps; i++) {
        SetPixel(hdc, (int)(x + 0.5), (int)(y + 0.5), color);
        x += xIncrement;
        y += yIncrement;
    }
}
void DrawLineMidpoint(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (true) {
        SetPixel(hdc, x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}
void DrawLineParametric(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color) {
    float dx = (float)(x2 - x1);
    float dy = (float)(y2 - y1);
    for (float t = 0; t <= 1; t += 0.001) {
        int x = (int)(x1 + t * dx + 0.5);
        int y = (int)(y1 + t * dy + 0.5);
        SetPixel(hdc, x, y, color);
    }
}
void DrawCircleDirect(HDC hdc, int xc, int yc, int r, COLORREF color) {
    for (int x = xc - r; x <= xc + r; x++) {
        int y_val = (int)(yc + sqrt(r * r - (x - xc) * (x - xc)) + 0.5);
        SetPixel(hdc, x, y_val, color);
        y_val = (int)(yc - sqrt(r * r - (x - xc) * (x - xc)) + 0.5);
        SetPixel(hdc, x, y_val, color);
    }
    for (int y = yc - r; y <= yc + r; y++) {
        int x_val = (int)(xc + sqrt(r * r - (y - yc) * (y - yc)) + 0.5);
        SetPixel(hdc, x_val, y, color);
        x_val = (int)(xc - sqrt(r * r - (y - yc) * (y - yc)) + 0.5);
        SetPixel(hdc, x_val, y, color);
    }
}
void DrawCirclePolar(HDC hdc, int xc, int yc, int r, COLORREF color) {
    for (float theta = 0; theta <= M_PI / 4; theta += 0.001) {
        int x = (int)(xc + r * cos(theta) + 0.5);
        int y = (int)(yc + r * sin(theta) + 0.5);
        SetPixel(hdc, x, y, color);
        SetPixel(hdc, xc - (x - xc), y, color);
        SetPixel(hdc, x, yc - (y - yc), color);
        SetPixel(hdc, xc - (x - xc), yc - (y - yc), color);
        SetPixel(hdc, xc + (y - yc), yc + (x - xc), color);
        SetPixel(hdc, xc - (y - yc), yc + (x - xc), color);
        SetPixel(hdc, xc + (y - yc), yc - (x - xc), color);
        SetPixel(hdc, xc - (y - yc), yc - (x - xc), color);
    }
}
void DrawCircleIterativePolar(HDC hdc, int xc, int yc, int r, COLORREF color) {
    double d_theta = 1.0 / r;
    if (d_theta > M_PI / (2 * r)) d_theta = M_PI / (2 * r);

    for (double theta = 0; theta <= M_PI / 4 + d_theta; theta += d_theta) {
        int x = (int)(r * cos(theta) + 0.5);
        int y = (int)(r * sin(theta) + 0.5);

        SetPixel(hdc, xc + x, yc + y, color);
        SetPixel(hdc, xc - x, yc + y, color);
        SetPixel(hdc, xc + x, yc - y, color);
        SetPixel(hdc, xc - x, yc - y, color);
        SetPixel(hdc, xc + y, yc + x, color);
        SetPixel(hdc, xc - y, yc + x, color);
        SetPixel(hdc, xc + y, yc - x, color);
        SetPixel(hdc, xc - y, yc - x, color);
    }
}
void DrawCircleMidpoint(HDC hdc, int xc, int yc, int r, COLORREF color) {
    int x = 0;
    int y = r;
    int d = 1 - r;

    auto draw_points = [&](int cur_x, int cur_y) {
        SetPixel(hdc, xc + cur_x, yc + cur_y, color);
        SetPixel(hdc, xc - cur_x, yc + cur_y, color);
        SetPixel(hdc, xc + cur_x, yc - cur_y, color);
        SetPixel(hdc, xc - cur_x, yc - cur_y, color);
        SetPixel(hdc, xc + cur_y, yc + cur_x, color);
        SetPixel(hdc, xc - cur_y, yc + cur_x, color);
        SetPixel(hdc, xc + cur_y, yc - cur_x, color);
        SetPixel(hdc, xc - cur_y, yc - cur_x, color);
        };

    draw_points(x, y);

    while (y > x) {
        if (d < 0) {
            d += 2 * x + 3;
        }
        else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
        draw_points(x, y);
    }
}
void DrawCircleModified(HDC hdc, int xc, int yc, int r, COLORREF color) {
    int x = 0;
    int y = r;
    int d = 1 - r;

    auto draw_points = [&](int cur_x, int cur_y) {
        SetPixel(hdc, xc + cur_x, yc + cur_y, color);
        SetPixel(hdc, xc - cur_x, yc + cur_y, color);
        SetPixel(hdc, xc + cur_x, yc - cur_y, color);
        SetPixel(hdc, xc - cur_x, yc - cur_y, color);
        SetPixel(hdc, xc + cur_y, yc + cur_x, color);
        SetPixel(hdc, xc - cur_y, yc + cur_x, color);
        SetPixel(hdc, xc + cur_y, yc - cur_x, color);
        SetPixel(hdc, xc - cur_y, yc - cur_x, color);
        };

    draw_points(x, y);

    while (x < y) {
        if (d < 0) {
            d += 2 * x + 3;
        }
        else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
        draw_points(x, y);
    }
}
void DrawEllipseDirect(HDC hdc, int xc, int yc, int a, int b, COLORREF color) {
    for (int x = xc - a; x <= xc + a; x++) {
        int y_val = (int)(yc + b * sqrt(1.0 - pow((double)(x - xc) / a, 2)) + 0.5);
        SetPixel(hdc, x, y_val, color);
        y_val = (int)(yc - b * sqrt(1.0 - pow((double)(x - xc) / a, 2)) + 0.5);
        SetPixel(hdc, x, y_val, color);
    }
    for (int y = yc - b; y <= yc + b; y++) {
        int x_val = (int)(xc + a * sqrt(1.0 - pow((double)(y - yc) / b, 2)) + 0.5);
        SetPixel(hdc, x_val, y, color);
        x_val = (int)(xc - a * sqrt(1.0 - pow((double)(y - yc) / b, 2)) + 0.5);
        SetPixel(hdc, x_val, y, color);
    }
}
void DrawEllipsePolar(HDC hdc, int xc, int yc, int a, int b, COLORREF color) {
    for (float theta = 0; theta <= M_PI / 2; theta += 0.001) {
        int x = (int)(xc + a * cos(theta) + 0.5);
        int y = (int)(yc + b * sin(theta) + 0.5);
        SetPixel(hdc, x, y, color);
        SetPixel(hdc, xc - (x - xc), y, color);
        SetPixel(hdc, x, yc - (y - yc), color);
        SetPixel(hdc, xc - (x - xc), yc - (y - yc), color);
    }
}
void DrawEllipseMidpoint(HDC hdc, int xc, int yc, int a, int b, COLORREF color) {
    int x = 0;
    int y = b;
    long p = (long)round(b * b - a * a * b + 0.25 * a * a);

    auto draw_points = [&](int cur_x, int cur_y) {
        SetPixel(hdc, xc + cur_x, yc + cur_y, color);
        SetPixel(hdc, xc - cur_x, yc + cur_y, color);
        SetPixel(hdc, xc + cur_x, yc - cur_y, color);
        SetPixel(hdc, xc - cur_x, yc - cur_y, color);
        };

    draw_points(x, y);

    while (2 * b * b * x <= 2 * a * a * y) {
        x++;
        if (p < 0) {
            p += 2 * b * b * x + b * b;
        }
        else {
            y--;
            p += 2 * b * b * x + b * b - 2 * a * a * y;
        }
        draw_points(x, y);
    }

    x = a;
    y = 0;
    p = (long)round(a * a - b * b * a + 0.25 * b * b);

    draw_points(x, y);

    while (2 * a * a * y <= 2 * b * b * x) {
        y++;
        if (p < 0) {
            p += 2 * a * a * y + a * a;
        }
        else {
            x--;
            p += 2 * a * a * y + a * a - 2 * b * b * x;
        }
        draw_points(x, y);
    }
}

Vector2D HermitePoint(Vector2D p0, Vector2D p1, Vector2D t0, Vector2D t1, float t) {
    float t2 = t * t;
    float t3 = t2 * t;
    float h1 = 2 * t3 - 3 * t2 + 1;
    float h2 = -2 * t3 + 3 * t2;
    float h3 = t3 - 2 * t2 + t;
    float h4 = t3 - t2;
    return Vector2D(p0.x * h1 + p1.x * h2 + t0.x * h3 + t1.x * h4,
        p0.y * h1 + p1.y * h2 + t0.y * h3 + t1.y * h4);
}
void DrawHermiteCurve(HDC hdc, Vector2D p0, Vector2D p1, Vector2D t0, Vector2D t1, int steps, COLORREF c) {
    Vector2D prev_p = p0;
    for (int i = 1; i <= steps; ++i) {
        float t = (float)i / steps;
        Vector2D current_p = HermitePoint(p0, p1, t0, t1, t);
        DrawLineDDA(hdc, (int)prev_p.x, (int)prev_p.y, (int)current_p.x, (int)current_p.y, c);
        prev_p = current_p;
    }
}

Vector2D BezierPoint(Vector2D p0, Vector2D p1, Vector2D p2, Vector2D p3, float t) {
    float invT = 1.0f - t;
    float invT2 = invT * invT;
    float invT3 = invT2 * invT;
    float t2 = t * t;
    float t3 = t2 * t;
    return Vector2D(invT3 * p0.x + 3 * invT2 * t * p1.x + 3 * invT * t2 * p2.x + t3 * p3.x,
        invT3 * p0.y + 3 * invT2 * t * p1.y + 3 * invT * t2 * p2.y + t3 * p3.y);
}
void DrawBezierCurve(HDC hdc, Vector2D& p0, Vector2D& p1, Vector2D& p2, Vector2D& p3, int numpoints, COLORREF c) {
    Vector2D prev_p = p0;
    for (int i = 1; i <= numpoints; ++i) {
        float t = (float)i / numpoints;
        Vector2D current_p = BezierPoint(p0, p1, p2, p3, t);
        DrawLineDDA(hdc, (int)prev_p.x, (int)prev_p.y, (int)current_p.x, (int)current_p.y, c);
        prev_p = current_p;
    }
}

void FillSquareWithHermite(HDC hdc, int x, int y, int size, COLORREF c) {
    DrawSquare(hdc, x, y, size, c);
    for (int offset = 0; offset <= size; offset += 2) {
        Vector2D p0(static_cast<double>(x + offset), static_cast<double>(y));
        Vector2D p1(static_cast<double>(x + offset), static_cast<double>(y + size));
        Vector2D t0(0, 20);
        Vector2D t1(0, -20);
        DrawHermiteCurve(hdc, p0, p1, t0, t1, 50, c);
    }
}

void FillRectangleWithBezier(HDC hdc, int x, int y, int width, int height, COLORREF c) {
    DrawRectangle(hdc, x, y, width, height, c);
    for (int offset = 0; offset <= height; offset += 2) {
        Vector2D p0(static_cast<double>(x), static_cast<double>(y + offset));
        Vector2D p1(static_cast<double>(x + width / 3), static_cast<double>(y + offset + 20));
        Vector2D p2(static_cast<double>(x + 2 * width / 3), static_cast<double>(y + offset - 20));
        Vector2D p3(static_cast<double>(x + width), static_cast<double>(y + offset));
        DrawBezierCurve(hdc, p0, p1, p2, p3, 50, c);
    }
}

void FloodFillRecursive(HDC hdc, int x, int y, COLORREF newColor, COLORREF oldColor, HWND hwnd) {
    if (x < 0 || x >= GetSystemMetrics(SM_CXSCREEN) || y < 0 || y >= GetSystemMetrics(SM_CYSCREEN)) return;
    if (GetPixel(hdc, x, y) == oldColor && newColor != oldColor && !g_stopFloodFill) {
        SetPixel(hdc, x, y, newColor);
        FloodFillRecursive(hdc, x + 1, y, newColor, oldColor, hwnd);
        FloodFillRecursive(hdc, x - 1, y, newColor, oldColor, hwnd);
        FloodFillRecursive(hdc, x, y + 1, newColor, oldColor, hwnd);
        FloodFillRecursive(hdc, x, y - 1, newColor, oldColor, hwnd);
    }
}
void FloodFillIterative(HDC hdc, int x, int y, COLORREF newColor, COLORREF oldColor, HWND hwnd) {
    if (oldColor == newColor) return;
    stack<Point> s;
    s.push({ x, y });
    while (!s.empty()) {
        Point p = s.top();
        s.pop();

        if (p.x < 0 || p.x >= GetSystemMetrics(SM_CXSCREEN) || p.y < 0 || p.y >= GetSystemMetrics(SM_CYSCREEN)) continue;
        if (GetPixel(hdc, p.x, p.y) != oldColor) continue;

        SetPixel(hdc, p.x, p.y, newColor);

        s.push({ p.x + 1, p.y });
        s.push({ p.x - 1, p.y });
        s.push({ p.x, p.y + 1 });
        s.push({ p.x, p.y - 1 });
    }
}

bool ClipPoint(Point p, ClipRect rect) {
    return (p.x >= rect.xmin && p.x <= rect.xmax && p.y >= rect.ymin && p.y <= rect.ymax);
}

int ComputeOutCode(Point p, ClipRect rect) {
    int code = 0;
    if (p.x < rect.xmin) code |= 1;
    if (p.x > rect.xmax) code |= 2;
    if (p.y < rect.ymin) code |= 4;
    if (p.y > rect.ymax) code |= 8;
    return code;
}

bool ClipLine(Point& p1, Point& p2, ClipRect rect) {
    int outcode1 = ComputeOutCode(p1, rect);
    int outcode2 = ComputeOutCode(p2, rect);
    bool accept = false;

    while (true) {
        if (!(outcode1 | outcode2)) {
            accept = true;
            break;
        }
        else if (outcode1 & outcode2) {
            break;
        }
        else {
            int outcodeOut = outcode1 ? outcode1 : outcode2;
            double x, y;

            if (outcodeOut & 1) {
                x = rect.xmin;
                y = p1.y + (p2.y - p1.y) * (rect.xmin - p1.x) / (double)(p2.x - p1.x);
            }
            else if (outcodeOut & 2) {
                x = rect.xmax;
                y = p1.y + (p2.y - p1.y) * (rect.xmax - p1.x) / (double)(p2.x - p1.x);
            }
            else if (outcodeOut & 4) {
                y = rect.ymin;
                x = p1.x + (p2.x - p1.x) * (rect.ymin - p1.y) / (double)(p2.y - p1.y);
            }
            else if (outcodeOut & 8) {
                y = rect.ymax;
                x = p1.x + (p2.x - p1.x) * (rect.ymax - p1.y) / (double)(p2.y - p1.y);
            }

            if (outcodeOut == outcode1) {
                p1.x = (int)x;
                p1.y = (int)y;
                outcode1 = ComputeOutCode(p1, rect);
            }
            else {
                p2.x = (int)x;
                p2.y = (int)y;
                outcode2 = ComputeOutCode(p2, rect);
            }
        }
    }
    return accept;
}

Point Intersect(Point p1, Point p2, ClipEdgeType edge, const RECT& clipRect) {
    Point ip;
    if (edge == CLIP_EDGE_LEFT) {
        ip.x = clipRect.left;
        ip.y = p1.y + (p2.y - p1.y) * (clipRect.left - p1.x) / (p2.x - p1.x);
    }
    else if (edge == CLIP_EDGE_RIGHT) {
        ip.x = clipRect.right;
        ip.y = p1.y + (p2.y - p1.y) * (clipRect.right - p1.x) / (p2.x - p1.x);
    }
    else if (edge == CLIP_EDGE_TOP) {
        ip.y = clipRect.top;
        ip.x = p1.x + (p2.x - p1.x) * (clipRect.top - p1.y) / (p2.y - p1.y);
    }
    else {
        ip.y = clipRect.bottom;
        ip.x = p1.x + (p2.x - p1.x) * (clipRect.bottom - p1.y) / (p2.y - p1.y);
    }
    return ip;
}

bool IsInside(Point p, ClipEdgeType edge, const RECT& clipRect) {
    if (edge == CLIP_EDGE_LEFT) return p.x >= clipRect.left;
    if (edge == CLIP_EDGE_RIGHT) return p.x <= clipRect.right;
    if (edge == CLIP_EDGE_TOP) return p.y >= clipRect.top;
    return p.y <= clipRect.bottom;
}

vector<Point> ClipPolygon(const vector<Point>& polygon, const RECT& clipRect) {
    vector<Point> outputList = polygon;
    ClipEdgeType edges[] = { CLIP_EDGE_LEFT, CLIP_EDGE_RIGHT, CLIP_EDGE_TOP, CLIP_EDGE_BOTTOM };

    for (ClipEdgeType edge : edges) {
        vector<Point> inputList = outputList;
        outputList.clear();

        if (inputList.empty()) break;

        Point S = inputList.back();

        for (const Point& P : inputList) {
            if (IsInside(P, edge, clipRect)) {
                if (!IsInside(S, edge, clipRect)) {
                    outputList.push_back(Intersect(S, P, edge, clipRect));
                }
                outputList.push_back(P);
            }
            else if (IsInside(S, edge, clipRect)) {
                outputList.push_back(Intersect(S, P, edge, clipRect));
            }
            S = P;
        }
    }
    return outputList;
}

void DrawCardinalSpline(HDC hdc, const vector<Point>& points, double tension, int segments, COLORREF color) {
    if (points.size() < 4) {
        if (points.size() >= 2) {
            MoveToEx(hdc, points[0].x, points[0].y, NULL);
            for (size_t i = 1; i < points.size(); ++i) {
                LineTo(hdc, points[i].x, points[i].y);
            }
        }
        return;
    }

    float t_val;
    Vector2D p0, p1, p2, p3;
    Vector2D tangent1, tangent2;
    Vector2D current_point, prev_point;

    for (size_t i = 0; i < points.size() - 3; ++i) {
        p0 = Vector2D(points[i].x, points[i].y);
        p1 = Vector2D(points[i + 1].x, points[i + 1].y);
        p2 = Vector2D(points[i + 2].x, points[i + 2].y);
        p3 = Vector2D(points[i + 3].x, points[i + 3].y);

        tangent1.x = tension * (p2.x - p0.x);
        tangent1.y = tension * (p2.y - p0.y);
        tangent2.x = tension * (p3.x - p1.x);
        tangent2.y = tension * (p3.y - p1.y);

        prev_point = p1;

        for (int j = 1; j <= segments; ++j) {
            t_val = (float)j / segments;
            current_point = HermitePoint(p1, p2, tangent1, tangent2, t_val);
            DrawLineDDA(hdc, (int)prev_point.x, (int)prev_point.y, (int)current_point.x, (int)current_point.y, color);
            prev_point = current_point;
        }
    }
}

void ClearScreen(HWND hwnd) {
    g_shapes.clear();
    g_tempControlPoints.clear();
    g_tempPolygonPoints.clear();
    InvalidateRect(hwnd, NULL, TRUE);
}

void ClearClipping(HWND hwnd) {
    g_userClipRectDefined = false;
    InvalidateRect(hwnd, NULL, TRUE);
}

void SaveToFile(HWND hwnd) {
    OPENFILENAME ofn;
    wchar_t szFile[260] = { 0 };
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"Graphics Files (*.gfx)\0*.gfx\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = L"gfx"; // Automatically append .gfx if not provided
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (GetSaveFileName(&ofn) == TRUE) {
        // Convert wide character path to UTF-8 for std::ofstream
        string filePath;
        int bufferSize = WideCharToMultiByte(CP_UTF8, 0, ofn.lpstrFile, -1, NULL, 0, NULL, NULL);
        if (bufferSize > 0) {
            filePath.resize(bufferSize);
            WideCharToMultiByte(CP_UTF8, 0, ofn.lpstrFile, -1, &filePath[0], bufferSize, NULL, NULL);
            // Remove null terminator from string
            if (!filePath.empty() && filePath.back() == '\0') {
                filePath.pop_back();
            }
        }
        else {
            MessageBox(hwnd, L"Error converting file path.", L"Error", MB_OK | MB_ICONERROR);
            return;
        }

        ofstream outFile(filePath, ios::binary);
        if (outFile.is_open()) {
            size_t numShapes = g_shapes.size();
            outFile.write(reinterpret_cast<const char*>(&numShapes), sizeof(size_t));
            for (const auto& shape : g_shapes) {
                outFile.write(reinterpret_cast<const char*>(&shape.type), sizeof(int));
                outFile.write(reinterpret_cast<const char*>(&shape.subtype), sizeof(int));
                outFile.write(reinterpret_cast<const char*>(&shape.center), sizeof(Point));
                outFile.write(reinterpret_cast<const char*>(&shape.size1), sizeof(int));
                outFile.write(reinterpret_cast<const char*>(&shape.size2), sizeof(int));
                outFile.write(reinterpret_cast<const char*>(&shape.color), sizeof(COLORREF));
                outFile.write(reinterpret_cast<const char*>(&shape.tension), sizeof(double));
                outFile.write(reinterpret_cast<const char*>(&shape.filledQuartersMask), sizeof(unsigned int));

                // Save points vector
                size_t numPoints = shape.points.size();
                outFile.write(reinterpret_cast<const char*>(&numPoints), sizeof(size_t));
                if (numPoints > 0) {
                    outFile.write(reinterpret_cast<const char*>(shape.points.data()), numPoints * sizeof(Point));
                }

                // Save algorithm string
                size_t algoLen = shape.algorithm.length();
                outFile.write(reinterpret_cast<const char*>(&algoLen), sizeof(size_t));
                if (algoLen > 0) {
                    outFile.write(shape.algorithm.c_str(), algoLen);
                }
            }
            outFile.close();
            MessageBox(hwnd, L"Shapes saved successfully!", L"Save", MB_OK | MB_ICONINFORMATION);
        }
        else {
            MessageBox(hwnd, L"Failed to open file for saving. Check path and permissions.", L"Error", MB_OK | MB_ICONERROR);
        }
    }
}

void LoadFromFile(HWND hwnd) {
    OPENFILENAME ofn;
    wchar_t szFile[260] = { 0 };
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"Graphics Files (*.gfx)\0*.gfx\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = L"gfx";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileName(&ofn) == TRUE) {
        // Convert wide character path to UTF-8 for std::ifstream
        string filePath;
        int bufferSize = WideCharToMultiByte(CP_UTF8, 0, ofn.lpstrFile, -1, NULL, 0, NULL, NULL);
        if (bufferSize > 0) {
            filePath.resize(bufferSize);
            WideCharToMultiByte(CP_UTF8, 0, ofn.lpstrFile, -1, &filePath[0], bufferSize, NULL, NULL);
            // Remove null terminator from string
            if (!filePath.empty() && filePath.back() == '\0') {
                filePath.pop_back();
            }
        }
        else {
            MessageBox(hwnd, L"Error converting file path.", L"Error", MB_OK | MB_ICONERROR);
            return;
        }

        ifstream inFile(filePath, ios::binary);
        if (inFile.is_open()) {
            ClearScreen(hwnd); // Clear existing shapes
            size_t numShapes;
            inFile.read(reinterpret_cast<char*>(&numShapes), sizeof(size_t));
            if (inFile.fail()) {
                inFile.close();
                MessageBox(hwnd, L"Failed to read number of shapes.", L"Error", MB_OK | MB_ICONERROR);
                return;
            }
            g_shapes.clear();
            g_shapes.reserve(numShapes);
            for (size_t i = 0; i < numShapes && !inFile.eof(); ++i) {
                Shape shape;
                inFile.read(reinterpret_cast<char*>(&shape.type), sizeof(int));
                inFile.read(reinterpret_cast<char*>(&shape.subtype), sizeof(int));
                inFile.read(reinterpret_cast<char*>(&shape.center), sizeof(Point));
                inFile.read(reinterpret_cast<char*>(&shape.size1), sizeof(int));
                inFile.read(reinterpret_cast<char*>(&shape.size2), sizeof(int));
                inFile.read(reinterpret_cast<char*>(&shape.color), sizeof(COLORREF));
                inFile.read(reinterpret_cast<char*>(&shape.tension), sizeof(double));
                inFile.read(reinterpret_cast<char*>(&shape.filledQuartersMask), sizeof(unsigned int));

                // Load points vector
                size_t numPoints;
                inFile.read(reinterpret_cast<char*>(&numPoints), sizeof(size_t));
                if (inFile.fail()) {
                    inFile.close();
                    MessageBox(hwnd, L"Failed to read points data.", L"Error", MB_OK | MB_ICONERROR);
                    return;
                }
                shape.points.resize(numPoints);
                if (numPoints > 0) {
                    inFile.read(reinterpret_cast<char*>(shape.points.data()), numPoints * sizeof(Point));
                    if (inFile.fail()) {
                        inFile.close();
                        MessageBox(hwnd, L"Failed to read points data.", L"Error", MB_OK | MB_ICONERROR);
                        return;
                    }
                }

                // Load algorithm string
                size_t algoLen;
                inFile.read(reinterpret_cast<char*>(&algoLen), sizeof(size_t));
                if (inFile.fail()) {
                    inFile.close();
                    MessageBox(hwnd, L"Failed to read algorithm string length.", L"Error", MB_OK | MB_ICONERROR);
                    return;
                }
                if (algoLen > 0) {
                    shape.algorithm.resize(algoLen);
                    inFile.read(&shape.algorithm[0], algoLen);
                    if (inFile.fail()) {
                        inFile.close();
                        MessageBox(hwnd, L"Failed to read algorithm string.", L"Error", MB_OK | MB_ICONERROR);
                        return;
                    }
                }
                g_shapes.push_back(shape);
            }
            inFile.close();
            InvalidateRect(hwnd, NULL, TRUE);
            MessageBox(hwnd, L"Shapes loaded successfully!", L"Load", MB_OK | MB_ICONINFORMATION);
        }
        else {
            MessageBox(hwnd, L"Failed to open file for loading. Check if file exists.", L"Error", MB_OK | MB_ICONERROR);
        }
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HDC hdc;
    static PAINTSTRUCT ps;
    switch (msg) {
    case WM_CREATE:
    {
        AllocConsole();
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        g_hCursor = LoadCursor(NULL, IDC_CROSS);
        g_hbrBackground = CreateSolidBrush(RGB(255, 255, 255));
        g_hMenu = CreateMenu();
        HMENU hMainMenu = CreateMenu();

        HMENU hFileMenu = CreatePopupMenu();
        AppendMenu(hFileMenu, MF_STRING, ID_SAVE_FILE, L"Save File");
        AppendMenu(hFileMenu, MF_STRING, ID_LOAD_FILE, L"Load File");
        AppendMenu(hFileMenu, MF_STRING, ID_CLEAR_SCREEN, L"Clear Screen");
        AppendMenu(hMainMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"File");

        HMENU hColorMenu = CreatePopupMenu();
        AppendMenu(hColorMenu, MF_STRING, ID_COLOR_RED, L"Red");
        AppendMenu(hColorMenu, MF_STRING, ID_COLOR_GREEN, L"Green");
        AppendMenu(hColorMenu, MF_STRING, ID_COLOR_BLUE, L"Blue");
        AppendMenu(hColorMenu, MF_STRING, ID_COLOR_YELLOW, L"Yellow");
        AppendMenu(hColorMenu, MF_SEPARATOR, 0, NULL);
        HMENU hBgMenu = CreatePopupMenu();
        AppendMenu(hBgMenu, MF_STRING, ID_BG_WHITE, L"White");
        AppendMenu(hBgMenu, MF_STRING, ID_BG_BLACK, L"Black");
        AppendMenu(hColorMenu, MF_POPUP, (UINT_PTR)hBgMenu, L"Background");
        AppendMenu(hMainMenu, MF_POPUP, (UINT_PTR)hColorMenu, L"Colors");

        HMENU hShapesMenu = CreatePopupMenu();
        HMENU hLineMenu = CreatePopupMenu();
        AppendMenu(hLineMenu, MF_STRING, ID_LINE_DDA, L"DDA Line");
        AppendMenu(hLineMenu, MF_STRING, ID_LINE_MIDPOINT, L"Midpoint Line");
        AppendMenu(hLineMenu, MF_STRING, ID_LINE_PARAMETRIC, L"Parametric Line");
        AppendMenu(hShapesMenu, MF_POPUP, (UINT_PTR)hLineMenu, L"Lines");

        HMENU hCircleMenu = CreatePopupMenu();
        AppendMenu(hCircleMenu, MF_STRING, ID_CIRCLE_DIRECT, L"Direct Circle");
        AppendMenu(hCircleMenu, MF_STRING, ID_CIRCLE_POLAR, L"Polar Circle");
        AppendMenu(hCircleMenu, MF_STRING, ID_CIRCLE_ITER_POLAR, L"Iterative Polar Circle");
        AppendMenu(hCircleMenu, MF_STRING, ID_CIRCLE_MIDPOINT, L"Midpoint Circle");
        AppendMenu(hCircleMenu, MF_STRING, ID_CIRCLE_MODIFIED, L"Modified Midpoint Circle");
        AppendMenu(hShapesMenu, MF_POPUP, (UINT_PTR)hCircleMenu, L"Circles");

        HMENU hEllipseMenu = CreatePopupMenu();
        AppendMenu(hEllipseMenu, MF_STRING, ID_ELLIPSE_DIRECT, L"Direct Ellipse");
        AppendMenu(hEllipseMenu, MF_STRING, ID_ELLIPSE_POLAR, L"Polar Ellipse");
        AppendMenu(hEllipseMenu, MF_STRING, ID_ELLIPSE_MIDPOINT, L"Midpoint Ellipse");
        AppendMenu(hShapesMenu, MF_POPUP, (UINT_PTR)hEllipseMenu, L"Ellipses");
        AppendMenu(hShapesMenu, MF_SEPARATOR, 0, NULL);

        AppendMenu(hShapesMenu, MF_STRING, ID_SHAPE_POINT, L"Point");
        AppendMenu(hShapesMenu, MF_STRING, ID_SHAPE_POLYGON_OUTLINE, L"Polygon (Outline)");
        AppendMenu(hShapesMenu, MF_STRING, ID_POLYGON_CONVEX, L"Polygon (Filled)");
        AppendMenu(hShapesMenu, MF_STRING, ID_SQUARE_HERMITE, L"Square (Hermite Fill)");
        AppendMenu(hShapesMenu, MF_STRING, ID_RECT_BEZIER, L"Rectangle (Bezier Fill)");
        AppendMenu(hShapesMenu, MF_STRING, ID_CARDINAL_SPLINE, L"Cardinal Spline");
        AppendMenu(hMainMenu, MF_POPUP, (UINT_PTR)hShapesMenu, L"Shapes");

        HMENU hFillsMenu = CreatePopupMenu();
        HMENU hCircleFillLines = CreatePopupMenu();
        AppendMenu(hCircleFillLines, MF_STRING, ID_CIRCLE_FILL_LINES_Q1, L"Quarter 1");
        AppendMenu(hCircleFillLines, MF_STRING, ID_CIRCLE_FILL_LINES_Q2, L"Quarter 2");
        AppendMenu(hCircleFillLines, MF_STRING, ID_CIRCLE_FILL_LINES_Q3, L"Quarter 3");
        AppendMenu(hCircleFillLines, MF_STRING, ID_CIRCLE_FILL_LINES_Q4, L"Quarter 4");
        AppendMenu(hFillsMenu, MF_POPUP, (UINT_PTR)hCircleFillLines, L"Circle Fill (Lines)");

        HMENU hCircleFillArcs = CreatePopupMenu();
        AppendMenu(hCircleFillArcs, MF_STRING, ID_CIRCLE_FILL_ARCS_Q1, L"Quarter 1");
        AppendMenu(hCircleFillArcs, MF_STRING, ID_CIRCLE_FILL_ARCS_Q2, L"Quarter 2");
        AppendMenu(hCircleFillArcs, MF_STRING, ID_CIRCLE_FILL_ARCS_Q3, L"Quarter 3");
        AppendMenu(hCircleFillArcs, MF_STRING, ID_CIRCLE_FILL_ARCS_Q4, L"Quarter 4");
        AppendMenu(hFillsMenu, MF_POPUP, (UINT_PTR)hCircleFillArcs, L"Circle Fill (Arcs)");
        AppendMenu(hFillsMenu, MF_SEPARATOR, 0, NULL);
        AppendMenu(hFillsMenu, MF_STRING, ID_FLOOD_RECURSIVE, L"Recursive Flood Fill");
        AppendMenu(hFillsMenu, MF_STRING, ID_FLOOD_ITERATIVE, L"Iterative Flood Fill");
        AppendMenu(hMainMenu, MF_POPUP, (UINT_PTR)hFillsMenu, L"Fills");

        HMENU hClipMenu = CreatePopupMenu();
        AppendMenu(hClipMenu, MF_STRING, ID_DEFINE_CLIP_RECT, L"Define Clipping Window");
        AppendMenu(hClipMenu, MF_STRING, ID_APPLY_POINT_CLIP, L"Apply Point Clipping");
        AppendMenu(hClipMenu, MF_STRING, ID_APPLY_LINE_CLIP, L"Apply Line Clipping");
        AppendMenu(hClipMenu, MF_STRING, ID_APPLY_POLYGON_CLIP, L"Apply Polygon Clipping");
        AppendMenu(hClipMenu, MF_SEPARATOR, 0, NULL);
        AppendMenu(hClipMenu, MF_STRING, ID_CLEAR_CLIPPING, L"Clear Clipping");
        AppendMenu(hMainMenu, MF_POPUP, (UINT_PTR)hClipMenu, L"Clipping");

        HMENU hCursorMenu = CreatePopupMenu();
        AppendMenu(hCursorMenu, MF_STRING, ID_CURSOR_CROSS, L"Cross");
        AppendMenu(hCursorMenu, MF_STRING, ID_CURSOR_ARROW, L"Arrow");
        AppendMenu(hCursorMenu, MF_STRING, ID_CURSOR_HAND, L"Hand");
        AppendMenu(hMainMenu, MF_POPUP, (UINT_PTR)hCursorMenu, L"Cursor");

        SetMenu(hwnd, hMainMenu);
        break;
    }
    case WM_COMMAND:
    {
        switch (LOWORD(wParam)) {
        case ID_COLOR_RED: g_selectedColor = RGB(255, 0, 0); g_colorSelected = true; g_fillMode = false; g_selectedFillType = FILL_TYPE_NONE; break;
        case ID_COLOR_GREEN: g_selectedColor = RGB(0, 255, 0); g_colorSelected = true; g_fillMode = false; g_selectedFillType = FILL_TYPE_NONE; break;
        case ID_COLOR_BLUE: g_selectedColor = RGB(0, 0, 255); g_colorSelected = true; g_fillMode = false; g_selectedFillType = FILL_TYPE_NONE; break;
        case ID_COLOR_YELLOW: g_selectedColor = RGB(255, 255, 0); g_colorSelected = true; g_fillMode = false; g_selectedFillType = FILL_TYPE_NONE; break;
        case ID_CLEAR_SCREEN: ClearScreen(hwnd); break;
        case ID_BG_WHITE: DeleteObject(g_hbrBackground); g_hbrBackground = CreateSolidBrush(RGB(255, 255, 255)); InvalidateRect(hwnd, NULL, TRUE); break;
        case ID_BG_BLACK: DeleteObject(g_hbrBackground); g_hbrBackground = CreateSolidBrush(RGB(0, 0, 0)); InvalidateRect(hwnd, NULL, TRUE); break;
        case ID_SAVE_FILE: SaveToFile(hwnd); break;
        case ID_LOAD_FILE: LoadFromFile(hwnd); break;

        case ID_CURSOR_CROSS: g_hCursor = LoadCursor(NULL, IDC_CROSS); break;
        case ID_CURSOR_ARROW: g_hCursor = LoadCursor(NULL, IDC_ARROW); break;
        case ID_CURSOR_HAND: g_hCursor = LoadCursor(NULL, IDC_HAND); break;

        case ID_DEFINE_CLIP_RECT: g_isDefiningClipRect = true; g_fillMode = false; g_selectedFillType = FILL_TYPE_NONE; cout << "Define clipping window mode enabled.\n"; break;
        case ID_CLEAR_CLIPPING: ClearClipping(hwnd); break;

        case ID_APPLY_POINT_CLIP:
            if (g_userClipRectDefined) {
                vector<Shape> final_shapes;
                for (const auto& shape : g_shapes) {
                    if (shape.type == ID_SHAPE_POINT) {
                        if (ClipPoint(shape.points[0], g_userDefinedClipRect)) {
                            final_shapes.push_back(shape);
                        }
                    }
                    else {
                        final_shapes.push_back(shape);
                    }
                }
                g_shapes = final_shapes;
                InvalidateRect(hwnd, NULL, TRUE);
            }
            else {
                MessageBox(hwnd, L"Please define a clipping window first!", L"Error", MB_OK | MB_ICONWARNING);
            }
            break;

        case ID_APPLY_LINE_CLIP:
            if (g_userClipRectDefined) {
                vector<Shape> final_shapes;
                for (const auto& shape : g_shapes) {
                    if ((shape.type >= ID_LINE_DDA && shape.type <= ID_LINE_PARAMETRIC) && shape.points.size() >= 2) {
                        Point p1 = shape.points[0];
                        Point p2 = shape.points[1];
                        if (ClipLine(p1, p2, g_userDefinedClipRect)) {
                            Shape new_shape = shape;
                            new_shape.points = { p1, p2 };
                            final_shapes.push_back(new_shape);
                        }
                    }
                    else {
                        final_shapes.push_back(shape);
                    }
                }
                g_shapes = final_shapes;
                InvalidateRect(hwnd, NULL, TRUE);
            }
            else {
                MessageBox(hwnd, L"Please define a clipping window first!", L"Error", MB_OK | MB_ICONWARNING);
            }
            break;

        case ID_APPLY_POLYGON_CLIP:
            if (g_userClipRectDefined) {
                vector<Shape> final_shapes;
                for (const auto& shape : g_shapes) {
                    if ((shape.type == ID_POLYGON_CONVEX || shape.type == ID_SHAPE_POLYGON_OUTLINE) && shape.points.size() >= 3) {
                        RECT clipRect = { g_userDefinedClipRect.xmin, g_userDefinedClipRect.ymin, g_userDefinedClipRect.xmax, g_userDefinedClipRect.ymax };
                        vector<Point> clipped_poly = ClipPolygon(shape.points, clipRect);
                        if (clipped_poly.size() >= 3) {
                            Shape new_shape = shape;
                            new_shape.points = clipped_poly;
                            final_shapes.push_back(new_shape);
                        }
                    }
                    else {
                        final_shapes.push_back(shape);
                    }
                }
                g_shapes = final_shapes;
                InvalidateRect(hwnd, NULL, TRUE);
            }
            else {
                MessageBox(hwnd, L"Please define a clipping window first!", L"Error", MB_OK | MB_ICONWARNING);
            }
            break;

        case ID_LINE_DDA: g_selectedShape = ID_LINE_DDA; g_fillMode = false; g_selectedFillType = FILL_TYPE_NONE; break;
        case ID_LINE_MIDPOINT: g_selectedShape = ID_LINE_MIDPOINT; g_fillMode = false; g_selectedFillType = FILL_TYPE_NONE; break;
        case ID_LINE_PARAMETRIC: g_selectedShape = ID_LINE_PARAMETRIC; g_fillMode = false; g_selectedFillType = FILL_TYPE_NONE; break;
        case ID_CIRCLE_DIRECT: g_selectedShape = ID_CIRCLE_DIRECT; g_fillMode = false; g_selectedFillType = FILL_TYPE_NONE; break;
        case ID_CIRCLE_POLAR: g_selectedShape = ID_CIRCLE_POLAR; g_fillMode = false; g_selectedFillType = FILL_TYPE_NONE; break;
        case ID_CIRCLE_ITER_POLAR: g_selectedShape = ID_CIRCLE_ITER_POLAR; g_fillMode = false; g_selectedFillType = FILL_TYPE_NONE; break;
        case ID_CIRCLE_MIDPOINT: g_selectedShape = ID_CIRCLE_MIDPOINT; g_fillMode = false; g_selectedFillType = FILL_TYPE_NONE; break;
        case ID_CIRCLE_MODIFIED: g_selectedShape = ID_CIRCLE_MODIFIED; g_fillMode = false; g_selectedFillType = FILL_TYPE_NONE; break;
        case ID_CIRCLE_FILL_LINES_Q1: g_fillMode = true; g_fillQuarter = 1; g_selectedShape = 0; g_selectedSubtype = ID_CIRCLE_FILL_LINES_Q1; g_selectedFillType = FILL_TYPE_LINES; break;
        case ID_CIRCLE_FILL_LINES_Q2: g_fillMode = true; g_fillQuarter = 2; g_selectedShape = 0; g_selectedSubtype = ID_CIRCLE_FILL_LINES_Q2; g_selectedFillType = FILL_TYPE_LINES; break;
        case ID_CIRCLE_FILL_LINES_Q3: g_fillMode = true; g_fillQuarter = 3; g_selectedShape = 0; g_selectedSubtype = ID_CIRCLE_FILL_LINES_Q3; g_selectedFillType = FILL_TYPE_LINES; break;
        case ID_CIRCLE_FILL_LINES_Q4: g_fillMode = true; g_fillQuarter = 4; g_selectedShape = 0; g_selectedSubtype = ID_CIRCLE_FILL_LINES_Q4; g_selectedFillType = FILL_TYPE_LINES; break;
        case ID_CIRCLE_FILL_ARCS_Q1: g_fillMode = true; g_fillQuarter = 1; g_selectedShape = 0; g_selectedSubtype = ID_CIRCLE_FILL_ARCS_Q1; g_selectedFillType = FILL_TYPE_ARCS; break;
        case ID_CIRCLE_FILL_ARCS_Q2: g_fillMode = true; g_fillQuarter = 2; g_selectedShape = 0; g_selectedSubtype = ID_CIRCLE_FILL_ARCS_Q2; g_selectedFillType = FILL_TYPE_ARCS; break;
        case ID_CIRCLE_FILL_ARCS_Q3: g_fillMode = true; g_fillQuarter = 3; g_selectedShape = 0; g_selectedSubtype = ID_CIRCLE_FILL_ARCS_Q3; g_selectedFillType = FILL_TYPE_ARCS; break;
        case ID_CIRCLE_FILL_ARCS_Q4: g_fillMode = true; g_fillQuarter = 4; g_selectedShape = 0; g_selectedSubtype = ID_CIRCLE_FILL_ARCS_Q4; g_selectedFillType = FILL_TYPE_ARCS; break;
        case ID_SQUARE_HERMITE: g_selectedShape = ID_SQUARE_HERMITE; g_fillMode = false; g_selectedFillType = FILL_TYPE_NONE; break;
        case ID_RECT_BEZIER: g_selectedShape = ID_RECT_BEZIER; g_fillMode = false; g_selectedFillType = FILL_TYPE_NONE; break;
        case ID_POLYGON_CONVEX: g_selectedShape = ID_POLYGON_CONVEX; g_tempPolygonPoints.clear(); g_fillMode = false; g_selectedFillType = FILL_TYPE_NONE; break;
        case ID_CARDINAL_SPLINE: g_selectedShape = ID_CARDINAL_SPLINE; g_tempControlPoints.clear(); g_fillMode = false; g_selectedFillType = FILL_TYPE_NONE; break;
        case ID_ELLIPSE_DIRECT: g_selectedShape = ID_ELLIPSE_DIRECT; g_fillMode = false; g_selectedFillType = FILL_TYPE_NONE; break;
        case ID_ELLIPSE_POLAR: g_selectedShape = ID_ELLIPSE_POLAR; g_fillMode = false; g_selectedFillType = FILL_TYPE_NONE; break;
        case ID_ELLIPSE_MIDPOINT: g_selectedShape = ID_ELLIPSE_MIDPOINT; g_fillMode = false; g_selectedFillType = FILL_TYPE_NONE; break;
        case ID_FLOOD_RECURSIVE: g_selectedShape = 0; g_selectedSubtype = ID_FLOOD_RECURSIVE; g_floodFillMode = true; g_fillMode = false; g_selectedFillType = FILL_TYPE_NONE; break;
        case ID_FLOOD_ITERATIVE: g_selectedShape = 0; g_selectedSubtype = ID_FLOOD_ITERATIVE; g_floodFillMode = true; g_fillMode = false; g_selectedFillType = FILL_TYPE_NONE; break;
        case ID_SHAPE_POINT: g_selectedShape = ID_SHAPE_POINT; g_fillMode = false; g_selectedFillType = FILL_TYPE_NONE; break;
        case ID_SHAPE_POLYGON_OUTLINE: g_selectedShape = ID_SHAPE_POLYGON_OUTLINE; g_tempPolygonPoints.clear(); g_fillMode = false; g_selectedFillType = FILL_TYPE_NONE; break;
        }
        if (g_selectedShape != 0 || g_fillMode || g_floodFillMode) cout << "Selected mode. Shape ID: " << g_selectedShape << ", Fill Mode: " << g_fillMode << ", Flood Fill Mode: " << g_floodFillMode << endl;
        break;
    }
    case WM_LBUTTONDOWN:
    {
        if (g_isDefiningClipRect) {
            g_isDrawing = true;
            g_tempClipRect.xmin = LOWORD(lParam);
            g_tempClipRect.ymin = HIWORD(lParam);
            g_tempClipRect.xmax = LOWORD(lParam);
            g_tempClipRect.ymax = HIWORD(lParam);
            SetCapture(hwnd);
            break;
        }

        if (g_fillMode) {
            int clickX = LOWORD(lParam);
            int clickY = HIWORD(lParam);
            g_fillShapeIndex = -1;
            for (size_t i = 0; i < g_shapes.size(); ++i) {
                const auto& s = g_shapes[i];
                if (s.type >= ID_CIRCLE_DIRECT && s.type <= ID_CIRCLE_MODIFIED) {
                    int dist_sq = (clickX - s.center.x) * (clickX - s.center.x) + (clickY - s.center.y) * (clickY - s.center.y);
                    if (dist_sq <= s.size1 * s.size1) {
                        g_fillShapeIndex = i;
                        g_isDrawing = true;
                        break;
                    }
                }
            }
            if (g_fillShapeIndex == -1) {
                MessageBox(hwnd, L"No circle found at this location to fill.", L"Information", MB_OK);
                g_fillMode = false;
                g_selectedFillType = FILL_TYPE_NONE;
            }
            else {
                SetCapture(hwnd);
            }
            break;
        }

        if (g_selectedShape == 0 && !g_floodFillMode) { MessageBox(hwnd, L"Please select a shape!", L"Error", MB_OK); break; }

        g_startPoint.x = LOWORD(lParam);
        g_startPoint.y = HIWORD(lParam);
        g_isDrawing = true;

        if (g_selectedShape == ID_POLYGON_CONVEX || g_selectedShape == ID_SHAPE_POLYGON_OUTLINE) {
            g_tempPolygonPoints.push_back(g_startPoint);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (g_selectedShape == ID_CARDINAL_SPLINE) {
            g_tempControlPoints.push_back(g_startPoint);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (g_selectedShape == ID_SHAPE_POINT) {
            Shape shape;
            shape.type = ID_SHAPE_POINT;
            shape.color = g_selectedColor;
            shape.points.push_back(g_startPoint);
            shape.filledQuartersMask = 0;
            g_shapes.push_back(shape);
            g_isDrawing = false;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (g_floodFillMode) {

        }
        else {
            SetCapture(hwnd);
        }
        break;
    }
    case WM_MOUSEMOVE:
    {
        if (g_isDrawing && g_isDefiningClipRect) {
            g_tempClipRect.xmax = LOWORD(lParam);
            g_tempClipRect.ymax = HIWORD(lParam);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if ((g_selectedShape == ID_POLYGON_CONVEX || g_selectedShape == ID_SHAPE_POLYGON_OUTLINE) && g_tempPolygonPoints.size() > 0) {
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else if (g_selectedShape == ID_CARDINAL_SPLINE && g_tempControlPoints.size() > 0) {
            InvalidateRect(hwnd, NULL, FALSE);
        }
        break;
    }
    case WM_LBUTTONUP:
    {
        if (g_isDrawing && g_isDefiningClipRect) {
            ReleaseCapture();
            g_isDrawing = false;
            g_isDefiningClipRect = false;
            g_userDefinedClipRect.xmin = min(g_tempClipRect.xmin, g_tempClipRect.xmax);
            g_userDefinedClipRect.ymin = min(g_tempClipRect.ymin, g_tempClipRect.ymax);
            g_userDefinedClipRect.xmax = max(g_tempClipRect.xmin, g_tempClipRect.xmax);
            g_userDefinedClipRect.ymax = max(g_tempClipRect.ymin, g_tempClipRect.ymax);
            g_userClipRectDefined = true;
            InvalidateRect(hwnd, NULL, TRUE);
            break;
        }

        if (g_fillMode && g_fillShapeIndex != -1) {
            ReleaseCapture();
            g_isDrawing = false;
            Shape& circleToFill = g_shapes[g_fillShapeIndex];

            if (g_selectedFillType == FILL_TYPE_LINES) {
                circleToFill.filledQuartersMask |= (1 << (g_fillQuarter - 1));
            }
            else if (g_selectedFillType == FILL_TYPE_ARCS) {
                circleToFill.filledQuartersMask |= (1 << (g_fillQuarter - 1 + 4));
            }

            InvalidateRect(hwnd, NULL, TRUE);
            g_fillMode = false;
            g_fillQuarter = 0;
            g_fillShapeIndex = -1;
            g_selectedFillType = FILL_TYPE_NONE;
            break;
        }

        if (g_isDrawing && !(g_selectedShape == ID_POLYGON_CONVEX || g_selectedShape == ID_SHAPE_POLYGON_OUTLINE || g_selectedShape == ID_CARDINAL_SPLINE)) {
            ReleaseCapture();
            g_isDrawing = false;
            int x2 = LOWORD(lParam);
            int y2 = HIWORD(lParam);

            if (g_floodFillMode) {
                hdc = GetDC(hwnd);
                COLORREF oldColor = GetPixel(hdc, x2, y2);
                if (g_selectedSubtype == ID_FLOOD_RECURSIVE) {
                    FloodFillRecursive(hdc, x2, y2, g_selectedColor, oldColor, hwnd);
                }
                else {
                    FloodFillIterative(hdc, x2, y2, g_selectedColor, oldColor, hwnd);
                }
                ReleaseDC(hwnd, hdc);
                g_floodFillMode = false;
                break;
            }

            Shape shape;
            shape.type = g_selectedShape;
            shape.subtype = 0;
            shape.color = g_selectedColor;
            shape.filledQuartersMask = 0;

            int radius = (int)sqrt(pow(x2 - g_startPoint.x, 2) + pow(y2 - g_startPoint.y, 2));
            int width = abs(x2 - g_startPoint.x);
            int height = abs(y2 - g_startPoint.y);

            switch (g_selectedShape) {
            case ID_LINE_DDA:
            case ID_LINE_MIDPOINT:
            case ID_LINE_PARAMETRIC:
                shape.points = { g_startPoint, {x2, y2} };
                break;
            case ID_CIRCLE_DIRECT:
            case ID_CIRCLE_POLAR:
            case ID_CIRCLE_ITER_POLAR:
            case ID_CIRCLE_MIDPOINT:
            case ID_CIRCLE_MODIFIED:
                shape.center = g_startPoint;
                shape.size1 = radius;
                break;
            case ID_SQUARE_HERMITE:
                shape.points.push_back(g_startPoint);
                shape.size1 = max(width, height);
                break;
            case ID_RECT_BEZIER:
                shape.points.push_back(g_startPoint);
                shape.size1 = width;
                shape.size2 = height;
                break;
            case ID_ELLIPSE_DIRECT:
            case ID_ELLIPSE_POLAR:
            case ID_ELLIPSE_MIDPOINT:
                shape.center = g_startPoint;
                shape.size1 = width;
                shape.size2 = height;
                break;
            }

            if (g_selectedShape != ID_POLYGON_CONVEX && g_selectedShape != ID_SHAPE_POLYGON_OUTLINE && g_selectedShape != ID_CARDINAL_SPLINE && g_selectedShape != ID_SHAPE_POINT) {
                g_shapes.push_back(shape);
            }
            InvalidateRect(hwnd, NULL, FALSE);
        }
        break;
    }
    case WM_RBUTTONDOWN:
    {
        if (g_selectedShape == ID_POLYGON_CONVEX || g_selectedShape == ID_SHAPE_POLYGON_OUTLINE) {
            if (g_tempPolygonPoints.size() >= 3) {
                Shape shape;
                shape.type = g_selectedShape;
                shape.color = g_selectedColor;
                shape.points = g_tempPolygonPoints;
                shape.filledQuartersMask = 0;
                g_shapes.push_back(shape);
            }
            g_tempPolygonPoints.clear();
        }
        else if (g_selectedShape == ID_CARDINAL_SPLINE) {
            if (g_tempControlPoints.size() >= 2) {
                Shape shape;
                shape.type = g_selectedShape;
                shape.color = g_selectedColor;
                shape.points = g_tempControlPoints;
                shape.tension = g_tension;
                shape.filledQuartersMask = 0;
                g_shapes.push_back(shape);
            }
            g_tempControlPoints.clear();
        }
        g_isDrawing = false;
        InvalidateRect(hwnd, NULL, TRUE);
        break;
    }
    case WM_PAINT:
    {
        hdc = BeginPaint(hwnd, &ps);
        FillRect(hdc, &ps.rcPaint, g_hbrBackground);

        if ((g_selectedShape == ID_POLYGON_CONVEX || g_selectedShape == ID_SHAPE_POLYGON_OUTLINE) && g_tempPolygonPoints.size() > 0 && g_isDrawing) {
            HPEN tempPen = CreatePen(PS_SOLID, 1, g_selectedColor);
            HPEN oldTempPen = (HPEN)SelectObject(hdc, tempPen);
            for (size_t i = 0; i < g_tempPolygonPoints.size(); ++i) {
                SetPixel(hdc, g_tempPolygonPoints[i].x, g_tempPolygonPoints[i].y, g_selectedColor);
                if (i > 0) {
                    DrawLineDDA(hdc, g_tempPolygonPoints[i - 1].x, g_tempPolygonPoints[i - 1].y, g_tempPolygonPoints[i].x, g_tempPolygonPoints[i].y, g_selectedColor);
                }
            }
            POINT currentMousePos;
            GetCursorPos(&currentMousePos);
            ScreenToClient(hwnd, &currentMousePos);
            DrawLineDDA(hdc, g_tempPolygonPoints.back().x, g_tempPolygonPoints.back().y, currentMousePos.x, currentMousePos.y, g_selectedColor);
            SelectObject(hdc, oldTempPen);
            DeleteObject(tempPen);
        }

        if (g_selectedShape == ID_CARDINAL_SPLINE && g_tempControlPoints.size() > 0 && g_isDrawing) {
            HPEN tempPen = CreatePen(PS_SOLID, 1, g_selectedColor);
            HPEN oldTempPen = (HPEN)SelectObject(hdc, tempPen);
            for (size_t i = 0; i < g_tempControlPoints.size(); ++i) {
                Ellipse(hdc, g_tempControlPoints[i].x - 3, g_tempControlPoints[i].y - 3, g_tempControlPoints[i].x + 3, g_tempControlPoints[i].y + 3);
                if (i > 0) {
                    DrawLineDDA(hdc, g_tempControlPoints[i - 1].x, g_tempControlPoints[i - 1].y, g_tempControlPoints[i].x, g_tempControlPoints[i].y, g_selectedColor);
                }
            }
            POINT currentMousePos;
            GetCursorPos(&currentMousePos);
            ScreenToClient(hwnd, &currentMousePos);
            DrawLineDDA(hdc, g_tempControlPoints.back().x, g_tempControlPoints.back().y, currentMousePos.x, currentMousePos.y, g_selectedColor);
            SelectObject(hdc, oldTempPen);
            DeleteObject(tempPen);
        }

        for (const auto& shape : g_shapes) {
            HPEN pen = CreatePen(PS_SOLID, 1, shape.color);
            HPEN oldPen = (HPEN)SelectObject(hdc, pen);

            switch (shape.type) {
            case ID_LINE_DDA: DrawLineDDA(hdc, shape.points[0].x, shape.points[0].y, shape.points[1].x, shape.points[1].y, shape.color); break;
            case ID_LINE_MIDPOINT: DrawLineMidpoint(hdc, shape.points[0].x, shape.points[0].y, shape.points[1].x, shape.points[1].y, shape.color); break;
            case ID_LINE_PARAMETRIC: DrawLineParametric(hdc, shape.points[0].x, shape.points[0].y, shape.points[1].x, shape.points[1].y, shape.color); break;

            case ID_CIRCLE_DIRECT:
            case ID_CIRCLE_POLAR:
            case ID_CIRCLE_ITER_POLAR:
            case ID_CIRCLE_MIDPOINT:
            case ID_CIRCLE_MODIFIED:
            {
                if (shape.type == ID_CIRCLE_DIRECT) DrawCircleDirect(hdc, shape.center.x, shape.center.y, shape.size1, shape.color);
                else if (shape.type == ID_CIRCLE_POLAR) DrawCirclePolar(hdc, shape.center.x, shape.center.y, shape.size1, shape.color);
                else if (shape.type == ID_CIRCLE_ITER_POLAR) DrawCircleIterativePolar(hdc, shape.center.x, shape.center.y, shape.size1, shape.color);
                else if (shape.type == ID_CIRCLE_MIDPOINT) DrawCircleMidpoint(hdc, shape.center.x, shape.center.y, shape.size1, shape.color);
                else if (shape.type == ID_CIRCLE_MODIFIED) DrawCircleModified(hdc, shape.center.x, shape.center.y, shape.size1, shape.color);

                if (shape.filledQuartersMask & FILL_LINE_Q1_MASK) DrawCircleFillLines(hdc, shape.center.x, shape.center.y, shape.size1, shape.color, 1);
                if (shape.filledQuartersMask & FILL_LINE_Q2_MASK) DrawCircleFillLines(hdc, shape.center.x, shape.center.y, shape.size1, shape.color, 2);
                if (shape.filledQuartersMask & FILL_LINE_Q3_MASK) DrawCircleFillLines(hdc, shape.center.x, shape.center.y, shape.size1, shape.color, 3);
                if (shape.filledQuartersMask & FILL_LINE_Q4_MASK) DrawCircleFillLines(hdc, shape.center.x, shape.center.y, shape.size1, shape.color, 4);

                if (shape.filledQuartersMask & FILL_ARC_Q1_MASK) DrawCircleFillArcs(hdc, shape.center.x, shape.center.y, shape.size1, shape.color, 1);
                if (shape.filledQuartersMask & FILL_ARC_Q2_MASK) DrawCircleFillArcs(hdc, shape.center.x, shape.center.y, shape.size1, shape.color, 2);
                if (shape.filledQuartersMask & FILL_ARC_Q3_MASK) DrawCircleFillArcs(hdc, shape.center.x, shape.center.y, shape.size1, shape.color, 3);
                if (shape.filledQuartersMask & FILL_ARC_Q4_MASK) DrawCircleFillArcs(hdc, shape.center.x, shape.center.y, shape.size1, shape.color, 4);
                break;
            }
            case ID_ELLIPSE_DIRECT: DrawEllipseDirect(hdc, shape.center.x, shape.center.y, shape.size1, shape.size2, shape.color); break;
            case ID_ELLIPSE_POLAR: DrawEllipsePolar(hdc, shape.center.x, shape.center.y, shape.size1, shape.size2, shape.color); break;
            case ID_ELLIPSE_MIDPOINT: DrawEllipseMidpoint(hdc, shape.center.x, shape.center.y, shape.size1, shape.size2, shape.color); break;

            case ID_SQUARE_HERMITE: FillSquareWithHermite(hdc, shape.points[0].x, shape.points[0].y, shape.size1, shape.color); break;
            case ID_RECT_BEZIER: FillRectangleWithBezier(hdc, shape.points[0].x, shape.points[0].y, shape.size1, shape.size2, shape.color); break;
            case ID_POLYGON_CONVEX: {
                HBRUSH brush = CreateSolidBrush(shape.color);
                HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);
                vector<POINT> pts(shape.points.size());
                for (size_t i = 0; i < shape.points.size(); ++i) pts[i] = { shape.points[i].x, shape.points[i].y };
                Polygon(hdc, pts.data(), pts.size());
                SelectObject(hdc, oldBrush);
                DeleteObject(brush);
                break;
            }
            case ID_SHAPE_POLYGON_OUTLINE:
                if (shape.points.size() >= 2) {
                    MoveToEx(hdc, shape.points[0].x, shape.points[0].y, NULL);
                    for (size_t i = 1; i < shape.points.size(); ++i) LineTo(hdc, shape.points[i].x, shape.points[i].y);
                    LineTo(hdc, shape.points[0].x, shape.points[0].y);
                }
                break;
            case ID_CARDINAL_SPLINE: DrawCardinalSpline(hdc, shape.points, shape.tension, g_numSegments, shape.color); break;
            case ID_SHAPE_POINT: SetPixel(hdc, shape.points[0].x, shape.points[0].y, shape.color); break;
            }
            SelectObject(hdc, oldPen);
            DeleteObject(pen);
        }
        if (g_isDefiningClipRect || g_userClipRectDefined) {
            HPEN pen = CreatePen(PS_DOT, 1, RGB(255, 0, 0));
            if (g_userClipRectDefined) {
                DeleteObject(pen);
                pen = CreatePen(PS_DASH, 1, RGB(0, 0, 255));
            }
            HPEN oldPen = (HPEN)SelectObject(hdc, pen);
            SelectObject(hdc, GetStockObject(NULL_BRUSH));
            if (g_isDefiningClipRect) {
                Rectangle(hdc, g_tempClipRect.xmin, g_tempClipRect.ymin, g_tempClipRect.xmax, g_tempClipRect.ymax);
            }
            else if (g_userClipRectDefined) {
                Rectangle(hdc, g_userDefinedClipRect.xmin, g_userDefinedClipRect.ymin, g_userDefinedClipRect.xmax, g_userDefinedClipRect.ymax);
            }
            SelectObject(hdc, oldPen);
            DeleteObject(pen);
        }
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            SetCursor(g_hCursor);
            return TRUE;
        }
        break;
    case WM_DESTROY:
        DeleteObject(g_hbrBackground);
        DestroyMenu(g_hMenu);
        FreeConsole();
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"GraphicsApp";
    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, L"Window Registration Failed!", L"Error", MB_OK);
        return 0;
    }
    HWND hwnd = CreateWindow(
        L"GraphicsApp", L"Graphics Application",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 700, 500,
        NULL, NULL, hInstance, NULL
    );
    if (!hwnd) {
        MessageBox(NULL, L"Window Creation Failed!", L"Error", MB_OK);
        return 0;
    }
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}