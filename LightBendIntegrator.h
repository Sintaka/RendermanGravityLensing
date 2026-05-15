#ifndef LightBendIntegrator_h
#define LightBendIntegrator_h

#include <cassert>
#include <algorithm>
#include <cstdlib>
// #include "RixBxdfLobe.h"
// #include "RixLighting.h"
// #include "RixColorUtils.h"
#include "RixShadingUtils.h"
#include "RixVolume.h"
// #include "RixBxdf.h"


// ----------------------------------------------------------------
// Helper class
// ----------------------------------------------------------------
class LiHelp
{
public:
    float length(RtFloat3 v) {
        return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }

    RtFloat3 normalize(RtFloat3 v) {
        float len = length(v);
        return len > 0 ? (v / len) : RtFloat3(0.0f);
    }

    RtFloat3 lerp(RtFloat3 a, RtFloat3 b, RtFloat3 blend) {
        return (1 - blend) * a + blend * b;
    }

    void setPuvw(RixShadingContext* vCtx, RtFloat3* P, bool TransformWorld2Object = true) {
        RixShadingContext::Allocator vpool(vCtx);
        RtInt nPts = vCtx->numPts;
        RtFloat* vu = vpool.AllocForVolume<RtFloat>(nPts);
        RtFloat* vv = vpool.AllocForVolume<RtFloat>(nPts);
        RtFloat* vw = vpool.AllocForVolume<RtFloat>(nPts);

        if (TransformWorld2Object)
            vCtx->Transform(RixShadingContext::k_AsPoints, Rix::k_world, Rix::k_uvw, P);

        for (int i = 0; i < nPts; ++i) {
            vu[i] = RixClamp(P[i].x, 0.f, 1.f);
            vv[i] = RixClamp(P[i].y, 0.f, 1.f);
            vw[i] = RixClamp(P[i].z, 0.f, 1.f);
        }

        vCtx->SetBuiltinVar(RixShadingContext::k_u, vu);
        vCtx->SetBuiltinVar(RixShadingContext::k_v, vv);
        vCtx->SetBuiltinVar(RixShadingContext::k_w, vw);
    }
};


// ----------------------------------------------------------------
// BendLight helper class  (replaces the scattered bendlight() overloads)
// ----------------------------------------------------------------
class BendLight
{
public:
    // overload 1: gravity-well bending with integrator context, fixed mass
    static void bend(RtInt numRays, RtRayGeometry* newRays,
                     RixIntegratorContext& iCtx, RtFloat m_mass = 3.f)
    {
        LiHelp lihp;
        for (int i = 0; i < 1000; ++i) {
            RtHitPoint* hits = iCtx.Allocate<RtHitPoint>(numRays);
            iCtx.GetNearestHits(numRays, newRays, hits);
            for (int nr = 0; nr < numRays; ++nr) {
                if (hits[nr].dist <= 0) {
                    float d = lihp.length(newRays[nr].origin);
                    RtFloat3 toWorldCenter = -lihp.normalize(newRays[nr].origin);
                    RtFloat3 force = toWorldCenter * m_mass / (d * d);
                    float dt = std::min(1.0f, float(.01 * d * d / m_mass));
                    newRays[nr].direction += force * dt;
                    newRays[nr].direction  = lihp.normalize(newRays[nr].direction);
                    newRays[nr].origin    += newRays[nr].direction * dt;
                }
            }
        }
    }

    // overload 2: gravity-well bending with integrator context, parametric mass/maxDist
    static void bend(RtInt numRays, RtRayGeometry const* rays,
                     RixIntegratorContext& iCtx,
                     RtFloat m_mass, RtFloat /*maxDist*/)
    {
        LiHelp lihp;
        RtRayGeometry* newRays = const_cast<RtRayGeometry*>(rays);
        for (int i = 0; i < 2000; ++i) {
            RtHitPoint* hits = iCtx.Allocate<RtHitPoint>(numRays);
            iCtx.GetNearestHits(numRays, newRays, hits);
            for (int nr = 0; nr < numRays; ++nr) {
                if (hits[nr].dist <= 0) {
                    float d = lihp.length(newRays[nr].origin);
                    RtFloat3 toWorldCenter = -lihp.normalize(newRays[nr].origin);
                    RtFloat3 force = toWorldCenter * m_mass / (d * d);
                    float dt = std::min(1.0f, float(.01 * d * d / m_mass));
                    newRays[nr].direction += force * dt;
                    newRays[nr].direction  = lihp.normalize(newRays[nr].direction);
                    newRays[nr].origin    += newRays[nr].direction * dt;
                }
            }
        }
    }

    // overload 3: simple origin offset
    static void bend(RtInt nRays, RtRayGeometry const* rays) {
        RtRayGeometry* newRays = const_cast<RtRayGeometry*>(rays);
        for (int nr = 0; nr < nRays; ++nr)
            newRays[nr].origin += RtPoint3(0.f, -1.f, 0.f);
    }

    // overload 4: direction flip based on world-space Y, uses shading context
    static void bend(RtInt numRays, RtRayGeometry const* rays,
                     RixShadingContext const* sctx,
                     RtFloat /*m_mass*/ = 1.0f, RtFloat /*maxDist*/ = 1.0f)
    {
        RixShadingContext::Allocator pool(sctx);
        RtRayGeometry* newRays = const_cast<RtRayGeometry*>(rays);
        RtPoint3* P = pool.AllocForVolume<RtPoint3>(numRays);
        for (int nr = 0; nr < numRays; ++nr)
            P[nr] = newRays[nr].origin;
        sctx->Transform(RixShadingContext::k_AsPoints, Rix::k_current, Rix::k_world, P);
        for (int nr = 0; nr < numRays; ++nr) {
            newRays[nr].direction = (P[nr].y < 0.f)
                ? RtFloat3(0.f,  1.f, 0.f)
                : RtFloat3(0.f, -1.f, 0.f);
        }
    }
};


