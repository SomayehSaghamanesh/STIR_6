//
// $Id$
//

/*! 
  \file 
  \ingroup reconstructors
  \brief serial FBP3DRP reconstruction implementation
  \author Claire LABBE
  \author Kris Thielemans
  \author PARAPET project
  $Date$
  $Revision$
*/
/*
    Copyright (C) 2000 PARAPET partners
    Copyright (C) 2000- $Date$, IRSL
    See STIR/LICENSE.txt for details
*/

/*
 Modification history:
 KT&SM 05/05/2000
 - corrected bug in z_position while determining rmin,rmax
   (previous result was wrong for span>1)
 - corrected bug in virtual_ring_offset in case of images with an even number
   of planes
 - some adjustements to allow for even number of planes
 - make sure that everything works when there are no missing projections 
   in the data (i.e. rmin>rmin_orig)
 KT 11/04/2000
 - always use rmin,rmax, and not rmin-1, rmax+1. 
   Previously, the larger range was used only in forward projection,
   but not in backprojection -> waste of CPU time
 - removed (old!) bug by adjusting range for rmin (and hence rmax)
   to use 'floor' instead of 'ceil'. Result was that sometimes 1 
   missing projection was not filled in. So, better axial uniformity now.
 - moved rmin,rmax determination to a separate function, as this is now more complicated
   They are now determined in virtual_ring_units, even for the span case. span case
   works now correctly !
 - made 'already 2D images' case work again (aside from fitting)
 - replaced output to cout,cerr with full_log
 - flagged some things that won't work with calls to error()

 KT&CL 160899
 3 changes that solve the dependency of the global normalisation
 on max_delta:
 -changed parameter of Colsher from max_delta+1 to max_delta
 - add scaling factors according to num_ring_differences_in_this_segment
 - approximate analytic integral over delta by having a 1/2 in the
 backprojection of the last segment
 other:
  
 allow a max_segment_num_to_process
 getting things ready for fitting estimated sinograms
 various renaming of variables, reordering of parameters in constructor etc.
 update use of parameter_info()
 various other smaller changes
*/
//CL 1st June 1999
// DIstinguish the alpha and Nyquist parameters from RAmp and Colsher filter
// by alpha_ramp and alpha_colsher (i.e for fc)

//CL 15 MARCH 1999
//NOW DONE:
// 1. Remove the write_PSOV_interfile_header as it is not needed
// 2. Change the defaut value of mashing to 1 instead of 0
// 3. Change the fovrad formula for calculating rmin and rmax
//    by setting the number of bins of the segment and not of the scanner
// 4. The axial uniformity for data with span >1, seems to be OK except on the 3 fisrt and last  planes
// 5. Drastic chnages in filtColsher.cxx as the definition of the
//    Colsher filter is done out of the view loop. This will speed up the
//    FBP3DRP implementation

//
// CL 20/10/98 CHnage theta0 to theta_max for the maximal aperture allowable for reconstruction
// Also, replaced theta by gamma to be consistent with Egger PhD as theta is a very confused parameter



#include "stir/RelatedViewgrams.h"
#include "stir/VoxelsOnCartesianGrid.h"
#include "stir/SegmentBySinogram.h"
#include "stir/Sinogram.h"
//#include "stir/CartesianCoordinate3D.h"
#include "stir/IndexRange3D.h"
#include "stir/Coordinate3D.h"

#include "local/stir/FBP3DRP/ColsherFilter.h" 
#include "stir/display.h"
#include "stir/IO/interfile.h" 
#include "stir/recon_buildblock/distributable.h"
//#include "local/stir/FBP3DRP/process_viewgrams.h"

#include "local/stir/FBP3DRP/FBP3DRPReconstruction.h"
#include "local/stir/FBP2D/FBP2DReconstruction.h"
#include "stir/ProjDataInfoCylindricalArcCorr.h"

