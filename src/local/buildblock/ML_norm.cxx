//
// $Id$
//
/*!

  \file
  \ingroup buildblock

  \brief utilities for finding normalisation factors using an ML approach

  \author Kris Thielemans

  $Date$
  $Revision$
*/
/*
    Copyright (C) 2001- $Date$, IRSL
    See STIR/LICENSE.txt for details
*/

#include "local/stir/ML_norm.h"
#include "stir/display.h"
#include "stir/SegmentBySinogram.h"
#include "stir/stream.h"

START_NAMESPACE_STIR

DetPairData::DetPairData()
{}

DetPairData::DetPairData(const IndexRange<2>& range)
:base_type(range), num_detectors(range.get_length())
{
}

DetPairData& 
DetPairData::operator=(const DetPairData& other)
{
  base_type::operator=(other);
  num_detectors = other.num_detectors;
  return *this;
}

float & DetPairData::operator()(const int a, const int b)
{
  return (*this)[a][b<get_min_index(a) ? b+num_detectors: b];
}

float DetPairData::operator()(const int a, const int b) const
{
  return (*this)[a][b<get_min_index(a) ? b+num_detectors: b];
}

bool 
DetPairData::
is_in_data(const int a, const int b) const
{
  if (b>=get_min_index(a))
    return b<=get_max_index(a);
  else
    return b+num_detectors<=get_max_index(a);
}

void DetPairData::fill(const float d)
{
  base_type::fill(d);
}

void DetPairData::grow(const IndexRange<2>& range)
{
  base_type::grow(range);
  num_detectors=range.get_length();
}

int DetPairData::get_min_index() const
{
  return base_type::get_min_index();
}

int DetPairData::get_max_index() const
{
  return base_type::get_max_index();
}

int DetPairData::get_min_index(const int a) const
{
  return (*this)[a].get_min_index();
}

int DetPairData::get_max_index(const int a) const
{
  return (*this)[a].get_max_index();
}

float DetPairData::sum() const
{
  return base_type::sum();
}

float DetPairData::sum(const int a) const
{
  return (*this)[a].sum();
}

float DetPairData::find_max() const
{
  return base_type::find_max();
}

float DetPairData::find_min() const
{
  return base_type::find_min();
}

int DetPairData::get_num_detectors() const
{
  return num_detectors;
}

void display(const DetPairData& det_pair_data, const char * const title)
{
  const int num_detectors = det_pair_data.get_num_detectors();
  Array<2,float> full_data(IndexRange2D(num_detectors,num_detectors));
  for (int a = det_pair_data.get_min_index(); a <= det_pair_data.get_max_index(); ++a)
    for (int b = det_pair_data.get_min_index(a); b <= det_pair_data.get_max_index(a); ++b)      
       full_data[a%num_detectors][b%num_detectors] =
         det_pair_data(a,b);
  display(full_data,title);
}

void make_det_pair_data(DetPairData& det_pair_data,
			const ProjData& proj_data,
			const int segment_num,
			const int ax_pos_num)
{
  const ProjDataInfo* proj_data_info_ptr =
    proj_data.get_proj_data_info_ptr();
  const ProjDataInfoCylindricalNoArcCorr& proj_data_info =
    dynamic_cast<const ProjDataInfoCylindricalNoArcCorr&>(*proj_data_info_ptr);

  const int num_detectors = 
    proj_data_info.get_scanner_ptr()->get_num_detectors_per_ring();
  const int fan_size = 
    2*max(proj_data_info.get_max_tangential_pos_num(),
          -proj_data_info.get_min_tangential_pos_num()) + 1;
  // fan will range from -half_fan_size to +half_fan_size (i.e. an odd number of elements)
  const int half_fan_size = fan_size/2;

  IndexRange<2> fan_indices;
  fan_indices.grow(0,num_detectors-1);
  for (int a = 0; a < num_detectors; ++a)
  {
    fan_indices[a] = 
      IndexRange<1>(a+num_detectors/2-half_fan_size,
                    a+num_detectors/2+half_fan_size);
  }
  det_pair_data.grow(fan_indices);
  det_pair_data.fill(0);

  shared_ptr<Sinogram<float> > pos_sino_ptr =
    new Sinogram<float>(proj_data.get_sinogram(ax_pos_num,segment_num));
  shared_ptr<Sinogram<float> > neg_sino_ptr;
  if (segment_num == 0)
    neg_sino_ptr = pos_sino_ptr;
  else
    neg_sino_ptr =
      new Sinogram<float>(proj_data.get_sinogram(ax_pos_num,-segment_num));
  
    
  for (int view_num = 0; view_num < num_detectors/2; view_num++)
    for (int tang_pos_num = proj_data.get_min_tangential_pos_num();
	 tang_pos_num <= proj_data.get_max_tangential_pos_num();
	 ++tang_pos_num)
      {
	int det_num_a = 0;
	int det_num_b = 0;

	proj_data_info.get_det_num_pair_for_view_tangential_pos_num(det_num_a, det_num_b, view_num, tang_pos_num);

	det_pair_data(det_num_a,det_num_b) =
	  (*pos_sino_ptr)[view_num][tang_pos_num];
	det_pair_data(det_num_b,det_num_a) =
	  (*neg_sino_ptr)[view_num][tang_pos_num];
      }
}

