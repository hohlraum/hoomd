/*
Highly Optimized Object-oriented Many-particle Dynamics -- Blue Edition
(HOOMD-blue) Open Source Software License Copyright 2008, 2009 Ames Laboratory
Iowa State University and The Regents of the University of Michigan All rights
reserved.

HOOMD-blue may contain modifications ("Contributions") provided, and to which
copyright is held, by various Contributors who have granted The Regents of the
University of Michigan the right to modify and/or distribute such Contributions.

Redistribution and use of HOOMD-blue, in source and binary forms, with or
without modification, are permitted, provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright notice, this
list of conditions, and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice, this
list of conditions, and the following disclaimer in the documentation and/or
other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of HOOMD-blue's
contributors may be used to endorse or promote products derived from this
software without specific prior written permission.

Disclaimer

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS ``AS IS''
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND/OR
ANY WARRANTIES THAT THIS SOFTWARE IS FREE OF INFRINGEMENT ARE DISCLAIMED.

IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// $Id$
// $URL$
// Maintainer: joaander

/*! \file HOOMDBinaryInitializer.cc
    \brief Defines the HOOMDBinaryInitializer class
*/

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4244 4267 )
#endif

#include "HOOMDBinaryInitializer.h"

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <algorithm>

using namespace std;

#include <boost/python.hpp>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#ifdef ENABLE_ZLIB
#include <boost/iostreams/filter/gzip.hpp>
#endif

using namespace boost::python;
using namespace boost;
using namespace boost::iostreams;

/*! \param fname File name with the data to load
    The file will be read and parsed fully during the constructor call.
*/
HOOMDBinaryInitializer::HOOMDBinaryInitializer(const std::string &fname)
    {
    // initialize member variables
    m_timestep = 0;
    m_num_dimensions = 3;
    // read in the file
    readFile(fname);
    }

/* XXX: shouldn't the following methods be put into
 * the header so that they get inlined? */

/*! \returns Number of dimensions parsed from the binary file
*/
unsigned int HOOMDBinaryInitializer::getNumDimensions() const
    {
    return m_num_dimensions;
    }

/*! \returns Number of particles parsed from the binary file
*/
unsigned int HOOMDBinaryInitializer::getNumParticles() const
    {
    assert(m_x_array.size() > 0);
    return (unsigned int)m_x_array.size();
    }

/*! \returns Numer of particle types parsed from the binary file
*/
unsigned int HOOMDBinaryInitializer::getNumParticleTypes() const
    {
    assert(m_type_mapping.size() > 0);
    return (unsigned int)m_type_mapping.size();
    }

/*! \returns Box dimensions parsed from the binary file
*/
BoxDim HOOMDBinaryInitializer::getBox() const
    {
    return m_box;
    }

/*! \returns Time step parsed from the binary file
*/
unsigned int HOOMDBinaryInitializer::getTimeStep() const
    {
    return m_timestep;
    }

/* change internal timestep number. */
void HOOMDBinaryInitializer::setTimeStep(unsigned int ts)
    {
    m_timestep = ts;
    }

/*! \param pdata The particle data

    initArrays takes the internally stored copy of the particle data and copies it
    into the provided particle data arrays for storage in ParticleData.
*/
void HOOMDBinaryInitializer::initArrays(const ParticleDataArrays &pdata) const
    {
    assert(m_x_array.size() > 0 && m_x_array.size() == pdata.nparticles);
        
    // loop through all the particles and set them up
    for (unsigned int i = 0; i < m_x_array.size(); i++)
        {
        pdata.tag[i] = m_tag_array[i];
        pdata.rtag[i] = m_rtag_array[i];

        pdata.x[i] = m_x_array[i];
        pdata.y[i] = m_y_array[i];
        pdata.z[i] = m_z_array[i];

        pdata.ix[i] = m_ix_array[i];
        pdata.iy[i] = m_iy_array[i];
        pdata.iz[i] = m_iz_array[i];
        
        pdata.vx[i] = m_vx_array[i];
        pdata.vy[i] = m_vy_array[i];
        pdata.vz[i] = m_vz_array[i];

        pdata.ax[i] = m_ax_array[i];
        pdata.ay[i] = m_ay_array[i];
        pdata.az[i] = m_az_array[i];

        pdata.mass[i] = m_mass_array[i];
        pdata.type[i] = m_type_array[i];
        pdata.diameter[i] = m_diameter_array[i];
        pdata.charge[i] = m_charge_array[i];
        pdata.body[i] = m_body_array[i];
        }        
    }

