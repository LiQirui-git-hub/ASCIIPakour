#define UNICODE
#define _UNICODE
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <chrono>
#include <thread>
#include <algorithm>
#include <vector>
#include <string>
#include <fstream>
#include <omp.h>
struct vec2 {
    float x, y;
    vec2(float v) : x(v), y(v) {}
    vec2(float a, float b) : x(a), y(b) {}
    vec2 operator+(vec2 o) const { return {x + o.x, y + o.y}; }
    vec2 operator-(vec2 o) const { return {x - o.x, y - o.y}; }
    vec2 operator*(vec2 o) const { return {x * o.x, y * o.y}; }
    vec2 operator*(float s) const { return {x * s, y * s}; }
    friend vec2 operator*(float s, const vec2& v) { return v * s; }
};

struct vec3 {
    float x, y, z;
    vec3() : x(0), y(0), z(0) {}
    vec3(float v) : x(v), y(v), z(v) {}
    vec3(float a, float b, float c) : x(a), y(b), z(c) {}
    vec3(float a, vec2 v) : x(a), y(v.x), z(v.y) {}
    vec3 operator+(vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    vec3 operator-(vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    vec3 operator*(vec3 o) const { return {x * o.x, y * o.y, z * o.z}; }
    vec3 operator/(vec3 o) const { return {x / o.x, y / o.y, z / o.z}; }
    vec3 operator-() const { return {-x, -y, -z}; }
    vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    friend vec3 operator*(float s, const vec3& v) { return v * s; }
};

inline float clamp(float v, float a, float b) { return v < b ? (v > a ? v : a) : b; }
inline float sign(float a) { return (0 < a) - (a < 0); }
inline float length(vec3 v) { return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z); }
inline vec3 norm(vec3 v) {
    float len = length(v);
    if (len < 1e-8f) return {0, 1, 0};
    return v * (1.0f / len);
}
inline float dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline vec3 cross(vec3 a, vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline void boxHit(vec3 ro, vec3 rd, vec3 halfSize, vec3& n, float& tmin_out) {
    vec3 inv = vec3(1.0f) / rd;
    vec3 t1 = (-ro - halfSize) * inv, t2 = (-ro + halfSize) * inv;
    float tmin = fmaxf(fminf(t1.x, t2.x), fmaxf(fminf(t1.y, t2.y), fminf(t1.z, t2.z)));
    float tmax = fminf(fmaxf(t1.x, t2.x), fminf(fmaxf(t1.y, t2.y), fmaxf(t1.z, t2.z)));
    if (tmin <= tmax && tmax > 0 && tmin < tmin_out) {
        tmin_out = tmin;
        if (tmin == fminf(t1.x, t2.x)) n = vec3(-sign(rd.x), 0, 0);
        else if (tmin == fminf(t1.y, t2.y)) n = vec3(0, -sign(rd.y), 0);
        else n = vec3(0, 0, -sign(rd.z));
    }
}

struct XorShift {
    uint32_t state;
    XorShift(uint32_t seed = 123456789) : state(seed) {}
    uint32_t operator()() {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return state;
    }
    float uniform() { return (*this)() * 2.3283064365386963e-10f; }
} rng;

void HideCursor() {
    HANDLE H = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO ci; GetConsoleCursorInfo(H, &ci);
    ci.bVisible = FALSE; SetConsoleCursorInfo(H, &ci);
}
POINT GetConsoleCenter() {
    HWND h = GetConsoleWindow();
    if (!h) { POINT c; c.x = GetSystemMetrics(SM_CXSCREEN) / 2; c.y = GetSystemMetrics(SM_CYSCREEN) / 2; return c; }
    RECT wa; SystemParametersInfo(SPI_GETWORKAREA, 0, &wa, 0);
    int ww = (wa.right - wa.left) * 0.7, wh = (wa.bottom - wa.top) * 0.7;
    MoveWindow(h, (wa.left + wa.right - ww) / 2, (wa.top + wa.bottom - wh) / 2, ww, wh, TRUE);
    RECT wr; GetWindowRect(h, &wr);
    return {(wr.left + wr.right) / 2, (wr.top + wr.bottom) / 2};
}
void GetConsoleSize(HANDLE H, int& w, int& h) {
    CONSOLE_SCREEN_BUFFER_INFO csbi; GetConsoleScreenBufferInfo(H, &csbi);
    w = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    h = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
}
void FitBufferToWindow(HANDLE H, int w, int h) {
    COORD bufSize = { (SHORT)w, (SHORT)h };
    SetConsoleScreenBufferSize(H, bufSize);
    SMALL_RECT windowRect = {0, 0, (SHORT)(w - 1), (SHORT)(h - 1)};
    SetConsoleWindowInfo(H, TRUE, &windowRect);
}

const int MIN_WINDOW_W = 20;
const int MIN_WINDOW_H = 10;
const int DEFAULT_W = 80;
const int DEFAULT_H = 25;

struct BoxCol { vec3 c; vec3 hs; };
const float PR = 0.4f;
const int CI = 8;
std::vector<BoxCol> ground, walls;
float nextZ = 0, lastWallZ = -1e9f, lastNeoZ = -100.0f;
int neoCooldown = 0;
bool neoActive = false;

const float GRAVITY = -18.0f, JUMP_VEL = 9.5f, GROUND_ACC = 18.0f, AIR_ACC = 0.5f;
const float MAX_SPEED = 8.0f, GROUND_FRICT = 12.0f, AIR_FRICT = 0.5f;
const float WALL_BOUNCE = 0.2f, WALL_BRAKE = 1.0f, MIN_WALL_GAP = 3.0f;
const size_t MAX_OBJECTS = 300;

void resolveSphereBox(vec3& p, vec3& vel, float r, const BoxCol& b, float bounce = 0.0f, bool isWall = false) {
    vec3 cl;
    cl.x = clamp(p.x, b.c.x - b.hs.x, b.c.x + b.hs.x);
    cl.y = clamp(p.y, b.c.y - b.hs.y, b.c.y + b.hs.y);
    cl.z = clamp(p.z, b.c.z - b.hs.z, b.c.z + b.hs.z);
    vec3 d = p - cl;
    float l = length(d);
    if (l < r) {
        vec3 n;
        if (l < 0.001f) {
            vec3 tc = p - b.c;
            float ax = fabsf(tc.x), ay = fabsf(tc.y), az = fabsf(tc.z);
            if (ax >= ay && ax >= az)      n = vec3(sign(tc.x), 0, 0);
            else if (ay >= ax && ay >= az) n = vec3(0, sign(tc.y), 0);
            else                           n = vec3(0, 0, sign(tc.z));
            if (length(n) < 0.5f) n = vec3(0, 1, 0);
            n = norm(n);
        } else n = d * (1.0f / l);
        p = cl + n * r;
        float vdotn = dot(vel, n);
        if (vdotn < 0) {
            vel = vel - (1.0f + bounce) * vdotn * n;
            if (isWall && fabsf(n.y) < 0.1f) {
                vel.x *= WALL_BRAKE; vel.z *= WALL_BRAKE;
            }
        }
    }
}
void resolveAll(vec3& p, vec3& vel) {
    for (int i = 0; i < CI; ++i) {
        for (auto& g : ground) resolveSphereBox(p, vel, PR, g, 0.0f, false);
        for (auto& w : walls)  resolveSphereBox(p, vel, PR, w, WALL_BOUNCE, true);
    }
}
bool onGround(const vec3& p, const vec3& vel) {
    vec3 t = p; t.y -= PR + 0.05f;
    for (auto& g : ground) {
        vec3 c;
        c.x = clamp(t.x, g.c.x - g.hs.x, g.c.x + g.hs.x);
        c.y = clamp(t.y, g.c.y - g.hs.y, g.c.y + g.hs.y);
        c.z = clamp(t.z, g.c.z - g.hs.z, g.c.z + g.hs.z);
        if (length(t - c) < PR) return true;
    }
    return false;
}
void clearScene() { ground.clear(); walls.clear(); nextZ = 0; lastWallZ = -1e9f; lastNeoZ = -100.0f; neoCooldown = 0; neoActive = false; }

void genUntil(float targetZ) {
    while (nextZ < targetZ && ground.size() + walls.size() < MAX_OBJECTS) {
        if (neoCooldown == 0 && rng.uniform() < 0.05f && nextZ - lastNeoZ > 15.0f) {
            neoActive = true; lastNeoZ = nextZ;
        }
        if (neoActive) {
            float frontZ = nextZ - 1.0f;
            bool hasFront = false;
            for (auto& g : ground) if (g.c.z >= frontZ && g.c.z <= frontZ + 1.0f) { hasFront = true; break; }
            if (!hasFront) ground.push_back({vec3(0, -0.5f, frontZ + 0.5f), {1.5f, 0.5f, 0.5f}});
            for (int i = 0; i < 1; ++i) {
                float wallZ = nextZ + 0.5f + i;
                float h = 10.0f, t = 0.1f;
                walls.push_back({vec3(0, h * 0.5f, wallZ), {1.45f, h * 0.5f, t * 0.5f}});
                lastWallZ = wallZ;
                ground.push_back({vec3(0, -0.5f, wallZ), {1.5f, 0.5f, 0.5f}});
            }
            float backZ = nextZ + 1.0f;
            bool hasBack = false;
            for (auto& g : ground) if (g.c.z >= backZ && g.c.z <= backZ + 1.0f) { hasBack = true; break; }
            if (!hasBack) ground.push_back({vec3(0, -0.5f, backZ + 0.5f), {1.5f, 0.5f, 0.5f}});
            nextZ += 3; neoActive = false; neoCooldown = 20; continue;
        }
        if (neoCooldown > 0) neoCooldown--;
        if (rng.uniform() < 0.22f) { nextZ += 1.0f; continue; }
        ground.push_back({vec3(0, -0.5f, nextZ + 0.5f), {1.5f, 0.5f, 0.5f}});
        if (rng.uniform() < 0.32f) {
            float newWallZ = nextZ + 0.5f;
            if (newWallZ - lastWallZ >= MIN_WALL_GAP) {
                float h = 0.8f + rng.uniform() * 1.7f;
                float t = 0.25f + rng.uniform() * 0.2f;
                walls.push_back({vec3(0, h * 0.5f, newWallZ), {1.45f, h * 0.5f, t * 0.5f}});
                lastWallZ = newWallZ;
            }
        }
        nextZ += 1.0f;
    }
}
void updateScene(const vec3& pp) {
    if (!std::isfinite(pp.x) || !std::isfinite(pp.y) || !std::isfinite(pp.z)) { clearScene(); return; }
    float cl = pp.z - 45.0f;
    ground.erase(std::remove_if(ground.begin(), ground.end(), [cl](BoxCol& g) {return g.c.z < cl; }), ground.end());
    walls.erase(std::remove_if(walls.begin(), walls.end(), [cl](BoxCol& w) {return w.c.z < cl; }), walls.end());
    genUntil(pp.z + 18.0f);
}

float g_bestDist = 0.0f;
const char* HIGHSCORE_FILE = "highscore.dat";
void loadHighScore() {
    std::ifstream ifs(HIGHSCORE_FILE);
    if (ifs.is_open()) { ifs >> g_bestDist; ifs.close(); }
}
void saveHighScore(float newDist) {
    if (newDist > g_bestDist) {
        g_bestDist = newDist;
        std::ofstream ofs(HIGHSCORE_FILE);
        if (ofs.is_open()) { ofs << g_bestDist; ofs.close(); }
    }
}

void writeHUD(wchar_t* buffer, int screenW, float dist, float bestDist, bool dead,
              float sprintTimer, float sprintCooldown) {
    wchar_t line[240] = {};
    const wchar_t* stateStr = L"";
    if (sprintTimer > 0.0f) stateStr = L"[SPRINT!]";
    else if (sprintCooldown > 0.0f) {
        int cool10 = (int)(sprintCooldown * 10.0f);
        line[0] = L'['; line[1] = L'C'; line[2] = L'O'; line[3] = L'O'; line[4] = L'L'; line[5] = L':';
        line[6] = L'0' + cool10 / 10; line[7] = L'.'; line[8] = L'0' + cool10 % 10; line[9] = L']';
        stateStr = line;
    } else stateStr = L"[READY]";
    int len = swprintf(line, 240, L"  Distance: %.0f m   Best: %.0f m   %s %s",
                       dist, bestDist, dead ? L"DEAD" : L"", stateStr);
    for (int i = 0; i < screenW; ++i) buffer[i] = i < len ? line[i] : L' ';
}

int main() {
    HideCursor();
    HANDLE H = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    SetConsoleActiveScreenBuffer(H);

    int w, h;
    Sleep(100);
    GetConsoleSize(H, w, h);
    if (w < MIN_WINDOW_W || h < MIN_WINDOW_H) {
        w = DEFAULT_W; h = DEFAULT_H;
    }
    FitBufferToWindow(H, w, h);

    std::vector<wchar_t> scr(w * h);
    float asp = (float)w / h;
    float pa = 0.4583f;
    char grad[] = " .:!/r(l1Z4H9W8$@"; int gs = sizeof(grad) - 2;

    POINT c = GetConsoleCenter(); SetCursorPos(c.x, c.y);
    ground.reserve(256); walls.reserve(128);
    loadHighScore();

    vec3 pos(0, 1.5f, -8), vel(0, 0, 0);
    float yaw = 0.0f, pitch = 0, ms = 0.0025f;
    bool grounded = false;
    enum { PLAYING, LOST } st = PLAYING;
    bool run = true;
    auto lt = std::chrono::steady_clock::now();
    vec3 ld = norm(vec3(-0.5f, 0.8f, -1.0f));

    clearScene(); nextZ = pos.z; lastWallZ = -1e9f; genUntil(pos.z + 18.f);
    float sprintTimer = 0.0f, sprintCooldown = 0.0f;
    int frameCounter = 0;

    while (run) {
        auto nt = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(nt - lt).count(); lt = nt;
        if (dt > 0.033f) dt = 0.033f;
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) run = false;

        if (GetAsyncKeyState('R') & 0x8000) {
            float curDist = pos.z + 8.0f;
            if (curDist > g_bestDist) saveHighScore(curDist);
            pos = vec3(0, 1.5f, -8); vel = vec3(0, 0, 0); yaw = 0.0f; pitch = 0;
            st = PLAYING; clearScene(); nextZ = pos.z; lastWallZ = -1e9f; genUntil(pos.z + 18.f);
            sprintTimer = sprintCooldown = 0.0f;
        }

        POINT mp; GetCursorPos(&mp); int dx = mp.x - c.x, dy = mp.y - c.y;
        if (dx || dy) {
            if (st == PLAYING) { yaw -= dx * ms; pitch -= dy * ms; pitch = clamp(pitch, -1.4f, 1.4f); }
            SetCursorPos(c.x, c.y);
        }
        vec3 fwd(cosf(pitch) * sinf(yaw), sinf(pitch), cosf(pitch) * cosf(yaw));
        vec3 rgt = norm(cross(fwd, vec3(0, 1, 0)));
        vec3 up = cross(rgt, fwd);

        updateScene(pos);

        if (st == PLAYING) {
            vec3 moveDir(0, 0, 0);
            if (GetAsyncKeyState('W') & 0x8000) moveDir = moveDir + vec3(fwd.x, 0, fwd.z);
            if (GetAsyncKeyState('S') & 0x8000) moveDir = moveDir - vec3(fwd.x, 0, fwd.z);
            if (GetAsyncKeyState('A') & 0x8000) moveDir = moveDir - rgt;
            if (GetAsyncKeyState('D') & 0x8000) moveDir = moveDir + rgt;
            float lenM = length(moveDir);
            if (lenM > 0.001f) moveDir = moveDir * (1.0f / lenM); else moveDir = vec3(0, 0, 0);
            grounded = onGround(pos, vel);

            if (sprintTimer > 0) { sprintTimer -= dt; if (sprintTimer < 0) sprintTimer = 0; }
            if (sprintCooldown > 0) { sprintCooldown -= dt; if (sprintCooldown < 0) sprintCooldown = 0; }
            if ((GetAsyncKeyState(VK_LSHIFT) & 0x8000) || (GetAsyncKeyState(VK_RSHIFT) & 0x8000)) {
                if (sprintTimer <= 0.0f && sprintCooldown <= 0.0f && grounded) {
                    sprintTimer = 0.6f; sprintCooldown = 1.2f;
                }
            }
            float curMaxSpeed = (sprintTimer > 0.0f) ? MAX_SPEED * 2.0f : MAX_SPEED;
            float curGroundAcc = (sprintTimer > 0.0f) ? GROUND_ACC * 1.5f : GROUND_ACC;
            float accScale = grounded ? curGroundAcc : AIR_ACC;
            vec3 acc = moveDir * accScale + vec3(0, GRAVITY, 0);
            vel = vel + acc * dt;

            if (!grounded) {
                vel.x *= (1.0f - AIR_FRICT * dt); vel.z *= (1.0f - AIR_FRICT * dt);
            } else {
                float spdHor = sqrtf(vel.x * vel.x + vel.z * vel.z);
                if (spdHor > 0.01f) {
                    float newSpd = fmaxf(0.0f, spdHor - GROUND_FRICT * dt);
                    vel.x *= newSpd / spdHor; vel.z *= newSpd / spdHor;
                } else vel.x = vel.z = 0;
            }
            float horSpeed = sqrtf(vel.x * vel.x + vel.z * vel.z);
            if (horSpeed > curMaxSpeed) {
                vel.x *= curMaxSpeed / horSpeed; vel.z *= curMaxSpeed / horSpeed;
            }

            if (GetAsyncKeyState(VK_SPACE) & 0x8000 && grounded) {
                vel.y = JUMP_VEL; grounded = false;
            }

            vec3 newPos = pos + vel * dt;
            resolveAll(newPos, vel);
            pos = newPos;

            grounded = onGround(pos, vel);
            if (grounded && vel.y < 0) vel.y = 0;

            if (pos.y < -5.0f) {
                st = LOST;
                float curDist = pos.z + 8.0f;
                if (curDist > g_bestDist) saveHighScore(curDist);
            }
            if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z)) {
                pos = vec3(0, 1.5f, -8); vel = vec3(0, 0, 0); yaw = 0.0f; pitch = 0;
                st = PLAYING; clearScene(); nextZ = pos.z; lastWallZ = -1e9f; genUntil(pos.z + 18.f);
                sprintTimer = sprintCooldown = 0.0f;
            }
        }

