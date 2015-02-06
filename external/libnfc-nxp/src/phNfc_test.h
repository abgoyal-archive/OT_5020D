

/*
	all interface : if OK return 0, or return !0 . 
*/
/* Modifications on Features Request / Changes Request / Problems Report       */
/*=============================================================================*/
/* date    | author          | Key                      | comment              */
/*=========|=================|==========================|======================*/

extern int phNfc_test_rf_on(void);
extern void phNfc_test_typeAReader_on(void);
extern void phNfc_test_ResetDetectedTagCount_on(int flag);
extern int phNfc_test_GetDetectedTagCount_on(void);
