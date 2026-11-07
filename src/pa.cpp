#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <objidl.h>
#include <propidl.h>
#include <gdiplus.h>
#include <GL/gl.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace Gdiplus;

namespace {

constexpr int logicalWidth = 800;
constexpr int logicalHeight = 600;

struct Configuration {
    int resolution = 1;
    int drawDistance = 7000;
    int sceneDetail = 1;
    int shadows = 1;
    int particleEffects = 1;
    int specularEffects = 1;
    int soundEnabled = 1;
    int sound3DEnabled = 1;
    int hardwareSound = 1;
    int musicEnabled = 1;
    int mouseX = 7;
    int mouseY = 7;
    int invert = 0;
    int joystickEnabled = 0;
    int language = 0;
};

std::string rootDirectory;
Configuration configuration;
std::ofstream gameLog;
HINSTANCE applicationInstance;
HWND applicationWindow;
ULONG_PTR gdiplusToken;
int outputWidth = 800;
int outputHeight = 600;
POINT pointerPosition{};
bool pointerDown;
bool pointerPressed;
HDC openGLDevice;
HGLRC openGLContext;
std::array<bool, 256> keys{};

std::string path(const std::string& relative) {
    return rootDirectory + "\\" + relative;
}

std::wstring wide(const std::string& value) {
    int count = MultiByteToWideChar(CP_ACP, 0, value.c_str(), -1, nullptr, 0);
    std::wstring result(static_cast<size_t>(count), L'\0');
    MultiByteToWideChar(CP_ACP, 0, value.c_str(), -1, result.data(), count);
    if (!result.empty()) result.pop_back();
    return result;
}

void startLog() {
    gameLog.open(path("log.txt"), std::ios::trunc);
    gameLog << "********************************************\n"
               "<<<<<<<<<<<<<<<P.L.E.1 Engine>>>>>>>>>>>>>>>\n"
               "********************************************\n"
               " copyright (c) 2004 3Division Entertainment\n"
               "********************************************\n\n"
               "Air Guard Full v1.15 (c) 3Division Entertainment 2004\n";
}

void loadConfiguration() {
    std::ifstream input(path("config.ini"));
    gameLog << "Nacitavam nastavenia z config.ini\n";
    if (!input) {
        gameLog << " Nepodarilo sa otvorit subor config.ini\n";
        return;
    }
    std::string key;
    int value;
    while (input >> key >> value) {
        if (key == "Resolution") configuration.resolution = value;
        else if (key == "DrawDistance") configuration.drawDistance = value;
        else if (key == "SceneDetail") configuration.sceneDetail = value;
        else if (key == "Shadows") configuration.shadows = value;
        else if (key == "ParticleEffects") configuration.particleEffects = value;
        else if (key == "SpecularEffects") configuration.specularEffects = value;
        else if (key == "SoundEnabled") configuration.soundEnabled = value;
        else if (key == "Sound3DEnabled") configuration.sound3DEnabled = value;
        else if (key == "HardwareSound") configuration.hardwareSound = value;
        else if (key == "MusicEnabled") configuration.musicEnabled = value;
        else if (key == "MouseX") configuration.mouseX = value;
        else if (key == "MouseY") configuration.mouseY = value;
        else if (key == "Invert") configuration.invert = value;
        else if (key == "JoyEnabled") configuration.joystickEnabled = value;
        else if (key == "Language") configuration.language = value;
    }
    static constexpr int widths[] = {640, 800, 1024};
    static constexpr int heights[] = {480, 600, 768};
    configuration.resolution = std::clamp(configuration.resolution, 0, 2);
    outputWidth = widths[configuration.resolution];
    outputHeight = heights[configuration.resolution];
    gameLog << ">>>Inicializijem ENGINE\nNastavenia:\n"
            << "   Sirka: " << outputWidth << "  Vyska: " << outputHeight << "\n"
            << "   Windowed: 0 \n"
            << "   Max: " << configuration.drawDistance << "  Min: 10.00\n"
            << "   Stencil Shadows: " << configuration.shadows << " \n";
}

class ImageAsset {
public:
    ImageAsset() = default;
    explicit ImageAsset(const std::string& relative) { load(relative); }

    bool load(const std::string& relative) {
        gameLog << "Vytvaram 2D Panel NumFrames: 1\n  AddFrame: 0 " << relative << "\n";
        image = std::make_unique<Image>(wide(path(relative)).c_str());
        const bool loaded = image && image->GetLastStatus() == Ok;
        gameLog << (loaded ? "  Textura vytvorena\n" : "  Chyba pri vytvarani textury\n");
        return loaded;
    }

    int width() const { return image ? static_cast<int>(image->GetWidth()) : 0; }
    int height() const { return image ? static_cast<int>(image->GetHeight()) : 0; }

    void draw(Graphics& graphics, float centerX, float centerY, float alpha = 1.0f,
              float red = 1.0f, float green = 1.0f, float blue = 1.0f) const {
        if (!image || image->GetLastStatus() != Ok) return;
        ColorMatrix matrix = {{
            {red, 0, 0, 0, 0},
            {0, green, 0, 0, 0},
            {0, 0, blue, 0, 0},
            {0, 0, 0, alpha, 0},
            {0, 0, 0, 0, 1}
        }};
        ImageAttributes attributes;
        attributes.SetColorMatrix(&matrix);
        attributes.SetColorKey(Color(255, 250, 0, 250), Color(255, 255, 10, 255));
        Rect destination(static_cast<int>(std::lround(centerX - width() * 0.5f)),
                         static_cast<int>(std::lround(centerY - height() * 0.5f)),
                         width(), height());
        graphics.DrawImage(image.get(), destination, 0, 0, width(), height(), UnitPixel, &attributes);
    }

    void drawStretched(Graphics& graphics, const Rect& destination) const {
        if (!image || image->GetLastStatus() != Ok) return;
        graphics.DrawImage(image.get(), destination, 0, 0, width(), height(), UnitPixel);
    }

private:
    std::unique_ptr<Image> image;
};

struct Assets {
    ImageAsset background{"Menu\\background.bmp"};
    ImageAsset loading{"Menu\\Loading.bmp"};
    ImageAsset progress{"Menu\\progress.bmp"};
    ImageAsset logo3DGames{"Menu\\logo3DGames.bmp"};
    ImageAsset logoPLE{"Menu\\logoPLE.bmp"};
    ImageAsset logoBecher{"Menu\\logobecher.bmp"};
    ImageAsset becherGame{"Menu\\bechergame.bmp"};
    ImageAsset logoPlane{"Menu\\logoplane.bmp"};
    ImageAsset logoAir{"Menu\\logoair.bmp"};
    ImageAsset logoArcade{"Menu\\logoarcade.bmp"};
    ImageAsset logoPilot{"Menu\\logopilot.bmp"};
    ImageAsset cursor{"Menu\\cursor.bmp"};
    ImageAsset credits{"Menu\\credits.bmp"};
    ImageAsset campaign;
    ImageAsset battle;
    ImageAsset creditsButton;
    ImageAsset exitButton;
    ImageAsset backButton;
    ImageAsset briefingStart;
    ImageAsset briefingReplay;
    ImageAsset briefingBack;
    std::array<ImageAsset, 3> campaignWindows;
    std::array<ImageAsset, 15> missions;
    std::array<ImageAsset, 16> briefings;

    Assets() {
        const std::string language = configuration.language == 1 ? "cz" : "eng";
        campaign.load("Menu\\" + language + "\\campaing.bmp");
        battle.load("Menu\\" + language + "\\battle.bmp");
        creditsButton.load("Menu\\" + language + "\\credits.bmp");
        exitButton.load("Menu\\" + language + "\\exit.bmp");
        backButton.load("Menu\\" + language + "\\back.bmp");
        for (int i = 0; i < 15; ++i)
            missions[static_cast<size_t>(i)].load("Menu\\missions\\" + std::to_string(i + 1) + ".bmp");
        const std::string suffix = configuration.language == 1 ? "cz" : "eng";
        for (int i = 0; i < 16; ++i)
            briefings[static_cast<size_t>(i)].load("Menu\\brief\\briefring" + suffix + std::to_string(i) + ".jpg");
        briefingStart.load("Menu\\brief\\but1" + suffix + ".jpg");
        briefingReplay.load("Menu\\brief\\but2" + suffix + ".jpg");
        briefingBack.load("Menu\\brief\\but3" + suffix + ".jpg");
        campaignWindows[0].load("Menu\\Window1.bmp");
        campaignWindows[1].load("Menu\\Window2.bmp");
        campaignWindows[2].load("Menu\\Window3.bmp");
    }
};

std::unique_ptr<Assets> assets;

struct Vector3 { float x, y, z; };
struct TextureCoordinate { float u, v; };

struct MeshFace {
    int vertex[3]{};
    int textureVertex[3]{};
    Vector3 normal[3]{};
};

struct Mesh {
    std::vector<Vector3> vertices;
    std::vector<TextureCoordinate> textureVertices;
    std::vector<MeshFace> faces;

