/* **********************************************************
 * Copyright 2010 VMware, Inc.  All rights reserved. -- VMware Confidential
 * **********************************************************/

/*
 * svga3d_video.h --
 *
 *       SVGA 3d video hardware definitions
 */

#ifndef _SVGA3D_VIDEO_H_
#define _SVGA3D_VIDEO_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE

#include "includeCheck.h"

/*
 * Overall hardware video processing design:
 *
 * The entire process of decoding a video stream in hardware has two separate
 * components: the video decoder itself and the video processor. The video
 * decoder is responsible for taking the compressed video data and outputting
 * YUV frames, while the processor takes those YUV surfaces (and possibly
 * others) and converts them to RGB surfaces which can be used as textures or
 * otherwise.
 *
 * The decoder itself takes a variety of buffers in GMR memory - picture
 * parameters, bitstream data, etc. - and decompresses those buffers to a
 * certain level of a decoder render target, which always has an NV12 surface
 * format. This decompression can reference surfaces that were decoded earlier
 * in other levels of that same render target.
 *
 * The video processor takes several source surfaces, converts them to RGB,
 * and combines them into one output RGB surface. The first surface in the
 * list is the source video itself, and may be interlaced with separate fields
 * of the video interleaved together. The processor is then responsible for
 * deinterlacing that video with a deinterlace technology that is selected
 * upon processor device creation.
 *
 * The processor then converts all of the surfaces to RGB using different
 * parameters (color primaries, etc.) that are listed in the process command.
 * The separate surfaces are blitted onto the target surface and alpha blended
 * together to allow for subtitles, overlays, and other effects to be
 * processed.
 */

#define SVGA3D_CAPS_VIDEO_DECODE_MPEG2_IDCT   (1 << SVGA3D_VIDEO_PROFILE_MPEG2_IDCT)
#define SVGA3D_CAPS_VIDEO_DECODE_H264_E       (1 << SVGA3D_VIDEO_PROFILE_H264_E)
#define SVGA3D_CAPS_VIDEO_DECODE_WMV9_C       (1 << SVGA3D_VIDEO_PROFILE_WMV9_C)
#define SVGA3D_CAPS_VIDEO_DECODE_VC1_C        (1 << SVGA3D_VIDEO_PROFILE_VC1_C)
#define SVGA3D_CAPS_VIDEO_DECODE_VC1_D        (1 << SVGA3D_VIDEO_PROFILE_VC1_D)

typedef enum {
   SVGA3D_VIDEO_PROFILE_MPEG2_IDCT,
   SVGA3D_VIDEO_PROFILE_H264_E,      // H264 VLD NoFGT
   SVGA3D_VIDEO_PROFILE_WMV9_C,      // WMV9 IDCT
   SVGA3D_VIDEO_PROFILE_VC1_C,       // VC1 IDCT
   SVGA3D_VIDEO_PROFILE_VC1_D,       // VC1 VLD
   SVGA3D_VIDEO_PROFILE_H263,
   SVGA3D_VIDEO_PROFILE_MAX,
} SVGA3dVideoProfile;

typedef enum {
   SVGA3D_BUFFER_PICTURE_PARAMETERS,
   SVGA3D_BUFFER_MBCONTROL,
   SVGA3D_BUFFER_RDDATA,
   SVGA3D_BUFFER_DEBLOCKINGDATA,
   SVGA3D_BUFFER_IQMATRIX,
   SVGA3D_BUFFER_SLICEDATA,
   SVGA3D_BUFFER_BITPLANE,
   SVGA3D_BUFFER_MAX
} SVGA3dVideoBufferType;

typedef enum {
   SVGA3D_H264CONFIGURATION_LONG_SLICEDATA = 1,
   SVGA3D_H264CONFIGURATION_SHORT_SLICEDATA,
   SVGA3D_H264CONFIGURATION_MAX
} SVGA3dH264Configuration;

typedef enum {
   SVGA3D_MPEG2CONFIGURATION_OFFHOST_INVERSESCAN,
   SVGA3D_MPEG2CONFIGURATION_HOST_INVERSESCAN,
   SVGA3D_MPEG2CONFIGURATION_MAX
} SVGA3dMPEG2Configuration;

/*
 * Bit 0 ~ 15 are available for de-interlace technique.
 */
