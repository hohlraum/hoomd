/*
Highly Optimized Object-oriented Many-particle Dynamics -- Blue Edition
(HOOMD-blue) Open Source Software License Copyright 2009-2015 The Regents of
the University of Michigan All rights reserved.

HOOMD-blue may contain modifications ("Contributions") provided, and to which
copyright is held, by various Contributors who have granted The Regents of the
University of Michigan the right to modify and/or distribute such Contributions.

You may redistribute, use, and create derivate works of HOOMD-blue, in source
and binary forms, provided you abide by the following conditions:

* Redistributions of source code must retain the above copyright notice, this
list of conditions, and the following disclaimer both in the code and
prominently in any materials provided with the distribution.

* Redistributions in binary form must reproduce the above copyright notice, this
list of conditions, and the following disclaimer in the documentation and/or
other materials provided with the distribution.

* All publications and presentations based on HOOMD-blue, including any reports
or published results obtained, in whole or in part, with HOOMD-blue, will
acknowledge its use according to the terms posted at the time of submission on:
http://codeblue.umich.edu/hoomd-blue/citations.html

* Any electronic documents citing HOOMD-Blue will link to the HOOMD-Blue website:
http://codeblue.umich.edu/hoomd-blue/

* Apart from the above required attributions, neither the name of the copyright
holder nor the names of HOOMD-blue's contributors may be used to endorse or
promote products derived from this software without specific prior written
permission.

Disclaimer

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND/OR ANY
WARRANTIES THAT THIS SOFTWARE IS FREE OF INFRINGEMENT ARE DISCLAIMED.

IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Maintainer: joaander

#include "Compute.h"
#include "GPUArray.h"
#include "GPUVector.h"
#include "GPUFlags.h"
#include "Index1D.h"

#include <boost/shared_ptr.hpp>
#include <boost/signals2.hpp>
#include <vector>

/*! \file NeighborList.h
    \brief Declares the NeighborList class
*/

#ifdef NVCC
#error This header cannot be compiled by nvcc
#endif

#ifndef __NEIGHBORLIST_H__
#define __NEIGHBORLIST_H__

#ifdef ENABLE_MPI
#include "Communicator.h"
#endif

