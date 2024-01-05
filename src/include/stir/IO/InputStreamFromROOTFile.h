/*!
  \file
  \ingroup IO
  \brief Declaration of class stir::InputStreamFromROOTFile

  \author Nikos Efthimiou
  \author Harry Tsoumpas
  \author Kris Thielemans
  \author Robert Twyman
*/
/*
 *  Copyright (C) 2015, 2016 University of Leeds
    Copyright (C) 2016, 2021, 2020, 2021 UCL
    Copyright (C) 2018 University of Hull
    This file is part of STIR.

    SPDX-License-Identifier: Apache-2.0

    See STIR/LICENSE.txt for details
*/

#ifndef __stir_IO_InputStreamFromROOTFile_H__
#define __stir_IO_InputStreamFromROOTFile_H__

#include "stir/shared_ptr.h"
#include "stir/Succeeded.h"
#include "stir/listmode/CListRecordROOT.h"
#include "stir/RegisteredObject.h"
#include "stir/error.h"

// forward declaration of ROOT's TChain
class TChain;
class TBranch;

START_NAMESPACE_STIR

//! A helper class to read data from a ROOT file Generated by GATE simulation toolkit
/*! \ingroup IO
        \author Nikos Efthimiou

        \details This is an abstract base class for inputs from ROOT files.
        Primarily, information not related to the scanner's geometry is held here.
        * InputStreamFromROOTFileForCylindricalPET is for cylindrical PET scanners
        (<a href="http://wiki.opengatecollaboration.org/index.php/Users_Guide:Defining_a_system#CylindricalPET">here</a> ) and
        * InputStreamFromROOTFileForECAT is for ECAT PET scanners
          (<a href="http://wiki.opengatecollaboration.org/index.php/Users_Guide:Defining_a_system#Ecat">here</a> ).

       The follow bit of the header file refers to members stored here.
       For appropriate values please check your simulation macro file.
       For the singles_readout_depth from GATE's online documentation:
       (<a href="http://wiki.opengatecollaboration.org/index.php/Users_Guide_V7.2:Digitizer_and_readout_parameters">here</a> )
       > the readout depth depends upon how the electronic readout functions.

       \verbatim
        name of data file := ${INPUT_ROOT_FILE}
        name of input TChain := Coincidences
        Singles readout depth := 1
        exclude scattered events := ${EXCLUDE_SCATTERED}
        exclude random events := ${EXCLUDE_RANDOM}
        low energy window (keV) := 0
        upper energy window (keV):= 10000
       \endverbatim

        \warning The initial validation of the ROOT input was done with version 5.34.
*/

class InputStreamFromROOTFile : public RegisteredObject< InputStreamFromROOTFile >
{
public:
    typedef std::vector<long long int>::size_type SavedPosition;

    //! Default constructor
    InputStreamFromROOTFile();

#if 0 // disabled as not used
    //! constructor
    InputStreamFromROOTFile(std::string filename,
                            std::string chain_name,
                            bool exclude_scattered, bool exclude_randoms,
                            float low_energy_window, float up_energy_window,
                            int offset_dets);
#endif

    virtual ~InputStreamFromROOTFile() {}
    //!  \details Returns the next record in the ROOT file.
    //!  The code is adapted from Sadek A. Nehmeh and CR Schmidtlein,
    //! downloaded from <a href="http://www.opengatecollaboration.org/STIR">here</a>
    virtual
    Succeeded get_next_record(CListRecordROOT& record) = 0;
    //! Go to the first event.
    inline Succeeded reset();
    //! Must be called before calling for the first event.
    virtual Succeeded set_up(const std::string& header_path);
    //! Save current position in a vector
    inline
    SavedPosition save_get_position();
    //! Set current position
    inline
    Succeeded set_get_position(const SavedPosition&);
    //! Get the vector with the saved positions
    inline
    std::vector<unsigned long int> get_saved_get_positions() const;
    //! Set a vector with saved positions
    inline
    void set_saved_get_positions(const std::vector<unsigned long int>& );
    //! Returns the total number of events
    inline unsigned long int
    get_total_number_of_events() const;

    inline std::string get_ROOT_filename() const;

