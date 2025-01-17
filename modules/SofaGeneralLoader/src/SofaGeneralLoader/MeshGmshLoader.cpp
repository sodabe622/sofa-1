/******************************************************************************
*                 SOFA, Simulation Open-Framework Architecture                *
*                    (c) 2006 INRIA, USTL, UJF, CNRS, MGH                     *
*                                                                             *
* This program is free software; you can redistribute it and/or modify it     *
* under the terms of the GNU Lesser General Public License as published by    *
* the Free Software Foundation; either version 2.1 of the License, or (at     *
* your option) any later version.                                             *
*                                                                             *
* This program is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       *
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License *
* for more details.                                                           *
*                                                                             *
* You should have received a copy of the GNU Lesser General Public License    *
* along with this program. If not, see <http://www.gnu.org/licenses/>.        *
*******************************************************************************
* Authors: The SOFA Team and external contributors (see Authors.txt)          *
*                                                                             *
* Contact information: contact@sofa-framework.org                             *
******************************************************************************/
#include <sofa/core/ObjectFactory.h>
#include <SofaGeneralLoader/MeshGmshLoader.h>
#include <sofa/core/visual/VisualParams.h>
#include <iostream>
#include <fstream>
#include <sofa/helper/io/Mesh.h>


namespace sofa::component::loader
{

using namespace sofa::defaulttype;
using namespace sofa::helper;
using std::string;
using std::stringstream;

int MeshGmshLoaderClass = core::RegisterObject("Specific mesh loader for Gmsh file format.")
        .add< MeshGmshLoader >()
        ;

bool MeshGmshLoader::doLoad()
{
    string cmd;
    unsigned int gmshFormat = 0;

    if (!canLoad())
    {
        msg_error(this) << "Can't load file " << m_filename.getFullPath().c_str();
        return false;
    }
    // -- Loading file
    const char* filename = m_filename.getFullPath().c_str();
    std::ifstream file(filename);

    // -- Looking for Gmsh version of this file.
    std::getline(file, cmd); //Version
    std::istringstream versionReader(cmd);
    string version;
    versionReader >> version;
    if (version == "$MeshFormat") // Reading gmsh 2.0 file
    {
        gmshFormat = 2;
        string line;
        std::getline(file, line); // we don't need this line (2 0 8)
        std::getline(file, cmd); // end Version
        std::istringstream endMeshReader(cmd);
        string endMesh;
        endMeshReader >> endMesh;

        if (endMesh != string("$EndMeshFormat") ) // it should end with $EndMeshFormat
        {
            msg_error() << "No $EndMeshFormat flag found at the end of the file. Closing File";
            file.close();
            return false;
        }
        else
        {
            std::getline(file, cmd); // First Command
        }
    }
    else
    {
        gmshFormat = 1;
    }

    std::istringstream nodeReader(cmd);
    string node;
    nodeReader >> node;
    // -- Reading file
    if (node == "$NOD" || node == "$Nodes") // Gmsh format
    {
        // By default for Gmsh file format, create subElements except if specified not to.
        if (!d_createSubelements.isSet())
            d_createSubelements.setValue(true);

        // TODO 2018-04-06: temporary change to unify loader API
        //fileRead = readGmsh(file, gmshFormat);
        (void)gmshFormat;
        file.close();
        helper::io::Mesh* _mesh = helper::io::Mesh::Create("gmsh", filename);

        copyMeshToData(*_mesh);
        delete _mesh;
        return true;
    }
    else //if it enter this "else", it means there is a problem before in the factory or in canLoad()
    {
        msg_error() << "File '" << m_filename << "' finally appears not to be a Gmsh file.";
        file.close();
        return false;
    }
}


void MeshGmshLoader::doClearBuffers()
{
    /// Nothing to do if no output is added to the "filename" dataTrackerEngine.
}

void MeshGmshLoader::addInGroup(helper::vector< sofa::core::loader::PrimitiveGroup>& group,int tag,int /*eid*/) {
    for (unsigned i=0;i<group.size();i++) {
        if (tag == group[i].p0) {
            group[i].nbp++;
            return;
        }
    }

    stringstream ss;
    string s;
    ss << tag;

    group.push_back(sofa::core::loader::PrimitiveGroup(tag,1,s,s,-1));
}

void MeshGmshLoader::normalizeGroup(helper::vector< sofa::core::loader::PrimitiveGroup>& group) {
    int start = 0;
    for (unsigned i=0;i<group.size();i++) {
        group[i].p0 = start;
        start += group[i].nbp;
    }
}

bool MeshGmshLoader::readGmsh(std::ifstream &file, const unsigned int gmshFormat)
{
    dmsg_info() << "Reading Gmsh file: " << gmshFormat;

    string cmd;

    unsigned int npoints = 0;
    unsigned int nelems = 0;

    unsigned int nlines = 0;
    unsigned int ntris = 0;
    unsigned int nquads = 0;
    unsigned int ntetrahedra = 0;
    unsigned int ncubes = 0;

    // --- Loading Vertices ---
    file >> npoints; //nb points

    auto my_positions = getWriteOnlyAccessor(d_positions);

    std::vector<unsigned int> pmap; // map for reordering vertices possibly not well sorted
    for (unsigned int i=0; i<npoints; ++i)
    {
        unsigned int index = i;
        double x,y,z;
        file >> index >> x >> y >> z;

        my_positions.push_back(Vector3(x, y, z));

        if (pmap.size() <= index)
            pmap.resize(index+1);

        pmap[index] = i; // In case of hole or switch
    }

    file >> cmd;
    if (cmd != "$ENDNOD" && cmd != "$EndNodes")
    {
        msg_error() << "'$ENDNOD' or '$EndNodes' expected, found '" << cmd << "'";
        file.close();
        return false;
    }


    // --- Loading Elements ---
    file >> cmd;
    if (cmd != "$ELM" && cmd != "$Elements")
    {
        msg_error() << "'$ELM' or '$Elements' expected, found '" << cmd << "'";
        file.close();
        return false;
    }

    file >> nelems; //Loading number of Element

    auto my_edges = getWriteOnlyAccessor(d_edges);
    auto my_triangles = getWriteOnlyAccessor(d_triangles);
    auto my_quads = getWriteOnlyAccessor(d_quads);
    auto my_tetrahedra = getWriteOnlyAccessor(d_tetrahedra);
    auto my_hexahedra = getWriteOnlyAccessor(d_hexahedra);

    auto my_highOrderEdgePositions = getWriteOnlyAccessor(d_highOrderEdgePositions);

    auto my_edgesGroups = getWriteOnlyAccessor(d_edgesGroups);
    auto my_trianglesGroups = getWriteOnlyAccessor(d_trianglesGroups);
    auto my_tetrahedraGroups = getWriteOnlyAccessor(d_tetrahedraGroups);
    auto my_hexahedraGroups = getWriteOnlyAccessor(d_hexahedraGroups);

    for (unsigned int i=0; i<nelems; ++i) // for each elem
    {
        int index, etype, rphys, relem, nnodes, ntags, tag = 0; // TODO: i don't know if tag must be set to 0, but if it's not, the application assert / crash on Windows (uninitialized value)

        if (gmshFormat==1)
        {
            // version 1.0 format is
            // elm-number elm-type reg-phys reg-elem number-of-nodes <node-number-list ...>
            file >> index >> etype >> rphys >> relem >> nnodes;
        }
        else /*if (gmshFormat == 2)*/
        {
            // version 2.0 format is
            // elm-number elm-type number-of-tags < tag > ... node-number-list
            file >> index >> etype >> ntags;

            for (int t=0; t<ntags; t++)
            {
                file >> tag;
            }


            switch (etype)
            {
            case 15: //point
                nnodes = 1;
                break;
            case 1: // Line
                nnodes = 2;
                break;
            case 2: // Triangle
                nnodes = 3;
                break;
            case 3: // Quad
                nnodes = 4;
                break;
            case 4: // Tetra
                nnodes = 4;
                break;
            case 5: // Hexa
                nnodes = 8;
                break;
            case 8: // Quadratic edge
                nnodes = 3;
                break;
            case 9: // Quadratic Triangle
                nnodes = 6;
                break;
            case 11: // Quadratic Tetrahedron
                nnodes = 10;
                break;
            default:
                msg_warning() << "Elements of type 1, 2, 3, 4, 5, or 6 expected. Element of type " << etype << " found.";
                nnodes = 0;
            }
        }


        //store real index of node and not line index
        helper::vector <unsigned int> nodes;
        nodes.resize (nnodes);
        const unsigned int edgesInQuadraticTriangle[3][2] = {{0,1}, {1,2}, {2,0}};
        const unsigned int edgesInQuadraticTetrahedron[6][2] = {{0,1}, {1,2}, {0,2},{0,3},{2,3},{1,3}};
        std::set<Edge> edgeSet;
        size_t j;
        for (int n=0; n<nnodes; ++n)
        {
            int t = 0;
            file >> t;
            nodes[n] = (((unsigned int)t)<pmap.size())?pmap[t]:0;
        }

        switch (etype)
        {
        case 1: // Line
            addInGroup(my_edgesGroups.wref(),tag,my_edges.size());
            addEdge(my_edges.wref(), Edge(nodes[0], nodes[1]));
            ++nlines;
            break;
        case 2: // Triangle
            addInGroup(my_trianglesGroups.wref(),tag,my_triangles.size());
            addTriangle(my_triangles.wref(), Triangle(nodes[0], nodes[1], nodes[2]));
            ++ntris;
            break;
        case 3: // Quad
            addQuad(my_quads.wref(), Quad(nodes[0], nodes[1], nodes[2], nodes[3]));
            ++nquads;
            break;
        case 4: // Tetra
            addInGroup(my_tetrahedraGroups.wref(),tag,my_tetrahedra.size());
            addTetrahedron(my_tetrahedra.wref(), Tetrahedron(nodes[0], nodes[1], nodes[2], nodes[3]));
            ++ntetrahedra;
            break;
        case 5: // Hexa
            addInGroup(my_hexahedraGroups.wref(),tag,my_hexahedra.size());
            addHexahedron(my_hexahedra.wref(),Hexahedron(nodes[0], nodes[1], nodes[2], nodes[3],nodes[4],nodes[5],nodes[6],nodes[7]));
            ++ncubes;
            break;
        case 8: // quadratic edge
            addInGroup(my_edgesGroups.wref(),tag,my_edges.size());
            addEdge(my_edges.wref(), Edge(nodes[0], nodes[1]));
            {
                HighOrderEdgePosition hoep;
                hoep[0]= nodes[2];
                hoep[1]=my_edges.size()-1;
                hoep[2]=1;
                hoep[3]=1;
                my_highOrderEdgePositions.push_back(hoep);
            }
            ++nlines;
            break;
        case 9: // quadratic triangle
            addInGroup(my_trianglesGroups.wref(),tag,my_triangles.size());
            addTriangle(my_triangles.wref(), Triangle(nodes[0], nodes[1], nodes[2]));
            {
                HighOrderEdgePosition hoep;
                for(j=0;j<3;++j) {
                    size_t v0=std::min( nodes[edgesInQuadraticTriangle[j][0]],
                        nodes[edgesInQuadraticTriangle[j][1]]);
                    size_t v1=std::max( nodes[edgesInQuadraticTriangle[j][0]],
                        nodes[edgesInQuadraticTriangle[j][1]]);
                    Edge e(v0,v1);
                    if (edgeSet.find(e)==edgeSet.end()) {
                        edgeSet.insert(e);
                        addEdge(my_edges.wref(), v0, v1);
                        hoep[0]= nodes[j+3];
                        hoep[1]=my_edges.size()-1;
                        hoep[2]=1;
                        hoep[3]=1;
                        my_highOrderEdgePositions.push_back(hoep);
                    }
                }
            }
            ++ntris;
            break;
        case 11: // quadratic tetrahedron
            addInGroup(my_tetrahedraGroups.wref(),tag,my_tetrahedra.size());
            addTetrahedron(my_tetrahedra.wref(), Tetrahedron(nodes[0], nodes[1], nodes[2], nodes[3]));
            {
                HighOrderEdgePosition hoep;
                for(j=0;j<6;++j) {
                    size_t v0=std::min( nodes[edgesInQuadraticTetrahedron[j][0]],
                        nodes[edgesInQuadraticTetrahedron[j][1]]);
                    size_t v1=std::max( nodes[edgesInQuadraticTetrahedron[j][0]],
                        nodes[edgesInQuadraticTetrahedron[j][1]]);
                    Edge e(v0,v1);
                    if (edgeSet.find(e)==edgeSet.end()) {
                        edgeSet.insert(e);
                        addEdge(my_edges.wref(), v0, v1);
                        hoep[0]= nodes[j+4];
                        hoep[1]=my_edges.size()-1;
                        hoep[2]=1;
                        hoep[3]=1;
                        my_highOrderEdgePositions.push_back(hoep);
                    }
                }
            }
            ++ntetrahedra;
            break;
        default:
            //if the type is not handled, skip rest of the line
            string tmp;
            std::getline(file, tmp);
        }
    }

    normalizeGroup(my_edgesGroups.wref());
    normalizeGroup(my_trianglesGroups.wref());
    normalizeGroup(my_tetrahedraGroups.wref());
    normalizeGroup(my_hexahedraGroups.wref());

    file >> cmd;
    if (cmd != "$ENDELM" && cmd!="$EndElements")
    {
        msg_error() << "'$ENDELM' or '$EndElements' expected, found '" << cmd << "'";
        file.close();
        return false;
    }

    file.close();
    return true;
}


} //namespace sofa::component::loader