//! Computes a Neighborlist from the particles
/*! \b Overview:

    A particle \c i is a neighbor of particle \c j if the distance between
    particle them is less than or equal to \c r_cut(i,j). The neighborlist for a given particle
    \c i includes all of these neighbors at a minimum. Other particles are included
    in the list: those up to \c r_list(i,j) which includes a buffer distance so that the neighbor list
    doesn't need to be updated every step.

    There are two ways of storing this information. One is to store only half of the
    neighbors (only those with i < j), and the other is to store all neighbors. There are
    potential tradeoffs between number of computations and memory access complexity for
    each method. NeighborList supports both of these modes via a switch: setStorageMode();

    Some classes with either setting, full or half, but they are faster with the half setting. However,
    others may require that the neighbor list storage mode is set to full.

    <b>Data access:</b>

    Up to Nmax neighbors can be stored for each particle. Data is stored in a flat array in memory. A secondary
    flat list is supplied for each particle which specifies where to start reading neighbors from the list
    (a "head" list). Each element in the list stores the index of the neighbor with the highest bits reserved for flags.
    The head list for accessing elements can be gotten with getHeadList()
    and the array itself can be accessed with getNlistArray().

    The number of neighbors for each particle is stored in an auxilliary array accessed with getNNeighArray().

     - <code>jf = nlist[head_list[i] + n]</code> is the index of neighbor \a n of particle \a i, where \a n can vary from
       0 to <code>n_neigh[i] - 1</code>

    \a jf includes flags in the highest bits. The format and use of these flags are yet to be determined.

    \b Filtering:

    By default, a neighbor list includes all particles within a single cutoff distance r_cut. Various filters can be
    applied to remove unwanted neighbors from the list.
     - setFilterBody() prevents two particles of the same body from being neighbors
     - setDiameterShift() enables slj type diameter shifting, where a single minimum cutoff is used and the actual
       r_cut(i,j) is shifted by the average diameter of the particles (d_i + d_j)/2 -1 (such that no shift is applied
       when d_i = d_j = 1

    \b Algorithms:

    This base class supplys no build algorithm for generating this list, it must be overridden by deriving classes.
    Derived classes implement O(N) efficient straetegies using a CellList or a BVH tree.

    <b>Needs update check:</b>

    When compute() is called, the neighbor list is updated, but only if it needs to be. Checks
    are performed to see if any particle has moved more than half of the buffer distance, and
    only then is the list actually updated. This check can even be avoided for a number of time
    steps by calling setEvery(). If the caller wants to force a full update, forceUpdate()
    can be called before compute() to do so. Note that if the particle data is resorted,
    an update is automatically forced.

    The CUDA profiler expects the exact same sequence of kernels on every run. Due to the non-deterministic cell list,
    a different sequence of calls may be generated with nlist builds at different times. To work around this problem
    setEvery takes a dist_check parameter. When dist_check=True, the above described behavior is followed. When
    dist_check is false, the nlist is built exactly m_every steps. This is intended for use in profiling only.

    \b Exclusions:

    Exclusions are stored in \a ex_list, a data structure similar in structure to \a nlist, except this time exclusions
    are stored. User-specified exclusions are stored by tag and translated to indices whenever a particle sort occurs
    (updateExListIdx()). If any exclusions are set, filterNlist() is called after buildNlist(). filterNlist() loops
    through the neighbor list and removes any particles that are excluded. This allows an arbitrary number of exclusions
    to be processed without slowing the performance of the buildNlist() step itself.

    <b>Overvlow handling:</b>
    For easy support of derived GPU classes to implement overflow detection the overflow condition is stored in the
    GPUArray \a d_conditions.

     - 0: Maximum nlist size (implementations are free to write to this element only in overflow conditions if they
          choose.)
     - Further indices may be added to handle other conditions at a later time.

    Condition flags are to be set during the buildNlist() call and will be checked by compute() which will then
    take the appropriate action.

    \ingroup computes
*/
class NeighborList : public Compute
    {
    public:
        //! Simple enum for the storage modes
        enum storageMode
            {
            half,   //!< Only neighbors i,j are stored where i < j
            full    //!< All neighbors are stored
            };

        //! Constructs the compute
        NeighborList(boost::shared_ptr<SystemDefinition> sysdef, Scalar _r_cut, Scalar r_buff);

        //! Destructor
        virtual ~NeighborList();

        //! \name Set parameters
        // @{
        
        //! Change the cutoff radius for all pairs
        virtual void setRCut(Scalar r_cut, Scalar r_buff);
        
        //! Change the cutoff radius by pair
        virtual void setRCutPair(unsigned int typ1, unsigned int typ2, Scalar r_cut);
        
        //! Change the global buffer radius
        virtual void setRBuff(Scalar r_buff);

        //! Change how many timesteps before checking to see if the list should be rebuilt
        /*! \param every Number of time steps to wait before beignning to check if particles have moved a sufficient distance
                   to require a neighbor list upate.
            \param dist_check Set to false to enforce nlist builds exactly \a every steps
        */
        void setEvery(unsigned int every, bool dist_check=true)
            {
            m_every = every;
            m_dist_check = dist_check;
            forceUpdate();
            }

        //! Set the storage mode
        /*! \param mode Storage mode to set
            - half only stores neighbors where i < j
            - full stores all neighbors

            The neighborlist is not immediately updated to reflect this change. It will take effect
            when compute is called for the next timestep.
        */
        void setStorageMode(storageMode mode)
            {
            m_storage_mode = mode;
            forceUpdate();
            }

        // @}
        //! \name Get properties
        // @{

        //! Get the storage mode
        storageMode getStorageMode()
            {
            return m_storage_mode;
            }

        //! Get the maximum of all rcut
        Scalar getMaxRCut()
            {
            if (m_rcut_changed) updateRList();
            return m_rcut_max_max;
            }

        //! Get the maximum of all the rlist
        Scalar getMaxRList()
            {
            Scalar max_rlist = getMaxRCut() + m_r_buff;
            if (m_diameter_shift)
                max_rlist += m_d_max - Scalar(1.0);
            return max_rlist;
            }

        //! Get the minimum of all rcut
        Scalar getMinRCut()
            {
            if (m_rcut_changed) updateRList();
            return m_rcut_min;
            }

        //! Get the minimum of all rlist
        Scalar getMinRList()
            {
            Scalar min_rlist = getMinRCut() + m_r_buff;
            if (m_diameter_shift)
                min_rlist += m_d_max - Scalar(1.0);
            return min_rlist;
            }

        // @}
        //! \name Statistics
        // @{

        //! Print statistics on the neighborlist
        virtual void printStats();

        //! Clear the count of updates the neighborlist has performed
        virtual void resetStats();

        //! Gets the shortest rebuild period this nlist has experienced since a call to resetStats
        unsigned int getSmallestRebuild();

        // @}
        //! \name Get data
        // @{

        //! Get the number of neighbors array
        const GPUArray<unsigned int>& getNNeighArray()
            {
            return m_n_neigh;
            }

        //! Get the neighbor list
        const GPUArray<unsigned int>& getNListArray()
            {
            return m_nlist;
            }
            
        //! Get the head list
        const GPUArray<unsigned int>& getHeadList()
            {
            return m_head_list;
            }

        //! Get the number of exclusions array
        const GPUArray<unsigned int>& getNExArray()
            {
            return m_n_ex_idx;
            }

         //! Get the exclusion list
         const GPUArray<unsigned int>& getExListArray()
            {
            return m_ex_list_idx;
            }

        //! Get the neighbor list indexer
        /*! \note Do not save indexers across calls. Get a new indexer after every call to compute() - they will
            change.
        */
        const Index2D& getExListIndexer()
            {
            return m_ex_list_indexer;
            }

        bool getExclusionsSet()
            {
            return m_exclusions_set;
            }

        bool wantExclusions()
            {
            return m_need_reallocate_exlist;
            }

        //! Gives an estimate of the number of nearest neighbors per particle
        virtual Scalar estimateNNeigh();

        // @}
        //! \name Handle exclusions
        // @{

        //! Exclude a pair of particles from being added to the neighbor list
        void addExclusion(unsigned int tag1, unsigned int tag2);

        //! Clear all existing exclusions
        void clearExclusions();

        //! Collect some statistics on exclusions.
        void countExclusions();

        //! Get number of exclusions involving n particles
        /*! \param n Size of the exclusion
         * \returns Number of excluded particles
         */
        unsigned int getNumExclusions(unsigned int size);

        //! Add an exclusion for every bond in the ParticleData
        void addExclusionsFromBonds();

        //! Add exclusions from angles
        void addExclusionsFromAngles();

        //! Add exclusions from dihedrals
        void addExclusionsFromDihedrals();

        //! Test if an exclusion has been made
        bool isExcluded(unsigned int tag1, unsigned int tag2);

        //! Add an exclusion for every 1,3 pair
        void addOneThreeExclusionsFromTopology();

        //! Add an exclusion for every 1,4 pair
        void addOneFourExclusionsFromTopology();

        //! Enable/disable body filtering
        virtual void setFilterBody(bool filter_body)
            {
            // only set the body exclusions if there are bodies in the rigid data, otherwise it just wastes time
            if (m_sysdef->getRigidData()->getNumBodies() > 0)
                {
                m_filter_body = filter_body;
                forceUpdate();
                }
            }

        //! Test if body filtering is set
        virtual bool getFilterBody()
            {
            return m_filter_body;
            }
        
        //! Enable/disable diameter shifting
        /*!
         * If diameter shifting is enabled, a value (d_i + d_j)/2.0 - 1.0 is added to r_cut(i,j) for
         * inclusion in the neighbor list (where d_i and d_j are the diameters). This is useful in simulations
         * where there is only a single particle type, but each particle may have a different diameter, and
         * the potential (and its cutoff) depends on this diameter (i.e. shifted Lennard-Jones).
         */
        virtual void setDiameterShift(bool diameter_shift)
            {
            m_diameter_shift = diameter_shift;
            m_rcut_signal();
            forceUpdate();
            }
            
        //! Test if diameter shifting is set
        virtual bool getDiameterShift()
            {
            return m_diameter_shift;
            }

        //! Set the maximum diameter to use in computing neighbor lists
        /*!
         * If diameter shifting is enabled, then this sets the maximum query radius for inclusion in the neighborlist.
         * The shift (d_i + d_j)/2.0 - 1.0 can be no bigger than d_max - 1.0.
         */
        virtual void setMaximumDiameter(Scalar d_max)
            {
            m_d_max = d_max;
            m_rcut_signal();
            forceUpdate();
            }

        //! Get the maximum diameter value
        Scalar getMaximumDiameter()
            {
            return m_d_max;
            }

        //! Return the requested ghost layer width
        virtual Scalar getGhostLayerWidth(unsigned int type) const
            {
            ArrayHandle<Scalar> h_rcut_max(m_rcut_max, access_location::host, access_mode::read);
            const Scalar rcut_max_i = h_rcut_max.data[type];

            if (rcut_max_i > Scalar(0.0)) // ensure communication is required
                {
                Scalar rmax = rcut_max_i + m_r_buff;

                // diameter shifting requires to communicate a larger rlist
                if (m_diameter_shift)
                    rmax += m_d_max - Scalar(1.0);
                return rmax;
                }
            else
                {
                return Scalar(0.0);
                }
            }

        // @}

        //! Computes the NeighborList if it needs updating
        void compute(unsigned int timestep);

        //! Benchmark the neighbor list
        virtual double benchmark(unsigned int num_iters);

        //! Forces a full update of the list on the next call to compute()
        void forceUpdate()
            {
            m_force_update = true;
            }

        //! Get the number of updates
        virtual unsigned int getNumUpdates()
            {
            return m_updates + m_forced_updates;
            }


#ifdef ENABLE_MPI
        //! Set the communicator to use
        /*! \param comm MPI communication class
         */
        virtual void setCommunicator(boost::shared_ptr<Communicator> comm);

        //! Returns true if the particle migration criterium is fulfilled
        /*! \param timestep The current timestep
         */
        bool peekUpdate(unsigned int timestep);
#endif

        //! Return true if the neighbor list has been updated this time step
        /*! \param timestep Current time step
         *
         *  This is supposed to be called after a call to compute().
         */
        bool hasBeenUpdated(unsigned int timestep)
            {
            return m_last_updated_tstep == timestep && m_has_been_updated_once;
            }

        /*! \param func Function to call when the cutoff radius changes
            \return Connection to manage the signal/slot connection
            Calls are performed by using boost::signals2. The function passed in
            \a func will be called every time the NeighborList is notified of a change in cutoff radius
            \note If the caller class is destroyed, it needs to disconnect the signal connection
            via \b con.disconnect where \b con is the return value of this function.
        */
        boost::signals2::connection connectRCutChange(const boost::function<void ()> &func)
            {
            return m_rcut_signal.connect(func);
            }

   protected:
        Index2D m_typpair_idx;      //!< Indexer for full type pair storage
        GPUArray<Scalar> m_r_cut;   //!< The potential cutoffs stored by pair type
        GPUArray<Scalar> m_r_listsq;//!< The neighborlist cutoff radius squared stored by pair type
        GPUArray<Scalar> m_rcut_max;//!< The maximum value of rcut per particle type
        Scalar m_rcut_max_max;      //!< The maximum cutoff radius of any pair
        Scalar m_rcut_min;          //!< The smallest cutoff radius of any pair (that is > 0)
        Scalar m_r_buff;            //!< The buffer around the cuttoff
        Scalar m_d_max;             //!< The maximum diameter of any particle in the system (or greater)
        bool m_filter_body;         //!< Set to true if particles in the same body are to be filtered
        bool m_diameter_shift;      //!< Set to true if the neighborlist rcut(i,j) should be diameter shifted
        storageMode m_storage_mode; //!< The storage mode

        GPUArray<unsigned int> m_nlist;      //!< Neighbor list data
        GPUArray<unsigned int> m_n_neigh;    //!< Number of neighbors for each particle
        GPUArray<Scalar4> m_last_pos;        //!< coordinates of last updated particle positions
        Scalar3 m_last_L;                    //!< Box lengths at last update
        Scalar3 m_last_L_local;              //!< Local Box lengths at last update

        GPUArray<unsigned int> m_head_list;     //!< Indexes for particles to read from the neighbor list
        GPUArray<unsigned int> m_Nmax;          //!< Holds the maximum number of neighbors for each particle type
        GPUArray<unsigned int> m_conditions;    //!< Holds the max number of computed particles by type for resizing

        GPUArray<unsigned int> m_ex_list_tag;  //!< List of excluded particles referenced by tag
        GPUArray<unsigned int> m_ex_list_idx;  //!< List of excluded particles referenced by index
        GPUVector<unsigned int> m_n_ex_tag;    //!< Number of exclusions for a given particle tag
        GPUArray<unsigned int> m_n_ex_idx;     //!< Number of exclusions for a given particle index
        Index2D m_ex_list_indexer;             //!< Indexer for accessing the exclusion list
        Index2D m_ex_list_indexer_tag;         //!< Indexer for accessing the by-tag exclusion list
        bool m_exclusions_set;                 //!< True if any exclusions have been set
        bool m_need_reallocate_exlist;         //!< True if global exclusion list needs to be reallocated

        boost::signals2::connection m_sort_connection;   //!< Connection to the ParticleData sort signal
        boost::signals2::connection m_max_particle_num_change_connection; //!< Connection to max particle number change signal
        boost::signals2::connection m_global_particle_num_change_connection; //!< Connection to global particle number change signal
        #ifdef ENABLE_MPI
        boost::signals2::connection m_migrate_request_connection; //!< Connection to trigger particle migration
        boost::signals2::connection m_comm_flags_request;         //!< Connection to request ghost particle fields
        boost::signals2::connection m_ghost_layer_width_request;  //!< Connection to request ghost layer width
        #endif

        //! Return true if we are supposed to do a distance check in this time step
        bool shouldCheckDistance(unsigned int timestep);

        //! Performs the distance check
        virtual bool distanceCheck(unsigned int timestep);

        //! Updates the previous position table for use in the next distance check
        virtual void setLastUpdatedPos();

        //! Builds the neighbor list
        virtual void buildNlist(unsigned int timestep);

        //! Updates the idx exlcusion list
        virtual void updateExListIdx();
        
        //! Loops through all pairs, and updates the r_list(i,j)
        void updateRList();

        //! Filter the neighbor list of excluded particles
        virtual void filterNlist();
        
        //! Build the head list to allocated memory
        virtual void buildHeadList();
        
        //! Amortized resizing of the neighborlist
        void resizeNlist(unsigned int size);

        #ifdef ENABLE_MPI
        CommFlags getRequestedCommFlags(unsigned int timestep)
            {
            // exclusions require ghost particle tags
            CommFlags flags(0);
            if (m_exclusions_set) flags[comm_flag::tag] = 1;
            return flags;
            }
        #endif

    private:
        boost::signals2::signal<void ()> m_rcut_signal;     //!< Signal that is triggered when the cutoff radius changes

        boost::signals2::connection m_rcut_change_conn;     //!< Connection to the rcut array changing
        bool m_rcut_changed;                                //!< Flag if the rcut array has changed
        //! Notify the NeighborList that the rcut has changed for delayed updating
        void slotRCutChange()
            {
            m_rcut_changed = true;
            }

        boost::signals2::connection m_num_type_change_conn; //!< Connection to the ParticleData number of types
    
        int64_t m_updates;              //!< Number of times the neighbor list has been updated
        int64_t m_forced_updates;       //!< Number of times the neighbor list has been foribly updated
        int64_t m_dangerous_updates;    //!< Number of dangerous builds counted
        bool m_force_update;            //!< Flag to handle the forcing of neighborlist updates
        bool m_dist_check;              //!< Set to false to disable distance checks (nlist always built m_every steps)
        bool m_has_been_updated_once;   //!< True if the neighbor list has been updated at least once

        unsigned int m_last_updated_tstep; //!< Track the last time step we were updated
        unsigned int m_last_checked_tstep; //!< Track the last time step we have checked
        bool m_last_check_result;          //!< Last result of rebuild check
        unsigned int m_every; //!< No update checks will be performed until m_every steps after the last one
        std::vector<unsigned int> m_update_periods;    //!< Steps between updates

        //! Test if the list needs updating
        bool needsUpdating(unsigned int timestep);

        //! Reallocate internal neighbor list data structures
        void reallocate();
        
        //! Reallocate internal data structures that depend on types
        void reallocateTypes();

        //! Check the status of the conditions
        bool checkConditions();

        //! Resets the condition status to all zeroes
        virtual void resetConditions();

        //! Grow the exclusions list memory capacity by one row
        void growExclusionList();

        //! Method to be called when the global particle number changes
        void slotGlobalParticleNumberChange()
            {
            m_need_reallocate_exlist = true;
            }
    };

//! Exports NeighborList to python
void export_NeighborList();

#endif