#include "stir/SSRB.h"
#include "stir/ProjDataInMemory.h"
#include "stir/recon_buildblock/BackProjectorByBinUsingInterpolation.h"
#include "stir/recon_buildblock/ForwardProjectorByBinUsingRayTracing.h"
//#include "stir/mash_views.h"

#include <fstream>
#include <iostream>
#include <numeric>
#include <string> 
// for asctime()
#include <ctime>

#ifndef STIR_NO_NAMESPACE
using std::cerr;
using std::endl;
using std::ofstream;

#endif

START_NAMESPACE_STIR



// should be private member, TODO
static ofstream full_log;

static void find_rmin_rmax(int& rmin, int& rmax, 
                           const ProjDataInfoCylindricalArcCorr& proj_data_info_cyl,
                           const int seg_num, 
                           const VoxelsOnCartesianGrid<float>& image)
{
  
  const float fovrad = 
    ((int) (proj_data_info_cyl.get_num_tangential_poss()/2)-1)*proj_data_info_cyl.get_tangential_sampling();
  // Compute minimum and maximum rings of 'missing' projections
   
  const float delta=proj_data_info_cyl.get_average_ring_difference(seg_num);
  
  // find correspondence between ring coordinates and image coordinates:
  // z = num_planes_per_virtual_ring * ring + virtual_ring_offset
  // compute the offset by matching up the centre of the scanner 
  // in the 2 coordinate systems
  // TODO get all this from ProjDataInfo or so

  const int num_planes_per_virtual_ring =
    (proj_data_info_cyl.get_max_ring_difference(seg_num) == proj_data_info_cyl.get_min_ring_difference(seg_num)) ? 2 : 1;
  const int num_virtual_rings_per_physical_ring =
    (proj_data_info_cyl.get_max_ring_difference(seg_num) == proj_data_info_cyl.get_min_ring_difference(seg_num)) ? 1 : 2;
  

   const float virtual_ring_offset = 
    (image.get_max_z() + image.get_min_z())/2.F
    - num_planes_per_virtual_ring
    *(proj_data_info_cyl.get_max_axial_pos_num(seg_num)+ num_virtual_rings_per_physical_ring*delta 
    + proj_data_info_cyl.get_min_axial_pos_num(seg_num))/2;
  
  
  // we first consider the LOR at s=0, phi=0 which goes through z=0,y=0, x=fovrad
  // later on, we will shift it to the 'left'most edge of the FOV.
  
  // find z position of intersection of this LOR with the detector radius 
  // (i.e. y=0, x=-ring_radius)
  // use image coordinates first
  float z_in_image_coordinates =
    -delta*num_planes_per_virtual_ring*num_virtual_rings_per_physical_ring*
    (fovrad + proj_data_info_cyl.get_ring_radius())/(2*proj_data_info_cyl.get_ring_radius());
  // now shift it to the edge of the FOV 
  // (taking into account that z==get_min_z() is in the middle of the voxel)
  z_in_image_coordinates += image.get_min_z() - .5;
  
  // now convert to virtual_ring_coordinates using z = num_planes_per_virtual_ring * ring + virtual_ring_offset
  const float z_in_virtual_ring_coordinates = 
    (z_in_image_coordinates - virtual_ring_offset)/num_planes_per_virtual_ring;

  // finally find the 'ring' number
  rmin = static_cast<int>(floor(z_in_virtual_ring_coordinates));
  
  
  // rmax is determined by using symmetry: at both ends there are just as many missing rings 
  rmax =  proj_data_info_cyl.get_max_axial_pos_num(seg_num) + (proj_data_info_cyl.get_min_axial_pos_num(seg_num) - rmin);
}

FBP3DRPReconstruction::~FBP3DRPReconstruction()
{}

VoxelsOnCartesianGrid<float>&  
FBP3DRPReconstruction::estimated_image()
{
  return static_cast<VoxelsOnCartesianGrid<float>&>(*image_estimate_density_ptr);
}

