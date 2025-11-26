
// Space Explorer - Final single-file version 

#include <windows.h>
#include <GL/glut.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <algorithm>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")   // links the Multimedia API

// -------------------------------------------------------------
//  SOUND CONTROL  (background music + one-shot sound effects)
// -------------------------------------------------------------
void playBackgroundMusic() {
    // plays and loops background.mp3 forever
    mciSendString(L"open \"background.mp3\" type mpegvideo alias bgm", NULL, 0, NULL);
    mciSendString(L"play bgm repeat", NULL, 0, NULL);
    // set volume (0–1000, lower = quieter)
    mciSendString(L"setaudio bgm volume to 300", NULL, 0, NULL);
}

void stopBackgroundMusic() {
    // stops and closes the music file
    mciSendString(L"stop bgm", NULL, 0, NULL);
    mciSendString(L"close bgm", NULL, 0, NULL);
}

void playSoundEffect(const char* file) {
    wchar_t cmd[256];
    printf("Playing sound: %s\n", file);

    swprintf(cmd, 256, L"open \"%hs\" type waveaudio alias se", file);
    MCIERROR err = mciSendString(cmd, NULL, 0, NULL);
    if (err != 0) {
        wchar_t buf[256];
        mciGetErrorString(err, buf, 256);
        MessageBox(NULL, buf, L"Sound Error", MB_OK);
        return;
    }
    mciSendString(L"play se from 0 wait", NULL, 0, NULL);  // waits for clip to finish
    mciSendString(L"close se", NULL, 0, NULL);

}





// -------------------------------
// Basic types & helpers
// -------------------------------
struct Vec2 { float x, y; };

static inline float dist(const Vec2& a, const Vec2& b) {
    float dx = a.x - b.x, dy = a.y - b.y; return sqrtf(dx * dx + dy * dy);
}

// -------------------------------
// Print on screen (instructor function equivalent)
// -------------------------------
void print_on_screen(int x, int y, const char* s) {
    glRasterPos2f((float)x, (float)y);
    int len = (int)strlen(s);
    for (int i = 0; i < len; i++) glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_24, s[i]);
}

// -------------------------------
// Bezier (safe float version)
// -------------------------------
void bezier_point_float(float t, const int p0[2], const int p1[2], const int p2[2], const int p3[2], float out[2]) {
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;

    float x = uuu * p0[0] + 3.0f * uu * t * p1[0] + 3.0f * u * tt * p2[0] + ttt * p3[0];
    float y = uuu * p0[1] + 3.0f * uu * t * p1[1] + 3.0f * u * tt * p2[1] + ttt * p3[1];
    out[0] = x; out[1] = y;
}

// -------------------------------
// Window & panel sizes
// -------------------------------
const int WIN_W = 1000;
const int WIN_H = 700;
const int TOP_H = 100;
const int BOTTOM_H = 100;
const int GAME_Y0 = BOTTOM_H;
const int GAME_Y1 = WIN_H - TOP_H;




// -------------------------------
// Game state
// -------------------------------
bool running = false;
bool showEnd = false;
bool playerWon = false;

int totalTime = 120;     // total seconds
int remainingTime = totalTime;
int playerScore = 0;
int playerLives = 5;

Vec2 playerPos, playerDir = { 1.0f, 0.0f };
float playerRadius = 18.0f;
float baseSpeed = 200.0f; // px / sec

// powerups
bool speedActive = false; float speedTimer = 0.0f;
bool doubleActive = false; float doubleTimer = 0.0f;

// invulnerability after hitting obstacle
float invulnTimer = 0.0f; // seconds

// timing
int prevTimeMs = 0;
float accumSec = 0.0f;

// placement mode
enum PlaceMode { NONE_MODE = 0, OBSTACLE_MODE, COLLECT_MODE, POWER1_MODE, POWER2_MODE };
PlaceMode currentMode = NONE_MODE;

// objects
struct Obstacle { Vec2 p; float r; };
struct Collectible { Vec2 p; float r; bool active; float rot; };
struct PowerUp { Vec2 p; float r; int type; bool active; float phase; };

std::vector<Obstacle> obstacles;
std::vector<Collectible> collectibles;
std::vector<PowerUp> powerups;

// bezier target (integers for compatibility with instructor code)
int bz_p0[2], bz_p1[2], bz_p2[2], bz_p3[2];
float bezT = 0.0f;
float bezSpeed = 0.12f; // t units per second
Vec2 targetPos;

