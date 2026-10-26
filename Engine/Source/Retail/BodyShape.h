#ifndef RETAIL_BODY_SHAPE_H
#define RETAIL_BODY_SHAPE_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "Engine/Source/Model2/M2Model.h"
#include "Engine/Source/Retail/BodyShapeFace.h"
#include "Engine/Source/Retail/BodyShapeWire.h"
#include "Engine/Source/Retail/BodyShapeMesh.h"
#include "Engine/Source/Tempest/C44Matrix.h"

// Local-only body sliders for Retail humanoid skeletons. Each slider drives bone transforms through the
// model's procedural-transform slot; nothing is stored in the mesh. Key-bone ids are shared by male and
// female skeletons, so one table serves both. The probe reaches any raw bone index (face bones have no key).
namespace BodyShape {

constexpr uint32_t ProcedurallyDriven = 0x80;
constexpr uint32_t BindAxisScale = 0x100; // see CM2Model bone update: scale before the animation, not after

enum Kind { ScaleX = 1, ScaleY = 2, ScaleZ = 4, ScaleXY = 3, ScaleXYZ = 7 };

// One contribution to a bone: scale by (1 + amount * value) on the axes in `scale`, and/or move by
// amount * value along an axis. `mirror` negates the translation's Y so a left/right pair spreads apart.
struct Op {
    uint32_t key;
    uint32_t scale;
    float scaleAmount;
    float moveX, moveY, moveZ;
    bool mirrorY;
};

// A face-region contribution: scale the region's bones about its centre (or each side's centre) and/or move
// them. The move's Y is mirrored per side, so a positive Y move spreads a left/right pair apart.
struct FaceEffect {
    const char* region;
    uint32_t scale;
    float scaleAmount;
    float moveX, moveY, moveZ;
};

enum Group { BodyGroup = 0, FaceGroup = 1 };

struct Slider {
    const char* name;
    std::vector<Op> ops;
    std::vector<FaceEffect> face = {};
    Group group = BodyGroup;
    float minValue = -1.0f;
};

inline const std::vector<Slider>& Sliders() {
    static const std::vector<Slider> table = {
        // Mesh sliders: no bones, they reshape the audited female mesh itself (see ApplyMesh). Females only.
        { "BreastSize", {}, {}, BodyGroup, 0.0f },
        { "ButtSize", {}, {}, BodyGroup, 0.0f },
        { "Height", { { 54, ScaleXYZ, 0.25f, 0, 0, 0, false } } },
        { "HeadSize", { { 6, ScaleXYZ, 0.35f, 0, 0, 0, false } } },
        { "NeckLength", { { 50, 0, 0, 0, 0, 0.06f, false } } },
        { "ShoulderWidth", { { 2, 0, 0, 0, 0.05f, 0, false }, { 3, 0, 0, 0, -0.05f, 0, false } } },
        { "ArmLength", { { 0, ScaleZ, 0.25f, 0, 0, 0, false }, { 1, ScaleZ, 0.25f, 0, 0, 0, false } } },
        { "ArmGirth", { { 0, ScaleXY, 0.3f, 0, 0, 0, false }, { 1, ScaleXY, 0.3f, 0, 0, 0, false } } },
        { "ChestSize", { { 48, ScaleXY, 0.25f, 0, 0, 0, false } } },
        { "WaistWidth", { { 4, ScaleXY, 0.25f, 0, 0, 0, false } } },
        { "TorsoLength", { { 4, ScaleZ, 0.2f, 0, 0, 0, false } } },
        { "HipWidth", { { 51, ScaleXY, 0.25f, 0, 0, 0, false } } },
        // Leg length also lifts the pelvis so the feet stay on the ground (see Apply).
        { "LegLength", { { 55, ScaleZ, 0.3f, 0, 0, 0, false }, { 56, ScaleZ, 0.3f, 0, 0, 0, false },
                         { 70, ScaleZ, 0.3f, 0, 0, 0, false }, { 71, ScaleZ, 0.3f, 0, 0, 0, false },
                         { 57, ScaleZ, 0.3f, 0, 0, 0, false }, { 58, ScaleZ, 0.3f, 0, 0, 0, false } } },
        { "LegGirth", { { 55, ScaleXY, 0.3f, 0, 0, 0, false }, { 56, ScaleXY, 0.3f, 0, 0, 0, false },
                        { 70, ScaleXY, 0.3f, 0, 0, 0, false }, { 71, ScaleXY, 0.3f, 0, 0, 0, false },
                        { 57, ScaleXY, 0.3f, 0, 0, 0, false }, { 58, ScaleXY, 0.3f, 0, 0, 0, false } } },
        { "JawSize", { { 7, ScaleXYZ, 0.3f, 0, 0, 0, false } }, {}, FaceGroup },
        { "JawLength", { { 7, 0, 0, 0, 0, -0.03f, false } }, {}, FaceGroup },
        { "FaceWidth", { { 6, ScaleY, 0.12f, 0, 0, 0, false } }, {}, FaceGroup },
        { "FaceLength", { { 6, ScaleZ, 0.12f, 0, 0, 0, false } }, {}, FaceGroup },
        // Region sliders: amounts are metres on a human male head and scale with the model.
        { "BrowHeight", {}, { { "Brow", 0, 0, 0, 0, 0.012f } }, FaceGroup },
        { "BrowDepth", {}, { { "Brow", 0, 0, 0.010f, 0, 0 } }, FaceGroup },
        { "EyeSize", {}, { { "Eyes", ScaleXYZ, 0.35f, 0, 0, 0 } }, FaceGroup },
        { "EyeSpacing", {}, { { "Eyes", 0, 0, 0, 0.010f, 0 } }, FaceGroup },
        { "EyeHeight", {}, { { "Eyes", 0, 0, 0, 0, 0.008f } }, FaceGroup },
        { "NoseSize", {}, { { "Nose", ScaleXYZ, 0.4f, 0, 0, 0 } }, FaceGroup },
        { "NoseWidth", {}, { { "Nose", ScaleY, 0.5f, 0, 0, 0 } }, FaceGroup },
        { "NoseLength", {}, { { "Nose", 0, 0, 0.012f, 0, 0 } }, FaceGroup },
        { "NoseHeight", {}, { { "Nose", 0, 0, 0, 0, 0.008f } }, FaceGroup },
        { "MouthWidth", {}, { { "Mouth", ScaleY, 0.3f, 0, 0, 0 } }, FaceGroup },
        { "LipFullness", {}, { { "Mouth", ScaleZ, 0.5f, 0, 0, 0 } }, FaceGroup },
        { "MouthHeight", {}, { { "Mouth", 0, 0, 0, 0, 0.010f } }, FaceGroup },
        { "ChinLength", {}, { { "Chin", 0, 0, 0, 0, -0.012f } }, FaceGroup },
        { "ChinDepth", {}, { { "Chin", 0, 0, 0.010f, 0, 0 } }, FaceGroup },
        { "CheekWidth", {}, { { "Cheeks", 0, 0, 0, 0.008f, 0 } }, FaceGroup },
        { "CheekHeight", {}, { { "Cheeks", 0, 0, 0, 0, 0.008f } }, FaceGroup },
        // Human male only: the other skeletons do not carry these bones.
        { "JawWidth", {}, { { "JawAngle", 0, 0, 0, 0.012f, 0 } }, FaceGroup },
        { "EarSize", {}, { { "Ears", ScaleXYZ, 0.35f, 0, 0, 0 } }, FaceGroup },
        { "EarHeight", {}, { { "Ears", 0, 0, 0, 0, 0.010f } }, FaceGroup },
    };
    return table;
}

constexpr uint32_t LegLengthKeys[2] = { 55, 56 };
constexpr uint32_t KneeKeys[2] = { 57, 58 };
constexpr uint32_t PelvisRootKey = 26;
constexpr uint32_t SpineKey = 4;
constexpr uint32_t HeadKey = 6;

struct Probe {
    int32_t index = -1;
    bool mirror = false;
    float scale[3] = { 1, 1, 1 };
    float move[3] = { 0, 0, 0 };
};

// A bone's accumulated contribution: diagonal scale plus a move, which is all any slider needs.
struct Accum {
    float scale[3] = { 1, 1, 1 };
    float move[3] = { 0, 0, 0 };
};

// A face region on the current model: its top-most bones (children ride along with their parent) and the
// centres to scale about, indexed by side + 1 (0 right, 1 centre, 2 left) and for the whole group.
struct RegionBone {
    uint32_t index;
    int side;
    float pivot[3];
};

struct RegionCache {
    std::vector<RegionBone> bones;
    float sideCentre[3][3] = {};
    float allCentre[3] = {};
};

struct State {
    const void* meshProfile = nullptr; // audited breast/rear profile for the current model, if it has one
    bool meshSearched = false;
    std::vector<float> bind; // the model's bind pose, 6 floats per vertex, read once per model
    float meshBreast = 0.0f;
    float meshButt = 0.0f;
    std::map<std::string, RegionCache> regions;
    bool ready = false;
    std::set<uint32_t> applied; // bone indices carrying our flag on the model
    float size = 1.0f; // head height relative to a human male, so fixed-distance moves fit the model
};

constexpr float ReferenceHeadHeight = 1.85f;

// Diagnostics for the creator page: Logs\BodyShape.log, one line per call.
inline void Log(const char* text) {
    if (FILE* file = std::fopen("Logs/BodyShape.log", "a")) {
        std::fprintf(file, "%s\n", text);
        std::fclose(file);
    }
}

// One set of slider values (-1..1, 0 = untouched) plus the bone probe. Anything that shows a body owns one.
struct Values {
    std::map<std::string, float> v;
    Probe probe;