/*! \param wall_data WallData to initialize with the data read from the file
*/
void HOOMDBinaryInitializer::initWallData(boost::shared_ptr<WallData> wall_data) const
    {
    // copy the walls over from our internal list
    for (unsigned int i = 0; i < m_walls.size(); i++)
        wall_data->addWall(m_walls[i]);
    }

void HOOMDBinaryInitializer::initIntegratorData(boost::shared_ptr<IntegratorData> integrator_data ) const
    {
    integrator_data->load(m_integrator_variables.size());
    for (unsigned int i=0; i<m_integrator_variables.size(); i++)
        {
        integrator_data->setIntegratorVariables(i, m_integrator_variables[i]);
        }
    }

//! Helper function to read a string from the file
static string read_string(istream &f)
    {
    unsigned int len;
    f.read((char*)&len, sizeof(unsigned int));
    if (len != 0)
        {
        char *cstr = new char[len+1];
        f.read(cstr, len*sizeof(char));
        cstr[len] = '\0';
        string str(cstr);
        delete[] cstr;
        return str;
        }
    else
        return string();
    }

/*! \param fname File name of the hoomd_binary file to read in
    \post Internal data arrays and members are filled out from which futre calls
    like initArrays will use to intialize the ParticleData

    This function implements the main parser loop. It reads in XML nodes from the
    file one by one and passes them of to parsers registered in \c m_parser_map.
*/
void HOOMDBinaryInitializer::readFile(const string &fname)
    {
    // check to see if the file has a .gz extension or not and enable decompression if it is
    bool enable_decompression = false;
    string ext = fname.substr(fname.size()-3, fname.size());
    if (ext == string(".gz"))
         enable_decompression = true;
    
    #ifndef ENABLE_ZLIB
    if (enable_decompression)
        {
        cerr << endl << "***Error! HOOMDBinaryInitialzier is trying to read a compressed .gz file, but ZLIB was not" << endl;
        cerr << "enabled in this build of hoomd" << endl << endl;
        throw runtime_error("Error reading binary file");
        }
    #endif
    
    // Open the file
    cout<< "Reading " << fname << "..." << endl;
    // setup the file input for decompression
    filtering_istream f;
    #ifdef ENABLE_ZLIB
    if (enable_decompression)
        f.push(gzip_decompressor());
    #endif
    f.push(file_source(fname.c_str(), ios::in | ios::binary));
    
    // handle errors
    if (f.fail())
        {
        cerr << endl << "***Error! Error opening " << fname << endl << endl;
        throw runtime_error("Error reading binary file");
        }
    
    // read magic
    unsigned int magic = 0x444d4f48;
    unsigned int file_magic;
    f.read((char*)&file_magic, sizeof(int));
    if (magic != file_magic)
        {
        cerr << endl << "***Error! " << fname << " does not appear to be a hoomd_bin file." << endl;
        if (enable_decompression)
            cerr << "Is it perhaps an uncompressed file with an erroneous .gz extension?" << endl << endl;
        else
            cerr << "Is it perhaps a compressed file without a .gz extension?" << endl << endl;

        throw runtime_error("Error reading binary file");
        }
    
    int version = 3;
    int file_version;
    f.read((char*)&file_version, sizeof(int));
    
    // right now, the version tag doesn't do anything: just warn if they don't match
    if (version != file_version)
        {
        cout << endl
             << "***Error! hoomd binary file does not match the current version,"
             << endl << endl;
        throw runtime_error("Error reading binary file");
        }
    
    //parse timestep
    int timestep;
    f.read((char*)&timestep, sizeof(unsigned int));
    m_timestep = timestep;

    //parse dimensions
    unsigned int dimensions;
    f.read((char*)&dimensions, sizeof(unsigned int));
    m_num_dimensions = dimensions;
    
    //parse box
    Scalar Lx,Ly,Lz;
    f.read((char*)&Lx, sizeof(Scalar));
    f.read((char*)&Ly, sizeof(Scalar));
    f.read((char*)&Lz, sizeof(Scalar));
    m_box = BoxDim(Lx,Ly,Lz);
    
    //allocate memory for particle arrays
    unsigned int np = 0;
    f.read((char*)&np, sizeof(unsigned int));
    m_tag_array.resize(np); m_rtag_array.resize(np);
    m_x_array.resize(np); m_y_array.resize(np); m_z_array.resize(np);
    m_ix_array.resize(np); m_iy_array.resize(np); m_iz_array.resize(np);
    m_vx_array.resize(np); m_vy_array.resize(np); m_vz_array.resize(np);
    m_ax_array.resize(np); m_ay_array.resize(np); m_az_array.resize(np);
    m_mass_array.resize(np); 
    m_diameter_array.resize(np); 
    m_type_array.resize(np);
    m_charge_array.resize(np);
    m_body_array.resize(np);
    
    //parse particle arrays
    f.read((char*)&(m_tag_array[0]), np*sizeof(unsigned int));
    f.read((char*)&(m_rtag_array[0]), np*sizeof(unsigned int));
    f.read((char*)&(m_x_array[0]), np*sizeof(Scalar));
    f.read((char*)&(m_y_array[0]), np*sizeof(Scalar));
    f.read((char*)&(m_z_array[0]), np*sizeof(Scalar));
    f.read((char*)&(m_ix_array[0]), np*sizeof(int));
    f.read((char*)&(m_iy_array[0]), np*sizeof(int));
    f.read((char*)&(m_iz_array[0]), np*sizeof(int));
    f.read((char*)&(m_vx_array[0]), np*sizeof(Scalar));
    f.read((char*)&(m_vy_array[0]), np*sizeof(Scalar));
    f.read((char*)&(m_vz_array[0]), np*sizeof(Scalar));
    f.read((char*)&(m_ax_array[0]), np*sizeof(Scalar));
    f.read((char*)&(m_ay_array[0]), np*sizeof(Scalar));
    f.read((char*)&(m_az_array[0]), np*sizeof(Scalar));
    f.read((char*)&(m_mass_array[0]), np*sizeof(Scalar));
    f.read((char*)&(m_diameter_array[0]), np*sizeof(Scalar));
    f.read((char*)&(m_charge_array[0]), np*sizeof(Scalar));
    f.read((char*)&(m_body_array[0]), np*sizeof(unsigned int));

    //parse types
    unsigned int ntypes = 0;
    f.read((char*)&ntypes, sizeof(unsigned int));
    m_type_mapping.resize(ntypes);
    for (unsigned int i = 0; i < ntypes; i++)
        m_type_mapping[i] = read_string(f);
    f.read((char*)&(m_type_array[0]), np*sizeof(unsigned int));

    //parse integrator states
    {
    std::vector<IntegratorVariables> v;
    unsigned int ni = 0;
    f.read((char*)&ni, sizeof(unsigned int));
    v.resize(ni);
    for (unsigned int j = 0; j < ni; j++)
        {
        v[j].type = read_string(f);

        v[j].variable.clear();
        unsigned int nv = 0;
        f.read((char*)&nv, sizeof(unsigned int));
        for (unsigned int k=0; k<nv; k++)
            {
            Scalar var;
            f.read((char*)&var, sizeof(Scalar));
            v[j].variable.push_back(var);
            }
        }
        m_integrator_variables = v;
    }
    
    //parse bonds
    {
    ntypes = 0;
    f.read((char*)&ntypes, sizeof(unsigned int));
    m_bond_type_mapping.resize(ntypes);
    for (unsigned int i = 0; i < ntypes; i++)
        m_bond_type_mapping[i] = read_string(f);
    
    unsigned int nb = 0;
    f.read((char*)&nb, sizeof(unsigned int));
    for (unsigned int j = 0; j < nb; j++)
        {
        unsigned int typ, a, b;
        f.read((char*)&typ, sizeof(unsigned int));
        f.read((char*)&a, sizeof(unsigned int));
        f.read((char*)&b, sizeof(unsigned int));
        
        m_bonds.push_back(Bond(typ, a, b));
        }
    }

    //parse angles
    {
    ntypes = 0;
    f.read((char*)&ntypes, sizeof(unsigned int));
    m_angle_type_mapping.resize(ntypes);
    for (unsigned int i = 0; i < ntypes; i++)
        m_angle_type_mapping[i] = read_string(f);

    unsigned int na = 0;
    f.read((char*)&na, sizeof(unsigned int));
    for (unsigned int j = 0; j < na; j++)
        {
        unsigned int typ, a, b, c;
        f.read((char*)&typ, sizeof(unsigned int));
        f.read((char*)&a, sizeof(unsigned int));
        f.read((char*)&b, sizeof(unsigned int));
        f.read((char*)&c, sizeof(unsigned int));
        
        m_angles.push_back(Angle(typ, a, b, c));
        }
    }

    //parse dihedrals
    {
    ntypes = 0;
    f.read((char*)&ntypes, sizeof(unsigned int));
    m_dihedral_type_mapping.resize(ntypes);
    for (unsigned int i = 0; i < ntypes; i++)
        m_dihedral_type_mapping[i] = read_string(f);
    
    unsigned int nd = 0;
    f.read((char*)&nd, sizeof(unsigned int));
    for (unsigned int j = 0; j < nd; j++)
        {
        unsigned int typ, a, b, c, d;
        f.read((char*)&typ, sizeof(unsigned int));
        f.read((char*)&a, sizeof(unsigned int));
        f.read((char*)&b, sizeof(unsigned int));
        f.read((char*)&c, sizeof(unsigned int));
        f.read((char*)&d, sizeof(unsigned int));
        
        m_dihedrals.push_back(Dihedral(typ, a, b, c, d));
        }
    }

    //parse impropers
    {
    ntypes = 0;
    f.read((char*)&ntypes, sizeof(unsigned int));
    m_improper_type_mapping.resize(ntypes);
    for (unsigned int i = 0; i < ntypes; i++)
        m_improper_type_mapping[i] = read_string(f);

    unsigned int nd = 0;
    f.read((char*)&nd, sizeof(unsigned int));
    for (unsigned int j = 0; j < nd; j++)
        {
        unsigned int typ, a, b, c, d;
        f.read((char*)&typ, sizeof(unsigned int));
        f.read((char*)&a, sizeof(unsigned int));
        f.read((char*)&b, sizeof(unsigned int));
        f.read((char*)&c, sizeof(unsigned int));
        f.read((char*)&d, sizeof(unsigned int));
        
        m_impropers.push_back(Dihedral(typ, a, b, c, d));
        }
    }
    
    //parse walls
    {
    unsigned int nw = 0;
    f.read((char*)&nw, sizeof(unsigned int));
    for (unsigned int j = 0; j < nw; j++)
        {
        Scalar ox, oy, oz, nx, ny, nz;
        f.read((char*)&(ox), sizeof(Scalar));
        f.read((char*)&(oy), sizeof(Scalar));
        f.read((char*)&(oz), sizeof(Scalar));
        f.read((char*)&(nx), sizeof(Scalar));
        f.read((char*)&(ny), sizeof(Scalar));
        f.read((char*)&(nz), sizeof(Scalar));
        m_walls.push_back(Wall(ox,oy,oz,nx,ny,nz));
        }
    }
    
    // parse rigid bodies
    {
    unsigned int n_bodies = 0;
    f.read((char*)&n_bodies, sizeof(unsigned int));
    
    if (n_bodies == 0) return;
       
    m_com.resize(n_bodies);
    m_vel.resize(n_bodies);
    m_angmom.resize(n_bodies);
    m_body_image.resize(n_bodies);
        
    for (unsigned int body = 0; body < n_bodies; body++)
        {
        f.read((char*)&(m_com[body].x), sizeof(Scalar));
        f.read((char*)&(m_com[body].y), sizeof(Scalar));
        f.read((char*)&(m_com[body].z), sizeof(Scalar));
        f.read((char*)&(m_com[body].w), sizeof(Scalar));
        
        f.read((char*)&(m_vel[body].x), sizeof(Scalar));
        f.read((char*)&(m_vel[body].y), sizeof(Scalar));
        f.read((char*)&(m_vel[body].z), sizeof(Scalar));
        f.read((char*)&(m_vel[body].w), sizeof(Scalar));
        
        f.read((char*)&(m_angmom[body].x), sizeof(Scalar));
        f.read((char*)&(m_angmom[body].y), sizeof(Scalar));
        f.read((char*)&(m_angmom[body].z), sizeof(Scalar));
        f.read((char*)&(m_angmom[body].w), sizeof(Scalar));
        
        f.read((char*)&(m_body_image[body].x), sizeof(int));
        f.read((char*)&(m_body_image[body].y), sizeof(int));
        f.read((char*)&(m_body_image[body]).z, sizeof(int));
        }
    
    }
    
    // check for required items in the file
    if (m_x_array.size() == 0)
        {
        cerr << endl << "***Error! No particles found in binary file" << endl << endl;
        throw runtime_error("Error extracting data from hoomd_binary file");
        }
        
    // notify the user of what we have accomplished
    cout << "--- hoomd_binary file read summary" << endl;
    cout << getNumParticles() << " positions at timestep " << m_timestep << endl;
    if (m_ix_array.size() > 0)
        cout << m_ix_array.size() << " images" << endl;
    if (m_vx_array.size() > 0)
        cout << m_vx_array.size() << " velocities" << endl;
    if (m_mass_array.size() > 0)
        cout << m_mass_array.size() << " masses" << endl;
    if (m_diameter_array.size() > 0)
        cout << m_diameter_array.size() << " diameters" << endl;
    if (m_charge_array.size() > 0)
        cout << m_charge_array.size() << " charges" << endl;
    cout << getNumParticleTypes() <<  " particle types" << endl;
    if (m_integrator_variables.size() > 0)
        cout << m_integrator_variables.size() << " integrator states" << endl;
    if (m_bonds.size() > 0)
        cout << m_bonds.size() << " bonds" << endl;
    if (m_angles.size() > 0)
        cout << m_angles.size() << " angles" << endl;
    if (m_dihedrals.size() > 0)
        cout << m_dihedrals.size() << " dihedrals" << endl;
    if (m_impropers.size() > 0)
        cout << m_impropers.size() << " impropers" << endl;
    if (m_walls.size() > 0)
        cout << m_walls.size() << " walls" << endl;
    }

