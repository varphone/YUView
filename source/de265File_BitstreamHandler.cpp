/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut f�r Nachrichtentechnik
*                       RWTH Aachen University, GERMANY
*
*   YUView is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   YUView is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with YUView.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "de265File_BitstreamHandler.h"
#include "common.h"
#include <assert.h>
#include <algorithm>
#include <QSize>
#include <QDebug>

int sub_byte_reader::readBits(int nrBits)
{
  int out = 0;

  while (nrBits > 0) {
    if (p_posInBuffer_bits == 8 && nrBits != 0) {
      // We read all bits we could from the current byte but we need more. Go to the next byte.
      if (!p_gotoNextByte())
        // We are at the end of the buffer but we need to read more. Error.
        return -1;
    }

    // How many bits can be gotton from the current byte?
    int curBitsLeft = 8 - p_posInBuffer_bits;

    int readBits;	// Nr of bits to read
    int offset;		// Offset for reading from the right
    if (nrBits >= curBitsLeft) {
      // Read "curBitsLeft" bits
      readBits = curBitsLeft;
      offset = 0;
    }
    else {
      // Read "nrBits" bits
      assert(nrBits < 8 && nrBits < curBitsLeft);
      readBits = nrBits;
      offset = curBitsLeft - nrBits;
    }

    // Shift output value so that the new bits fit
    out = out << readBits;

    char c = p_byteArray[p_posInBuffer_bytes];
    c = c >> offset;
    int mask = ((1<<readBits) - 1);

    // Write bits to output
    out += (c & mask);

    // Update counters
    nrBits -= readBits;
    p_posInBuffer_bits += readBits;
  }
  
  return out;
}

int sub_byte_reader::readUE_V()
{
  int readBit = readBits(1);
  if (readBit == 1)
    return 0;
  
  // Get the length of the golomb
  int golLength = 0;
  while (readBit == 0) {
    readBit = readBits(1);
    golLength++;
  }

  // Read "golLength" bits
  int val = readBits(golLength);
  // Add the exponentional part
  val += (1 << golLength)-1;
  
  return val;
}

bool sub_byte_reader::p_gotoNextByte()
{
  // Before we go to the neyt byte, check if the last (current) byte is a zero byte.
  if (p_byteArray[p_posInBuffer_bytes] == (char)0)
    p_numEmuPrevZeroBytes++;

  // Skip the remaining sub-byte-bits
  p_posInBuffer_bits = 0;
  // Advance pointer
  p_posInBuffer_bytes++;
  
  if (p_posInBuffer_bytes >= p_byteArray.size()) {
    // The next byte is outside of the current buffer. Error.
    return false;    
  }

  if (p_numEmuPrevZeroBytes == 2 && p_byteArray[p_posInBuffer_bytes] == (char)3) {
    // The current byte is an emulation prevention 3 byte. Skip it.
    p_posInBuffer_bytes++; // Skip byte

    if (p_posInBuffer_bytes >= p_byteArray.size()) {
      // The next byte is outside of the current buffer. Error
      return false;
    }

    // Reset counter
    p_numEmuPrevZeroBytes = 0;
  }
  else if (p_byteArray[p_posInBuffer_bytes] != (char)0)
    // No zero byte. No emulation prevention 3 byte
    p_numEmuPrevZeroBytes = 0;

  return true;
}

// Read "numBits" bits into the variable "into" and return false if -1 was returned by the reading function.
#define READBITS(into,numBits) {into = reader.readBits(numBits); if (into==-1) return false;}
// Read one bit and set into to true or false. Return false if -1 was returned by the reading function.
#define READFLAG(into) {int val = reader.readBits(1); if (val==-1) return false; into = (val != 0);}
// Read an UEV code. Return false if -1 was returned by the reading function.
#define READUEV(into) {into = reader.readUE_V(); if (into==-1) return false;}

// Read "numBits" bits and ignore them. Return false if -1 was returned by the reading function.
#define IGNOREBITS(numBits) {int val = reader.readBits(numBits); if (val==-1) return false;}
// Same as IGNOREBITS but return false if the read value is unequal to 0.
#define IGNOREBITS_Z(numBits) {int val = reader.readBits(numBits); if (val!=0) return false;}
// Read an UEV code and ignore the value. Return false if -1 was returned by the reading function.
#define IGNOREUEV() {int into = reader.readUE_V(); if (into==-1) return false;}