    float Get(const char* name) const {
        auto it = v.find(name);
        return it == v.end() ? 0.0f : it->second;
    }
};

// The values being edited right now: the creator's, or the local player's in the world (never both at once).
// `model` is the model last driven from them, which the support queries describe.
struct Edit {
    Values values;
    CM2Model* model = nullptr;
};

inline Edit& Edited() {
    static Edit edit;
    return edit;
}

inline Values& Local() {
    return Edited().values;
}

// Per-model working state (bone flags we set, face regions, bind pose, mesh profile). Dropped when the model dies.
inline std::map<CM2Model*, State>& Appliers() {
    static std::map<CM2Model*, State> appliers;
    return appliers;
}

inline State& For(CM2Model* model) {
    return Appliers()[model];
}

inline void Forget(CM2Model* model) {
    Appliers().erase(model);
    if (Edited().model == model) {
        Edited().model = nullptr;
    }
}

inline int FindSlider(const char* name) {
    const auto& table = Sliders();
    for (size_t i = 0; i < table.size(); ++i) {
        if (!strcmp(table[i].name, name)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

inline bool Set(const char* name, float value) {
    int found = FindSlider(name);
    if (found < 0) {
        return false;
    }
    Local().v[name] = std::max(Sliders()[found].minValue, std::min(1.0f, value));
    return true;
}

inline bool IsMeshSlider(const char* name) {
    return !strcmp(name, "BreastSize") || !strcmp(name, "ButtSize");
}

inline float Get(const char* name) {
    return Local().Get(name);
}

// Resolves each face region's bone-name hashes to this model's bone indices.
inline void BuildRegions(CM2Model* model, State& s) {
    s.regions.clear();
    uint32_t count = model->GetBoneCount();
    std::map<uint32_t, uint32_t> byCrc;
    for (uint32_t i = 0; i < count; ++i) {
        byCrc[model->GetBoneNameCrcByIndex(i)] = i;
    }

    int32_t key, parent;
    C3Vector head(0.0f, 0.0f, 0.0f);
    uint32_t headIndex = model->FindBoneIndexByKey(HeadKey);
    if (headIndex != 0xFFFFFFFF) {
        model->GetBoneInfoByIndex(headIndex, key, parent, head);
    }

    for (const FaceRegionDef& def : FaceRegions) {
        std::set<uint32_t> members;
        for (uint32_t c = 0; c < def.count; ++c) {
            auto it = byCrc.find(def.crcs[c]);
            if (it != byCrc.end()) {
                members.insert(it->second);
            }
        }

        RegionCache& cache = s.regions[def.name];
        int sideCount[3] = {};
        for (uint32_t index : members) {
            C3Vector pivot;
            model->GetBoneInfoByIndex(index, key, parent, pivot);
            if (parent >= 0 && members.count(static_cast<uint32_t>(parent))) {
                continue; // a parent in the same region carries this bone along
            }
            float offset = pivot.y - head.y;
            int side = offset > 0.004f * s.size ? 1 : (offset < -0.004f * s.size ? -1 : 0);
            float p[3] = { pivot.x, pivot.y, pivot.z };
            cache.bones.push_back({ index, side, { p[0], p[1], p[2] } });
            ++sideCount[side + 1];
            for (int a = 0; a < 3; ++a) {
                cache.sideCentre[side + 1][a] += p[a];
                cache.allCentre[a] += p[a];
            }
        }
        for (int side = 0; side < 3; ++side) {
            for (int a = 0; a < 3; ++a) {
                if (sideCount[side]) {
                    cache.sideCentre[side][a] /= sideCount[side];
                }
            }
        }
        for (int a = 0; a < 3 && !cache.bones.empty(); ++a) {
            cache.allCentre[a] /= cache.bones.size();
        }
    }
}

// True when the current model has every bone the slider needs. Nothing is known until a model has loaded.
inline int Supported(const char* name) {
    CM2Model* model = Edited().model;
    int found = FindSlider(name);
    if (found < 0 || !model || !model->IsLoaded()) {
        return -1;
    }
    if (IsMeshSlider(name)) {
        return For(model).meshProfile ? 1 : 0;
    }
    for (const Op& op : Sliders()[found].ops) {
        if (model->FindBoneIndexByKey(op.key) == 0xFFFFFFFF) {
            return 0;
        }
    }
    for (const FaceEffect& effect : Sliders()[found].face) {
        auto it = For(model).regions.find(effect.region);
        if (it == For(model).regions.end() || it->second.bones.empty()) {
            return 0;
        }
    }
    if (!strcmp(name, "LegLength") && model->FindBoneIndexByKey(PelvisRootKey) == 0xFFFFFFFF) {
        return 0;
    }
    return 1;
}

// Random values for every slider the current model supports; the rest are cleared.
inline void Randomize() {
    for (const Slider& slider : Sliders()) {
        if (Supported(slider.name) == 0) {
            Local().v.erase(slider.name);
            continue;
        }
        float lo = std::max(slider.minValue, -0.6f);
        Set(slider.name, lo + (0.6f - lo) * (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)));
    }
}

inline void SetProbe(int32_t index, bool mirror, const float scale[3], const float move[3]) {
    Probe& p = Local().probe;
    p.index = index;
    p.mirror = mirror;
    for (int i = 0; i < 3; ++i) {
        p.scale[i] = scale[i];
        p.move[i] = move[i];
    }
}

inline void Reset() {
    Values& values = Local();
    values.v.clear();
    values.probe = Probe();
}

// Mirror partner of a bone: same parent, pivot reflected across Y. -1 when the bone has none.
inline int32_t MirrorOf(CM2Model* model, uint32_t index) {
    int32_t key, parent;
    C3Vector pivot;
    if (!model->GetBoneInfoByIndex(index, key, parent, pivot) || fabsf(pivot.y) < 0.002f) {
        return -1;
    }
    int32_t best = -1;
    float bestDistance = 0.004f;
    for (uint32_t i = 0; i < model->GetBoneCount(); ++i) {
        int32_t k, p;
        C3Vector q;
        if (i == index || !model->GetBoneInfoByIndex(i, k, p, q)) {
            continue;
        }
        float d = fabsf(q.x - pivot.x) + fabsf(q.y + pivot.y) + fabsf(q.z - pivot.z);
        if (d < bestDistance) {
            bestDistance = d;
            best = static_cast<int32_t>(i);
        }
    }
    return best;
}

inline void Fold(std::map<uint32_t, Accum>& out, uint32_t index, uint32_t axes, float factor, float mx, float my, float mz) {
    Accum& a = out[index];
    for (int i = 0; i < 3; ++i) {
        if (axes & (1u << i)) {
            a.scale[i] *= factor;
        }
    }
    a.move[0] += mx;
    a.move[1] += my;
    a.move[2] += mz;
}

inline bool Neutral(const Accum& a) {
    for (int i = 0; i < 3; ++i) {
        if (fabsf(a.scale[i] - 1.0f) > 1e-5f || fabsf(a.move[i]) > 1e-6f) {
            return false;
        }
    }
    return true;
}

inline C44Matrix ToMatrix(const Accum& a) {
    C44Matrix m;
    m.Identity();
    m.a0 = a.scale[0];
    m.b1 = a.scale[1];
    m.c2 = a.scale[2];
    m.d0 = a.move[0];
    m.d1 = a.move[1];
    m.d2 = a.move[2];
    return m;
}

// Reshapes the mesh (breast / rear) when those sliders change. Finds the audited profile on first use.
inline void ApplyMesh(CM2Model* model, State& s, const Values& vals) {
    if (!s.meshSearched) {
        s.meshSearched = true;
        uint32_t count = model->GetBindVertexCount();
        s.bind.resize(size_t(count) * 6);
        for (uint32_t i = 0; i < count; ++i) {
            C3Vector p, n;
            model->GetBindVertex(i, p, n);
            float* v = &s.bind[size_t(i) * 6];
            v[0] = p.x; v[1] = p.y; v[2] = p.z;
            v[3] = n.x; v[4] = n.y; v[5] = n.z;
        }
        s.meshProfile = BodyShapeMesh::FindProfile(s.bind.data(), count);
        s.meshBreast = s.meshButt = 0.0f;
    }

    float breast = std::max(0.0f, vals.Get("BreastSize"));
    float butt = std::max(0.0f, vals.Get("ButtSize"));
    if (!s.meshProfile || (breast == s.meshBreast && butt == s.meshButt)) {
        return;
    }

    s.meshBreast = breast;
    s.meshButt = butt;
    uint32_t count = static_cast<uint32_t>(s.bind.size() / 6);
    if (breast == 0.0f && butt == 0.0f) {
        model->SetBodyShapeVertices(nullptr);
        return;
    }
    float* shaped = new float[s.bind.size()];
    BodyShapeMesh::Apply(s.meshProfile, s.bind.data(), count, breast, butt, shaped);
    model->SetBodyShapeVertices(shaped);
}

// Drives one model from a set of values; call once a frame, after the engine's own spine/head twist has run.
// twistSet/twistBase[0..1] are what that code wrote to the spine (key 4) and head (key 6) this frame.
inline void Apply(CM2Model* model, const Values& vals, const bool twistSet[2], const C44Matrix twistBase[2]) {
    if (!model || !model->IsLoaded()) {
        return;
    }

    State& s = For(model);

    if (!s.ready) {
        s.ready = true;
        s.size = 1.0f;
        int32_t key, parent;
        C3Vector head;
        uint32_t headIndex = model->FindBoneIndexByKey(HeadKey);
        if (headIndex != 0xFFFFFFFF && model->GetBoneInfoByIndex(headIndex, key, parent, head) && head.z > 0.1f) {
            s.size = head.z / ReferenceHeadHeight;
        }
        BuildRegions(model, s);
    }

    std::map<uint32_t, Accum> wanted;

    for (const Slider& slider : Sliders()) {
        float v = vals.Get(slider.name);
        if (v == 0.0f) {
            continue;
        }
        for (const Op& op : slider.ops) {
            uint32_t index = model->FindBoneIndexByKey(op.key);
            if (index == 0xFFFFFFFF) {
                continue;
            }
            float factor = 1.0f + op.scaleAmount * v;
            Fold(wanted, index, op.scale, factor, op.moveX * v * s.size, op.moveY * v * s.size, op.moveZ * v * s.size);
        }
    }

    for (const Slider& slider : Sliders()) {
        float v = vals.Get(slider.name);
        if (v == 0.0f) {
            continue;
        }
        for (const FaceEffect& effect : slider.face) {
            auto region = s.regions.find(effect.region);
            if (region == s.regions.end()) {
                continue;
            }
            bool perSide = false;
            for (const FaceRegionDef& def : FaceRegions) {
                if (!strcmp(def.name, effect.region)) {
                    perSide = def.perSide;
                }
            }
            float factor = 1.0f + effect.scaleAmount * v;
            for (const RegionBone& bone : region->second.bones) {
                const float* centre = perSide ? region->second.sideCentre[bone.side + 1] : region->second.allCentre;
                float move[3] = { effect.moveX * v * s.size, effect.moveY * v * s.size * bone.side, effect.moveZ * v * s.size };
                for (int a = 0; a < 3; ++a) {
                    if (effect.scale & (1u << a)) {
                        move[a] += (1.0f - factor) * (centre[a] - bone.pivot[a]);
                    }
                }
                Fold(wanted, bone.index, effect.scale, factor, move[0], move[1], move[2]);
            }
        }
    }

    // Longer legs lower the feet, so lift the pelvis by the same amount (hip-to-ankle height * growth).
    float leg = vals.Get("LegLength");
    if (leg != 0.0f) {
        uint32_t hip = model->FindBoneIndexByKey(LegLengthKeys[0]);
        uint32_t knee = model->FindBoneIndexByKey(KneeKeys[0]);
        uint32_t root = model->FindBoneIndexByKey(PelvisRootKey);
        if (hip != 0xFFFFFFFF && knee != 0xFFFFFFFF && root != 0xFFFFFFFF) {
            int32_t k, p;
            C3Vector hipPivot, c;
            float ankleZ = 0.0f;
            bool found = false;
            model->GetBoneInfoByIndex(hip, k, p, hipPivot);
            for (uint32_t i = 0; i < model->GetBoneCount(); ++i) {
                if (model->GetBoneInfoByIndex(i, k, p, c) && p == static_cast<int32_t>(knee) && (!found || c.z < ankleZ)) {
                    ankleZ = c.z;
                    found = true;
                }
            }
            if (found) {
                Fold(wanted, root, 0, 1.0f, 0, 0, (hipPivot.z - ankleZ) * 0.3f * leg);
            }
        }
    }

    const Probe& pr = vals.probe;
    if (pr.index >= 0 && static_cast<uint32_t>(pr.index) < model->GetBoneCount()) {
        auto addProbe = [&](uint32_t index, float side) {
            for (int i = 0; i < 3; ++i) {
                Accum& a = wanted[index];
                a.scale[i] *= pr.scale[i];
                a.move[i] += (i == 1 ? side : 1.0f) * pr.move[i];
            }
        };
        addProbe(static_cast<uint32_t>(pr.index), 1.0f);
        if (pr.mirror) {
            int32_t partner = MirrorOf(model, static_cast<uint32_t>(pr.index));
            if (partner >= 0) {
                addProbe(static_cast<uint32_t>(partner), -1.0f);
            }
        }
    }

    ApplyMesh(model, s, vals);

    const uint32_t twistIndex[2] = { model->FindBoneIndexByKey(SpineKey), model->FindBoneIndexByKey(HeadKey) };

    for (auto& [index, accum] : wanted) {
        if (Neutral(accum)) {
            continue;
        }
        C44Matrix matrix = ToMatrix(accum);
        uint32_t flags = ProcedurallyDriven | BindAxisScale;
        for (int t = 0; t < 2; ++t) {
            if (index == twistIndex[t] && twistSet[t]) {
                // The engine is also turning this bone this frame: stack ours on its rotation, post-multiplied.
                matrix = twistBase[t] * matrix;
                flags = ProcedurallyDriven;
            }
        }
        model->SetBoneProceduralTransformByIndex(index, matrix);
        model->SetBoneFlagsByIndex(index, flags, ProcedurallyDriven | BindAxisScale);
        s.applied.insert(index);
    }

    // Release bones that no longer have anything to apply.
    for (auto it = s.applied.begin(); it != s.applied.end();) {
        auto w = wanted.find(*it);
        if (w != wanted.end() && !Neutral(w->second)) {
            ++it;
            continue;
        }
        bool twisted = (*it == twistIndex[0] && twistSet[0]) || (*it == twistIndex[1] && twistSet[1]);
        if (!twisted) {
            model->SetBoneFlagsByIndex(*it, 0, ProcedurallyDriven | BindAxisScale);
        } else {
            model->SetBoneFlagsByIndex(*it, 0, BindAxisScale);
        }
        it = s.applied.erase(it);
    }
}

// Wire form: one signed byte per slot (see BodyShapeWire.h).
inline CharacterBodyShape::Values ToWire(const Values& values) {
    CharacterBodyShape::Values wire{};
    for (size_t i = 0; i < CharacterBodyShape::AssignedSlots; ++i) {
        int v = static_cast<int>(std::lround(values.Get(CharacterBodyShape::SlotNames[i]) * 100.0f));
        wire[i] = static_cast<int8_t>(std::max(CharacterBodyShape::MeshSlot(i) ? 0 : -100, std::min(100, v)));
    }
    return wire;
}

inline Values FromWire(const CharacterBodyShape::Values& wire) {
    Values values;
    for (size_t i = 0; i < CharacterBodyShape::AssignedSlots; ++i) {
        if (wire[i] != 0) {
            values.v[CharacterBodyShape::SlotNames[i]] = static_cast<float>(wire[i]) / 100.0f;
        }
    }
    return values;
}

// Saved values of the characters in the character list, by guid (from the server's list tail).
inline std::map<uint64_t, Values>& Roster() {
    static std::map<uint64_t, Values> roster;
    return roster;
}

inline const Values* RosterFor(uint64_t guid) {
    auto it = Roster().find(guid);
    return it == Roster().end() ? nullptr : &it->second;
}

// Values the server sent for the players in view, by guid. `distant` is the same without the mesh sliders: those cost a
// vertex buffer per player, so they only apply to players close by.
struct UnitShape {
    Values full;
    Values distant;
};

inline std::map<uint64_t, UnitShape>& Units() {
    static std::map<uint64_t, UnitShape> units;
    return units;
}

inline void SetUnit(uint64_t guid, const Values& values) {
    UnitShape& shape = Units()[guid];
    shape.full = values;
    shape.distant = values;
    shape.distant.v.erase("BreastSize");
    shape.distant.v.erase("ButtSize");
}

inline const UnitShape* UnitFor(uint64_t guid) {
    auto it = Units().find(guid);
    return it == Units().end() ? nullptr : &it->second;
}

// True when a model was driven before, so it still needs one pass with defaults to release what we set.
inline bool HasApplier(CM2Model* model) {
    return Appliers().count(model) != 0;
}

// The edited values on the creator preview or the local player's model.
inline void ApplyEdited(CM2Model* model, const bool twistSet[2], const C44Matrix twistBase[2]) {
    Edited().model = model;
    Apply(model, Edited().values, twistSet, twistBase);
}

} // namespace BodyShape

#endif
