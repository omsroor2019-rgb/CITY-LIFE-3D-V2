/*
 * CITY LIFE 3D — NOVA BAY EDITION (V2)
 * Original mobile open-city driving and delivery game.
 * C99 + raylib 5.5 + KarvinJ/raylib-mobile Android template.
 * This project is not affiliated with Rockstar Games or GTA.
 *
 * V2 highlights:
 * - Larger procedural city with several building styles and a central plaza.
 * - Detailed low-poly buildings, storefronts, windows, balconies and rooftops.
 * - Third-person character with animated limbs, sprint and jump.
 * - Player car with enter/exit, steering, acceleration, braking and collisions.
 * - Moving traffic and walking pedestrians.
 * - Three delivery missions, cash rewards and an objective marker.
 * - Mobile virtual joystick, contextual action, jump, sprint, gas and brake.
 * - Custom title, pause screen, HUD, minimap and speed display.
 *
 * All world art is procedurally drawn from raylib primitives. No external
 * model, texture or audio files are required for this build.
 */

#include "raymob.h"
#include "rlgl.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define PI_F                 3.14159265358979323846f
#define RAD_TO_DEG           57.29577951308232f
#define CITY_HALF            192.0f
#define CITY_BLOCK           48.0f
#define CITY_ROAD_COUNT      9
#define MAX_BUILDINGS        256
#define WALKER_COUNT         22
#define TRAFFIC_COUNT        12
#define MISSION_COUNT        6
#define DRAW_DISTANCE        158.0f
#define PLAYER_RADIUS        0.62f
#define CAR_RADIUS           1.55f

/* ---- Palette ----------------------------------------------------------- */
static const Color C_SKY          = { 111, 183, 221, 255 };
static const Color C_GRASS        = { 79, 143, 91, 255 };
static const Color C_GRASS_DARK   = { 57, 112, 75, 255 };
static const Color C_ROAD         = { 43, 49, 58, 255 };
static const Color C_ROAD_EDGE    = { 65, 72, 82, 255 };
static const Color C_SIDEWALK     = { 157, 157, 148, 255 };
static const Color C_CURB         = { 205, 202, 188, 255 };
static const Color C_LANE         = { 246, 194, 69, 255 };
static const Color C_WINDOW       = { 103, 174, 202, 255 };
static const Color C_WINDOW_LIT   = { 246, 205, 131, 255 };
static const Color C_GLASS_DARK   = { 50, 95, 120, 255 };
static const Color C_HUD          = { 12, 20, 31, 232 };
static const Color C_HUD_SOFT     = { 18, 29, 43, 222 };
static const Color C_GOLD         = { 255, 194, 66, 255 };
static const Color C_MINT         = { 91, 220, 174, 255 };
static const Color C_WHITE        = { 245, 247, 249, 255 };
static const Color C_RED          = { 225, 84, 74, 255 };
static const Color C_BLUE         = { 64, 143, 222, 255 };
static const Color C_DARK         = { 22, 28, 39, 255 };

/* ---- Data -------------------------------------------------------------- */
typedef struct {
    Vector3 position;
    float width, depth, height;
    int style;
    Color wall;
    Color trim;
    Color sign;
    unsigned int seed;
} Building;

typedef struct {
    Vector3 position;
    float yaw;
    float speed;
    float steering;
    Color paint;
    int model;
} Vehicle;

typedef struct {
    Vector3 position;
    float yaw;
    float phase;
    float speed;
    float routeMin;
    float routeMax;
    bool alongX;
    int direction;
    Color shirt;
    Color trousers;
    Color skin;
} Pedestrian;

typedef struct {
    float x;
    float y;
} MoveInput;

typedef enum {
    GAME_TITLE = 0,
    GAME_PLAYING,
    GAME_PAUSED
} GameState;

/* ---- Global city data -------------------------------------------------- */
static Building gBuildings[MAX_BUILDINGS];
static int gBuildingCount = 0;
static Pedestrian gPedestrians[WALKER_COUNT];
static Vehicle gTraffic[TRAFFIC_COUNT];

