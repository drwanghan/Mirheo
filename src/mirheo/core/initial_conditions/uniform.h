#pragma once

#include "interface.h"
#include <mirheo/core/datatypes.h>

namespace mirheo
{

/** \brief Fill the domain with uniform number density
    \ingroup ICs
    
    Initialize particles uniformly with the given number density on the whole domain.
    The domain considered is that of the \c ParticleVector.
    \c ObjectVector objects are not supported.
 */
class UniformIC : public InitialConditions
{
public:

    /** \brief Construct a \c UniformIC object
        \param [in] numDensity Number density of the particles to initialize
     */
    UniformIC(real numDensity);
    ~UniformIC();

    void exec(const MPI_Comm& comm, ParticleVector *pv, cudaStream_t stream) override;

private:
    real numDensity_;
};


} // namespace mirheo