typedef enum {
   SVGA3D_DEINTERLACE_NONE = 0, /* Progressive */
   SVGA3D_DEINTERLACE_BOB,
   SVGA3D_DEINTERLACE_PIXELADAPTIVE,
   SVGA3D_DEINTERLACE_MAX
} SVGA3dDeinterlaceType;

/*
 * Bit 16 ~ 31 are available for video processor caps.
 */
typedef enum {
   SVGA3D_PROCESSOROPS_YUV2RGBEXTEND = 16,
   SVGA3D_PROCESSOROPS_MAX
} SVGA3dProcessorOperations;

#define SVGA3D_CAPS_VIDEO_PROCESS_PROGRESSIVE   (1 << SVGA3D_DEINTERLACE_NONE)
#define SVGA3D_CAPS_VIDEO_PROCESS_BOB           (1 << SVGA3D_DEINTERLACE_BOB)
#define SVGA3D_CAPS_VIDEO_PROCESS_PIXELADAPTIVE (1 << SVGA3D_DEINTERLACE_PIXELADAPTIVE)
#define SVGA3D_CAPS_VIDEO_PROCESS_YUV2RGBEXTEND (1 << SVGA3D_PROCESSOROPS_YUV2RGBEXTEND)

/*
 * For now, the video processor and its data structures are based off of DXVA
 * 2.0: http://msdn.microsoft.com/en-us/library/cc307964%28VS.85%29.aspx
 */

typedef
#include "vmware_pack_begin.h"
struct {
   uint16 Cr;
   uint16 Cb;
   uint16 Y;
   uint16 Alpha;
}
#include "vmware_pack_end.h"
SVGA3dAYUV16;

typedef
#include "vmware_pack_begin.h"
struct {
   union {
      struct {
         uint16 fracPart;
         int16 intPart;
      };
      uint32 fixed;
   };
}
#include "vmware_pack_end.h"
SVGA3dFixed32;

typedef
#include "vmware_pack_begin.h"
struct {
   uint32 numerator;
   uint32 denominator;
}
#include "vmware_pack_end.h"
SVGA3dFraction64;

typedef enum {
   SVGA3D_SAMPLE_UNKNOWN                     = 0,
   SVGA3D_SAMPLE_PROGRESSIVEFRAME            = 2,
   SVGA3D_SAMPLE_FIELDINTERLEAVEDEVENFIRST   = 3,
   SVGA3D_SAMPLE_FIELDINTERLEAVEDODDFIRST    = 4,
   SVGA3D_SAMPLE_FIELDSINGLEEVEN             = 5,
   SVGA3D_SAMPLE_FIELDSINGLEODD              = 6,
   SVGA3D_SAMPLE_SUBSTREAM                   = 7,
} SVGA3dSampleFormat;

typedef enum {
   SVGA3D_VIDEOCHROMASUBSAMPLING_UNKNOWN = 0,

   /*
    * Reconstruct the chroma as if the video was progressive - don't skip
    * fields or otherwise try to reduce aliasing artifacts.
    */
   SVGA3D_VIDEOCHROMASUBSAMPLING_PROGRESSIVECHROMA = 0x8,

   /*
    * The chroma samples are aligned with the luma samples instead of being
    * half a pixel to the right.
    */
   SVGA3D_VIDEOCHROMASUBSAMPLING_HORIZONTALLY_COSITED = 0x4,

   /*
    * The chroma samples are aligned with the luma samples instead of being
    * half a pixel down.
    */
   SVGA3D_VIDEOCHROMASUBSAMPLING_VERTICALLY_COSITED = 0x2,

   /*
    * If this is set then the chroma planes are vertically aligned -
    * otherwise, the Cb and Cr samples are on alternate lines.
    */
   SVGA3D_VIDEOCHROMASUBSAMPLING_VERTICALLY_ALIGNEDCHROMAPLANES = 0x1,
} SVGA3dVideoChromaSubSampling;

typedef enum {
   SVGA3D_NOMINALRANGE_UNKNOWN   = 0,
   SVGA3D_NOMINALRANGE_0_255     = 1,
   SVGA3D_NOMINALRANGE_16_235    = 2,
   SVGA3D_NOMINALRANGE_48_208    = 3,
} SVGA3dNominalRange;