    bool loadASE(const std::string& filename) {
        gameLog << "Inicializujem ASE model\n";
        std::ifstream input(filename);
        if (!input) {
            gameLog << "  Nenasiel som subor " << filename << "\n";
            return false;
        }
        std::string line;
        int normalFace = -1;
        int normalCorner = 0;
        while (std::getline(input, line)) {
            int index, a, b, c;
            float x, y, z;
            if (std::sscanf(line.c_str(), " *MESH_NUMVERTEX %d", &index) == 1) {
                vertices.resize(static_cast<size_t>(std::max(index, 0)));
            } else if (std::sscanf(line.c_str(), " *MESH_NUMTVERTEX %d", &index) == 1) {
                textureVertices.resize(static_cast<size_t>(std::max(index, 0)));
            } else if (std::sscanf(line.c_str(), " *MESH_NUMFACES %d", &index) == 1) {
                faces.resize(static_cast<size_t>(std::max(index, 0)));
            } else if (std::sscanf(line.c_str(), " *MESH_VERTEX %d %f %f %f", &index, &x, &y, &z) == 4) {
                if (index >= 0 && static_cast<size_t>(index) < vertices.size())
                    vertices[static_cast<size_t>(index)] = {x, z, y};
            } else if (std::sscanf(line.c_str(), " *MESH_FACE %d: A: %d B: %d C: %d", &index, &a, &b, &c) == 4) {
                if (index >= 0 && static_cast<size_t>(index) < faces.size()) {
                    faces[static_cast<size_t>(index)].vertex[0] = a;
                    faces[static_cast<size_t>(index)].vertex[1] = b;
                    faces[static_cast<size_t>(index)].vertex[2] = c;
                }
            } else if (std::sscanf(line.c_str(), " *MESH_TVERT %d %f %f %f", &index, &x, &y, &z) == 4) {
                if (index >= 0 && static_cast<size_t>(index) < textureVertices.size())
                    textureVertices[static_cast<size_t>(index)] = {x, -y};
            } else if (std::sscanf(line.c_str(), " *MESH_TFACE %d %d %d %d", &index, &a, &b, &c) == 4) {
                if (index >= 0 && static_cast<size_t>(index) < faces.size()) {
                    faces[static_cast<size_t>(index)].textureVertex[0] = a;
                    faces[static_cast<size_t>(index)].textureVertex[1] = b;
                    faces[static_cast<size_t>(index)].textureVertex[2] = c;
                }
            } else if (std::sscanf(line.c_str(), " *MESH_FACENORMAL %d %f %f %f", &index, &x, &y, &z) == 4) {
                normalFace = index;
                normalCorner = 0;
            } else if (std::sscanf(line.c_str(), " *MESH_VERTEXNORMAL %d %f %f %f", &index, &x, &y, &z) == 4) {
                if (normalFace >= 0 && static_cast<size_t>(normalFace) < faces.size() && normalCorner < 3)
                    faces[static_cast<size_t>(normalFace)].normal[normalCorner++] = {x, z, y};
            }
        }
        for (auto& face : faces) {
            const Vector3& p0 = vertices[static_cast<size_t>(face.vertex[0])];
            const Vector3& p1 = vertices[static_cast<size_t>(face.vertex[1])];
            const Vector3& p2 = vertices[static_cast<size_t>(face.vertex[2])];
            Vector3 u{p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
            Vector3 v{p2.x - p0.x, p2.y - p0.y, p2.z - p0.z};
            Vector3 n{u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x};
            const float length = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
            if (length > 0.0f) { n.x /= length; n.y /= length; n.z /= length; }
            for (auto& vertexNormal : face.normal) {
                const float normalLength = std::sqrt(vertexNormal.x * vertexNormal.x +
                                                     vertexNormal.y * vertexNormal.y +
                                                     vertexNormal.z * vertexNormal.z);
                if (normalLength > 0.0f) {
                    vertexNormal.x /= normalLength;
                    vertexNormal.y /= normalLength;
                    vertexNormal.z /= normalLength;
                } else {
                    vertexNormal = n;
                }
            }
        }
        gameLog << "  NumFrames: 1\n  NumFaces: " << faces.size() << "\n"
                << (faces.empty() ? "  Chyba pri vytvarani VB\n" : "  Vertex Buffer vytvoreny\n");
        return !faces.empty();
    }

    void draw() const {
        glBegin(GL_TRIANGLES);
        for (const auto& face : faces) {
            for (int corner = 0; corner < 3; ++corner) {
                const int textureIndex = face.textureVertex[corner];
                if (textureIndex >= 0 && static_cast<size_t>(textureIndex) < textureVertices.size()) {
                    const auto& uv = textureVertices[static_cast<size_t>(textureIndex)];
                    glTexCoord2f(uv.u, uv.v);
                }
                const auto& normal = face.normal[corner];
                const auto& vertex = vertices[static_cast<size_t>(face.vertex[corner])];
                glNormal3f(normal.x, normal.y, normal.z);
                glVertex3f(vertex.x, vertex.y, vertex.z);
            }
        }
        glEnd();
    }
};

#pragma pack(push, 1)
struct TerrainVertex {
    float x, y, z;
    float nx, ny, nz;
    float u0, v0;
    float u1, v1;
};
struct TerrainFaceRecord {
    TerrainVertex vertex[3];
    float plane[4];
    std::uint32_t flags;
    std::uint32_t index;
    std::uint32_t group;
};
#pragma pack(pop)

static_assert(sizeof(TerrainFaceRecord) == 148);

std::vector<TerrainFaceRecord> terrainFaces;
Mesh spitfireMesh;
Mesh messerschmittMesh;
Mesh volkerMesh;
Mesh bombardierMesh;
Mesh cockpitMesh;
Mesh spitfireShadowMesh;
Mesh bombMesh;
Mesh propellerMesh;
Mesh fireball1Mesh;
Mesh fireball2Mesh;
std::array<Mesh, 26> structureMeshes;
std::array<GLuint, 26> structureTextures{};
std::array<GLuint, 26> structureDestroyedTextures{};
Mesh truckBackMesh;
Mesh truckCannonMesh;
Mesh truckDestroyedMesh;
GLuint truckTexture;
GLuint truckDestroyedTexture;
std::array<Mesh, 5> skyboxMeshes;
GLuint terrainTexture;
GLuint desertTexture;
GLuint terrainDetailTexture;
GLuint desertDetailTexture;
GLuint waterTexture;
GLuint spitfireTexture;
GLuint messerschmittTexture;
GLuint volkerTexture;
GLuint bombardierTexture;
std::array<GLuint, 5> skyboxTextures{};
std::array<GLuint, 7> pilotTextures{};
GLuint bevelTexture;
GLuint throttleTexture;
GLuint gaugeTexture;
GLuint bombsTexture;
GLuint compassTexture;
GLuint messageBackTexture;
GLuint mapTexture;
GLuint cockpitTexture;
GLuint bombMeshTexture;
GLuint propellerTexture;
GLuint fireball1Texture;
GLuint fireball2Texture;
GLuint spitfireDamagedTexture;
GLuint spitfireDestroyedTexture;
GLuint spitfireEnvironmentTexture;
GLuint messerschmittDestroyedTexture;
GLuint volkerDestroyedTexture;
GLuint bombardierDestroyedTexture;
std::array<GLuint, 6> fragmentTextures{};
std::array<GLuint, 16> briefingTexturesGL{};
std::array<GLuint, 3> briefingButtonTexturesGL{};
GLuint bulletParticleTexture;
GLuint bombParticleTexture;
GLuint smokeParticleTexture;
GLuint fireParticleTexture;
GLuint hitParticleTexture;
GLuint genericParticleTexture;
std::array<GLuint, 10> explosionTextures{};
std::array<GLuint, 10> scoreTextures{};
std::array<GLuint, 4> lensTextures{};
GLuint fontDisplayLists;
bool flightResourcesLoaded;
using ActiveTextureFunction = void (APIENTRY*)(GLenum);
using MultiTexCoord2fFunction = void (APIENTRY*)(GLenum, GLfloat, GLfloat);
ActiveTextureFunction activeTexture;
MultiTexCoord2fFunction multiTexCoord2f;
Vector3 aircraftPosition{0.0f, 500.0f, -4000.0f};
float aircraftYaw;
float aircraftPitch;
float aircraftRoll;
float aircraftSpeed = 150.0f;
int bombs = 5;
int playerHealth = 100;
std::chrono::steady_clock::time_point lastFlightFrame;

enum class AircraftKind { Messerschmitt, Volker, Bombardier };
struct AircraftEntity {
    AircraftKind kind;
    int health;
    int team;
    Vector3 position;
    Vector3 target;
    float yaw;
    bool active = true;
    float firingDelay = 0.0f;
};
struct Projectile {
    Vector3 position;
    Vector3 velocity;
    float life;
    bool bomb;
    bool enemy;
};
struct ExplosionEffect {
    Vector3 position;
    float age;
    float size;
};
struct FloatingScore {
    Vector3 position;
    float age;
    int points;
};
struct GroundEntity {
    int model;
    int health;
    Vector3 position;
    Vector3 rotation;
    bool objective = true;
    bool active = true;
};
struct TruckEntity {
    int health;
    int team;
    Vector3 position;
    Vector3 rotation;
    bool active = true;
};
std::vector<AircraftEntity> missionAircraft;
std::vector<Projectile> projectiles;
std::vector<ExplosionEffect> explosionEffects;
std::vector<FloatingScore> floatingScores;
std::vector<GroundEntity> missionGround;
std::vector<TruckEntity> missionTrucks;
float firingDelay;
bool missionCompleted;
bool missionFailed;
float flightElapsedTime;
float missionResultTime;
int score;
int bestBattleScore;
int cameraMode;
bool mapVisible = true;
bool compassVisible = true;
bool fpsVisible;
float displayedFps;
bool missionWaveTriggered;
bool missionObjectiveTriggered;
unsigned missionTriggerMask;
float missionObjectiveTime = -1.0f;
float missionSecondaryTime = -1.0f;
bool desertWorld;
bool joystickTriggerDown;
bool joystickBombDown;
bool flightPaused;
bool lensVisible;
float lensX;
float lensY;
int enemyEngineKind = -1;

float terrainHeightAt(float x, float z) {
    for (const auto& face : terrainFaces) {
        if (face.group != 0) continue;
        const TerrainVertex& a = face.vertex[0];
        const TerrainVertex& b = face.vertex[1];
        const TerrainVertex& c = face.vertex[2];
        const float denominator = (b.z - c.z) * (a.x - c.x) +
                                  (c.x - b.x) * (a.z - c.z);
        if (std::abs(denominator) < 0.00001f) continue;
        const float wa = ((b.z - c.z) * (x - c.x) +
                          (c.x - b.x) * (z - c.z)) / denominator;
        const float wb = ((c.z - a.z) * (x - c.x) +
                          (a.x - c.x) * (z - c.z)) / denominator;
        const float wc = 1.0f - wa - wb;
        if (wa >= -0.0001f && wb >= -0.0001f && wc >= -0.0001f)
            return wa * a.y + wb * b.y + wc * c.y;
    }
    return 0.0f;
}

void addAircraft(AircraftKind kind, int health, int team, Vector3 position, float yaw,
                 Vector3 target = {}) {
    missionAircraft.push_back({kind, health, team, position, target, yaw, true, 0.0f});
}

void addGround(int model, Vector3 position, Vector3 rotation, bool objective = true) {
    static constexpr int healthByModel[26] = {
        320, 500, 300, 350, 200, 300, 330, 380, 380, 380, 300, 300, 200,
        200, 130, 500, 400, 350, 500, 30000, 30000, 30000, 30000, 30000, 30000, 30000
    };
    position.y += terrainHeightAt(position.x, position.z);
    missionGround.push_back({model, healthByModel[model], position, rotation, objective, true});
}

void addTruck(int team, Vector3 position, Vector3 rotation) {
    position.y += terrainHeightAt(position.x, position.z);
    missionTrucks.push_back({130, team, position, rotation, true});
}

struct SceneryPlacement {
    int model;
    Vector3 position;
    Vector3 rotation;
};

#include "scenery_placements.inc"

void populateMission(int mission) {
    missionAircraft.clear();
    projectiles.clear();
    explosionEffects.clear();
    floatingScores.clear();
    missionGround.clear();
    missionTrucks.clear();
    if (mission == 0) {
        for (const auto& placement : battleScenery)
            addGround(placement.model, placement.position, placement.rotation, false);
    } else {
        for (const auto& placement : campaignScenery)
            addGround(placement.model, placement.position, placement.rotation, false);
    }
    switch (mission) {
    case 0:
        addAircraft(AircraftKind::Messerschmitt, 110, 0, {0, 600, 5300}, 0);
        addAircraft(AircraftKind::Messerschmitt, 110, 0, {4500, 600, -4300}, 0);
        addAircraft(AircraftKind::Messerschmitt, 110, 1, {4000, 600, 3000}, 0);
        addAircraft(AircraftKind::Volker, 150, 0, {3000, 600, 7300}, 0);
        addAircraft(AircraftKind::Volker, 150, 0, {-4500, 600, -8300}, 0);
        break;
    case 2:
        addAircraft(AircraftKind::Messerschmitt, 70, 0, {-4000, 600, -7000}, 0); break;
    case 3:
        addAircraft(AircraftKind::Messerschmitt, 100, 0, {-8064, 600, -5849}, 0);
        addAircraft(AircraftKind::Messerschmitt, 100, 0, {-6386, 600, -4124}, 0);
        addAircraft(AircraftKind::Bombardier, 400, 0, {-6723, 750, -7175}, 3.14f, {-5826, 0, -6281});
        break;
    case 4:
        addGround(13, {3099.6f, 0, -2128.6f}, {0, -18.6f, 0});
        addGround(12, {3248.4f, 0, -2175.9f}, {0, -18.6f, 0});
        addGround(12, {3398.9f, 0, -2210.5f}, {0, -18.6f, 0});
        addGround(11, {2916.6f, 0, -2077.0f}, {0, -18.5f, 0});
        addTruck(0, {3592.5f, 0, -2306.4f}, {0, -0.8f, 0});
        break;
    case 5:
        addGround(13, {5638.4f, 0, 2129.0f}, {0, -13.2f, 0});
        addGround(11, {5365.3f, 0, 1960.3f}, {0, -12.9f, 0});
        addGround(13, {5053.2f, 0, 1838.6f}, {0, -13.1f, 0});
        addGround(10, {4897.9f, 0, 1741.9f}, {0, -13.1f, 0});
        addGround(12, {4590.6f, 0, 1546.1f}, {0, -13.1f, 0});
        addGround(12, {4436.1f, 0, 1463.8f}, {0, -12.9f, 0});
        addGround(12, {3903.2f, 0, 1323.7f}, {0, -12.7f, 0});
        addGround(13, {3586.6f, 0, 1245.4f}, {0, -12.7f, 0});
        addGround(11, {3404.3f, 0, 1223.8f}, {0, -12.7f, 0});
        addTruck(0, {5203.7f, 0, 1907.0f}, {0, -12.9f, 0});
        addTruck(0, {3730.6f, 0, 1285.5f}, {0, -12.7f, 0});
        break;
    case 6:
        addAircraft(AircraftKind::Volker, 100, 0, {-7500, 600, -4000}, 1.57f);
        addAircraft(AircraftKind::Volker, 100, 0, {-7500, 600, -2000}, 1.57f);
        addAircraft(AircraftKind::Volker, 100, 0, {-7500, 600, 0}, 1.57f);
        addAircraft(AircraftKind::Volker, 100, 0, {-7500, 600, 2000}, 1.57f);
        addAircraft(AircraftKind::Messerschmitt, 100, 0, {7500, 600, -3000}, -1.57f);
        addAircraft(AircraftKind::Messerschmitt, 100, 1, {7500, 600, -2500}, -1.57f);
        break;
    case 7:
        addTruck(0, {-5677.9f, 0, -750.7f}, {0, -19.6f, 0});
        addTruck(0, {-5572.9f, 0, -780.2f}, {0, -19.6f, 0});
        addTruck(0, {-1414.4f, 0, -4071.0f}, {0, -19.3f, 0});
        addTruck(0, {3237.3f, 0, -1814.9f}, {0, -19.4f, 0});
        addTruck(0, {4597.7f, 0, 2260.9f}, {0, -19.7f, 0});
        addTruck(0, {-4009.4f, 0, 4520.9f}, {0, -18.4f, 0});
        addTruck(0, {-2769.8f, 0, 7238.0f}, {0, -19.5f, 0});
        break;
    case 8:
        addAircraft(AircraftKind::Bombardier, 300, 0, {-4600, 500, -4600}, 0.7f, {13000, 0, 13000});
        addAircraft(AircraftKind::Bombardier, 300, 0, {-4300, 600, -4300}, 0.7f, {13300, 0, 13300});
        addAircraft(AircraftKind::Volker, 60, 0, {4500, 600, -4300}, 0);
        addTruck(1, {4407.2f, 0, 4902.6f}, {0, -1.0f, 0});
        addTruck(1, {4707.7f, 0, 4264.9f}, {0, -0.5f, 0});
        addTruck(1, {3980.9f, 0, 4080.8f}, {0, -1.5f, 0});
        break;
    case 9:
        addAircraft(AircraftKind::Messerschmitt, 100, 0, {4000, 600, 2000}, 0);
        addAircraft(AircraftKind::Messerschmitt, 100, 1, {4500, 600, 1000}, 0);
        addAircraft(AircraftKind::Volker, 120, 0, {4000, 600, 0}, 0);
        addAircraft(AircraftKind::Volker, 120, 0, {8000, 600, -1500}, 0);
        addGround(11, {-940.8f, 0, 306.3f}, {0, -19.4f, 0});
        addGround(11, {-940.8f, 0, 427.4f}, {0, -19.4f, 0});
        addGround(11, {-940.8f, 0, 548.1f}, {0, -19.4f, 0});
        addGround(13, {-1047.0f, 0, -373.9f}, {0, -19.5f, 0});
        addGround(13, {-999.8f, 0, -536.1f}, {0, -19.5f, 0});
        addGround(13, {-1377.4f, 0, -201.8f}, {0, -21.0f, 0});
        addTruck(0, {-1912.0f, 0, -138.7f}, {0, -11.5f, 0});
        addTruck(0, {-1672.0f, 0, 553.2f}, {0, -12.7f, 0});
        addTruck(0, {-1337.7f, 0, 696.1f}, {0, -14.0f, 0});
        break;
    case 10:
        addGround(10, {-3630.2f, 0, -4149.2f}, {0, -15.8f, 0});
        addGround(10, {-3318.0f, 0, -4132.8f}, {0, -15.7f, 0});
        addGround(11, {-3036.1f, 0, -4132.5f}, {0, -15.4f, 0});
        addGround(12, {4877.7f, 0, -5640.8f}, {0, -25.0f, 0});
        addGround(11, {4911.8f, 0, -5758.6f}, {0, -25.0f, 0});
        addGround(4, {4999.7f, 0, -5617.7f}, {0, -22.6f, 0});
        addGround(4, {5015.8f, 0, -5753.5f}, {0, -22.6f, 0});
        addGround(4, {5084.4f, 0, -5720.7f}, {0, -22.6f, 0});
        addGround(14, {2833.0f, 0, 738.4f}, {0, -21.4f, 0});
        addGround(13, {-366.7f, 0, 5693.5f}, {0, -19.7f, 0});
        addGround(13, {-276.7f, 0, 5861.9f}, {0, -20.2f, 0});
        addTruck(0, {-2830.1f, 0, -4198.4f}, {0, -16.9f, 0});
        addTruck(0, {-3824.7f, 0, -4180.2f}, {0, -17.5f, 0});
        addTruck(0, {4912.8f, 0, -5470.3f}, {0, -26.2f, 0});
        addTruck(0, {4643.7f, 0, -5740.9f}, {0, -25.0f, 0});
        break;
    case 11:
        addAircraft(AircraftKind::Volker, 150, 0, {10175, 400, -8023.9f}, -2.4f);
        addAircraft(AircraftKind::Volker, 150, 0, {10121.8f, 400, -7448.8f}, -2.4f);
        addAircraft(AircraftKind::Volker, 150, 0, {10344.7f, 400, -7783.9f}, -2.4f);
        addAircraft(AircraftKind::Volker, 150, 0, {9853.4f, 400, -10107.7f}, -1.1f);
        break;
    case 12:
        addTruck(0, {-5072.0f, 0, -4118.9f}, {0, -16.3f, 0});
        addTruck(0, {-4922.5f, 0, -4202.4f}, {0, -17.0f, 0});
        break;
    case 13:
        addAircraft(AircraftKind::Bombardier, 400, 1, {5279, 500, -18735}, 0, {4079, 0, -7978});
        addAircraft(AircraftKind::Bombardier, 400, 1, {4302, 500, -12389}, 0, {4532, 0, -2956});
        addAircraft(AircraftKind::Bombardier, 400, 1, {6764, 500, -5418}, 0, {6423, 0, 4766});
        addTruck(0, {4737.0f, 0, -6816.0f}, {0, -18.1f, 0});
        addTruck(0, {4828.3f, 0, -6682.3f}, {0, -18.4f, 0});
        addTruck(0, {4777.1f, 0, 1660.3f}, {0, -17.8f, 0});
        addTruck(0, {4940.1f, 0, 1761.6f}, {0, -18.1f, 0});
        break;
    case 14:
        addAircraft(AircraftKind::Messerschmitt, 100, 1, {-4044.9f, 600, -9456.4f}, -18.7f);
        addAircraft(AircraftKind::Messerschmitt, 100, 1, {-3272.3f, 600, -8858.7f}, -18.7f);
        addAircraft(AircraftKind::Volker, 150, 0, {-2785.4f, 600, -8762.1f}, -18.7f);
        addAircraft(AircraftKind::Volker, 150, 0, {2348.8f, 600, -3798.8f}, -21.0f);
        addAircraft(AircraftKind::Volker, 100, 0, {1492.8f, 600, -2727.1f}, -19.6f);
        addAircraft(AircraftKind::Bombardier, 550, 1, {-4148.4f, 600, -8779.2f}, 0, {-4869.3f, 0, -4426.7f});
        addAircraft(AircraftKind::Bombardier, 550, 1, {-3107.7f, 600, -9281.8f}, 0, {-3820.8f, 0, -5224.2f});
        addTruck(0, {-5096.9f, 0, -3388.9f}, {0, -19.2f, 0});
        addTruck(0, {-5212.6f, 0, -3078.9f}, {0, -19.0f, 0});
        addTruck(0, {-5287.7f, 0, -2704.5f}, {0, -18.6f, 0});
        addTruck(0, {-5493.1f, 0, -4638.5f}, {0, -19.7f, 0});
        break;
    default:
        break;
    }
}

GLuint loadOpenGLTexture(const std::string& filename) {
    Bitmap bitmap(wide(filename).c_str());
    if (bitmap.GetLastStatus() != Ok) {
        gameLog << "  chyba pri vytvarani textury: " << filename << "\n";
        return 0;
    }
    const UINT width = bitmap.GetWidth();
    const UINT height = bitmap.GetHeight();
    Rect area(0, 0, static_cast<INT>(width), static_cast<INT>(height));
    BitmapData data{};
    if (bitmap.LockBits(&area, ImageLockModeRead, PixelFormat32bppARGB, &data) != Ok) return 0;
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
    for (UINT y = 0; y < height; ++y) {
        const auto* source = static_cast<const unsigned char*>(data.Scan0) + static_cast<ptrdiff_t>(y) * data.Stride;
        auto* destination = pixels.data() + static_cast<size_t>(y) * width * 4;
        std::memcpy(destination, source, static_cast<size_t>(width) * 4);
        for (UINT x = 0; x < width; ++x) {
            unsigned char* pixel = destination + static_cast<size_t>(x) * 4;
            if (pixel[2] >= 250 && pixel[1] <= 10 && pixel[0] >= 250) pixel[3] = 0;
        }
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                 0, 0x80E1, GL_UNSIGNED_BYTE, pixels.data());
    bitmap.UnlockBits(&data);
    gameLog << "  Textura: " << filename << "\n  Textura modelu vytvorena\n";
    return texture;
}

void loadTerrain() {
    std::ifstream input(path("Scene\\terrain.scn"), std::ios::binary);
    int count = 0;
    input.read(reinterpret_cast<char*>(&count), sizeof(count));
    if (!input || count < 0 || count > 1000000) return;
    terrainFaces.resize(static_cast<size_t>(count));
    input.read(reinterpret_cast<char*>(terrainFaces.data()),
               static_cast<std::streamsize>(terrainFaces.size() * sizeof(TerrainFaceRecord)));
    if (!input) terrainFaces.clear();
}

void initializeFlightResources() {
    if (flightResourcesLoaded) return;
    wglMakeCurrent(openGLDevice, openGLContext);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    const GLfloat lightDirection[] = {-0.4f, 0.8f, 0.3f, 0.0f};
    const GLfloat lightAmbient[] = {0.55f, 0.55f, 0.55f, 1.0f};
    const GLfloat lightDiffuse[] = {0.8f, 0.8f, 0.8f, 1.0f};
    const GLfloat lightSpecular[] = {0.7f, 0.7f, 0.7f, 1.0f};
    const GLfloat noSpecular[] = {0.0f, 0.0f, 0.0f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightDirection);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, configuration.specularEffects ? lightSpecular : noSpecular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, configuration.specularEffects ? lightSpecular : noSpecular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, configuration.specularEffects ? 32.0f : 0.0f);
    glEnable(GL_COLOR_MATERIAL);
    activeTexture = reinterpret_cast<ActiveTextureFunction>(wglGetProcAddress("glActiveTextureARB"));
    multiTexCoord2f = reinterpret_cast<MultiTexCoord2fFunction>(wglGetProcAddress("glMultiTexCoord2fARB"));
    loadTerrain();
    spitfireMesh.loadASE(path("Models\\spitfire\\spitfire.ASE"));
    messerschmittMesh.loadASE(path("Models\\messerschmitt\\messerschmitt.ASE"));
    volkerMesh.loadASE(path("Models\\Volker\\volker.ASE"));
    bombardierMesh.loadASE(path("Models\\bombarder\\bombarder.ASE"));
    cockpitMesh.loadASE(path("Models\\spitfire\\kabina.ASE"));
    spitfireShadowMesh.loadASE(path("Models\\spitfire\\spitfireshadow.ASE"));
    bombMesh.loadASE(path("Models\\spitfire\\bomb.ASE"));
    propellerMesh.loadASE(path("Models\\vrtula\\vrtula.ASE"));
    fireball1Mesh.loadASE(path("Particles\\fireball1.ASE"));
    fireball2Mesh.loadASE(path("Particles\\fireball2.ASE"));
    static constexpr const char* structureModelFiles[26] = {
        "budova", "tovaren", "komin", "sklad", "stan", "letisko", "hangar", "dom1", "dom2",
        "kostol", "tank1", "tank2", "conv1", "conv2", "plane", "mesto1", "mesto2", "mesto3",
        "mesto4", "billboard", "strom1", "strom2", "strom3", "strom4", "palma1", "palma2"
    };
    static constexpr const char* structureTextureFiles[26] = {
        "budova", "tovaren", "komin", "sklad", "stan", "letisko", "hangar", "dom1", "dom2",
        "kostol", "tank1", "tank2", "conv1", "conv2", "letisko", "mesto", "mesto", "mesto",
        "mesto", "billboard", "strom1", "strom2", "strom3", "strom4", "palma1", "palma2"
    };
    static constexpr const char* structureDestroyedFiles[26] = {
        "budovadestroy", "Tovarendestroy", "Komindestroy", "skladdestroy", "Standestroy",
        "Letiskodestroy", "Hangardestroy", "Dom1destroy", "Dom2destroy", "Kostoldestroy",
        "Tank1destroy", "Tank2destroy", "Conv1destroy", "Conv2destroy", nullptr,
        "Mestodestroy", "Mestodestroy", "Mestodestroy", "Mestodestroy", nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
    };
    for (size_t i = 0; i < structureMeshes.size(); ++i) {
        structureMeshes[i].loadASE(path(std::string("Models\\structures\\") + structureModelFiles[i] + ".ASE"));
        structureTextures[i] = loadOpenGLTexture(path(i == 14
            ? "Models\\messerschmitt\\messernormal.bmp"
            : std::string("Models\\structures\\") + structureTextureFiles[i] + ".bmp"));
        if (structureDestroyedFiles[i])
            structureDestroyedTextures[i] = loadOpenGLTexture(path(
                std::string("Models\\structures\\") + structureDestroyedFiles[i] + ".bmp"));
    }
    truckBackMesh.loadASE(path("Models\\truck\\truckback.ASE"));
    truckCannonMesh.loadASE(path("Models\\truck\\truckcannon.ASE"));
    truckDestroyedMesh.loadASE(path("Models\\truck\\truckDestroyed.ASE"));
    skyboxMeshes[0].loadASE(path("Scene\\skyboxleft.ASE"));
    skyboxMeshes[1].loadASE(path("Scene\\skyboxright.ASE"));
    skyboxMeshes[2].loadASE(path("Scene\\skyboxfront.ASE"));
    skyboxMeshes[3].loadASE(path("Scene\\skyboxback.ASE"));
    skyboxMeshes[4].loadASE(path("Scene\\skyboxtop.ASE"));
    terrainTexture = loadOpenGLTexture(path(configuration.sceneDetail == 0 ?
                                             "Scene\\texturepole512.jpg" : "Scene\\texturepole1024.jpg"));
    desertTexture = loadOpenGLTexture(path(configuration.sceneDetail == 0 ?
                                            "Scene\\texturepust512.jpg" : "Scene\\texturepust1024.jpg"));
    terrainDetailTexture = loadOpenGLTexture(path("Scene\\detailpole.jpg"));
    desertDetailTexture = loadOpenGLTexture(path("Scene\\detailpust.jpg"));
    waterTexture = loadOpenGLTexture(path("Scene\\water.jpg"));
    spitfireTexture = loadOpenGLTexture(path("Models\\spitfire\\spitfirenormal1.bmp"));
    messerschmittTexture = loadOpenGLTexture(path("Models\\messerschmitt\\messernormal.bmp"));
    volkerTexture = loadOpenGLTexture(path("Models\\Volker\\volkernormal.bmp"));
    bombardierTexture = loadOpenGLTexture(path("Models\\bombarder\\bombardernormal.bmp"));
    cockpitTexture = loadOpenGLTexture(path("Models\\spitfire\\kabina.bmp"));
    bombMeshTexture = loadOpenGLTexture(path("Models\\spitfire\\bomb.bmp"));
    propellerTexture = loadOpenGLTexture(path("Models\\vrtula\\vrtula.bmp"));
    fireball1Texture = loadOpenGLTexture(path("Particles\\fireball1.jpg"));
    fireball2Texture = loadOpenGLTexture(path("Particles\\fireball2.jpg"));
    spitfireDamagedTexture = loadOpenGLTexture(path("Models\\spitfire\\spitfirenormal2.bmp"));
    spitfireDestroyedTexture = loadOpenGLTexture(path("Models\\spitfire\\spitfiredestroy.bmp"));
    spitfireEnvironmentTexture = loadOpenGLTexture(path("Models\\spitfire\\enviro.bmp"));
    messerschmittDestroyedTexture = loadOpenGLTexture(path("Models\\messerschmitt\\messerdestroy.bmp"));
    volkerDestroyedTexture = loadOpenGLTexture(path("Models\\Volker\\Volkerdestroy.bmp"));
    bombardierDestroyedTexture = loadOpenGLTexture(path("Models\\bombarder\\bombarderdestroy.bmp"));
    truckTexture = loadOpenGLTexture(path("Models\\truck\\trucknormal.bmp"));
    truckDestroyedTexture = loadOpenGLTexture(path("Models\\truck\\truckDestroy.bmp"));
    skyboxTextures[0] = loadOpenGLTexture(path("Scene\\sleft.jpg"));
    skyboxTextures[1] = loadOpenGLTexture(path("Scene\\sright.jpg"));
    skyboxTextures[2] = loadOpenGLTexture(path("Scene\\sfront.jpg"));
    skyboxTextures[3] = loadOpenGLTexture(path("Scene\\sback.jpg"));
    skyboxTextures[4] = loadOpenGLTexture(path("Scene\\stop.jpg"));
    static constexpr const char* pilotFiles[7] = {
        "pilot1.bmp", "pilot1fire.bmp", "pilot2.bmp", "pilot2fire.bmp",
        "pilot3.bmp", "pilot3fire.bmp", "pilotdeath.bmp"
    };
    for (size_t i = 0; i < pilotTextures.size(); ++i)
        pilotTextures[i] = loadOpenGLTexture(path(std::string("Menu\\gui\\") + pilotFiles[i]));
    bevelTexture = loadOpenGLTexture(path("Menu\\gui\\bevel.bmp"));
    throttleTexture = loadOpenGLTexture(path("Menu\\gui\\tbk.bmp"));
    gaugeTexture = loadOpenGLTexture(path("Menu\\gui\\truc.bmp"));
    bombsTexture = loadOpenGLTexture(path("Menu\\gui\\bombs.bmp"));
    compassTexture = loadOpenGLTexture(path(configuration.language == 1 ?
                                             "Menu\\gui\\kompascz.bmp" : "Menu\\gui\\kompaseng.bmp"));
    messageBackTexture = loadOpenGLTexture(path("Menu\\gui\\msgback.bmp"));
    mapTexture = loadOpenGLTexture(path("Menu\\gui\\mapa.bmp"));
    static constexpr const char* fragmentFiles[6] = {
        "frag1.bmp", "frag2.bmp", "frag3.bmp", "frag4.bmp", "frag5.bmp", "frag6.bmp"
    };
    for (size_t i = 0; i < fragmentTextures.size(); ++i)
        fragmentTextures[i] = loadOpenGLTexture(path(std::string("Menu\\gui\\") + fragmentFiles[i]));
    const std::string briefingLanguage = configuration.language == 1 ? "cz" : "eng";
    for (size_t i = 0; i < briefingTexturesGL.size(); ++i)
        briefingTexturesGL[i] = loadOpenGLTexture(path("Menu\\brief\\briefring" + briefingLanguage +
                                                       std::to_string(i) + ".jpg"));
    briefingButtonTexturesGL[0] = loadOpenGLTexture(path("Menu\\brief\\but1" + briefingLanguage + ".jpg"));
    briefingButtonTexturesGL[1] = loadOpenGLTexture(path("Menu\\brief\\but2" + briefingLanguage + ".jpg"));
    briefingButtonTexturesGL[2] = loadOpenGLTexture(path("Menu\\brief\\but3" + briefingLanguage + ".jpg"));
    bulletParticleTexture = loadOpenGLTexture(path("Particles\\bullet.bmp"));
    bombParticleTexture = loadOpenGLTexture(path("Particles\\bomb.bmp"));
    smokeParticleTexture = loadOpenGLTexture(path("Particles\\smoke.bmp"));
    fireParticleTexture = loadOpenGLTexture(path("Particles\\fire.bmp"));
    hitParticleTexture = loadOpenGLTexture(path("Particles\\hit.bmp"));
    genericParticleTexture = loadOpenGLTexture(path("Particles\\particle.bmp"));
    static constexpr const char* explosionFiles[10] = {
        "ex0001.jpg", "ex0002.jpg", "ex0003.jpg", "ex0004.jpg", "ex0005.jpg",
        "ex0006.jpg", "ex0007.jpg", "ex0008.jpg", "ex0009.jpg", "ex0010.jpg"
    };
    for (size_t i = 0; i < explosionTextures.size(); ++i)
        explosionTextures[i] = loadOpenGLTexture(path(std::string("Particles\\") + explosionFiles[i]));
    for (size_t i = 0; i < scoreTextures.size(); ++i)
        scoreTextures[i] = loadOpenGLTexture(path("Menu\\score\\" + std::to_string((i + 1) * 10) + ".bmp"));
    for (size_t i = 0; i < lensTextures.size(); ++i)
        lensTextures[i] = loadOpenGLTexture(path("Scene\\lens" + std::to_string(i + 1) + ".jpg"));
    HFONT font = CreateFontA(20, 0, 0, 0, 700, FALSE, FALSE, FALSE, ANSI_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                             DEFAULT_PITCH, "Arial");
    HGDIOBJ previousFont = SelectObject(openGLDevice, font);
    fontDisplayLists = glGenLists(256);
    wglUseFontBitmapsA(openGLDevice, 0, 256, fontDisplayLists);
    SelectObject(openGLDevice, previousFont);
    DeleteObject(font);
    flightResourcesLoaded = true;
}

enum class Screen {
    Loading,
    Intro3DGames,
    IntroBecher,
    MainMenu,
    Campaign,
    Credits,
    Briefing,
    Flight
};

Screen screen = Screen::Loading;
std::chrono::steady_clock::time_point screenStarted;
std::chrono::steady_clock::time_point transitionStarted;
float mainMenuPhaseOffset;
bool mainMenuLeaving;
float mainMenuLeaveTarget;
Screen mainMenuDestination = Screen::MainMenu;
bool mainMenuQuit;
bool submenuLeaving;
Screen submenuDestination = Screen::MainMenu;
int unlockedMission = 1;
int selectedMission = 1;
bool flightAudioActive;
int loadingStage;
int campaignPopup;
std::chrono::steady_clock::time_point campaignPopupStarted;

void closeAudioAlias(const char* alias) {
    std::string command = std::string("stop ") + alias;
    mciSendStringA(command.c_str(), nullptr, 0, nullptr);
    command = std::string("close ") + alias;
    mciSendStringA(command.c_str(), nullptr, 0, nullptr);
}

void playLoopingAudio(const char* alias, const std::string& filename) {
    closeAudioAlias(alias);
    std::string command = std::string("open \"") + filename + "\" type mpegvideo alias " + alias;
    mciSendStringA(command.c_str(), nullptr, 0, nullptr);
    command = std::string("play ") + alias + " repeat";
    mciSendStringA(command.c_str(), nullptr, 0, nullptr);
}

void playLoopingWave(const char* alias, const std::string& filename) {
    closeAudioAlias(alias);
    std::string command = std::string("open \"") + filename + "\" type waveaudio alias " + alias;
    mciSendStringA(command.c_str(), nullptr, 0, nullptr);
    command = std::string("play ") + alias + " repeat";
    mciSendStringA(command.c_str(), nullptr, 0, nullptr);
}

void playOneShot(const char* alias, const std::string& filename) {
    closeAudioAlias(alias);
    std::string command = std::string("open \"") + filename + "\" type waveaudio alias " + alias;
    mciSendStringA(command.c_str(), nullptr, 0, nullptr);
    command = std::string("play ") + alias + " from 0";
    mciSendStringA(command.c_str(), nullptr, 0, nullptr);
}

std::uint32_t rotateLeft(std::uint32_t value, std::uint32_t count) {
    return (value << count) | (value >> (32 - count));
}

std::string md5(const std::string& input) {
    static constexpr std::uint32_t shifts[64] = {
        7,12,17,22, 7,12,17,22, 7,12,17,22, 7,12,17,22,
        5,9,14,20, 5,9,14,20, 5,9,14,20, 5,9,14,20,
        4,11,16,23, 4,11,16,23, 4,11,16,23, 4,11,16,23,
        6,10,15,21, 6,10,15,21, 6,10,15,21, 6,10,15,21
    };
    static constexpr std::uint32_t constants[64] = {
        0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
        0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
        0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
        0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
        0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
        0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
        0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
        0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
    };
    std::vector<std::uint8_t> bytes(input.begin(), input.end());
    const std::uint64_t bitLength = static_cast<std::uint64_t>(bytes.size()) * 8;
    bytes.push_back(0x80);
    while ((bytes.size() % 64) != 56) bytes.push_back(0);
    for (int i = 0; i < 8; ++i) bytes.push_back(static_cast<std::uint8_t>(bitLength >> (i * 8)));

    std::uint32_t a0 = 0x67452301;
    std::uint32_t b0 = 0xefcdab89;
    std::uint32_t c0 = 0x98badcfe;
    std::uint32_t d0 = 0x10325476;
    for (size_t offset = 0; offset < bytes.size(); offset += 64) {
        std::uint32_t words[16]{};
        for (int i = 0; i < 16; ++i) {
            const size_t base = offset + static_cast<size_t>(i) * 4;
            words[i] = static_cast<std::uint32_t>(bytes[base]) |
                       (static_cast<std::uint32_t>(bytes[base + 1]) << 8) |
                       (static_cast<std::uint32_t>(bytes[base + 2]) << 16) |
                       (static_cast<std::uint32_t>(bytes[base + 3]) << 24);
        }
        std::uint32_t a = a0, b = b0, c = c0, d = d0;
        for (std::uint32_t i = 0; i < 64; ++i) {
            std::uint32_t f, g;
            if (i < 16) { f = (b & c) | (~b & d); g = i; }
            else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
            else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) % 16; }
            else { f = c ^ (b | ~d); g = (7 * i) % 16; }
            const std::uint32_t next = d;
            d = c;
            c = b;
            b += rotateLeft(a + f + constants[i] + words[g], shifts[i]);
            a = next;
        }
        a0 += a; b0 += b; c0 += c; d0 += d;
    }
    char digest[33]{};
    const std::uint32_t state[4] = {a0, b0, c0, d0};
    for (int i = 0; i < 16; ++i) {
        const unsigned byte = (state[i / 4] >> ((i % 4) * 8)) & 0xff;
        std::snprintf(digest + i * 2, 3, "%02x", byte);
    }
    return digest;
}

