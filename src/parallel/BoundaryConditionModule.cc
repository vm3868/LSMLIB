/*
 * File:        BoundaryConditionModule.cc
 * Copyrights:  (c) 2005 The Trustees of Princeton University and Board of
 *                  Regents of the University of Texas.  All rights reserved.
 *              (c) 2009 Kevin T. Chu.  All rights reserved.
 * Revision:    $Revision$
 * Modified:    $Date$
 * Description: Header file for anti-periodic bc module
 */
 
#ifndef included_BoundaryConditionModule_cc
#define included_BoundaryConditionModule_cc

#include "BoundaryConditionModule.h"

// SAMRAI Headers
#include "SAMRAI/geom/CartesianPatchGeometry.h"
#include "SAMRAI/pdat/CellData.h"
#include "SAMRAI/hier/PatchGeometry.h"

// Toolbox headers
#include "lsm_boundary_conditions1d.h"
#include "lsm_boundary_conditions2d.h"
#include "lsm_boundary_conditions3d.h"

using namespace geom;
using namespace pdat;
typedef PatchGeometry::TwoDimBool TwoDimBool;

namespace LSMLIB {


/* Standard constructor */
BoundaryConditionModule::BoundaryConditionModule(
  boost::shared_ptr< PatchHierarchy > patch_hierarchy,
  const IntVector& ghostcell_width)
:
d_ghostcell_width(ghostcell_width),
d_geom_periodic_dirs(patch_hierarchy->getDim(),0)
{
 // invoke resetHierarchyConfiguration() to set up boundary boxes, etc.
  resetHierarchyConfiguration(
    patch_hierarchy, 0, patch_hierarchy->getFinestLevelNumber(), 
    ghostcell_width);
}

/* Default constructor */
BoundaryConditionModule::BoundaryConditionModule()
:
d_ghostcell_width(d_patch_hierarchy->getDim(),0),
d_geom_periodic_dirs(d_patch_hierarchy->getDim(),0)
{
  d_patch_hierarchy = boost::shared_ptr< PatchHierarchy > (); 
  d_boundary_boxes.setNull();
  d_touches_boundary.setNull();
}


/* Copy constructor */
BoundaryConditionModule::BoundaryConditionModule(
  const BoundaryConditionModule& rhs) :
d_ghostcell_width(rhs.d_ghostcell_width),
d_geom_periodic_dirs(rhs.d_geom_periodic_dirs)
{
  d_patch_hierarchy = rhs.d_patch_hierarchy;
  d_boundary_boxes = rhs.d_boundary_boxes;
  d_touches_boundary = rhs.d_touches_boundary;
}


/* imposeBoundaryConditions() */
void BoundaryConditionModule::imposeBoundaryConditions(
  const int phi_handle,
  const IntVector& lower_bc,
  const IntVector& upper_bc,
  const SPATIAL_DERIVATIVE_TYPE spatial_derivative_type,
  const int spatial_derivative_order,
  const int component)
{
  // loop over hierarchy and impose boundary conditions
  const int num_levels = d_patch_hierarchy->getNumberOfLevels();
  for ( int ln=0 ; ln < num_levels; ln++ ) {

    boost::shared_ptr< PatchLevel> level = d_patch_hierarchy->getPatchLevel(ln);
    for (PatchLevel::Iterator pi(level->begin()); pi!=level->end(); pi++) { // loop over patches
      //const int patch_num = *pi;
      //boost::shared_ptr< Patch > patch = level->getPatch(patch_num);
      boost::shared_ptr< Patch > patch = *pi;//returns second patch in line.
      if ( patch==NULL ) {
        TBOX_ERROR(  "BoundaryConditionModule::"
                  << "imposeBoundaryConditions(): "
                  << "Cannot find patch. Null patch pointer."
                  << endl);
      }

      // check that patch touches boundary of computational domain
      if ( d_touches_boundary[ln][patch->getLocalId().getValue()] ) {

        imposeBoundaryConditionsOnPatch(
          *patch,
          phi_handle,
          lower_bc,
          upper_bc,
          spatial_derivative_type,
          spatial_derivative_order,
          component); 

      } // end case: Patch touches boundary

    } // end loop over Patches

  } // end loop over PatchLevels

}


/* imposeBoundaryConditionsOnPatch() */
void BoundaryConditionModule::imposeBoundaryConditionsOnPatch(
  Patch& patch,
  const int phi_handle,
  const IntVector& lower_bc,
  const IntVector& upper_bc,
  const SPATIAL_DERIVATIVE_TYPE spatial_derivative_type,
  const int spatial_derivative_order,
  const int component)
{

  if ( (lower_bc(0) != -1) && (upper_bc(0) != -1) ) {

    // set linear extrapolation boundary conditions
    imposeLinearExtrapolationBCsOnPatch(patch, 
                                        phi_handle,
                                        lower_bc,
                                        upper_bc,
                                        component);

    // set signed linear extrapolation boundary conditions
    imposeSignedLinearExtrapolationBCsOnPatch(patch, 
                                              phi_handle,
                                              lower_bc,
                                              upper_bc,
                                              component);
    // set anti-periodic boundary conditions
    imposeAntiPeriodicBCsOnPatch(patch, 
                                 phi_handle,
                                 lower_bc,
                                 upper_bc,
                                 component);

    // set homogeneous Neumann boundary conditions
    imposeHomogeneousNeumannBCsOnPatch(patch, 
                                       phi_handle,
                                       lower_bc,
                                       upper_bc,
                                       spatial_derivative_type,
                                       spatial_derivative_order,
                                       component);

  } else {  // boundary conditions are incomplete, so use default BCs

    IntVector default_bc(d_patch_hierarchy->getDim(),HOMOGENEOUS_NEUMANN);
    int DIM = d_patch_hierarchy->getDim().getValue();
    for (int dim=0; dim<DIM; dim++) {
      if (d_geom_periodic_dirs(dim)) {
        default_bc(dim) = NONE;
      }
    }

    // set homogeneous Neumann boundary conditions
    imposeHomogeneousNeumannBCsOnPatch(patch,
                                       phi_handle,
                                       default_bc,
                                       default_bc,
                                       spatial_derivative_type,
                                       spatial_derivative_order,
                                       component);

  }
}


/* imposeAntiPeriodicBCs() */
void BoundaryConditionModule::imposeAntiPeriodicBCs( 
  const int phi_handle,
  const IntVector& lower_bc,
  const IntVector& upper_bc,
  const int component )
{
  // loop over hierarchy and impose anti-periodic boundary conditions
  const int num_levels = d_patch_hierarchy->getNumberOfLevels();
  for ( int ln=0 ; ln < num_levels; ln++ ) {

    boost::shared_ptr< PatchLevel> level = d_patch_hierarchy->getPatchLevel(ln);
    for (PatchLevel::Iterator pi(level->begin()); pi!=level->end(); pi++) { // loop over patches
      //const int patch_num = *pi;
      //boost::shared_ptr< Patch > patch = level->getPatch(patch_num);
      boost::shared_ptr< Patch > patch = *pi;//returns second patch in line.
      if ( patch==NULL ) {
        TBOX_ERROR(  "BoundaryConditionModule::"
                  << "imposeAntiPeriodicBCs(): "
                  << "Cannot find patch. Null patch pointer."
                  << endl);
      }

      // check that patch touches boundary of computational domain
      if ( d_touches_boundary[ln][patch->getLocalId().getValue()] ) {

        imposeAntiPeriodicBCsOnPatch(*patch,
                                     phi_handle,
                                     lower_bc,
                                     upper_bc,
                                     component);

      } // end case: Patch touches boundary of computational domain

    } // end loop over Patches
  } // end loop over PatchLevels
}


/* imposeAntiPeriodicBCsOnPatch() */
void BoundaryConditionModule::imposeAntiPeriodicBCsOnPatch( 
  Patch& patch,
  const int phi_handle,
  const IntVector& lower_bc,
  const IntVector& upper_bc,
  const int component )
{
  // get patch level number and patch number
  int level_num = patch.getPatchLevelNumber();
  BoxId boxid = patch.getBox().getBoxId();
  // get PatchData
  boost::shared_ptr< CellData<LSMLIB_REAL> > phi_data = 
    BOOST_CAST<CellData<LSMLIB_REAL>, PatchData>(patch.getPatchData( phi_handle ));

  // check that the ghostcell width for phi is compatible
  // with the ghostcell width 
  if (d_ghostcell_width != phi_data->getGhostCellWidth()) {
    TBOX_ERROR(  "BoundaryConditionModule::"
              << "imposeAntiPeriodicBCsOnPatch(): "
              << "ghostcell width for phi_handle is incompatible "
              << "with ghostcell width for this object."
              << endl );
  }

  // get PatchGeometry
  boost::shared_ptr< CartesianPatchGeometry > patch_geom = 
    BOOST_CAST <CartesianPatchGeometry, PatchGeometry>(patch.getPatchGeometry());  

  // get interior box for patch (used to compute boundary fill box)
  Box interior_box(patch.getBox());

  // get ghostcell width to fill
  IntVector phi_ghost_width_to_fill = phi_data->getGhostCellWidth();
  Box phi_ghostbox = phi_data->getGhostBox();

  // get data components
  int comp_lo = component;
  int comp_hi = component+1;
  if (component < 0) {
    comp_lo = 0;
    comp_hi = phi_data->getDepth();
  }
  int DIM = d_patch_hierarchy->getDim().getValue();
  if (DIM == 3) {

    /*
     * fill face boundaries
     */
    const std::vector<BoundaryBox> face_bdry = 
      d_boundary_boxes[level_num].find(boxid)->second[2];//dimensionality of a face??
    for (unsigned int i = 0; i < face_bdry.size(); i++) {

      // check that boundary is a periodic boundary for level
      // set functions
      int bdry_location_idx = face_bdry[i].getLocationIndex();
      if (  d_geom_periodic_dirs(bdry_location_idx/2) &&
            (lower_bc[bdry_location_idx/2] == ANTI_PERIODIC) ) {

        /*
         * impose anti-periodic boundary conditions for phi 
         */
        Box phi_fillbox = patch_geom->getBoundaryFillBox(
          face_bdry[i], interior_box, phi_ghost_width_to_fill);
        int phi_ghostbox_num_cells_x = phi_ghostbox.numberCells(0);
        int phi_ghostbox_num_cells_y = phi_ghostbox.numberCells(1);
        IntVector phi_ghostbox_lower = phi_ghostbox.lower();

        IntVector fillbox_lower = phi_fillbox.lower();
        IntVector fillbox_upper = phi_fillbox.upper();

        // loop over components
        for (int comp = comp_lo; comp < comp_hi; comp++) {

          LSMLIB_REAL* phi = phi_data->getPointer(comp);

          for (int k = fillbox_lower(2); k <= fillbox_upper(2); k++) {
            for (int j = fillbox_lower(1); j <= fillbox_upper(1); j++) {
              for (int i = fillbox_lower(0); i <= fillbox_upper(0); i++) {

                int phi_idx = (i - phi_ghostbox_lower(0))
                            + (j - phi_ghostbox_lower(1))
                             *phi_ghostbox_num_cells_x
                            + (k - phi_ghostbox_lower(2))
                             *phi_ghostbox_num_cells_x
                             *phi_ghostbox_num_cells_y;

                phi[phi_idx] *= -1.0;  // flip sign of boundary data
                
              }
            }
          } // end loop over grid

        } // end loop over components

      } // end case: boundary is a periodic direction for the
        //           level set functions

    } // end loop over face boundaries


    /* 
     * fill edge boundaries
     */
   const std::vector<BoundaryBox> edge_bdry =
      d_boundary_boxes[level_num].find(boxid)->second[1];//dimensionality of a edge??
    for (unsigned int i = 0; i < edge_bdry.size(); i++) {

      // check that boundary is a periodic boundary for level
      // set functions
      int bdry_location_idx = edge_bdry[i].getLocationIndex();
      int check_dir_a = -1, check_dir_b = -1; // invalid values
      switch(bdry_location_idx/4) {
        case 0: { // edge lies along x-direction
          check_dir_a = 1;
          check_dir_b = 2;
          break;
        }
        case 1: { // edge lies along y-direction
          check_dir_a = 2;
          check_dir_b = 0;
          break;
        }
        case 2: { // edge lies along z-direction
          check_dir_a = 0;
          check_dir_b = 1;
          break;
        }
        default: { 
          TBOX_ERROR(  "BoundaryConditionModule::"
                    << "imposeAntiPeriodicBCsOnPatch(): "
                    << "Invalid boundary location index for edge type when DIM = 3"
                    << endl );
        }
      }
      if ( ( (d_geom_periodic_dirs(check_dir_a) &&
              (lower_bc[check_dir_a] == ANTI_PERIODIC)) && 
             !(d_geom_periodic_dirs(check_dir_b) &&
              (lower_bc[check_dir_b] == ANTI_PERIODIC)) ) ||
           (!(d_geom_periodic_dirs(check_dir_a) &&
              (lower_bc[check_dir_a] == ANTI_PERIODIC)) &&
             (d_geom_periodic_dirs(check_dir_b) &&
              (lower_bc[check_dir_b] == ANTI_PERIODIC)) ) ) {

        /*
         * impose anti-periodic boundary conditions for phi 
         */
        Box phi_fillbox = patch_geom->getBoundaryFillBox(
          edge_bdry[i], interior_box, phi_ghost_width_to_fill);
        int phi_ghostbox_num_cells_x = phi_ghostbox.numberCells(0);
        int phi_ghostbox_num_cells_y = phi_ghostbox.numberCells(1);
        IntVector phi_ghostbox_lower = phi_ghostbox.lower();

        IntVector fillbox_lower = phi_fillbox.lower();
        IntVector fillbox_upper = phi_fillbox.upper();

        // loop over components
        for (int comp = comp_lo; comp < comp_hi; comp++) {

          LSMLIB_REAL* phi = phi_data->getPointer(comp);

          for (int k = fillbox_lower(2); k <= fillbox_upper(2); k++) {
            for (int j = fillbox_lower(1); j <= fillbox_upper(1); j++) {
              for (int i = fillbox_lower(0); i <= fillbox_upper(0); i++) {

                int phi_idx = (i - phi_ghostbox_lower(0))
                            + (j - phi_ghostbox_lower(1))
                             *phi_ghostbox_num_cells_x
                            + (k - phi_ghostbox_lower(2))
                             *phi_ghostbox_num_cells_x
                             *phi_ghostbox_num_cells_y;

                phi[phi_idx] *= -1.0;  // flip sign of boundary data
                
              }
            }
          } // end loop over grid

        } // end loop over components

      } // end case: boundary is a periodic direction for the
        //           level set functions

    } // end loop over edge boundaries

    /* 
     * fill node boundaries
     */
    const std::vector<BoundaryBox> node_bdry =
      d_boundary_boxes[level_num].find(boxid)->second[1];//dimensionality of a node??
    for (unsigned int i = 0; i < node_bdry.size(); i++) {

      // check that boundary is a periodic boundary for level
      // set functions
      bool anti_periodic = false;
      for (int k = 0; k < 3; k++) {
        if ( d_geom_periodic_dirs(k) && 
             (lower_bc[k] == ANTI_PERIODIC) ) 
          anti_periodic = !anti_periodic;
      }

      if ( anti_periodic ) {

        /*
         * impose anti-periodic boundary conditions for phi 
         */
        Box phi_fillbox = patch_geom->getBoundaryFillBox(
          node_bdry[i], interior_box, phi_ghost_width_to_fill);
        int phi_ghostbox_num_cells_x = phi_ghostbox.numberCells(0);
        int phi_ghostbox_num_cells_y = phi_ghostbox.numberCells(1);
        IntVector phi_ghostbox_lower = phi_ghostbox.lower();

        IntVector fillbox_lower = phi_fillbox.lower();
        IntVector fillbox_upper = phi_fillbox.upper();

        // loop over components
        for (int comp = comp_lo; comp < comp_hi; comp++) {

          LSMLIB_REAL* phi = phi_data->getPointer(comp);

          for (int k = fillbox_lower(2); k <= fillbox_upper(2); k++) {
            for (int j = fillbox_lower(1); j <= fillbox_upper(1); j++) {
              for (int i = fillbox_lower(0); i <= fillbox_upper(0); i++) {

                int phi_idx = (i - phi_ghostbox_lower(0))
                            + (j - phi_ghostbox_lower(1))
                             *phi_ghostbox_num_cells_x
                            + (k - phi_ghostbox_lower(2))
                             *phi_ghostbox_num_cells_x
                             *phi_ghostbox_num_cells_y;

                phi[phi_idx] *= -1.0;  // flip sign of boundary data
                
              }
            }
          } // end loop over grid

        } // end loop over components

      } // end case: boundary is a periodic direction for the
        //           level set functions

    } // end loop over node boundaries

// BEGIN DEBUGGING {
/*
    int phi_idx = 0;
    for (int k = 0; k < phi_ghostbox.numberCells(2); k++) {
      for (int j = 0; j < phi_ghostbox.numberCells(1); j++) {
        for (int i = 0; i < phi_ghostbox.numberCells(0); i++) {

           pout << phi[phi_idx] << ",";
           phi_idx++;

        }
        pout << endl;
      }
      pout << "--------------------" << endl;
    } // end loop over grid
*/
// } END DEBUGGING

  } else if (DIM == 2) {

    /*
     * fill edge boundaries
     */
   const std::vector<BoundaryBox> edge_bdry =
      d_boundary_boxes[level_num].find(boxid)->second[1];//dimensionality of an edge??
    for (unsigned int i = 0; i < edge_bdry.size(); i++) {

      // check that boundary is a periodic boundary for level
      // set functions
      int bdry_location_idx = edge_bdry[i].getLocationIndex();
      if ( d_geom_periodic_dirs(bdry_location_idx/2) &&
           (lower_bc[bdry_location_idx/2] == ANTI_PERIODIC) ) { 

        /*
         * impose anti-periodic boundary conditions for phi 
         */
        Box phi_fillbox = patch_geom->getBoundaryFillBox(
          edge_bdry[i], interior_box, phi_ghost_width_to_fill);
        int phi_ghostbox_num_cells_x = phi_ghostbox.numberCells(0);
        IntVector phi_ghostbox_lower = phi_ghostbox.lower();

        IntVector fillbox_lower = phi_fillbox.lower();
        IntVector fillbox_upper = phi_fillbox.upper();

        // loop over components
        for (int comp = comp_lo; comp < comp_hi; comp++) {

          LSMLIB_REAL* phi = phi_data->getPointer(comp);

          for (int j = fillbox_lower(1); j <= fillbox_upper(1); j++) {
            for (int i = fillbox_lower(0); i <= fillbox_upper(0); i++) {

              int phi_idx = (i - phi_ghostbox_lower(0))
                          + (j - phi_ghostbox_lower(1))
                            *phi_ghostbox_num_cells_x;

              phi[phi_idx] *= -1.0;  // flip sign of boundary data

            }
          } // end loop over grid

        } // end loop over components

      } // end case: boundary is a periodic direction for the
        //           level set functions

    } // end loop over edge boundaries


    /* 
     * fill node boundaries
     */
   const std::vector<BoundaryBox> node_bdry =
      d_boundary_boxes[level_num].find(boxid)->second[1];//dimensionality of a node??
    for (unsigned int i = 0; i < node_bdry.size(); i++) {

      // check that boundary is a periodic boundary for level
      // set functions
      if ( ( (d_geom_periodic_dirs(0) && 
             (lower_bc[0] == ANTI_PERIODIC)) &&
            !(d_geom_periodic_dirs(1) &&
             (lower_bc[1] == ANTI_PERIODIC)) ) ||
           (!(d_geom_periodic_dirs(0) && 
             (lower_bc[0] == ANTI_PERIODIC)) &&
             (d_geom_periodic_dirs(1) &&
             (lower_bc[1] == ANTI_PERIODIC)) ) ) {

        /*
         * impose anti-periodic boundary conditions for phi 
         */
        Box phi_fillbox = patch_geom->getBoundaryFillBox(
          node_bdry[i], interior_box, phi_ghost_width_to_fill);
        int phi_ghostbox_num_cells_x = phi_ghostbox.numberCells(0);
        IntVector phi_ghostbox_lower = phi_ghostbox.lower();

        IntVector fillbox_lower = phi_fillbox.lower();
        IntVector fillbox_upper = phi_fillbox.upper();

        // loop over components
        for (int comp = comp_lo; comp < comp_hi; comp++) {

          LSMLIB_REAL* phi = phi_data->getPointer(comp);

          for (int j = fillbox_lower(1); j <= fillbox_upper(1); j++) {
            for (int i = fillbox_lower(0); i <= fillbox_upper(0); i++) {

              int phi_idx = (i - phi_ghostbox_lower(0))
                          + (j - phi_ghostbox_lower(1))
                            *phi_ghostbox_num_cells_x;

              phi[phi_idx] *= -1.0;  // flip sign of boundary data
                
            }
          } // end loop over grid

        } // end loop over components

      } // end case: boundary is a periodic direction for the
        //           level set functions

    } // end loop over node boundaries

// BEGIN DEBUGGING {
/*
    int phi_idx = 0;
    for (int k = 0; k < phi_ghostbox.numberCells(2); k++) {
      for (int j = 0; j < phi_ghostbox.numberCells(1); j++) {
        for (int i = 0; i < phi_ghostbox.numberCells(0); i++) {

           pout << phi[phi_idx] << ",";
           phi_idx++;

        }
        pout << endl;
      }
      pout << "--------------------" << endl;
    } // end loop over grid
*/  
// } END DEBUGGING

  } else if (DIM == 1) {

    /*
     * fill node boundaries
     */
    const std::vector<BoundaryBox> node_bdry =
      d_boundary_boxes[level_num].find(boxid)->second[1];//dimensionality of a node??
    for (unsigned int i = 0; i < node_bdry.size(); i++) {

      // check that boundary is a periodic boundary for level
      // set functions
      int bdry_location_idx = node_bdry[i].getLocationIndex();
      if ( d_geom_periodic_dirs(bdry_location_idx/2) &&
             (lower_bc[bdry_location_idx/2] == ANTI_PERIODIC) ) {

        /*
         * impose anti-periodic boundary conditions for phi 
         */
        Box phi_fillbox = patch_geom->getBoundaryFillBox(
          node_bdry[i], interior_box, phi_ghost_width_to_fill);
        IntVector phi_ghostbox_lower = phi_ghostbox.lower();

        IntVector fillbox_lower = phi_fillbox.lower();
        IntVector fillbox_upper = phi_fillbox.upper();

        // loop over components
        for (int comp = comp_lo; comp < comp_hi; comp++) {

          LSMLIB_REAL* phi = phi_data->getPointer(comp);

          for (int i = fillbox_lower(0); i <= fillbox_upper(0); i++) {

                int phi_idx = i - phi_ghostbox_lower(0);

                phi[phi_idx] *= -1.0;  // flip sign of boundary data
                
          } // end loop over grid

        } // end loop over components

      } // end case: boundary is a periodic direction for the
        //           level set functions

    } // end loop over node boundaries


// BEGIN DEBUGGING {
/*
    int phi_idx = 0;
    for (int k = 0; k < phi_ghostbox.numberCells(2); k++) {
      for (int j = 0; j < phi_ghostbox.numberCells(1); j++) {
        for (int i = 0; i < phi_ghostbox.numberCells(0); i++) {

           pout << phi[phi_idx] << ",";
           phi_idx++;

        }
        pout << endl;
      }
      pout << "--------------------" << endl;
    } // end loop over grid
*/
// } END DEBUGGING

  } else { 

    TBOX_ERROR(  "BoundaryConditionModule::"
              << "imposeAntiPeriodicBCsOnPatch()"
              << "Invalid value of DIM.  "
              << "Only DIM = 1, 2, and 3 are supported."
              << endl );

  } // end switch over DIM
}


/* imposeHomogeneousNeumannBCs() */
void BoundaryConditionModule::imposeHomogeneousNeumannBCs( 
  const int phi_handle,
  const IntVector& lower_bc,
  const IntVector& upper_bc,
  const SPATIAL_DERIVATIVE_TYPE spatial_derivative_type,
  const int spatial_derivative_order,
  const int component )
{
  // loop over hierarchy and impose homogeneous Neumann BCs
  const int num_levels = d_patch_hierarchy->getNumberOfLevels();
  for ( int ln=0 ; ln < num_levels; ln++ ) {

        boost::shared_ptr< PatchLevel> level = d_patch_hierarchy->getPatchLevel(ln);
    for (PatchLevel::Iterator pi(level->begin()); pi!=level->end(); pi++) { // loop over patches
      boost::shared_ptr< Patch > patch = *pi;//returns second patch in line.
      if ( patch==NULL ) {
        TBOX_ERROR(  "BoundaryConditionModule::"
                  << "imposeHomogeneousNeumannBCs(): "
                  << "Cannot find patch. Null patch pointer."
                  << endl);
      }

      // check that patch touches boundary of computational domain
      if ( d_touches_boundary[ln][patch->getLocalId().getValue()] ) {

        imposeHomogeneousNeumannBCsOnPatch(*patch,
                                           phi_handle,
                                           lower_bc,
                                           upper_bc,
                                           spatial_derivative_type,
                                           spatial_derivative_order,
                                           component);

      } // end check: Patch touches computational boundary

    } // end loop over Patches
  } // end loop over PatchLevels
}



/* imposeHomogeneousNeumannBCsOnPatch() */
void BoundaryConditionModule::imposeHomogeneousNeumannBCsOnPatch( 
  Patch& patch,
  const int phi_handle,
  const IntVector& lower_bc,
  const IntVector& upper_bc,
  const SPATIAL_DERIVATIVE_TYPE spatial_derivative_type,
  const int spatial_derivative_order,
  const int component )
{
  // get patch level number and patch number
  int level_num = patch.getPatchLevelNumber();

  // get PatchData
  boost::shared_ptr< CellData<LSMLIB_REAL> > phi_data =
    BOOST_CAST<CellData<LSMLIB_REAL>, PatchData>(
    patch.getPatchData( phi_handle ));

  // check that the ghostcell width for phi is compatible
  // with the ghostcell width
  if (d_ghostcell_width != phi_data->getGhostCellWidth()) {
    TBOX_ERROR(  "BoundaryConditionModule::"
              << "imposeHomogeneousNeumannBCsOnPatch(): "
              << "ghostcell width for phi_handle is incompatible "
              << "with ghostcell width for this object."
              << endl );
  }

  // get PatchGeometry
  boost::shared_ptr< CartesianPatchGeometry > patch_geom =
    BOOST_CAST <CartesianPatchGeometry, PatchGeometry>(patch.getPatchGeometry());

  // get interior box for patch (used to compute boundary fill box)
  Box interior_box(patch.getBox());
  IntVector interior_box_lower = interior_box.lower();
  IntVector interior_box_upper = interior_box.upper();

  // get ghostcell width to fill
  IntVector phi_ghost_width_to_fill = phi_data->getGhostCellWidth();
  Box phi_ghostbox = phi_data->getGhostBox();
  IntVector phi_ghostbox_lower = phi_ghostbox.lower();
  IntVector phi_ghostbox_upper = phi_ghostbox.upper();

  // get data components
  int comp_lo = component;
  int comp_hi = component+1;
  if (component < 0) {
    comp_lo = 0;
    comp_hi = phi_data->getDepth();
  }
  BoxId boxid = patch.getBox().getBoxId();
  int DIM = d_patch_hierarchy->getDim().getValue();
  const std::vector<BoundaryBox> bdry_boxes =
    d_boundary_boxes[level_num].find(boxid)->second[DIM-1];//dimensionality of a box??
  for (unsigned int i = 0; i < bdry_boxes.size(); i++) {

    // check that boundary is homogeneous Neumann boundary
    int bdry_loc_idx = bdry_boxes[i].getLocationIndex();
    if ( ((bdry_loc_idx%2==0) && 
          (lower_bc[bdry_loc_idx/2] == HOMOGENEOUS_NEUMANN)) ||
         ((bdry_loc_idx%2==1) && 
          (upper_bc[bdry_loc_idx/2] == HOMOGENEOUS_NEUMANN)) ) {

      switch (spatial_derivative_type) {
        case ENO: {
          switch (spatial_derivative_order) {
            case 1: {

              if ( DIM == 3 ) {

                for (int comp = comp_lo; comp < comp_hi; comp++) {

                  LSMLIB_REAL* phi = phi_data->getPointer(comp);

                  LSM3D_HOMOGENEOUS_NEUMANN_ENO1(
                    phi,
                    &phi_ghostbox_lower[0],
                    &phi_ghostbox_upper[0],
                    &phi_ghostbox_lower[1],
                    &phi_ghostbox_upper[1],
                    &phi_ghostbox_lower[2],
                    &phi_ghostbox_upper[2],
                    &interior_box_lower[0],
                    &interior_box_upper[0],
                    &interior_box_lower[1],
                    &interior_box_upper[1],
                    &interior_box_lower[2],
                    &interior_box_upper[2],
                    &bdry_loc_idx);

                } // end loop over components of PatchData

              } else if ( DIM == 2 ) {

                for (int comp = comp_lo; comp < comp_hi; comp++) {

                  LSMLIB_REAL* phi = phi_data->getPointer(comp);

                  LSM2D_HOMOGENEOUS_NEUMANN_ENO1(
                    phi,
                    &phi_ghostbox_lower[0],
                    &phi_ghostbox_upper[0],
                    &phi_ghostbox_lower[1],
                    &phi_ghostbox_upper[1],
                    &interior_box_lower[0],
                    &interior_box_upper[0],
                    &interior_box_lower[1],
                    &interior_box_upper[1],
                    &bdry_loc_idx);

                } // end loop over components of PatchData

              } else if ( DIM == 1 ) {

                for (int comp = comp_lo; comp < comp_hi; comp++) {

                  LSMLIB_REAL* phi = phi_data->getPointer(comp);

                  LSM1D_HOMOGENEOUS_NEUMANN_ENO1(
                    phi,
                    &phi_ghostbox_lower[0],
                    &phi_ghostbox_upper[0],
                    &interior_box_lower[0],
                    &interior_box_upper[0],
                    &bdry_loc_idx);

                } // end loop over components of PatchData

              } else {

                TBOX_ERROR(  "BoundaryConditionModule::"
                          << "imposeHomogeneousNeumannBCsOnPatch(): "
                          << "Invalid value of DIM.  "
                          << "Only DIM = 1, 2, and 3 are supported."
                          << endl );

              } // end switch over dimensions

              break;
            } // end case: ENO1

            case 2: {

              if ( DIM == 3 ) {

                for (int comp = comp_lo; comp < comp_hi; comp++) {

                  LSMLIB_REAL* phi = phi_data->getPointer(comp);

                  LSM3D_HOMOGENEOUS_NEUMANN_ENO2(
                    phi,
                    &phi_ghostbox_lower[0],
                    &phi_ghostbox_upper[0],
                    &phi_ghostbox_lower[1],
                    &phi_ghostbox_upper[1],
                    &phi_ghostbox_lower[2],
                    &phi_ghostbox_upper[2],
                    &interior_box_lower[0],
                    &interior_box_upper[0],
                    &interior_box_lower[1],
                    &interior_box_upper[1],
                    &interior_box_lower[2],
                    &interior_box_upper[2],
                    &bdry_loc_idx);

                } // end loop over components of PatchData

              } else if ( DIM == 2 ) {

                for (int comp = comp_lo; comp < comp_hi; comp++) {

                  LSMLIB_REAL* phi = phi_data->getPointer(comp);

                  LSM2D_HOMOGENEOUS_NEUMANN_ENO2(
                    phi,
                    &phi_ghostbox_lower[0],
                    &phi_ghostbox_upper[0],
                    &phi_ghostbox_lower[1],
                    &phi_ghostbox_upper[1],
                    &interior_box_lower[0],
                    &interior_box_upper[0],
                    &interior_box_lower[1],
                    &interior_box_upper[1],
                    &bdry_loc_idx);

                } // end loop over components of PatchData

              } else if ( DIM == 1 ) {

                for (int comp = comp_lo; comp < comp_hi; comp++) {

                  LSMLIB_REAL* phi = phi_data->getPointer(comp);

                  LSM1D_HOMOGENEOUS_NEUMANN_ENO2(
                    phi,
                    &phi_ghostbox_lower[0],
                    &phi_ghostbox_upper[0],
                    &interior_box_lower[0],
                    &interior_box_upper[0],
                    &bdry_loc_idx);

                } // end loop over components of PatchData

              } else {

                TBOX_ERROR(  "BoundaryConditionModule::"
                          << "imposeHomogeneousNeumannBCsOnPatch(): "
                          << "Invalid value of DIM.  "
                          << "Only DIM = 1, 2, and 3 are supported."
                          << endl );

              } // end switch over dimensions

              break;
            } // end case: ENO2

            case 3: {

              if ( DIM == 3 ) {

                for (int comp = comp_lo; comp < comp_hi; comp++) {

                  LSMLIB_REAL* phi = phi_data->getPointer(comp);

                  LSM3D_HOMOGENEOUS_NEUMANN_ENO3(
                    phi,
                    &phi_ghostbox_lower[0],
                    &phi_ghostbox_upper[0],
                    &phi_ghostbox_lower[1],
                    &phi_ghostbox_upper[1],
                    &phi_ghostbox_lower[2],
                    &phi_ghostbox_upper[2],
                    &interior_box_lower[0],
                    &interior_box_upper[0],
                    &interior_box_lower[1],
                    &interior_box_upper[1],
                    &interior_box_lower[2],
                    &interior_box_upper[2],
                    &bdry_loc_idx);

                } // end loop over components of PatchData

              } else if ( DIM == 2 ) {

                for (int comp = comp_lo; comp < comp_hi; comp++) {

                  LSMLIB_REAL* phi = phi_data->getPointer(comp);

                  LSM2D_HOMOGENEOUS_NEUMANN_ENO3(
                    phi,
                    &phi_ghostbox_lower[0],
                    &phi_ghostbox_upper[0],
                    &phi_ghostbox_lower[1],
                    &phi_ghostbox_upper[1],
                    &interior_box_lower[0],
                    &interior_box_upper[0],
                    &interior_box_lower[1],
                    &interior_box_upper[1],
                    &bdry_loc_idx);

                } // end loop over components of PatchData

              } else if ( DIM == 1 ) {

                for (int comp = comp_lo; comp < comp_hi; comp++) {

                  LSMLIB_REAL* phi = phi_data->getPointer(comp);

                  LSM1D_HOMOGENEOUS_NEUMANN_ENO3(
                    phi,
                    &phi_ghostbox_lower[0],
                    &phi_ghostbox_upper[0],
                    &interior_box_lower[0],
                    &interior_box_upper[0],
                    &bdry_loc_idx);

                } // end loop over components of PatchData

              } else {

                TBOX_ERROR(  "BoundaryConditionModule::"
                          << "imposeHomogeneousNeumannBCsOnPatch(): "
                          << "Invalid value of DIM.  "
                          << "Only DIM = 1, 2, and 3 are supported."
                          << endl );

              } // end switch over dimensions

              break;
            } // end case: ENO3

            default: {
              TBOX_ERROR(  "BoundaryConditionModule::"
                        << "imposeHomogeneousNeumannBCsOnPatch(): "
                        << "Unsupported order for ENO derivative.  "
                        << "Only ENO1, ENO2, and ENO3 supported."  
                        << endl );
 
            }
          } // end switch on spatial derivative order

          break;
        } // end case: ENO

      case WENO: {
          switch (spatial_derivative_order) {
            case 5: {

              if ( DIM == 3 ) {

                for (int comp = comp_lo; comp < comp_hi; comp++) {

                  LSMLIB_REAL* phi = phi_data->getPointer(comp);

                  LSM3D_HOMOGENEOUS_NEUMANN_WENO5(
                    phi,
                    &phi_ghostbox_lower[0],
                    &phi_ghostbox_upper[0],
                    &phi_ghostbox_lower[1],
                    &phi_ghostbox_upper[1],
                    &phi_ghostbox_lower[2],
                    &phi_ghostbox_upper[2],
                    &interior_box_lower[0],
                    &interior_box_upper[0],
                    &interior_box_lower[1],
                    &interior_box_upper[1],
                    &interior_box_lower[2],
                    &interior_box_upper[2],
                    &bdry_loc_idx);

                } // end loop over components of PatchData

              } else if ( DIM == 2 ) {

                for (int comp = comp_lo; comp < comp_hi; comp++) {

                  LSMLIB_REAL* phi = phi_data->getPointer(comp);

                  LSM2D_HOMOGENEOUS_NEUMANN_WENO5(
                    phi,
                    &phi_ghostbox_lower[0],
                    &phi_ghostbox_upper[0],
                    &phi_ghostbox_lower[1],
                    &phi_ghostbox_upper[1],
                    &interior_box_lower[0],
                    &interior_box_upper[0],
                    &interior_box_lower[1],
                    &interior_box_upper[1],
                    &bdry_loc_idx);

                } // end loop over components of PatchData

              } else if ( DIM == 1 ) {

                for (int comp = comp_lo; comp < comp_hi; comp++) {

                  LSMLIB_REAL* phi = phi_data->getPointer(comp);

                  LSM1D_HOMOGENEOUS_NEUMANN_WENO5(
                    phi,
                    &phi_ghostbox_lower[0],
                    &phi_ghostbox_upper[0],
                    &interior_box_lower[0],
                    &interior_box_upper[0],
                    &bdry_loc_idx);

                } // end loop over components of PatchData

              } else {

                TBOX_ERROR(  "BoundaryConditionModule::"
                          << "imposeHomogeneousNeumannBCsOnPatch(): "
                          << "Invalid value of DIM.  "
                          << "Only DIM = 1, 2, and 3 are supported."
                          << endl );

              } // end switch over dimensions

              break;
            } // end case: WENO5

            default: {
              TBOX_ERROR(  "BoundaryConditionModule::"
                           << "imposeHomogeneousNeumannBCsOnPatch(): "
                           << "Unsupported order for WENO derivative.  "
                           << "Only WENO5 supported."
                           << endl );
            }

          } // end switch on spatial derivative order

          break;

        } // end case: WENO

        default: {
          TBOX_ERROR(  "BoundaryConditionModule::"
                    << "imposeHomogeneousNeumannBCsOnPatch(): "
                    << "Unsupported spatial derivative type.  "
                    << "Only ENO and WENO derivatives are supported."
                    << endl );
        } 

      } // end switch on spatial derivative type

    } // end check that boundary is homogeneous Neumann boundary
  } // end loop over boundary boxes
}


/* imposeLinearExtrapolationBCs() */
void BoundaryConditionModule::imposeLinearExtrapolationBCs( 
  const int phi_handle,
  const IntVector& lower_bc,
  const IntVector& upper_bc,
  const int component )
{
  // loop over hierarchy and impose homogeneous Neumann BCs
    const int num_levels = d_patch_hierarchy->getNumberOfLevels();
  for ( int ln=0 ; ln < num_levels; ln++ ) {

    boost::shared_ptr< PatchLevel> level = d_patch_hierarchy->getPatchLevel(ln);
    for (PatchLevel::Iterator pi(level->begin()); pi!=level->end(); pi++) { // loop over patches
      boost::shared_ptr< Patch > patch = *pi;//returns second patch in line.
      if ( patch==NULL ) {
        TBOX_ERROR(  "BoundaryConditionModule::"
                  << "imposeLinearExtrapolationBCs(): "
                  << "Cannot find patch. Null patch pointer."
                  << endl);
      }

      // check that patch touches boundary of computational domain
      if ( d_touches_boundary[ln][patch->getLocalId().getValue()] ) {

        imposeLinearExtrapolationBCsOnPatch(*patch,
                                            phi_handle,
                                            lower_bc,
                                            upper_bc,
                                            component);

      } // end check: Patch touches computational boundary

    } // end loop over Patches
  } // end loop over PatchLevels
}



/* imposeLinearExtrapolationBCsOnPatch() */
void BoundaryConditionModule::imposeLinearExtrapolationBCsOnPatch( 
  Patch& patch,
  const int phi_handle,
  const IntVector& lower_bc,
  const IntVector& upper_bc,
  const int component )
{
  // get patch level number and patch number
  int level_num = patch.getPatchLevelNumber();
  
// get PatchData
  boost::shared_ptr< CellData<LSMLIB_REAL> > phi_data =
    BOOST_CAST<CellData<LSMLIB_REAL>, PatchData>(
    patch.getPatchData( phi_handle ));

  // check that the ghostcell width for phi is compatible
  // with the ghostcell width
  if (d_ghostcell_width != phi_data->getGhostCellWidth()) {
    TBOX_ERROR(  "BoundaryConditionModule::"
              << "imposeLinearExtrapolationBCsOnPatch(): "
              << "ghostcell width for phi_handle is incompatible "
              << "with ghostcell width for this object."
              << endl );
  }

  // get PatchGeometry
  boost::shared_ptr< CartesianPatchGeometry > patch_geom =
    BOOST_CAST <CartesianPatchGeometry, PatchGeometry>(patch.getPatchGeometry());

  // get interior box for patch (used to compute boundary fill box)
  Box interior_box(patch.getBox());
  IntVector interior_box_lower = interior_box.lower();
  IntVector interior_box_upper = interior_box.upper();

  // get ghostcell width to fill
  IntVector phi_ghost_width_to_fill = phi_data->getGhostCellWidth();
  Box phi_ghostbox = phi_data->getGhostBox();
  IntVector phi_ghostbox_lower = phi_ghostbox.lower();
  IntVector phi_ghostbox_upper = phi_ghostbox.upper();

  // get data components
  int comp_lo = component;
  int comp_hi = component+1;
  if (component < 0) {
    comp_lo = 0;
    comp_hi = phi_data->getDepth();
  }

  BoxId boxid = patch.getBox().getBoxId();
  int DIM = d_patch_hierarchy->getDim().getValue();
  const std::vector<BoundaryBox> bdry_boxes =
    d_boundary_boxes[level_num].find(boxid)->second[DIM-1];//dimensionality of a box??
  for (unsigned int i = 0; i < bdry_boxes.size(); i++) {

    // check that boundary is linear extrapolation boundary
    int bdry_loc_idx = bdry_boxes[i].getLocationIndex();
    if ( ((bdry_loc_idx%2==0) && 
          (lower_bc[bdry_loc_idx/2] == LINEAR_EXTRAPOLATION)) ||
         ((bdry_loc_idx%2==1) && 
          (upper_bc[bdry_loc_idx/2] == LINEAR_EXTRAPOLATION)) ) {

      if ( DIM == 3 ) {

        for (int comp = comp_lo; comp < comp_hi; comp++) {

          LSMLIB_REAL* phi = phi_data->getPointer(comp);

          LSM3D_LINEAR_EXTRAPOLATION(
            phi,
            &phi_ghostbox_lower[0],
            &phi_ghostbox_upper[0],
            &phi_ghostbox_lower[1],
            &phi_ghostbox_upper[1],
            &phi_ghostbox_lower[2],
            &phi_ghostbox_upper[2],
            &interior_box_lower[0],
            &interior_box_upper[0],
            &interior_box_lower[1],
            &interior_box_upper[1],
            &interior_box_lower[2],
            &interior_box_upper[2],
            &bdry_loc_idx);

        } // end loop over components of PatchData

      } else if ( DIM == 2 ) {

        for (int comp = comp_lo; comp < comp_hi; comp++) {

          LSMLIB_REAL* phi = phi_data->getPointer(comp);

          LSM2D_LINEAR_EXTRAPOLATION(
            phi,
            &phi_ghostbox_lower[0],
            &phi_ghostbox_upper[0],
            &phi_ghostbox_lower[1],
            &phi_ghostbox_upper[1],
            &interior_box_lower[0],
            &interior_box_upper[0],
            &interior_box_lower[1],
            &interior_box_upper[1],
            &bdry_loc_idx);

        } // end loop over components of PatchData

      } else if ( DIM == 1 ) {

        for (int comp = comp_lo; comp < comp_hi; comp++) {

          LSMLIB_REAL* phi = phi_data->getPointer(comp);

          LSM1D_LINEAR_EXTRAPOLATION(
            phi,
            &phi_ghostbox_lower[0],
            &phi_ghostbox_upper[0],
            &interior_box_lower[0],
            &interior_box_upper[0],
            &bdry_loc_idx);

        } // end loop over components of PatchData

      } else {

        TBOX_ERROR(  "BoundaryConditionModule::"
                  << "imposeLinearExtrapolationBCsOnPatch(): "
                  << "Invalid value of DIM.  "
                  << "Only DIM = 1, 2, and 3 are supported."
                  << endl );

      } // end switch over dimensions
    } // end check that boundary is linear extrapolation boundary
  } // end loop over boundary boxes
}


/* imposeSignedLinearExtrapolationBCs() */
void BoundaryConditionModule::imposeSignedLinearExtrapolationBCs( 
  const int phi_handle,
  const IntVector& lower_bc,
  const IntVector& upper_bc,
  const int component )
{
  // loop over hierarchy and impose homogeneous Neumann BCs
  const int num_levels = d_patch_hierarchy->getNumberOfLevels();
  for ( int ln=0 ; ln < num_levels; ln++ ) {

    boost::shared_ptr< PatchLevel> level = d_patch_hierarchy->getPatchLevel(ln);
    for (PatchLevel::Iterator pi(level->begin()); pi!=level->end(); pi++) { // loop over patches
      boost::shared_ptr< Patch > patch = *pi;//returns second patch in line.
      if ( patch==NULL ) {
        TBOX_ERROR(  "BoundaryConditionModule::"
                  << "imposeSignedLinearExtrapolationBCs(): "
                  << "Cannot find patch. Null patch pointer."
                  << endl);
      }

      // check that patch touches boundary of computational domain
      if ( d_touches_boundary[ln][patch->getLocalId().getValue()] ) {

        imposeSignedLinearExtrapolationBCsOnPatch(*patch,
                                                  phi_handle,
                                                  lower_bc,
                                                  upper_bc,
                                                  component);

      } // end check: Patch touches computational boundary

    } // end loop over Patches
  } // end loop over PatchLevels
}



/* imposeSignedLinearExtrapolationBCsOnPatch() */
void BoundaryConditionModule::imposeSignedLinearExtrapolationBCsOnPatch( 
  Patch& patch,
  const int phi_handle,
  const IntVector& lower_bc,
  const IntVector& upper_bc,
  const int component )
{
  // get patch level number and patch number
  int level_num = patch.getPatchLevelNumber();
  
  // get PatchData
  boost::shared_ptr< CellData<LSMLIB_REAL> > phi_data =
    BOOST_CAST<CellData<LSMLIB_REAL>, PatchData>(
    patch.getPatchData( phi_handle ));

  // check that the ghostcell width for phi is compatible
  // with the ghostcell width
  if (d_ghostcell_width != phi_data->getGhostCellWidth()) {
    TBOX_ERROR(  "BoundaryConditionModule::"
              << "imposeSignedLinearExtrapolationBCsOnPatch(): "
              << "ghostcell width for phi_handle is incompatible "
              << "with ghostcell width for this object."
              << endl );
  }

  // get PatchGeometry
  boost::shared_ptr< CartesianPatchGeometry > patch_geom =
    BOOST_CAST <CartesianPatchGeometry, PatchGeometry>(patch.getPatchGeometry());

  // get interior box for patch (used to compute boundary fill box)
  Box interior_box(patch.getBox());
  IntVector interior_box_lower = interior_box.lower();
  IntVector interior_box_upper = interior_box.upper();

  // get ghostcell width to fill
  IntVector phi_ghost_width_to_fill = phi_data->getGhostCellWidth();
  Box phi_ghostbox = phi_data->getGhostBox();
  IntVector phi_ghostbox_lower = phi_ghostbox.lower();
  IntVector phi_ghostbox_upper = phi_ghostbox.upper();

  // get data components
  int comp_lo = component;
  int comp_hi = component+1;
  if (component < 0) {
    comp_lo = 0;
    comp_hi = phi_data->getDepth();
  }

  BoxId boxid = patch.getBox().getBoxId();
  int DIM = d_patch_hierarchy->getDim().getValue();
  const std::vector<BoundaryBox> bdry_boxes =
    d_boundary_boxes[level_num].find(boxid)->second[DIM-1];//dimensionality of a box??
  for (unsigned int i = 0; i < bdry_boxes.size(); i++) {

    // check that boundary is linear extrapolation boundary
    int bdry_loc_idx = bdry_boxes[i].getLocationIndex();
    if ( ((bdry_loc_idx%2==0) && 
          (lower_bc[bdry_loc_idx/2] == SIGNED_LINEAR_EXTRAPOLATION)) ||
         ((bdry_loc_idx%2==1) && 
          (upper_bc[bdry_loc_idx/2] == SIGNED_LINEAR_EXTRAPOLATION)) ) {

      if ( DIM == 3 ) {

        for (int comp = comp_lo; comp < comp_hi; comp++) {

          LSMLIB_REAL* phi = phi_data->getPointer(comp);

          LSM3D_SIGNED_LINEAR_EXTRAPOLATION(
            phi,
            &phi_ghostbox_lower[0],
            &phi_ghostbox_upper[0],
            &phi_ghostbox_lower[1],
            &phi_ghostbox_upper[1],
            &phi_ghostbox_lower[2],
            &phi_ghostbox_upper[2],
            &interior_box_lower[0],
            &interior_box_upper[0],
            &interior_box_lower[1],
            &interior_box_upper[1],
            &interior_box_lower[2],
            &interior_box_upper[2],
            &bdry_loc_idx);

        } // end loop over components of PatchData

      } else if ( DIM == 2 ) {

        for (int comp = comp_lo; comp < comp_hi; comp++) {

          LSMLIB_REAL* phi = phi_data->getPointer(comp);

          LSM2D_SIGNED_LINEAR_EXTRAPOLATION(
            phi,
            &phi_ghostbox_lower[0],
            &phi_ghostbox_upper[0],
            &phi_ghostbox_lower[1],
            &phi_ghostbox_upper[1],
            &interior_box_lower[0],
            &interior_box_upper[0],
            &interior_box_lower[1],
            &interior_box_upper[1],
            &bdry_loc_idx);

        } // end loop over components of PatchData

      } else if ( DIM == 1 ) {

        for (int comp = comp_lo; comp < comp_hi; comp++) {

          LSMLIB_REAL* phi = phi_data->getPointer(comp);

          LSM1D_SIGNED_LINEAR_EXTRAPOLATION(
            phi,
            &phi_ghostbox_lower[0],
            &phi_ghostbox_upper[0],
            &interior_box_lower[0],
            &interior_box_upper[0],
            &bdry_loc_idx);

        } // end loop over components of PatchData

      } else {

        TBOX_ERROR(  "BoundaryConditionModule::"
                  << "imposeSignedLinearExtrapolationBCsOnPatch(): "
                  << "Invalid value of DIM.  "
                  << "Only DIM = 1, 2, and 3 are supported."
                  << endl );

      } // end switch over dimensions
    } // end check that boundary is linear extrapolation boundary
  } // end loop over boundary boxes
}


/* resetHierarchyConfiguration() */
void BoundaryConditionModule::resetHierarchyConfiguration(
  const boost::shared_ptr< PatchHierarchy > patch_hierarchy,
  const int coarsest_level,
  const int finest_level,
  const IntVector& ghostcell_width)
{
  // reset hierarchy
  d_patch_hierarchy = patch_hierarchy;
  // reset ghostcell width and periodic directions
  d_ghostcell_width = ghostcell_width;
  //Initialize with ratio to level 0 equal to 1 
  IntVector ratio_to_level_zero(d_patch_hierarchy->getDim(),1); 
  d_geom_periodic_dirs = 
    d_patch_hierarchy->getGridGeometry()->getPeriodicShift(ratio_to_level_zero);
  // check if the patch_hierarchy has already been constructed
  const int num_levels = d_patch_hierarchy->getNumberOfLevels();
  if (num_levels <= 0) {

    IntVector zero_int_vect(d_patch_hierarchy->getDim(),0);
    d_ghostcell_width = zero_int_vect;
    d_geom_periodic_dirs = zero_int_vect;
    d_boundary_boxes.setNull();
    d_touches_boundary.setNull();
    return;
  }

  // resize output arrays 
  d_boundary_boxes.resizeArray(num_levels);
  d_touches_boundary.resizeArray(num_levels);
  // get grid geometry
  boost::shared_ptr< GridGeometry > grid_geometry = 
   BOOST_CAST<CartesianGridGeometry, BaseGridGeometry>(d_patch_hierarchy->getGridGeometry());

  // loop over PatchHierarchy and compute boundary boxes in periodic
  // directions
  for (int ln = coarsest_level; ln <= finest_level; ln++) {
    boost::shared_ptr< PatchLevel > level = d_patch_hierarchy->getPatchLevel(ln);
    const int num_patches = level->getNumberOfPatches();

    // set periodic shift to zero so that ALL boundary boxes
    // (including periodic boundaries) are computed
    IntVector periodic_shift(d_patch_hierarchy->getDim(),0);
    // find the patches in the current level touching the boundaries
    // of the computational domain
    std::map<BoxId,TwoDimBool> touches_regular_bdry;
    std::map<BoxId,TwoDimBool> touches_periodic_bdry;
    grid_geometry->findPatchesTouchingBoundaries(
      touches_regular_bdry,
      touches_periodic_bdry,
      *level);
    d_touches_boundary[ln].resizeArray(num_patches);
    int DIM = d_patch_hierarchy->getDim().getValue();
    for (PatchLevel::Iterator pi(level->begin()); pi!=level->end(); pi++) { // loop over patches
      BoxId boxid = pi->getBox().getBoxId();
      int patch_num = pi->getLocalId().getValue();
      
      d_touches_boundary[ln][patch_num] = false;

     for (int dim = 0; dim < DIM; dim++) {//find if the patch touches a boundary
        if ( touches_regular_bdry.find(boxid)->second(dim,0)  ||
             touches_regular_bdry.find(boxid)->second(dim,1)  ||
             touches_periodic_bdry.find(boxid)->second(dim,0) ||
             touches_periodic_bdry.find(boxid)->second(dim,1) ) {

          d_touches_boundary[ln][patch_num] = true;

        } 
      } // end loop over dimension
    }

    // compute boundary boxes for patches in current level
    grid_geometry->computeBoundaryBoxesOnLevel(
      d_boundary_boxes[ln],
      *level,
      periodic_shift,
      d_ghostcell_width,
      level->getPhysicalDomainArray(),
      true);  // true indicates that boundary boxes should be computed for
              // ALL patches (including those touching periodic boundaries)

  } // end loop over PatchLevels
}

} // end LSMLIB namespace

#endif