/* ---- Small helpers ----------------------------------------------------- */
static float ClampF(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float LerpF(float a, float b, float t) {
    return a + (b - a) * t;
}

static float Distance2D(float x1, float z1, float x2, float z2) {
    float dx = x1 - x2;
    float dz = z1 - z2;
    return sqrtf(dx*dx + dz*dz);
}

static float NormalizeAngle(float a) {
    while (a > PI_F) a -= 2.0f*PI_F;
    while (a < -PI_F) a += 2.0f*PI_F;
    return a;
}

static Color ColorScale(Color c, float factor) {
    Color out = c;
    out.r = (unsigned char)ClampF((float)c.r*factor, 0.0f, 255.0f);
    out.g = (unsigned char)ClampF((float)c.g*factor, 0.0f, 255.0f);
    out.b = (unsigned char)ClampF((float)c.b*factor, 0.0f, 255.0f);
    return out;
}

static unsigned int Hash2(int a, int b, int c) {
    unsigned int h = (unsigned int)(a*73856093u) ^
                     (unsigned int)(b*19349663u) ^
                     (unsigned int)(c*83492791u);
    h ^= h >> 13;
    h *= 1274126177u;
    h ^= h >> 16;
    return h;
}

static bool PointInRect(Vector2 p, Rectangle r) {
    return p.x >= r.x && p.x <= r.x + r.width &&
           p.y >= r.y && p.y <= r.y + r.height;
}

static bool TouchHeld(Rectangle r) {
    int count = GetTouchPointCount();
    for (int i = 0; i < count; i++) {
        if (PointInRect(GetTouchPosition(i), r)) return true;
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && PointInRect(GetMousePosition(), r)) return true;
    return false;
}

static bool TouchPressed(Rectangle r, bool *wasDown) {
    bool down = TouchHeld(r);
    bool pressed = down && !(*wasDown);
    *wasDown = down;
    return pressed;
}

static void DrawPanel(Rectangle r, Color fill, Color outline) {
    DrawRectangleRounded(r, 0.12f, 8, fill);
    DrawRectangleRoundedLines(r, 0.12f, 8, outline);
}

static void DrawButton(Rectangle r, const char *label, Color fill, bool active) {
    Color face = active ? ColorScale(fill, 1.23f) : fill;
    DrawRectangleRounded(r, 0.20f, 9, Fade(face, 0.91f));
    DrawRectangleRoundedLines(r, 0.20f, 9, Fade(C_WHITE, 0.82f));
    int size = (r.width < 82.0f) ? 15 : 17;
    int tw = MeasureText(label, size);
    DrawText(label, (int)(r.x + (r.width - (float)tw)*0.5f),
             (int)(r.y + (r.height - (float)size)*0.5f), size, C_WHITE);
}

/* ---- City generation --------------------------------------------------- */
static void AddBuilding(float x, float z, float w, float d, float h,
                        int style, unsigned int seed) {
    if (gBuildingCount >= MAX_BUILDINGS) return;

    static const Color walls[] = {
        { 206, 188, 165, 255 }, { 182, 194, 200, 255 },
        { 210, 171, 151, 255 }, { 170, 186, 176, 255 },
        { 190, 177, 209, 255 }, { 220, 205, 168, 255 },
        { 146, 171, 190, 255 }, { 204, 179, 185, 255 }
    };
    static const Color trims[] = {
        { 87, 100, 112, 255 }, { 73, 90, 103, 255 },
        { 116, 91, 75, 255 }, { 83, 105, 94, 255 }
    };
    static const Color signs[] = {
        { 40, 105, 151, 255 }, { 181, 76, 65, 255 },
        { 44, 133, 105, 255 }, { 126, 82, 156, 255 }
    };

    Building *b = &gBuildings[gBuildingCount++];
    b->position = (Vector3){ x, 0.0f, z };
    b->width = w;
    b->depth = d;
    b->height = h;
    b->style = style;
    b->seed = seed;
    b->wall = walls[seed % (sizeof(walls)/sizeof(walls[0]))];
    b->trim = trims[(seed >> 4) % (sizeof(trims)/sizeof(trims[0]))];
    b->sign = signs[(seed >> 7) % (sizeof(signs)/sizeof(signs[0]))];
}

static void BuildCity(void) {
    gBuildingCount = 0;

    /* Eight by eight city blocks; the centre block becomes a public plaza. */
    for (int bx = -4; bx < 4; bx++) {
        for (int bz = -4; bz < 4; bz++) {
            if (bx == 0 && bz == 0) continue;

            float cx = (float)bx*CITY_BLOCK + CITY_BLOCK*0.5f;
            float cz = (float)bz*CITY_BLOCK + CITY_BLOCK*0.5f;

            for (int sx = -1; sx <= 1; sx += 2) {
                for (int sz = -1; sz <= 1; sz += 2) {
                    unsigned int seed = Hash2(bx, bz, (sx+2)*5 + (sz+2));
                    int style = (int)(seed % 4u);
                    float h = 13.0f + (float)((seed >> 5) % 8u)*3.1f;
                    if (style == 2) h += 12.0f;
                    if (style == 3) h = 10.0f + (float)((seed >> 9) % 4u)*2.0f;
                    float width = 11.5f + (float)((seed >> 12) % 4u)*0.75f;
                    float depth = 11.5f + (float)((seed >> 15) % 4u)*0.75f;
                    AddBuilding(cx + (float)sx*9.0f,
                                cz + (float)sz*9.0f,
                                width, depth, h, style, seed);
                }
            }
        }
    }
}

static bool HitsBuilding(float x, float z, float radius) {
    for (int i = 0; i < gBuildingCount; i++) {
        const Building *b = &gBuildings[i];
        float minX = b->position.x - b->width*0.5f;
        float maxX = b->position.x + b->width*0.5f;
        float minZ = b->position.z - b->depth*0.5f;
        float maxZ = b->position.z + b->depth*0.5f;
        float nearX = ClampF(x, minX, maxX);
        float nearZ = ClampF(z, minZ, maxZ);
        if (Distance2D(x, z, nearX, nearZ) < radius) return true;
    }
    return false;
}

static void MoveWithCollision(Vector3 *p, float dx, float dz, float radius) {
    float nx = ClampF(p->x + dx, -CITY_HALF + 4.0f, CITY_HALF - 4.0f);
    if (!HitsBuilding(nx, p->z, radius)) p->x = nx;

    float nz = ClampF(p->z + dz, -CITY_HALF + 4.0f, CITY_HALF - 4.0f);
    if (!HitsBuilding(p->x, nz, radius)) p->z = nz;
}

/* ---- World rendering --------------------------------------------------- */
static void DrawRoadNetwork(void) {
    /* City ground. */
    DrawPlane((Vector3){0.0f, -0.12f, 0.0f},
              (Vector2){430.0f, 430.0f}, C_GRASS);

    /* A small waterfront beyond the north end of the city. */
    DrawCube((Vector3){0.0f, -0.08f, -224.0f},
             430.0f, 0.12f, 72.0f, (Color){ 65, 151, 190, 255 });
    DrawCube((Vector3){0.0f, -0.01f, -188.0f},
             430.0f, 0.14f, 5.0f, (Color){ 222, 196, 145, 255 });
    DrawCube((Vector3){0.0f, 0.025f, -180.0f},
             430.0f, 0.11f, 2.0f, C_SIDEWALK);

    for (int i = 0; i < CITY_ROAD_COUNT; i++) {
        float c = -CITY_HALF + (float)i*CITY_BLOCK;
        float width = (i == 4) ? 17.0f : 14.0f;
        float sidewalk = 3.0f;

        /* Road surfaces. */
        DrawCube((Vector3){c, 0.0f, 0.0f}, width, 0.12f, 410.0f, C_ROAD);
        DrawCube((Vector3){0.0f, 0.0f, c}, 410.0f, 0.12f, width, C_ROAD);

        /* Road shoulders and sidewalks on both sides. */
        for (int side = -1; side <= 1; side += 2) {
            float offset = (width*0.5f) + sidewalk*0.5f;
            DrawCube((Vector3){c + (float)side*offset, 0.015f, 0.0f},
                     sidewalk, 0.15f, 410.0f, C_SIDEWALK);
            DrawCube((Vector3){0.0f, 0.015f, c + (float)side*offset},
                     410.0f, 0.15f, sidewalk, C_SIDEWALK);
            DrawCube((Vector3){c + (float)side*(width*0.5f + sidewalk), 0.10f, 0.0f},
                     0.22f, 0.18f, 410.0f, C_CURB);
            DrawCube((Vector3){0.0f, 0.10f, c + (float)side*(width*0.5f + sidewalk)},
                     410.0f, 0.18f, 0.22f, C_CURB);
        }

        /* Broken centre lines. */
        for (int m = -190; m <= 190; m += 18) {
            DrawCube((Vector3){c, 0.075f, (float)m}, 0.20f, 0.025f, 7.0f, C_LANE);
            DrawCube((Vector3){(float)m, 0.075f, c}, 7.0f, 0.025f, 0.20f, C_LANE);
        }
    }

    /* Crosswalk bars at the central civic plaza. */
    for (int i = -4; i <= 4; i++) {
        float p = (float)i*1.55f;
        DrawCube((Vector3){p, 0.09f, 8.8f}, 0.72f, 0.025f, 3.4f, C_WHITE);
        DrawCube((Vector3){8.8f, 0.09f, p}, 3.4f, 0.025f, 0.72f, C_WHITE);
        DrawCube((Vector3){p, 0.09f, -8.8f}, 0.72f, 0.025f, 3.4f, C_WHITE);
        DrawCube((Vector3){-8.8f, 0.09f, p}, 3.4f, 0.025f, 0.72f, C_WHITE);
    }
}

static void DrawBuildingWindows(const Building *b, float distance) {
    if (distance > DRAW_DISTANCE) return;

    int floors = (int)(b->height/3.25f);
    if (floors < 2) floors = 2;
    if (floors > 10) floors = 10;

    float windowY;
    float windowW = (b->style == 2) ? 1.55f : 1.35f;
    float windowH = (b->style == 2) ? 1.55f : 1.25f;
    float sideOffset = b->width*0.5f + 0.055f;
    float frontOffset = b->depth*0.5f + 0.055f;
    int columns = (b->width > 13.0f) ? 3 : 2;

    for (int f = 0; f < floors; f++) {
        windowY = 2.1f + (float)f*3.05f;
        if (windowY + 0.8f > b->height - 0.8f) break;

        for (int col = 0; col < columns; col++) {
            float x = b->position.x + ((float)col - (float)(columns-1)*0.5f)*3.0f;
            float z1 = b->position.z + frontOffset;
            float z2 = b->position.z - frontOffset;
            unsigned int lit = Hash2((int)b->position.x + col, (int)b->position.z + f, b->style);
            Color glass = ((lit % 7u) == 0u) ? C_WINDOW_LIT : C_WINDOW;
            DrawCube((Vector3){x, windowY, z1}, windowW, windowH, 0.09f, glass);
            DrawCube((Vector3){x, windowY, z2}, windowW, windowH, 0.09f, glass);
        }

        /* Side windows on glass and office buildings. */
        if (b->style == 1 || b->style == 2) {
            for (int col = -1; col <= 1; col += 2) {
                float z = b->position.z + (float)col*2.5f;
                DrawCube((Vector3){b->position.x + sideOffset, windowY, z},
                         0.09f, windowH, windowW, C_GLASS_DARK);
                DrawCube((Vector3){b->position.x - sideOffset, windowY, z},
                         0.09f, windowH, windowW, C_GLASS_DARK);
            }
        }
    }
}

static void DrawBuilding(const Building *b, Vector3 focus) {
    float distance = Distance2D(focus.x, focus.z, b->position.x, b->position.z);
    if (distance > DRAW_DISTANCE) return;

    Vector3 center = {b->position.x, b->height*0.5f, b->position.z};
    DrawCube(center, b->width, b->height, b->depth, b->wall);
    DrawCubeWires(center, b->width, b->height, b->depth,
                  (Color){ 65, 70, 77, 255 });

    /* Contrasting roof cap and vertical facade strips. */
    DrawCube((Vector3){b->position.x, b->height + 0.20f, b->position.z},
             b->width + 0.45f, 0.40f, b->depth + 0.45f, b->trim);
    DrawCube((Vector3){b->position.x - b->width*0.5f + 0.30f,
                       b->height*0.5f, b->position.z},
             0.32f, b->height, b->depth, ColorScale(b->wall, 0.82f));

    DrawBuildingWindows(b, distance);

    /* Shopfront / awning gives street level a recognizable silhouette. */
    if (b->style == 3) {
        DrawCube((Vector3){b->position.x, 1.55f, b->position.z + b->depth*0.5f + 0.15f},
                 b->width*0.78f, 1.65f, 0.14f, C_GLASS_DARK);
        DrawCube((Vector3){b->position.x, 3.05f, b->position.z + b->depth*0.5f + 0.48f},
                 b->width*0.90f, 0.48f, 0.72f, b->sign);
        DrawCube((Vector3){b->position.x, 0.34f, b->position.z + b->depth*0.5f + 0.38f},
                 b->width*0.94f, 0.18f, 0.72f, b->trim);
    } else {
        DrawCube((Vector3){b->position.x, 1.45f, b->position.z + b->depth*0.5f + 0.06f},
                 b->width*0.72f, 1.35f, 0.10f, C_WINDOW);
    }

    /* Small balconies on selected residential towers. */
    if (b->style == 0 || b->style == 1) {
        for (int f = 1; f < 4; f++) {
            float y = 2.7f + (float)f*6.0f;
            if (y < b->height - 2.0f && ((b->seed >> (unsigned int)f) & 1u)) {
                DrawCube((Vector3){b->position.x, y, b->position.z + b->depth*0.5f + 0.55f},
                         b->width*0.55f, 0.13f, 1.0f, b->trim);
                DrawCube((Vector3){b->position.x, y + 0.45f, b->position.z + b->depth*0.5f + 1.0f},
                         b->width*0.55f, 0.12f, 0.10f, C_WHITE);
            }
        }
    }

    /* Rooftop water tank / antenna. */
    if ((b->seed % 3u) == 0u) {
        DrawCylinder((Vector3){b->position.x + b->width*0.22f,
                               b->height + 0.85f,
                               b->position.z - b->depth*0.18f},
                     0.72f, 0.72f, 1.30f, 10, (Color){ 85, 111, 128, 255 });
    } else if ((b->seed % 4u) == 0u) {
        DrawCube((Vector3){b->position.x, b->height + 1.6f, b->position.z},
                 0.18f, 2.8f, 0.18f, b->trim);
        DrawSphere((Vector3){b->position.x, b->height + 3.05f, b->position.z},
                   0.23f, C_RED);
    }
}

static void DrawTree(float x, float z, float scale) {
    DrawCylinder((Vector3){x, 0.95f*scale, z},
                 0.22f*scale, 0.28f*scale, 1.9f*scale, 8,
                 (Color){ 119, 82, 53, 255 });
    DrawSphere((Vector3){x, 2.15f*scale, z}, 1.18f*scale,
               (Color){ 51, 133, 76, 255 });
    DrawSphere((Vector3){x - 0.58f*scale, 2.0f*scale, z + 0.12f*scale},
               0.70f*scale, (Color){ 66, 151, 84, 255 });
    DrawSphere((Vector3){x + 0.55f*scale, 2.08f*scale, z - 0.18f*scale},
               0.72f*scale, (Color){ 43, 119, 69, 255 });
}

static void DrawStreetLamp(float x, float z) {
    DrawCylinder((Vector3){x, 3.2f, z}, 0.10f, 0.10f, 6.4f, 8,
                 (Color){ 63, 72, 82, 255 });
    DrawCube((Vector3){x + 0.68f, 6.25f, z}, 1.45f, 0.12f, 0.12f,
             (Color){ 63, 72, 82, 255 });
    DrawCube((Vector3){x + 1.23f, 6.05f, z}, 0.52f, 0.18f, 0.52f,
             (Color){ 244, 218, 151, 255 });
}

static void DrawCentralPlaza(float time) {
    DrawCube((Vector3){24.0f, 0.04f, 24.0f}, 31.0f, 0.18f, 31.0f,
             (Color){ 176, 176, 162, 255 });
    DrawCube((Vector3){24.0f, 0.15f, 24.0f}, 19.0f, 0.12f, 19.0f,
             (Color){ 202, 193, 170, 255 });

    /* Fountain with a softly moving water column. */
    DrawCylinder((Vector3){24.0f, 0.60f, 24.0f}, 4.8f, 4.8f, 0.65f,
                 32, (Color){ 91, 114, 126, 255 });
    DrawCylinder((Vector3){24.0f, 1.02f, 24.0f}, 3.9f, 3.9f, 0.18f,
                 32, (Color){ 92, 173, 196, 255 });
    float waterHeight = 1.5f + 0.18f*sinf(time*2.4f);
    DrawCylinder((Vector3){24.0f, 1.65f + waterHeight*0.5f, 24.0f},
                 0.52f, 0.08f, waterHeight, 16,
                 (Color){ 126, 202, 221, 255 });
    DrawSphere((Vector3){24.0f, 1.55f + waterHeight, 24.0f}, 0.28f,
               (Color){ 176, 230, 237, 255 });

    for (int i = 0; i < 4; i++) {
        float a = (float)i*PI_F*0.5f;
        float x = 24.0f + cosf(a)*11.2f;
        float z = 24.0f + sinf(a)*11.2f;
        DrawCube((Vector3){x, 0.65f, z}, 4.2f, 0.55f, 0.70f,
                 (Color){ 110, 79, 57, 255 });
        DrawCube((Vector3){x, 0.98f, z}, 4.2f, 0.12f, 0.76f,
                 (Color){ 183, 143, 101, 255 });
    }

    DrawTree(10.0f, 10.0f, 0.92f);
    DrawTree(38.0f, 10.0f, 0.92f);
    DrawTree(10.0f, 38.0f, 0.92f);
    DrawTree(38.0f, 38.0f, 0.92f);
}

/* ---- Vehicles ---------------------------------------------------------- */
static void DrawVehicleModel(const Vehicle *v, bool playerVehicle) {
    rlPushMatrix();
    rlTranslatef(v->position.x, v->position.y, v->position.z);
    rlRotatef(v->yaw*RAD_TO_DEG, 0.0f, 1.0f, 0.0f);

    float bodyY = (v->model == 1) ? 0.82f : 0.78f;
    float bodyW = (v->model == 1) ? 2.18f : 1.92f;
    float bodyL = (v->model == 1) ? 4.65f : 4.05f;
    Color paint = v->paint;
    Color glass = (Color){ 65, 121, 151, 255 };

    /* Main body, hood and trunk. */
    DrawCube((Vector3){0.0f, bodyY, 0.0f}, bodyW, 0.72f, bodyL, paint);
    DrawCubeWires((Vector3){0.0f, bodyY, 0.0f}, bodyW, 0.72f, bodyL,
                  ColorScale(paint, 0.62f));
    DrawCube((Vector3){0.0f, 1.18f, -1.12f}, bodyW*0.83f, 0.28f, 1.1f,
             ColorScale(paint, 1.12f));
    DrawCube((Vector3){0.0f, 1.17f, 1.38f}, bodyW*0.82f, 0.25f, 0.76f,
             ColorScale(paint, 0.88f));

    /* Cabin and windows. */
    DrawCube((Vector3){0.0f, 1.47f, 0.24f}, bodyW*0.78f, 0.82f, 2.0f,
             ColorScale(paint, 0.94f));
    DrawCube((Vector3){0.0f, 1.52f, -0.79f}, bodyW*0.72f, 0.58f, 0.08f, glass);
    DrawCube((Vector3){0.0f, 1.52f, 1.27f}, bodyW*0.70f, 0.55f, 0.08f, glass);
    DrawCube((Vector3){-bodyW*0.405f, 1.51f, 0.20f}, 0.08f, 0.58f, 1.45f, glass);
    DrawCube((Vector3){ bodyW*0.405f, 1.51f, 0.20f}, 0.08f, 0.58f, 1.45f, glass);

    /* Headlights, tail lamps and bumpers. */
    DrawCube((Vector3){-0.66f, 0.91f, -bodyL*0.5f - 0.035f},
             0.42f, 0.22f, 0.09f, (Color){ 255, 240, 190, 255 });
    DrawCube((Vector3){ 0.66f, 0.91f, -bodyL*0.5f - 0.035f},
             0.42f, 0.22f, 0.09f, (Color){ 255, 240, 190, 255 });
    DrawCube((Vector3){-0.67f, 0.91f, bodyL*0.5f + 0.035f},
             0.38f, 0.24f, 0.09f, C_RED);
    DrawCube((Vector3){ 0.67f, 0.91f, bodyL*0.5f + 0.035f},
             0.38f, 0.24f, 0.09f, C_RED);
    DrawCube((Vector3){0.0f, 0.47f, -bodyL*0.5f - 0.08f}, bodyW*0.94f, 0.18f, 0.18f,
             (Color){ 64, 68, 75, 255 });
    DrawCube((Vector3){0.0f, 0.47f, bodyL*0.5f + 0.08f}, bodyW*0.94f, 0.18f, 0.18f,
             (Color){ 64, 68, 75, 255 });

    /* Four low-poly tyres and silver hubs. */
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sz = -1; sz <= 1; sz += 2) {
            float x = (float)sx*(bodyW*0.51f);
            float z = (float)sz*(bodyL*0.31f);
            DrawSphere((Vector3){x, 0.50f, z}, 0.43f,
                       (Color){ 24, 28, 34, 255 });
            DrawSphere((Vector3){x*1.015f, 0.50f, z}, 0.20f,
                       (Color){ 157, 166, 174, 255 });
        }
    }

    if (playerVehicle) {
        DrawCube((Vector3){0.0f, 1.91f, -0.15f}, 0.18f, 0.08f, 0.32f,
                 (Color){ 235, 235, 235, 255 });
    }
    rlPopMatrix();
}