std::string scoreDigest(const std::string& identifier, const std::string& scoreText) {
    return md5("3df32" + md5(identifier) + "21fds1PlaneArcadeF5A4f" + scoreText + "Ad_5:");
}

void updateScoreUpload() {
    const std::string recordPath = path("Models\\structures\\tank3destroy.bmp");
    std::ifstream input(recordPath);
    std::string identifier, storedScoreText, storedDigest;
    if (!(input >> identifier >> storedScoreText >> storedDigest)) return;
    const int storedScore = std::atoi(storedScoreText.c_str());
    if (storedScore != 0 && scoreDigest(identifier, storedScoreText) != storedDigest) return;
    if (bestBattleScore <= storedScore) return;

    identifier = std::to_string(std::rand());
    const std::string scoreText = std::to_string(bestBattleScore);
    const std::string digest = scoreDigest(identifier, scoreText);
    std::ofstream record(recordPath, std::ios::trunc);
    record << identifier << ' ' << scoreText << ' ' << digest;

    const std::string url = "http://www.iwannaplay.com/ag/hs/write.php?action=" + digest +
        "&game=PlaneArcade&id=" + identifier + "&score=" + scoreText + "&version=1.15";
    std::ofstream upload(path("upload score.htm"), std::ios::trunc);
    upload << "<html><head><meta http-equiv=\"refresh\" content=\"0; url=" << url
           << "\"><title>Upload score</title></head><body><h2>Please wait</h2>"
              "<h3>Loading...</h3></body></html>";
}

