

#ifndef _fan5405_SW_H_
#define _fan5405_SW_H_

#define fan5405_CON0      0x00
#define fan5405_CON1      0x01
#define fan5405_CON2      0x02
#define fan5405_CON3      0x03
#define fan5405_CON4      0x04
#define fan5405_CON5      0x05
#define fan5405_CON6      0x06
#define fan5405_CON16      0x10

struct fan5405_struct {
        const char *name;

        struct fan5405_ops      *ops;

	uint8_t 	boost_fault_status;
	uint8_t 	chg_fault_status;	/*add for check and recovery from fault state*/

	unsigned int 	chg_type;	/**/

	uint8_t	config_count;	/*mark charge params need be config or not, if > 0, need to config again*/
	uint8_t	disable_count;	/*mark if should set fan5405 to HZ mode, if > 0 && no charger on, need to disable charging*/

	uint8_t	work_mode;	/*charge mode or Boost mode*/
#define	FAN5405_CHARGE_MODE	0x00
#define	FAN5405_BOOST_MODE	0x01

	struct	mutex	fan_ops_mutex;
	
};

enum boost_mode_fault_status{
	BOOST_NORMAL_STATUS	= 0x00,
	BOOST_VBUS_OVP_STATUS	= 0x01,	/*vbus > vbus_ovp(6.09--6.49)*/
	BOOST_VBUS_FAIL_STATUS	= 0x02,	/*Vbus fails to achieve the voltage required to advance to the next state during soft-start or ...*/
	BOOST_VBAT_LOWER_STATUS	= 0x03,	/*VBAT 2.42 --- 2.7*/
	BOOST_IC_STUCK0_STATUS  = 0x04, /*N/A: This code does not appear*/
	BOOST_THERMAL_SHUTDOWN	= 0x05,	/*thermal shutdown*/
	BOOST_TIMER_FAULT	= 0x06,	/*timer fault, all register reset*/
	BOOST_IC_STUCK1_STATUS  = 0x07, /*N/A: This code does not appear*/
	BOOST_INVALID_STATUS
};

enum charge_mode_fault_status{
        CHARGE_NORMAL_STATUS     = 0x00,
        CHARGE_VBUS_OVP_STATUS   = 0x01, /*Vbus > vbus_ovp(6.09--6.49)*/
        CHARGE_SLEEP_MODE  = 0x02, /*vbus fall below Vbat+Vslp*/
        CHARGE_POOR_INPUT_SOURCE	= 0x03, /**/
        CHARGE_BATTERY_OVP	= 0x04, /**/
        CHARGE_THERMAL_SHUTDOWN  = 0x05, /*thermal shutdown*/
        CHARGE_TIMER_FAULT       = 0x06,  /*timer fault, all register reset*/
	CHARGE_NO_BATTERY	= 0x07,
	CHARGE_INVALID_STATUS	
};

struct fan5405_ops {

        int8_t (*get_reg_value)(struct fan5405_struct *fan_struct, uint8_t reg);                /*CON1 bit7:6           CONTROL1*/


        int8_t (*set_con0_val)(struct fan5405_struct *fan_struct, uint8_t t32_rst, uint8_t en_stat);    /*CON0 bit7     CONTROL0*/

        int8_t (*set_con1_value)(struct fan5405_struct *fan_struct, uint8_t Iinlim, uint8_t Vlowv, uint8_t Te, uint8_t Ce, uint8_t He, uint8_t Opa);      
								/*CON1 bit7:6  CONTROL1*/

        int8_t (*set_con2_val)(struct fan5405_struct *fan_struct, uint8_t Vchg_out, uint8_t otg_pl, uint8_t otg_en);  
									/*CON2 bit7:2           OREG*/

//        int8_t (*get_ic_info)(struct fan5405_struct *); /*CON3 bit 7:0, vendor code(100), PN(4:3), REV(2:0)*/

        int8_t (*set_con4_val)(struct fan5405_struct *fan_struct,uint8_t reset, uint8_t Iochg, uint8_t Iterm);        
										/*bit7      IBAT*/

        int8_t (*set_con5_val)(struct fan5405_struct *fan_struct,
                                uint8_t dis_verg, uint8_t io_level, uint8_t vsp);       /*CON5 bit2:0           special charger voltage*/

        int8_t (*set_con6_val)(struct fan5405_struct *fan_struct,
                                uint8_t Isafe, uint8_t Vsafe);          /*CON6 bit6:4   Isafe*/

	int8_t (*disable_charging)(struct fan5405_struct *fan_struct, uint8_t disable);
	int8_t (*check_chgstat_pin)(struct fan5405_struct *fan_struct);
	int8_t (*set_otg_pin)(struct fan5405_struct *fan_struct, uint8_t otg_level);

};

//CON0
#define CON0_TMR_RST_MASK 	0x01
#define CON0_TMR_RST_SHIFT 	7

