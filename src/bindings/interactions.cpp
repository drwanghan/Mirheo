#include <core/pvs/particle_vector.h>

#include <core/interactions/density.h>
#include <core/interactions/dpd.h>
#include <core/interactions/pairwise.h>
#include <core/interactions/dpd_with_stress.h>
#include <core/interactions/factory.h>
#include <core/interactions/interface.h>
#include <core/interactions/lj.h>
#include <core/interactions/mdpd.h>
#include <core/interactions/mdpd_with_stress.h>
#include <core/interactions/membrane.h>
#include <core/interactions/obj_rod_binding.h>
#include <core/interactions/rod.h>
#include <core/interactions/sdpd.h>

#include "bindings.h"
#include "class_wrapper.h"
#include "variant_cast.h"

using namespace pybind11::literals;

static std::map<std::string, InteractionFactory::VarParam>
castToMap(const py::kwargs& kwargs, const std::string& intName)
{
    std::map<std::string, InteractionFactory::VarParam> parameters;
    
    for (const auto& item : kwargs) {
        std::string key;
        try {
            key = py::cast<std::string>(item.first);
        }
        catch (const py::cast_error& e) {
            die("Could not cast one of the arguments in interaction '%s' to string", intName.c_str());
        }
        try {
            parameters[key] = py::cast<InteractionFactory::VarParam>(item.second);
        }
        catch (const py::cast_error& e) {
            die("Could not cast argument '%s' in interaction '%s': wrong type", key.c_str(), intName.c_str());
        }
    }
    return parameters;
}

static std::shared_ptr<InteractionMembrane>
createInteractionMembrane(const MirState *state, std::string name,
                          std::string shearDesc, std::string bendingDesc,
                          bool stressFree, float growUntil, py::kwargs kwargs)
{
    auto parameters = castToMap(kwargs, name);
    
    return InteractionFactory::createInteractionMembrane
        (state, name, shearDesc, bendingDesc, parameters, stressFree, growUntil);
}

static std::shared_ptr<InteractionRod>
createInteractionRod(const MirState *state, std::string name, std::string stateUpdateDesc, bool dumpEnergies, py::kwargs kwargs)
{
    auto parameters = castToMap(kwargs, name);
    
    return InteractionFactory::createInteractionRod(state, name, stateUpdateDesc, dumpEnergies, parameters);
}

static std::shared_ptr<PairwiseInteraction>
createPairwiseInteraction(const MirState *state, const std::string& name,
                          float rc, const std::string& kind, py::kwargs kwargs)
{
    auto parameters = castToMap(kwargs, name);
    return InteractionFactory::createPairwiseInteraction(state, name, rc, kind, parameters);
}