const VoxelsOnCartesianGrid<float>&  
FBP3DRPReconstruction::estimated_image() const
{
  return static_cast<const VoxelsOnCartesianGrid<float>&>(*image_estimate_density_ptr);
}

FBP3DRPReconstruction::
FBP3DRPReconstruction(const string& parameter_filename)
: FBP3DRPParameters(parameter_filename)
{  
  proj_data_info_cyl =
    dynamic_cast<const ProjDataInfoCylindricalArcCorr&> (*(proj_data_ptr->get_proj_data_info_ptr()));
  cerr<<parameter_info() << endl;
}

FBP3DRPReconstruction::FBP3DRPReconstruction(const FBP3DRPParameters& parameters)
  : FBP3DRPParameters(parameters)
{
  proj_data_info_cyl =
    dynamic_cast<const ProjDataInfoCylindricalArcCorr&> (*(proj_data_ptr->get_proj_data_info_ptr()));

  cerr<<parameter_info()  << endl;
}



string FBP3DRPReconstruction::parameter_info () 
{ 
  return FBP3DRPParameters::parameter_info();
}

Succeeded FBP3DRPReconstruction::reconstruct()
{
  return Reconstruction::reconstruct();
}

Succeeded FBP3DRPReconstruction::reconstruct(shared_ptr<DiscretisedDensity<3,float> > const& target_image_ptr)
{
  VoxelsOnCartesianGrid<float>& image =
    dynamic_cast<VoxelsOnCartesianGrid<float> &>(*target_image_ptr);
  // set default values such that it will work also in the case of already_2D_recon
  alpha_fit = 1.0F;
  beta_fit = 0.0F;

  if (PadS<2 || PadZ<2)
    warning("WARNING: PadS=1 (or PadZ=1) should ONLY be used when the non-zero data \n\
occupy only half of the FOV. Otherwise aliasing will occur!\n");

  start_timers();

  {
    // set projectors to be used for the calculations
    // TODO get type and parameters for projectors from *Parameters
    // TODO this really should take a proj_data_info which has more axial positions 
    // (for the 'missing projections')
    shared_ptr<ForwardProjectorByBin> forward_projector_ptr =
      new ForwardProjectorByBinUsingRayTracing(proj_data_ptr->get_proj_data_info_ptr()->clone(), 
					       target_image_ptr);
    shared_ptr<BackProjectorByBin> back_projector_ptr =
      new BackProjectorByBinUsingInterpolation(proj_data_ptr->get_proj_data_info_ptr()->clone(), 
                                               target_image_ptr,
                                               /*use_piecewise_linear_interpolation = */false, 
                                               /*use_exact_Jacobian = */ false);
    set_projectors_and_symmetries(forward_projector_ptr, 
                                  back_projector_ptr, 
                                  back_projector_ptr->get_symmetries_used()->clone());
  }
  {
    //char file[max_filename_length];
    //sprintf(file,"%s.full_log",output_filename_prefix.c_str());
    string file = output_filename_prefix;
    file += ".full_log";
    full_log.open(file.c_str(), ios::out);
    if (!full_log)
      error("Couldn't open full_log file %s", file.c_str());
  }

  full_log << "\n\n********** PROCESSING FBP3DRP RECONSTRUCTION *************" << endl;
  
  const int old_max_segment_num_to_process = max_segment_num_to_process;
  
  // Use funny convention that -1 means 'use maximum available'
  if (max_segment_num_to_process<0)
    max_segment_num_to_process = proj_data_ptr->get_max_segment_num();
  
  if(image_for_reprojection_filename == "")
  {
    do_2D_reconstruction();
    
#if 0      
    if (fit_projections==1){//Fitting between measured and estimaed sinograms
      full_log << "  - Fitting projections" << endl;  //CL 010699 Forward project measured sinograms and fitting with alpha and beta
      
      // From the paper of  M. Defrise et al., Phys. Med. Biol. 1990 Vol. 35, No 10, pp1361-1372
      // As the forwrad projected sinograms have, in general, a different scaling factor
      // to the measured sinograms, it is necessary to rescale the former.
      // The appropriate scaling factors are obtained by forwarding projecting
      // a typical measured sinogram and then fitting the forward projected sinogram
      // to the corresponding measurements using a linear least squares methods
      // The coefficients from the fit are then used to rescale all forward projected sinograms
      
      //  Computes the global coefficients alpha and beta for the best fit of alpha*calculated+beta with measured.
      // Look for the plane of the highest activity in the measured sinograms
      int plane_with_max_activity =0;
      float max_activity = 0.F;
      for (int plane = estimated_image().get_min_z(); plane <= estimated_image().get_max_z();plane++){
        const float current_max = (estimated_image())[plane].find_max();
        if (max_activity < current_max){
          max_activity = current_max;
          plane_with_max_activity = plane;
        }
      }
      {
        full_log << "  - Maximum activity = " << max_activity << " in plane= " << plane_with_max_activity << endl;
        
        //Now forward only on this plane
        Sinogram<float> sino_fwd_pos = 
          direct_sinos_ptr->get_proj_data_info_ptr()->get_empty_sinogram(plane_with_max_activity, 0);
        
        full_log << "    Forward projection on one ring which contains maximum activity from seg0" << endl;
        //KTTODO forward_project_2D(estimated_image(),sino_fwd_pos, plane_with_max_activity);
        error("Fitting not yet implemented\n");
        // Calculate the fitting the coefficients alpha_fit and beta_fit
        // according to alpha_fit x + beta_fit
        do_best_fit(direct_sinos_ptr->get_sinogram(plane_with_max_activity),sino_fwd_pos);
      }
    }else{
      alpha_fit = 1.F;
      beta_fit = 0.F;
    } 
#endif
  } 
  else
  {      
    do_read_image2D();
    // TODO set fit parameters
  }

  if(max_segment_num_to_process!=0)
    do_3D_Reconstruction(image );
  else
    {
      // TODO 
      warning("\nOutput image will NOT be zoomed.\n");
      image = estimated_image();
    }

  stop_timers();
  do_log_file(image);

  full_log.close();
 
  // restore max_segment_num_to_process to its original value,
  // just in case someone wants to use the reconstruction object twice
  max_segment_num_to_process = old_max_segment_num_to_process;

  return Succeeded::yes;
}
   