static void InitTraffic(void) {
    static const Color paints[6] = {
        { 219, 87, 70, 255 }, { 54, 120, 177, 255 },
        { 224, 221, 208, 255 }, { 51, 62, 73, 255 },
        { 205, 165, 75, 255 }, { 93, 156, 119, 255 }
    };

    for (int i = 0; i < TRAFFIC_COUNT; i++) {
        Vehicle *v = &gTraffic[i];
        int road = (i%5) - 2;
        float coord = (float)road*48.0f;
        v->speed = 5.0f + (float)(i%4)*1.35f;
        v->paint = paints[i%6];
        v->model = i%3;
        v->position.y = 0.0f;
        if ((i%2) == 0) {
            v->position.x = -170.0f + (float)i*25.0f;
            v->position.z = coord + ((i%4 < 2) ? -3.2f : 3.2f);
            v->yaw = (i%4 < 2) ? -PI_F*0.5f : PI_F*0.5f;
        } else {
            v->position.x = coord + ((i%4 < 2) ? -3.2f : 3.2f);
            v->position.z = -170.0f + (float)i*25.0f;
            v->yaw = (i%4 < 2) ? 0.0f : PI_F;
        }
    }
}

static void UpdateTraffic(float dt) {
    for (int i = 0; i < TRAFFIC_COUNT; i++) {
        Vehicle *v = &gTraffic[i];
        float dx = -sinf(v->yaw)*v->speed*dt;
        float dz = -cosf(v->yaw)*v->speed*dt;
        v->position.x += dx;
        v->position.z += dz;
        if (v->position.x > CITY_HALF + 8.0f) v->position.x = -CITY_HALF - 8.0f;
        if (v->position.x < -CITY_HALF - 8.0f) v->position.x = CITY_HALF + 8.0f;
        if (v->position.z > CITY_HALF + 8.0f) v->position.z = -CITY_HALF - 8.0f;
        if (v->position.z < -CITY_HALF - 8.0f) v->position.z = CITY_HALF + 8.0f;
    }
}