bool parameter_set_nal::parse_profile_tier_level(sub_byte_reader &reader, bool profilePresentFlag, int maxNumSubLayersMinus1)
{
  /// Profile tier level
  if (profilePresentFlag) {
    
    IGNOREBITS(2); // general_profile_space
    IGNOREBITS(1); // general_tier_flag
      
    int general_profile_idc;
    READBITS(general_profile_idc, 5);
      
    bool general_profile_compatibility_flag[32];
    for( int j = 0; j < 32; j++ ) {
      READFLAG(general_profile_compatibility_flag[j]);
    }
    
    IGNOREBITS(1); // general_progressive_source_flag
    IGNOREBITS(1); // general_interlaced_source_flag
      
    IGNOREBITS(1); // general_non_packed_constraint_flag
    IGNOREBITS(1); // general_frame_only_constraint_flag

    if( general_profile_idc == 4 || general_profile_compatibility_flag[4] || 
      general_profile_idc == 5 || general_profile_compatibility_flag[5] || 
      general_profile_idc == 6 || general_profile_compatibility_flag[6] || 
      general_profile_idc == 7 || general_profile_compatibility_flag[7] ) {

      IGNOREBITS(1); // general_max_12bit_constraint_flag
      IGNOREBITS(1); // general_max_10bit_constraint_flag
      IGNOREBITS(1); // general_max_8bit_constraint_flag
      IGNOREBITS(1); // general_max_422chroma_constraint_flag
      IGNOREBITS(1); // general_max_420chroma_constraint_flag
      IGNOREBITS(1); // general_max_monochrome_constraint_flag
      IGNOREBITS(1); // general_intra_constraint_flag
      IGNOREBITS(1); // general_one_picture_only_constraint_flag
      IGNOREBITS(1); // general_lower_bit_rate_constraint_flag

      IGNOREBITS_Z(34); // general_reserved_zero_34bits
    }
    else {
      IGNOREBITS_Z(43); // general_reserved_zero_43bits
    }

    if( ( general_profile_idc >= 1 && general_profile_idc <= 5 ) ||
        general_profile_compatibility_flag[1] || general_profile_compatibility_flag[2] || 
        general_profile_compatibility_flag[3] || general_profile_compatibility_flag[4] || 
        general_profile_compatibility_flag[5] ) {
      
      IGNOREBITS(1); // general_inbld_flag
    }
    else {
      IGNOREBITS_Z(1); // general_reserved_zero_bit
    }
  }

  IGNOREBITS(8); // general_level_idc
    
  bool sub_layer_profile_present_flag[8];
  bool sub_layer_level_present_flag[8];
  for( int i = 0; i < maxNumSubLayersMinus1; i++ ) {
    READFLAG(sub_layer_profile_present_flag[i]);
    READFLAG(sub_layer_level_present_flag[i]);
  }

  if( maxNumSubLayersMinus1 > 0 )
    for( int i = maxNumSubLayersMinus1; i < 8; i++ ) {
      IGNOREBITS_Z(2); // reserved_zero_2bits[i]
    }

  int sub_layer_profile_idc[8];
  bool sub_layer_profile_compatibility_flag[8][32];
  for( int i = 0; i < maxNumSubLayersMinus1; i++ ) {
    if( sub_layer_profile_present_flag[i] ) {
         
      IGNOREBITS(2); // sub_layer_profile_space[i]
      IGNOREBITS(1); // sub_layer_tier_flag[i]
        
      READBITS(sub_layer_profile_idc[i], 5);
        
      for( int j = 0; j < 32; j++ ) {
        READFLAG(sub_layer_profile_compatibility_flag[i][j]);
      }

      IGNOREBITS(1); // sub_layer_progressive_source_flag[i]
      IGNOREBITS(1); // sub_layer_interlaced_source_flag[i]
      IGNOREBITS(1); // sub_layer_non_packed_constraint_flag[i]
      IGNOREBITS(1); // sub_layer_frame_only_constraint_flag[i]
        
      if( sub_layer_profile_idc[i] == 4 || sub_layer_profile_compatibility_flag[i][4] || 
        sub_layer_profile_idc[i] == 5 || sub_layer_profile_compatibility_flag[i][5] || 
        sub_layer_profile_idc[i] == 6 || sub_layer_profile_compatibility_flag[i][6] || 
        sub_layer_profile_idc[i] == 7 || sub_layer_profile_compatibility_flag[i][7] ) {

        IGNOREBITS(1); // sub_layer_max_12bit_constraint_flag[i]
        IGNOREBITS(1); // sub_layer_max_10bit_constraint_flag[i]
        IGNOREBITS(1); // sub_layer_max_8bit_constraint_flag[i]
        IGNOREBITS(1); // sub_layer_max_422chroma_constraint_flag[i]
        IGNOREBITS(1); // sub_layer_max_420chroma_constraint_flag[i]
        IGNOREBITS(1); // sub_layer_max_monochrome_constraint_flag[i]
        IGNOREBITS(1); // sub_layer_intra_constraint_flag[i]
        IGNOREBITS(1); // sub_layer_one_picture_only_constraint_flag[i]
        IGNOREBITS(1); // sub_layer_lower_bit_rate_constraint_flag[i]

        IGNOREBITS_Z(34); // sub_layer_reserved_zero_34bits[i]
      }
      else {
        IGNOREBITS_Z(43); // sub_layer_reserved_zero_43bits[i]
      }

      if(( sub_layer_profile_idc[i] >= 1 && sub_layer_profile_idc[i] <= 5 ) || 
          sub_layer_profile_compatibility_flag[1] || sub_layer_profile_compatibility_flag[2] || 
          sub_layer_profile_compatibility_flag[3] || sub_layer_profile_compatibility_flag[4] || 
          sub_layer_profile_compatibility_flag[5] ) {
        IGNOREBITS(1); // sub_layer_inbld_flag[i]
      }
      else {
        IGNOREBITS_Z(1); // sub_layer_reserved_zero_bit[i]
      }
    }

    if( sub_layer_level_present_flag[i] ) {
      IGNOREBITS(8); // sub_layer_level_idc[i]
    }
  }
  return true;
} 

bool vps::parse_vps(QByteArray parameterSetData)
{
  parameter_set_data = parameterSetData;
  
  sub_byte_reader reader(parameter_set_data);
  
  READBITS(vps_video_parameter_set_id, 4);
  IGNOREBITS(1);  // vps_base_layer_internal_flag
  IGNOREBITS(1);  // vps_base_layer_available_flag
  READBITS(vps_max_layers_minus1, 6);

  int vps_max_sub_layers_minus1;
  READBITS(vps_max_sub_layers_minus1, 3);
  
  IGNOREBITS(1);  // vps_temporal_id_nesting_flag
  int val = 0;
  READBITS(val, 16);
  //IGNOREBITS(16); // vps_reserved_0xffff_16bits

  // profile_tier_level( 1, vps_max_sub_layers_minus1 )
  if (!parse_profile_tier_level(reader, true, vps_max_sub_layers_minus1))
    return false;
  
  bool vps_sub_layer_ordering_info_present_flag;
  READFLAG(vps_sub_layer_ordering_info_present_flag);

  for( int i = (vps_sub_layer_ordering_info_present_flag ? 0 : vps_max_sub_layers_minus1); i <= vps_max_sub_layers_minus1; i++) {
    IGNOREUEV(); //vps_max_dec_pic_buffering_minus1
    IGNOREUEV(); //vps_max_num_reorder_pics
    IGNOREUEV(); //vps_max_latency_increase_plus1
  }

  int vps_max_layer_id;
  READBITS(vps_max_layer_id, 6);
  int vps_num_layer_sets_minus1;
  READUEV(vps_num_layer_sets_minus1);

  for( int i=1; i <= vps_num_layer_sets_minus1; i++)
    for( int j=0; j <= vps_max_layer_id; j++)
      IGNOREBITS(1);  // layer_id_included_flag

  READFLAG(vps_timing_info_present_flag);
  if( vps_timing_info_present_flag ) {
    // The VPS can provide timing info (the time between two POCs. So the refresh rate)
    int vps_num_units_in_tick;
    READBITS(vps_num_units_in_tick,32);

    int vps_time_scale;
    READBITS(vps_time_scale,32);

    frameRate = (double)vps_time_scale / (double)vps_num_units_in_tick;

    bool vps_poc_proportional_to_timing_flag;
    READFLAG(vps_poc_proportional_to_timing_flag);

    int vps_num_ticks_poc_diff_one_minus1 = 0;
    if( vps_poc_proportional_to_timing_flag )
      READUEV(vps_num_ticks_poc_diff_one_minus1);

    // HRD parameters ...

  }
  
  // There is more to parse but we are not interested in this information (for now)
  return true;
}