void FBP3DRPReconstruction::do_2D_reconstruction()
{ // SSRB+2D FBP with ramp filter

  shared_ptr<ProjData> proj_data_to_FBP_ptr = 0;

   int num_segments_to_combine_to_use = num_segments_to_combine;
    if (num_segments_to_combine==-1)
    {
      if (proj_data_info_cyl.get_min_ring_difference(0) != 
          proj_data_info_cyl.get_max_ring_difference(0))
        num_segments_to_combine_to_use = 1;
      else
        num_segments_to_combine_to_use = 3;
    }

    if (num_segments_to_combine_to_use>1)
    {
      full_log << "\n---------------------------------------------------------\n";
      full_log << "SSRB combining " << num_segments_to_combine_to_use 
               << " segments in input file to a new segment 0\n" << endl; 
  
      proj_data_to_FBP_ptr = 
        new ProjDataInMemory (SSRB(proj_data_info_cyl, num_segments_to_combine_to_use,(num_segments_to_combine_to_use-1)/2 ));
      SSRB(*proj_data_to_FBP_ptr, *proj_data_ptr);      
    }
    else
    {
      proj_data_to_FBP_ptr = proj_data_ptr;
    }

  full_log << "\n---------------------------------------------------------\n";
  full_log << "2D FBP OF  DIRECT SINOGRAMS (=> IMAGE_ESTIMATE)\n" << endl; 
  
  
  // image_estimate should have 'default' dimensions, origin and voxel_size
  image_estimate_density_ptr =
    new VoxelsOnCartesianGrid<float>(proj_data_info_cyl);      
  
  {
    // set ramp filter with appropriate sizes
    const int fft_size = (int) pow(2.,(int) ceil(log((double)(PadS + 1)* proj_data_ptr->get_num_tangential_poss()) / log(2.)));
    
    RampFilter ramp_filter(proj_data_info_cyl.get_tangential_sampling(), 
                           fft_size, 
                           float(alpha_ramp), float(fc_ramp));   
    full_log << "Parameters of the filter used in the 2D FBP reconstruction" << endl;
    full_log << ramp_filter.parameter_info()<< endl;
        
    FBP2DReconstruction recon2d(proj_data_to_FBP_ptr, ramp_filter);
    recon2d.reconstruct(image_estimate_density_ptr);
  }

  full_log << "  - min and max in SSRB+FBP image " << estimated_image().find_min()
	   << " " << estimated_image().find_max() << " SUM= " << estimated_image().sum() << endl;
      
  if(disp>1) {
    full_log << "  - Displaying estimated image" << endl;
    display(estimated_image(),estimated_image().find_max(), "Image estimate"); 
  }
            
  if (save_intermediate_files)
    {
      char file[max_filename_length];
      sprintf(file,"%s_estimated",output_filename_prefix.c_str()); 
      do_save_img(file,estimated_image() );      
    }
}


 

