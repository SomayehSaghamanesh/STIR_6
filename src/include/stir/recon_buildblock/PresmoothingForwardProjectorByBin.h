/*!
  \file

  \brief Declaration of class stir::PresmoothingForwardProjectorByBin

  \author Kris Thielemans
  \author Richard Brown

*/
/*
    Copyright (C) 2002- 2007, Hammersmith Imanet
    Copyright (C) 2019, University College London

    This file is part of STIR.

    SPDX-License-Identifier: Apache-2.0


    See STIR/LICENSE.txt for details
*/
#ifndef __stir_recon_buildblock_PresmoothingForwardProjectorByBin__H__
#define __stir_recon_buildblock_PresmoothingForwardProjectorByBin__H__

#include "stir/RegisteredParsingObject.h"
#include "stir/recon_buildblock/ForwardProjectorByBin.h"
#include "stir/shared_ptr.h"

START_NAMESPACE_STIR

template <typename elemT>
class Viewgram;
template <typename DataT>
class DataProcessor;
/*!
  \brief A very preliminary class that first smooths the image, then forward projects.

  \warning. It assumes that the DataProcessor does not change
  the size of the image.
*/
class PresmoothingForwardProjectorByBin : public RegisteredParsingObject<PresmoothingForwardProjectorByBin, ForwardProjectorByBin>
{
#ifdef SWIG
  // work-around swig problem. It gets confused when using a private (or protected)
  // typedef in a definition of a public typedef/member
public:
#else
private:
#endif
  typedef ForwardProjectorByBin base_type;

public:
  //! Name which will be used when parsing a PresmoothingForwardProjectorByBin object
  static const char* const registered_name;

  //! Default constructor (calls set_defaults())
  PresmoothingForwardProjectorByBin();

  ~PresmoothingForwardProjectorByBin() override;

  //! Stores all necessary geometric info
  /*! Note that the density_info_ptr is not stored in this object. It's only used to get some info on sizes etc.
   */
  void set_up(const shared_ptr<const ProjDataInfo>& proj_data_info_ptr,
              const shared_ptr<const DiscretisedDensity<3, float>>& density_info_ptr // TODO should be Info only
              ) override;

  PresmoothingForwardProjectorByBin(const shared_ptr<ForwardProjectorByBin>& original_forward_projector_ptr,
                                    const shared_ptr<DataProcessor<DiscretisedDensity<3, float>>>&);

  // Informs on which symmetries the projector handles
  // It should get data related by at least those symmetries.
  // Otherwise, a run-time error will occur (unless the derived
  // class has other behaviour).
  const DataSymmetriesForViewSegmentNumbers* get_symmetries_used() const override;

private:
  shared_ptr<ForwardProjectorByBin> original_forward_projector_ptr;

#ifdef STIR_PROJECTORS_AS_V3
  void actual_forward_project(RelatedViewgrams<float>&,
                              const DiscretisedDensity<3, float>&,
                              const int min_axial_pos_num,
                              const int max_axial_pos_num,
                              const int min_tangential_pos_num,
                              const int max_tangential_pos_num);
#endif
  void actual_forward_project(RelatedViewgrams<float>&,
                              const int min_axial_pos_num,
                              const int max_axial_pos_num,
                              const int min_tangential_pos_num,
                              const int max_tangential_pos_num) override;

#if 0 // disabled as currently not used. needs to be written in the new style anyway
  void actual_forward_project(Bin&,
                              const DiscretisedDensity<3,float>&);
#endif

  void set_defaults() override;
  void initialise_keymap() override;
  bool post_processing() override;
};

END_NAMESPACE_STIR

#endif