//debug liao no use
#define CON0_SLRST_MASK 	0x01
#define CON0_SLRST_SHIFT 	7

#define CON0_EN_STAT_MASK 	0x01
#define CON0_EN_STAT_SHIFT 	6

#define CON0_STAT_MASK 		0x03
#define CON0_STAT_SHIFT 	4

#define CON0_FAULT_MASK 	0x07
#define CON0_FAULT_SHIFT 	0

//CON1
#define CON1_LIN_LIMIT_MASK 	0x03
#define CON1_LIN_LIMIT_SHIFT 	6

#define CON1_LOW_V_2_MASK 	0x01
#define CON1_LOW_V_2_SHIFT 	5

#define CON1_LOW_V_1_MASK 	0x01
#define CON1_LOW_V_1_SHIFT 	4

#define CON1_TE_MASK 	0x01
#define CON1_TE_SHIFT 	3

#define CON1_CE_MASK 	0x01
#define CON1_CE_SHIFT 	2

#define CON1_HZ_MODE_MASK 	0x01
#define CON1_HZ_MODE_SHIFT 	1

#define CON1_OPA_MODE_MASK 	0x01
#define CON1_OPA_MODE_SHIFT 0

//CON2
#define CON2_CV_VTH_MASK 	0x3F
#define CON2_CV_VTH_SHIFT 	2

//CON3
#define CON3_VENDER_CODE_MASK 	0x07
#define CON3_VENDER_CODE_SHIFT 	5

#define CON3_PIN_MASK 	0x03
#define CON3_PIN_SHIFT 	3

#define CON3_REVISION_MASK 		0x07
#define CON3_REVISION_SHIFT 	0

//CON4
#define CON4_RESET_MASK		0x01
#define CON4_RESET_SHIFT 	7

//#define CON4_I_CHR_MASK		0x0F
//#define CON4_I_CHR_SHIFT 	3


#define CON4_I_CHR_MASK		0x07
#define CON4_I_CHR_SHIFT 	4

#define CON4_I_TERM_MASK	0x07
#define CON4_I_TERM_SHIFT 	0

//CON5
#define CON5_LOW_CHG_MASK	0x01
#define CON5_LOW_CHG_SHIFT 	5

#define CON5_DPM_STATUS_MASK	0x01
#define CON5_DPM_STATUS_SHIFT 	4

#define CON5_CD_STATUS_MASK		0x01
#define CON5_CD_STATUS_SHIFT 	3

#define CON5_VSREG_MASK		0x07
#define CON5_VSREG_SHIFT 	0

//CON6
//#define CON6_MCHRG_MASK		0x0F
//#define CON6_MCHRG_SHIFT 	4

#define CON6_MCHRG_MASK		0x07
#define CON6_MCHRG_SHIFT 	4

#define CON6_MREG_MASK		0x0F
#define CON6_MREG_SHIFT 	0


int8_t fan5405_set_con1_val(struct fan5405_struct *fan_struct,
                                uint8_t Iinlim, uint8_t Vlowv, uint8_t Te, uint8_t Ce, uint8_t Hz, uint8_t Opa);


#if 0
//CON0
extern void fan5405_set_tmr_rst(unsigned int val);
extern unsigned int fan5405_get_slrst_status(void);
extern unsigned int fan5405_get_chip_status(void);
extern unsigned int fan5405_get_fault_reason(void);

//CON1
extern void fan5405_set_lin_limit(unsigned int val);
extern void fan5405_set_lowv_2(unsigned int val);
extern void fan5405_set_lowv_1(unsigned int val);
extern void fan5405_set_te(unsigned int val);
extern void fan5405_set_ce(unsigned int val);
extern void fan5405_set_opa_mode(unsigned int val);

//CON2
extern void fan5405_set_cv_vth(unsigned int val);

//CON3
extern unsigned int fan5405_get_vender_code(void);
extern unsigned int fan5405_get_pin(void);
extern unsigned int fan5405_get_revision(void);

//CON4
extern void fan5405_set_reset(unsigned int val);
extern void fan5405_set_ac_charging_current(unsigned int val);
extern void fan5405_set_termination_current(unsigned int val);

//CON5
extern void fan5405_set_low_chg(unsigned int val);
extern unsigned int fan5405_get_dpm_status(void);
extern unsigned int fan5405_get_cd_status(void);
extern void fan5405_set_vsreg(unsigned int val);

//CON6
extern void fan5405_set_mchrg(unsigned int val);
extern void fan5405_set_mreg(unsigned int val);

extern void fan5405_dump_register(void);

//extern int fan5405_read_byte(struct fan5405_struct * fan_struct, uint8_t addr, uint8_t reg, uint8_t mask, uint8_t shift);
extern unsigned int  fan5405_config_interface_liao (unsigned char RegNum, unsigned char val);
#endif
#endif // _fan5405_SW_H_