void set_det_pair_data(ProjData& proj_data,
                       const DetPairData& det_pair_data,
			const int segment_num,
			const int ax_pos_num)
{
  const ProjDataInfo* proj_data_info_ptr =
    proj_data.get_proj_data_info_ptr();
  const ProjDataInfoCylindricalNoArcCorr& proj_data_info =
    dynamic_cast<const ProjDataInfoCylindricalNoArcCorr&>(*proj_data_info_ptr);

  const int num_detectors = det_pair_data.get_num_detectors();
  assert(proj_data_info.get_scanner_ptr()->get_num_detectors_per_ring() == num_detectors);

  shared_ptr<Sinogram<float> > pos_sino_ptr =
    new Sinogram<float>(proj_data.get_empty_sinogram(ax_pos_num,segment_num));
  shared_ptr<Sinogram<float> > neg_sino_ptr;
  if (segment_num != 0)
    neg_sino_ptr =
      new Sinogram<float>(proj_data.get_empty_sinogram(ax_pos_num,-segment_num));
  
    
  for (int view_num = 0; view_num < num_detectors/2; view_num++)
    for (int tang_pos_num = proj_data.get_min_tangential_pos_num();
	 tang_pos_num <= proj_data.get_max_tangential_pos_num();
	 ++tang_pos_num)
      {
	int det_num_a = 0;
	int det_num_b = 0;

	proj_data_info.get_det_num_pair_for_view_tangential_pos_num(det_num_a, det_num_b, view_num, tang_pos_num);

	(*pos_sino_ptr)[view_num][tang_pos_num] =
          det_pair_data(det_num_a,det_num_b);
        if (segment_num!=0)
	  (*neg_sino_ptr)[view_num][tang_pos_num] =
            det_pair_data(det_num_b,det_num_a);
      }
  proj_data.set_sinogram(*pos_sino_ptr);
  if (segment_num != 0)
    proj_data.set_sinogram(*neg_sino_ptr);
}


void apply_block_norm(DetPairData& det_pair_data, const BlockData& block_data, const bool apply)
{
  const int num_detectors = det_pair_data.get_num_detectors();
  const int num_blocks = block_data.get_length();
  const int num_crystals_per_block = num_detectors/num_blocks;
  assert(num_blocks * num_crystals_per_block == num_detectors);
  
  for (int a = det_pair_data.get_min_index(); a <= det_pair_data.get_max_index(); ++a)
    for (int b = det_pair_data.get_min_index(a); b <= det_pair_data.get_max_index(a); ++b)      
      {
	// note: add 2*num_detectors to newb to avoid using mod with negative numbers
	if (det_pair_data(a,b) == 0)
	  continue;
        if (apply)
          det_pair_data(a,b) *=
	    block_data[a/num_crystals_per_block][(b/num_crystals_per_block)%num_blocks];
        else
          det_pair_data(a,b) /=
	    block_data[a/num_crystals_per_block][(b/num_crystals_per_block)%num_blocks];
      }
}