typedef enum {
   /*
    * Unknown should be treated as BT.601 for SD content and BT.709 for HD
    * content (anything with a height greater than 576 lines).
    */
   SVGA3D_VIDEOTRANSFERMATRIX_UNKNOWN     = 0,
   SVGA3D_VIDEOTRANSFERMATRIX_BT709       = 1,
   SVGA3D_VIDEOTRANSFERMATRIX_BT601       = 2,
   SVGA3D_VIDEOTRANSFERMATRIX_SMPTE240M   = 3,
} SVGA3dVideoTransferMatrix;

typedef enum {
   SVGA3D_VIDEOLIGHTING_UNKNOWN  = 0,
   SVGA3D_VIDEOLIGHTING_BRIGHT   = 1,
   SVGA3D_VIDEOLIGHTING_OFFICE   = 2,
   SVGA3D_VIDEOLIGHTING_DIM      = 3,
   SVGA3D_VIDEOLIGHTING_DARK     = 4,
} SVGA3dVideoLighting;

typedef enum {
   /* Treat like BT709 */
   SVGA3D_VIDEOPRIMARIES_UNKNOWN       = 0,
   SVGA3D_VIDEOPRIMARIES_BT709         = 2,
   SVGA3D_VIDEOPRIMARIES_BT470_2_SysM  = 3,
   SVGA3D_VIDEOPRIMARIES_BT470_2_SysBG = 4,
   SVGA3D_VIDEOPRIMARIES_SMPTE170M     = 5,
   SVGA3D_VIDEOPRIMARIES_SMPTE240M     = 6,
   SVGA3D_VIDEOPRIMARIES_EBU3213       = 7,
   SVGA3D_VIDEOPRIMARIES_SMPTE_C       = 8,
} SVGA3dVideoPrimaries;

/* Choose the gamma to use when converting from linear RGB to non-linear */
typedef enum {
   SVGA3D_VIDEOTRANSFERFUNC_UNKNOWN = 0,
   SVGA3D_VIDEOTRANSFERFUNC_10      = 1,
   SVGA3D_VIDEOTRANSFERFUNC_18      = 2,
   SVGA3D_VIDEOTRANSFERFUNC_20      = 3,
   SVGA3D_VIDEOTRANSFERFUNC_22      = 4,
   SVGA3D_VIDEOTRANSFERFUNC_709     = 5,
   SVGA3D_VIDEOTRANSFERFUNC_240M    = 6,
   SVGA3D_VIDEOTRANSFERFUNC_SRGB    = 7,
   SVGA3D_VIDEOTRANSFERFUNC_28      = 8,
} SVGA3dVideoTransferFunction;

typedef
#include "vmware_pack_begin.h"
union {
   struct {
      uint32 sampleFormat : 8;            /* SVGA3dSampleFormat */
      uint32 videoChromaSubsampling : 4;  /* SVGA3dVideoChromaSubSampling */
      uint32 nominalRange : 3;            /* SVGA3dNominalRange */
      uint32 videoTransferMatrix : 3;     /* SVGA3dVideoTransferMatrix */
      uint32 videoLighting : 4;           /* SVGA3dVideoLighting */
      uint32 videoPrimaries : 5;          /* SVGA3dVideoPrimaries */
      uint32 videoTransferFunction : 5;   /* SVGA3dVideoTransferFunction */
   };
   uint32 value;
}
#include "vmware_pack_end.h"
SVGA3dExtendedVideoFormat;

/*
 * The ProcAmp represents what is traditionally known as the "processing
 * amplifier". It is in charge of modifying colors in various ways.
 */

typedef enum {
   SVGA3D_PROCAMP_BRIGHTNESS,
   SVGA3D_PROCAMP_CONTRAST,
   SVGA3D_PROCAMP_HUE,
   SVGA3D_PROCAMP_SATURATION,
   SVGA3D_PROCAMP_MAX
} SVGA3dProcAmpTypes;

typedef struct {
   SVGA3dFixed32 min;
   SVGA3dFixed32 max;
   SVGA3dFixed32 defaultValue;
   SVGA3dFixed32 increment;
} SVGA3dProcAmpRange;

typedef
#include "vmware_pack_begin.h"
struct {
   SVGA3dFixed32 values[SVGA3D_PROCAMP_MAX];
}
#include "vmware_pack_end.h"
SVGA3dProcAmpValues;


/*
 * The _SVGA_PicEntry_H264 and _SVGA_PicParams_H264 are based on
 * the DXVA_H264.pdf document that can be found at -
 * http://download.microsoft.com/download/5/f/c/5fc4ec5c-bd8c-4624-8034-319c1bab7671/DXVA_H264.pdf
 */