float elapsedMilliseconds() {
    return std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - screenStarted).count();
}

float transitionMilliseconds() {
    return std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - transitionStarted).count();
}

void changeScreen(Screen next) {
    if (next == Screen::Flight && !flightAudioActive) {
        closeAudioAlias("music");
        if (configuration.musicEnabled) playLoopingAudio("music", path("Sounds\\gamemusic.mp3"));
        if (configuration.soundEnabled) playLoopingAudio("engine", path("Sounds\\SpitFire.wav"));
        flightAudioActive = true;
    } else if (screen == Screen::Flight && next != Screen::Flight) {
        closeAudioAlias("enemyengine");
        enemyEngineKind = -1;
        closeAudioAlias("engine");
        closeAudioAlias("music");
        if (configuration.musicEnabled) playLoopingAudio("music", path("Sounds\\menumusic.mp3"));
        flightAudioActive = false;
    }
    screen = next;
    screenStarted = std::chrono::steady_clock::now();
    if (next == Screen::Flight) {
        closeAudioAlias("enemyengine");
        switch (selectedMission) {
        case 0: aircraftPosition = {0, 400, -4000}; break;
        case 1: aircraftPosition = {0, 500, -4000}; break;
        case 4: aircraftPosition = {-6884, 250, 6302}; break;
        case 6: aircraftPosition = {0, 450, -8702}; break;
        case 7:
        case 8: aircraftPosition = {-6884, 450, 5702}; break;
        case 11: aircraftPosition = {8904, 350, -9592}; break;
        default: aircraftPosition = {-6884, 408, 5302}; break;
        }
        if (selectedMission >= 2 && selectedMission != 6 && selectedMission != 11) {
            aircraftPitch = 0.4f;
            aircraftYaw = 3.1400001f;
        } else if (selectedMission == 11) {
            aircraftPitch = 0.0f;
            aircraftYaw = -0.8f;
        } else {
            aircraftPitch = 0.0f;
            aircraftYaw = 0.0f;
        }
        aircraftRoll = 0.0f;
        aircraftSpeed = 150.0f;
        static constexpr int missionBombs[16] = {5, 5, 5, 0, 0, 25, 0, 20, 13, 30, 25, 2, 10, 5, 7, 25};
        bombs = missionBombs[std::clamp(selectedMission, 0, 15)];
        playerHealth = 100;
        desertWorld = selectedMission == 5 || selectedMission == 6 || selectedMission == 7 ||
                      selectedMission == 8 || selectedMission == 10;
        populateMission(selectedMission);
        firingDelay = 0.0f;
        missionCompleted = false;
        missionFailed = false;
        flightElapsedTime = 0.0f;
        missionResultTime = 0.0f;
        score = 0;
        cameraMode = 0;
        mapVisible = true;
        compassVisible = true;
        fpsVisible = false;
        missionWaveTriggered = false;
        missionObjectiveTriggered = false;
        missionTriggerMask = 0;
        missionObjectiveTime = -1.0f;
        missionSecondaryTime = -1.0f;
        joystickTriggerDown = false;
        joystickBombDown = false;
        flightPaused = false;
        enemyEngineKind = -1;
        lastFlightFrame = screenStarted;
    }
}

bool hit(float centerX, float centerY, float width, float height) {
    return pointerPosition.x >= centerX - width * 0.5f && pointerPosition.x <= centerX + width * 0.5f &&
           pointerPosition.y >= centerY - height * 0.5f && pointerPosition.y <= centerY + height * 0.5f;
}

void background(Graphics& graphics) {
    const Color sampled(255, 190, 190, 169);
    graphics.Clear(sampled);
    if (assets->background.width() > 0)
        assets->background.drawStretched(graphics, Rect(0, 0, logicalWidth, logicalHeight));
}

void drawCursor(Graphics& graphics) {
    assets->cursor.draw(graphics, static_cast<float>(pointerPosition.x + 15),
                        static_cast<float>(pointerPosition.y + 15));
}

void drawMenuFooter(Graphics& graphics) {
    Font font(L"Arial", 18.0f, FontStyleRegular, UnitPixel);
    SolidBrush black(Color(255, 0, 0, 0));
    graphics.DrawString(L"Air Guard Full v1.15 (c) 3Division Entertainment 2004", -1, &font,
                        PointF(20.0f, static_cast<float>(logicalHeight - 22)), &black);
}

void renderIntro3D(Graphics& graphics) {
    background(graphics);
    const float timer = elapsedMilliseconds();
    assets->logo3DGames.draw(graphics, 400.0f, 300.0f);
    float travel = timer;
    if (timer >= 1000.0f && timer <= 3000.0f) travel = 1000.0f;
    else if (timer > 3000.0f) travel = 4000.0f - timer;
    assets->logoPLE.draw(graphics, 400.0f, 710.0f - travel * 0.14285715f);
    if (timer > 4000.0f || pointerPressed) changeScreen(Screen::MainMenu);
}

void renderLoading(Graphics& graphics) {
    background(graphics);
    assets->loading.draw(graphics, 400.0f, 300.0f);
    for (int i = 0; i < loadingStage; ++i)
        assets->progress.draw(graphics, 284.0f + i * 32.0f, 350.0f);
    static constexpr const wchar_t* messages[9] = {
        L"Loading SkyBox", L"Initalizing StencilShadow", L"Loading Models", L"Loading Sounds",
        L"Loading SpitFire", L"Initializing World", L"Initializing Particle Systems",
        L"Initializing Particle Systems", L"Initializing Particle Systems"
    };
    Font font(L"Arial", 18.0f, FontStyleRegular, UnitPixel);
    SolidBrush text(Color(255, 72, 74, 64));
    graphics.DrawString(messages[std::clamp(loadingStage, 0, 8)], -1, &font, PointF(0, 390), &text);
}

void renderIntroBecher(Graphics& graphics) {
    background(graphics);
    const float timer = elapsedMilliseconds();
    float alpha = timer < 1000.0f ? timer / 1000.0f : (timer > 3000.0f ? (4000.0f - timer) / 1000.0f : 1.0f);
    assets->becherGame.draw(graphics, 400.0f, 300.0f, std::clamp(alpha, 0.0f, 1.0f));
    if (timer > 4000.0f || pointerPressed) changeScreen(Screen::MainMenu);
}

void renderMainMenu(Graphics& graphics) {
    background(graphics);
    const float phase = mainMenuLeaving
        ? std::max(mainMenuLeaveTarget, 2.0f - transitionMilliseconds() * 0.001f)
        : std::min(mainMenuPhaseOffset + elapsedMilliseconds() * 0.001f, 2.0f);
    const float settled = std::min(phase, 1.0f);
    const float buttons = phase - 1.0f;

    assets->logoPlane.draw(graphics, settled * 460.0f - 300.0f, 90.0f);
    assets->logoAir.draw(graphics, settled * 565.0f - 200.0f, 500.0f - settled * 415.0f);
    assets->logoArcade.draw(graphics, 600.0f, settled * 160.0f - 70.0f);
    assets->logoPilot.draw(graphics, 1200.0f - settled * 620.0f, 430.0f);
    assets->logoBecher.draw(graphics, 221.0f, 546.0f, std::max(buttons, 0.0f));

    const std::array<float, 4> x = {buttons * 460.0f - 250.0f, buttons * 440.0f - 250.0f,
                                    buttons * 420.0f - 250.0f, buttons * 400.0f - 250.0f};
    const std::array<float, 4> y = {230.0f, 280.0f, 330.0f, 380.0f};
    const std::array<const ImageAsset*, 4> buttonsImages = {
        &assets->campaign, &assets->battle, &assets->creditsButton, &assets->exitButton
    };
    for (size_t i = 0; i < buttonsImages.size(); ++i) {
        const bool hovered = phase >= 2.0f && hit(x[i], y[i], 180.0f, 40.0f);
        buttonsImages[i]->draw(graphics, x[i], y[i], 1.0f,
                               hovered ? 1.0f : 0.284f, hovered ? 1.0f : 0.278f, hovered ? 1.0f : 0.227f);
    }

    if (!mainMenuLeaving && phase >= 2.0f && pointerPressed) {
        if (hit(x[0], y[0], 180.0f, 40.0f)) {
            mainMenuLeaving = true;
            mainMenuLeaveTarget = 1.0f;
            mainMenuDestination = Screen::Campaign;
            mainMenuQuit = false;
            transitionStarted = std::chrono::steady_clock::now();
        } else if (hit(x[1], y[1], 180.0f, 40.0f)) {
            selectedMission = 0;
            mainMenuLeaving = true;
            mainMenuLeaveTarget = 0.0f;
            mainMenuDestination = Screen::Briefing;
            mainMenuQuit = false;
            transitionStarted = std::chrono::steady_clock::now();
        } else if (hit(x[2], y[2], 180.0f, 40.0f)) {
            mainMenuLeaving = true;
            mainMenuLeaveTarget = 1.0f;
            mainMenuDestination = Screen::Credits;
            mainMenuQuit = false;
            transitionStarted = std::chrono::steady_clock::now();
        } else if (hit(x[3], y[3], 180.0f, 40.0f)) {
            mainMenuLeaving = true;
            mainMenuLeaveTarget = 0.0f;
            mainMenuQuit = true;
            transitionStarted = std::chrono::steady_clock::now();
        }
        else if (pointerPosition.y > 519 && pointerPosition.y < 560) {
            const char* target = nullptr;
            if (pointerPosition.x > 4 && pointerPosition.x < 95)
                target = "http://www.iwannaplay.com/ag/ag_send.php";
            else if (pointerPosition.x > 102 && pointerPosition.x < 220)
                target = "http://www.iwannaplay.com";
            else if (pointerPosition.x > 235 && pointerPosition.x < 330) {
                updateScoreUpload();
                target = "upload score.htm";
            } else if (pointerPosition.x > 350 && pointerPosition.x < 445)
                target = "http://www.iwannaplay.com/ag/ag_buy.php";
            if (target) {
                ShellExecuteA(nullptr, "open", target, nullptr, rootDirectory.c_str(), SW_SHOW);
                PostQuitMessage(0);
            }
        }
    }
    drawMenuFooter(graphics);
    drawCursor(graphics);
    if (mainMenuLeaving && phase <= mainMenuLeaveTarget) {
        const bool quit = mainMenuQuit;
        const Screen destination = mainMenuDestination;
        mainMenuLeaving = false;
        if (quit) PostQuitMessage(0);
        else changeScreen(destination);
    }
}

void renderCampaign(Graphics& graphics) {
    background(graphics);
    if (campaignPopup != 0 &&
        std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - campaignPopupStarted).count() > 2500.0f)
        campaignPopup = 0;
    const bool dismissPopup = campaignPopup != 0 && pointerPressed;
    if (dismissPopup) campaignPopup = 0;
    const bool allowClicks = !dismissPopup && campaignPopup == 0;
    const float phase = submenuLeaving
        ? std::max(0.0f, 1.0f - transitionMilliseconds() * 0.001f)
        : std::min(elapsedMilliseconds() * 0.001f, 1.0f);
    assets->logoPlane.draw(graphics, 160.0f, 90.0f);
    assets->logoAir.draw(graphics, 365.0f, 85.0f);
    assets->logoArcade.draw(graphics, 600.0f, 90.0f);
    assets->logoPilot.draw(graphics, 580.0f, 430.0f);
    for (int mission = 1; mission <= 15; ++mission) {
        const int index = mission - 1;
        const int column = index % 3;
        const int row = index / 3;
        const float x = phase * 400.0f + column * 80.0f - 300.0f;
        const float y = row * 70.0f + 200.0f;
        const bool enabled = mission <= unlockedMission;
        const bool inside = !submenuLeaving && phase >= 1.0f && hit(x, y, 64.0f, 32.0f);
        const bool hovered = enabled && inside;
        assets->missions[static_cast<size_t>(index)].draw(
            graphics, x, y, 1.0f,
            hovered ? 1.0f : (enabled ? 1.0f : 0.5f),
            hovered ? 1.0f : (enabled ? 1.0f : 0.45f),
            hovered ? 1.0f : (enabled ? 1.0f : 0.3f));
        if (allowClicks && pointerPressed && inside) {
            if (enabled) {
                selectedMission = mission;
                submenuLeaving = true;
                submenuDestination = Screen::Briefing;
                transitionStarted = std::chrono::steady_clock::now();
            } else {
                campaignPopup = 3;
                campaignPopupStarted = std::chrono::steady_clock::now();
            }
        }
    }
    const float backY = 650.0f - phase * 110.0f;
    const bool backHovered = !submenuLeaving && phase >= 1.0f && hit(400.0f, backY, 180.0f, 40.0f);
    assets->backButton.draw(graphics, 400.0f, backY, 1.0f,
                            backHovered ? 1.0f : 0.284f,
                            backHovered ? 1.0f : 0.278f,
                            backHovered ? 1.0f : 0.227f);
    if (allowClicks && pointerPressed && backHovered) {
        submenuLeaving = true;
        submenuDestination = Screen::MainMenu;
        transitionStarted = std::chrono::steady_clock::now();
    }
    drawMenuFooter(graphics);
    if (campaignPopup >= 1 && campaignPopup <= 3)
        assets->campaignWindows[static_cast<size_t>(campaignPopup - 1)].draw(graphics, 400.0f, 300.0f);
    drawCursor(graphics);
    if (submenuLeaving && phase <= 0.0f) {
        const Screen destination = submenuDestination;
        submenuLeaving = false;
        if (destination == Screen::MainMenu) mainMenuPhaseOffset = 1.0f;
        changeScreen(destination);
    }
}

