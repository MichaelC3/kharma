/* 
 *  File: types.hpp
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
#pragma once

#include "decs.hpp"

#include "boundary_types.hpp"
#include "kharma_package.hpp"

#include <parthenon/parthenon.hpp>

using namespace parthenon;

using parthenon::MeshBlockData;

/**
 * Types, macros, and convenience functions
 * 
 * Anything potentially useful throughout KHARMA, but specific to it
 * (general copy/pastes from StackOverflow go in kharma_utils.hpp)
 */

// This provides a way of addressing vectors that matches
// directions, to make derivatives etc more readable
// TODO Spammy to namespace. Keep?
#define V1 0
#define V2 1
#define V3 2

// Struct for derived 4-vectors at a point, usually calculated and needed together
typedef struct {
    Real ucon[GR_DIM];
    Real ucov[GR_DIM];
    Real bcon[GR_DIM];
    Real bcov[GR_DIM];
} FourVectors;

typedef struct {
    IndexRange ib;
    IndexRange jb;
    IndexRange kb;
} IndexRange3;

/**
 * Map of the locations of particular variables in a VariablePack
 * Used for operations conducted over all vars which must still
 * distinguish between them, e.g. flux.hpp
 *
 * We use this instead of the PackIndexMap, because comparing strings
 * on the device every time we need the index of a variable is slow.
 *
 * Think of this a bit like the macros in iharm2d or 3d, except
 * that since we're packing on the fly, they can't be globally
 * constant anymore.
 *
 * Note the values of any variables not present in the pack will be -1
 */
class VarMap {
    public:
        // Use int8. 127 values ought to be enough for anybody, right?
        // Basic primitive variables
        int8_t RHO, UU, U1, U2, U3, B1, B2, B3;
        // Tracker variables
        int8_t RHO_ADDED, UU_ADDED, PASSIVE;
        // Electron entropy/energy tracking
        int8_t KTOT, K_CONSTANT, K_HOWES, K_KAWAZURA, K_WERNER, K_ROWAN, K_SHARMA;
        // Implicit-solver variables: constraint damping, EGRMHD
        int8_t PSI, Q, DP;
        // Total struct size ~20 bytes, < 1 vector of 4 doubles

        VarMap(parthenon::PackIndexMap& name_map, bool is_cons)
        {
            if (is_cons) {
                // HD
                RHO = name_map["cons.rho"].first;
                UU = name_map["cons.u"].first;
                U1 = name_map["cons.uvec"].first;
                // B
                B1 = name_map["cons.B"].first;
                PSI = name_map["cons.psi_cd"].first;
                // Floors
                RHO_ADDED = name_map["cons.rho_added"].first;
                UU_ADDED = name_map["cons.u_added"].first;
                // Electrons
                KTOT = name_map["cons.Ktot"].first;
                K_CONSTANT = name_map["cons.Kel_Constant"].first;
                K_HOWES = name_map["cons.Kel_Howes"].first;
                K_KAWAZURA = name_map["cons.Kel_Kawazura"].first;
                K_WERNER = name_map["cons.Kel_Werner"].first;
                K_ROWAN = name_map["cons.Kel_Rowan"].first;
                K_SHARMA = name_map["cons.Kel_Sharma"].first;
                // Extended MHD
                Q = name_map["cons.q"].first;
                DP = name_map["cons.dP"].first;
            } else {
                // HD
                RHO = name_map["prims.rho"].first;
                UU = name_map["prims.u"].first;
                U1 = name_map["prims.uvec"].first;
                // B
                B1 = name_map["prims.B"].first;
                PSI = name_map["prims.psi_cd"].first;
                // Floors (TODO cons only?)
                RHO_ADDED = name_map["prims.rho_added"].first;
                UU_ADDED = name_map["prims.u_added"].first;
                // Electrons
                KTOT = name_map["prims.Ktot"].first;
                K_CONSTANT = name_map["prims.Kel_Constant"].first;
                K_HOWES = name_map["prims.Kel_Howes"].first;
                K_KAWAZURA = name_map["prims.Kel_Kawazura"].first;
                K_WERNER = name_map["prims.Kel_Werner"].first;
                K_ROWAN = name_map["prims.Kel_Rowan"].first;
                K_SHARMA = name_map["prims.Kel_Sharma"].first;
                // Extended MHD
                Q = name_map["prims.q"].first;
                DP = name_map["prims.dP"].first;
            }
            U2 = U1 + 1;
            U3 = U1 + 2;
            B2 = B1 + 1;
            B3 = B1 + 2;
        }
        
};

/**
 * Functions for checking boundaries in 3D.
 * Uses IndexRange objects, or this would be in kharma_utils.hpp
 */
