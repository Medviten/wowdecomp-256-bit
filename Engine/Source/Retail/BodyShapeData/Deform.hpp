#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace body_shape
{
struct Vec { float x, y, z; };
struct Region { Vec offset, scale; };
struct MaskEntry { uint32_t x, y, z, bits; };
struct Profile
{
    const char* path;
    Region chest, rear;
    Vec minimum, maximum;
    MaskEntry const* mask = nullptr;
    size_t maskSize = 0;
    const char* asset = nullptr; // Actual modern payload; path may be a legacy runtime alias.
    bool rounded = false;
    float chestLower = 0.f; // Canonical lower boundary; zero preserves existing profiles.
    float rearLower = 0.f;
    float rearBlend = .06f;
    float chestBlend = .035f;
    float chestUpper = 0.f; // Optional per-model collarbone limit; zero keeps the existing field.
    float chestUpperBlend = .065f;
};
constexpr Region Identity{{0,0,0},{1,1,1}};
constexpr Profile Human{"character\\human\\female\\humanfemale_hd",Identity,Identity,
    {-2,-2,-.3f},{2,2,2.5f}};
inline Vec Canonical(Vec p, Region const& r)
{
    return {(p.x-r.offset.x)/r.scale.x,(p.y-r.offset.y)/r.scale.y,(p.z-r.offset.z)/r.scale.z};
}
// Prepared GPU streams may use different vertex order and palette indexes. Match audited
// bind positions instead; signed zero is canonicalized. Unknown positions remain untouched.
inline uint32_t VertexMask(Vec p, Profile const& profile)
{
    if (!profile.mask) return 0xff03; // Original human profile preserves its established behavior.
    uint32_t key[3];
    float values[]={p.x==0 ? 0.f:p.x,p.y==0 ? 0.f:p.y,p.z==0 ? 0.f:p.z};
    std::memcpy(key,values,sizeof(key));
    size_t low=0, high=profile.maskSize;
    while (low<high)
    {
        size_t mid=low+(high-low)/2;
        auto const& m=profile.mask[mid];
        bool less=m.x<key[0] || (m.x==key[0] && (m.y<key[1] || (m.y==key[1] && m.z<key[2])));
        if (less) low=mid+1; else high=mid;
    }
    if (low==profile.maskSize) return 0;
    auto const& m=profile.mask[low];
    return m.x==key[0] && m.y==key[1] && m.z==key[2] ? m.bits : 0;
}
inline float Bell(float value)
{
    value = std::abs(value);
    if (value >= 1.0f) return 0.0f;
    float t = 1.0f - value * value;
    return t * t;
}
// Broad, flat influence in the breast region with a C2-smooth transition to zero.
// Unlike a peaked bell, this does not focus the added volume at a single tip.
inline float Plateau(float distance, float inner, float outer)
{
    float t = std::clamp((std::abs(distance)-inner)/(outer-inner),0.0f,1.0f);
    return std::clamp(1.0f-t*t*t*(t*(t*6.0f-15.0f)+10.0f),0.0f,1.0f);
}
// Give the underside vertical room independently of the backward expansion.
// The short upper-thigh transition moves only vertically: extending the backward
// field here would recreate the secondary thigh bulge. Anchor relative to each
// audited crease, with zero displacement and slope at both ends.
inline float RearUnderside(Vec r, Profile const& profile)
{
    auto smooth = [](float t) { t=std::clamp(t,0.f,1.f); return t*t*(3.f-2.f*t); };
    float lower = profile.rearLower > 0.f ? profile.rearLower : .965f;
    float depth = std::max(0.f, lower + .160f - r.z);
    float support = smooth((r.z-lower+.070f)/.100f) * smooth(depth/.050f);
    support *= Plateau(r.y,.19f,.30f) * smooth((.015f-r.x)/.070f);
    return -.28f * depth * support;
}
// Independent chest and pelvis transforms calibrate the human masks to audited skeletons.
// This deforms bind-space geometry; it does not change bone matrices or animation tracks.
inline Vec Deform(Vec p, float breast, float butt, Profile const& profile = Human)
{
    if (breast==0 && butt==0) return p;
    Vec q = p;
    Vec c = Canonical(p,profile.chest), r = Canonical(p,profile.rear);
    auto smooth = [](float t) { t=std::clamp(t,0.f,1.f); return t*t*(3.f-2.f*t); };
    float lowerChest = smooth((c.z-profile.chestLower)/profile.chestBlend);
    float upperChest = profile.chestUpper > 0.f
        ? smooth((profile.chestUpper-c.z)/profile.chestUpperBlend) : 1.f;
    float lowerRear = smooth((r.z-profile.rearLower)/profile.rearBlend);
    if (lowerChest == 0.f || upperChest == 0.f)
        breast = 0.f;
    if (breast == 0.f && butt == 0.f)
        return p;
    q.z += butt * RearUnderside(r,profile) * profile.rear.scale.z;
    if (profile.rounded)
    {
        // Enlarge existing contours around each lobe, instead of pushing a narrow
        // peak outward. The broad plateau scales the whole curved surface together.
        float front=std::max(0.f,c.x-.0174181f);
        float chest=Plateau(c.z-1.454276f,.10f,.19f)*Plateau(c.y,.15f,.22f)*smooth(front/.06f);
        // Keep the lower rib cage fixed while blending into the breast underside.
        chest *= lowerChest * upperChest;
        float side=.0824443f*std::tanh(c.y/.045f);
        q.x += breast*.32f*front*chest*profile.chest.scale.x;
        q.y += breast*.24f*(c.y-side)*chest*profile.chest.scale.y;
        q.z += breast*.24f*(c.z-1.454276f)*chest*profile.chest.scale.z;
        float back=std::min(0.f,r.x-.0280119f);
        float rear=Plateau(r.z-1.091429f,.14f,.25f)*Plateau(r.y,.19f,.30f)*smooth(-back/.08f);
        rear *= lowerRear;
        float hip=.0869541f*std::tanh(r.y/.06f);
        q.x += butt*.28f*back*rear*profile.rear.scale.x;
        q.y += butt*.18f*(r.y-hip)*rear*profile.rear.scale.y;
        // Retain the upper-lobe rounding; only replace the lower vertical field.
        float upper = std::max(0.f,r.z-1.091429f);
        q.z += butt*.16f*upper*rear*smooth(upper/.040f)*profile.rear.scale.z;
        return q;
    }
    float chest = Plateau(c.z-1.465f,.055f,.155f) * Plateau(c.y,.12f,.20f);
    chest *= std::clamp((c.x - .035f) / .065f, 0.0f, 1.0f);
    chest *= lowerChest * upperChest;
    q.x += breast * .055f * chest * profile.chest.scale.x;
    q.y += breast * c.y * .18f * chest * profile.chest.scale.y;
    float rear = Bell((r.z - 1.075f) / .185f) * Bell(r.y / .235f);
    rear *= std::clamp((-r.x - .025f) / .075f, 0.0f, 1.0f);
    rear *= lowerRear;
    q.x -= butt * .065f * rear * profile.rear.scale.x;
    q.y += butt * r.y * .18f * rear * profile.rear.scale.y;
    return q;
}
inline Vec Cross(Vec a, Vec b)
{
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}
inline Vec Normal(Vec p, Vec n, float breast, float butt, Profile const& profile = Human)
{
    constexpr float e = .0001f;
    Vec base = Deform(p, breast, butt,profile);
    Vec x = Deform({p.x+e,p.y,p.z},breast,butt,profile);
    Vec y = Deform({p.x,p.y+e,p.z},breast,butt,profile);
    Vec z = Deform({p.x,p.y,p.z+e},breast,butt,profile);
    x = {(x.x-base.x)/e,(x.y-base.y)/e,(x.z-base.z)/e};
    y = {(y.x-base.x)/e,(y.y-base.y)/e,(y.z-base.z)/e};
    z = {(z.x-base.x)/e,(z.y-base.y)/e,(z.z-base.z)/e};
    Vec a = Cross(y,z), b = Cross(z,x), c = Cross(x,y);
    Vec out{a.x*n.x+b.x*n.y+c.x*n.z,a.y*n.x+b.y*n.y+c.y*n.z,a.z*n.x+b.z*n.y+c.z*n.z};
    float len = std::sqrt(out.x*out.x+out.y*out.y+out.z*out.z);
    return len > .00001f ? Vec{out.x/len,out.y/len,out.z/len} : n;
}
}
