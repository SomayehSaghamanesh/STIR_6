!INTERFILE  :=
!imaging modality := PT
name of data file := template_for_ROOT_scanner.s
originating system := ROOT_defined_scanner
!version of keys := STIR3.0
!GENERAL DATA :=
patient orientation := head_in
patient rotation :=  supine
!GENERAL IMAGE DATA :=
!type of data := PET
imagedata byte order := LITTLEENDIAN
!PET STUDY (General) :=
!PET data type := Emission
applied corrections := {None}
!number format := float
!number of bytes per pixel := 4
number of dimensions := 5
matrix axis label [5] := timing positions
!matrix size [5] := 5
matrix axis label [4] := segment
!matrix size [4] := 7
matrix axis label [3] := view
!matrix size [3] := 252
matrix axis label [2] := axial coordinate
!matrix size [2] := { 1,2,3,4,3,2,1}
matrix axis label [1] := tangential coordinate
!matrix size [1] := 501
minimum ring difference per segment := { -3,-2,-1,0,1,2,3}
maximum ring difference per segment := { -3,-2,-1,0,1,2,3}
%TOF mashing factor:= 82
Scanner parameters:= 
Scanner type := ROOT_demo_scanner
Number of rings                          := 4
Number of detectors per ring             := 504
Inner ring diameter (cm)                 := 65.6
Average depth of interaction (cm)        := 0.7
Distance between rings (cm)              := 0.40625
Default bin size (cm)                    := 0.208626
View offset (degrees)                    := 0
Maximum number of non-arc-corrected bins := 501
Default number of arc-corrected bins     := 501
Energy resolution := 0
Reference energy (in keV) := 511
Number of TOF time bins :=411
Size of timing bin (ps) := 10
Timing resolution (ps) := 400
Number of blocks per bucket in transaxial direction         := 1
Number of blocks per bucket in axial direction              := 1
Number of crystals per block in axial direction             := 4
Number of crystals per block in transaxial direction        := 1
Number of detector layers                                   := 1
Number of crystals per singles unit in axial direction      := 4
Number of crystals per singles unit in transaxial direction := 1
end scanner parameters:=
effective central bin size (cm) := 0.208815
number of time frames := 1
!END OF INTERFILE :=