// ----------------------------------------------------------------
// Integrator
// ----------------------------------------------------------------
class LightBendIntegrator : public RixVolumeIntegrator
{
public:
    LightBendIntegrator(RixShadingContext const* sc, RixBxdfFactory* bx, RtPointer instanceData)
        : RixVolumeIntegrator(sc, bx, instanceData)
    { }

    void GetNearestHits(RtInt numRays, RtRayGeometry const* rays, RixRNG* rng,
        RixBXLobeTraits const& lobesWanted, RixIntegratorContext& iCtx,
        RixLightingServices* lightingServices, IntegratorDelegate* lcb,
        RtInt* numGrps, RixShadingContext const** shadeGrps,
        RtUString const subset, RtUString const excludeSubset,
        bool isLightPath, RtHitSides hitSides, bool isPrimary) override
    {
        // Init
        RixShadingContext const* sCtx = GetShadingCtx();
        RixShadingContext* vCtx = this->BeginVolumeSampling();
        RixShadingContext::Allocator vpool(vCtx);
        RtInt nPts = sCtx->numPts;

        // Allocate
        RtPoint3 const* P;
        LiHelp lihp;
        sCtx->GetBuiltinVar(RixShadingContext::k_P, &P);

        RtPoint3* vP = vpool.AllocForVolume<RtPoint3>(nPts);
        RtRayGeometry* newRays = const_cast<RtRayGeometry*>(rays);
        RtInt LeftnumRays = numRays;
        RtInt* LeftRaysId = vpool.AllocForVolume<RtInt>(nPts);
        for (int nr = 0; nr < numRays; ++nr)
            LeftRaysId[nr] = nr;

        // ----------------------------------------------------------------
        // Tracing
        RtFloat const* densityFloat = NULL;
        RtFloat3 const* densityColor = NULL;

        for (int traceRound = 0; traceRound < 2000 && LeftnumRays > 0; ++traceRound)
        {
            RtRayGeometry* Rays2Trace = vpool.AllocForVolume<RtRayGeometry>(LeftnumRays);
            RtInt* Rays2TraceId      = vpool.AllocForVolume<RtInt>(LeftnumRays);
            memcpy(Rays2TraceId, LeftRaysId, LeftnumRays * sizeof(RtInt));
            RtHitPoint* hits = vpool.AllocForVolume<RtHitPoint>(LeftnumRays);
            int ActiveNumRays = 0;

            for (int nr = 0; nr < LeftnumRays; ++nr)
                Rays2Trace[nr] = newRays[Rays2TraceId[nr]];

            for (int nr = 0; nr < LeftnumRays; ++nr)
                vP[nr] = Rays2Trace[nr].origin;

            vCtx->Transform(RixShadingContext::k_AsPoints, Rix::k_current, Rix::k_world, vP);
            lihp.setPuvw(vCtx, vP);
            vCtx->GetPrimVar(RtUString("density"),      1.f,           &densityFloat);
            vCtx->GetPrimVar(RtUString("densityColor"), RtFloat3(0.f), &densityColor);

            for (int nr = 0; nr < LeftnumRays; ++nr) {
                if (densityFloat[nr] <= 0.f) { Rays2Trace[nr].maxDist = -1; continue; }
                Rays2Trace[nr].maxDist = densityFloat[nr];
            }

            iCtx.GetNearestHits(LeftnumRays, Rays2Trace, hits);

            for (int nr = 0; nr < LeftnumRays; ++nr) {
                if (hits[nr].dist <= 0.f && densityFloat[nr] > 0.f) {
                    // Miss — advance along bent direction
                    float dt = densityFloat[nr];
                    RtFloat3 sampledir = densityColor[nr];
                    Rays2Trace[nr].direction += sampledir;
                    Rays2Trace[nr].direction  = (RtVector3)lihp.normalize(Rays2Trace[nr].direction);
                    Rays2Trace[nr].origin    += dt * Rays2Trace[nr].direction;
                    LeftRaysId[ActiveNumRays++] = Rays2TraceId[nr];
                }
                // Hit — do nothing
                newRays[Rays2TraceId[nr]] = Rays2Trace[nr];
            }
            LeftnumRays = ActiveNumRays;
        }
        // ----------------------------------------------------------------

        // Restore maxDist
        for (int nr = 0; nr < numRays; ++nr)
            if (newRays[nr].maxDist > 0.f)
                newRays[nr].maxDist = 1e10f;

        // Restore P
        memcpy(vP, P, numRays * sizeof(RtFloat3));
        lihp.setPuvw(vCtx, vP);

        iCtx.GetNearestHits(numRays, rays, lobesWanted, false,
            numGrps, shadeGrps,
            subset, excludeSubset, isLightPath, hitSides, isPrimary);

        this->EndVolumeSampling();
    }

    void GetTransmission(RtInt numRays, RtRayGeometry const* rays, RixRNG* rng,
        RixIntegratorContext& iCtx,
        RtColorRGB* trans, RtColorRGB* emission,
        RtUString const subset      = US_NULL,
        RtUString const excludeSubset = US_NULL) override
    {
        for (int i = 0; i < numRays; ++i)
            trans[i] = RtColorRGB(1.0f);
    }
};


#endif
