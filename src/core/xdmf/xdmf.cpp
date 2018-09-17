#include "xdmf.h"
#include "common.h"

#include "xmf_helpers.h"
#include "hdf5_helpers.h"

#include <hdf5.h>

#include <core/logger.h>

namespace XDMF
{
    void write(std::string filename, const Grid* grid, const std::vector<Channel>& channels, float time, MPI_Comm comm)
    {        
        std::string h5Filename  = filename + ".h5";
        std::string xmfFilename = filename + ".xmf";
        
        debug("Writing XDMF data to %s[.h5,.xmf]", filename.c_str());

        XMF::write(xmfFilename, h5Filename, comm, grid, channels, time);
        HDF5::write(h5Filename, comm, grid, channels);
    }
}