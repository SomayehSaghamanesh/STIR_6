//
// $Id$
//
/*!

  \file

  \brief 

  \author Sanida Mustafovic
  \author Kris Thielemans
  
  \date $Date$
  \version $Revision$
*/

#ifndef __Tomo_DAVImageFilter3D_H__
#define __Tomo_DAVImageFilter3D_H__


#include "tomo/ImageProcessor.h"
#include "tomo/local/DAVArrayFilter3D.h"

#include "tomo/RegisteredParsingObject.h"

START_NAMESPACE_TOMO

template <typename coordT> class CartesianCoordinate3D;

template <typename elemT>
class DAVImageFilter3D:
  public 
      RegisteredParsingObject<
	      DAVImageFilter3D<elemT>,
              ImageProcessor<3,elemT>
	       >

{
public:
  static const char * const registered_name; 

  DAVImageFilter3D();

  DAVImageFilter3D(const CartesianCoordinate3D<int>& mask_radius);  
 
private:
  DAVArrayFilter3D<elemT> dav_filter;
  int mask_radius_x;
  int mask_radius_y;
  int mask_radius_z;


  virtual void set_defaults();
  virtual void initialise_keymap();

  Succeeded virtual_build_filter (const DiscretisedDensity< 3,elemT>& density);
  void filter_it(DiscretisedDensity<3,elemT>& density, const DiscretisedDensity<3,elemT>& in_density) const; 
  void filter_it(DiscretisedDensity<3,elemT>& density) const; 
};


END_NAMESPACE_TOMO


#endif  // DAVImageFilter3D
