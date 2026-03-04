#include <windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <functional>
#include <algorithm>


using namespace std;
struct Point3D { double x, y, z; };
struct Point2D { int x, y; };
using ScreenBuffer = vector<string>;
constexpr double PI = 3.14159265358979323846;

auto createBuffer = [](int w, int h) -> ScreenBuffer {
    return ScreenBuffer(h, string(w, ' '));
};
auto clearBuffer = [](ScreenBuffer& buf) {
    for (auto& row : buf) fill(row.begin(), row.end(), ' ');
};
auto setPixel = [](ScreenBuffer& buf, int x, int y, char c) {
    if (y >= 0 && y < (int)buf.size() && x >= 0 && x < (int)buf[0].size())
        buf[y][x] = c;
};

auto drawLineToBuffer = [](ScreenBuffer& buf, int x1, int y1, int x2, int y2, char s){
    int dx = abs(x2-x1), dy = abs(y2-y1);
    int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    int x = x1, y = y1;
    while (true) {
        setPixel(buf, x, y, s);
        if (x == x2 && y == y2) break;
        int e2 = 2*err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 <  dx) { err += dx; y += sy; }
    }
};

auto drawBuffer = [](const ScreenBuffer& buffer){
    COORD c{0,0}; HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleCursorPosition(h, c);
    string out;
    int H = buffer.size(), W = buffer.empty() ? 0 : buffer[0].size();
    out.reserve(W*H + H);
    for (const auto& row : buffer) { out.append(row.begin(), row.end()); out.push_back('\n'); }
    cout << out;
};

auto rotateX = [](Point3D p, double ang)->Point3D {
    double s = sin(ang), c = cos(ang);
    return { p.x, p.y * c - p.z * s, p.y * s + p.z * c };
};
auto rotateY = [](Point3D p, double ang)->Point3D {
    double s = sin(ang), c = cos(ang);
    return { p.x * c + p.z * s, p.y, -p.x * s + p.z * c };
};

auto project = [](const Point3D& p, int w, int h, double fov, double vz) -> Point2D {
    double focal = (w / 2.0) / tan(fov * PI / 360.0);
    double z = p.z + vz;
    if (z == 0) z = 1e-6;
    double x = (p.x * focal) / z + w / 2.0;
    double y = (p.y * focal) / z + h / 2.0;
    return { static_cast<int>(round(x)), static_cast<int>(round(y)) };
};

auto composeTransformations = [](initializer_list<function<Point3D(Point3D)>> funcs) {
    return [funcs](Point3D p){
        Point3D cur = p;
        for (auto &f : funcs) cur = f(cur);
        return cur;
    };
};

auto makeCubeVertices = [](double size)->vector<Point3D>{
    double h = size / 2.0;
    return {
        {-h,-h,-h},{ h,-h,-h},{ h, h,-h},{-h, h,-h},
        {-h,-h, h},{ h,-h, h},{ h, h, h},{-h, h, h}
    };
};

auto makeCubeEdges = []()->vector<pair<int,int>>{
    return {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };
};


auto generateAllFacePoints = [](const vector<Point3D>& verts, int density)->vector<Point3D>{
    double minx = verts[0].x, maxx = verts[0].x;
    double miny = verts[0].y, maxy = verts[0].y;
    double minz = verts[0].z, maxz = verts[0].z;
    for (const auto& v : verts) {
        minx = min(minx, v.x); maxx = max(maxx, v.x);
        miny = min(miny, v.y); maxy = max(maxy, v.y);
        minz = min(minz, v.z); maxz = max(maxz, v.z);
    }

    vector<Point3D> pts;
    pts.reserve(6 * max(1, density) * max(1, density));
    auto linspace = [](double a, double b, int n, int i)->double {
        if (n == 1) return (a + b) / 2.0;
        return a + (b - a) * (double)i / (double)(n - 1);
    };


    for (int side = 0; side < 6; ++side) {
        double fixedVal;
        int axisFixed, axU, axV;
        if (side == 0) { axisFixed = 0; fixedVal = minx; axU = 1; axV = 2; }
        if (side == 1) { axisFixed = 0; fixedVal = maxx; axU = 1; axV = 2; }
        if (side == 2) { axisFixed = 1; fixedVal = miny; axU = 0; axV = 2; }


if (side == 3) { axisFixed = 1; fixedVal = maxy; axU = 0; axV = 2; }
        if (side == 4) { axisFixed = 2; fixedVal = minz; axU = 0; axV = 1; }
        if (side == 5) { axisFixed = 2; fixedVal = maxz; axU = 0; axV = 1; }

        double minU = (axU == 0 ? minx : (axU == 1 ? miny : minz));
        double maxU = (axU == 0 ? maxx : (axU == 1 ? maxy : maxz));
        double minV = (axV == 0 ? minx : (axV == 1 ? miny : minz));
        double maxV = (axV == 0 ? maxx : (axV == 1 ? maxy : maxz));

        for (int iu = 0; iu < max(1, density); ++iu) {
            double u = linspace(minU, maxU, density, iu);
            for (int iv = 0; iv < max(1, density); ++iv) {
                double v = linspace(minV, maxV, density, iv);
                Point3D p{0,0,0};
                double *fp = &p.x;
                fp[axisFixed] = fixedVal;
                fp[axU] = u;
                fp[axV] = v;
                pts.push_back(p);
            }
        }
    }

    return pts;
};