void apply_geo_norm(DetPairData& det_pair_data, const GeoData& geo_data, const bool apply)
{
  const int num_detectors = det_pair_data.get_num_detectors();
  const int num_crystals_per_block = geo_data.get_length()*2;

  for (int a = det_pair_data.get_min_index(); a <= det_pair_data.get_max_index(); ++a)
    for (int b = det_pair_data.get_min_index(a); b <= det_pair_data.get_max_index(a); ++b)      
      {
	if (det_pair_data(a,b) == 0)
	  continue;
        int newa = a % num_crystals_per_block;
	int newb = b - (a - newa); 
	if (newa > num_crystals_per_block - 1 - newa)
	  { 
	    newa = num_crystals_per_block - 1 - newa; 
	    newb = - newb + num_crystals_per_block - 1;
	  }
	// note: add 2*num_detectors to newb to avoid using mod with negative numbers
        if (apply)
          det_pair_data(a,b) *=
	    geo_data[newa][(2*num_detectors + newb)%num_detectors];
        else
          det_pair_data(a,b) /=
	    geo_data[newa][(2*num_detectors + newb)%num_detectors];
      }
}

void apply_efficiencies(DetPairData& det_pair_data, const Array<1,float>& efficiencies, const bool apply)
{
  const int num_detectors = det_pair_data.get_num_detectors();
  for (int a = det_pair_data.get_min_index(); a <= det_pair_data.get_max_index(); ++a)
    for (int b = det_pair_data.get_min_index(a); b <= det_pair_data.get_max_index(a); ++b)      
      {
	if (det_pair_data(a,b) == 0)
	  continue;
        if (apply)
	  det_pair_data(a,b) *=
	    efficiencies[a]*efficiencies[b%num_detectors];
        else
          det_pair_data(a,b) /=
	    efficiencies[a]*efficiencies[b%num_detectors];
      }
}

float KL(const DetPairData& d1, const DetPairData& d2, const float threshold)
{
  float sum=0;
  for (int a = d1.get_min_index(); a <= d1.get_max_index(); ++a)
    for (int b = d1.get_min_index(a); b <= d1.get_max_index(a); ++b)      
      sum += KL(d1(a,b), d2(a,b), threshold);
  return sum;
}

//************ 3D


FanProjData::FanProjData()
{}

FanProjData::~FanProjData()
{}

#if 0
FanProjData::FanProjData(const IndexRange<4>& range)
:base_type(range), num_rings(range.get_length()), num_detectors_per_ring(range[range.get_min_index()].get_length())
{
}
#endif

FanProjData::
FanProjData(const int num_rings, const int num_detectors_per_ring, const int max_ring_diff, const int fan_size)
: num_rings(num_rings), num_detectors_per_ring(num_detectors_per_ring),
  max_ring_diff(max_ring_diff), half_fan_size(fan_size/2)
{
  assert(num_detectors_per_ring%2 == 0);
  assert(max_ring_diff<num_rings);
  assert(fan_size < num_detectors_per_ring);
  
  IndexRange<4> fan_indices;
  fan_indices.grow(0,num_rings-1);
  for (int ra = 0; ra < num_rings; ++ra)
  {
    const int min_rb = max(ra-max_ring_diff, 0);
    const int max_rb = min(ra+max_ring_diff, num_rings-1);
    fan_indices[ra].grow(0,num_detectors_per_ring-1);
    for (int a = 0; a < num_detectors_per_ring; ++a)
    {
      // store only 1 half of data as ra,a,rb,b = rb,b,ra,a
      fan_indices[ra][a].grow(max(ra,min_rb), max_rb);
      for (int rb = max(ra,min_rb); rb <= max_rb; ++rb)
      fan_indices[ra][a][rb] = 
        IndexRange<1>(a+num_detectors_per_ring/2-half_fan_size,
                      a+num_detectors_per_ring/2+half_fan_size);
    }
  }
  grow(fan_indices);
  fill(0);
}

FanProjData& 
FanProjData::operator=(const FanProjData& other)
{
  base_type::operator=(other);
  num_detectors_per_ring = other.num_detectors_per_ring;
  num_rings = other.num_rings;
  max_ring_diff = other.max_ring_diff;
  half_fan_size = other.half_fan_size;
  return *this;
}

