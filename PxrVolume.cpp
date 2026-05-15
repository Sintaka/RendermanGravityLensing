#include "RixBxdf.h"
// #include "RixRNG.h"
#include "RixShadingUtils.h"
#include "LightBendIntegrator.h"
// #include "PxrVolHomoIntegrator.h"
// #include "PxrVolHeteroIntegrator.h"
// #include "RixVolume.h"

using namespace RixConstants;

static const unsigned char k_tranDiffuseLobeId = 0;
static RixBXLobeSampled s_tranDiffuseLobe;
static RixBXLobeTraits s_tranDiffuseLobeTraits;

// Constants
const static RtUString US_STEPLENGTH("stepLength");
const static RtUString US_STEPDIRECTION("stepDirection");
const static RtUString US_STEPSIZE("stepSize");

// Default primvar Name
const static RtUString US_DEFAULT_STEPLENGTH("stepLength");
const static RtUString US_DEFAULT_STEPDIRECTION("stepDirection");

struct volumeInstanceData
{
    RtFloat stepSize;
    bool isLight;
    RtColorRGB othreshold;
    RtUString stepLengthName;
    RtUString stepDirectionName;
};

enum paramId
{
    k_stepLength,
    k_stepDirection,
    k_stepSize,
    k_numParams
};

class interfaceBxdf : public RixBxdf
{
public:
    interfaceBxdf(RixShadingContext const* sc, RixBxdfFactory* bx, RixBXLobeTraits lobesWanted)
        : RixBxdf(sc, bx), m_lobesWanted(lobesWanted)
    {
        m_lobesWanted &= s_tranDiffuseLobeTraits;
        m_lobesWanted.SetAll();
        m_lobesWanted.SetContinuation(true);
        m_lobesWanted.SetDiscrete(true);
        sc->GetBuiltinVar(RixShadingContext::k_Vn, &m_Vn);
    }

    RixBXEvaluateDomain GetEvaluateDomain() override
    {
        return k_RixBXEmptyDomain;
    }

    void GetAggregateLobeTraits(RixBXLobeTraits* t) override
    {
        *t = m_lobesWanted;
    }

    RixSCDetail GetProperty(BxdfProperty, void const**) const override
    {
        return k_RixSCInvalidDetail;
    }

    bool EmitLocal(RtColorRGB*) override
    {
        return false;
    }

    void GenerateSample(RixBXTransportTrait transportTrait, RixBXLobeTraits const* lobesWanted,
        RixRNG* rng, RixBXLobeSampled* lobesSampled, RtVector3* On,
        RixBXLobeWeights& W, RtFloat* FPdf, RtFloat* RPdf,
        RtColorRGB* compTrans) override
    {
        PIXAR_ARGUSED(transportTrait);
        PIXAR_ARGUSED(rng);
        RtInt nPts = shadingCtx->numPts;
        RixBXLobeTraits all = GetAllLobeTraits();
        RtColorRGB* diffuseWgt = NULL;

        for (int i = 0; i < nPts; i++)
        {
            lobesSampled[i].SetValid(false);
            RixBXLobeTraits lobes = all & lobesWanted[i];
            bool doDiff = (lobes & s_tranDiffuseLobeTraits).HasAny();

            if (!diffuseWgt && doDiff) diffuseWgt = W.AddActiveLobe(s_tranDiffuseLobe);
            if (doDiff)
            {
                On[i] = -m_Vn[i];
                diffuseWgt[i] = RixConstants::k_OneRGB;
                FPdf[i] = RPdf[i] = 1.0f;
                lobesSampled[i] = s_tranDiffuseLobe;
                lobesSampled[i].SetContinuation(true);
                lobesSampled[i].SetDiscrete(true);
            }
            if (compTrans) compTrans[i] = k_OneRGB;
        }
    }

    void EvaluateSample(RixBXTransportTrait, RixBXLobeTraits const*, RixRNG*,
        RixBXLobeTraits* lobesEvaluated, RtVector3 const*, RixBXLobeWeights&,
        RtFloat*, RtFloat*) override
    {
        RtInt nPts = shadingCtx->numPts;
        for (int i = 0; i < nPts; i++) lobesEvaluated[i].SetNone();
    }

    void EvaluateSamplesAtIndex(RixBXTransportTrait, RixBXLobeTraits const&, RixRNG*, RtInt,
        RtInt nSamples, RixBXLobeTraits* lobesEvaluated, RtVector3 const*,
        RixBXLobeWeights&, RtFloat*, RtFloat*) override
    {
        for (int i = 0; i < nSamples; i++) lobesEvaluated[i].SetNone();
    }

private:
    RixBXLobeTraits m_lobesWanted;
    RtVector3 const* m_Vn;
};