/*! \return Number of bond types determined from the XML file
*/
unsigned int HOOMDBinaryInitializer::getNumBondTypes() const
    {
    return (unsigned int)m_bond_type_mapping.size();
    }

/*! \return Number of angle types determined from the XML file
*/
unsigned int HOOMDBinaryInitializer::getNumAngleTypes() const
    {
    return (unsigned int)m_angle_type_mapping.size();
    }

/*! \return Number of dihedral types determined from the XML file
*/
unsigned int HOOMDBinaryInitializer::getNumDihedralTypes() const
    {
    return (unsigned int)m_dihedral_type_mapping.size();
    }

/*! \return Number of improper types determined from the XML file
*/
unsigned int HOOMDBinaryInitializer::getNumImproperTypes() const
    {
    return (unsigned int)m_improper_type_mapping.size();
    }

/*! \param bond_data Shared pointer to the BondData to be initialized
    Adds all bonds found in the XML file to the BondData
*/
void HOOMDBinaryInitializer::initBondData(boost::shared_ptr<BondData> bond_data) const
    {
    // loop through all the bonds and add a bond for each
    for (unsigned int i = 0; i < m_bonds.size(); i++)
        bond_data->addBond(m_bonds[i]);
        
    bond_data->setBondTypeMapping(m_bond_type_mapping);
    }