float & FanProjData::operator()(const int ra, const int a, const int rb, const int b)
{
  assert(a>=0);
  assert(b>=0);
  return 
    ra<rb 
    ? (*this)[ra][a%num_detectors_per_ring][rb][b<get_min_b(a) ? b+num_detectors_per_ring: b]
    : (*this)[rb][b%num_detectors_per_ring][ra][a<get_min_b(b%num_detectors_per_ring) ? a+num_detectors_per_ring: a];
}

float FanProjData::operator()(const int ra, const int a, const int rb, const int b) const
{  
  assert(a>=0);
  assert(b>=0);
  return 
    ra<rb 
    ? (*this)[ra][a%num_detectors_per_ring][rb][b<get_min_b(a) ? b+num_detectors_per_ring: b]
    : (*this)[rb][b%num_detectors_per_ring][ra][a<get_min_b(b%num_detectors_per_ring) ? a+num_detectors_per_ring: a];
}

bool 
FanProjData::
is_in_data(const int ra, const int a, const int rb, const int b) const
{
  assert(a>=0);
  assert(b>=0);
  if (rb<(*this)[ra][a].get_min_index() || rb >(*this)[ra][a].get_max_index())
    return false;
  if (b>=get_min_b(a))
    return b<=get_max_b(a);
  else
    return b+num_detectors_per_ring<=get_max_b(a);
}

void FanProjData::fill(const float d)
{
  base_type::fill(d);
}

#if 0
void FanProjData::grow(const IndexRange<4>& range)
{
  base_type::grow(range);
  num_rings =range.get_length();
  num_detectors_per_ring = range[range.get_min_index()].get_length();
}
#endif

int FanProjData::get_min_ra() const
{
  return base_type::get_min_index();
}

int FanProjData::get_max_ra() const
{
  return base_type::get_max_index();
}


int FanProjData::get_min_a() const
{
  return (*this)[get_min_index()].get_min_index();
}

int FanProjData::get_max_a() const
{
  return (*this)[get_min_index()].get_max_index();
}


int FanProjData::get_min_rb(const int ra) const
{
  return max(ra-max_ring_diff, 0); 
  // next is no longer true because we store only half the data
  //return (*this)[ra][(*this)[ra].get_min_index()].get_min_index();
}

int FanProjData::get_max_rb(const int ra) const
{
  return (*this)[ra][(*this)[ra].get_min_index()].get_max_index();
}

int FanProjData::get_min_b(const int a) const
{
  return (*this)[get_min_index()][a][(*this)[get_min_index()][a].get_min_index()].get_min_index();
}

int FanProjData::get_max_b(const int a) const
{
  return (*this)[get_min_index()][a][(*this)[get_min_index()][a].get_min_index()].get_max_index();
}


float FanProjData::sum() const
{
  //return base_type::sum();
  float sum = 0;
  for (int ra=get_min_ra(); ra <= get_max_ra(); ++ra)
    for (int a = get_min_a(); a <= get_max_a(); ++a)      
      sum += this->sum(ra,a);
  return sum;
}

float FanProjData::sum(const int ra, const int a) const
{
  //return (*this)[ra][a].sum();
  float sum = 0;
  for (int rb=get_min_rb(ra); rb <= get_max_rb(ra); ++rb)
    for (int b = get_min_b(a); b <= get_max_b(a); ++b)      
      sum += (*this)(ra,a,rb,b%num_detectors_per_ring);
  return sum;
}

float FanProjData::find_max() const
{
  return base_type::find_max();
}

float FanProjData::find_min() const
{
  return base_type::find_min();
}

int FanProjData::get_num_detectors_per_ring() const
{
  return num_detectors_per_ring;
}


int FanProjData::get_num_rings() const
{
  return num_rings;
}

ostream& operator<<(ostream& s, const FanProjData& fan_data)
{
  return s << static_cast<FanProjData::base_type>(fan_data);
}