bool sps::parse_sps(QByteArray parameterSetData)
{
  parameter_set_data = parameterSetData;
  
  sub_byte_reader reader(parameter_set_data);

  READBITS(sps_video_parameter_set_id,4);
  READBITS(sps_max_sub_layers_minus1, 3);
  IGNOREBITS(1); // sps_temporal_id_nesting_flag
  
  // profile_tier_level( 1, sps_max_sub_layers_minus1 )
  if (!parse_profile_tier_level(reader, true, sps_max_sub_layers_minus1))
    return false;
  
  /// Back to the seq_parameter_set_rbsp

  READUEV(sps_seq_parameter_set_id);
  READUEV(chroma_format_idc);

  if (chroma_format_idc == 3) {
    READBITS(separate_colour_plane_flag,1);
  }
  else
    separate_colour_plane_flag = false;

  READUEV(pic_width_in_luma_samples);
  READUEV(pic_height_in_luma_samples);
  READFLAG(conformance_window_flag);

  if (conformance_window_flag) {
    READUEV(conf_win_left_offset);
    READUEV(conf_win_right_offset);
    READUEV(conf_win_top_offset);
    READUEV(conf_win_bottom_offset);
  }
  else {
    conf_win_left_offset = 0;
    conf_win_right_offset = 0;
    conf_win_top_offset = 0;
    conf_win_bottom_offset = 0;
  }

  READUEV(bit_depth_luma_minus8);
  READUEV(bit_depth_chroma_minus8);
  READUEV(log2_max_pic_order_cnt_lsb_minus4);

  bool sps_sub_layer_ordering_info_present_flag;
  READFLAG(sps_sub_layer_ordering_info_present_flag);
  for( int i= (sps_sub_layer_ordering_info_present_flag ? 0 : sps_max_sub_layers_minus1); i <= sps_max_sub_layers_minus1; i++) {
    IGNOREUEV(); //sps_max_dec_pic_buffering_minus1[ i ]
    IGNOREUEV(); //sps_max_num_reorder_pics[ i ]
    IGNOREUEV(); //sps_max_latency_increase_plus1[ i ]
  }

  IGNOREUEV(); // log2_min_luma_coding_block_size_minus3
  IGNOREUEV(); // log2_diff_max_min_luma_coding_block_size
  IGNOREUEV(); // log2_min_luma_transform_block_size_minus2
  IGNOREUEV(); // log2_diff_max_min_luma_transform_block_size
  IGNOREUEV(); // max_transform_hierarchy_depth_inter
  IGNOREUEV(); // max_transform_hierarchy_depth_intra

  bool scaling_list_enabled_flag;
  READFLAG(scaling_list_enabled_flag);

  if( scaling_list_enabled_flag ) {
    bool sps_scaling_list_data_present_flag;
    READFLAG(sps_scaling_list_data_present_flag);
    if( sps_scaling_list_data_present_flag ) {
    
      // IGNORE: scaling_list_data( );
      {
        for( int sizeId=0; sizeId<4; sizeId++ )
          for( int matrixId=0; matrixId<6; matrixId += ( sizeId == 3 ) ? 3 : 1 ) {
            bool scaling_list_pred_mode_flag; 
            READFLAG(scaling_list_pred_mode_flag); // scaling_list_pred_mode_flag[ sizeId ][ matrixId ]
            if( !scaling_list_pred_mode_flag ) {
              IGNOREUEV(); // scaling_list_pred_matrix_id_delta[ sizeId ][ matrixId ]
            }
            else {
              if( sizeId > 1 )
                IGNOREUEV(); // scaling_list_dc_coef_minus8[ sizeId - 2 ][ matrixId ]
              int coefNum = coefNum = MIN(64, (1 << (4 + (sizeId << 1))));
              for (int i=0; i<coefNum; i++)
                IGNOREUEV(); // scaling_list_delta_coef
            }
          }
      }
    }
  }

  IGNOREBITS(1); // amp_enabled_flag
  IGNOREBITS(1); // sample_adaptive_offset_enabled_flag

  bool pcm_enabled_flag;
  READFLAG(pcm_enabled_flag);

  if( pcm_enabled_flag ) {
    IGNOREBITS(4); // pcm_sample_bit_depth_luma_minus1
    IGNOREBITS(4); // pcm_sample_bit_depth_chroma_minus1
    IGNOREUEV();   // log2_min_pcm_luma_coding_block_size_minus3
    IGNOREUEV();   // log2_diff_max_min_pcm_luma_coding_block_size
    IGNOREBITS(1); // pcm_loop_filter_disabled_flag
  }

  int num_short_term_ref_pic_sets;
  READUEV(num_short_term_ref_pic_sets);
  
  // Variables required for parsing the short term reference sets
  int NumNegativePics[65], NumPositivePics[65];
  int DeltaPocS0[65][16], DeltaPocS1[65][16];
  int NumDeltaPocs[16];
  bool use_delta_flag[16];

  for( int i=0; i<num_short_term_ref_pic_sets; i++) {
    // IGNORE: st_ref_pic_set( i )
    int stRpsIdx = i; // Shall be in the range of 0 .. 64 inclusive
    {
      bool inter_ref_pic_set_prediction_flag = false;
      if( stRpsIdx != 0 ) {
        READFLAG(inter_ref_pic_set_prediction_flag);
      }
      if( inter_ref_pic_set_prediction_flag ) {
        int delta_idx_minus1 = 0;
        if( stRpsIdx == num_short_term_ref_pic_sets ) {
          READUEV(delta_idx_minus1);
        }
        int delta_rps_sign;
        READBITS(delta_rps_sign, 1);
        int abs_delta_rps_minus1;
        READUEV(abs_delta_rps_minus1);

        int RefRpsIdx = stRpsIdx - (delta_idx_minus1 + 1);                     // Rec. ITU-T H.265 v3 (04/2015) (7-57)
        int deltaRps = (1 - 2 * delta_rps_sign) * (abs_delta_rps_minus1 + 1);  // Rec. ITU-T H.265 v3 (04/2015) (7-58)

        for( int j=0; j<=NumDeltaPocs[RefRpsIdx]; j++ ) {
          bool used_by_curr_pic_flag;
          READFLAG(used_by_curr_pic_flag);
          if( !used_by_curr_pic_flag ) {
            READFLAG(use_delta_flag[j]);
          }
          else
            use_delta_flag[j] = true; // Infer to 1
        }
        
        // Derive NumNegativePics Rec. ITU-T H.265 v3 (04/2015) (7-59)
        int i = 0;
        for( int j=NumPositivePics[RefRpsIdx] - 1; j >= 0; j-- ) { 
          int dPoc = DeltaPocS1[RefRpsIdx][j] + deltaRps;
          if( dPoc < 0 && use_delta_flag[NumNegativePics[RefRpsIdx] + j] ) { 
            DeltaPocS0[stRpsIdx][i] = dPoc;
            //UsedByCurrPicS0[stRpsIdx][i++] = used_by_curr_pic_flag[NumNegativePics[RefRpsIdx] + j]
            i++;
          }
        }
        if( deltaRps < 0 && use_delta_flag[NumDeltaPocs[RefRpsIdx]] ) { 
          DeltaPocS0[stRpsIdx][i] = deltaRps;
          //UsedByCurrPicS0[stRpsIdx][i++] = used_by_curr_pic_flag[NumDeltaPocs[RefRpsIdx]];
          i++;
        }
        for( int j=0; j<NumNegativePics[RefRpsIdx]; j++ ) { 
          int dPoc = DeltaPocS0[RefRpsIdx][j] + deltaRps;
          if( dPoc < 0 && use_delta_flag[j] ) { 
            DeltaPocS0[stRpsIdx][i] = dPoc;
            //UsedByCurrPicS0[stRpsIdx][i++] = used_by_curr_pic_flag[j];
            i++;
          } 
        } 
        NumNegativePics[stRpsIdx] = i;

        // Derive NumPositivePics Rec. ITU-T H.265 v3 (04/2015) (7-60)
        i = 0;
        for( int j=NumNegativePics[RefRpsIdx] - 1; j>=0; j-- ) { 
          int dPoc = DeltaPocS0[RefRpsIdx][j] + deltaRps;
          if( dPoc > 0 && use_delta_flag[j] ) { 
            DeltaPocS1[stRpsIdx][i] = dPoc;
            // UsedByCurrPicS1[stRpsIdx][i++] = used_by_curr_pic_flag[j]
            i++;
          }
        }
        if( deltaRps > 0 && use_delta_flag[NumDeltaPocs[RefRpsIdx]] ) {
          DeltaPocS1[stRpsIdx][i] = deltaRps;
          // UsedByCurrPicS1[stRpsIdx][i++] = used_by_curr_pic_flag[NumDeltaPocs[RefRpsIdx]]
          i++;
        }
        for( int j=0; j<NumPositivePics[RefRpsIdx]; j++) { 
          int dPoc = DeltaPocS1[RefRpsIdx][j] + deltaRps;
          if( dPoc > 0 && use_delta_flag[NumNegativePics[RefRpsIdx] + j] ) { 
            DeltaPocS1[stRpsIdx][i] = dPoc;
            // UsedByCurrPicS1[stRpsIdx][i++] = used_by_curr_pic_flag[NumNegativePics[RefRpsIdx] + j] 
            i++;
          }
        }
        NumPositivePics[stRpsIdx] = i;
      }
      else {
        int num_negative_pics;
        READUEV(num_negative_pics);
        int num_positive_pics;
        READUEV(num_positive_pics);
        for( int i=0; i<num_negative_pics; i++ ) {
          int delta_poc_s0_minus1; // delta_poc_s0_minus1[ i ]
          READUEV(delta_poc_s0_minus1);
          int used_by_curr_pic_s0_flag;
          READBITS(used_by_curr_pic_s0_flag, 1);
          
          if (i==0)
            DeltaPocS0[stRpsIdx][i] = -(delta_poc_s0_minus1 + 1); // (7-65)
          else
            DeltaPocS0[stRpsIdx][i] = DeltaPocS0[stRpsIdx][i-1] - (delta_poc_s0_minus1 + 1); // (7-67)
        }
        for( int i=0; i<num_positive_pics; i++ ) {
          int delta_poc_s1_minus1; // delta_poc_s1_minus1[ i ]
          READUEV(delta_poc_s1_minus1);
          int used_by_curr_pic_s1_flag;
          READBITS(used_by_curr_pic_s1_flag, 1);

          if (i==0)
            DeltaPocS1[stRpsIdx][i] = delta_poc_s1_minus1 + 1; // (7-66)
          else
            DeltaPocS1[stRpsIdx][i] = DeltaPocS1[stRpsIdx][i-1] + (delta_poc_s1_minus1 + 1 ); // (7-68)
        }

        NumNegativePics[ stRpsIdx ] = num_negative_pics;
        NumPositivePics[ stRpsIdx ] = num_positive_pics; 
      }
      NumDeltaPocs[stRpsIdx] = NumNegativePics[stRpsIdx] + NumPositivePics[stRpsIdx]; // (7-69)
    }
  }

  bool long_term_ref_pics_present_flag;
  READFLAG(long_term_ref_pics_present_flag);
  if( long_term_ref_pics_present_flag ) {
    int num_long_term_ref_pics_sps;
    READUEV(num_long_term_ref_pics_sps);
    for( int i=0; i<num_long_term_ref_pics_sps; i++ ) {
      int nrBits = log2_max_pic_order_cnt_lsb_minus4 + 4;
      IGNOREBITS(nrBits); // lt_ref_pic_poc_lsb_sps[ i ]
      IGNOREBITS(1); // used_by_curr_pic_lt_sps_flag[ i ]
    }
  }

  IGNOREBITS(1); // sps_temporal_mvp_enabled_flag
  IGNOREBITS(1); // strong_intra_smoothing_enabled_flag

  bool vui_parameters_present_flag;
  READFLAG(vui_parameters_present_flag);
  if( vui_parameters_present_flag ) {
    //vui_parameters( )

    bool aspect_ratio_info_present_flag;
    READFLAG(aspect_ratio_info_present_flag);
    if( aspect_ratio_info_present_flag ) {
      int aspect_ratio_idc;
      READBITS(aspect_ratio_idc, 8);
      if( aspect_ratio_idc == 255 ) { // EXTENDED_SAR=255
        IGNOREBITS(16); // sar_width
        IGNOREBITS(16); // sar_height
      }
    }

    bool overscan_info_present_flag;
    READFLAG(overscan_info_present_flag);
    if( overscan_info_present_flag )
      IGNOREBITS(1); // overscan_appropriate_flag

    bool video_signal_type_present_flag;
    READFLAG(video_signal_type_present_flag);
    if( video_signal_type_present_flag ) {
      IGNOREBITS(3); // video_format
      IGNOREBITS(1); // video_full_range_flag

      bool colour_description_present_flag;
      READFLAG(colour_description_present_flag);
      if( colour_description_present_flag ) {
        IGNOREBITS(8); // colour_primaries
        IGNOREBITS(8); // transfer_characteristics
        IGNOREBITS(8); // matrix_coeffs
      }
    }

    bool chroma_loc_info_present_flag;
    READFLAG(chroma_loc_info_present_flag);
    if( chroma_loc_info_present_flag ) {
      IGNOREUEV(); // chroma_sample_loc_type_top_field
      IGNOREUEV(); // chroma_sample_loc_type_bottom_field
    }

    IGNOREBITS(1); // neutral_chroma_indication_flag
    IGNOREBITS(1); // field_seq_flag
    IGNOREBITS(1); // frame_field_info_present_flag
    bool default_display_window_flag;
    READFLAG(default_display_window_flag);
    if( default_display_window_flag ) {
      IGNOREUEV(); // def_disp_win_left_offset
      IGNOREUEV(); // def_disp_win_right_offset
      IGNOREUEV(); // def_disp_win_top_offset
      IGNOREUEV(); // def_disp_win_bottom_offset
    }
    
    READFLAG(vui_timing_info_present_flag);
    if( vui_timing_info_present_flag ) {
      // The VUI has timing information for us
      int vui_num_units_in_tick;
      READBITS(vui_num_units_in_tick, 32);
      int vui_time_scale;
      READBITS(vui_time_scale, 32);

      frameRate = (double)vui_time_scale / (double)vui_num_units_in_tick;

      bool vui_poc_proportional_to_timing_flag;
      READFLAG(vui_poc_proportional_to_timing_flag);
      if( vui_poc_proportional_to_timing_flag )
        IGNOREUEV(); // vui_num_ticks_poc_diff_one_minus1

      // ...
    }
  }

  // There is more to parse but we are not interested in this information (for now)
  return true;
}