/*! \param angle_data Shared pointer to the AngleData to be initialized
    Adds all angles found in the XML file to the AngleData
*/
void HOOMDBinaryInitializer::initAngleData(boost::shared_ptr<AngleData> angle_data) const
    {
    // loop through all the angles and add an angle for each
    for (unsigned int i = 0; i < m_angles.size(); i++)
        angle_data->addAngle(m_angles[i]);
        
    angle_data->setAngleTypeMapping(m_angle_type_mapping);
    }

/*! \param dihedral_data Shared pointer to the DihedralData to be initialized
    Adds all dihedrals found in the XML file to the DihedralData
*/
void HOOMDBinaryInitializer::initDihedralData(boost::shared_ptr<DihedralData> dihedral_data) const
    {
    // loop through all the dihedrals and add an dihedral for each
    for (unsigned int i = 0; i < m_dihedrals.size(); i++)
        dihedral_data->addDihedral(m_dihedrals[i]);
        
    dihedral_data->setDihedralTypeMapping(m_dihedral_type_mapping);
    }

/*! \param improper_data Shared pointer to the ImproperData to be initialized
    Adds all impropers found in the XML file to the ImproperData
*/
void HOOMDBinaryInitializer::initImproperData(boost::shared_ptr<DihedralData> improper_data) const
    {
    // loop through all the impropers and add an improper for each
    for (unsigned int i = 0; i < m_impropers.size(); i++)
        improper_data->addDihedral(m_impropers[i]);
        
    improper_data->setDihedralTypeMapping(m_improper_type_mapping);
    }

