/*************************************************************************************************
a5142_otp.h
---------------------------------------------------------
OTP Application file From Truly for AR0543
2012.10.26
---------------------------------------------------------
NOTE:
The modification is appended to initialization of image sensor. 
After sensor initialization, use the function , and get the id value.
bool otp_wb_update(USHORT zone)
and
bool otp_lenc_update(USHORT zone), 
then the calibration of AWB and LSC will be applied. 
After finishing the OTP written, we will provide you the golden_rg and golden_bg settings.
**************************************************************************************************/
#ifndef _A5142_OTP_H_
#define _A5142_OTP_H_
#define USHORT             unsigned short

#ifndef __CAM_CAL_H
#define __CAM_CAL_H

#define CAM_CAL_DEV_MAJOR_NUMBER 226

/* CAM_CAL READ/WRITE ID */
#define A5142OTP_DEVICE_ID				0x6c//  0x40
#define I2C_UNIT_SIZE                                  2 //in byte
#define OTP_START_ADDR                            0x3810
#define OTP_SIZE                                      4

typedef struct{
    u16 flag;
    u16 data;
}stCAM_CAL_AWB_OTP_DATA, *stPCAM_CAL_AWB_OTP_DATA;


#endif /* __CAM_CAL_H */

/*************************************************************************************************
* Function    :  start_read_otp
* Description :  before read otp , set the reading block setting  
* Parameters  :  [USHORT] zone : OTP type index , ARO543 zone == 0
* Return      :  0, reading block setting err
                 1, reading block setting ok 
**************************************************************************************************/
bool start_read_otp(USHORT zone);


/*************************************************************************************************
* Function    :  get_otp_date
* Description :  get otp date value 
* Parameters  :  [USHORT] zone : OTP type index , ARO543 zone == 0
* Return      :  [CString] "" : OTP data fail 
                  other value : otp_written_date. example : 2012.10.26               
**************************************************************************************************/
/*
CString get_otp_date(USHORT zone) ;
*/


/*************************************************************************************************
* Function    :  get_otp_flag
* Description :  get otp WRITTEN_FLAG  
* Parameters  :  [USHORT] zone : OTP type index , ARO543 zone == 0
* Return      :  [USHORT], if 0x0001 , this type has valid otp data, otherwise, invalid otp data
**************************************************************************************************/
USHORT get_otp_flag(USHORT zone);


/*************************************************************************************************
* Function    :  get_otp_module_id
* Description :  get otp MID value 
* Parameters  :  [USHORT] zone : OTP type index , ARO543 zone == 0
* Return      :  [USHORT] 0 : OTP data fail 
                 other value : module ID data , TRULY ID is 0x0001            
**************************************************************************************************/
USHORT get_otp_module_id(USHORT zone);


/*************************************************************************************************
* Function    :  get_otp_lens_id
* Description :  get otp LENS_ID value 
* Parameters  :  [USHORT] zone : OTP type index , ARO543 zone == 0
* Return      :  [USHORT] 0 : OTP data fail 
                 other value : LENS ID data             
**************************************************************************************************/
USHORT get_otp_lens_id(USHORT zone);


/*************************************************************************************************
* Function    :  get_otp_vcm_id
* Description :  get otp VCM_ID value 
* Parameters  :  [USHORT] zone : OTP type index , ARO543 zone == 0
* Return      :  [USHORT] 0 : OTP data fail 
                 other value : VCM ID data             
**************************************************************************************************/
USHORT get_otp_vcm_id(USHORT zone);


/*************************************************************************************************
* Function    :  get_otp_driver_id
* Description :  get otp driver id value 
* Parameters  :  [USHORT] zone : OTP type index , ARO543 zone == 0
* Return      :  [USHORT] 0 : OTP data fail 
                 other value : driver ID data             
**************************************************************************************************/
USHORT get_otp_driver_id(USHORT zone);

/*************************************************************************************************
* Function    :  get_light_id
* Description :  get otp environment light temperature value  , ARO543 zone == 0
* Parameters  :  [USHORT] zone : OTP type index
* Return      :  [CString] "" : OTP data fail 
                 other value : Light temperature data             
**************************************************************************************************/
/*
CString get_light_id(USHORT zone);
*/

/*************************************************************************************************
* Function    :  otp_lenc_update
* Description :  Update lens correction 
* Parameters  :  [USHORT] zone : OTP type index , ARO543 zone == 0
* Return      :  [bool] 0 : OTP data fail 
                        1 : otp_lenc update success            
**************************************************************************************************/
bool otp_lenc_update(USHORT zone);

/*************************************************************************************************
* Function    :  wb_multi_cal
* Description :  Calculate register gain to gain muliple
* Parameters  :  [USHORT]value : Register value
* Return      :  [double]-1 : wb_multi_cal fail 
                     other : wb multiple value           
**************************************************************************************************/
int wb_multi_cal(USHORT value);

/*************************************************************************************************
* Function    :  wb_gain_cal
* Description :  Calculate gain muliple to register gain
* Parameters  :  [double]multiple : multiple value
* Return      :  [USHORT]other : wb multiple value           
**************************************************************************************************/
USHORT wb_gain_cal(int multiple);

/*************************************************************************************************
* Function    :  wb_gain_set
* Description :  Set WB ratio to register gain setting  
* Parameters  :  [double] r_ratio : R ratio data compared with golden module R
                         b_ratio : B ratio data compared with golden module B
* Return      :  [bool] 0 : set wb fail 
                        1 : WB set success            
**************************************************************************************************/
bool wb_gain_set(int r_ratio, int b_ratio);

/*************************************************************************************************
* Function    :  otp_wb_update
* Description :  Update WB correction 
* Parameters  :  [USHORT] zone : OTP type index , ARO543 zone == 0
* Return      :  [bool] 0 : OTP data fail 
                        1 : otp_WB update success            
**************************************************************************************************/
bool otp_wb_update(USHORT zone);

/*************************************************************************************************
* Function    :  get_otp_wb
* Description :  Get WB data  
* Parameters  :  [USHORT] zone : OTP type index , ARO543 zone == 0
* Return      :  [bool] 0 : OTP data fail 
                        1 : otp_WB update success            
**************************************************************************************************/
bool get_otp_wb(USHORT zone);

#endif