void FBP3DRPReconstruction::do_save_img(const char *file, const VoxelsOnCartesianGrid<float> &data)
{              
    full_log <<"  - Saving " << file  << endl;
    write_basic_interfile( file, data);
    full_log << "    Min= " << data.find_min()
         << " Max = " << data.find_max()
         << " Sum = " << data.sum() << endl;

}
   
void FBP3DRPReconstruction::do_read_image2D()
{    
        full_log <<"  - Reading  estimated image : "<< image_for_reprojection_filename << endl;
        
        image_estimate_density_ptr =
          DiscretisedDensity<3,float>::read_from_file(image_for_reprojection_filename.c_str() );

	// TODO do scale checks            
          
}

 

void FBP3DRPReconstruction::do_3D_Reconstruction(
    VoxelsOnCartesianGrid<float> &image)
{

  full_log << "\n---------------------------------------------------------\n";
  full_log << "3D PROCESSING\n" << endl;
  
  // KT segment 0 is now handled in here as well

  int  oblique_segments_start = 0;

 
  do_byview_initialise(image);
  for (int seg_num= oblique_segments_start; seg_num <= max_segment_num_to_process; seg_num++) 
  {
    full_log << "\n--------------------------------\n";
    full_log << "PROCESSING SEGMENT  No " << seg_num << endl ;

    // initialise variables to avoid compiler warnings (correct values are set in the function call)
    int rmin=0;
    int rmax=0;
    find_rmin_rmax(rmin, rmax, proj_data_info_cyl, seg_num, image);        
        
    full_log << "Average delta= " <<  proj_data_info_cyl.get_average_ring_difference(seg_num)
	     << " with span= " << proj_data_info_cyl.get_max_ring_difference(seg_num) - proj_data_info_cyl.get_min_ring_difference(seg_num) +1
	     << " and extended axial position numbers: min= " << rmin << " and max= " << rmax  <<endl;
        
    // KT 07/04/98 changed upper boundary of first forward projection from
    // '-1' to proj_data_ptr->get_min_axial_pos_num(seg_num)-1
    // this is to handle the fancy case that the segment contains more
    // (or less) rings than expected
    // KT 07/04/98 similarly, changed lower boundary for second forward
    // projection from ' proj_data_info_ptr->get_num_axial_poss()-segment_pos.get_average_ring_difference()'
    // to proj_data_ptr->get_max_axial_pos_num(seg_num)+1
  
    int orig_min_axial_pos_num = proj_data_ptr->get_min_axial_pos_num(seg_num);
    int orig_max_axial_pos_num = proj_data_ptr->get_max_axial_pos_num(seg_num);
            
    // TODO get boundaries from the symmetries 
    for (int view=0; view <= proj_data_ptr->get_num_views() /4; view++) { 
      full_log << endl <<"*************************************************************";
      full_log << endl <<"        Processing views No " << view
	       << " of segment No " << seg_num << endl;
	      
      full_log << endl <<"  - Getting related viewgrams"  << endl;
 
      RelatedViewgrams<float> viewgrams = 
	proj_data_ptr->get_related_viewgrams(ViewSegmentNumbers(view, seg_num),
					     forward_projector_ptr->get_symmetries_used()->clone());
        
      do_process_viewgrams(
			   viewgrams,
			   rmin, rmax, orig_min_axial_pos_num, orig_max_axial_pos_num,
			   image);
 
                 
    }            
    full_log << "    Min= " << image.find_min()
	     << " Max = " << image.find_max()
	     << " Sum = " << image.sum() << endl;

#ifndef PARALLEL
    if(save_intermediate_files){ 
      char *file = new char[output_filename_prefix.size() + 20];
      sprintf(file,"%s_afterseg%d",output_filename_prefix.c_str(),seg_num);
      do_save_img(file, image);        
      delete[] file;
    }
#endif 
  }

  do_byview_finalise(image);
        

}