void renderCredits(Graphics& graphics) {
    background(graphics);
    const float phase = submenuLeaving
        ? std::max(0.0f, 1.0f - transitionMilliseconds() * 0.001f)
        : std::min(elapsedMilliseconds() * 0.001f, 1.0f);
    assets->logoPlane.draw(graphics, 160.0f, 90.0f);
    assets->logoAir.draw(graphics, 365.0f, 85.0f);
    assets->logoArcade.draw(graphics, 600.0f, 90.0f);
    assets->logoPilot.draw(graphics, 580.0f, 430.0f);
    assets->credits.draw(graphics, phase * 500.0f - 300.0f, 340.0f);
    Font font(L"Arial", 18.0f, FontStyleBold, UnitPixel);
    SolidBrush white(Color(255, 255, 255, 255));
    const std::array<std::pair<const wchar_t*, float>, 5> credits = {{
        {L"Peter Adamcik", 225.0f},
        {L"Simon Meszaros, Peter Adamcik", 265.0f},
        {L"Martin Hubacek", 305.0f},
        {L"aDuan Music Studio", 345.0f},
        {L"HS23 Band", 365.0f}
    }};
    if (phase >= 1.0f)
        for (const auto& credit : credits)
            graphics.DrawString(credit.first, -1, &font, PointF(80.0f, credit.second), &white);
    const float backY = 650.0f - phase * 110.0f;
    const bool backHovered = !submenuLeaving && phase >= 1.0f && hit(400.0f, backY, 180.0f, 40.0f);
    assets->backButton.draw(graphics, 400.0f, backY, 1.0f,
                            backHovered ? 1.0f : 0.284f,
                            backHovered ? 1.0f : 0.278f,
                            backHovered ? 1.0f : 0.227f);
    if (pointerPressed && backHovered) {
        submenuLeaving = true;
        submenuDestination = Screen::MainMenu;
        transitionStarted = std::chrono::steady_clock::now();
    }
    drawMenuFooter(graphics);
    drawCursor(graphics);
    if (submenuLeaving && phase <= 0.0f) {
        submenuLeaving = false;
        mainMenuPhaseOffset = 1.0f;
        changeScreen(Screen::MainMenu);
    }
}

void renderBriefing(Graphics& graphics) {
    background(graphics);
    const int briefingIndex = std::clamp(selectedMission, 0, 15);
    assets->briefings[static_cast<size_t>(briefingIndex)].draw(graphics, 400.0f, 300.0f);
    const std::array<float, 3> x = {290.0f, 400.0f, 510.0f};
    const std::array<const ImageAsset*, 3> buttons = {
        &assets->briefingStart, &assets->briefingReplay, &assets->briefingBack
    };
    for (size_t i = 0; i < buttons.size(); ++i) {
        const bool hovered = hit(x[i], 460.0f, 98.0f, 36.0f);
        buttons[i]->draw(graphics, x[i], 460.0f, hovered ? 1.0f : 0.6f);
    }
    if (pointerPressed) {
        if (hit(x[0], 460.0f, 98.0f, 36.0f)) changeScreen(Screen::Flight);
        else if (hit(x[1], 460.0f, 98.0f, 36.0f)) changeScreen(Screen::Briefing);
        else if (hit(x[2], 460.0f, 98.0f, 36.0f))
            changeScreen(selectedMission == 0 ? Screen::MainMenu : Screen::Campaign);
    }
    drawMenuFooter(graphics);
    drawCursor(graphics);
}

void perspective(float fieldOfView, float aspect, float nearPlane, float farPlane) {
    const float top = std::tan(fieldOfView * 0.5f * 3.1415926535f / 180.0f) * nearPlane;
    const float right = top * aspect;
    glFrustum(-right, right, -top, top, nearPlane, farPlane);
}

void spawnExplosion(const Vector3& position, float size) {
    if (!configuration.particleEffects) return;
    explosionEffects.push_back({position, 0.0f, size});
    if (configuration.soundEnabled)
        playOneShot("explosion", path(size >= 150.0f ? "Sounds\\ExplodeLarge.wav" :
                                                      "Sounds\\ExplodeSmall.wav"));
}

void dropPlayerBomb() {
    if (screen != Screen::Flight || bombs <= 0) return;
    --bombs;
    const float horizontal = std::cos(aircraftPitch);
    Vector3 forward{std::sin(aircraftYaw) * horizontal,
                    std::sin(aircraftPitch),
                    std::cos(aircraftYaw) * horizontal};
    projectiles.push_back({aircraftPosition,
                           {forward.x * aircraftSpeed, forward.y * aircraftSpeed - 15.0f,
                            forward.z * aircraftSpeed},
                           12.0f, true, false});
}

