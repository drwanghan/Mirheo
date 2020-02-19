#include "factory.h"

#include "membrane.h"

#include "force_kernels/common.h"
#include "force_kernels/dihedral/kantor.h"
#include "force_kernels/dihedral/juelicher.h"
#include "force_kernels/triangle/lim.h"
#include "force_kernels/triangle/wlc.h"

#include <mirheo/core/utils/variant_foreach.h>

namespace mirheo
{

std::shared_ptr<BaseMembraneInteraction>
createInteractionMembrane(const MirState *state, const std::string& name,
                          CommonMembraneParameters commonParams,
                          VarBendingParams varBendingParams, VarShearParams varShearParams,
                          bool stressFree, real growUntil, VarMembraneFilter varFilter)
{
    std::shared_ptr<BaseMembraneInteraction> impl;

    mpark::visit([&](auto bendingParams, auto shearParams, auto filter)
    {                     
        using FilterType    = decltype(filter);
        using DihedralForce = typename decltype(bendingParams)::DihedralForce;
        
        if (stressFree)
        {
            using TriangleForce = typename decltype(shearParams)::TriangleForce <StressFreeState::Active>;
            
            impl = std::make_shared<MembraneInteraction<TriangleForce, DihedralForce, FilterType>>
                (state, name, commonParams, shearParams, bendingParams, growUntil, filter);
        }
        else
        {
            using TriangleForce = typename decltype(shearParams)::TriangleForce <StressFreeState::Inactive>;
            
            impl = std::make_shared<MembraneInteraction<TriangleForce, DihedralForce, FilterType>>
                (state, name, commonParams, shearParams, bendingParams, growUntil, filter);
        }
    }, varBendingParams, varShearParams, varFilter);

    return std::move(impl);
}


namespace
{
struct MembraneFactoryVisitor
{
    template <typename BendingParams, typename ShearParams, typename FilterType>
    void operator()()
    {
        using DihedralForce = typename BendingParams::DihedralForce;

        {
            using TriangleForce = typename ShearParams::TriangleForce <StressFreeState::Active>;
            using Impl = MembraneInteraction<TriangleForce, DihedralForce, FilterType>;
            if (Impl::getTypeName() == typeName)
            {
                *impl = std::make_shared<Impl>(state, loader, config);
                return;
            }
        }
        {
            using TriangleForce = typename ShearParams::TriangleForce <StressFreeState::Inactive>;
            using Impl = MembraneInteraction<TriangleForce, DihedralForce, FilterType>;
            if (Impl::getTypeName() == typeName)
            {
                *impl = std::make_shared<Impl>(state, loader, config);
                return;
            }
        }
    }

    const MirState *state;
    Loader& loader;
    const ConfigObject& config;
    const std::string& typeName;
    std::shared_ptr<BaseMembraneInteraction> *impl;
};
} // anonymous namespace


std::shared_ptr<BaseMembraneInteraction>
loadInteractionMembrane(const MirState *state, Loader& loader, const ConfigObject& config)
{
    std::shared_ptr<BaseMembraneInteraction> impl;
    const std::string& typeName = config["__type"].getString();

    /// Check all possible template combinations and match with the `typeName`.
    variantForeach<MembraneFactoryVisitor, VarBendingParams, VarShearParams, VarMembraneFilter>(
            MembraneFactoryVisitor{state, loader, config, typeName, &impl});

    if (!impl)
        die("Unrecognized impl type \"%s\".", typeName.c_str());

    return std::move(impl);

}

} // namespace mirheo