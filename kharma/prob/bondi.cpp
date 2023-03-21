/* 
 *  File: bondi.cpp
 *  
 *  BSD 3-Clause License
 *  
 *  Copyright (c) 2020, AFD Group at UIUC
 *  All rights reserved.
 *  
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  
 *  1. Redistributions of source code must retain the above copyright notice, this
 *     list of conditions and the following disclaimer.
 *  
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  
 *  3. Neither the name of the copyright holder nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *  
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bondi.hpp"

#include "floors.hpp"
#include "flux_functions.hpp"

/**
 * Initialization of a Bondi problem with specified sonic point, BH mdot, and horizon radius
 * TODO mdot and rs are redundant and should be merged into one parameter. Uh, no.
 */
TaskStatus InitializeBondi(std::shared_ptr<MeshBlockData<Real>>& rc, ParameterInput *pin)
{
    Flag(rc, "Initializing Bondi problem");
    auto pmb = rc->GetBlockPointer();

    const Real mdot = pin->GetOrAddReal("bondi", "mdot", 1.0);
    const Real rs = pin->GetOrAddReal("bondi", "rs", 8.0);

    // Set the innermost radius to apply the Bondi problem initialization
    // By default, stay away from the outer BL coordinate singularity
    const Real a = pin->GetReal("coordinates", "a");
    const Real rin_bondi_default = 1 + m::sqrt(1 - a*a) + 0.1;
    // TODO take r_shell
    const Real rin_bondi = pin->GetOrAddReal("bondi", "r_in", rin_bondi_default);


    // Add these to package properties, since they continue to be needed on boundaries
    // TODO Problems need params
    if(! pmb->packages.Get("GRMHD")->AllParams().hasKey("mdot"))
        pmb->packages.Get("GRMHD")->AddParam<Real>("mdot", mdot);
    if(! pmb->packages.Get("GRMHD")->AllParams().hasKey("rs"))
        pmb->packages.Get("GRMHD")->AddParam<Real>("rs", rs);
    if(! pmb->packages.Get("GRMHD")->AllParams().hasKey("rin_bondi"))
        pmb->packages.Get("GRMHD")->AddParam<Real>("rin_bondi", rin_bondi);

    // Set this problem to control the outer X1 boundary by default
    // remember to disable inflow_check in parameter file!
    auto bound_pkg = static_cast<KHARMAPackage*>(pmb->packages.Get("Boundaries").get());
    if (pin->GetOrAddBoolean("bondi", "set_outer_bound", true)) {
        bound_pkg->KHARMAOuterX1Boundary = SetBondi;
    }
    if (pin->GetOrAddBoolean("bondi", "set_inner_bound", false)) {
        bound_pkg->KHARMAInnerX1Boundary = SetBondi;
    }

    // Set the interior domain to the analytic solution to begin
    // This tests that PostInitialize will correctly fill ghost zones with the boundary we set
    SetBondi(rc, IndexDomain::interior);

    if (rin_bondi > pin->GetReal("coordinates", "r_in")) {
        // Apply floors to initialize the rest of the domain (regardless of the 'disable_floors' param)
        // Bondi's BL coordinates do not like the EH, so we replace the zeros with something reasonable.
        Floors::ApplyInitialFloors(rc.get(), IndexDomain::interior);
    }

    Flag(rc, "Initialized");
    return TaskStatus::complete;
}