istream& operator>>(istream& s, FanProjData& fan_data)
{
  s >> static_cast<FanProjData::base_type&>(fan_data);
  if (!s)
    return s;
  fan_data.num_detectors_per_ring = fan_data.get_max_a() - fan_data.get_min_a() + 1;
  fan_data.num_rings = fan_data.get_max_ra() - fan_data.get_min_ra() + 1;
      
  //int max_delta = 0;
  //for (int ra = 0; ra < fan_data.num_rings; ++ra)
  //  max_delta = max(max_delta,fan_data[ra][0].get_length()-1);
  const int max_delta = fan_data[0][0].get_length()-1;
  const int half_fan_size = 
    fan_data[0][0][0].get_length()/2;
  fan_data.half_fan_size = half_fan_size;
  fan_data.max_ring_diff = max_delta;

  for (int ra = 0; ra < fan_data.num_rings; ++ra)
  {
    const int min_rb = max(ra-max_delta, 0);
    const int max_rb = min(ra+max_delta, fan_data.num_rings-1);
    for (int a = 0; a < fan_data.num_detectors_per_ring; ++a)
    {
      if (fan_data[ra][a].get_length() != max_rb - max(ra,min_rb) + 1)
      {
        warning("Reading FanProjData: inconsistent length %d for rb at ra=%d, a=%d, "
                "Expected length %d\n", 
                fan_data[ra][a].get_length(), ra, a, max_rb - max(ra,min_rb) + 1);
      }
      fan_data[ra][a].set_offset(max(ra,min_rb));
      for (int rb = fan_data[ra][a].get_min_index(); rb <= fan_data[ra][a].get_max_index(); ++rb)
      {
        if (fan_data[ra][a][rb].get_length() != 2*half_fan_size+1)
        {
          warning("Reading FanProjData: inconsistent length %d for b at ra=%d, a=%d, rb=%d\n"
                 "Expected length %d\n", 
                  fan_data[ra][a][rb].get_length(), ra, a, rb, 2*half_fan_size+1);
        }
        fan_data[ra][a][rb].set_offset(a+fan_data.num_detectors_per_ring/2-half_fan_size);
      }
    }
  }

  return s;
}

void display(const FanProjData& fan_data, const char * const title)
{
  const int num_rings = fan_data.get_num_rings();
  const int num_detectors_per_ring = fan_data.get_num_detectors_per_ring();
  Array<3,float> full_data(IndexRange3D(num_rings,num_detectors_per_ring,num_detectors_per_ring));
  for (int ra=fan_data.get_min_ra(); ra <= fan_data.get_max_ra(); ++ra)
  {
    full_data.fill(0);
    for (int a = 0; a<num_detectors_per_ring; ++a)
      for (int rb=fan_data.get_min_rb(ra); rb <= fan_data.get_max_rb(ra); ++rb)
        for (int b = fan_data.get_min_b(a); b <= fan_data.get_max_b(a); ++b)      
          full_data[rb][a%num_detectors_per_ring][b%num_detectors_per_ring] =
             fan_data(ra,a,rb,b);
      display(full_data, full_data.find_max(), title);
  }
}

void make_fan_data(FanProjData& fan_data,
	           const ProjData& proj_data)
{
  const ProjDataInfo* proj_data_info_ptr =
    proj_data.get_proj_data_info_ptr();
  const ProjDataInfoCylindricalNoArcCorr& proj_data_info =
    dynamic_cast<const ProjDataInfoCylindricalNoArcCorr&>(*proj_data_info_ptr);

  const int num_rings = 
    proj_data_info.get_scanner_ptr()->get_num_rings();
  const int num_detectors_per_ring = 
    proj_data_info.get_scanner_ptr()->get_num_detectors_per_ring();
  const int half_fan_size = 
    min(proj_data_info.get_max_tangential_pos_num(),
          -proj_data_info.get_min_tangential_pos_num());
  const int max_delta = proj_data_info_ptr->get_max_segment_num();

  fan_data = FanProjData(num_rings, num_detectors_per_ring, max_delta, 2*half_fan_size+1);

  shared_ptr<SegmentBySinogram<float> > segment_ptr;      
  Bin bin;

  for (bin.segment_num() = proj_data.get_min_segment_num(); bin.segment_num() <= proj_data.get_max_segment_num();  ++ bin.segment_num())
  {
    segment_ptr = new SegmentBySinogram<float>(proj_data.get_segment_by_sinogram(bin.segment_num()));
    
    for (bin.axial_pos_num() = proj_data.get_min_axial_pos_num(bin.segment_num());
	 bin.axial_pos_num() <= proj_data.get_max_axial_pos_num(bin.segment_num());
	 ++bin.axial_pos_num())
       for (bin.view_num() = 0; bin.view_num() < num_detectors_per_ring/2; bin.view_num()++)
          for (bin.tangential_pos_num() = -half_fan_size;
	       bin.tangential_pos_num() <= half_fan_size;
               ++bin.tangential_pos_num())
          {
            int ra = 0, a = 0;
            int rb = 0, b = 0;
            
            proj_data_info.get_det_pair_for_bin(a, ra, b, rb, bin);
            
            fan_data(ra, a, rb, b) =
	      fan_data(rb, b, ra, a) =
              (*segment_ptr)[bin.axial_pos_num()][bin.view_num()][bin.tangential_pos_num()];
          }
  }
}