bool pps::parse_pps(QByteArray parameterSetData)
{
  parameter_set_data = parameterSetData;
  
  sub_byte_reader reader(parameter_set_data);

  READUEV(pps_pic_parameter_set_id);
  READUEV(pps_seq_parameter_set_id);

  IGNOREBITS(1); // dependent_slice_segments_enabled_flag
  READFLAG(output_flag_present_flag);
  READBITS(num_extra_slice_header_bits, 3);

  // There is more to parse but we are not interested in this information (for now)
  return true;
}

// Only true for the first slice instance
bool slice::bFirstAUInDecodingOrder = true;
int slice::prevTid0Pic_slice_pic_order_cnt_lsb = 0;
int slice::prevTid0Pic_PicOrderCntMsb = 0;

// T-REC-H.265-201410 - 7.3.6.1 slice_segment_header()
bool slice::parse_slice(QByteArray sliceHeaderData,
                        QMap<int, sps*> p_active_SPS_list,
                        QMap<int, pps*> p_active_PPS_list )
{
  int val;

  sub_byte_reader reader(sliceHeaderData);

  READFLAG(first_slice_segment_in_pic_flag);
  
  if (!first_slice_segment_in_pic_flag) {
    // We are only interested in the first slices of each picture
    return true;
  }
  
  if (nal_type == BLA_W_LP     || nal_type == BLA_W_RADL ||
    nal_type == BLA_N_LP       || nal_type == IDR_W_RADL ||
    nal_type == IDR_N_LP       || nal_type == CRA_NUT    ||
    nal_type == RSV_IRAP_VCL22 || nal_type == RSV_IRAP_VCL23 ) {
        
    IGNOREBITS(1); // no_output_of_prior_pics_flag
  }

  READUEV(slice_pic_parameter_set_id);
  // The value of slice_pic_parameter_set_id shall be in the range of 0 to 63, inclusive. (Max 11 bits read)
  if (slice_pic_parameter_set_id >= 63) return false;

  // For the first slice dependent_slice_segment_flag is 0 so we can continue here.

  // Get the active PPS with the read ID
  if (!p_active_PPS_list.contains(slice_pic_parameter_set_id))
    // Parameter set not found. Something went wrong.
    return false;
  pps *act_pps = p_active_PPS_list[slice_pic_parameter_set_id];

  if (act_pps == NULL) return false;
        
  for (int i=0; i < act_pps->num_extra_slice_header_bits; i++) {
    IGNOREBITS(1); // slice_reserved_flag
  }

  READUEV(slice_type); // Max 3 bits read
  
  if (act_pps->output_flag_present_flag) {
    READFLAG(pic_output_flag);
  }

  // Get the active sps with the read ID
  if (!p_active_SPS_list.contains(act_pps->pps_seq_parameter_set_id))
    // Parameter set not found. Something went wrong.
    return false;
  sps *act_sps = p_active_SPS_list[act_pps->pps_seq_parameter_set_id];

  if (act_sps->separate_colour_plane_flag) {
    READFLAG(colour_plane_id);
  }

  if( nal_type != IDR_W_RADL && nal_type != IDR_N_LP ) {
    READBITS(slice_pic_order_cnt_lsb, act_sps->log2_max_pic_order_cnt_lsb_minus4 + 4); // Max 16 bits read

    // ... short_term_ref_pic_set_sps_flag and so on
  }
  else {
    slice_pic_order_cnt_lsb = 0;
  }

  // Thats all we wanted from the slice header

  // Calculate the picture order count
  int MaxPicOrderCntLsb = 1 << (act_sps->log2_max_pic_order_cnt_lsb_minus4 + 4);

  // The variable NoRaslOutputFlag is specified as follows:
  bool NoRaslOutputFlag = false;
  if (nal_type == IDR_W_RADL || nal_type == BLA_W_LP)
    NoRaslOutputFlag = true;
  else if (bFirstAUInDecodingOrder) {
    NoRaslOutputFlag = true;
    bFirstAUInDecodingOrder = false;
  }

  // T-REC-H.265-201410 - 8.3.1 Decoding process for picture order count
  
  int prevPicOrderCntLsb = 0;
  int prevPicOrderCntMsb = 0;
  // When the current picture is not an IRAP picture with NoRaslOutputFlag equal to 1, ...
  if (!(isIRAP() && NoRaslOutputFlag)) {
    // the variables prevPicOrderCntLsb and prevPicOrderCntMsb are derived as follows:
     
    prevPicOrderCntLsb = prevTid0Pic_slice_pic_order_cnt_lsb;
    prevPicOrderCntMsb = prevTid0Pic_PicOrderCntMsb;
  }

  // The variable PicOrderCntMsb of the current picture is derived as follows:
  if (isIRAP() && NoRaslOutputFlag) {
    // If the current picture is an IRAP picture with NoRaslOutputFlag equal to 1, PicOrderCntMsb is set equal to 0.
    PicOrderCntMsb = 0;
  }
  else {
    // Otherwise, PicOrderCntMsb is derived as follows: (8-1)
    if((slice_pic_order_cnt_lsb < prevPicOrderCntLsb) && ((prevPicOrderCntLsb - slice_pic_order_cnt_lsb) >= (MaxPicOrderCntLsb / 2)))
      PicOrderCntMsb = prevPicOrderCntMsb + MaxPicOrderCntLsb;
    else if ((slice_pic_order_cnt_lsb > prevPicOrderCntLsb ) && ((slice_pic_order_cnt_lsb - prevPicOrderCntLsb) > (MaxPicOrderCntLsb / 2))) 
      PicOrderCntMsb = prevPicOrderCntMsb - MaxPicOrderCntLsb;
    else 
      PicOrderCntMsb = prevPicOrderCntMsb;
  }

  // PicOrderCntVal is derived as follows: (8-2)
  PicOrderCntVal = PicOrderCntMsb + slice_pic_order_cnt_lsb;

  if (nuh_temporal_id_plus1 - 1 == 0 && !isRASL() && !isRADL()) {
    // Let prevTid0Pic be the previous picture in decoding order that has TemporalId 
    // equal to 0 and that is not a RASL picture, a RADL picture or an SLNR picture.

    // Set these for the next slice
    prevTid0Pic_slice_pic_order_cnt_lsb = slice_pic_order_cnt_lsb;
    prevTid0Pic_PicOrderCntMsb = PicOrderCntMsb;
  }

  return true;
}