/* ---- People ------------------------------------------------------------ */
static void DrawHuman(Vector3 p, float yaw, float phase,
                      Color shirt, Color trousers, Color skin,
                      bool isPlayer, bool running) {
    rlPushMatrix();
    rlTranslatef(p.x, p.y, p.z);
    rlRotatef(yaw*RAD_TO_DEG, 0.0f, 1.0f, 0.0f);

    float swing = (running ? 32.0f : 20.0f)*sinf(phase);
    float bob = (running ? 0.075f : 0.035f)*fabsf(sinf(phase));
    Color shoe = (Color){ 34, 39, 46, 255 };
    Color hair = (Color){ 47, 39, 35, 255 };

    /* Torso, collar and belt. */
    DrawCube((Vector3){0.0f, 1.12f + bob, 0.0f}, 0.66f, 0.86f, 0.38f, shirt);
    DrawCube((Vector3){0.0f, 0.68f + bob, 0.0f}, 0.62f, 0.12f, 0.40f,
             (Color){ 54, 48, 47, 255 });
    DrawCube((Vector3){0.0f, 1.57f + bob, -0.01f}, 0.34f, 0.14f, 0.34f, skin);

    /* Head, hair and face. */
    DrawSphere((Vector3){0.0f, 1.86f + bob, 0.0f}, 0.31f, skin);
    DrawSphere((Vector3){0.0f, 2.04f + bob, 0.035f}, 0.285f, hair);
    DrawCube((Vector3){0.0f, 1.86f + bob, -0.274f}, 0.12f, 0.055f, 0.035f,
             (Color){ 44, 43, 43, 255 });

    /* Arms pivot from shoulders and swing while walking. */
    rlPushMatrix();
    rlTranslatef(-0.40f, 1.43f + bob, 0.0f);
    rlRotatef(swing, 1.0f, 0.0f, 0.0f);
    DrawCube((Vector3){0.0f, -0.34f, 0.0f}, 0.20f, 0.68f, 0.23f, shirt);
    DrawSphere((Vector3){0.0f, -0.72f, -0.01f}, 0.12f, skin);
    rlPopMatrix();

    rlPushMatrix();
    rlTranslatef(0.40f, 1.43f + bob, 0.0f);
    rlRotatef(-swing, 1.0f, 0.0f, 0.0f);
    DrawCube((Vector3){0.0f, -0.34f, 0.0f}, 0.20f, 0.68f, 0.23f, shirt);
    DrawSphere((Vector3){0.0f, -0.72f, -0.01f}, 0.12f, skin);
    rlPopMatrix();

    /* Legs and shoes. */
    rlPushMatrix();
    rlTranslatef(-0.18f, 0.67f + bob, 0.0f);
    rlRotatef(-swing*0.72f, 1.0f, 0.0f, 0.0f);
    DrawCube((Vector3){0.0f, -0.30f, 0.0f}, 0.25f, 0.60f, 0.29f, trousers);
    DrawCube((Vector3){0.0f, -0.59f, -0.10f}, 0.28f, 0.16f, 0.42f, shoe);
    rlPopMatrix();

    rlPushMatrix();
    rlTranslatef(0.18f, 0.67f + bob, 0.0f);
    rlRotatef(swing*0.72f, 1.0f, 0.0f, 0.0f);
    DrawCube((Vector3){0.0f, -0.30f, 0.0f}, 0.25f, 0.60f, 0.29f, trousers);
    DrawCube((Vector3){0.0f, -0.59f, -0.10f}, 0.28f, 0.16f, 0.42f, shoe);
    rlPopMatrix();

    if (isPlayer) {
        DrawCube((Vector3){0.0f, 1.12f + bob, 0.25f}, 0.42f, 0.48f, 0.20f,
                 (Color){ 42, 57, 80, 255 });
    }
    rlPopMatrix();
}