// PxrVolumeFactory
class PxrVolumeFactory : public RixBxdfFactory
{
public:
    PxrVolumeFactory();
    ~PxrVolumeFactory() override;

    int Init(RixContext&, RtUString const pluginpath) override;
    RixSCParamInfo const* GetParamTable() override;
    void Finalize(RixContext&) override;

    void Synchronize(RixContext& ctx, RixSCSyncMsg syncMsg,
        RixParameterList const* parameterList) override;

    void CreateInstanceData(RixContext&, RtUString const handle, RixParameterList const*,
        InstanceData* idata) override;

    void SynchronizeInstanceData(RixContext&, RtUString const, RixParameterList const*,
        uint32_t editHints, InstanceData*) override;

    int GetInstanceHints(RtPointer instanceData) const override
    {
        return k_ComputesInterior | k_InteriorTransmission | k_InteriorOverlapping |
            k_InteriorHeterogeneous;
    }

    // 
    void RegisterTemporalVolumeParams(RtPointer instanceData,
        std::vector<RtInt>& paramid) const override
    {

    }

    RixBxdf* BeginScatter(RixShadingContext const*, RixBXLobeTraits const& lobesWanted,
        RixSCShadingMode sm, void* parentData, RtPointer instanceData) override;

    void EndScatter(RixBxdf*) override {}

    RixOpacity* BeginOpacity(RixShadingContext const*, RixSCShadingMode, void* parentData,
        RtPointer instanceData) override
    {
        return NULL;
    }

    void EndOpacity(class RixOpacity*) override {}

    RixVolumeIntegrator* BeginInterior(RixShadingContext const*, RixSCShadingMode, void* parentData,
        RtPointer instanceData) override;

    void EndInterior(RixVolumeIntegrator*) override {}

    RixVolumeIntegrator* BeginSubsurface(RixShadingContext const*, RixSCShadingMode, void*,
        RtPointer) override
    {
        return NULL;
    }

    void EndSubsurface(class RixVolumeIntegrator*) override {}

    float GetIndexOfRefraction(RtPointer) const override
    {
        return 1.0f;
    }

private:
    RtFloat m_defaultStepSize;
    bool m_defaultIsLight;
};

extern "C" PRMANEXPORT RixBxdfFactory* CreateRixBxdfFactory(const char* hint)
{
    PIXAR_ARGUSED(hint);
    return new PxrVolumeFactory();
}

extern "C" PRMANEXPORT void DestroyRixBxdfFactory(RixBxdfFactory* bxdf)
{
    delete (PxrVolumeFactory*)bxdf;
}

PxrVolumeFactory::PxrVolumeFactory()
{
    // m_defaultStepSize = 0.1f;
    m_defaultStepSize = 2000;
    m_defaultIsLight = false;
}

PxrVolumeFactory::~PxrVolumeFactory() {}

int PxrVolumeFactory::Init(RixContext& ctx, RtUString const pluginpath)
{
    PIXAR_ARGUSED(ctx);
    PIXAR_ARGUSED(pluginpath);
    return 0;
}

void PxrVolumeFactory::Synchronize(RixContext& ctx, RixSCSyncMsg syncMsg,
    RixParameterList const* parameterList)
{
    PIXAR_ARGUSED(parameterList);
    if (syncMsg == k_RixSCRenderBegin)
    {
        s_tranDiffuseLobe = RixBXLookupLobeByName(ctx, false, false, false, false,
            k_tranDiffuseLobeId, "Diffuse");
        s_tranDiffuseLobeTraits = RixBXLobeTraits(s_tranDiffuseLobe);
    }
}

RixSCParamInfo const* PxrVolumeFactory::GetParamTable()
{
    const RixSCAccess densityInput = (RixSCAccess)((int)k_RixSCVolumeScatterInput |
        (int)k_RixSCScatterInput |
        (int)k_RixSCVolumeTransmissionInput);

    static RixSCParamInfo s_ptable[] = {
        RixSCParamInfo(US_STEPLENGTH,    k_RixSCFloat,  densityInput),
        RixSCParamInfo(US_STEPDIRECTION, k_RixSCColor,  densityInput),
        RixSCParamInfo(US_STEPSIZE,      k_RixSCFloat,  k_RixSCVolumeScatterInput),
        RixSCParamInfo()
    };
    return &s_ptable[0];
}

void PxrVolumeFactory::Finalize(RixContext&) {}