de265File_FileHandler::de265File_FileHandler()
{
  p_FileBuffer.resize(BUFFER_SIZE);
  p_posInBuffer = 0;
  p_bufferStartPosInFile = 0;
  p_numZeroBytes = 0;

  // Set the start code to look for (0x00 0x00 0x01)
  p_startCode.append((char)0);
  p_startCode.append((char)0);
  p_startCode.append((char)1);
}

// The file handler, ... well ... it handeles the Annex B formatted file.
bool de265File_FileHandler::loadFile(QString fileName)
{
  // Open the input file
  p_srcFile = new QFile(fileName);
  p_srcFile->open(QIODevice::ReadOnly);

  // Fill the buffer
  p_FileBufferSize = p_srcFile->read(p_FileBuffer.data(), BUFFER_SIZE);
  if (p_FileBufferSize == 0) {
    // The file is empty of there was an error reading from the file.
    return false;
  }
    
  // Get the positions where we can start decoding
  return scanFileForNalUnits();
}

bool de265File_FileHandler::updateBuffer()
{
  // Save the position of the first byte in this new buffer
  p_bufferStartPosInFile += p_FileBufferSize;

  p_FileBufferSize = p_srcFile->read(p_FileBuffer.data(), BUFFER_SIZE);
  p_posInBuffer = 0;

  return (p_FileBufferSize > 0);
}