// keyboard state
bool keyLeft = false, keyRight = false, keyUp = false, keyDown = false;

// -------------------------------
// Utility drawing helpers
// -------------------------------
void drawCircle(const Vec2& c, float r, int seg = 32) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(c.x, c.y);
    for (int i = 0; i <= seg; i++) {
        float a = (float)i / seg * 2.0f * 3.14159265358979323846f;
        glVertex2f(c.x + cosf(a) * r, c.y + sinf(a) * r);
    }
    glEnd();
}

void drawStringCentered(int x, int y, const char* s) {
    print_on_screen(x - (int)strlen(s) * 6, y, s);
}

// -----------------------------------------------
// Keep player within the playable area boundaries
// -----------------------------------------------
Vec2 clampToArea(Vec2 p) {
    float pad = playerRadius + 2.0f;
    if (p.x < pad) p.x = pad;
    if (p.x > WIN_W - pad) p.x = WIN_W - pad;
    if (p.y < GAME_Y0 + pad) p.y = GAME_Y0 + pad;
    if (p.y > GAME_Y1 - pad) p.y = GAME_Y1 - pad;
    return p;
}

// -------------------------------
// Overlap / placement helper
// -------------------------------
bool overlapsExisting(const Vec2& p, float r) {
    for (auto& ob : obstacles) if (dist(p, ob.p) < r + ob.r + 6.0f) return true;
    for (auto& c : collectibles) if (dist(p, c.p) < r + c.r + 6.0f) return true;
    for (auto& pu : powerups) if (dist(p, pu.p) < r + pu.r + 6.0f) return true;
    // avoid target and player
    if (dist(p, targetPos) < r + 20.0f + 6.0f) return true;
    if (dist(p, playerPos) < r + playerRadius + 6.0f) return true;
    return false;
}