// CL 010699 NEW function
void FBP3DRPReconstruction::do_best_fit(const Sinogram<float> &sino_measured,const Sinogram<float> &sino_calculated)
{
  float meas_calc = 0.F;
  float  meas_square = 0.F;
  float calc_square = 0.F;
  float sigma_square = 0.F;
  full_log << "  - Fitting estimated sinograms with the measured ones (Max in measured sino = " << sino_measured.find_max()
	   << " Max in fwd sino = " << sino_calculated.find_max() << ")" <<endl;
    
  for (int view=sino_measured.get_min_view_num();view <= sino_measured.get_max_view_num();view++){
    for (int bin=sino_measured.get_min_tangential_pos_num();bin <= sino_measured.get_max_tangential_pos_num();bin++){
      meas_calc += (sino_calculated[view][bin] * sino_measured[view][bin]);
      meas_square += (sino_measured[view][bin] * sino_measured[view][bin]);
      calc_square += (sino_calculated[view][bin] * sino_calculated[view][bin]);
      sigma_square +=((sino_measured[view][bin] - sino_calculated[view][bin])* (sino_measured[view][bin] - sino_calculated[view][bin]));
        
    }   
  }
  const float meas_sum = sino_measured.sum();
  const float calc_sum = sino_calculated.sum();
  const int  num_voxels = sino_measured.get_num_views() * sino_measured.get_num_tangential_poss();
  const float determinant = num_voxels*calc_square - (calc_sum*calc_sum);

  if (determinant == 0) {
    warning("\nwarning: unable to fit sinograms. resorting to no fitting.\n");
    return;
  }
    
  alpha_fit = (meas_calc * num_voxels  - meas_sum * calc_sum) / determinant;
  beta_fit = (calc_square * meas_sum - calc_sum * meas_calc ) / determinant;
    
  full_log << "  - Calculated fitted coefficients : alpha= " << alpha_fit << " beta= " << beta_fit
	   << " with quality factor= " << ((meas_square - alpha_fit * meas_calc - beta_fit * sino_measured.sum()) / meas_square )
	   << endl;          
}

          
void FBP3DRPReconstruction::do_grow3D_viewgram(RelatedViewgrams<float> & viewgrams,
                                                 int rmin, int rmax)
{        
  // we have to grow the viewgrams along axial direction in the (normal) 
  // case that rmin<get_min_axial_pos_num()
  const int rmin_grow = min(rmin, viewgrams.get_min_axial_pos_num());
  const int rmax_grow = max(rmax, viewgrams.get_max_axial_pos_num());
  const IndexRange2D 
    new_range(rmin_grow, 
	      rmax_grow, 
	      viewgrams.get_min_tangential_pos_num(),
	      viewgrams.get_max_tangential_pos_num());
  viewgrams.grow(new_range);     
}