bool de265File_FileHandler::seekToNextNALUnit()
{
  // Are we currently at the one byte of a start code?
  if (curPosAtStartCode())
    return gotoNextByte();

  p_numZeroBytes = 0;
  
  // Check if there is another start code in the buffer
  int idx = p_FileBuffer.indexOf(p_startCode, p_posInBuffer);
  while (idx < 0) {
    // Start code not found in this buffer. Load next chuck of data from file.

    // Before we load more data, check with how many zeroes the current buffer ends.
    // This could be the beginning of a start code.
    int nrZeros = 0;
    for (int i = 1; i <= 3; i++) {
      if (p_FileBuffer.at(p_FileBufferSize-i) == 0)
        nrZeros++;
    }
    
    // Load the next buffer
    if (!updateBuffer()) {
      // Out of file
      return false;
    }

    if (nrZeros > 0) {
      // The last buffer ended with zeroes. 
      // Now check if the beginning of this buffer is the remaining part of a start code
      if ((nrZeros == 2 || nrZeros == 3) && p_FileBuffer.at(0) == 1) {
        // Start code found
        p_posInBuffer = 1;
        return true;
      }

      if ((nrZeros == 1) && p_FileBuffer.at(0) == 0 && p_FileBuffer.at(1) == 1) {
        // Start code found
        p_posInBuffer = 2;
        return true;
      }
    }

    // New buffer loaded but no start code found yet. Search for it again.
    idx = p_FileBuffer.indexOf(p_startCode, p_posInBuffer);
  }

  assert(idx >= 0);
  if (idx + 3 >= p_FileBufferSize) {
    // The start code is exactly at the end of the current buffer. 
    if (!updateBuffer()) {
      // Out of file
      return false;
    }
    return true;
  }

  // Update buffer position
  p_posInBuffer = idx + 3;
  return true;
}