    //! Get the number of rings as calculated from the number of repeaters
    virtual int get_num_rings() const = 0;
    //! Get the number of dets per ring as calculated from the number of repeaters
    virtual int get_num_dets_per_ring() const = 0;
    //! Get the number of axial modules
    virtual int get_num_axial_blocks_per_bucket_v() const = 0;
    //! Get the number of transaxial modules
    virtual int get_num_transaxial_blocks_per_bucket_v() const = 0;
    //! Get the axial number of crystals per module
    inline int get_num_axial_crystals_per_block_v() const;
    //! Get the transaxial number of crystals per module
    inline int get_num_transaxial_crystals_per_block_v() const;
    //! Get the number of axial crystals per singles unit
    virtual int get_num_axial_crystals_per_singles_unit() const = 0;
    //! Get the number of transaxial crystals per singles unit
    virtual int get_num_trans_crystals_per_singles_unit() const = 0;
    //! \name number of "fake" crystals per block, inserted by the scanner
    /*! Some scanners (including many Siemens scanners) insert virtual crystals in the sinogram data.
      The other members of the class return the size of the "virtual" block. With these
      functions you can find its true size (or set it).
    */
    //@{!
    inline int get_num_virtual_axial_crystals_per_block() const;
    inline int get_num_virtual_transaxial_crystals_per_block() const;
    void set_num_virtual_axial_crystals_per_block(int);
    void set_num_virtual_transaxial_crystals_per_block(int);
    //@}

    //! Lower energy threshold
    inline float get_low_energy_thres() const;
    //! Upper energy threshold
    inline float get_up_energy_thres() const;

    //! Set singles_readout_depth
    inline void set_singles_readout_depth(int);

    inline void set_input_filename(const std::string&);

    inline void set_chain_name(const std::string&);

    inline void set_exclude_true_events(bool);

    inline void set_exclude_scattered_events(bool);

    inline void set_exclude_unscattered_events(bool);

    inline void set_exclude_random_events(bool);

#ifdef STIR_ROOT_ROTATION_AS_V4
    inline void set_detectors_offset(int);
#endif

    inline void set_low_energy_window(float);

    inline void set_upper_energy_window(float);
    //! Set the read_optional_root_fields flag
    inline void set_optional_ROOT_fields(bool);

    void set_crystal_repeater_x(int);
    void set_crystal_repeater_y(int);
    void set_crystal_repeater_z(int);

 protected:

    virtual void set_defaults();
    virtual void initialise_keymap();
    virtual bool post_processing();

    //! Input data file name
    std::string filename;
    //! The starting position.
    unsigned long int starting_stream_position;
    //! The total number of entries
    unsigned long int nentries;
    //! Current get position
    unsigned long int current_position;
    //! A vector with saved position indices.
    std::vector<unsigned long int> saved_get_positions;
    //! The name of the ROOT chain to be read
    std::string chain_name;
    //! This variable can be used to setBranchAddress to ROOT fields that currently
    //! are not used by STIR. Because they might be related to medical image reconstruction
    //! or because STIR does not support a relevant use. Of course, just activating this
    //! flag does not mean that something meaningfull will happen. Please edit get_next_record()
    //! function accordingly.
    bool read_optional_root_fields;

    //! \name repeaters
    //@{
    int crystal_repeater_x;
    int crystal_repeater_y;
    int crystal_repeater_z;
    //}

    //! \name ROOT Variables, i.e. to hold data from each entry.
    //@{
    TChain *stream_ptr;
    // note: should be ROOT's Int_t, Double_t and Float_t types, but those
    // are only defined when including ROOT .h files, which we want to avoid
    // here, as it creates a public dependency on the ROOT .h files
    // checking https://github.com/root-project/root/blob/8695045aeff4b2e606a5febdcd58a0a7e7f6c7af/core/base/inc/RtypesCore.h
    // we can use int32_t, float and double instead
    std::int32_t eventID1, eventID2, runID, sourceID1, sourceID2;
    double time1, time2;
    float energy1, energy2, rotation_angle, sinogramS, sinogramTheta, axialPos;
    int32_t comptonphantom1, comptonphantom2;
    float globalPosX1, globalPosX2, globalPosY1, globalPosY2, globalPosZ1, globalPosZ2;
    float sourcePosX1, sourcePosX2, sourcePosY1, sourcePosY2, sourcePosZ1, sourcePosZ2;
     //@}