void PxrVolumeFactory::CreateInstanceData(RixContext& ctx, RtUString const handle,
    RixParameterList const* plist, InstanceData* idata)
{
    PIXAR_ARGUSED(handle);

    RtFloat stepSize = m_defaultStepSize;
    RtInt isLight = m_defaultIsLight;
    RtInt paramId;
    RixSCType dType;
    RixSCConnectionInfo dCinfo;

    // stepSize
    if (0 == plist->GetParamId(US_STEPSIZE, &paramId))
    {
        plist->GetParamInfo(paramId, &dType, &dCinfo);
        if (dType == k_RixSCFloat && dCinfo == k_RixSCParameterListValue)
        {
            if (plist->EvalParam(paramId, -1, &stepSize) == k_RixSCInvalidDetail)
                stepSize = m_defaultStepSize;
        }
    }

    // isLight
    if (0 == plist->GetParamId(Rix::k_islight, &paramId))
    {
        plist->GetParamInfo(paramId, &dType, &dCinfo);
        if (dType == k_RixSCInteger && dCinfo == k_RixSCParameterListValue)
            plist->EvalParam(paramId, -1, &isLight);
    }

    // stepLength primvar 名称（用户可覆盖，默认 "stepLength"）
    RtUString stepLengthName = US_DEFAULT_STEPLENGTH;
    if (0 == plist->GetParamId(US_STEPLENGTH, &paramId))
    {
        plist->GetParamInfo(paramId, &dType, &dCinfo);
        if (dType == k_RixSCString && dCinfo == k_RixSCParameterListValue)
        {
            RtUString customName;
            if (plist->EvalParam(paramId, -1, &customName) != k_RixSCInvalidDetail)
                stepLengthName = customName;
        }
    }

    // stepDirection primvar 名称（用户可覆盖，默认 "stepDirection"）
    RtUString stepDirectionName = US_DEFAULT_STEPDIRECTION;
    if (0 == plist->GetParamId(US_STEPDIRECTION, &paramId))
    {
        plist->GetParamInfo(paramId, &dType, &dCinfo);
        if (dType == k_RixSCString && dCinfo == k_RixSCParameterListValue)
        {
            RtUString customName;
            if (plist->EvalParam(paramId, -1, &customName) != k_RixSCInvalidDetail)
                stepDirectionName = customName;
        }
    }

    volumeInstanceData* instanceData = (volumeInstanceData*)malloc(sizeof(volumeInstanceData));
    instanceData->stepSize        = stepSize;
    instanceData->isLight         = isLight;
    instanceData->othreshold      = RixConstants::k_OneRGB;
    instanceData->stepLengthName    = stepLengthName;
    instanceData->stepDirectionName = stepDirectionName;

    idata->data = instanceData;
    idata->datalen = sizeof(volumeInstanceData);
    idata->freefunc = free;
    idata->synchronizeHints = RixShadingPlugin::SynchronizeHints::k_All;
}

void PxrVolumeFactory::SynchronizeInstanceData(RixContext& ctx, RtUString const handle,
    RixParameterList const* parms, uint32_t editHints,
    InstanceData* instance)
{
    PIXAR_ARGUSED(editHints);
    PIXAR_ARGUSED(handle);
    PIXAR_ARGUSED(parms);

    volumeInstanceData* instanceData = static_cast<volumeInstanceData*>(instance->data);
    RixRenderState& state = *reinterpret_cast<RixRenderState*>(
        ctx.GetRixInterface(k_RixRenderState));

    instanceData->othreshold = RixConstants::k_OneRGB;
    RixRenderState::Type otype;
    RtInt ocount;
    const static RtUString US_OTHRESHOLD("limits:othreshold");
    if (state.GetOption(US_OTHRESHOLD, &instanceData->othreshold, sizeof(RtColorRGB), &otype,
        &ocount) != 0 || otype != RixRenderState::k_Color || ocount != 3)
    {
        instanceData->othreshold = RixConstants::k_OneRGB;
    }
}

RixBxdf* PxrVolumeFactory::BeginScatter(RixShadingContext const* sCtx,
    RixBXLobeTraits const& lobesWanted, RixSCShadingMode sm,
    void* parentData, RtPointer instanceData)
{
    PIXAR_ARGUSED(parentData);
    PIXAR_ARGUSED(instanceData);

    RixShadingContext::Allocator pool(sCtx);
    void* mem = pool.AllocForBxdf<interfaceBxdf>(1);
    return new (mem) interfaceBxdf(sCtx, this, lobesWanted);
}

RixVolumeIntegrator* PxrVolumeFactory::BeginInterior(RixShadingContext const* sCtx,
    RixSCShadingMode mode, void* parentData,
    RtPointer instanceData)
{
    PIXAR_ARGUSED(parentData);

    if (mode == k_RixSCPresenceQuery) return NULL;

    RixShadingContext::Allocator pool(sCtx);
    void* mem = pool.AllocForBxdf<LightBendIntegrator>(1);
    return new (mem) LightBendIntegrator(sCtx, this, instanceData);
}