bool de265File_FileHandler::gotoNextByte()
{
  // First check if the current byte is a zero byte
  if (getCurByte() == (char)0)
    p_numZeroBytes++;
  else
    p_numZeroBytes = 0;

  p_posInBuffer++;

  if (p_posInBuffer >= p_FileBufferSize) {
    // The next byte is in the next buffer
    if (!updateBuffer()) {
      // Out of file
      return false;
    }
    p_posInBuffer = 0;
  }

  return true;
}

bool de265File_FileHandler::scanFileForNalUnits()
{
  // These maps hold the last active VPS, SPS and PPS. This is required for parsing
  // the parameter sets.
  QMap<int, sps*> active_SPS_list;
  QMap<int, pps*> active_PPS_list;

  while (seekToNextNALUnit()) {
    // Seek successfull. The file is now pointing to the first byte after the start code.

    // Save the position of the first byte of the start code
    quint64 curFilePos = tell() - 3;

    // Read two bytes (the nal header)
    QByteArray nalHeaderBytes;
    nalHeaderBytes.append(getCurByte());
    gotoNextByte();
    nalHeaderBytes.append(getCurByte());
    gotoNextByte();

    // Create a sub byte parser to access the bits
    sub_byte_reader reader(nalHeaderBytes);

    // Read forbidden_zeor_bit
    int forbidden_zero_bit = reader.readBits(1);
    if (forbidden_zero_bit != 0) return false;

    // Read nal unit type
    int nal_unit_type_id = reader.readBits(6);
    if (nal_unit_type_id == -1) return false;
    nal_unit_type nal_type = (nal_unit_type_id > UNSPECIFIED) ? UNSPECIFIED : (nal_unit_type)nal_unit_type_id;

    // Read layer id
    int layer_id = reader.readBits(6);
    if (layer_id == -1) return false;
    
    int temporal_id_plus1 = reader.readBits(3);
    if (temporal_id_plus1 == -1) return false;

    if (nal_type == VPS_NUT) {
      // A video parameter set
      vps *new_vps = new vps(curFilePos, nal_type, layer_id, temporal_id_plus1);
      if (!new_vps->parse_vps( getRemainingNALBytes() )) return false;

      // Put parameter sets into the NAL unit list
      p_nalUnitList.append(new_vps);
    }
    else if (nal_type == SPS_NUT) {
      // A sequence parameter set
      sps *new_sps = new sps(curFilePos, nal_type, layer_id, temporal_id_plus1);
      if (!new_sps->parse_sps( getRemainingNALBytes() )) return false;
      
      // Add sps (replace old one if existed)
      active_SPS_list.insert(new_sps->sps_seq_parameter_set_id, new_sps);

      // Also add sps to list of all nals
      p_nalUnitList.append(new_sps);
    }
    else if (nal_type == PPS_NUT) {
      // A picture parameter set
      pps *new_pps = new pps(curFilePos, nal_type, layer_id, temporal_id_plus1);
      if (!new_pps->parse_pps( getRemainingNALBytes() )) return false;
      
      // Add pps (replace old one if existed)
      active_PPS_list.insert(new_pps->pps_pic_parameter_set_id, new_pps);

      // Also add pps to list of all nals
      p_nalUnitList.append(new_pps);
    }
    else if (nal_type == IDR_W_RADL || nal_type == IDR_N_LP   || nal_type == CRA_NUT ||
             nal_type == BLA_W_LP   || nal_type == BLA_W_RADL || nal_type == BLA_N_LP ) {
      // Create a new slice unit
      slice *newSlice = new slice(curFilePos, nal_type, layer_id, temporal_id_plus1);
      if (!newSlice->parse_slice(getRemainingNALBytes(8), active_SPS_list, active_PPS_list)) return false;

      if (newSlice->first_slice_segment_in_pic_flag) {
        // This is the first slice of a random access pont. Add it to the list.
        p_nalUnitList.append(newSlice);

        // Get the poc
        if (newSlice->PicOrderCntVal != -1) {
          if (!p_addPOCToList(newSlice->PicOrderCntVal)) return false;
        }
      }
      else {
        // Do not add. Delete.
        delete newSlice;
      }
    }
    else if (nal_type == TRAIL_N  || nal_type == TRAIL_R    || nal_type == TSA_N  ||
             nal_type == TSA_R    || nal_type == STSA_N     || nal_type == STSA_R ||
             nal_type == RADL_N   || nal_type == RADL_R     || nal_type == RASL_N ||
             nal_type == RASL_R ) {
      
      // This is a slice and has a slice header (and a POC) but it is not a random access picture.
      // Parse the slice header (at least until we know the POC).
      slice *newSlice = new slice(curFilePos, nal_type, layer_id, temporal_id_plus1);
      if (!newSlice->parse_slice(getRemainingNALBytes(8), active_SPS_list, active_PPS_list)) return false;

      // Get the poc
      if (newSlice->PicOrderCntVal != -1) {
        if (!p_addPOCToList(newSlice->PicOrderCntVal)) return false;
      }

      // Don't save the position of non random access points.
      delete newSlice;
    }
  }

  // Finally sort the POC list
  std::sort(p_POC_List.begin(), p_POC_List.end());
  
  return true;
}

