#pragma once

#include <mirheo/plugins/interface.h>
#include <string>
#include <functional>

#include <mirheo/core/utils/folders.h>

namespace mirheo
{

class RigidObjectVector;

class MagneticOrientationPlugin : public SimulationPlugin
{
public:

    using UniformMagneticFunc = std::function<real3(real)>;
    
    MagneticOrientationPlugin(const MirState *state, std::string name, std::string rovName, real3 moment, UniformMagneticFunc magneticFunction);

    void setup(Simulation* simulation, const MPI_Comm& comm, const MPI_Comm& interComm) override;
    void beforeForces(cudaStream_t stream) override;

    bool needPostproc() override { return false; }

private:
    std::string rovName;
    RigidObjectVector* rov;
    real3 moment;
    UniformMagneticFunc magneticFunction;
};

} // namespace mirheo