void updateAircraft(float seconds) {
    flightElapsedTime += seconds;
    if (keys[VK_SHIFT]) aircraftSpeed = std::min(aircraftSpeed + 80.0f * seconds, 300.0f);
    if (keys[VK_CONTROL]) aircraftSpeed = std::max(aircraftSpeed - 80.0f * seconds, 60.0f);
    const float mousePitch = (300.0f - pointerPosition.y) / 300.0f * configuration.mouseY / 10.0f;
    const float mouseRoll = (pointerPosition.x - 400.0f) / 400.0f * configuration.mouseX / 10.0f;
    float pitchInput = mousePitch;
    float rollInput = mouseRoll;
    bool firing = pointerDown;
    if (configuration.joystickEnabled) {
        JOYINFOEX joystick{};
        joystick.dwSize = sizeof(joystick);
        joystick.dwFlags = JOY_RETURNX | JOY_RETURNY | JOY_RETURNBUTTONS;
        if (joyGetPosEx(JOYSTICKID1, &joystick) == JOYERR_NOERROR) {
            rollInput = (static_cast<float>(joystick.dwXpos) - 32767.5f) / 32767.5f;
            pitchInput = (32767.5f - static_cast<float>(joystick.dwYpos)) / 32767.5f;
            if (std::abs(rollInput) < 0.08f) rollInput = 0.0f;
            if (std::abs(pitchInput) < 0.08f) pitchInput = 0.0f;
            const bool trigger = (joystick.dwButtons & JOY_BUTTON1) != 0;
            const bool bombButton = (joystick.dwButtons & JOY_BUTTON2) != 0;
            firing = firing || trigger;
            if (trigger && !joystickTriggerDown && configuration.soundEnabled)
                playOneShot("gunfire", path("Sounds\\Fire.wav"));
            if (bombButton && !joystickBombDown) dropPlayerBomb();
            joystickTriggerDown = trigger;
            joystickBombDown = bombButton;
        }
    }
    aircraftPitch += pitchInput * seconds * (configuration.invert ? -0.8f : 0.8f);
    aircraftRoll += rollInput * seconds * 1.2f;
    aircraftRoll *= std::pow(0.25f, seconds);
    aircraftPitch = std::clamp(aircraftPitch, -1.35f, 1.35f);
    aircraftRoll = std::clamp(aircraftRoll, -1.45f, 1.45f);
    aircraftYaw += std::sin(aircraftRoll) * seconds * 0.65f;
    const float horizontal = std::cos(aircraftPitch);
    Vector3 forward{std::sin(aircraftYaw) * horizontal,
                    std::sin(aircraftPitch),
                    std::cos(aircraftYaw) * horizontal};
    aircraftPosition.x += forward.x * aircraftSpeed * seconds;
    aircraftPosition.y += forward.y * aircraftSpeed * seconds;
    aircraftPosition.z += forward.z * aircraftSpeed * seconds;
    aircraftPosition.y = std::max(
        aircraftPosition.y,
        terrainHeightAt(aircraftPosition.x, aircraftPosition.z) + 15.0f);

    firingDelay -= seconds;
    if (firing && firingDelay <= 0.0f) {
        projectiles.push_back({
            {aircraftPosition.x + forward.x * 45.0f,
             aircraftPosition.y + forward.y * 45.0f,
             aircraftPosition.z + forward.z * 45.0f},
            {forward.x * 900.0f, forward.y * 900.0f, forward.z * 900.0f},
            2.5f, false, false
        });
        firingDelay = 0.09f;
    }

    for (auto& entity : missionAircraft) {
        if (!entity.active) continue;
        Vector3 destination = entity.kind == AircraftKind::Bombardier &&
                                      (entity.target.x != 0 || entity.target.z != 0)
                                  ? entity.target : aircraftPosition;
        const float dx = destination.x - entity.position.x;
        const float dy = destination.y - entity.position.y;
        const float dz = destination.z - entity.position.z;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (distance > 1.0f) {
            const float desiredYaw = std::atan2(dx, dz);
            float yawDifference = desiredYaw - entity.yaw;
            while (yawDifference > 3.1415926535f) yawDifference -= 6.283185307f;
            while (yawDifference < -3.1415926535f) yawDifference += 6.283185307f;
            entity.yaw += std::clamp(yawDifference, -1.2f * seconds, 1.2f * seconds);
            const float speed = entity.kind == AircraftKind::Messerschmitt ? 145.0f :
                                (entity.kind == AircraftKind::Volker ? 105.0f : 85.0f);
            entity.position.x += std::sin(entity.yaw) * speed * seconds;
            entity.position.y += std::clamp(dy, -speed, speed) * seconds;
            entity.position.z += std::cos(entity.yaw) * speed * seconds;
        }
        entity.firingDelay -= seconds;
        if (entity.team == 0 && entity.kind != AircraftKind::Bombardier &&
            distance > 1.0f && distance < 2500.0f) {
            const float desiredYaw = std::atan2(dx, dz);
            float yawDifference = desiredYaw - entity.yaw;
            while (yawDifference > 3.1415926535f) yawDifference -= 6.283185307f;
            while (yawDifference < -3.1415926535f) yawDifference += 6.283185307f;
            if (std::abs(yawDifference) < 0.08f && std::abs(dy) < 100.0f &&
                entity.firingDelay <= 0.0f) {
                const Vector3 direction{dx / distance, dy / distance, dz / distance};
                projectiles.push_back({
                    {entity.position.x + direction.x * 42.0f,
                     entity.position.y + direction.y * 42.0f,
                     entity.position.z + direction.z * 42.0f},
                    {direction.x * 900.0f, direction.y * 900.0f, direction.z * 900.0f},
                    3.0f, false, true
                });
                entity.firingDelay = 0.13f;
            }
        }
    }
    if (configuration.soundEnabled) {
        const AircraftEntity* closest = nullptr;
        float closestDistance = std::numeric_limits<float>::max();
        for (const auto& entity : missionAircraft) {
            if (!entity.active) continue;
            const float dx = entity.position.x - aircraftPosition.x;
            const float dy = entity.position.y - aircraftPosition.y;
            const float dz = entity.position.z - aircraftPosition.z;
            const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (distance < closestDistance) { closestDistance = distance; closest = &entity; }
        }
        const int kind = closest && closestDistance < 6000.0f ? static_cast<int>(closest->kind) : -1;
        if (kind != enemyEngineKind) {
            closeAudioAlias("enemyengine");
            enemyEngineKind = kind;
            if (kind == static_cast<int>(AircraftKind::Messerschmitt))
                playLoopingWave("enemyengine", path("Sounds\\Messerschmitt.wav"));
            else if (kind == static_cast<int>(AircraftKind::Volker))
                playLoopingWave("enemyengine", path("Sounds\\Volker.wav"));
            else if (kind == static_cast<int>(AircraftKind::Bombardier))
                playLoopingWave("enemyengine", path("Sounds\\Bombarder.wav"));
        }
        if (kind >= 0) {
            int leftVolume = 1000;
            int rightVolume = 1000;
            if (configuration.sound3DEnabled) {
                const float attenuation = std::clamp(1000.0f - closestDistance / 6.0f, 80.0f, 1000.0f);
                const float dx = closest->position.x - aircraftPosition.x;
                const float dz = closest->position.z - aircraftPosition.z;
                const float localX = dx * std::cos(aircraftYaw) - dz * std::sin(aircraftYaw);
                const float pan = std::clamp(localX / std::max(closestDistance, 1.0f), -1.0f, 1.0f);
                leftVolume = static_cast<int>(attenuation * (pan > 0.0f ? 1.0f - pan * 0.75f : 1.0f));
                rightVolume = static_cast<int>(attenuation * (pan < 0.0f ? 1.0f + pan * 0.75f : 1.0f));
            }
            std::string command = "setaudio enemyengine left volume to " + std::to_string(leftVolume);
            mciSendStringA(command.c_str(), nullptr, 0, nullptr);
            command = "setaudio enemyengine right volume to " + std::to_string(rightVolume);
            mciSendStringA(command.c_str(), nullptr, 0, nullptr);
        }
    }

    for (auto& projectile : projectiles) {
        projectile.life -= seconds;
        projectile.position.x += projectile.velocity.x * seconds;
        projectile.position.y += projectile.velocity.y * seconds;
        projectile.position.z += projectile.velocity.z * seconds;
        if (projectile.bomb) projectile.velocity.y -= 120.0f * seconds;
        if (projectile.life <= 0.0f) continue;
        if (projectile.enemy) {
            const float dx = projectile.position.x - aircraftPosition.x;
            const float dy = projectile.position.y - aircraftPosition.y;
            const float dz = projectile.position.z - aircraftPosition.z;
            if (dx * dx + dy * dy + dz * dz <= 55.0f * 55.0f) {
                playerHealth = std::max(0, playerHealth - 10);
                spawnExplosion(projectile.position, 35.0f);
                projectile.life = 0.0f;
            }
            continue;
        }
        for (auto& entity : missionAircraft) {
            if (!entity.active) continue;
            const float dx = projectile.position.x - entity.position.x;
            const float dy = projectile.position.y - entity.position.y;
            const float dz = projectile.position.z - entity.position.z;
            const float radius = projectile.bomb ? 120.0f : 55.0f;
            if (dx * dx + dy * dy + dz * dz <= radius * radius) {
                entity.health -= projectile.bomb ? 100 : 10;
                spawnExplosion(projectile.position,
                               entity.health <= 0 ? 220.0f : (projectile.bomb ? 120.0f : 35.0f));
                projectile.life = 0.0f;
                if (entity.health <= 0) {
                    entity.active = false;
                    const int points = entity.kind == AircraftKind::Messerschmitt ? 300 :
                                       (entity.kind == AircraftKind::Volker ? 400 : 500);
                    score += points;
                    floatingScores.push_back({entity.position, 0.0f, points});
                }
                break;
            }
        }
        if (projectile.life > 0.0f) {
            for (auto& entity : missionGround) {
                if (!entity.active) continue;
                const float dx = projectile.position.x - entity.position.x;
                const float dy = projectile.position.y - entity.position.y;
                const float dz = projectile.position.z - entity.position.z;
                const float radius = projectile.bomb ? 150.0f : 45.0f;
                if (dx * dx + dy * dy + dz * dz <= radius * radius) {
                    entity.health -= projectile.bomb ? 220 : 10;
                    spawnExplosion(projectile.position,
                                   entity.health <= 0 ? 240.0f : (projectile.bomb ? 140.0f : 35.0f));
                    projectile.life = 0.0f;
                    if (entity.health <= 0) {
                        entity.active = false;
                        if (entity.objective) {
                            score += 100;
                            floatingScores.push_back({entity.position, 0.0f, 100});
                        }
                    }
                    break;
                }
            }
        }
        if (projectile.life > 0.0f) {
            for (auto& entity : missionTrucks) {
                if (!entity.active) continue;
                const float dx = projectile.position.x - entity.position.x;
                const float dy = projectile.position.y - entity.position.y;
                const float dz = projectile.position.z - entity.position.z;
                const float radius = projectile.bomb ? 130.0f : 38.0f;
                if (dx * dx + dy * dy + dz * dz <= radius * radius) {
                    entity.health -= projectile.bomb ? 160 : 10;
                    spawnExplosion(projectile.position,
                                   entity.health <= 0 ? 190.0f : (projectile.bomb ? 120.0f : 35.0f));
                    projectile.life = 0.0f;
                    if (entity.health <= 0) {
                        entity.active = false;
                        score += 200;
                        floatingScores.push_back({entity.position, 0.0f, 200});
                    }
                    break;
                }
            }
        }
        if (projectile.life > 0.0f && projectile.bomb &&
            projectile.position.y <= terrainHeightAt(projectile.position.x, projectile.position.z)) {
            projectile.position.y = terrainHeightAt(projectile.position.x, projectile.position.z);
            spawnExplosion(projectile.position, 180.0f);
            projectile.life = 0.0f;
        }
    }
    projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(),
                                     [](const Projectile& projectile) { return projectile.life <= 0.0f; }),
                      projectiles.end());
    for (auto& effect : explosionEffects) effect.age += seconds;
    explosionEffects.erase(std::remove_if(explosionEffects.begin(), explosionEffects.end(),
                                           [](const ExplosionEffect& effect) { return effect.age >= 1.0f; }),
                           explosionEffects.end());
    for (auto& floating : floatingScores) floating.age += seconds;
    floatingScores.erase(std::remove_if(floatingScores.begin(), floatingScores.end(),
                                        [](const FloatingScore& floating) { return floating.age >= 2.0f; }),
                         floatingScores.end());

    const float elapsed = flightElapsedTime;
    const bool groundDestroyed = std::none_of(missionGround.begin(), missionGround.end(),
                                              [](const GroundEntity& entity) {
                                                  return entity.objective && entity.active;
                                              });
    const bool hostileTrucksDestroyed = std::none_of(missionTrucks.begin(), missionTrucks.end(),
                                                     [](const TruckEntity& entity) {
                                                         return entity.active && entity.team == 0;
                                                     });
    if (selectedMission == 4 && !missionObjectiveTriggered &&
        aircraftPosition.x > 1651.0f && aircraftPosition.x < 5023.0f &&
        aircraftPosition.z > -3646.0f && aircraftPosition.z < -898.0f) {
        missionObjectiveTriggered = true;
        missionObjectiveTime = elapsed;
        addAircraft(AircraftKind::Messerschmitt, 50, 1, {4443, 600, -3873}, -0.8f);
        addAircraft(AircraftKind::Messerschmitt, 50, 1, {4998, 600, -3725}, -0.8f);
    }
    if (selectedMission == 5 && !missionObjectiveTriggered && groundDestroyed && hostileTrucksDestroyed) {
        missionObjectiveTriggered = true;
        missionObjectiveTime = elapsed;
        addAircraft(AircraftKind::Messerschmitt, 100, 0, {11500, 600, 4000}, -1.57f);
        addAircraft(AircraftKind::Messerschmitt, 100, 0, {11500, 300, 4300}, -1.57f);
    }
    if (selectedMission == 10) {
        if ((missionTriggerMask & 1u) == 0 && aircraftPosition.x > -4347 && aircraftPosition.x < -2443 &&
            aircraftPosition.z > -5271 && aircraftPosition.z < -3508) {
            missionTriggerMask |= 1u;
            addAircraft(AircraftKind::Messerschmitt, 150, 1, {-3853, 600, -6768}, 0);
        }
        if ((missionTriggerMask & 2u) == 0 && aircraftPosition.x > 4035 && aircraftPosition.x < 5809 &&
            aircraftPosition.z > -6090 && aircraftPosition.z < -4886) {
            missionTriggerMask |= 2u;
            addAircraft(AircraftKind::Volker, 150, 0, {7392, 700, -6042}, -1.5700001f);
        }
        if ((missionTriggerMask & 4u) == 0 && aircraftPosition.x > 2136 && aircraftPosition.x < 3558 &&
            aircraftPosition.z > -541 && aircraftPosition.z < 1576) {
            missionTriggerMask |= 4u;
            addAircraft(AircraftKind::Messerschmitt, 150, 1, {2595, 700, 4036}, -3.1400001f);
            addAircraft(AircraftKind::Messerschmitt, 150, 1, {2263, 700, 4288}, -3.1400001f);
        }
        if ((missionTriggerMask & 8u) == 0 && aircraftPosition.x > -2059 && aircraftPosition.x < 2035 &&
            aircraftPosition.z > 4591 && aircraftPosition.z < 7253) {
            missionTriggerMask |= 8u;
            addAircraft(AircraftKind::Volker, 100, 0, {-257, 600, 8913}, -3.1400001f);
        }
    }
    if (selectedMission == 12 && !missionObjectiveTriggered && aircraftPosition.z < 0.0f) {
        missionObjectiveTriggered = true;
        addAircraft(AircraftKind::Messerschmitt, 120, 0, {-6368.2f, 600, -12209.7f}, -18.3f);
        addAircraft(AircraftKind::Messerschmitt, 120, 1, {-3816.0f, 600, -12200.9f}, -19.2f);
        addAircraft(AircraftKind::Volker, 120, 0, {458.4f, 600, -11082.7f}, -20.0f);
    }
    if (selectedMission == 13 && missionAircraft.size() >= 3) {
        const auto closeTo = [](const Vector3& a, const Vector3& b) {
            return std::abs(a.x - b.x) < 2000.0f && std::abs(a.z - b.z) < 2000.0f;
        };
        if ((missionTriggerMask & 1u) == 0 && closeTo(aircraftPosition, missionAircraft[0].position)) {
            missionTriggerMask |= 1u;
            addAircraft(AircraftKind::Messerschmitt, 130, 1, {7939, 600, -8700}, 0);
            addAircraft(AircraftKind::Messerschmitt, 130, 1, {8353, 600, -8353}, 0);
        }
        if ((missionTriggerMask & 2u) == 0 && closeTo(aircraftPosition, missionAircraft[1].position)) {
            missionTriggerMask |= 2u;
            addAircraft(AircraftKind::Volker, 130, 0, {8523, 600, -3231}, -1.5700001f);
            addAircraft(AircraftKind::Volker, 130, 0, {8504, 600, -3685}, -1.5700001f);
        }
        if ((missionTriggerMask & 4u) == 0 && closeTo(aircraftPosition, missionAircraft[2].position)) {
            missionTriggerMask |= 4u;
            addAircraft(AircraftKind::Volker, 130, 0, {6878, 600, 8169}, -3.1400001f);
            addAircraft(AircraftKind::Volker, 130, 0, {5677, 600, 8753}, -3.1400001f);
        }
    }
    if (selectedMission == 15 && !missionWaveTriggered && aircraftPosition.z < -2500.0f) {
        missionWaveTriggered = true;
        missionObjectiveTime = elapsed;
        addAircraft(AircraftKind::Messerschmitt, 150, 1, {-200, 600, -6068}, 0);
        addAircraft(AircraftKind::Messerschmitt, 150, 1, {-2300, 600, -6068}, 0);
        addAircraft(AircraftKind::Messerschmitt, 150, 1, {-2600, 600, -6068}, 0);
        addAircraft(AircraftKind::Messerschmitt, 150, 1, {-2900, 600, -6068}, 0);
        addAircraft(AircraftKind::Messerschmitt, 150, 1, {-3200, 600, -6068}, 0);
        addAircraft(AircraftKind::Volker, 150, 0, {200, 600, -6068}, 0);
        addAircraft(AircraftKind::Volker, 150, 0, {2300, 600, -6068}, 0);
        addAircraft(AircraftKind::Volker, 150, 0, {2600, 600, -6068}, 0);
        addAircraft(AircraftKind::Volker, 150, 0, {2900, 600, -6068}, 0);
        addAircraft(AircraftKind::Volker, 150, 0, {3200, 600, -6068}, 0);
    }
    const bool atBase = aircraftPosition.x > -7000.0f && aircraftPosition.x < -6000.0f &&
                        aircraftPosition.z > 4000.0f && aircraftPosition.z < 5000.0f;
    const bool enemiesDestroyed = std::none_of(missionAircraft.begin(), missionAircraft.end(),
                                               [](const AircraftEntity& entity) { return entity.active && entity.team == 0; });
    const bool bombersDestroyed = std::none_of(missionAircraft.begin(), missionAircraft.end(),
                                               [](const AircraftEntity& entity) {
                                                   return entity.active && entity.kind == AircraftKind::Bombardier;
                                               });
    const bool allAircraftDestroyed = std::none_of(missionAircraft.begin(), missionAircraft.end(),
                                                   [](const AircraftEntity& entity) { return entity.active; });
    if (selectedMission == 2 && enemiesDestroyed && missionObjectiveTime < 0.0f)
        missionObjectiveTime = elapsed;
    if (selectedMission == 15 && missionWaveTriggered && allAircraftDestroyed && missionSecondaryTime < 0.0f)
        missionSecondaryTime = elapsed;
    if (selectedMission == 0) bestBattleScore = std::max(bestBattleScore, score);
    if (!missionCompleted && !missionFailed) {
        if (selectedMission == 1 && elapsed > 73.0f && atBase) missionCompleted = true;
        else if (selectedMission == 2) {
            if (elapsed > 300.0f) missionFailed = true;
            else if (elapsed > 20.0f && enemiesDestroyed && atBase) missionCompleted = true;
        } else if (selectedMission == 3) {
            if (elapsed > 20.0f && allAircraftDestroyed) missionCompleted = true;
        } else if (selectedMission == 4) {
            if (elapsed > 420.0f || (!missionObjectiveTriggered && aircraftPosition.y >= 400.0f)) missionFailed = true;
            else if (missionObjectiveTriggered && atBase) missionCompleted = true;
        } else if (selectedMission == 5) {
            if (missionObjectiveTriggered && allAircraftDestroyed) missionCompleted = true;
        } else if (selectedMission == 6) {
            if (elapsed > 180.0f) missionFailed = true;
            else if (elapsed >= 90.0f && allAircraftDestroyed) missionCompleted = true;
        } else if (selectedMission == 7) {
            if (hostileTrucksDestroyed) missionCompleted = true;
            else if (elapsed > 180.0f) missionFailed = true;
        } else if (selectedMission == 8) {
            if (bombersDestroyed) missionCompleted = true;
        } else if (selectedMission == 9) {
            if (elapsed > 300.0f) missionFailed = true;
            else if (allAircraftDestroyed && groundDestroyed && hostileTrucksDestroyed)
                missionCompleted = true;
        } else if (selectedMission == 10) {
            if (missionTriggerMask == 15u && allAircraftDestroyed && groundDestroyed && hostileTrucksDestroyed)
                missionCompleted = true;
        } else if (selectedMission == 11) {
            if (atBase && allAircraftDestroyed) missionCompleted = true;
        } else if (selectedMission == 12) {
            if (missionObjectiveTriggered && allAircraftDestroyed && hostileTrucksDestroyed)
                missionCompleted = true;
        } else if (selectedMission == 13) {
            if (missionTriggerMask == 7u && allAircraftDestroyed && hostileTrucksDestroyed)
                missionCompleted = true;
        } else if (selectedMission == 14) {
            if (elapsed > 20.0f && allAircraftDestroyed && groundDestroyed && hostileTrucksDestroyed)
                missionCompleted = true;
        } else if (selectedMission == 15) {
            if (missionWaveTriggered && allAircraftDestroyed && aircraftPosition.z < -7500.0f)
                missionCompleted = true;
        }
        if (std::abs(aircraftPosition.x) > 12500.0f ||
            std::abs(aircraftPosition.y) > 12500.0f ||
            std::abs(aircraftPosition.z) > 12500.0f)
            missionFailed = true;
        if (aircraftPosition.y <= terrainHeightAt(aircraftPosition.x, aircraftPosition.z) + 15.0f &&
            std::abs(aircraftPitch) > 0.7f) missionFailed = true;
        if (playerHealth <= 0) missionFailed = true;
        if (missionCompleted || missionFailed) missionResultTime = elapsed;
        if (missionCompleted && selectedMission > 0 && selectedMission >= unlockedMission) {
            unlockedMission = std::min(selectedMission + 1, 16);
            std::ofstream progress(path("Menu\\array.dat"), std::ios::trunc);
            progress << unlockedMission;
        }
    }
}

void drawWorldBillboard(GLuint texture, const Vector3& position, float size, float alpha = 1.0f) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glColor4f(1.0f, 1.0f, 1.0f, alpha);
    glPushMatrix();
    glTranslatef(position.x, position.y, position.z);
    GLfloat modelView[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, modelView);
    modelView[0] = 1.0f; modelView[1] = 0.0f; modelView[2] = 0.0f;
    modelView[4] = 0.0f; modelView[5] = 1.0f; modelView[6] = 0.0f;
    modelView[8] = 0.0f; modelView[9] = 0.0f; modelView[10] = 1.0f;
    glLoadMatrixf(modelView);
    const float half = size * 0.5f;
    glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex3f(-half,  half, 0);
    glTexCoord2f(1, 1); glVertex3f( half,  half, 0);
    glTexCoord2f(1, 0); glVertex3f( half, -half, 0);
    glTexCoord2f(0, 0); glVertex3f(-half, -half, 0);
    glEnd();
    glPopMatrix();
}

void drawFlightEffects() {
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_TEXTURE_2D);
    for (const auto& projectile : projectiles) {
        if (projectile.bomb) {
            glBindTexture(GL_TEXTURE_2D, bombMeshTexture);
            glColor4f(1, 1, 1, 1);
            glPushMatrix();
            glTranslatef(projectile.position.x, projectile.position.y, projectile.position.z);
            glRotatef(projectile.life * 180.0f, 1, 0, 0);
            bombMesh.draw();
            glPopMatrix();
            drawWorldBillboard(bombParticleTexture, projectile.position, 24.0f, 0.75f);
        } else {
            drawWorldBillboard(bulletParticleTexture, projectile.position, 9.0f);
        }
    }
    if (configuration.particleEffects) {
        for (const auto& entity : missionAircraft) {
            if (!entity.active || entity.health > 60) continue;
            Vector3 trail = entity.position;
            trail.y += 4.0f;
            drawWorldBillboard(entity.health <= 30 ? fireParticleTexture : smokeParticleTexture,
                               trail, entity.health <= 30 ? 70.0f : 55.0f, 0.85f);
        }
        for (const auto& effect : explosionEffects) {
            const size_t frame = std::min<size_t>(9, static_cast<size_t>(effect.age * 10.0f));
            drawWorldBillboard(effect.size <= 40.0f ? hitParticleTexture : explosionTextures[frame],
                               effect.position, effect.size * (0.65f + effect.age * 0.7f),
                               1.0f - effect.age * 0.35f);
            if (effect.size > 40.0f) {
                glBindTexture(GL_TEXTURE_2D, effect.size >= 150.0f ? fireball2Texture : fireball1Texture);
                glColor4f(1, 1, 1, std::max(0.0f, 1.0f - effect.age));
                glPushMatrix();
                glTranslatef(effect.position.x, effect.position.y, effect.position.z);
                const float scale = effect.size / 100.0f;
                glScalef(scale, scale, scale);
                (effect.size >= 150.0f ? fireball2Mesh : fireball1Mesh).draw();
                glPopMatrix();
                Vector3 debris = effect.position;
                debris.y += effect.age * 45.0f;
                drawWorldBillboard(genericParticleTexture, debris, effect.size * 0.22f,
                                   std::max(0.0f, 1.0f - effect.age));
            }
        }
        for (const auto& floating : floatingScores) {
            Vector3 position = floating.position;
            position.y += floating.age * 80.0f;
            const size_t index = static_cast<size_t>(std::clamp(floating.points / 100 - 1, 0, 9));
            drawWorldBillboard(scoreTextures[index], position,
                               72.0f * std::max(0.1f, 1.0f - floating.age * 0.45f),
                               std::max(0.0f, 1.0f - floating.age * 0.5f));
        }
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
    glColor4f(1, 1, 1, 1);
}

void drawPlayerShadow() {
    if (!configuration.shadows || cameraMode == 1) return;
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.36f);
    glPushMatrix();
    glTranslatef(aircraftPosition.x,
                 terrainHeightAt(aircraftPosition.x, aircraftPosition.z) + 1.0f,
                 aircraftPosition.z);
    glRotatef(aircraftYaw * 57.2957795f, 0, 1, 0);
    glScalef(1.1f, 0.01f, 1.1f);
    spitfireShadowMesh.draw();
    glPopMatrix();
    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glColor4f(1, 1, 1, 1);
}