// -------------------------------
// Draw HUD panels
// -------------------------------
void drawTopPanel() {
    // panel background
    glColor3f(0.08f, 0.09f, 0.12f);
    glBegin(GL_QUADS);
    glVertex2f(0, WIN_H); glVertex2f(WIN_W, WIN_H);
    glVertex2f(WIN_W, GAME_Y1); glVertex2f(0, GAME_Y1);
    glEnd();

    // Time (big, white)
    char buf[64];
    sprintf(buf, "TIME: %d s", remainingTime);
    glColor3f(1.0f, 0.95f, 0.6f);
    print_on_screen(20, WIN_H - 60, buf);

    // Score
    sprintf(buf, "SCORE: %d", playerScore);
    glColor3f(0.8f, 0.9f, 1.0f);
    print_on_screen(240, WIN_H - 60, buf);

    // Lives as hearts (white outline + red fill)
    float startX = WIN_W - 220;
    for (int i = 0; i < playerLives; i++) {
        float cx = startX + i * 36, cy = WIN_H - 60;
        // red heart (two circles + triangle)
        glColor3f(1.0f, 0.15f, 0.25f);
        drawCircle({ cx - 6, cy + 6 }, 7, 16);
        drawCircle({ cx + 6, cy + 6 }, 7, 16);
        glBegin(GL_TRIANGLES);
        glVertex2f(cx - 12, cy + 4); glVertex2f(cx + 12, cy + 4); glVertex2f(cx, cy - 8);
        glEnd();
        // outline
        glColor3f(0.0f, 0.0f, 0.0f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(cx - 12, cy + 4); glVertex2f(cx + 12, cy + 4); glVertex2f(cx, cy - 8);
        glEnd();
    }
}

void drawBottomPanel() {
    glColor3f(0.06f, 0.06f, 0.08f);
    glBegin(GL_QUADS);
    glVertex2f(0, BOTTOM_H); glVertex2f(WIN_W, BOTTOM_H);
    glVertex2f(WIN_W, 0); glVertex2f(0, 0);
    glEnd();

    // icons
    float gap = 120.0f;
    float x = 200.0f, y = 50.0f;
    // Obstacle - red square icon
    glColor3f(0.7f, 0.25f, 0.2f);
    glBegin(GL_QUADS);
    glVertex2f(x - 18, y - 18); glVertex2f(x + 18, y - 18); glVertex2f(x + 18, y + 18); glVertex2f(x - 18, y + 18);
    glEnd();
    // Collectible - golden triangle
    glColor3f(1.0f, 0.92f, 0.16f);
    glBegin(GL_TRIANGLES);
    glVertex2f(x + gap, y + 16); glVertex2f(x + gap - 12, y - 12); glVertex2f(x + gap + 12, y - 12);
    glEnd();
    // Power1 - blue pulsing
    glColor3f(0.12f, 0.55f, 1.0f); drawCircle({ x + 2 * gap,y }, 15);
    // Power2 - green pulsing
    glColor3f(0.14f, 0.95f, 0.28f); drawCircle({ x + 3 * gap,y }, 15);

    // labels
    glColor3f(1, 1, 1);
    print_on_screen((int)(x - 34), 12, "Obstacle");
    print_on_screen((int)(x + gap - 40), 12, "Collectible");
    print_on_screen((int)(x + 2 * gap - 28), 12, "Speed");
    print_on_screen((int)(x + 3 * gap - 44), 12, "Double");
}

// -------------------------------
// Draw game objects and visuals
// -------------------------------
void drawBackgroundStars(float dt) {
    // subtle moving stars
    glColor3f(0.85f, 0.85f, 1.0f);
    int count = 80;
    float time = (float)glutGet(GLUT_ELAPSED_TIME) * 0.001f;
    for (int i = 0; i < count; i++) {
        float x = fmodf(i * 37.1f + time * 30.0f, (float)WIN_W);
        float y = GAME_Y0 + fmodf(i * 61.7f + time * 13.0f, (float)(GAME_Y1 - GAME_Y0));
        glPointSize((i % 6 == 0) ? 2.8f : 1.6f);
        glBegin(GL_POINTS); glVertex2f(x, y); glEnd();
    }
}

void drawPlayer() {
    // glow outer
    glColor4f(0.1f, 0.6f, 1.0f, 0.18f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    drawCircle({ playerPos.x, playerPos.y }, playerRadius + 8, 32);
    glDisable(GL_BLEND);

    // main body
    glColor3f(0.18f, 0.7f, 1.0f);
    drawCircle(playerPos, playerRadius, 32);

    // nose / triangle pointing to direction
    glColor3f(0.95f, 0.6f, 0.2f);
    Vec2 tip = { playerPos.x + playerDir.x * (playerRadius + 10), playerPos.y + playerDir.y * (playerRadius + 10) };
    Vec2 left = { playerPos.x - playerDir.y * 8.0f, playerPos.y + playerDir.x * 8.0f };
    Vec2 right = { playerPos.x + playerDir.y * 8.0f, playerPos.y - playerDir.x * 8.0f };
    glBegin(GL_TRIANGLES);
    glVertex2f(tip.x, tip.y);
    glVertex2f(left.x, left.y);
    glVertex2f(right.x, right.y);
    glEnd();

    // direction line
    glColor3f(1, 1, 1);
    glBegin(GL_LINES);
    glVertex2f(playerPos.x, playerPos.y);
    glVertex2f(playerPos.x + playerDir.x * (playerRadius + 20), playerPos.y + playerDir.y * (playerRadius + 20));
    glEnd();
    // Add glowing center point for fourth primitive
    glPointSize(6);
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_POINTS);
    glVertex2f(playerPos.x, playerPos.y);
    glEnd();
}

void drawTarget() {
    // outer ring (glow)
    glColor4f(0.8f, 0.25f, 0.9f, 0.18f);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    drawCircle(targetPos, 24, 36);
    glDisable(GL_BLEND);

    glColor3f(0.7f, 0.18f, 0.9f);
    drawCircle(targetPos, 14, 32);
    glColor3f(1.0f, 0.6f, 1.0f);
    drawCircle(targetPos, 7, 24);
}

void drawObstacles() {
    for (auto& o : obstacles) {
        glColor3f(0.6f, 0.28f, 0.12f);
        glBegin(GL_QUADS);
        glVertex2f(o.p.x - o.r, o.p.y - o.r);
        glVertex2f(o.p.x + o.r, o.p.y - o.r);
        glVertex2f(o.p.x + o.r, o.p.y + o.r);
        glVertex2f(o.p.x - o.r, o.p.y + o.r);
        glEnd();
        glColor3f(0, 0, 0);
        glBegin(GL_LINE_LOOP);
        glVertex2f(o.p.x - o.r, o.p.y - o.r);
        glVertex2f(o.p.x + o.r, o.p.y - o.r);
        glVertex2f(o.p.x + o.r, o.p.y + o.r);
        glVertex2f(o.p.x - o.r, o.p.y + o.r);
        glEnd();
    }
}

void drawCollectibles(float dt) {
    for (auto& c : collectibles) {
        if (!c.active) continue;
        // glow
        glColor4f(1.0f, 0.86f, 0.2f, 0.12f);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        drawCircle({ c.p.x, c.p.y }, c.r + 6, 24);
        glDisable(GL_BLEND);

        glPushMatrix();
        glTranslatef(c.p.x, c.p.y, 0);
        glRotatef(c.rot, 0, 0, 1);
        glColor3f(1.0f, 0.92f, 0.2f);
        glBegin(GL_TRIANGLES);
        glVertex2f(0, c.r); glVertex2f(-c.r * 0.6f, -c.r * 0.6f); glVertex2f(c.r * 0.6f, -c.r * 0.6f);
        glEnd();

        // Outline using a line loop (3rd primitive)
        glColor3f(0.3f, 0.3f, 0.3f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(0, c.r);
        glVertex2f(-c.r * 0.6f, -c.r * 0.6f);
        glVertex2f(c.r * 0.6f, -c.r * 0.6f);
        glEnd();
        glPopMatrix();

        c.rot += 90.0f * dt;
        if (c.rot > 360.0f) c.rot -= 360.0f;
    }
}

void drawPowerUps(float dt) {
    for (auto& p : powerups) {
        if (!p.active) continue;
        p.phase += dt * 3.0f;
        float bob = sinf(p.phase) * 6.0f;

        if (p.type == 1) {
            // blue speed
            glColor3f(0.18f, 0.55f, 1.0f);
            drawCircle({ p.p.x, p.p.y + bob }, p.r);
            // small white lines
            glColor3f(1, 1, 1);
            glBegin(GL_LINES);
            glVertex2f(p.p.x - 6, p.p.y + bob - 2); glVertex2f(p.p.x + 6, p.p.y + bob + 2);
            glVertex2f(p.p.x - 6, p.p.y + bob + 2); glVertex2f(p.p.x + 6, p.p.y + bob - 2);
            glEnd();
        }
        else {
            // green double score
            glColor3f(0.12f, 0.95f, 0.28f);
            glBegin(GL_POLYGON);
            glVertex2f(p.p.x, p.p.y + bob + p.r);
            glVertex2f(p.p.x + p.r * 0.7f, p.p.y + bob + p.r * 0.2f);
            glVertex2f(p.p.x + p.r * 0.4f, p.p.y + bob - p.r * 0.8f);
            glVertex2f(p.p.x - p.r * 0.4f, p.p.y + bob - p.r * 0.8f);
            glVertex2f(p.p.x - p.r * 0.7f, p.p.y + bob + p.r * 0.2f);
            glEnd();
            glColor3f(0, 0, 0);
            glBegin(GL_LINE_LOOP);
            glVertex2f(p.p.x, p.p.y + bob + p.r);
            glVertex2f(p.p.x + p.r * 0.7f, p.p.y + bob + p.r * 0.2f);
            glVertex2f(p.p.x + p.r * 0.4f, p.p.y + bob - p.r * 0.8f);
            glVertex2f(p.p.x - p.r * 0.4f, p.p.y + bob - p.r * 0.8f);
            glVertex2f(p.p.x - p.r * 0.7f, p.p.y + bob + p.r * 0.2f);
            glEnd();
        }
    }
}

// -------------------------------
// Game logic: collisions, timers
// -------------------------------

    // -----------------------------------------------
    // Bezier vertical motion: loops endlessly and speeds up with time
    // -----------------------------------------------
    void computeBezierTarget(float dt) {
        static bool reverse = false;

        // make target speed up as time decreases (min 0.08f, max 0.35f)
        float timeRatio = (float)remainingTime / (float)totalTime;  // 1.0 → 0.0
        float dynamicSpeed = 0.12f + (1.0f - timeRatio) * 0.33f; // faster as time runs out

        // ping-pong movement
        if (!reverse) bezT += dynamicSpeed * dt;
        else bezT -= dynamicSpeed * dt;

        // reverse direction at ends
        if (bezT >= 1.0f) { bezT = 1.0f; reverse = true; }
        if (bezT <= 0.0f) { bezT = 0.0f; reverse = false; }

        // compute point along Bezier curve
        float out[2];
        bezier_point_float(bezT, bz_p0, bz_p1, bz_p2, bz_p3, out);
        targetPos.x = out[0];
        targetPos.y = out[1];
    }



void handleCollisions(float dt) {
    // obstacles: if player collides and not invulnerable -> lose life and push back
    if (invulnTimer > 0.0f) invulnTimer -= dt;
    for (auto& ob : obstacles) {
        float d = dist(playerPos, ob.p);
        if (d <= playerRadius + ob.r) {
            if (invulnTimer <= 0.0f) {
                playerLives = (std::max)(0, playerLives - 1);
                invulnTimer = 0.7f; // small invulnerability
                playSoundEffect("hit.wav");
            }
            // push back
            Vec2 push = { playerPos.x - ob.p.x, playerPos.y - ob.p.y };
            float mag = sqrtf(push.x * push.x + push.y * push.y);
            if (mag > 0.001f) {
                push.x /= mag; push.y /= mag;
                playerPos.x += push.x * 6.0f;
                playerPos.y += push.y * 6.0f;
                playerPos = clampToArea(playerPos);
            }
        }
    }

    // collectibles
    for (auto& c : collectibles) {
        if (!c.active) continue;
        float d = dist(playerPos, c.p);
        if (d <= playerRadius + c.r) {
            int add = 5;
            if (doubleActive) add *= 2;
            playerScore += add;
            c.active = false;
            playSoundEffect("collect.wav");
            // small pop visual: we simply scale it briefly by setting rot negative? We'll skip audio.
        }
    }

    // powerups
    for (auto& p : powerups) {
        if (!p.active) continue;
        float d = dist(playerPos, p.p);
        if (d <= playerRadius + p.r) {
            p.active = false;
            if (p.type == 1) { speedActive = true; speedTimer = 6.0f; }
            else { doubleActive = true; doubleTimer = 8.0f; }
        }
    }

    // update powerup timers
    if (speedActive) {
        speedTimer -= dt;
        if (speedTimer <= 0.0f) speedActive = false;
    }
    if (doubleActive) {
        doubleTimer -= dt;
        if (doubleTimer <= 0.0f) doubleActive = false;
    }
}

// -------------------------------
// Movement & update loop
// -------------------------------
void updateMovement(float dt) {
    Vec2 mv = { 0,0 };
    if (keyLeft) mv.x -= 1.0f;
    if (keyRight) mv.x += 1.0f;
    if (keyUp) mv.y += 1.0f;
    if (keyDown) mv.y -= 1.0f;

    float mag = sqrtf(mv.x * mv.x + mv.y * mv.y);
    if (mag > 0.0f) {
        mv.x /= mag; mv.y /= mag;
        playerDir = mv;
        // if currently colliding with obstacle and invuln active, we still allow movement but pushback handled in collision
        float spd = baseSpeed * (speedActive ? 1.8f : 1.0f);
        playerPos.x += mv.x * spd * dt;
        playerPos.y += mv.y * spd * dt;
        playerPos = clampToArea(playerPos);
    }
}

bool checkEndCondition() {
    if (playerLives <= 0) { playerWon = false; return true; }
    if (remainingTime <= 0) { playerWon = false; return true; }
    if (dist(playerPos, targetPos) <= playerRadius + 14.0f) { playerWon = true; return true; }
    return false;
}

// -------------------------------
// Rendering + game loop
// -------------------------------
void display();
void idle() {
    int nowMs = glutGet(GLUT_ELAPSED_TIME);
    float dt = (prevTimeMs == 0) ? 0.016f : (nowMs - prevTimeMs) / 1000.0f;
    prevTimeMs = nowMs;

    // animate background stars (uses glut elapsed inside draw)
    if (running) {
        // movement
        updateMovement(dt);
        // bezier target
        computeBezierTarget(dt);
        // collisions
        handleCollisions(dt);
        // countdown by accumulated seconds
        accumSec += dt;
        if (accumSec >= 1.0f) {
            remainingTime = (std::max)(0, remainingTime - 1); // use parenthesized std::max to avoid windows macro
            accumSec -= 1.0f;
        }
        // check end
        if (checkEndCondition()) {
            running = false;
            showEnd = true;
            stopBackgroundMusic();                          
            if (playerWon) playSoundEffect("win.wav");      
            else           playSoundEffect("lose.wav");     
        }
    }

    glutPostRedisplay();
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    // background game area
    glColor3f(0.02f, 0.02f, 0.05f);
    glBegin(GL_QUADS);
    glVertex2f(0, GAME_Y0); glVertex2f(WIN_W, GAME_Y0);
    glVertex2f(WIN_W, GAME_Y1); glVertex2f(0, GAME_Y1);
    glEnd();
    // animated stars
    drawBackgroundStars(0.016f);

    // panels
    drawTopPanel();
    drawBottomPanel();

    // objects
    drawObstacles();
    drawCollectibles(0.016f);
    drawPowerUps(0.016f);

    // target & player
    drawTarget();
    drawPlayer();

    // overlay end screen
    if (showEnd) {
        // dim background
        glEnable(GL_BLEND);
        glColor4f(0, 0, 0, 0.6f);
        glBegin(GL_QUADS);
        glVertex2f(0, 0); glVertex2f(WIN_W, 0);
        glVertex2f(WIN_W, WIN_H); glVertex2f(0, WIN_H);
        glEnd();
        glDisable(GL_BLEND);
        // bright text
        char buf[128];
        if (playerWon) {
            glColor3f(1.0f, 0.9f, 0.2f);
            sprintf(buf, "GAME WIN! Final Score: %d", playerScore);
            print_on_screen(WIN_W / 2 - 160, WIN_H / 2 + 20, buf);
        }
        else {
            glColor3f(1.0f, 0.6f, 0.6f);
            sprintf(buf, "GAME OVER. Final Score: %d", playerScore);
            print_on_screen(WIN_W / 2 - 160, WIN_H / 2 + 20, buf);
        }
        glColor3f(1.0f, 1.0f, 1.0f);
        print_on_screen(WIN_W / 2 - 120, WIN_H / 2 - 20, "Press C to clear & R to restart");
    }

    glutSwapBuffers();
}

// -------------------------------
// Input handling
// -------------------------------
void specialDown(int key, int, int) {
    if (key == GLUT_KEY_LEFT) keyLeft = true;
    if (key == GLUT_KEY_RIGHT) keyRight = true;
    if (key == GLUT_KEY_UP) keyUp = true;
    if (key == GLUT_KEY_DOWN) keyDown = true;
}
void specialUp(int key, int, int) {
    if (key == GLUT_KEY_LEFT) keyLeft = false;
    if (key == GLUT_KEY_RIGHT) keyRight = false;
    if (key == GLUT_KEY_UP) keyUp = false;
    if (key == GLUT_KEY_DOWN) keyDown = false;
}

void keyboard(unsigned char key, int, int) {
    if (key == 'r' || key == 'R') {
        playBackgroundMusic();
        // start/reset
        running = true;
        showEnd = false;
        playerScore = 0;
        playerLives = 5;
        remainingTime = totalTime;
        speedActive = doubleActive = false;
        speedTimer = doubleTimer = 0.0f;
        invulnTimer = 0.0f;
        prevTimeMs = glutGet(GLUT_ELAPSED_TIME);
        accumSec = 0.0f;
        // place player at left and target at right
        playerPos.x = 80.0f; playerPos.y = (GAME_Y0 + GAME_Y1) * 0.5f;
        targetPos.x = WIN_W - 80.0f; targetPos.y = (GAME_Y0 + GAME_Y1) * 0.5f;
        // right-side vertical Bezier curve (slight horizontal curve for visibility)
        bz_p0[0] = WIN_W - 120;  bz_p0[1] = GAME_Y0 + 80;   // bottom
        bz_p1[0] = WIN_W - 180;  bz_p1[1] = GAME_Y0 + 250;  // curve left
        bz_p2[0] = WIN_W - 60;  bz_p2[1] = GAME_Y1 - 250;  // curve right
        bz_p3[0] = WIN_W - 120;  bz_p3[1] = GAME_Y1 - 80;   // top

        bezT = 0.0f;

    }
    if (key == 'c' || key == 'C') {
        // clear everything (reset to placement mode)
        stopBackgroundMusic();
        obstacles.clear(); collectibles.clear(); powerups.clear();
        currentMode = NONE_MODE;
        running = false; showEnd = false;
        playerScore = 0; playerLives = 5; remainingTime = totalTime;
    }
}

void mouseClick(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        int oglY = WIN_H - y;
        if (oglY <= BOTTOM_H) {
            float bx[4] = { 200.0f, 200.0f + 120.0f, 200.0f + 240.0f, 200.0f + 360.0f };
            if (fabsf(x - bx[0]) < 30.0f) currentMode = OBSTACLE_MODE;
            else if (fabsf(x - bx[1]) < 30.0f) currentMode = COLLECT_MODE;
            else if (fabsf(x - bx[2]) < 30.0f) currentMode = POWER1_MODE;
            else if (fabsf(x - bx[3]) < 30.0f) currentMode = POWER2_MODE;
            return;
        }
        // placement in game area
        if (oglY > GAME_Y0 && oglY < GAME_Y1) {
            Vec2 p = { (float)x, (float)oglY };
            // clamp to area (padding)
            float pad = 20.0f;
            if (p.x < pad) p.x = pad;
            if (p.x > WIN_W - pad) p.x = WIN_W - pad;
            if (p.y < GAME_Y0 + pad) p.y = GAME_Y0 + pad;
            if (p.y > GAME_Y1 - pad) p.y = GAME_Y1 - pad;

            if (currentMode == OBSTACLE_MODE) {
                Obstacle ob; ob.p = p; ob.r = 20.0f;
                if (!overlapsExisting(ob.p, ob.r)) obstacles.push_back(ob);
            }
            else if (currentMode == COLLECT_MODE) {
                Collectible c; c.p = p; c.r = 12.0f; c.active = true; c.rot = 0.0f;
                if (!overlapsExisting(c.p, c.r)) collectibles.push_back(c);
            }
            else if (currentMode == POWER1_MODE) {
                PowerUp pu; pu.p = p; pu.r = 14.0f; pu.type = 1; pu.active = true; pu.phase = 0.0f;
                if (!overlapsExisting(pu.p, pu.r)) powerups.push_back(pu);
            }
            else if (currentMode == POWER2_MODE) {
                PowerUp pu; pu.p = p; pu.r = 14.0f; pu.type = 2; pu.active = true; pu.phase = 0.0f;
                if (!overlapsExisting(pu.p, pu.r)) powerups.push_back(pu);
            }
        }
    }
}

// -------------------------------
// Initialization & main
// -------------------------------
void initGame() {
    glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WIN_W, 0, WIN_H, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Start positions: player left, target right
    playerPos.x = 80.0f; playerPos.y = (GAME_Y0 + GAME_Y1) * 0.5f;
    targetPos.x = WIN_W - 80.0f; targetPos.y = (GAME_Y0 + GAME_Y1) * 0.5f;

    // Bezier points for vertical motion (right side)
    bz_p0[0] = WIN_W - 100;  bz_p0[1] = GAME_Y0 + 60;          // bottom point
    bz_p1[0] = WIN_W - 105;  bz_p1[1] = GAME_Y0 + 220;         // lower-mid
    bz_p2[0] = WIN_W - 95;   bz_p2[1] = GAME_Y1 - 220;         // upper-mid
    bz_p3[0] = WIN_W - 100;  bz_p3[1] = GAME_Y1 - 60;          // top point
    bezT = 0.0f;


    prevTimeMs = glutGet(GLUT_ELAPSED_TIME);
}

// -------------------------------
// Draw animated stars (helper used earlier, but defined here)
// -------------------------------
//void drawBackgroundStars(float dt) {
//    // subtle moving stars
//    glColor3f(0.85f, 0.85f, 1.0f);
//    int count = 80;
//    float time = (float)glutGet(GLUT_ELAPSED_TIME) * 0.001f;
//    for (int i = 0; i < count; i++) {
//        float x = fmodf(i * 37.1f + time * 30.0f, (float)WIN_W);
//        float y = GAME_Y0 + fmodf(i * 61.7f + time * 13.0f, (float)(GAME_Y1 - GAME_Y0));
//        glPointSize((i % 6 == 0) ? 2.8f : 1.4f);
//        glBegin(GL_POINTS); glVertex2f(x, y); glEnd();
//    }
//}

// -------------------------------
// Main loop & callbacks
// -------------------------------
void displayWrapper() { display(); }
void idleWrapper() { idle(); }

int main(int argc, char** argv) {
    srand((unsigned)time(NULL));
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(WIN_W, WIN_H);
    glutCreateWindow("Space Explorer - Final");

    initGame();

    glutDisplayFunc(displayWrapper);
    glutIdleFunc(idleWrapper);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialDown);
    glutSpecialUpFunc(specialUp);
    glutMouseFunc(mouseClick);

    glutMainLoop();
    return 0;
}