QByteArray de265File_FileHandler::getRemainingNALBytes(int maxBytes)
{
  QByteArray retArray;
  int nrBytesRead = 0;

  while (!curPosAtStartCode() && (maxBytes == -1 || nrBytesRead < maxBytes)) {
    // Save byte and goto the next one
    retArray.append( getCurByte() );

    if (!gotoNextByte()) {
      // No more bytes. Return all we got.
      return retArray;
    }

    nrBytesRead++;
  }

  // We should now be at a header byte. Remove the zeroes from the start code that we put into retArray
  while (retArray.endsWith(char(0))) {
    // Chop off last zero byte
    retArray.chop(1);
  }

  return retArray;
}

bool de265File_FileHandler::p_addPOCToList(int poc)
{
  if (poc < 0)
    return false;

  if (p_POC_List.contains(poc)) {
    // Two pictures with the same POC are not allowed
    return false;
  }
  
  p_POC_List.append(poc);
  return true;
}

// Look through the random access points and find the closest one before (or equal)
// the given frameIdx where we can start decoding
int de265File_FileHandler::getClosestSeekableFrameNumber(int frameIdx)
{
  // Get the POC for the frame number
  int iPOC = p_POC_List[frameIdx];

  // We schould always be able to seek to the beginning of the file
  int bestSeekPOC = p_POC_List[0];

  foreach(nal_unit *nal, p_nalUnitList) {
    if (nal->isSlice()) {
      // We can cast this to a slice.
      slice *s = dynamic_cast<slice*>(nal);

      if (s->PicOrderCntVal <= frameIdx) {
        // We could seek here
        bestSeekPOC = s->PicOrderCntVal;
      }
      else
        break;
    }
  }

  // Get the frame index for the given POC
  return p_POC_List.indexOf(bestSeekPOC);
}

QByteArray de265File_FileHandler::seekToFrameNumber(int iFrameNr)
{
  // Get the POC for the frame number
  int iPOC = p_POC_List[iFrameNr];

  // Collect the active parameter sets
  QMap<int, vps*> active_VPS_list;
  QMap<int, sps*> active_SPS_list;
  QMap<int, pps*> active_PPS_list;
  
  foreach(nal_unit *nal, p_nalUnitList) {
    if (nal->isSlice()) {
      // We can cast this to a slice.
      slice *s = dynamic_cast<slice*>(nal);

      if (s->PicOrderCntVal == iPOC) {
        // Seek here
        p_seekToFilePos(s->filePos);

        // Get the bitstream of all active parameter sets
        QByteArray paramSetStream;

        foreach(vps *v, active_VPS_list) {
          paramSetStream.append(v->getParameterSetData());
        }
        foreach(sps *s, active_SPS_list) {
          paramSetStream.append(s->getParameterSetData());
        }
        foreach(pps *p, active_PPS_list) {
          paramSetStream.append(p->getParameterSetData());
        }

        return paramSetStream;
      }
    }
    else if (nal->nal_type == VPS_NUT) {
      // Add vps (replace old one if existed)
      vps *v = dynamic_cast<vps*>(nal);
      active_VPS_list.insert(v->vps_video_parameter_set_id, v);
    }
    else if (nal->nal_type == SPS_NUT) {
      // Add sps (replace old one if existed)
      sps *s = dynamic_cast<sps*>(nal);
      active_SPS_list.insert(s->sps_seq_parameter_set_id, s);
    }
    else if (nal->nal_type == PPS_NUT) {
      // Add pps (replace old one if existed)
      pps *p = dynamic_cast<pps*>(nal);
      active_PPS_list.insert(p->pps_pic_parameter_set_id, p);
    }
  }

  return QByteArray();
}

bool de265File_FileHandler::p_seekToFilePos(quint64 pos)
{
  if (!p_srcFile->seek(pos))
    return false;

  p_bufferStartPosInFile = pos;
  p_numZeroBytes = 0;
  p_FileBufferSize = 0;

  return updateBuffer();
}

QSize de265File_FileHandler::getSequenceSize()
{
  // Find the first SPS and return the size
  foreach(nal_unit *nal, p_nalUnitList) {
    if (nal->nal_type == SPS_NUT) {
      sps *s = dynamic_cast<sps*>(nal);
      return QSize(s->pic_width_in_luma_samples, s->pic_height_in_luma_samples);
    }
  }

  return QSize(-1,-1);
}

double de265File_FileHandler::getFramerate()
{
  // First try to get the framerate from the parameter sets themselves
  foreach(nal_unit *nal, p_nalUnitList) {
    if (nal->nal_type == VPS_NUT) {
      vps *v = dynamic_cast<vps*>(nal);
      if (v->vps_timing_info_present_flag) {
        // The VPS knows the frame rate
        return v->frameRate;
      }
    }
  }

  // The VPS had no information on the frame rate.
  // Look for VUI information in the sps
  foreach(nal_unit *nal, p_nalUnitList) {
    if (nal->nal_type == SPS_NUT) {
      sps *s = dynamic_cast<sps*>(nal);
      if (s->vui_timing_info_present_flag) {
        // The VUI knows the frame rate
        return s->frameRate;
      }
    }
  }

  // Try to find the framerate from the file name
  int width, height, frameRate, bitDepth, subFormat;
  formatFromFilename(p_srcFile->fileName(), width, height, frameRate, bitDepth, subFormat);
  if (frameRate != -1) {
    return (double)frameRate;
  }

  return DEFAULT_FRAMERATE;
}