void FBP3DRPReconstruction::do_forward_project_view(RelatedViewgrams<float> & viewgrams,
                                                      int rmin, int rmax,
                                                      int orig_min_axial_pos_num, int orig_max_axial_pos_num) const 
{ 

  // do not forward project if we don't need to...
  if (rmin <= orig_min_axial_pos_num-1)
    {
      full_log << "  - Forward projection of missing data first from ring No " 
	       << rmin
	       << " to "
	       << orig_min_axial_pos_num-1 << endl;

      forward_projector_ptr->forward_project(viewgrams, estimated_image(),
					     rmin ,orig_min_axial_pos_num-1);	    

    }

  if (orig_max_axial_pos_num+1 <= rmax)
    {
      full_log << "  - Forward projection from ring No "
	       << orig_max_axial_pos_num+1
	       << " to " << rmax << endl;
    
      forward_projector_ptr->forward_project(viewgrams, estimated_image(),
					     orig_max_axial_pos_num+1, rmax);
    
    }
#if 0
  if (fit_projections)
    {
      // fitting for viewgrams
      // Adjusting estimated sinograms by using the fitting coefficients :
      // sino = sino * alpha_fit + beta_fit;
      
      full_log << "  - Adjusting all sinograms with alpha = " << alpha_fit << " and beta = " << beta_fit << endl;
      // TODO This is wrong: it adjusts the measured projections as well !!!
      // It needs a loop over rrings from rmin to orig_min_axial_pos_num, etc.
      // Messy !
      error("This is not correctly implemented at the moment. disable fitting (recommended)\n");
      //viewgrams  *= alpha_fit ;
      //viewgrams += beta_fit;
    }
#endif

  if(disp>2) {
    display( viewgrams,viewgrams.find_max(),"Original+Forward projected");
  }
}
                    
	
void FBP3DRPReconstruction::do_colsher_filter_view( RelatedViewgrams<float> & viewgrams)
{ 
  static int prev_seg_num = viewgrams.get_proj_data_info_ptr()->get_min_segment_num()-1;  
  static ColsherFilter colsher_filter(0,0,0,0,0,0,0,0,0,0);
  const int seg_num = viewgrams.get_basic_segment_num();

  if (prev_seg_num != seg_num)
  {
    prev_seg_num = seg_num;
    full_log << "  - Constructing Colsher filter for this segment\n";
    int nrings = viewgrams.get_num_axial_poss(); 
    int nprojs = viewgrams.get_num_tangential_poss();
    
    int width = (int) pow(2, ((int) ceil(log((PadS + 1) * nprojs) / log(2))));
    int height = (int) pow(2, ((int) ceil(log((PadZ + 1) * nrings) / log(2))));	
    
    
    const float theta_max = atan(proj_data_info_cyl.get_tantheta(Bin(max_segment_num_to_process,0,0,0)));
    
    const float gamma = _PI/2 - atan(proj_data_info_cyl.get_tantheta(Bin(seg_num,0,0,0)));
    
    full_log << "Colsher filter theta_max = " << theta_max << " gamma = " << gamma
      << " d_a = " << proj_data_info_cyl.get_tangential_sampling()
      << " d_b = " << proj_data_info_cyl.get_axial_sampling(seg_num)*sin(gamma) << endl;
    
      
    colsher_filter = 
      ColsherFilter(height, width, gamma, theta_max, 
                    proj_data_info_cyl.get_tangential_sampling(), 
                    proj_data_info_cyl.get_axial_sampling(seg_num)*sin(gamma),// TODO replace with get_sampling_in_t()
                    alpha_colsher_axial, fc_colsher_axial,
                    alpha_colsher_planar, fc_colsher_planar);
  }

  full_log << "  - Apply Colsher filter to complete oblique sinograms" << endl;

  assert(viewgrams.get_num_viewgrams()%2 == 0);
    
  RelatedViewgrams<float>::iterator viewgram_iter = viewgrams.begin();

  for (; viewgram_iter != viewgrams.end(); viewgram_iter+=2) 
    Filter_proj_Colsher(*viewgram_iter, *(viewgram_iter+1),
                        colsher_filter,
                        PadS, PadZ); 


  /* If the segment is really an amalgam of different ring differences,
     we have to multiply it with the number of ring differences 
     in the segment.
     This is to assure that backprojecting each ring difference 
     on its own would give roughly the same result.

     TODO: should this be put in the backprojector itself ?
			 */  
      {
	const int num_ring_differences = 
	  proj_data_info_cyl.get_max_ring_difference(seg_num) - proj_data_info_cyl.get_min_ring_difference(seg_num) + 1;
	full_log << "  - Multiplying filtered projections by " << num_ring_differences << endl;
	if (num_ring_differences != 1){
          viewgrams *= num_ring_differences;
	}
      
      }
    if(disp>2) {
      display( viewgrams,viewgrams.find_max(), "Colsher filtered");
    }
}