/*! \param rigid_data Shared pointer to the ImproperData to be initialized
    Adds all rigid bodies found in the XML file to the RigidData
*/
void HOOMDBinaryInitializer::initRigidData(boost::shared_ptr<RigidData> rigid_data) const
    {
    ArrayHandle<Scalar4> r_com_handle(rigid_data->getCOM(), access_location::host, access_mode::readwrite);
    ArrayHandle<Scalar4> r_vel_handle(rigid_data->getVel(), access_location::host, access_mode::readwrite);
    ArrayHandle<Scalar4> r_angmom_handle(rigid_data->getAngMom(), access_location::host, access_mode::readwrite);
    ArrayHandle<int3> r_body_image_handle(rigid_data->getBodyImage(), access_location::host, access_mode::readwrite);
    
    // We don't need to restore force, torque and orientation because the setup will do the rest,
    // and simulation still resumes smoothly.
    unsigned int n_bodies = rigid_data->getNumBodies();
    for (unsigned int body = 0; body < n_bodies; body++)
        {
        r_com_handle.data[body].x = m_com[body].x;
        r_com_handle.data[body].y = m_com[body].y;
        r_com_handle.data[body].z = m_com[body].z;
        r_com_handle.data[body].w = m_com[body].w;
        
        r_vel_handle.data[body].x = m_vel[body].x;
        r_vel_handle.data[body].y = m_vel[body].y;
        r_vel_handle.data[body].z = m_vel[body].z;
        r_vel_handle.data[body].w = m_vel[body].w;
        
        r_angmom_handle.data[body].x = m_angmom[body].x;
        r_angmom_handle.data[body].y = m_angmom[body].y;
        r_angmom_handle.data[body].z = m_angmom[body].z;
        r_angmom_handle.data[body].w = m_angmom[body].w;
        
        r_body_image_handle.data[body] = m_body_image[body];
        }
    }


/*! \returns A mapping of type ids to type names deteremined from the XML input file
*/
std::vector<std::string> HOOMDBinaryInitializer::getTypeMapping() const
    {
    return m_type_mapping;
    }

void export_HOOMDBinaryInitializer()
    {
    class_< HOOMDBinaryInitializer, bases<ParticleDataInitializer> >("HOOMDBinaryInitializer", init<const string&>())
    // virtual methods from ParticleDataInitializer are inherited
    .def("getTimeStep", &HOOMDBinaryInitializer::getTimeStep)
    .def("setTimeStep", &HOOMDBinaryInitializer::setTimeStep)
    ;
    }

#ifdef WIN32
#pragma warning( pop )
#endif