static void InitPedestrians(void) {
    static const Color shirts[] = {
        { 42, 116, 185, 255 }, { 195, 82, 73, 255 },
        { 68, 143, 104, 255 }, { 192, 146, 63, 255 },
        { 120, 91, 165, 255 }, { 220, 220, 213, 255 },
        { 56, 82, 105, 255 }
    };
    static const Color pants[] = {
        { 44, 55, 73, 255 }, { 84, 84, 80, 255 },
        { 90, 102, 116, 255 }, { 70, 64, 58, 255 }
    };

    for (int i = 0; i < WALKER_COUNT; i++) {
        Pedestrian *p = &gPedestrians[i];
        p->alongX = ((i%2) == 0);
        p->direction = ((i%3) == 0) ? -1 : 1;
        p->routeMin = -180.0f;
        p->routeMax = 180.0f;
        p->speed = 1.0f + (float)(i%5)*0.27f;
        p->phase = (float)i*0.83f;
        p->shirt = shirts[i%7];
        p->trousers = pants[i%4];
        p->skin = (Color){
            (unsigned char)(183 + (i%4)*11),
            (unsigned char)(132 + (i%3)*12),
            (unsigned char)(101 + (i%4)*9), 255
        };

        if (p->alongX) {
            int road = (i/2)%7 - 3;
            p->position = (Vector3){ -160.0f + (float)i*13.0f,
                                     0.0f,
                                     (float)road*48.0f + ((i%4 < 2) ? -10.2f : 10.2f) };
            p->yaw = (p->direction > 0) ? -PI_F*0.5f : PI_F*0.5f;
        } else {
            int road = (i/2)%7 - 3;
            p->position = (Vector3){ (float)road*48.0f + ((i%4 < 2) ? -10.2f : 10.2f),
                                     0.0f,
                                     -160.0f + (float)i*13.0f };
            p->yaw = (p->direction > 0) ? PI_F : 0.0f;
        }
    }
}

static void UpdatePedestrians(float dt) {
    for (int i = 0; i < WALKER_COUNT; i++) {
        Pedestrian *p = &gPedestrians[i];
        float movement = p->speed*(float)p->direction*dt;
        if (p->alongX) p->position.x += movement;
        else p->position.z += movement;
        p->phase += dt*7.0f;

        float value = p->alongX ? p->position.x : p->position.z;
        if (value > p->routeMax) {
            if (p->alongX) p->position.x = p->routeMin;
            else p->position.z = p->routeMin;
        } else if (value < p->routeMin) {
            if (p->alongX) p->position.x = p->routeMax;
            else p->position.z = p->routeMax;
        }
    }
}

/* ---- Missions ---------------------------------------------------------- */
static const Vector3 gMissionPoints[MISSION_COUNT] = {
    {  45.0f, 0.0f,  11.0f },
    {  98.0f, 0.0f, -48.0f },
    { -44.0f, 0.0f,  60.0f },
    { -96.0f, 0.0f, -52.0f },
    { 145.0f, 0.0f, -105.0f },
    { -145.0f, 0.0f, 144.0f }
};

static void DrawMissionMarker(Vector3 p, float time, bool pickup) {
    float pulse = 1.0f + 0.16f*sinf(time*3.0f);
    Color c = pickup ? C_MINT : C_GOLD;
    DrawCylinder((Vector3){p.x, 0.13f, p.z}, 1.35f*pulse, 1.35f*pulse,
                 0.18f, 24, Fade(c, 0.55f));
    DrawCylinderWires((Vector3){p.x, 0.15f, p.z}, 1.5f*pulse, 1.5f*pulse,
                      0.22f, 24, c);
    DrawCylinder((Vector3){p.x, 1.35f, p.z}, 0.12f, 0.12f, 2.2f,
                 12, Fade(c, 0.36f));
    DrawSphere((Vector3){p.x, 2.65f + 0.15f*sinf(time*2.0f), p.z},
               0.45f*pulse, c);
}

/* ---- Minimap and HUD --------------------------------------------------- */
static void DrawMinimap(Rectangle map, Vector3 actor, Vector3 target,
                        bool hasTarget, bool inCar) {
    DrawPanel(map, (Color){ 11, 19, 28, 226 }, (Color){ 153, 174, 188, 220 });
    Rectangle inner = {map.x + 9.0f, map.y + 9.0f, map.width - 18.0f, map.height - 18.0f};
    DrawRectangle((int)inner.x, (int)inner.y, (int)inner.width, (int)inner.height,
                  (Color){ 47, 78, 69, 255 });

    for (int i = -4; i <= 4; i++) {
        float x = inner.x + (float)(i + 4)*(inner.width/8.0f);
        float y = inner.y + (float)(i + 4)*(inner.height/8.0f);
        DrawRectangle((int)(x - 1.0f), (int)inner.y, 2, (int)inner.height,
                      (Color){ 85, 91, 96, 255 });
        DrawRectangle((int)inner.x, (int)(y - 1.0f), (int)inner.width, 2,
                      (Color){ 85, 91, 96, 255 });
    }

    float sx = inner.width/(CITY_HALF*2.0f);
    float sz = inner.height/(CITY_HALF*2.0f);
    float px = inner.x + (actor.x + CITY_HALF)*sx;
    float py = inner.y + (actor.z + CITY_HALF)*sz;
    if (hasTarget) {
        float tx = inner.x + (target.x + CITY_HALF)*sx;
        float ty = inner.y + (target.z + CITY_HALF)*sz;
        DrawCircle((int)tx, (int)ty, 3.8f, C_GOLD);
    }
    DrawCircle((int)px, (int)py, inCar ? 4.6f : 3.5f, C_MINT);
    DrawCircleLines((int)px, (int)py, 6.0f, C_WHITE);
}