        float dist = pos.z + 8.0f; if (dist < 0) dist = 0;
        if (++frameCounter % 10 == 0) {
            std::string title = st == PLAYING ? ("无尽跑酷 | NEO挑战 | 距离:" + std::to_string((int)dist) + "m  |  R重置")
                                              : ("坠落! 距离:" + std::to_string((int)dist) + "m |  R重新开始");
            SetConsoleTitleA(title.c_str());
        }

        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(H, &csbi);
        int visW = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        int visH = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        int left = csbi.srWindow.Left, top = csbi.srWindow.Top;

        if (visW < MIN_WINDOW_W || visH < MIN_WINDOW_H) {
            visW = MIN_WINDOW_W; visH = MIN_WINDOW_H;
            FitBufferToWindow(H, visW, visH);
        }

        if (visW != w || visH != h) {
            w = visW; h = visH;
            FitBufferToWindow(H, w, h);
            scr.resize(w * h);
            asp = (float)w / h;
        }

        float viewZ = pos.z;
        #pragma omp parallel for schedule(dynamic)
        for (int i = 0; i < w; ++i) {
            for (int j = 0; j < h; ++j) {
                vec2 uv((float)i / w, (float)j / h);
                uv = uv * 2.0f - 1.0f;
                uv.x *= asp * pa; uv.y *= -1.0f;
                vec3 rd = norm(fwd + rgt * uv.x + up * uv.y);
                float tmin = 1e9f; vec3 n; float alb = 1; bool hit = false;

                for (auto& g : ground) {
                    if (g.c.z < viewZ - 1.5f) continue;
                    float oldT = tmin;
                    vec3 bn;
                    boxHit(pos - g.c, rd, g.hs, bn, tmin);
                    if (tmin < oldT) { n = bn; alb = 0.75f; hit = true; }
                }
                for (auto& w : walls) {
                    if (w.c.z < viewZ - 1.5f) continue;
                    float oldT = tmin;
                    vec3 bn;
                    boxHit(pos - w.c, rd, w.hs, bn, tmin);
                    if (tmin < oldT) { n = bn; alb = 1.0f; hit = true; }
                }
                float diff = 0;
                if (hit) {
                    float ndl = fmaxf(0.0f, dot(n, ld));
                    diff = (0.2f + ndl) * alb;
                    if (diff > 1.0f) diff = 1.0f;
                }
                int idx = (int)(diff * gs);
                if (idx > gs) idx = gs;
                scr[i + j * w] = grad[idx];
            }
        }

        writeHUD(scr.data(), w, dist, g_bestDist, st == LOST, sprintTimer, sprintCooldown);

        for (int j=0;j<h;j++) {
            COORD coord = { (SHORT)left, (SHORT)(top + j) };
            DWORD written;
            WriteConsoleOutputCharacterW(H, scr.data() + j * w, w, coord, &written);
            int bufW = csbi.dwSize.X;
            int clearLen = bufW - (left + w);
            if (clearLen > 0) {
                std::wstring spaces(clearLen, L' ');
                COORD fillPos = { (SHORT)(left + w), (SHORT)(top + j) };
                WriteConsoleOutputCharacterW(H, spaces.c_str(), clearLen, fillPos, &written);
            }
        }
    }

    saveHighScore(pos.z + 8.0f);
    CloseHandle(H);
    return 0;
}