    //! \name ROOT Branch address variables.
    //@{
    TBranch *br_time1 = nullptr;
    TBranch *br_time2 = nullptr;
    TBranch *br_eventID1 = nullptr;
    TBranch *br_eventID2 = nullptr;
    TBranch *br_energy1 = nullptr;
    TBranch *br_energy2 = nullptr;
    TBranch *br_comptonPhantom1 = nullptr;
    TBranch *br_comptonPhantom2 = nullptr;
    //Optional Branch variables. To be used if read_optional_root_fields is true.
    TBranch *br_axialPos = nullptr;
    TBranch *br_globalPosX1 = nullptr;
    TBranch *br_globalPosX2 = nullptr;
    TBranch *br_globalPosY1 = nullptr;
    TBranch *br_globalPosY2 = nullptr;
    TBranch *br_globalPosZ1 = nullptr;
    TBranch *br_globalPosZ2 = nullptr;
    TBranch *br_rotation_angle = nullptr;
    TBranch *br_runID = nullptr;
    TBranch *br_sinogramS = nullptr;
    TBranch *br_sinogramTheta = nullptr;
    TBranch *br_sourceID1 = nullptr;
    TBranch *br_sourceID2 = nullptr;
    TBranch *br_sourcePosX1 = nullptr;
    TBranch *br_sourcePosX2 = nullptr;
    TBranch *br_sourcePosY1 = nullptr;
    TBranch *br_sourcePosY2 = nullptr;
    TBranch *br_sourcePosZ1 = nullptr;
    TBranch *br_sourcePosZ2 = nullptr;
    //@}

    //! \name number of "fake" crystals per block, inserted by the scanner
    //@{!
    int num_virtual_axial_crystals_per_block;
    int num_virtual_transaxial_crystals_per_block;
    //@}
    //! Skip True events (eventID1 == eventID2). Default is false
    bool exclude_nonrandom;
    //! Skip scattered events (comptonphantom1 > 0 && comptonphantom2 > 0). Default is false
    bool exclude_scattered;
    //! Skip unscattered events (comptonphantom1 == 0 && comptonphantom2 == 0)). Default is false
    bool exclude_unscattered;
    //! Skip random events (eventID1 != eventID2). Default is false
    bool exclude_randoms;
    //! Check energy window information (low_energy_window < energy <  up_energy_window). Default is true
    bool check_energy_window_information;
    //! Lower energy threshold. Default is 1000 (keV)
    float low_energy_window;
    //! Upper energy threshold. Default is 0 (keV)
    float up_energy_window;
#ifdef STIR_ROOT_ROTATION_AS_V4
    //! This value will apply a rotation on the detectors' id in the same ring.
    int offset_dets;
#endif
    //!For the singles_readout_depth from GATE's online documentation:
    //! (<a href="http://wiki.opengatecollaboration.org/index.php/Users_Guide_V7.2:Digitizer_and_readout_parameters">here</a> )
    //! > the readout depth depends upon how the electronic readout functions.
    int singles_readout_depth;

    // This member will try to give to the continuous time register in GATE
    // data, a finite least significant bit.
    double least_significant_clock_bit;

    //! OpenGATE output ROOT energy information is given in MeV, these methods convert to keV
    float get_energy1_in_keV() const
    { return energy1 * 1e3; };
    float get_energy2_in_keV() const
    { return energy2 * 1e3; };

    //! Checks brentry satisfies the randoms, scatter and energy conditions.
    bool check_brentry_randoms_scatter_energy_conditions(long long int brentry);

    //! Checks the return of branch->GetEntry(brentry) and errors if return <= 0
    void GetEntryCheck(const int ret)
    { if (ret > 0) return; error(ret == 0 ? "Entry is null." : "ROOT I/O error."); };
};

END_NAMESPACE_STIR

#include "stir/IO/InputStreamFromROOTFile.inl"

#endif