static void DrawGameHUD(int screenW, int screenH, int cash,
                        int delivered, int missionStep, float speed,
                        bool inCar, bool nearCar, bool nearMission,
                        Vector3 actor, Vector3 target, bool hasTarget,
                        MoveInput stick, float stickX, float stickY,
                        float time) {
    DrawPanel((Rectangle){14.0f, 14.0f, 290.0f, 112.0f}, C_HUD, (Color){ 111, 142, 160, 220 });
    DrawText("NOVA BAY", 28, 23, 16, C_MINT);
    DrawText("CITY LIFE 3D", 28, 44, 25, C_WHITE);
    DrawText(TextFormat("CASH  $%d", cash), 28, 79, 18, C_GOLD);
    DrawText(TextFormat("DELIVERIES  %d / 3", delivered), 28, 103, 14, C_WHITE);

    Rectangle map = {(float)screenW - 160.0f, 14.0f, 146.0f, 146.0f};
    DrawMinimap(map, actor, target, hasTarget, inCar);

    DrawPanel((Rectangle){(float)screenW - 160.0f, 169.0f, 146.0f, 42.0f}, C_HUD, Fade(C_WHITE, 0.30f));
    DrawText(inCar ? TextFormat("SPEED  %03d", (int)fabsf(speed)) : "ON FOOT",
             screenW - 147, 181, 16, C_WHITE);

    /* Mission hint. */
    if (missionStep < MISSION_COUNT) {
        const char *task = ((missionStep%2) == 0) ? "PICK UP PACKAGE" : "DELIVER PACKAGE";
        DrawPanel((Rectangle){(float)screenW*0.5f - 128.0f, 16.0f, 256.0f, 44.0f},
                  (Color){ 14, 23, 35, 210 }, Fade(C_GOLD, 0.7f));
        DrawText(task, screenW/2 - MeasureText(task, 16)/2, 30, 16, C_GOLD);
    } else {
        const char *done = "ALL JOBS COMPLETE!";
        DrawPanel((Rectangle){(float)screenW*0.5f - 128.0f, 16.0f, 256.0f, 44.0f},
                  (Color){ 14, 23, 35, 210 }, Fade(C_MINT, 0.7f));
        DrawText(done, screenW/2 - MeasureText(done, 16)/2, 30, 16, C_MINT);
    }

    /* Virtual analogue stick. */
    DrawCircle((int)stickX, (int)stickY, 62.0f, Fade(C_DARK, 0.48f));
    DrawCircleLines((int)stickX, (int)stickY, 62.0f, Fade(C_WHITE, 0.85f));
    DrawCircle((int)(stickX + stick.x*34.0f),
               (int)(stickY - stick.y*34.0f), 24.0f,
               Fade(C_WHITE, 0.88f));

    Rectangle actionRect = {(float)screenW - 118.0f, (float)screenH - 254.0f, 100.0f, 58.0f};
    Rectangle jumpRect = {(float)screenW - 118.0f, (float)screenH - 326.0f, 100.0f, 58.0f};
    Rectangle gasRect = {(float)screenW - 112.0f, (float)screenH - 144.0f, 94.0f, 82.0f};
    Rectangle brakeRect = {(float)screenW - 218.0f, (float)screenH - 132.0f, 94.0f, 72.0f};
    Rectangle sprintRect = {18.0f, (float)screenH - 118.0f, 90.0f, 48.0f};

    const char *actionLabel = inCar ? "EXIT CAR" : (nearCar ? "ENTER CAR" : (nearMission ? "INTERACT" : "ACTION"));
    DrawButton(actionRect, actionLabel, C_BLUE, nearCar || nearMission);
    if (!inCar) DrawButton(jumpRect, "JUMP", (Color){ 67, 113, 168, 255 }, false);
    else DrawButton(jumpRect, "CAMERA", (Color){ 65, 90, 120, 255 }, false);
    DrawButton(gasRect, "GAS", (Color){ 34, 139, 91, 255 }, false);
    DrawButton(brakeRect, "BRAKE", (Color){ 163, 68, 64, 255 }, false);
    if (!inCar) DrawButton(sprintRect, "SPRINT", (Color){ 121, 94, 164, 255 }, false);

    DrawText("LEFT STICK  MOVE / STEER", 16, screenH - 24, 13, Fade(C_WHITE, 0.86f));
    (void)time;
}

