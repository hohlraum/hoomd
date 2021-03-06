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

// Maintainer: mphoward

#include "NeighborListGPU.h"
#include "NeighborListGPUTree.cuh"
#include "Autotuner.h"

/*! \file NeighborListGPUTree.h
    \brief Declares the NeighborListGPUTree class
*/

#ifdef NVCC
#error This header cannot be compiled by nvcc
#endif

#ifndef __NEIGHBORLISTGPUTREE_H__
#define __NEIGHBORLISTGPUTREE_H__

//! Efficient neighbor list build on the GPU using BVH trees
/*!
 * GPU kernel methods are defined in NeighborListGPUTree.cuh and implemented in NeighborListGPUTree.cu.
 *
 * \ingroup computes
 */
class NeighborListGPUTree : public NeighborListGPU
    {
    public:
        //! Constructs the compute
        NeighborListGPUTree(boost::shared_ptr<SystemDefinition> sysdef,
                            Scalar r_cut,
                            Scalar r_buff);

        //! Destructor
        virtual ~NeighborListGPUTree();

        //! Set autotuner parameters
        /*! \param enable Enable/disable autotuning
            \param period period (approximate) in time steps when returning occurs
        */
        virtual void setAutotunerParams(bool enable, unsigned int period)
            {
            NeighborListGPU::setAutotunerParams(enable, period);
            
            m_tuner_morton->setPeriod(period/10);
            m_tuner_morton->setEnabled(enable);
            
            m_tuner_merge->setPeriod(period/10);
            m_tuner_merge->setEnabled(enable);
            
            m_tuner_hierarchy->setPeriod(period/10);
            m_tuner_hierarchy->setEnabled(enable);
            
            m_tuner_bubble->setPeriod(period/10);
            m_tuner_bubble->setEnabled(enable);
            
            m_tuner_move->setPeriod(period/10);
            m_tuner_move->setEnabled(enable);
            
            m_tuner_map->setPeriod(period/10);
            m_tuner_map->setEnabled(enable);
            
            m_tuner_traverse->setPeriod(period/10);
            m_tuner_traverse->setEnabled(enable);
            }
        
    protected:
        //! Builds the neighbor list
        virtual void buildNlist(unsigned int timestep);
        
    private:
        //! \name Autotuners
        // @{
        boost::scoped_ptr<Autotuner> m_tuner_morton;    //!< Tuner for kernel to calculate morton codes
        boost::scoped_ptr<Autotuner> m_tuner_merge;     //!< Tuner for kernel to merge particles into leafs
        boost::scoped_ptr<Autotuner> m_tuner_hierarchy; //!< Tuner for kernel to generate tree hierarchy
        boost::scoped_ptr<Autotuner> m_tuner_bubble;    //!< Tuner for kernel to bubble aabbs up hierarchy
        boost::scoped_ptr<Autotuner> m_tuner_move;      //!< Tuner for kernel to move particles to leaf order
        boost::scoped_ptr<Autotuner> m_tuner_map;       //!< Tuner for kernel to help map particles by type
        boost::scoped_ptr<Autotuner> m_tuner_traverse;  //!< Tuner for kernel to traverse generated tree
        // @}
        
        //! \name Signal updates
        // @{
        
        //! Notification of a box size change
        void slotBoxChanged()
            {
            m_box_changed = true;
            }
            
        //! Notification of a change in the maximum number of particles on any rank    
        void slotMaxNumChanged()
            {
            m_max_num_changed = true;
            }
        
        //! Notification of a change in the number of types
        void slotNumTypesChanged()
            {
            // skip the reallocation if the number of types does not change
            // this keeps old parameters when restoring a snapshot
            // it will result in invalid coeficients if the snapshot has a different type id -> name mapping
            if (m_pdata->getNTypes() == m_prev_ntypes)
                return;

            m_type_changed = true;
            }
        
        unsigned int m_prev_ntypes;                         //!< Previous number of types
        bool m_type_changed;                                //!< Flag if types changed
        boost::signals2::connection m_num_type_change_conn; //!< Connection to the ParticleData number of types
        
        bool m_box_changed;                                 //!< Flag if box changed
        boost::signals2::connection m_boxchange_connection; //!< Connection to the ParticleData box size change signal
        
        bool m_max_num_changed;                             //!< Flag if max number of particles changed
        boost::signals2::connection m_max_numchange_conn;   //!< Connection to max particle number change signal
        // @}
        
        //! \name Tree building
        // @{
        // mapping and sorting
        GPUArray<unsigned int> m_map_tree_pid;      //!< Map a leaf order id to a particle id
        GPUArray<unsigned int> m_map_tree_pid_alt;  //!< Double buffer for map needed for sorting
        
        GPUArray<uint64_t> m_morton_types;      //!< 30 bit morton codes + type for particles to sort on z-order curve
        GPUArray<uint64_t> m_morton_types_alt;  //!< Double buffer for morton codes needed for sorting
        GPUFlags<int> m_morton_conditions;      //!< Condition flag to catch out of bounds particles
        
        GPUArray<unsigned int> m_leaf_offset;   //!< Total offset in particle index for leaf nodes by type
        GPUArray<unsigned int> m_num_per_type;  //!< Number of particles per type
        GPUArray<unsigned int> m_type_head;     //!< Head list to each particle type
        GPUArray<unsigned int> m_tree_roots;    //!< Index for root node of each tree by type
        
        // hierarchy generation
        unsigned int m_n_leaf;                      //!< Total number of leaves in trees
        unsigned int m_n_internal;                  //!< Total number of internal nodes in trees
        unsigned int m_n_node;                      //!< Total number of leaf + internal nodes in trees
        
        GPUVector<uint32_t> m_morton_codes_red;     //!< Reduced capacity 30 bit morton code array (per leaf)
        GPUVector<Scalar4> m_tree_aabbs;            //!< AABBs for merged leaf nodes and internal nodes
        GPUVector<unsigned int> m_node_locks;       //!< Node locks for if node has been visited or not
        GPUVector<uint2> m_tree_parent_sib;         //!< Parents and siblings of all nodes
        
        //! Performs initial allocation of tree internal data structure memory
        void allocateTree();
        
        //! Performs all tasks needed before tree build and traversal
        void setupTree();
        
        //! Determines the number and head indexes for particle types and leafs
        void countParticlesAndTrees();
        
        //! Driver for tree multi-step tree build on the GPU
        void buildTree();
        
        //! Calculates 30-bit morton codes for particles
        void calcMortonCodes();
        
        //! Driver to sort particles by type and morton code along a Z order curve
        void sortMortonCodes();
        
        //! Calculates the number of bits needed to represent the largest particle type
        void calcTypeBits();
        unsigned int m_n_type_bits;     //!< the number of bits it takes to represent all the type ids
        
        //! Merges sorted particles into leafs based on adjacency
        void mergeLeafParticles();
        
        //! Generates the edges between nodes based on the sorted morton codes
        void genTreeHierarchy();
        
        //! Constructs enclosing AABBs from leaf to roots
        void bubbleAABBs();
        
        // @}
        //! \name Tree traversal
        // @{
        
        GPUArray<Scalar4> m_leaf_xyzf;          //!< Position and id of each particle in a leaf
        GPUArray<Scalar2> m_leaf_db;            //!< Diameter and body of each particle in a leaf
        
        GPUArray<Scalar3> m_image_list; //!< List of translation vectors
        unsigned int m_n_images;        //!< Number of translation vectors
        
        //! Computes the image vectors to query for 
        void updateImageVectors();

        //! Moves particles from ParticleData order to leaf order for more efficient tree traversal
        void moveLeafParticles();

        //! Traverses the trees on the GPU
        void traverseTree();
        // @}
    };

//! Exports NeighborListGPUBinned to python
void export_NeighborListGPUTree();
#endif //__NEIGHBORLISTGPUTREE_H__