void set_fan_data(ProjData& proj_data,
                       const FanProjData& fan_data)
{
  const ProjDataInfo* proj_data_info_ptr =
    proj_data.get_proj_data_info_ptr();
  const ProjDataInfoCylindricalNoArcCorr& proj_data_info =
    dynamic_cast<const ProjDataInfoCylindricalNoArcCorr&>(*proj_data_info_ptr);

  const int num_rings = fan_data.get_num_rings();
  assert(num_rings == proj_data_info.get_scanner_ptr()->get_num_rings());
  const int num_detectors_per_ring = fan_data.get_num_detectors_per_ring();
  assert(proj_data_info.get_scanner_ptr()->get_num_detectors_per_ring() == num_detectors_per_ring);
  const int half_fan_size = 
    min(proj_data_info.get_max_tangential_pos_num(),
          -proj_data_info.get_min_tangential_pos_num());

    
  Bin bin;
  shared_ptr<SegmentBySinogram<float> > segment_ptr;    
 
  for (bin.segment_num() = proj_data.get_min_segment_num(); bin.segment_num() <= proj_data.get_max_segment_num();  ++ bin.segment_num())
  {
    segment_ptr = new SegmentBySinogram<float>(proj_data.get_empty_segment_by_sinogram(bin.segment_num()));
    
    for (bin.axial_pos_num() = proj_data.get_min_axial_pos_num(bin.segment_num());
	 bin.axial_pos_num() <= proj_data.get_max_axial_pos_num(bin.segment_num());
	 ++bin.axial_pos_num())
       for (bin.view_num() = 0; bin.view_num() < num_detectors_per_ring/2; bin.view_num()++)
          for (bin.tangential_pos_num() = -half_fan_size;
	       bin.tangential_pos_num() <= half_fan_size;
               ++bin.tangential_pos_num())
          {
            int ra = 0, a = 0;
            int rb = 0, b = 0;
            
            proj_data_info.get_det_pair_for_bin(a, ra, b, rb, bin);
            
            (*segment_ptr)[bin.axial_pos_num()][bin.view_num()][bin.tangential_pos_num()] =
              fan_data(ra, a, rb, b);
          }
    proj_data.set_segment(*segment_ptr);
  }
}