void exportInteractions(py::module& m)
{
    py::handlers_class<Interaction> pyInt(m, "Interaction", "Base interaction class");

    py::handlers_class<PairwiseInteraction> pyIntPairwise(m, "Pairwise", pyInt, R"(
        Generic pairwise interaction class. 
        Can be applied between any kind of :any:`ParticleVector` classes.
        The following interactions are currently implemented:

        * **DPD**: Pairwise interaction with conservative part and dissipative + random part acting as a thermostat, see [Groot1997]_
    
            .. math::
            
                \mathbf{F}_{ij} &= \mathbf{F}^C(\mathbf{r}_{ij}) + \mathbf{F}^D(\mathbf{r}_{ij}, \mathbf{u}_{ij}) + \mathbf{F}^R(\mathbf{r}_{ij}) \\
                \mathbf{F}^C(\mathbf{r}) &= \begin{cases} a(1-\frac{r}{r_c}) \mathbf{\hat r}, & r < r_c \\ 0, & r \geqslant r_c \end{cases} \\
                \mathbf{F}^D(\mathbf{r}, \mathbf{u}) &= \gamma w^2(\frac{r}{r_c}) (\mathbf{r} \cdot \mathbf{u}) \mathbf{\hat r} \\
                \mathbf{F}^R(\mathbf{r}) &= \sigma w(\frac{r}{r_c}) \, \theta \sqrt{\Delta t} \, \mathbf{\hat r}
        
            where bold symbol means a vector, its regular counterpart means vector length: 
            :math:`x = \left\lVert \mathbf{x} \right\rVert`, hat-ed symbol is the normalized vector:
            :math:`\mathbf{\hat x} = \mathbf{x} / \left\lVert \mathbf{x} \right\rVert`. Moreover, :math:`\theta` is the random variable with zero mean
            and unit variance, that is distributed independently of the interacting pair *i*-*j*, dissipation and random forces 
            are related by the fluctuation-dissipation theorem: :math:`\sigma^2 = 2 \gamma \, k_B T`; and :math:`w(r)` is the weight function
            that we define as follows:
        
            .. math::
            
                w(r) = \begin{cases} (1-r)^{p}, & r < 1 \\ 0, & r \geqslant 1 \end{cases}


        * **MDPD**: Compute MDPD interaction as described in [Warren2003].
            Must be used together with :any:`Density` interaction with kernel "MDPD".

            The interaction forces are the same as described in :any:`DPD` with the modified conservative term

            .. math::
    
                F^C_{ij} = a w_c(r_{ij}) + b (\rho_i + \rho_j) w_d(r_{ij}),
 
            where :math:`\rho_i` is computed from :any:`Density` and

            .. math::

                w_c(r) = \begin{cases} (1-\frac{r}{r_c}), & r < r_c \\ 0, & r \geqslant r_c \end{cases} \\
                w_d(r) = \begin{cases} (1-\frac{r}{r_d}), & r < r_d \\ 0, & r \geqslant r_d \end{cases}


        * **SDPD**: Compute SDPD interaction with angular momentum conservation.
            Must be used together with :any:`Density` interaction with the same density kernel.
    
            The available density kernels are listed in :any:`Density`.
            The available equations of state (EOS) are:

            Linear equation of state:

                .. math::

                    p(\rho) = c_S^2 \left(\rho - \rho_0 \right)
 
                where :math:`c_S` is the speed of sound and :math:`\rho_0` is a parameter.

            Quasi incompressible EOS:

                .. math::

                    p(\rho) = p_0 \left[ \left( \frac {\rho}{\rho_r} \right)^\gamma - 1 \right],

                where :math:`p_0`, :math:`\rho_r` and :math:`\gamma = 7` are parameters to be fitted to the desired fluid.


        * **RepulsiveLJ**: Pairwise interaction according to the classical `Lennard-Jones potential <https://en.wikipedia.org/wiki/Lennard-Jones_potential>`_
            The force however is truncated such that it is *always repulsive*.
        
            .. math::
            
                \mathbf{F}_{ij} = \max \left[ 0.0, 24 \epsilon \left( 2\left( \frac{\sigma}{r_{ij}} \right)^{14} - \left( \frac{\sigma}{r_{ij}} \right)^{8} \right) \right]


        * **Density**: Compute density of particles with a given kernel. 
    
            .. math::
            
                \rho_i = \sum\limits_{j \neq i} w_\rho (r_{ij})

            where the summation goes over the neighbours of particle :math:`i` within a cutoff range of :math:`r_c`.
            The implemented densities are listed below:


            kernel "MDPD":
            
                see [Warren2003]_
            
                .. math::
            
                    w_\rho(r) = \begin{cases} \frac{15}{2\pi r_d^3}\left(1-\frac{r}{r_d}\right)^2, & r < r_d \\ 0, & r \geqslant r_d \end{cases}
        
            kernel "WendlandC2":
            
                .. math::
    
                    w_\rho(r) = \frac{21}{2\pi} \left( 1 - \frac{r}{r_c} \right)^4 \left( 1 + 4 \frac{r}{r_c} \right)

            
        .. [Groot1997] Groot, R. D., & Warren, P. B. (1997).
            Dissipative particle dynamics: Bridging the gap between atomistic and mesoscopic simulations.
            J. Chem. Phys., 107(11), 4423-4435. `doi <https://doi.org/10.1063/1.474784>`_

        .. [Warren2003] Warren, P. B. 
           "Vapor-liquid coexistence in many-body dissipative particle dynamics."
           Physical Review E 68.6 (2003): 066702.`_


    )");

    pyIntPairwise.def(py::init(&createPairwiseInteraction),
                 "state"_a, "name"_a, "rc"_a, "kind"_a, R"(  
            Args:
                name: name of the interaction
                rc: interaction cut-off (no forces between particles further than **rc** apart)
                kind: interaction kind (e.g. DPD). See below for all possibilities.

            Create one pairwise interaction handler of kind **kind**.
            When applicable, stress computation is activated by passing **stress = True**.
            This activates virial stress computation every **stress_period** time units (also passed in **kwars**) 

            **kind = "DPD"**

                * **a**: :math:`a`
                * **gamma**: :math:`\gamma`
                * **kbt**: :math:`k_B T`
                * **power**: :math:`p` in the weight function
                
            **kind = "MDPD"**

                * **rd**: :math:`r_d`
                * **a**: :math:`a`
                * **b**: :math:`b`
                * **gamma**: :math:`\gamma`
                * **kbt**: temperature :math:`k_B T`
                * **power**: :math:`p` in the weight function


            **kind = "SDPD"**

                * **viscosity**: fluid viscosity
                * **kbt**: temperature :math:`k_B T`
                * **EOS**: the desired equation of state (see below) 
                * **density_kernel**: the desired density kernel (see below)


            **kind = "RepulsiveLJ"**

                * **epsilon**: :math:`\varepsilon`
                * **sigma**: :math:`\sigma`
                * **max_force**: force magnitude will be capped to not exceed **max_force**
                * **aware_mode:
                    * if "None", all particles interact with each other.
                    * if "Object", the particles belonging to the same object in an object vector do not interact with each other.
                      That restriction only applies if both Particle Vectors in the interactions are the same and is actually an Object Vector. 
                    * if "Rod", the particles interact with all other particles except with the ones which are below a given a distance
                      (in number of segment) of the same rod vector. The distance is specified by the kwargs parameter **min_segments_distance**.


            **kind = "Density"**

                * **density_kernel**: the desired density kernel (see below)

            The available density kernels are "MDPD" and "WendlandC2". Note that "MDPD" can not be used with SDPD interactions.
            MDPD interactions can use only "MDPD" density kernel.

            For SDPD, the available equation of states are given below:

            **EOS = "Linear" parameters:**

                * **sound_speed**: the speed of sound
                * **rho_0**: background pressure in :math:`c_S` units

            **EOS = "QuasiIncompressible" parameters:**

                * **p0**: :math:`p_0`
                * **rho_r**: :math:`\rho_r`
    )");
    
    pyIntPairwise.def("setSpecificPair", [](PairwiseInteraction *self, ParticleVector *pv1, ParticleVector *pv2, py::kwargs kwargs)
    {
        auto params = castToMap(kwargs, self->name);
        self->setSpecificPair(pv1, pv2, params);
    }, "pv1"_a, "pv2"_a, R"(
        Set specific parameters of a given interaction for a specific pair of :any:`ParticleVector`.
        This is useful when interactions only slightly differ between different pairs of :any:`ParticleVector`.
        The specific parameters should be set in the **kwargs** field, with same naming as in construction of the interaction.
        Note that only the values of the parameters can be modified, not the kernel types (e.g. change of density kernel is not supported in the case of SDPD interactions).
        
        Args:
            pv1: first :any:`ParticleVector`
            pv2: second :any:`ParticleVector`
    )");
    
    py::handlers_class<InteractionMembrane> pyMembraneForces(m, "MembraneForces", pyInt, R"(
        Abstract class for membrane interactions.
        Mesh-based forces acting on a membrane according to the model in [Fedosov2010]_

        The membrane interactions are composed of forces comming from:
            - bending of the membrane, potential :math:`U_b`
            - shear elasticity of the membrane, potential :math:`U_s`
            - constraint: area conservation of the membrane (local and global), potential :math:`U_A`
            - constraint: volume of the cell (assuming incompressible fluid), potential :math:`U_V`
            - membrane viscosity, pairwise force :math:`\mathbf{F}^v`
            - membrane fluctuations, pairwise force :math:`\mathbf{F}^R`

        The form of the constraint potentials are given by (see [Fedosov2010]_ for more explanations):

        .. math::

            U_A = \frac{k_a (A_{tot} - A^0_{tot})^2}{2 A^0_{tot}} + \sum_{j \in {1 ... N_t}} \frac{k_d (A_j-A_0)^2}{2A_0}, \\
            U_V = \frac{k_v (V-V^0_{tot})^2}{2 V^0_{tot}}.

        The viscous and dissipation forces are central forces and are the same as DPD interactions with :math:`w(r) = 1` 
        (no cutoff radius, applied to each bond).

        Several bending models are implemented. First, the Kantor enrgy reads (see [kantor1987]_):

        .. math::

            U_b = \sum_{j \in {1 ... N_s}} k_b \left[  1-\cos(\theta_j - \theta_0) \right].

        The Juelicher energy is (see [Juelicher1996]_):

        .. math::

            U_b = 2 k_b \sum_{\alpha = 1}^{N_v} \frac {\left( M_{\alpha} - C_0\right)^2}{A_\alpha}, \\
            M_{\alpha} = \frac 1 4 \sum_{<i,j>}^{(\alpha)} l_{ij} \theta_{ij}.

        It is improved with the ADE model (TODO: ref).

        Currently, the stretching and shear energy models are:

        WLC model:

        .. math::

            U_s = \sum_{j \in {1 ... N_s}} \left[ \frac {k_s l_m \left( 3x_j^2 - 2x_j^3 \right)}{4(1-x_j)} + \frac{k_p}{l_0} \right].

        Lim model: an extension of the Skalak shear energy (see [Lim2008]_).

        .. math::
        
            U_{Lim} =& \sum_{i=1}^{N_{t}}\left(A_{0}\right)_{i}\left(\frac{k_a}{2}\left(\alpha_{i}^{2}+a_{3} \alpha_{i}^{3}+a_{4} \alpha_{i}^{4}\right)\right.\\
                     & +\mu\left(\beta_{i}+b_{1} \alpha_{i} \beta_{i}+b_{2} \beta_{i}^{2}\right) ),

        where :math:`\alpha` and :math:`\beta` are the invariants of the strains.

        .. [Fedosov2010] Fedosov, D. A.; Caswell, B. & Karniadakis, G. E. 
                         A multiscale red blood cell model with accurate mechanics, rheology, and dynamics 
                         Biophysical journal, Elsevier, 2010, 98, 2215-2225

        .. [kantor1987] Kantor, Y. & Nelson, D. R. 
                        Phase transitions in flexible polymeric surfaces 
                        Physical Review A, APS, 1987, 36, 4020

        .. [Juelicher1996] Juelicher, Frank, and Reinhard Lipowsky. 
                           Shape transformations of vesicles with intramembrane domains.
                           Physical Review E 53.3 (1996): 2670.

        .. [Lim2008] Lim HW, Gerald, Michael Wortis, and Ranjan Mukhopadhyay. 
                     Red blood cell shapes and shape transformations: newtonian mechanics of a composite membrane: sections 2.1–2.4.
                     Soft Matter: Lipid Bilayers and Red Blood Cells 4 (2008): 83-139.
    )");

    pyMembraneForces.def(py::init(&createInteractionMembrane),
                         "state"_a, "name"_a, "shear_desc"_a, "bending_desc"_a,
                         "stress_free"_a=false, "grow_until"_a=0.f, R"( 
             Args:
                 name: name of the interaction
                 shear_desc: a string describing what shear force is used
                 bending_desc: a string describing what bending force is used
                 stress_free: if True, stress Free shape is used for the shear parameters
                 grow_until: the size increases linearly in time from half of the provided mesh 
                             to its full size after that time; the parameters are scaled accordingly with time

             kwargs:

                 * **tot_area**:   total area of the membrane at equilibrium
                 * **tot_volume**: total volume of the membrane at equilibrium
                 * **ka_tot**:     constraint energy for total area
                 * **kv_tot**:     constraint energy for total volume
                 * **kBT**:        fluctuation temperature (set to zero will switch off fluctuation forces)
                 * **gammaC**:     central component of dissipative forces
                 * **gammaT**:     tangential component of dissipative forces (warning: if non zero, the interaction will NOT conserve angular momentum)

             Shear Parameters, warm like chain model (set **shear_desc** = 'wlc'):

                 * **x0**:   :math:`x_0`
                 * **ks**:   energy magnitude for bonds
                 * **mpow**: :math:`m`
                 * **ka**:   energy magnitude for local area

             Shear Parameters, Lim model (set **shear_desc** = 'Lim'):

                 * **ka**: :math:`k_a`, magnitude of stretching force
                 * **mu**: :math:`\mu`, magnitude of shear force
                 * **a3**: :math:`a_3`, non linear part for stretching 
                 * **a4**: :math:`a_4`, non linear part for stretching 
                 * **b1**: :math:`b_1`, non linear part for shear
                 * **b2**: :math:`b_2`, non linear part for shear

             Bending Parameters, Kantor model (set **bending_desc** = 'Kantor'):

                 * **kb**:    local bending energy magnitude
                 * **theta**: spontaneous angle

             Bending Parameters, Juelicher model (set **bending_desc** = 'Juelicher'):

                 * **kb**:  local bending energy magnitude
                 * **C0**:  spontaneous curvature
                 * **kad**: area difference energy magnitude
                 * **DA0**: area difference at relaxed state divided by the offset of the leaflet midplanes
    )");


    py::handlers_class<ObjectRodBindingInteraction> pyObjRodBinding(m, "ObjRodBinding", pyInt, R"(
        Forces attaching a :any:`RodVector` to a :any:`RigidObjectVector`.
    )");

    pyObjRodBinding.def(py::init(&InteractionFactory::createInteractionObjRodBinding),
                        "state"_a, "name"_a, "torque"_a, "rel_anchor"_a, "k_bound"_a, R"(
            Args:
                name: name of the interaction
                torque: torque magnitude to apply to the rod
                rel_anchor: position of the anchor relative to the rigid object
                k_bound: anchor harmonic potential magnitude

    )");


    py::handlers_class<InteractionRod> pyRodForces(m, "RodForces", pyInt, R"(
        Forces acting on an elastic rod.

        The rod interactions are composed of forces comming from:
            - bending energy, :math:`E_{\text{bend}}`
            - twist energy, :math:`E_{\text{twist}}`
            - bounds energy,  :math:`E_{\text{bound}}`

        The form of the bending energy is given by (for a bi-segment):

        .. math::

            E_{\mathrm{bend}}=\frac{l}{4} \sum_{j=0}^{1}\left(\kappa^{j}-\overline{\kappa}\right)^{T} B\left(\kappa^{j}-\overline{\kappa}\right),

        where

        .. math::

            \kappa^{j}=\frac {1} {l} \left((\kappa \mathbf{b}) \cdot \mathbf{m}_{2}^{j},-(\kappa \mathbf{b}) \cdot \mathbf{m}_{1}^{j}\right).

        See, e.g. [bergou2008]_ for more details.
        The form of the twist energy is given by (for a bi-segment):

        .. math::

            E_{\mathrm{twist}}=\frac{k_{t} l}{2}\left(\frac{\theta^{1}-\theta^{0}}{l}-\overline{\tau}\right)^{2}.

        The additional bound energy is a simple harmonic potential with a given equilibrium length.

        .. [bergou2008] Bergou, M.; Wardetzky, M.; Robinson, S.; Audoly, B. & Grinspun, E. 
                        Discrete elastic rods 
                        ACM transactions on graphics (TOG), 2008, 27, 63

    )");

    pyRodForces.def(py::init(&createInteractionRod),
                    "state"_a, "name"_a, "state_update"_a="none", "save_energies"_a=false, R"( 
             Args:
                 name: name of the interaction
                 state_update: description of the state update method; only makes sense for multiple states. See below for possible choices.
                 save_energies: if `True`, save the energies of each bisegment

             kwargs:

                 * **a0** (float):           equilibrium length between 2 opposite cross vertices
                 * **l0** (float):           equilibrium length between 2 consecutive vertices on the centerline 
                 * **k_s_center** (float):   elastic force magnitude for centerline
                 * **k_s_frame** (float):    elastic force magnitude for material frame particles
                 * **k_bending** (float3):   Bending symmetric tensor :math:`B` in the order :math:`\left(B_{xx}, B_{xy}, B_{zz} \right)`
                 * **kappa0** (float2):      Spontaneous curvatures along the two material frames :math:`\overline{\kappa}`
                 * **k_twist** (float):      Twist energy magnitude :math:`k_\mathrm{twist}`
                 * **tau0** (float):         Spontaneous twist :math:`\overline{\tau}`
                 * **E0** (float):           (optional) energy ground state

             state update parameters, for **state_update** = 'smoothing':

                 (not fully implemented yet; for now just takes minimum state but no smoothing term)

             state update parameters, for **state_update** = 'spin':

                 * **nsteps** number of MC step per iteration
                 * **kBT** temperature used in the acceptance-rejection algorithm
                 * **J** neighbouring spin 'dislike' energy

             The interaction can support multiple polymorphic states if **kappa0**, **tau0** and **E0** are lists of equal size.
             In this case, the **E0** parameter is required.
             Only lists of 1, 2 and 11 states are supported.
    )");
}