KOKKOS_INLINE_FUNCTION bool outside(const int& k, const int& j, const int& i,
                                    const IndexRange& kb, const IndexRange& jb, const IndexRange& ib)
{
    return (i < ib.s) || (i > ib.e) || (j < jb.s) || (j > jb.e) || (k < kb.s) || (k > kb.e);
}
KOKKOS_INLINE_FUNCTION bool inside(const int& k, const int& j, const int& i,
                                   const IndexRange& kb, const IndexRange& jb, const IndexRange& ib)
{
    // This is faster in the case that the point is outside
    return !outside(k, j, i, kb, jb, ib);
}

/**
 * Get zones which are inside the physical domain, i.e. set by computation or MPI halo sync,
 * not by problem boundary conditions. 
 */
inline IndexRange3 GetPhysicalZones(std::shared_ptr<MeshBlock> pmb, IndexShape& bounds)
{
    using KBoundaries::IsPhysicalBoundary;
    return IndexRange3{IndexRange{IsPhysicalBoundary(pmb, BoundaryFace::inner_x1)
                                    ? bounds.is(IndexDomain::interior)
                                    : bounds.is(IndexDomain::entire),
                                  IsPhysicalBoundary(pmb, BoundaryFace::outer_x1)
                                    ? bounds.ie(IndexDomain::interior)
                                    : bounds.ie(IndexDomain::entire)},
                       IndexRange{IsPhysicalBoundary(pmb, BoundaryFace::inner_x2)
                                    ? bounds.js(IndexDomain::interior)
                                    : bounds.js(IndexDomain::entire),
                                  IsPhysicalBoundary(pmb, BoundaryFace::outer_x2)
                                    ? bounds.je(IndexDomain::interior)
                                    : bounds.je(IndexDomain::entire)},
                       IndexRange{IsPhysicalBoundary(pmb, BoundaryFace::inner_x3)
                                    ? bounds.ks(IndexDomain::interior)
                                    : bounds.ks(IndexDomain::entire),
                                  IsPhysicalBoundary(pmb, BoundaryFace::outer_x3)
                                    ? bounds.ke(IndexDomain::interior)
                                    : bounds.ke(IndexDomain::entire)}};
}

#if DEBUG
/**
 * Function to generate outputs wherever, whenever.
 */
inline void OutputNow(Mesh *pmesh, std::string name)
{
    auto tm = SimTime(0., 0., 0, 0, 0, 0, 0.);
    auto pouts = std::make_unique<Outputs>(pmesh, pin, &tm);
    auto pin = pmesh->packages.Get("Globals")->Param<ParameterInput>("pin");
    pouts->MakeOutputs(pmesh, pin, &tm, SignalHandler::OutputSignal::now);
    // TODO: find most recently written "now" files and move them to "name"
}
#endif

/**
 * Functions for "tracing" execution by printing strings at each entry/exit.
 * Normally, they profile the code, but they can print a nested execution trace.
 * 
 * Don't laugh at my dumb mutex, it works.
 */
#if TRACE
// Can we namespace these?
extern int kharma_debug_trace_indent;
extern int kharma_debug_trace_mutex;
#define MAX_INDENT_SPACES 160
inline void Flag(std::string label)
{
    if(MPIRank0()) {
        int& indent = kharma_debug_trace_indent;
        int& mutex = kharma_debug_trace_mutex;
        // If no other thread is printing one of these...
        while (mutex != 0);
        // ... take the mutex and print
        mutex = 1;
        char tab[MAX_INDENT_SPACES] = {0};
        // Make very sure the indent does not exceed the available space.
        // Forgetting EndFlag() is easy and buffer overflows are bad.
        indent = m::max(m::min(indent, MAX_INDENT_SPACES/2), 0);
        for (int i=0; i < indent; i++) tab[i*2] = tab[i*2+1] = ' ';
        // Print everything in one call so we have the best chance of coherence
        fprintf(stderr, "%sStarting %s\n", tab, label.c_str());
        indent = m::min(indent++, MAX_INDENT_SPACES/2);
        // Release mutex
        mutex = 0;
    }
}
inline void EndFlag()
{
    if(MPIRank0()) {
        int& indent = kharma_debug_trace_indent;
        int& mutex = kharma_debug_trace_mutex;
        while (mutex != 0);
        mutex = 1;
        indent = m::min(m::max(indent--, 0), MAX_INDENT_SPACES/2);
        char tab[MAX_INDENT_SPACES] = {0};
        for (int i=0; i < indent; i++) tab[i*2] = tab[i*2+1] = ' ';
        fprintf(stderr, "%sDone\n", tab);
        mutex = 0;
    }
}
#else
inline void Flag(std::string label)
{
    Kokkos::Profiling::pushRegion(label);
}
inline void EndFlag()
{
    Kokkos::Profiling::popRegion();
}
#endif