TaskStatus SetBondi(std::shared_ptr<MeshBlockData<Real>>& rc, IndexDomain domain, bool coarse)
{
    Flag(rc, "Setting Bondi zones");
    auto pmb = rc->GetBlockPointer();

    //std::cerr << "Bondi on domain: " << BoundaryName(domain) << std::endl;

    PackIndexMap prims_map, cons_map;
    auto P = GRMHD::PackMHDPrims(rc.get(), prims_map);
    auto U = GRMHD::PackMHDCons(rc.get(), cons_map);
    const VarMap m_u(cons_map, true), m_p(prims_map, false);

    const Real mdot = pmb->packages.Get("GRMHD")->Param<Real>("mdot");
    const Real rs = pmb->packages.Get("GRMHD")->Param<Real>("rs");
    const Real gam = pmb->packages.Get("GRMHD")->Param<Real>("gamma");
    const Real rin_bondi = pmb->packages.Get("GRMHD")->Param<Real>("rin_bondi");

    const EMHD::EMHD_parameters& emhd_params = EMHD::GetEMHDParameters(pmb->packages);

    // Just the X1 right boundary
    GRCoordinates G = pmb->coords;
    SphKSCoords ks = mpark::get<SphKSCoords>(G.coords.base);
    SphBLCoords bl = SphBLCoords(ks.a);
    CoordinateEmbedding cs = G.coords;

    // Solution constants
    // These don't depend on which zone we're calculating
    const Real n = 1. / (gam - 1.);
    const Real uc = m::sqrt(1. / (2. * rs));
    const Real Vc = m::sqrt(uc * uc / (1. - 3. * uc * uc));
    const Real Tc = -n * Vc * Vc / ((n + 1.) * (n * Vc * Vc - 1.));
    const Real C1 = uc * rs * rs * m::pow(Tc, n);
    const Real A = 1. + (1. + n) * Tc;
    const Real C2 = A * A * (1. - 2. / rs + uc * uc);
    const Real K  = m::pow(4 * M_PI * C1 / mdot, 1/n);
    const Real Kn = m::pow(K, n);

    // Set the Bondi conditions wherever we're asked
    auto bounds = coarse ? pmb->c_cellbounds : pmb->cellbounds;

    const IndexRange ib = bounds.GetBoundsI(domain);
    const IndexRange jb = bounds.GetBoundsJ(domain);
    const IndexRange kb = bounds.GetBoundsK(domain);
    pmb->par_for("bondi_boundary", kb.s, kb.e, jb.s, jb.e, ib.s, ib.e,
        KOKKOS_LAMBDA (const int &k, const int &j, const int &i) {
            GReal Xnative[GR_DIM], Xembed[GR_DIM];
            G.coord(k, j, i, Loci::center, Xnative);
            G.coord_embed(k, j, i, Loci::center, Xembed);
            GReal r = Xembed[1];
            // Unless we're doing a Schwarzchild problem & comparing solutions,
            // be a little cautious about initializing the Ergosphere zones
            if (r < rin_bondi) return;

            const Real T = get_T(r, C1, C2, n, rs);
            const Real Tn = m::pow(T, n);
            const Real ur = -C1 / (Tn * r * r);
            const Real rho = Tn / Kn;
            const Real u = rho * T * n;

            // Set u^t to make u^r a 4-vector
            Real ucon_bl[GR_DIM] = {0, ur, 0, 0};
            Real gcov_bl[GR_DIM][GR_DIM];
            bl.gcov_embed(Xembed, gcov_bl);
            set_ut(gcov_bl, ucon_bl);

            // Then transform that 4-vector to KS, then to native
            Real ucon_ks[GR_DIM], ucon_mks[GR_DIM];
            ks.vec_from_bl(Xembed, ucon_bl, ucon_ks);
            cs.con_vec_to_native(Xnative, ucon_ks, ucon_mks);

            // Convert native 4-vector to primitive u-twiddle, see Gammie '04
            Real gcon[GR_DIM][GR_DIM], u_prim[NVEC];
            G.gcon(Loci::center, j, i, gcon);
            fourvel_to_prim(gcon, ucon_mks, u_prim);

            // This used to have NaN guards. No point, as for optimized builds they are ignored (!)
            // Now we just avoid initializing near the EH
            P(m_p.RHO, k, j, i) = rho;
            P(m_p.UU, k, j, i) = u;
            P(m_p.U1, k, j, i) = u_prim[0];
            P(m_p.U2, k, j, i) = u_prim[1];
            P(m_p.U3, k, j, i) = u_prim[2];
        }
    );

    Flag(rc, "Set");
    return TaskStatus::complete;
}