void drawMissionAircraft() {
    for (const auto& entity : missionAircraft) {
        if (!entity.active) continue;
        const Mesh* mesh = nullptr;
        GLuint texture = 0;
        switch (entity.kind) {
        case AircraftKind::Messerschmitt:
            mesh = &messerschmittMesh;
            texture = entity.health <= 50 ? messerschmittDestroyedTexture : messerschmittTexture;
            break;
        case AircraftKind::Volker:
            mesh = &volkerMesh;
            texture = entity.health <= 60 ? volkerDestroyedTexture : volkerTexture;
            break;
        case AircraftKind::Bombardier:
            mesh = &bombardierMesh;
            texture = entity.health <= 150 ? bombardierDestroyedTexture : bombardierTexture;
            break;
        }
        glBindTexture(GL_TEXTURE_2D, texture);
        glPushMatrix();
        glTranslatef(entity.position.x, entity.position.y, entity.position.z);
        glRotatef(entity.yaw * 57.2957795f, 0, 1, 0);
        mesh->draw();
        glPopMatrix();
    }
}

void drawMissionGround() {
    glColor4f(1, 1, 1, 1);
    for (const auto& entity : missionGround) {
        if (entity.model < 0 || entity.model >= static_cast<int>(structureMeshes.size())) continue;
        const size_t model = static_cast<size_t>(entity.model);
        if (!entity.active && structureDestroyedTextures[model] == 0) continue;
        glBindTexture(GL_TEXTURE_2D, entity.active ? structureTextures[model] : structureDestroyedTextures[model]);
        glPushMatrix();
        glTranslatef(entity.position.x, entity.position.y, entity.position.z);
        glRotatef(entity.rotation.x * 57.2957795f, 1, 0, 0);
        glRotatef(entity.rotation.y * 57.2957795f, 0, 1, 0);
        glRotatef(entity.rotation.z * 57.2957795f, 0, 0, 1);
        structureMeshes[model].draw();
        glPopMatrix();
    }
    for (const auto& entity : missionTrucks) {
        glBindTexture(GL_TEXTURE_2D, entity.active ? truckTexture : truckDestroyedTexture);
        glPushMatrix();
        glTranslatef(entity.position.x, entity.position.y, entity.position.z);
        glRotatef(entity.rotation.x * 57.2957795f, 1, 0, 0);
        glRotatef(entity.rotation.y * 57.2957795f, 0, 1, 0);
        glRotatef(entity.rotation.z * 57.2957795f, 0, 0, 1);
        if (entity.active) {
            truckBackMesh.draw();
            truckCannonMesh.draw();
        } else {
            truckDestroyedMesh.draw();
        }
        glPopMatrix();
    }
}

void drawTerrain() {
    glColor4f(1, 1, 1, 1);
    std::uint32_t activeGroup = std::numeric_limits<std::uint32_t>::max();
    for (const auto& face : terrainFaces) {
        if (face.group != activeGroup) {
            if (activeGroup != std::numeric_limits<std::uint32_t>::max()) glEnd();
            activeGroup = face.group;
            GLuint texture = desertWorld ? desertTexture : terrainTexture;
            if (activeGroup == 1) texture = structureTextures[21];
            else if (activeGroup == 2) texture = structureTextures[22];
            else if (activeGroup == 3) texture = structureTextures[23];
            else if (activeGroup == 4) texture = structureTextures[20];
            else if (activeGroup == 5) texture = waterTexture;
            if (activeTexture) {
                activeTexture(0x84C1);
                if (activeGroup == 0) {
                    glEnable(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, desertWorld ? desertDetailTexture : terrainDetailTexture);
                    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                } else glDisable(GL_TEXTURE_2D);
                activeTexture(0x84C0);
            }
            glBindTexture(GL_TEXTURE_2D, texture);
            if (activeGroup >= 1 && activeGroup <= 4) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDisable(GL_CULL_FACE);
            } else {
                glDisable(GL_BLEND);
                glEnable(GL_CULL_FACE);
            }
            glBegin(GL_TRIANGLES);
        }
        for (const auto& vertex : face.vertex) {
            glNormal3f(vertex.nx, vertex.ny, vertex.nz);
            if (multiTexCoord2f && face.group == 0) {
                multiTexCoord2f(0x84C0, vertex.u0, vertex.v0);
                multiTexCoord2f(0x84C1, vertex.u1, vertex.v1);
            } else glTexCoord2f(vertex.u0, vertex.v0);
            glVertex3f(vertex.x, vertex.y, vertex.z);
        }
    }
    if (activeGroup != std::numeric_limits<std::uint32_t>::max()) glEnd();
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    if (activeTexture) {
        activeTexture(0x84C1);
        glDisable(GL_TEXTURE_2D);
        activeTexture(0x84C0);
    }
}

void drawSkybox(const Vector3& camera) {
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glPushMatrix();
    glTranslatef(camera.x, camera.y, camera.z);
    glScalef(250.0f, 200.0f, 250.0f);
    glColor4f(1, 1, 1, 1);
    for (size_t i = 0; i < skyboxMeshes.size(); ++i) {
        glBindTexture(GL_TEXTURE_2D, skyboxTextures[i]);
        skyboxMeshes[i].draw();
    }
    glPopMatrix();
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
}

void drawOverlayTexture(GLuint texture, float centerX, float centerY, float width, float height,
                        float alpha = 1.0f) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glColor4f(1.0f, 1.0f, 1.0f, alpha);
    const float left = centerX - width * 0.5f;
    const float right = centerX + width * 0.5f;
    const float top = centerY - height * 0.5f;
    const float bottom = centerY + height * 0.5f;
    glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex2f(left, top);
    glTexCoord2f(1, 1); glVertex2f(right, top);
    glTexCoord2f(1, 0); glVertex2f(right, bottom);
    glTexCoord2f(0, 0); glVertex2f(left, bottom);
    glEnd();
}

void drawOverlayTextureRotated(GLuint texture, float centerX, float centerY, float width, float height,
                               float radians, float alpha = 1.0f) {
    glPushMatrix();
    glTranslatef(centerX, centerY, 0);
    glRotatef(radians * 57.2957795f, 0, 0, 1);
    drawOverlayTexture(texture, 0, 0, width, height, alpha);
    glPopMatrix();
}

void drawOverlayText(float x, float y, float red, float green, float blue, const std::string& text) {
    glDisable(GL_TEXTURE_2D);
    glColor4f(red, green, blue, 1.0f);
    glRasterPos2f(x, y);
    glListBase(fontDisplayLists);
    glCallLists(static_cast<GLsizei>(text.size()), GL_UNSIGNED_BYTE, text.data());
    glEnable(GL_TEXTURE_2D);
}

std::string trainingMessage(float elapsed) {
    struct TimedMessage { float time; const char* text; };
    static constexpr TimedMessage messages[] = {
        {3, "Welcome to Air Guard! This is a training mission"},
        {8, "Use your right hand to operate the mouse."},
        {13, "Leave your left hand on the keyboard."},
        {18, "You can change the view by using the arrows."},
        {23, "Ctrl,Shift is used to control the speed of the plane"},
        {33, "At the right corner are a revmeter"},
        {38, "and a number showing how many bombs you have"},
        {43, "Right now, you have only five bombs."},
        {48, "Drop bombs by right-clicking on the mouse"},
        {53, "The machine gun is operated by left-clicking"},
        {58, "If you've forgotten your goal, press the ESC key"},
        {63, "Here's the order: Fly back to base!"},
        {68, "The base is indicated in green on the radar"},
        {73, "Back to base! This is an order!"},
        {110, "Go Go Go!"}
    };
    for (const auto& message : messages)
        if (elapsed >= message.time && elapsed < message.time + 3.0f) return message.text;
    return {};
}

std::string missionMessage(float elapsed) {
    const auto active = [elapsed](float start, float duration) {
        return start >= 0.0f && elapsed >= start && elapsed < start + duration;
    };
    const auto activeAfter = [elapsed](float base, float offset, float duration) {
        return base >= 0.0f && elapsed >= base + offset && elapsed < base + offset + duration;
    };
    if (!missionCompleted && !missionFailed &&
        (std::abs(aircraftPosition.x) > 10000.0f ||
         std::abs(aircraftPosition.y) > 10000.0f ||
         std::abs(aircraftPosition.z) > 10000.0f))
        return "YOU MUSS FLY BACK INTO BATTLE";
    if (selectedMission == 1) return trainingMessage(elapsed);
    if (selectedMission == 2) {
        if (active(12.0f, 4.0f)) return "Incoming enemy fighters! Destroy them all!";
        if (active(missionObjectiveTime, 7.0f)) return "Good work! Fly back to the airbase.";
    } else if (selectedMission == 4) {
        if (active(missionObjectiveTime, 8.0f)) return "Fly back to base! You have company.";
        if (elapsed >= 8.0f && elapsed < 24.0f) return "You have spying equipment instead of weapons.";
        if (active(3.0f, 4.0f)) return "Keep close to the ground!";
    } else if (selectedMission == 5) {
        if (activeAfter(missionObjectiveTime, 16.0f, 5.0f)) return "Hold your position and destroy them!";
        if (activeAfter(missionObjectiveTime, 8.0f, 5.0f)) return "Careful! Incoming squadron of enemy fighters!";
        if (active(missionObjectiveTime, 6.0f)) return "Good work";
    } else if (selectedMission == 6) {
        if (active(150.0f, 6.0f)) return "You have only 60 seconds left!";
    } else if (selectedMission == 7) {
        if (active(150.0f, 6.0f)) return "Move your arse! Time is tight!";
    } else if (selectedMission == 8) {
        if (active(20.0f, 6.0f)) return "Concentrate on the bombers!";
        if (active(27.0f, 6.0f)) return "They won't leave the island!";
    } else if (selectedMission == 15) {
        if (activeAfter(missionSecondaryTime, 6.0f, 5.0f)) return "Fly south to go home.";
        if (active(missionSecondaryTime, 5.0f)) return "There's our freedom!";
        if (active(missionObjectiveTime, 5.0f)) return "Unexpected enemy squadron! Eliminate the fighters!";
    }
    return {};
}

void updateLensProjection(const Vector3& camera) {
    GLfloat model[16]{};
    GLfloat projection[16]{};
    glGetFloatv(GL_MODELVIEW_MATRIX, model);
    glGetFloatv(GL_PROJECTION_MATRIX, projection);
    const GLfloat world[4] = {camera.x + 1000.0f, camera.y + 575.0f, camera.z + 1000.0f, 1.0f};
    GLfloat eye[4]{};
    GLfloat clip[4]{};
    for (int row = 0; row < 4; ++row)
        eye[row] = model[row] * world[0] + model[4 + row] * world[1] +
                   model[8 + row] * world[2] + model[12 + row] * world[3];
    for (int row = 0; row < 4; ++row)
        clip[row] = projection[row] * eye[0] + projection[4 + row] * eye[1] +
                    projection[8 + row] * eye[2] + projection[12 + row] * eye[3];
    lensVisible = clip[3] > 0.0f;
    if (!lensVisible) return;
    lensX = (clip[0] / clip[3] + 1.0f) * 0.5f * logicalWidth;
    lensY = (1.0f - clip[1] / clip[3]) * 0.5f * logicalHeight;
    lensVisible = lensX >= 0.0f && lensX <= logicalWidth && lensY >= 0.0f && lensY <= logicalHeight;
}

