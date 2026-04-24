#define PIN_GND     100
#define PIN_5V	    150
#define PIN_3V3	    133
#define PIN_RST	    101  
#define PIN_BT      0

#if defined(USE_ESP32) 
//ESP32			
#define	PIN_L_1 	PIN_3V3	//3V3
#define	PIN_L_2 	39		//IO
#define	PIN_L_3 	34		//SD_MISO
#define	PIN_L_4	    13		//SD_MOSI
#define	PIN_L_5	    32		//IO
#define	PIN_L_6	    14		//SD_SCLK
#define	PIN_L_7	    34		//SD_MISO
#define	PIN_L_8 	33		//IO
#define	PIN_L_9	    14		//SD_SCLK
#define	PIN_L_10	13		//SD_MOSI
#define	PIN_L_11	4		//IO
#define	PIN_L_12	2		//IO
#define	PIN_L_13	15		//IO
#define	PIN_L_14	PIN_GND	//GND
#define	PIN_L_15	PIN_3V3	//3V3
			
			
#define	PIN_R_1	    PIN_5V	//5V
#define	PIN_R_2	    PIN_GND	//GND
#define	PIN_R_3	    1		//TXD
#define	PIN_R_4	    3		//RXD
#define	PIN_R_5	    PIN_RST	//RST
#define	PIN_R_6	    PIN_BT	//BOOT
#define	PIN_R_7	    5		//Out only
#define	PIN_R_8	    35		//IO
#define	PIN_R_9	    38		//Out only
#define	PIN_R_10	PIN_GND	//GND
#define	PIN_R_11	PIN_5V	//5V
#define	PIN_R_12	PIN_5V	//5V
#define	PIN_R_13	PIN_GND	//GND
#define	PIN_R_14	PIN_5V	//5V
#define	PIN_R_15	PIN_GND	//GND
			
#define	SD_MISO	5	
#define	SD_MOSI	6	
#define	SD_SCLK	7	
#define	SD_CS	42	
			
#define	ETH_RESET_PIN	-1	
#define	ETH_MDC_PIN	23	
#define	ETH_POWER_PIN	12	
#define	ETH_MDIO_PIN	18	

endif			
//ESP32-S3	
#if defined(USE_ESP32) 		
#define	PIN_L_1	    PIN_3V3	//3V3
#define	PIN_L_2	    4		//IO
#define	PIN_L_3	    5		//SD_MISO
#define	PIN_L_4	    6		//SD_MOSI
#define	PIN_L_5 	7		//SD_SCLK
#define	PIN_L_6	    15		//IO
#define	PIN_L_7	    16		//IO
#define	PIN_L_8	    17		//IO
#define	PIN_L_9	    18		//IO
#define	PIN_L_10	8		//IO
#define	PIN_L_11	19		//IO
#define	PIN_L_12	20		//IO
#define	PIN_L_13	3		//IO
#define	PIN_L_14	PIN_GND	//GND
#define	PIN_L_15	PIN_3V3	//3V3
			
			
#define	PIN_R_1	    PIN_5V	//5V
#define	PIN_R_2	    PIN_GND	//GND
#define	PIN_R_3	    43		//TXD
#define	PIN_R_4 	44		//RXD
#define	PIN_R_5	    PIN_RST	//RST
#define	PIN_R_6	    PIN_BT  //BOOT
#define	PIN_R_7	    1		//IO
#define	PIN_R_8	    2		//IO
#define	PIN_R_9	    41		//Out only
#define	PIN_R_10	40		//Out only
#define	PIN_R_11	39		//Out only
#define	PIN_R_12	38		//Out only
#define	PIN_R_13	21		//IO
#define	PIN_R_14	46		//IO
#define	PIN_R_15	PIN_GND	//GND
#define	PIN_R_16	PIN_5V	//5V
			
#define	PIN_E_1	48	//IO
#define	PIN_E_2	46	//IO
#define	PIN_E_3	47	//IO
			
#define	SD_MISO		5	
#define	SD_MOSI		6	
#define	SD_SCLK		7	
#define	SD_CS		42	
			
#define	ETH_CS_PIN		9	
#define	ETH_SCLK_PIN	10	
#define	ETH_MISO_PIN	11	
#define	ETH_MOSI_PIN	12	
#define	ETH_INT		    13	
#define	ETH_RST		    14	
#define	ETH_ADDR		-1	
endif