auto renderCube = [](
    const vector<Point3D>& vertices,
    const vector<Point3D>& facePoints,
    const vector<pair<int, int>>& edges,
    double angleX, double angleY,
    int screenW, int screenH, double fov, double vz
) -> pair<vector<Point2D>, vector<pair<Point2D, Point2D>>> {

    auto transform = composeTransformations({
        [angleY](Point3D p) { return rotateY(p, angleY); },
        [angleX](Point3D p) { return rotateX(p, angleX); }
    });

    vector<Point3D> transformedVerts; transformedVerts.reserve(vertices.size());
    for (auto &v : vertices) transformedVerts.push_back(transform(v));

    vector<Point3D> transformedFacePoints; transformedFacePoints.reserve(facePoints.size());
    for (auto &p : facePoints) transformedFacePoints.push_back(transform(p));

    auto proj = [screenW, screenH, fov, vz](const Point3D& p){ return project(p, screenW, screenH, fov, vz); };

    vector<Point2D> projFacePoints; projFacePoints.reserve(transformedFacePoints.size());
    for (auto &p : transformedFacePoints) projFacePoints.push_back(proj(p));

    vector<Point2D> projVerts; projVerts.reserve(transformedVerts.size());
    for (auto &v : transformedVerts) projVerts.push_back(proj(v));

    vector<pair<Point2D, Point2D>> lines; lines.reserve(edges.size());
    for (auto &e : edges) lines.emplace_back(projVerts[e.first], projVerts[e.second]);

    return { move(projFacePoints), move(lines) };
};

auto setupConsole = [](){
    SetConsoleOutputCP(65001);
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi; GetConsoleScreenBufferInfo(h, &csbi);
    csbi.dwSize.X = 120; csbi.dwSize.Y = 50; SetConsoleScreenBufferSize(h, csbi.dwSize);
    CONSOLE_CURSOR_INFO ci; GetConsoleCursorInfo(h, &ci); ci.bVisible = FALSE; SetConsoleCursorInfo(h, &ci);
};

auto renderSceneToBuffer = [](ScreenBuffer& buf, const vector<Point2D>& facePts, const vector<pair<Point2D,Point2D>>& lines, char fillSymbol, char lineSymbol){
    for (const auto& p : facePts) setPixel(buf, p.x, p.y, fillSymbol);
    for (const auto& l : lines) drawLineToBuffer(buf, l.first.x, l.first.y, l.second.x, l.second.y, lineSymbol);
};

int main() {
    const int W = 240, H = 120;
    const double SIZE = 4.0, FOV = 90.0, VZ = 10.0;
    const char LINE_SYMBOL = '#', FILL_SYMBOL = '.';
    const int FACE_DENSITY = 12;

    auto vertices = makeCubeVertices(SIZE);
    auto edges = makeCubeEdges();
    auto facePoints = generateAllFacePoints(vertices, FACE_DENSITY);

    setupConsole();
    auto buffer = createBuffer(W, H);

    double angleX = 0.0, angleY = 0.0;

    function<void(double,double)> animationLoop = [&](double ax, double ay){
        clearBuffer(buffer);
        auto [pts2d, lines] = renderCube(vertices, facePoints, edges, ax, ay, W, H, FOV, VZ);
        renderSceneToBuffer(buffer, pts2d, lines, FILL_SYMBOL, LINE_SYMBOL);
        drawBuffer(buffer);
        Sleep(50);
        animationLoop(ax + 0.04, ay + 0.04 * 0.6);
    };

    animationLoop(angleX, angleY);
    return 0;
}