void drawLensFlares() {
    if (!lensVisible) return;
    static constexpr float factors[7] = {1.0f, 0.5f, 0.33333334f, 0.125f, -0.5f, -0.25f, -0.18181819f};
    static constexpr float scales[7] = {2.0f, 0.5f, 0.25f, 0.7f, 0.5f, 0.45f, 0.35f};
    static constexpr size_t frames[7] = {0, 1, 0, 2, 3, 2, 1};
    const float dx = lensX - 400.0f;
    const float dy = lensY - 300.0f;
    glBlendFunc(GL_ONE, GL_ONE);
    for (size_t i = 0; i < 7; ++i)
        drawOverlayTexture(lensTextures[frames[i]], 400.0f + dx * factors[i],
                           300.0f + dy * factors[i], 128.0f * scales[i], 128.0f * scales[i], 0.7f);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void drawFlightOverlay() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, logicalWidth, logicalHeight, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);

    drawLensFlares();

    const int pilotFrame = playerHealth > 60 ? (pointerDown ? 1 : 0) :
                           (playerHealth > 30 ? (pointerDown ? 3 : 2) :
                            (playerHealth > 0 ? (pointerDown ? 5 : 4) : 6));
    drawOverlayTexture(pilotTextures[static_cast<size_t>(pilotFrame)], 610.0f, 32.0f, 72.0f, 72.0f);
    drawOverlayTexture(bevelTexture, 722.0f, 32.0f, 148.0f, 81.0f);
    drawOverlayTexture(throttleTexture, 720.0f, 540.0f, 96.0f, 96.0f, 0.8f);
    const float throttleStep = 2.0f + (aircraftSpeed - 70.0f) * (10.0f / 180.0f);
    const float gaugeAngle = ((12.0f - throttleStep) + 2.0f) * 0.2857143f - 1.4f;
    drawOverlayTextureRotated(gaugeTexture, 720.0f, 553.0f, 26.0f, 78.0f, gaugeAngle, 0.8f);
    drawOverlayTexture(bombsTexture, 620.0f, 540.0f, 96.0f, 96.0f, 0.8f);
    if (compassVisible)
        drawOverlayTexture(compassTexture, 400.0f, 600.0f, 128.0f, 128.0f);
    if (mapVisible) {
        constexpr float radarX = 160.0f;
        constexpr float radarY = 520.0f;
        drawOverlayTexture(mapTexture, radarX, radarY, 147.0f, 147.0f, 0.9f);
        const auto radarPoint = [&](const Vector3& world, GLuint texture) {
            const float dx = world.x - aircraftPosition.x;
            const float dz = world.z - aircraftPosition.z;
            const float localX = dx * std::cos(aircraftYaw) - dz * std::sin(aircraftYaw);
            const float localZ = dx * std::sin(aircraftYaw) + dz * std::cos(aircraftYaw);
            const float x = radarX + std::clamp(localX * 0.025f, -64.0f, 64.0f);
            const float y = radarY - std::clamp(localZ * 0.025f, -64.0f, 64.0f);
            drawOverlayTexture(texture, x, y, 15.0f, 15.0f);
        };
        radarPoint({-6500, 0, 4500}, fragmentTextures[5]);
        for (const auto& entity : missionAircraft) {
            if (!entity.active) continue;
            const size_t frame = entity.kind == AircraftKind::Messerschmitt ? 0u :
                                 (entity.kind == AircraftKind::Volker ? 2u : 4u);
            radarPoint(entity.position, fragmentTextures[frame]);
        }
        for (const auto& entity : missionTrucks)
            if (entity.active) radarPoint(entity.position, fragmentTextures[2]);
        for (const auto& entity : missionGround)
            if (entity.objective && entity.active && entity.health < 30000)
                radarPoint(entity.position, fragmentTextures[4]);
    }

    const float elapsed = flightElapsedTime;
    const int seconds = static_cast<int>(elapsed);
    const std::string time = std::to_string(seconds / 60) + (seconds % 60 < 10 ? ":0" : ":") +
                             std::to_string(seconds % 60);
    drawOverlayText(665.0f, 18.0f, 1, 1, 1, "Score:");
    drawOverlayText(720.0f, 18.0f, 0.28f, 0.29f, 0.25f, std::to_string(score));
    drawOverlayText(665.0f, 40.0f, 1, 1, 1, "Time:");
    drawOverlayText(720.0f, 40.0f, 0.28f, 0.29f, 0.25f, time);
    drawOverlayText(612.0f, 550.0f, 1, 1, 1, std::to_string(bombs));
    if (fpsVisible)
        drawOverlayText(15.0f, 24.0f, 1, 1, 1, "FPS: " + std::to_string(static_cast<int>(displayedFps)));

    const std::string message = missionMessage(elapsed);
    if (!message.empty()) {
        drawOverlayTexture(messageBackTexture, 400.0f, 470.0f, 512.0f, 64.0f, 0.9f);
        drawOverlayText(185.0f, 470.0f, 1, 1, 1, "Message:");
        drawOverlayText(260.0f, 470.0f, 1, 1, 1, message);
    }
    if (selectedMission == 0 && (missionCompleted || missionFailed)) {
        const std::string scoreMessage = configuration.language == 1 ? "TVOJE SCORE: " : "YOUR SCORE: ";
        drawOverlayText(315.0f, 285.0f, 1.0f, 1.0f, 0.0f, scoreMessage + std::to_string(score));
        if (configuration.language == 0)
            drawOverlayText(170.0f, 325.0f, 1.0f, 1.0f, 0.0f,
                            "You can upload score on internet from start menu or game directory");
    } else {
        if (missionCompleted)
            drawOverlayText(285.0f, 300.0f, 0.0f, 1.0f, 0.0f, "MISSION COMPLETED");
        if (missionFailed)
            drawOverlayText(315.0f, 300.0f, 1.0f, 0.0f, 0.0f, "MISSION FAILED");
    }

    if (missionCompleted || missionFailed) {
        const float fade = std::clamp((elapsed - missionResultTime) / 1.5f, 0.0f, 1.0f);
        glDisable(GL_TEXTURE_2D);
        glColor4f(0.0f, 0.0f, 0.0f, fade);
        glBegin(GL_QUADS);
        glVertex2f(0.0f, 0.0f); glVertex2f(static_cast<float>(logicalWidth), 0.0f);
        glVertex2f(static_cast<float>(logicalWidth), static_cast<float>(logicalHeight));
        glVertex2f(0.0f, static_cast<float>(logicalHeight));
        glEnd();
        glEnable(GL_TEXTURE_2D);
        glColor4f(1, 1, 1, 1);
    }

    if (flightPaused) {
        const size_t briefingIndex = static_cast<size_t>(std::clamp(selectedMission, 0, 15));
        drawOverlayTexture(briefingTexturesGL[briefingIndex], 400.0f, 300.0f, 320.0f, 272.0f);
        static constexpr float buttonX[3] = {290.0f, 400.0f, 510.0f};
        for (size_t i = 0; i < briefingButtonTexturesGL.size(); ++i) {
            const bool hovered = hit(buttonX[i], 460.0f, 98.0f, 36.0f);
            drawOverlayTexture(briefingButtonTexturesGL[i], buttonX[i], 460.0f, 98.0f, 36.0f,
                               hovered ? 1.0f : 0.6f);
        }
    }

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void renderFlightOpenGL() {
    initializeFlightResources();
    const auto now = std::chrono::steady_clock::now();
    float seconds = std::chrono::duration<float>(now - lastFlightFrame).count();
    lastFlightFrame = now;
    seconds = std::min(seconds, 0.05f);
    if (seconds > 0.0f) displayedFps = displayedFps * 0.9f + (1.0f / seconds) * 0.1f;
    if (!flightPaused) updateAircraft(seconds);

    wglMakeCurrent(openGLDevice, openGLContext);
    glViewport(0, 0, outputWidth, outputHeight);
    glClearColor(0.46f, 0.62f, 0.76f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    perspective(60.0f, static_cast<float>(outputWidth) / static_cast<float>(outputHeight),
                5.0f, static_cast<float>(configuration.drawDistance));
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    const float horizontal = std::cos(aircraftPitch);
    Vector3 forward{std::sin(aircraftYaw) * horizontal,
                    std::sin(aircraftPitch),
                    std::cos(aircraftYaw) * horizontal};
    Vector3 camera{};
    float viewYaw = aircraftYaw;
    float viewPitch = aircraftPitch;
    if (cameraMode == 1) {
        camera = {aircraftPosition.x + forward.x * 8.0f,
                  aircraftPosition.y + forward.y * 8.0f + 8.0f,
                  aircraftPosition.z + forward.z * 8.0f};
    } else if (cameraMode == 2) {
        camera = {aircraftPosition.x + forward.x * 230.0f,
                  aircraftPosition.y + forward.y * 230.0f + 45.0f,
                  aircraftPosition.z + forward.z * 230.0f};
        viewYaw += 3.1415926535f;
        viewPitch = -aircraftPitch;
    } else if (cameraMode == 3) {
        camera = {aircraftPosition.x + std::cos(aircraftYaw) * 210.0f,
                  aircraftPosition.y + 40.0f,
                  aircraftPosition.z - std::sin(aircraftYaw) * 210.0f};
        viewYaw -= 1.5707963267f;
        viewPitch = 0.0f;
    } else {
        camera = {aircraftPosition.x - forward.x * 230.0f,
                  aircraftPosition.y - forward.y * 230.0f + 75.0f,
                  aircraftPosition.z - forward.z * 230.0f};
    }
    glRotatef(-viewPitch * 57.2957795f, 1, 0, 0);
    glRotatef(-viewYaw * 57.2957795f, 0, 1, 0);
    glTranslatef(-camera.x, -camera.y, -camera.z);

    drawSkybox(camera);
    drawTerrain();
    drawPlayerShadow();
    drawMissionGround();
    drawMissionAircraft();

    if (cameraMode != 1) {
        glBindTexture(GL_TEXTURE_2D, playerHealth <= 30 ? spitfireDestroyedTexture :
                                     (playerHealth <= 60 ? spitfireDamagedTexture : spitfireTexture));
        glPushMatrix();
        glTranslatef(aircraftPosition.x, aircraftPosition.y, aircraftPosition.z);
        glRotatef(aircraftYaw * 57.2957795f, 0, 1, 0);
        glRotatef(-aircraftPitch * 57.2957795f, 1, 0, 0);
        glRotatef(-aircraftRoll * 57.2957795f, 0, 0, 1);
        glScalef(1.1f, 1.1f, 1.1f);
        spitfireMesh.draw();
        if (configuration.specularEffects) {
            glDisable(GL_LIGHTING);
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            glColor4f(0.18f, 0.18f, 0.18f, 1.0f);
            glBindTexture(GL_TEXTURE_2D, spitfireEnvironmentTexture);
            spitfireMesh.draw();
            glColor4f(1, 1, 1, 1);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_BLEND);
            glEnable(GL_LIGHTING);
        }
        glBindTexture(GL_TEXTURE_2D, propellerTexture);
        glRotatef(static_cast<float>(GetTickCount64() % 360), 0, 0, 1);
        propellerMesh.draw();
        glPopMatrix();
    } else {
        glBindTexture(GL_TEXTURE_2D, cockpitTexture);
        glPushMatrix();
        glTranslatef(aircraftPosition.x, aircraftPosition.y, aircraftPosition.z);
        glRotatef(aircraftYaw * 57.2957795f, 0, 1, 0);
        glRotatef(-aircraftPitch * 57.2957795f, 1, 0, 0);
        glRotatef(-aircraftRoll * 57.2957795f, 0, 0, 1);
        glScalef(1.1f, 1.1f, 1.1f);
        cockpitMesh.draw();
        glPopMatrix();
    }

    drawFlightEffects();

    updateLensProjection(camera);

    drawFlightOverlay();

    SwapBuffers(openGLDevice);
    const float elapsed = flightElapsedTime;
    if ((missionCompleted || missionFailed) && elapsed - missionResultTime >= 1.5f) {
        if (missionFailed && selectedMission > 0) {
            changeScreen(Screen::Flight);
        } else if (selectedMission == 0) {
            mainMenuPhaseOffset = 1.0f;
            changeScreen(Screen::MainMenu);
        } else {
            submenuLeaving = false;
            changeScreen(Screen::Campaign);
        }
    }
}

void render(HDC destination) {
    Bitmap frame(logicalWidth, logicalHeight, PixelFormat32bppPARGB);
    Graphics graphics(&frame);
    graphics.SetInterpolationMode(InterpolationModeNearestNeighbor);
    graphics.SetPixelOffsetMode(PixelOffsetModeHalf);
    switch (screen) {
    case Screen::Loading: renderLoading(graphics); break;
    case Screen::Intro3DGames: renderIntro3D(graphics); break;
    case Screen::IntroBecher: renderIntroBecher(graphics); break;
    case Screen::MainMenu: renderMainMenu(graphics); break;
    case Screen::Campaign: renderCampaign(graphics); break;
    case Screen::Credits: renderCredits(graphics); break;
    case Screen::Briefing: renderBriefing(graphics); break;
    case Screen::Flight: break;
    }
    Graphics output(destination);
    output.SetInterpolationMode(InterpolationModeNearestNeighbor);
    output.DrawImage(&frame, Rect(0, 0, outputWidth, outputHeight), 0, 0,
                     logicalWidth, logicalHeight, UnitPixel);
    pointerPressed = false;
}

LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE:
        pointerPosition.x = LOWORD(lParam) * logicalWidth / outputWidth;
        pointerPosition.y = HIWORD(lParam) * logicalHeight / outputHeight;
        return 0;
    case WM_LBUTTONDOWN:
        if (screen == Screen::Flight && flightPaused) {
            if (hit(290.0f, 460.0f, 98.0f, 36.0f)) {
                flightPaused = false;
                lastFlightFrame = std::chrono::steady_clock::now();
            } else if (hit(400.0f, 460.0f, 98.0f, 36.0f)) {
                changeScreen(Screen::Flight);
            } else if (hit(510.0f, 460.0f, 98.0f, 36.0f)) {
                mainMenuPhaseOffset = 1.0f;
                changeScreen(Screen::MainMenu);
            }
            return 0;
        }
        pointerDown = true;
        pointerPressed = true;
        if (screen == Screen::Flight && configuration.soundEnabled)
            playOneShot("gunfire", path("Sounds\\Fire.wav"));
        SetCapture(window);
        return 0;
    case WM_RBUTTONDOWN:
        dropPlayerBomb();
        return 0;
    case WM_LBUTTONUP:
        pointerDown = false;
        ReleaseCapture();
        return 0;
    case WM_KEYDOWN:
        if (wParam < keys.size()) keys[static_cast<size_t>(wParam)] = true;
        if (screen == Screen::Flight && (lParam & (1u << 30)) == 0) {
            if (wParam == 'M') mapVisible = !mapVisible;
            else if (wParam == 'K') compassVisible = !compassVisible;
            else if (wParam == 'F') fpsVisible = !fpsVisible;
            else if (wParam == 'C') cameraMode = (cameraMode + 1) % 2;
            else if (wParam == VK_UP) cameraMode = 0;
            else if (wParam == VK_DOWN) cameraMode = 2;
            else if (wParam == VK_LEFT || wParam == VK_RIGHT) cameraMode = 3;
        }
        if (wParam == VK_ESCAPE && screen != Screen::Loading) {
            if (screen == Screen::Flight) {
                flightPaused = !flightPaused;
                pointerDown = false;
                lastFlightFrame = std::chrono::steady_clock::now();
            } else if (screen == Screen::Campaign || screen == Screen::Credits) {
                if (screen == Screen::Campaign && campaignPopup != 0) {
                    campaignPopup = 0;
                } else if (!submenuLeaving) {
                    submenuLeaving = true;
                    submenuDestination = Screen::MainMenu;
                    transitionStarted = std::chrono::steady_clock::now();
                }
            } else if (screen == Screen::MainMenu) {
                if (!mainMenuLeaving) {
                    mainMenuLeaving = true;
                    mainMenuLeaveTarget = 0.0f;
                    mainMenuQuit = true;
                    transitionStarted = std::chrono::steady_clock::now();
                }
            } else if (screen == Screen::Briefing) {
                submenuLeaving = false;
                changeScreen(selectedMission == 0 ? Screen::MainMenu : Screen::Campaign);
            } else {
                mainMenuPhaseOffset = 1.0f;
                changeScreen(Screen::MainMenu);
            }
        }
        return 0;
    case WM_KEYUP:
        if (wParam < keys.size()) keys[static_cast<size_t>(wParam)] = false;
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        if (screen != Screen::Flight) render(dc);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_TIMER:
        if (screen == Screen::Flight) renderFlightOpenGL();
        else {
            if (screen == Screen::Loading) {
                if (loadingStage == 5) initializeFlightResources();
                if (loadingStage < 8) ++loadingStage;
                else changeScreen(Screen::Intro3DGames);
            }
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    case WM_DESTROY:
        KillTimer(window, 1);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(window, message, wParam, lParam);
}

} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int) {
    applicationInstance = instance;
    char executable[MAX_PATH]{};
    GetModuleFileNameA(nullptr, executable, MAX_PATH);
    char* slash = strrchr(executable, '\\');
    if (slash) *slash = 0;
    rootDirectory = executable;
    SetCurrentDirectoryA(rootDirectory.c_str());
    startLog();
    loadConfiguration();

    DEVMODEA displayMode{};
    displayMode.dmSize = sizeof(displayMode);
    displayMode.dmPelsWidth = static_cast<DWORD>(outputWidth);
    displayMode.dmPelsHeight = static_cast<DWORD>(outputHeight);
    displayMode.dmBitsPerPel = 32;
    displayMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
    if (ChangeDisplaySettingsA(&displayMode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL) {
        gameLog << "Depht - Stencil nieje kompatibliny s modom obrazovky\n";
        MessageBoxA(nullptr, "Your graphic card is not compatible with Direct3D 8.1",
                    "Air Guard", MB_OK | MB_ICONERROR);
        return 1;
    }

    GdiplusStartupInput gdiplusInput;
    if (GdiplusStartup(&gdiplusToken, &gdiplusInput, nullptr) != Ok) {
        ChangeDisplaySettingsA(nullptr, 0);
        return 1;
    }
    assets = std::make_unique<Assets>();
    std::ifstream progress(path("Menu\\array.dat"));
    progress >> unlockedMission;
    unlockedMission = std::clamp(unlockedMission, 1, 16);

    WNDCLASSEXA klass{};
    klass.cbSize = sizeof(klass);
    klass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    klass.lpfnWndProc = windowProcedure;
    klass.hInstance = instance;
    klass.hCursor = nullptr;
    klass.lpszClassName = "PLE1 - Air Guard";
    RegisterClassExA(&klass);

    applicationWindow = CreateWindowExA(0, klass.lpszClassName, "PLE1 - Air Guard",
                                         WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                                         0, 0, outputWidth, outputHeight,
                                         nullptr, nullptr, instance, nullptr);
    if (!applicationWindow) {
        assets.reset();
        GdiplusShutdown(gdiplusToken);
        ChangeDisplaySettingsA(nullptr, 0);
        return 1;
    }
    openGLDevice = GetDC(applicationWindow);
    PIXELFORMATDESCRIPTOR pixelFormat{};
    pixelFormat.nSize = sizeof(pixelFormat);
    pixelFormat.nVersion = 1;
    pixelFormat.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pixelFormat.iPixelType = PFD_TYPE_RGBA;
    pixelFormat.cColorBits = 32;
    pixelFormat.cDepthBits = 24;
    pixelFormat.iLayerType = PFD_MAIN_PLANE;
    const int selectedPixelFormat = ChoosePixelFormat(openGLDevice, &pixelFormat);
    if (selectedPixelFormat && SetPixelFormat(openGLDevice, selectedPixelFormat, &pixelFormat))
        openGLContext = wglCreateContext(openGLDevice);
    if (!openGLContext || !wglMakeCurrent(openGLDevice, openGLContext)) {
        gameLog << "chyba pri inicializacii g_pd3dDevice\n";
        MessageBoxA(applicationWindow, "Your graphic card is not compatible with Direct3D 8.1",
                    "Air Guard", MB_OK | MB_ICONERROR);
        if (openGLContext) wglDeleteContext(openGLContext);
        if (openGLDevice) ReleaseDC(applicationWindow, openGLDevice);
        DestroyWindow(applicationWindow);
        assets.reset();
        GdiplusShutdown(gdiplusToken);
        ChangeDisplaySettingsA(nullptr, 0);
        return 1;
    }
    gameLog << "inicializacia g_pd3dDevice  uspesna\n";
    ShowWindow(applicationWindow, SW_SHOW);
    UpdateWindow(applicationWindow);
    SetTimer(applicationWindow, 1, 16, nullptr);
    screenStarted = std::chrono::steady_clock::now();
    if (configuration.musicEnabled) playLoopingAudio("music", path("Sounds\\menumusic.mp3"));

    MSG message{};
    while (GetMessageA(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageA(&message);
    }
    updateScoreUpload();
    if (openGLContext) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(openGLContext);
    }
    if (openGLDevice) ReleaseDC(applicationWindow, openGLDevice);
    closeAudioAlias("gunfire");
    closeAudioAlias("explosion");
    closeAudioAlias("enemyengine");
    closeAudioAlias("engine");
    closeAudioAlias("music");
    assets.reset();
    GdiplusShutdown(gdiplusToken);
    ChangeDisplaySettingsA(nullptr, 0);
    return static_cast<int>(message.wParam);
}