void FBP3DRPReconstruction::do_3D_backprojection_view(const RelatedViewgrams<float> & viewgrams,
                                                        VoxelsOnCartesianGrid<float> &image,
                                                        int rmin, int rmax)
{ 
    full_log << "  - Backproject the filtered Colsher complete sinograms" << endl;
    // TODO drop rmin,rmax

    back_projector_ptr->back_project(image, viewgrams,rmin, rmax);
        
}


        
void FBP3DRPReconstruction::do_log_file(const VoxelsOnCartesianGrid<float> &image)
{
    char file[max_filename_length];
    sprintf(file,"%s.log",output_filename_prefix.c_str()); 
 
    full_log << endl << "- WRITE LOGFILE ("
         << file << ")" << endl;

    ofstream logfile(file);
 
    if (logfile.fail() || logfile.bad()) {
        warning("Error opening log file\n");
        return;
    }
    full_log << endl ;

    const time_t now  = time(NULL);

    logfile << "Date of the image reconstruction : " << asctime(localtime(&now))
             << parameter_info() ;
             

#ifndef PARALLEL
    logfile << "\n\n TIMING RESULTS :\n"    
            << "Total CPU time : " << get_CPU_timer_value() << '\n' 
            << "forward projection CPU time : " << forward_projector_ptr->get_CPU_timer_value() << '\n' 
            << "back projection CPU time : " << back_projector_ptr->get_CPU_timer_value() << '\n';
#endif    
}


void FBP3DRPReconstruction::do_process_viewgrams(RelatedViewgrams<float> & viewgrams, 
                                                   int rmin, int rmax,
                                                   int orig_min_axial_pos_num, int orig_max_axial_pos_num,
                                                   VoxelsOnCartesianGrid<float> &image)
{ //PROCESS BY VIEW              
        do_grow3D_viewgram(viewgrams, rmin, rmax);
        
	do_forward_project_view(viewgrams,
                                rmin, rmax, orig_min_axial_pos_num, orig_max_axial_pos_num); 
        
        do_colsher_filter_view(viewgrams);

	

	/* The backprojection here is really an approximation of a continuous integral
	over delta and phi, where
	-max_delta <= delta <= max_delta
	0 <= phi < pi
	We discretise the integral over delta by summing values 
	at discrete ring differences. However, we include the boundary points.
	The appropriate formula for the integral is then
	f(-max_delta)/2 + f(-max_delta+1) + ... f(max_delta-1) + f(max_delta/2)
	Note the factors 1/2 at the boundary.
	These are inserted below
	*/
	if (viewgrams.get_basic_segment_num() == max_segment_num_to_process)
	{
	  viewgrams /= 2;
	}
   
        do_3D_backprojection_view(viewgrams,
                                  image,
                                  rmin, rmax);
    
}


ReconstructionParameters& FBP3DRPReconstruction::params()
{
  return *this;
}

const ReconstructionParameters& FBP3DRPReconstruction::params() const
{
  return *this;
}

END_NAMESPACE_STIR