/* ---- Main game --------------------------------------------------------- */
int main(void) {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(1280, 720, "CITY LIFE 3D - NOVA BAY");
    SetTargetFPS(60);

    BuildCity();
    InitPedestrians();
    InitTraffic();

    GameState state = GAME_TITLE;
    float worldTime = 0.0f;
    float walkPhase = 0.0f;
    float jumpVelocity = 0.0f;
    bool grounded = true;
    bool inCar = false;
    int cash = 0;
    int missionStep = 0;
    int delivered = 0;
    float messageTimer = 0.0f;
    char message[96] = "WELCOME TO NOVA BAY";

    Vector3 player = { 0.0f, 0.0f, 12.0f };
    float playerYaw = 0.0f;
    Vehicle playerCar = {
        { 0.0f, 0.0f, 4.0f }, 0.0f, 0.0f, 0.0f,
        { 206, 58, 67, 255 }, 0
    };

    Camera3D camera = {0};
    camera.position = (Vector3){ -8.0f, 7.0f, 20.0f };
    camera.target = (Vector3){ 0.0f, 1.2f, 12.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 58.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    bool actionWasDown = false;
    bool jumpWasDown = false;
    bool pauseWasDown = false;
    bool startWasDown = false;
    bool resumeWasDown = false;
    bool restartWasDown = false;

    while (!WindowShouldClose()) {
        float dt = ClampF(GetFrameTime(), 0.0f, 0.045f);
        worldTime += dt;
        int screenW = GetScreenWidth();
        int screenH = GetScreenHeight();

        Rectangle actionRect = {(float)screenW - 118.0f, (float)screenH - 254.0f, 100.0f, 58.0f};
        Rectangle jumpRect = {(float)screenW - 118.0f, (float)screenH - 326.0f, 100.0f, 58.0f};
        Rectangle gasRect = {(float)screenW - 112.0f, (float)screenH - 144.0f, 94.0f, 82.0f};
        Rectangle brakeRect = {(float)screenW - 218.0f, (float)screenH - 132.0f, 94.0f, 72.0f};
        Rectangle sprintRect = {18.0f, (float)screenH - 118.0f, 90.0f, 48.0f};
        Rectangle pauseRect = {(float)screenW - 54.0f, 218.0f, 40.0f, 38.0f};
        Rectangle playRect = {(float)screenW*0.5f - 128.0f, (float)screenH*0.62f, 256.0f, 66.0f};
        Rectangle resumeRect = {(float)screenW*0.5f - 115.0f, (float)screenH*0.53f, 230.0f, 58.0f};
        Rectangle restartRect = {(float)screenW*0.5f - 115.0f, (float)screenH*0.63f, 230.0f, 58.0f};

        if (state == GAME_TITLE) {
            bool start = TouchPressed(playRect, &startWasDown) || IsKeyPressed(KEY_ENTER);
            if (start) {
                state = GAME_PLAYING;
                messageTimer = 2.6f;
                snprintf(message, sizeof(message), "YOUR FIRST DELIVERY IS READY");
            }
        } else if (state == GAME_PAUSED) {
            bool resume = TouchPressed(resumeRect, &resumeWasDown) || IsKeyPressed(KEY_ENTER);
            bool restart = TouchPressed(restartRect, &restartWasDown);
            if (resume) state = GAME_PLAYING;
            if (restart) {
                player = (Vector3){0.0f, 0.0f, 12.0f};
                playerYaw = 0.0f;
                playerCar.position = (Vector3){0.0f, 0.0f, 4.0f};
                playerCar.speed = 0.0f;
                playerCar.yaw = 0.0f;
                cash = 0;
                missionStep = 0;
                delivered = 0;
                inCar = false;
                jumpVelocity = 0.0f;
                grounded = true;
                state = GAME_PLAYING;
            }
        } else {
            bool pauseDown = TouchHeld(pauseRect) || IsKeyDown(KEY_ESCAPE);
            bool pausePressed = pauseDown && !pauseWasDown;
            pauseWasDown = pauseDown;
            if (pausePressed) state = GAME_PAUSED;
        }

        Vector3 actor = inCar ? playerCar.position : player;
        Vector3 missionTarget = (missionStep < MISSION_COUNT) ? gMissionPoints[missionStep] : actor;
        bool hasTarget = missionStep < MISSION_COUNT;
        bool nearCar = !inCar && Distance2D(player.x, player.z,
                                             playerCar.position.x, playerCar.position.z) < 5.0f;
        bool nearMission = hasTarget &&
            Distance2D(actor.x, actor.z, missionTarget.x, missionTarget.z) < 5.2f;

        MoveInput stick = {0.0f, 0.0f};
        float stickX = (float)screenW*0.165f;
        float stickY = (float)screenH*0.79f;
        bool gasDown = false;
        bool brakeDown = false;
        bool sprintDown = false;
        bool jumpPressed = false;
        bool actionPressed = false;

        if (state == GAME_PLAYING) {
            /* Keyboard support for desktop testing. */
            if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) stick.x -= 1.0f;
            if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) stick.x += 1.0f;
            if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) stick.y += 1.0f;
            if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) stick.y -= 1.0f;

            /* Touch joystick is on the lower left; other buttons are right side. */
            for (int t = 0; t < GetTouchPointCount(); t++) {
                Vector2 p = GetTouchPosition(t);
                if (p.x < (float)screenW*0.46f && p.y > (float)screenH*0.40f) {
                    float dx = p.x - stickX;
                    float dy = p.y - stickY;
                    float len = sqrtf(dx*dx + dy*dy);
                    if (len > 8.0f) {
                        float scale = (len > 62.0f) ? 62.0f/len : 1.0f;
                        stick.x = ClampF(dx*scale/62.0f, -1.0f, 1.0f);
                        stick.y = ClampF(-dy*scale/62.0f, -1.0f, 1.0f);
                    }
                }
            }
            gasDown = TouchHeld(gasRect) || IsKeyDown(KEY_LEFT_SHIFT);
            brakeDown = TouchHeld(brakeRect) || IsKeyDown(KEY_SPACE);
            sprintDown = !inCar && (TouchHeld(sprintRect) || IsKeyDown(KEY_LEFT_CONTROL));
            jumpPressed = TouchPressed(jumpRect, &jumpWasDown) || IsKeyPressed(KEY_SPACE);
            actionPressed = TouchPressed(actionRect, &actionWasDown) || IsKeyPressed(KEY_E);

            stick.x = ClampF(stick.x, -1.0f, 1.0f);
            stick.y = ClampF(stick.y, -1.0f, 1.0f);

            if (!inCar) {
                if (actionPressed && nearCar) {
                    inCar = true;
                    playerCar.position = player;
                    playerCar.position.y = 0.0f;
                    playerCar.yaw = playerYaw;
                    snprintf(message, sizeof(message), "VEHICLE READY - DRIVE SAFE");
                    messageTimer = 2.0f;
                } else if (actionPressed && nearMission && hasTarget) {
                    if ((missionStep%2) == 0) {
                        missionStep++;
                        snprintf(message, sizeof(message), "PACKAGE COLLECTED - DELIVER IT");
                    } else {
                        missionStep++;
                        delivered++;
                        cash += 650;
                        snprintf(message, sizeof(message), "DELIVERY COMPLETE  +$650");
                    }
                    messageTimer = 2.6f;
                    if (missionStep >= MISSION_COUNT) {
                        snprintf(message, sizeof(message), "NOVA BAY THANKS YOU - ALL JOBS DONE");
                        messageTimer = 4.0f;
                    }
                }

                playerYaw = NormalizeAngle(playerYaw - stick.x*2.3f*dt);
                float speed = sprintDown ? 7.3f : 4.6f;
                float move = stick.y*speed;
                float dx = -sinf(playerYaw)*move*dt;
                float dz = -cosf(playerYaw)*move*dt;
                MoveWithCollision(&player, dx, dz, PLAYER_RADIUS);
                if (fabsf(stick.y) > 0.06f) walkPhase += dt*(sprintDown ? 13.0f : 9.0f);

                if (jumpPressed && grounded) {
                    jumpVelocity = 6.7f;
                    grounded = false;
                }
                jumpVelocity -= 18.0f*dt;
                player.y += jumpVelocity*dt;
                if (player.y <= 0.0f) {
                    player.y = 0.0f;
                    jumpVelocity = 0.0f;
                    grounded = true;
                }
            } else {
                if (actionPressed) {
                    inCar = false;
                    playerCar.speed = 0.0f;
                    player = playerCar.position;
                    player.x += cosf(playerCar.yaw)*3.2f;
                    player.z -= sinf(playerCar.yaw)*3.2f;
                    player.y = 0.0f;
                    playerYaw = playerCar.yaw;
                    if (HitsBuilding(player.x, player.z, PLAYER_RADIUS)) {
                        player.x = playerCar.position.x - cosf(playerCar.yaw)*3.2f;
                        player.z = playerCar.position.z + sinf(playerCar.yaw)*3.2f;
                    }
                    snprintf(message, sizeof(message), "YOU ARE ON FOOT");
                    messageTimer = 1.5f;
                } else {
                    float throttle = stick.y;
                    if (gasDown) throttle = 1.0f;
                    if (brakeDown) throttle = -0.50f;

                    playerCar.speed += throttle*13.5f*dt;
                    if (!gasDown && !brakeDown && fabsf(stick.y) < 0.05f) {
                        playerCar.speed *= (1.0f - ClampF(1.12f*dt, 0.0f, 0.9f));
                    }
                    if (brakeDown) {
                        if (playerCar.speed > 0.0f) playerCar.speed = fmaxf(0.0f, playerCar.speed - 25.0f*dt);
                        else playerCar.speed = fminf(0.0f, playerCar.speed + 25.0f*dt);
                    }
                    playerCar.speed = ClampF(playerCar.speed, -7.5f, 25.0f);
                    float turn = stick.x*1.55f*ClampF(fabsf(playerCar.speed)/4.0f, 0.18f, 1.1f)*dt;
                    playerCar.yaw = NormalizeAngle(playerCar.yaw - turn*(playerCar.speed < 0.0f ? -1.0f : 1.0f));

                    float dx = -sinf(playerCar.yaw)*playerCar.speed*dt;
                    float dz = -cosf(playerCar.yaw)*playerCar.speed*dt;
                    float oldX = playerCar.position.x;
                    float oldZ = playerCar.position.z;
                    MoveWithCollision(&playerCar.position, dx, dz, CAR_RADIUS);
                    if (fabsf(playerCar.position.x - (oldX + dx)) > 0.01f ||
                        fabsf(playerCar.position.z - (oldZ + dz)) > 0.01f) {
                        playerCar.speed *= -0.18f;
                        snprintf(message, sizeof(message), "BE CAREFUL - COLLISION");
                        messageTimer = 1.0f;
                    }
                }
            }

            UpdateTraffic(dt);
            UpdatePedestrians(dt);
            if (messageTimer > 0.0f) messageTimer -= dt;
        }

        /* Camera follows smoothly behind the player or vehicle. */
        actor = inCar ? playerCar.position : player;
        float actorYaw = inCar ? playerCar.yaw : playerYaw;
        Vector3 desiredTarget = { actor.x, actor.y + (inCar ? 1.35f : 1.35f), actor.z };
        Vector3 desiredCamera = {
            actor.x + sinf(actorYaw)*10.6f,
            actor.y + 6.7f,
            actor.z + cosf(actorYaw)*10.6f
        };
        if (state == GAME_TITLE) {
            desiredTarget = (Vector3){ 0.0f, 2.0f, 8.0f };
            desiredCamera = (Vector3){ 3.5f, 12.0f, 26.0f };
        }
        camera.target.x = LerpF(camera.target.x, desiredTarget.x, 1.0f - expf(-5.2f*dt));
        camera.target.y = LerpF(camera.target.y, desiredTarget.y, 1.0f - expf(-5.2f*dt));
        camera.target.z = LerpF(camera.target.z, desiredTarget.z, 1.0f - expf(-5.2f*dt));
        camera.position.x = LerpF(camera.position.x, desiredCamera.x, 1.0f - expf(-3.5f*dt));
        camera.position.y = LerpF(camera.position.y, desiredCamera.y, 1.0f - expf(-3.5f*dt));
        camera.position.z = LerpF(camera.position.z, desiredCamera.z, 1.0f - expf(-3.5f*dt));

        /* ---- Render 3D world ------------------------------------------- */
        BeginDrawing();
        ClearBackground(C_SKY);
        BeginMode3D(camera);

        DrawRoadNetwork();
        DrawCentralPlaza(worldTime);

        /* Buildings are distance-culled around the current actor. */
        for (int i = 0; i < gBuildingCount; i++) {
            DrawBuilding(&gBuildings[i], actor);
        }

        /* Decorative trees at selected sidewalk corners. */
        for (int x = -3; x <= 3; x++) {
            for (int z = -3; z <= 3; z++) {
                float tx = (float)x*48.0f + 10.0f;
                float tz = (float)z*48.0f + 10.0f;
                if (Distance2D(actor.x, actor.z, tx, tz) < DRAW_DISTANCE &&
                    ((x + z)%2 == 0)) {
                    DrawTree(tx, tz, 0.85f);
                    DrawTree(tx - 5.0f, tz + 4.0f, 0.72f);
                }
            }
        }

        /* Street lamps along the centre boulevards. */
        for (int i = -3; i <= 3; i++) {
            float c = (float)i*48.0f;
            float lx1 = c + 11.0f;
            float lx2 = c - 11.0f;
            float lz = (float)((i%2 == 0) ? 48 : -48);
            if (Distance2D(actor.x, actor.z, lx1, lz) < DRAW_DISTANCE) DrawStreetLamp(lx1, lz);
            if (Distance2D(actor.x, actor.z, lx2, -lz) < DRAW_DISTANCE) DrawStreetLamp(lx2, -lz);
        }

        /* Traffic and walkers are updated continuously. */
        for (int i = 0; i < TRAFFIC_COUNT; i++) {
            if (Distance2D(actor.x, actor.z, gTraffic[i].position.x,
                           gTraffic[i].position.z) < DRAW_DISTANCE) {
                DrawVehicleModel(&gTraffic[i], false);
            }
        }
        for (int i = 0; i < WALKER_COUNT; i++) {
            Pedestrian *p = &gPedestrians[i];
            if (Distance2D(actor.x, actor.z, p->position.x, p->position.z) < 90.0f) {
                DrawHuman(p->position, p->yaw, p->phase, p->shirt,
                          p->trousers, p->skin, false, false);
            }
        }

        /* Player vehicle and character. */
        DrawVehicleModel(&playerCar, true);
        if (!inCar) {
            DrawHuman(player, playerYaw, walkPhase,
                      (Color){ 32, 111, 190, 255 },
                      (Color){ 42, 53, 74, 255 },
                      (Color){ 196, 151, 117, 255 }, true,
                      false);
        }

        if (missionStep < MISSION_COUNT) {
            DrawMissionMarker(gMissionPoints[missionStep], worldTime,
                              (missionStep%2) == 0);
        }

        /* Sun and distant horizon landmark. */
        DrawSphere((Vector3){ -115.0f, 82.0f, -160.0f }, 7.0f,
                   (Color){ 255, 224, 158, 255 });
        EndMode3D();

        /* ---- Render 2D interface -------------------------------------- */
        if (state == GAME_TITLE) {
            DrawRectangle(0, 0, screenW, screenH, Fade((Color){ 5, 12, 22, 255 }, 0.54f));
            DrawPanel((Rectangle){(float)screenW*0.5f - 225.0f, 62.0f, 450.0f, 130.0f},
                      (Color){ 10, 19, 31, 228 }, Fade(C_MINT, 0.70f));
            DrawText("NOVA BAY", screenW/2 - MeasureText("NOVA BAY", 22)/2,
                     81, 22, C_MINT);
            DrawText("CITY LIFE 3D", screenW/2 - MeasureText("CITY LIFE 3D", 43)/2,
                     111, 43, C_WHITE);
            DrawText("DRIVE  •  EXPLORE  •  DELIVER",
                     screenW/2 - MeasureText("DRIVE  •  EXPLORE  •  DELIVER", 16)/2,
                     165, 16, C_GOLD);
            DrawButton(playRect, "ENTER THE CITY", (Color){ 35, 137, 93, 255 }, true);
            DrawText("A NEW DAY STARTS IN NOVA BAY",
                     screenW/2 - MeasureText("A NEW DAY STARTS IN NOVA BAY", 14)/2,
                     (int)(playRect.y + 82.0f), 14, Fade(C_WHITE, 0.82f));
            DrawText("TOUCH THE BUTTON TO PLAY",
                     screenW/2 - MeasureText("TOUCH THE BUTTON TO PLAY", 13)/2,
                     screenH - 34, 13, Fade(C_WHITE, 0.72f));
        } else {
            DrawGameHUD(screenW, screenH, cash, delivered, missionStep,
                        playerCar.speed, inCar, nearCar, nearMission,
                        actor, missionTarget, hasTarget,
                        stick, stickX, stickY, worldTime);

            DrawButton(pauseRect, "II", (Color){ 58, 73, 91, 255 }, false);

            if (messageTimer > 0.0f) {
                Rectangle mr = {(float)screenW*0.5f - 190.0f,
                                (float)screenH - 60.0f, 380.0f, 36.0f};
                DrawPanel(mr, (Color){ 12, 22, 32, 218 }, Fade(C_MINT, 0.45f));
                DrawText(message, screenW/2 - MeasureText(message, 15)/2,
                         screenH - 50, 15, C_WHITE);
            }

            if (state == GAME_PAUSED) {
                DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.62f));
                DrawPanel((Rectangle){(float)screenW*0.5f - 165.0f,
                                      (float)screenH*0.5f - 140.0f,
                                      330.0f, 290.0f},
                          (Color){ 12, 21, 34, 245 }, Fade(C_MINT, 0.75f));
                DrawText("PAUSED", screenW/2 - MeasureText("PAUSED", 32)/2,
                         (int)((float)screenH*0.5f - 112.0f), 32, C_WHITE);
                DrawButton(resumeRect, "RESUME", (Color){ 36, 139, 94, 255 }, true);
                DrawButton(restartRect, "RESTART CITY", (Color){ 55, 99, 153, 255 }, false);
                DrawText("YOUR PROGRESS IS KEPT WHILE THE APP IS OPEN",
                         screenW/2 - MeasureText("YOUR PROGRESS IS KEPT WHILE THE APP IS OPEN", 11)/2,
                         (int)((float)screenH*0.5f + 103.0f), 11, Fade(C_WHITE, 0.72f));
            }
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