typedef
struct _SVGA_PicEntry_H264 {
   union {
      struct {
         uint8 Index7Bits :7;
         uint8 AssociatedFlag :1;
      };
      uint8 bPicEntry;
   };
} SVGA_PicEntry_H264, *LPSVGA_PicEntry_H264;

typedef
struct _SVGA_PicParams_H264 {
   uint16 wFrameWidthInMbsMinus1;
   uint16 wFrameHeightInMbsMinus1;
   SVGA_PicEntry_H264 CurrPic;
   uint8 num_ref_frames;
   union {
      struct {
         uint8 field_pic_flag                 :1;
         uint8 MbaffFrameFlag                 :1;
         uint8 residual_colour_transform_flag :1;
         uint8 sp_for_switch_flag             :1;
         uint8 chroma_format_idc              :2;
         uint8 RefPicFlag                     :1;
         uint8 constrained_intra_pred_flag    :1;
         uint8 weighted_pred_flag             :1;
         uint8 weighted_bipred_idc            :2;
         uint8 MbsConsecutiveFlag             :1;
         uint8 frame_mbs_only_flag            :1;
         uint8 transform_8x8_mode_flag        :1;
         uint8 MinLumaBipredSize8x8Flag       :1;
         uint8 IntraPicFlag                   :1;
      };
      uint16 wBitFields;
   };
   uint8 bit_depth_luma_minus8;
   uint8 bit_depth_chroma_minus8;
   uint16 Reserved16Bits;
   unsigned int StatusReportFeedbackNumber;
   SVGA_PicEntry_H264 RefFrameList[16];
   int32 CurrFieldOrderCnt[2];
   int32 FieldOrderCntList[16][2];
   int8	pic_init_qs_minus26;
   int8	chroma_qp_index_offset;
   int8	second_chroma_qp_index_offset;
   uint8 ContinuationFlag;
   int8	pic_init_qp_minus26;
   uint8 num_ref_idx_10_active_minus1;
   uint8 num_ref_idx_11_active_minus1;
   uint8 Reserved8BitsA;
   uint16 FrameNumList[16];
   unsigned int UsedForReferenceFlags;
   uint16 NonExistingFrameFlags;
   uint16 frame_num;
   uint8 log2_max_frame_num_minus4;
   uint8 pic_order_cnt_type;
   uint8 log2_max_pic_order_cnt_lsb_minus4;
   uint8 delta_pic_order_always_zero_flag;
   uint8 direct_8x8_inference_flag;
   uint8 entropy_coding_mode_flag;
   uint8 pic_order_present_flag;
   uint8 num_slice_groups_minus1;
   uint8 slice_group_map_type;
   uint8 deblocking_filter_control_present_flag;
   uint8 redundant_pic_cnt_present_flag;
   uint8 Reserved8BitsB;
   uint16 slice_group_change_rate_minus1;
   uint8 SliceGroupMap[810];
} SVGA_PicParams_H264, *LPSVGA_PicParams_H264;

typedef
struct _SVGA_Qmatrix_H264 {
    uint8 scalingLists4x4[6][16];
    uint8 scalingLists8x8[2][64];
} SVGA_Qmatrix_H264;

typedef
struct _SVGA_Slice_H264_Long {
   uint32 BSNALunitDataLocation;
   uint32 SliceBytesInBuffer;
   uint16 wBadSliceChopping;
   uint16 first_mb_in_slice;
   uint16 NumMbsForSlice;
   uint16 BitOffsetToSliceData;
   uint8  slice_type;
   uint8  luma_log2_weight_denom;
   uint8  chroma_log2_weight_denom;
   uint8  num_ref_idx_l0_active_minus1;
   uint8  num_ref_idx_l1_active_minus1;
   int8   slice_alpha_c0_offset_div2;
   int8   slice_beta_offset_div2;
   uint8  Reserved8Bits;
   SVGA_PicEntry_H264 RefPicList[2][32];
   int16  Weights[2][32][3][2];
   int8   slice_qs_delta;
   int8   slice_qp_delta;
   uint8  redundant_pic_cnt;
   uint8  direct_spatial_mv_pred_flag;
   uint8  cabac_init_idc;
   uint8  disable_deblocking_filter_idc;
   uint16 slice_id;
} SVGA_Slice_H264_Long;

#endif /* _SVGA3D_VIDEO_H_ */