void apply_block_norm(FanProjData& fan_data, const BlockData3D& block_data, const bool apply)
{
  const int num_axial_detectors = fan_data.get_num_rings();
  const int num_tangential_detectors = fan_data.get_num_detectors_per_ring();
  const int num_axial_blocks = block_data.get_num_rings();
  const int num_tangential_blocks = block_data.get_num_detectors_per_ring();
  const int num_axial_crystals_per_block = num_axial_detectors/num_axial_blocks;
  assert(num_axial_blocks * num_axial_crystals_per_block == num_axial_detectors);
  const int num_tangential_crystals_per_block = num_tangential_detectors/num_tangential_blocks;
  assert(num_tangential_blocks * num_tangential_crystals_per_block == num_tangential_detectors);
  
  for (int ra = fan_data.get_min_ra(); ra <= fan_data.get_max_ra(); ++ra)
    for (int a = fan_data.get_min_a(); a <= fan_data.get_max_a(); ++a)
      // loop rb from ra to avoid double counting
      for (int rb = max(ra,fan_data.get_min_rb(ra)); rb <= fan_data.get_max_rb(ra); ++rb)
        for (int b = fan_data.get_min_b(a); b <= fan_data.get_max_b(a); ++b)      
      {
	// note: add 2*num_detectors_per_ring to newb to avoid using mod with negative numbers
	if (fan_data(ra,a,rb,b) == 0)
	  continue;
        if (apply)
          fan_data(ra,a,rb,b) *=
	    block_data(ra/num_axial_crystals_per_block,a/num_tangential_crystals_per_block,
                       rb/num_axial_crystals_per_block,b/num_tangential_crystals_per_block);
        else
          fan_data(ra,a,rb,b) /=
	    block_data(ra/num_axial_crystals_per_block,a/num_tangential_crystals_per_block,
                       rb/num_axial_crystals_per_block,b/num_tangential_crystals_per_block);
      }
}

#if 0

void apply_geo_norm(FanProjData& fan_data, const GeoData& geo_data, const bool apply)
{
  const int num_detectors_per_ring = fan_data.get_num_detectors_per_ring();
  const int num_crystals_per_block = geo_data.get_length()*2;

  for (int a = fan_data.get_min_ra(); a <= fan_data.get_max_ra(); ++a)
    for (int b = fan_data.get_min_ra(a); b <= fan_data.get_max_ra(a); ++b)      
      {
	if (fan_data(ra,a,rb,b) == 0)
	  continue;
        int newa = a % num_crystals_per_block;
	int newb = b - (a - newa); 
	if (newa > num_crystals_per_block - 1 - newa)
	  { 
	    newa = num_crystals_per_block - 1 - newa; 
	    newb = - newb + num_crystals_per_block - 1;
	  }
	// note: add 2*num_detectors_per_ring to newb to avoid using mod with negative numbers
        if (apply)
          fan_data(ra,a,rb,b) *=
	    geo_data[newa][(2*num_detectors_per_ring + newb)%num_detectors_per_ring];
        else
          fan_data(ra,a,rb,b) /=
	    geo_data[newa][(2*num_detectors_per_ring + newb)%num_detectors_per_ring];
      }
}
#endif

void apply_efficiencies(FanProjData& fan_data, const DetectorEfficiencies& efficiencies, const bool apply)
{
  const int num_detectors_per_ring = fan_data.get_num_detectors_per_ring();
  for (int ra = fan_data.get_min_ra(); ra <= fan_data.get_max_ra(); ++ra)
    for (int a = fan_data.get_min_a(); a <= fan_data.get_max_a(); ++a)
      // loop rb from ra to avoid double counting
      for (int rb = max(ra,fan_data.get_min_rb(ra)); rb <= fan_data.get_max_rb(ra); ++rb)
        for (int b = fan_data.get_min_b(a); b <= fan_data.get_max_b(a); ++b)      
        {
	  if (fan_data(ra,a,rb,b) == 0)
	    continue;
          if (apply)
	    fan_data(ra,a,rb,b) *=
	      efficiencies[ra][a]*efficiencies[rb][b%num_detectors_per_ring];
          else
            fan_data(ra,a,rb,b) /=
	      efficiencies[ra][a]*efficiencies[rb][b%num_detectors_per_ring];
      }
}


float KL(const FanProjData& d1, const FanProjData& d2, const float threshold)
{
  float sum=0;
  for (int ra = d1.get_min_ra(); ra <= d1.get_max_ra(); ++ra)
    for (int a = d1.get_min_a(); a <= d1.get_max_a(); ++a)
      for (int rb = max(ra,d1.get_min_rb(ra)); rb <= d1.get_max_rb(ra); ++rb)
        for (int b = d1.get_min_b(a); b <= d1.get_max_b(a); ++b)      
          sum += KL(d1(ra,a,rb,b), d2(ra,a,rb,b), threshold);
  return sum;
}

END_NAMESPACE_STIR
