/*_____ I N C L U D E S ____________________________________________________*/
#include <stdio.h>
#include <string.h>
#include "NuMicro.h"

#include "misc_config.h"
#include "i2c_master.h"
#include <stdlib.h>

/*_____ D E C L A R A T I O N S ____________________________________________*/

struct flag_32bit flag_PROJ_CTL;
#define FLAG_PROJ_TIMER_PERIOD_1000MS                 	(flag_PROJ_CTL.bit0)
#define FLAG_PROJ_EEP_DUMP                   			(flag_PROJ_CTL.bit1)
#define FLAG_PROJ_EEP_WRITE_ADDR                 		(flag_PROJ_CTL.bit2)
#define FLAG_PROJ_EEP_WRITE_DATA                        (flag_PROJ_CTL.bit3)
#define FLAG_PROJ_EEP_WRITE_DATA1                       (flag_PROJ_CTL.bit4)
#define FLAG_PROJ_EEP_READ                       		(flag_PROJ_CTL.bit5)
#define FLAG_PROJ_EEP_ERASE                             (flag_PROJ_CTL.bit6)
#define FLAG_PROJ_EEP_CTRL                              (flag_PROJ_CTL.bit7)
#define FLAG_PROJ_EEP_WRITE_DATA_ARRAY                  (flag_PROJ_CTL.bit8)
#define FLAG_PROJ_EEP_USE_API                         	(flag_PROJ_CTL.bit9)
#define FLAG_PROJ_EEP_READ_ALL                         	(flag_PROJ_CTL.bit10)


/*_____ D E F I N I T I O N S ______________________________________________*/

volatile unsigned int counter_systick = 0;
volatile uint32_t counter_tick = 0;

// I2C
#define EEPROM_SLAVE_ADDR    					(0xA0)


volatile uint8_t g_u8DeviceAddr_m;
volatile uint16_t g_u16DataLen_m;
volatile uint16_t rawlenth;
volatile uint16_t g_au16Reg;
volatile uint8_t g_u8EndFlag = 0;
uint8_t *g_au8Buffer;

typedef void (*I2C_FUNC)(uint32_t u32Status);

I2C_FUNC __IO I2Cx_Master_HandlerFn = NULL;

/*_____ M A C R O S ________________________________________________________*/

/*_____ F U N C T I O N S __________________________________________________*/

unsigned int get_systick(void)
{
	return (counter_systick);
}

void set_systick(unsigned int t)
{
	counter_systick = t;
}

void systick_counter(void)
{
	counter_systick++;
}

void SysTick_Handler(void)
{

    systick_counter();

    if (get_systick() >= 0xFFFFFFFF)
    {
        set_systick(0);      
    }

    // if ((get_systick() % 1000) == 0)
    // {
       
    // }

    #if defined (ENABLE_TICK_EVENT)
    TickCheckTickEvent();
    #endif    
}

void SysTick_delay(unsigned int delay)
{  
    
    unsigned int tickstart = get_systick(); 
    unsigned int wait = delay; 

    while((get_systick() - tickstart) < wait) 
    { 
    } 

}

void SysTick_enable(unsigned int ticks_per_second)
{
    set_systick(0);
    if (SysTick_Config(SystemCoreClock / ticks_per_second))
    {
        /* Setup SysTick Timer for 1 second interrupts  */
        printf("Set system tick error!!\n");
        while (1);
    }

    #if defined (ENABLE_TICK_EVENT)
    TickInitTickEvent();
    #endif
}

uint32_t get_tick(void)
{
	return (counter_tick);
}

void set_tick(uint32_t t)
{
	counter_tick = t;
}

void tick_counter(void)
{
	counter_tick++;
    if (get_tick() >= 60000)
    {
        set_tick(0);
    }
}

// void delay_ms(uint16_t ms)
// {
// 	TIMER_Delay(TIMER0, 1000*ms);
// }


void I2Cx_Master_LOG(uint32_t u32Status)
{
	#if defined (DEBUG_LOG_MASTER_LV1)
    printf("%s  : 0x%2x \r\n", __FUNCTION__ , u32Status);
	#endif
}


void I2Cx_Master_IRQHandler(void)
{
    uint32_t u32Status;

    u32Status = I2C_GET_STATUS(MASTER_I2C);

    if (I2C_GET_TIMEOUT_FLAG(MASTER_I2C))
    {
        /* Clear I2C Timeout Flag */
        I2C_ClearTimeoutFlag(MASTER_I2C);                   
    }    
    else
    {
        if (I2Cx_Master_HandlerFn != NULL)
            I2Cx_Master_HandlerFn(u32Status);
    }
}

void I2Cx_MasterRx_multi(uint32_t u32Status)
{
    if(u32Status == MASTER_START_TRANSMIT) //0x08                       	/* START has been transmitted and prepare SLA+W */
    {
        I2C_SET_DATA(MASTER_I2C, ((g_u8DeviceAddr_m << 1) | I2C_WR));    				/* Write SLA+W to Register I2CDAT */
        I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI);

		I2Cx_Master_LOG(u32Status);
    }
    else if(u32Status == MASTER_TRANSMIT_ADDRESS_ACK) //0x18        			/* SLA+W has been transmitted and ACK has been received */
    {
        I2C_SET_DATA(MASTER_I2C, HIBYTE(g_au16Reg));
        I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI );

		FLAG_PROJ_EEP_CTRL = 1;
		
		I2Cx_Master_LOG(u32Status);
    }
    else if(u32Status == MASTER_TRANSMIT_ADDRESS_NACK) //0x20            	/* SLA+W has been transmitted and NACK has been received */
    {
        I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI | I2C_CTL_STA | I2C_CTL_STO);
		
		I2Cx_Master_LOG(u32Status);
    }
    else if(u32Status == MASTER_TRANSMIT_DATA_ACK) //0x28                  	/* DATA has been transmitted and ACK has been received */
    {
        if (rawlenth > 0)
        {
			if (FLAG_PROJ_EEP_CTRL)
			{
				FLAG_PROJ_EEP_CTRL = 0;

				I2C_SET_DATA(MASTER_I2C, LOBYTE(g_au16Reg));
	        	I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI );
			}
			else
			{		
				I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI | I2C_CTL_STA);				//repeat start
			}
        }
		else
		{
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI | I2C_CTL_STO);
			g_u8EndFlag = 1;
		}
		
		I2Cx_Master_LOG(u32Status);
    }
    else if(u32Status == MASTER_REPEAT_START) //0x10                  		/* Repeat START has been transmitted and prepare SLA+R */
    {
        I2C_SET_DATA(MASTER_I2C, ((g_u8DeviceAddr_m << 1) | I2C_RD));   		/* Write SLA+R to Register I2CDAT */
        I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI );
		
		I2Cx_Master_LOG(u32Status);
    }
    else if(u32Status == MASTER_RECEIVE_ADDRESS_ACK) //0x40                	/* SLA+R has been transmitted and ACK has been received */
    {
		if (rawlenth > 1)
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI | I2C_CTL_AA);
		else
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI);

		I2Cx_Master_LOG(u32Status);
    }
	else if(u32Status == MASTER_RECEIVE_DATA_ACK) //0x50                 	/* DATA has been received and ACK has been returned */
    {
        g_au8Buffer[g_u16DataLen_m++] = (unsigned char) I2C_GetData(MASTER_I2C);
        if (g_u16DataLen_m < (rawlenth-1))
		{
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI | I2C_CTL_AA);
		}
		else
		{
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI);
		}
		
		I2Cx_Master_LOG(u32Status);
    }
    else if(u32Status == MASTER_RECEIVE_DATA_NACK) //0x58                  	/* DATA has been received and NACK has been returned */
    {
        g_au8Buffer[g_u16DataLen_m++] = (unsigned char) I2C_GetData(MASTER_I2C);
        

        if (g_u16DataLen_m < (rawlenth-1))
		{
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI);
		}
		else
		{
			I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI | I2C_CTL_STO);
			g_u8EndFlag = 1;
		}
      		
		I2Cx_Master_LOG(u32Status);
    }
    else
    {
		// #if defined (DEBUG_LOG_MASTER_LV1)
        /* TO DO */
        printf("I2Cx_MasterRx_multi Status 0x%x is NOT processed\n", u32Status);
		// #endif
    }
}

void I2Cx_MasterTx_multi(uint32_t u32Status)
{	
    if(u32Status == MASTER_START_TRANSMIT)  //0x08                     	/* START has been transmitted */
    {
        I2C_SET_DATA(MASTER_I2C, ((g_u8DeviceAddr_m << 1) | I2C_WR));    			/* Write SLA+W to Register I2CDAT */
        I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI);

		I2Cx_Master_LOG(u32Status);
		
    }
    else if(u32Status == MASTER_TRANSMIT_ADDRESS_ACK)  //0x18           	/* SLA+W has been transmitted and ACK has been received */
    {
        I2C_SET_DATA(MASTER_I2C, HIBYTE(g_au16Reg));
        I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI );

		FLAG_PROJ_EEP_CTRL = 1;
		
		I2Cx_Master_LOG(u32Status);	
    }
    else if(u32Status == MASTER_TRANSMIT_ADDRESS_NACK) //0x20           /* SLA+W has been transmitted and NACK has been received */
    {
        I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_STA | I2C_CTL_STO | I2C_CTL_SI);

		I2Cx_Master_LOG(u32Status);	
    }
    else if(u32Status == MASTER_TRANSMIT_DATA_ACK) //0x28              	/* DATA has been transmitted and ACK has been received */
    {
        if(g_u16DataLen_m < rawlenth)
        {
			if (FLAG_PROJ_EEP_CTRL)
			{
				FLAG_PROJ_EEP_CTRL = 0;

				I2C_SET_DATA(MASTER_I2C, LOBYTE(g_au16Reg));
			}
			else
			{
				// printf("g_au8Buffer = 0x%2X\r\n" , g_au8Buffer[g_u16DataLen_m]);
	            I2C_SET_DATA(MASTER_I2C, g_au8Buffer[g_u16DataLen_m++]);
			}
	        I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI | I2C_CTL_AA);
        }
        else
        {
            I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_STO | I2C_CTL_SI);
            g_u8EndFlag = 1;
        }

		I2Cx_Master_LOG(u32Status);		
    }
    else if(u32Status == MASTER_ARBITRATION_LOST) //0x38
    {
		I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_STA_SI_AA);

		I2Cx_Master_LOG(u32Status);		
    }
    else if(u32Status == BUS_ERROR) //0x00
    {
		I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_STO_SI_AA);
		I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_SI_AA);
		
		I2Cx_Master_LOG(u32Status);		
    }		
    else
    {
		// #if defined (DEBUG_LOG_MASTER_LV1)
        /* TO DO */
        printf("I2Cx_MasterTx_multi Status 0x%x is NOT processed\n", u32Status);
		// #endif
    }
}

void I2Cx_WriteMultiToSlaveIRQ(uint8_t address,uint16_t reg,uint8_t *data,uint16_t len)
{		
	g_u8DeviceAddr_m = address;
	rawlenth = len;
	g_au16Reg = reg;
	g_au8Buffer = data;

	g_u16DataLen_m = 0;
	g_u8EndFlag = 0;

	/* I2C function to write data to slave */
	I2Cx_Master_HandlerFn = (I2C_FUNC)I2Cx_MasterTx_multi;

//	printf("I2Cx_MasterTx_multi finish\r\n");

	/* I2C as master sends START signal */
	I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_STA);

	/* Wait I2C Tx Finish */
	while(g_u8EndFlag == 0);
	g_u8EndFlag = 0;

}

void I2Cx_ReadMultiFromSlaveIRQ(uint8_t address,uint16_t reg,uint8_t *data,uint16_t len)
{ 
	g_u8DeviceAddr_m = address;
	rawlenth = len;
	g_au16Reg = reg ;
	g_au8Buffer = data;

	g_u8EndFlag = 0;
	g_u16DataLen_m = 0;

	/* I2C function to read data from slave */
	I2Cx_Master_HandlerFn = (I2C_FUNC)I2Cx_MasterRx_multi;

//	printf("I2Cx_MasterRx_multi finish\r\n");
	
	I2C_SET_CONTROL_REG(MASTER_I2C, I2C_CTL_STA);

	/* Wait I2C Rx Finish */
	while(g_u8EndFlag == 0);
	g_u8EndFlag = 0;	
}



void I2Cx_Init(void)	//PB1 : SCL , PB0 : SDA
{
	if (MASTER_I2C == I2C0)
	{
    	SYS_ResetModule(I2C0_RST);
	}
	else
	{
    	SYS_ResetModule(I2C1_RST);
	}


    /* Open I2C module and set bus clock */
    I2C_Open(MASTER_I2C, 100000);

    I2C_SetSlaveAddr(MASTER_I2C, 0, EEPROM_SLAVE_ADDR, 0);   /* Slave Address : 1101011b */

    /* Get I2C1 Bus Clock */
    printf("I2C clock %d Hz\n", I2C_GetBusClockFreq(MASTER_I2C));

    I2C_EnableInt(MASTER_I2C);
    NVIC_EnableIRQ(MASTER_I2C_IRQn);
	
}

void EEPROM_TEST(void)
{
	uint8_t value = 0;
	uint16_t reg = 0;	
	uint8_t array[2] = {0};
	
	uint8_t u8SlaveAddr = EEPROM_SLAVE_ADDR >>1;

	#if 1	//clear EEPROM
	printf("clear EEPROM\r\n");	
	value = 0xFF;
	for (reg = 0 ; reg < 0x100 ; reg++ )
	{
		I2Cx_WriteMultiToSlaveIRQ(u8SlaveAddr , reg , &value , 1);		
		CLK_SysTickDelay(3500);
	}

	#endif

	value = 0xF4;
	reg = 0x00;
	I2Cx_WriteMultiToSlaveIRQ(u8SlaveAddr , reg , &value , 1);
	CLK_SysTickDelay(3500);
	printf("WR : 0x%2X : 0x%2X \r\n" ,reg ,value);

	value = 0;
	I2Cx_ReadMultiFromSlaveIRQ(u8SlaveAddr , reg , &value , 1);
	printf("RD : 0x%2X : 0x%2X \r\n" ,reg ,value);
	
	value = 0x12;
	reg = 0x01;	
	I2Cx_WriteMultiToSlaveIRQ(u8SlaveAddr , reg , &value , 1);
	CLK_SysTickDelay(3500);
	printf("WR : 0x%2X : 0x%2X \r\n" ,reg ,value);

	value = 0;
	I2Cx_ReadMultiFromSlaveIRQ(u8SlaveAddr , reg , &value , 1);
	printf("RD : 0x%2X : 0x%2X \r\n" ,reg ,value);
	
	value = 0x34;
	reg = 0x02;		
	I2Cx_WriteMultiToSlaveIRQ(u8SlaveAddr , reg , &value , 1);
	CLK_SysTickDelay(3500);
	printf("WR : 0x%2X : 0x%2X \r\n" ,reg ,value);

	value = 0;
	I2Cx_ReadMultiFromSlaveIRQ(u8SlaveAddr , reg , &value , 1);
	printf("RD : 0x%2X : 0x%2X \r\n" ,reg ,value);
	
	value = 0x56;
	reg = 0x03;	
	I2Cx_WriteMultiToSlaveIRQ(u8SlaveAddr , reg , &value , 1);
	CLK_SysTickDelay(3500);
	printf("WR : 0x%2X : 0x%2X \r\n" ,reg ,value);

	value = 0;
	I2Cx_ReadMultiFromSlaveIRQ(u8SlaveAddr , reg , &value , 1);
	printf("RD : 0x%2X : 0x%2X \r\n" ,reg ,value);	

	array[1] = 0x12;
	array[0] = 0x46;	
	reg = 0x1320;	
	I2Cx_WriteMultiToSlaveIRQ(u8SlaveAddr , reg , array , 2);
	CLK_SysTickDelay(3500);
	printf("WR : 0x%2X : 0x%2X , 0x%2X \r\n" ,reg ,array[0],array[1]);

	value = 0;
	I2Cx_ReadMultiFromSlaveIRQ(u8SlaveAddr , reg , &value , 1);
	printf("RD : 0x%2X : 0x%2X \r\n" ,reg , value);
	value = 0;	
	I2Cx_ReadMultiFromSlaveIRQ(u8SlaveAddr , reg+1 , &value , 1);
	printf("RD : 0x%2X : 0x%2X \r\n" ,reg+1 ,value);

	#if 1	//dump EEPROM
	printf("dump EEPROM\r\n");	
	for (reg = 0 ; reg < 0x100 ; reg++ )
	{
		I2Cx_ReadMultiFromSlaveIRQ(u8SlaveAddr , reg , &value , 1);
		printf("0x%2X," ,value);

		if ((reg+1)%8 ==0)
        {
            printf("\r\n");
        }
	}

	#endif

	
}

void EEPROM_Process(void)
{
	uint8_t u8SlaveAddr = EEPROM_SLAVE_ADDR >>1;
	uint16_t i = 0;
	uint8_t value = 0;
	static uint8_t addr_write = 0;
	static uint8_t addr_read = 0;
	static uint8_t temp = 0;
	const uint8_t data1[16] = 
	{
		0x23 , 0x16 , 0x80 , 0x49 , 0x56 , 0x30 , 0x17 , 0x22 ,
		0x33 , 0x46 , 0x55 , 0x27 , 0x39 , 0x48 , 0x57 , 0x60			
	};
	uint8_t txbuffer[0x100] = {0};
	uint8_t rxbuffer[0x100] = {0};

	if (FLAG_PROJ_EEP_DUMP)
	{
		FLAG_PROJ_EEP_DUMP = 0;
		
		printf("dump EEPROM\r\n");
		for (i = 0 ; i < 0x100 ; i++ )
		{
			I2Cx_ReadMultiFromSlaveIRQ(u8SlaveAddr , i , &value , 1);		
			// CLK_SysTickDelay(3500);	
			printf("0x%2X," ,value);

			if ((i+1)%16 ==0)
	        {
	            printf("\r\n");
	        }
		}
	}

	if (FLAG_PROJ_EEP_WRITE_ADDR)		// fix vaule , to incr address
	{
		FLAG_PROJ_EEP_WRITE_ADDR = 0;
	
		value = 0x01;
		I2Cx_WriteMultiToSlaveIRQ(u8SlaveAddr , addr_write , &value , 1);
		printf("WR : 0x%2X : 0x%2X \r\n" , addr_write++ , value);
	}

	if (FLAG_PROJ_EEP_WRITE_DATA)		// incr vaule , to fix address
	{
		FLAG_PROJ_EEP_WRITE_DATA = 0;
	
		value = temp++;
		addr_write = 0x10;
		I2Cx_WriteMultiToSlaveIRQ(u8SlaveAddr , addr_write , &value , 1);
		printf("WR : 0x%2X : 0x%2X \r\n" , addr_write++ , value);	
	}

	if (FLAG_PROJ_EEP_WRITE_DATA1)
	{
		FLAG_PROJ_EEP_WRITE_DATA1 = 0;

		addr_write = 0x40;
		for ( i = 0 ; i < 16; i++)
		{
			I2Cx_WriteMultiToSlaveIRQ(u8SlaveAddr , addr_write++ , (uint8_t *) &data1[i] , 1);
//			CLK_SysTickDelay(1000);
			printf("WR : 0x%2X : 0x%2X \r\n" , addr_write , data1[i]);	
			
		}		
	}	


	if (FLAG_PROJ_EEP_READ)
	{
		FLAG_PROJ_EEP_READ = 0;

		I2Cx_ReadMultiFromSlaveIRQ(u8SlaveAddr , addr_read , (uint8_t *) &rxbuffer[0] , 0x10);
		printf("addr_read : 0x%2X\r\n" , addr_read);	
		dump_buffer_hex(rxbuffer , 0x10);
		addr_read += 0x10;

	}	


	if (FLAG_PROJ_EEP_WRITE_DATA_ARRAY)
	{
		FLAG_PROJ_EEP_WRITE_DATA_ARRAY = 0;

		for (i = 0; i < 0x10 ; i++)
		{
			txbuffer[i] = (uint8_t) i;
		}

		I2Cx_WriteMultiToSlaveIRQ(u8SlaveAddr , addr_write , (uint8_t *) &txbuffer[0] , 0x10);
		printf("WRITE_DATA_ARRAY done\r\n");
		addr_write += 0x10;
	}	

	if (FLAG_PROJ_EEP_USE_API)
	{
		FLAG_PROJ_EEP_USE_API = 0;

		I2C_DisableInt(MASTER_I2C);
		NVIC_DisableIRQ(MASTER_I2C_IRQn);


		for (i = 0; i < 0x10 ; i++)
		{
			txbuffer[i] = (uint8_t) i + 3;
		}

		while(I2C_WriteMultiBytesTwoRegs(MASTER_I2C, EEPROM_SLAVE_ADDR >>1, 0x0000, txbuffer, 0x10) < 0x10);
		printf("Multi bytes Write access 1\r\n");

		while(I2C_ReadMultiBytesTwoRegs(MASTER_I2C, EEPROM_SLAVE_ADDR >>1 , 0x0000, rxbuffer, 0x10) < 0x10);
		printf("Multi bytes Read access 1\r\n");
		dump_buffer_hex(rxbuffer , 0x10);


		for (i = 0; i < 0x10 ; i++)
		{
			txbuffer[i] = (uint8_t) i + 2;
		}

		while(I2C_WriteMultiBytesTwoRegs(MASTER_I2C, EEPROM_SLAVE_ADDR >>1, 0x0010, txbuffer, 0x10) < 0x10);
		printf("Multi bytes Write access 2\r\n");

		while(I2C_ReadMultiBytesTwoRegs(MASTER_I2C, EEPROM_SLAVE_ADDR >>1 , 0x0010, rxbuffer, 0x10) < 0x10);
		printf("Multi bytes Read access 2\r\n");
		dump_buffer_hex(rxbuffer , 0x10);

		for (i = 0; i < 0x10 ; i++)
		{
			txbuffer[i] = (uint8_t) i + 1;
		}

		while(I2C_WriteMultiBytesTwoRegs(MASTER_I2C, EEPROM_SLAVE_ADDR >>1, 0x0020, txbuffer, 0x10) < 0x10);
		printf("Multi bytes Write access 3\r\n");

		while(I2C_ReadMultiBytesTwoRegs(MASTER_I2C, EEPROM_SLAVE_ADDR >>1 , 0x0020, rxbuffer, 0x10) < 0x10);
		printf("Multi bytes Read access 3\r\n");
		dump_buffer_hex(rxbuffer , 0x10);


		I2C_EnableInt(MASTER_I2C);
		NVIC_EnableIRQ(MASTER_I2C_IRQn);
	}	


	if (FLAG_PROJ_EEP_READ_ALL)
	{
		FLAG_PROJ_EEP_READ_ALL = 0;
	
		I2Cx_ReadMultiFromSlaveIRQ(u8SlaveAddr , addr_read , (uint8_t *) &rxbuffer[0] , 0x100);
		printf("addr_read : 0x%2X\r\n" , addr_read);	
		dump_buffer_hex(rxbuffer , 0x100);
	}


	if (FLAG_PROJ_EEP_ERASE)
	{
		FLAG_PROJ_EEP_ERASE = 0;
	
		printf("clear EEPROM\r\n");
		value = 0xFF;
		for (i = 0 ; i < 0x100 ; i++ )
		{
			I2Cx_WriteMultiToSlaveIRQ(u8SlaveAddr , i , &value , 1);		
			CLK_SysTickDelay(3500);	
		}
		printf("EEP_ERASE done\r\n");
	}
	
}


void TMR1_IRQHandler(void)
{
	
    if(TIMER_GetIntFlag(TIMER1) == 1)
    {
        TIMER_ClearIntFlag(TIMER1);
		tick_counter();

		if ((get_tick() % 1000) == 0)
		{
            FLAG_PROJ_TIMER_PERIOD_1000MS = 1;//set_flag(flag_timer_period_1000ms ,ENABLE);
		}

		if ((get_tick() % 50) == 0)
		{

		}	
    }
}

void TIMER1_Init(void)
{
    TIMER_Open(TIMER1, TIMER_PERIODIC_MODE, 1000);
    TIMER_EnableInt(TIMER1);
    NVIC_EnableIRQ(TMR1_IRQn);	
    TIMER_Start(TIMER1);
}

void loop(void)
{
	// static uint32_t LOG1 = 0;
	// static uint32_t LOG2 = 0;

    if ((get_systick() % 1000) == 0)
    {
        // printf("%s(systick) : %4d\r\n",__FUNCTION__,LOG2++);    
    }

    if (FLAG_PROJ_TIMER_PERIOD_1000MS)//(is_flag_set(flag_timer_period_1000ms))
    {
        FLAG_PROJ_TIMER_PERIOD_1000MS = 0;//set_flag(flag_timer_period_1000ms ,DISABLE);

        // printf("%s(timer) : %4d\r\n",__FUNCTION__,LOG1++);
        PB14 ^= 1;        
    }
    
    EEPROM_Process();
}

void UARTx_Process(void)
{
	uint8_t res = 0;
	res = UART_READ(UART0);

	if (res > 0x7F)
	{
		printf("invalid command\r\n");
	}
	else
	{
		printf("press : %c\r\n" , res);
		switch(res)
		{
			case '1':
				FLAG_PROJ_EEP_DUMP = 1;
				break;

			case '2': 
				FLAG_PROJ_EEP_WRITE_ADDR = 1;
			
				break;

			case '3': 
				FLAG_PROJ_EEP_WRITE_DATA = 1;			
		
				break;			

			case '4': 
				FLAG_PROJ_EEP_WRITE_DATA1 = 1;		
		
				break;	

			case '5': 
				FLAG_PROJ_EEP_READ = 1;	
		
				break;	

			case '6': 
				FLAG_PROJ_EEP_WRITE_DATA_ARRAY = 1;	
		
				break;	

			case '7': 
				FLAG_PROJ_EEP_USE_API = 1;	
		
				break;	

			case '8': 
				FLAG_PROJ_EEP_READ_ALL = 1;	
		
				break;	

			case '0' : 
				FLAG_PROJ_EEP_ERASE = 1;

				break;


			case '?' : 
				printf("1:FLAG_PROJ_EEP_DUMP\r\n");
				printf("2:FLAG_PROJ_EEP_WRITE_ADDR\r\n");
				printf("3:FLAG_PROJ_EEP_WRITE_DATA\r\n");
				printf("4:FLAG_PROJ_EEP_WRITE_DATA1\r\n");
				printf("5:FLAG_PROJ_EEP_READ\r\n");
				printf("6:FLAG_PROJ_EEP_WRITE_DATA_ARRAY\r\n");
				printf("7:FLAG_PROJ_EEP_USE_API\r\n");
				printf("8:FLAG_PROJ_EEP_READ_ALL\r\n");
				printf("0:FLAG_PROJ_EEP_ERASE\r\n");
				break;

			case 'X':
			case 'x':
			case 'Z':
			case 'z':
                SYS_UnlockReg();
				// NVIC_SystemReset();	// Reset I/O and peripherals , only check BS(FMC_ISPCTL[1])
                // SYS_ResetCPU();     // Not reset I/O and peripherals
                SYS_ResetChip();    // Reset I/O and peripherals ,  BS(FMC_ISPCTL[1]) reload from CONFIG setting (CBS)	
				break;
		}
	}
}

void UART0_IRQHandler(void)
{
    if(UART_GET_INT_FLAG(UART0, UART_INTSTS_RDAINT_Msk | UART_INTSTS_RXTOINT_Msk))     /* UART receive data available flag */
    {
        while(UART_GET_RX_EMPTY(UART0) == 0)
        {
			UARTx_Process();
        }
    }

    if(UART0->FIFOSTS & (UART_FIFOSTS_BIF_Msk | UART_FIFOSTS_FEF_Msk | UART_FIFOSTS_PEF_Msk | UART_FIFOSTS_RXOVIF_Msk))
    {
        UART_ClearIntFlag(UART0, (UART_INTSTS_RLSINT_Msk| UART_INTSTS_BUFERRINT_Msk));
    }	
}

void UART0_Init(void)
{
    SYS_ResetModule(UART0_RST);

    /* Configure UART0 and set UART0 baud rate */
    UART_Open(UART0, 115200);
    UART_EnableInt(UART0, UART_INTEN_RDAIEN_Msk | UART_INTEN_RXTOIEN_Msk);
    NVIC_EnableIRQ(UART0_IRQn);
	
	#if (_debug_log_UART_ == 1)	//debug
	printf("\r\nCLK_GetCPUFreq : %8d\r\n",CLK_GetCPUFreq());
	printf("CLK_GetHCLKFreq : %8d\r\n",CLK_GetHCLKFreq());
	printf("CLK_GetHXTFreq : %8d\r\n",CLK_GetHXTFreq());
	printf("CLK_GetLXTFreq : %8d\r\n",CLK_GetLXTFreq());	
	printf("CLK_GetPCLK0Freq : %8d\r\n",CLK_GetPCLK0Freq());
	printf("CLK_GetPCLK1Freq : %8d\r\n",CLK_GetPCLK1Freq());	
	#endif	

    #if 0
    printf("FLAG_PROJ_TIMER_PERIOD_1000MS : 0x%2X\r\n",FLAG_PROJ_TIMER_PERIOD_1000MS);
    printf("FLAG_PROJ_REVERSE1 : 0x%2X\r\n",FLAG_PROJ_REVERSE1);
    printf("FLAG_PROJ_REVERSE2 : 0x%2X\r\n",FLAG_PROJ_REVERSE2);
    printf("FLAG_PROJ_REVERSE3 : 0x%2X\r\n",FLAG_PROJ_REVERSE3);
    printf("FLAG_PROJ_REVERSE4 : 0x%2X\r\n",FLAG_PROJ_REVERSE4);
    printf("FLAG_PROJ_REVERSE5 : 0x%2X\r\n",FLAG_PROJ_REVERSE5);
    printf("FLAG_PROJ_REVERSE6 : 0x%2X\r\n",FLAG_PROJ_REVERSE6);
    printf("FLAG_PROJ_REVERSE7 : 0x%2X\r\n",FLAG_PROJ_REVERSE7);
    #endif

}

void GPIO_Init (void)
{
//    SYS->GPB_MFPH = (SYS->GPB_MFPH & ~(SYS_GPB_MFPH_PB14MFP_Msk)) | (SYS_GPB_MFPH_PB14MFP_GPIO);
		
	GPIO_SetMode(PB, BIT14, GPIO_MODE_OUTPUT);

//    GPIO_SetMode(PB, BIT15, GPIO_MODE_OUTPUT);	
}


void SYS_Init(void)
{
    /* Unlock protected registers */
    SYS_UnlockReg();

    CLK_EnableXtalRC(CLK_PWRCTL_HIRCEN_Msk);
    CLK_WaitClockReady(CLK_STATUS_HIRCSTB_Msk);

//    CLK_EnableXtalRC(CLK_PWRCTL_HXTEN_Msk);
//    CLK_WaitClockReady(CLK_STATUS_HXTSTB_Msk);

//    CLK_EnableXtalRC(CLK_PWRCTL_LIRCEN_Msk);
//    CLK_WaitClockReady(CLK_STATUS_LIRCSTB_Msk);	

//    CLK_EnableXtalRC(CLK_PWRCTL_LXTEN_Msk);
//    CLK_WaitClockReady(CLK_STATUS_LXTSTB_Msk);	

    /* Switch HCLK clock source to Internal RC and HCLK source divide 1 */
    CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_HIRC, CLK_CLKDIV0_HCLK(1));

    /* Enable UART clock */
    CLK_EnableModuleClock(UART0_MODULE);

    /* Select UART clock source from HIRC */
    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART0SEL_HIRC, CLK_CLKDIV0_UART0(1));

    CLK_EnableModuleClock(TMR1_MODULE);
  	CLK_SetModuleClock(TMR1_MODULE, CLK_CLKSEL1_TMR1SEL_HIRC, 0);

    /* Set PB multi-function pins for UART0 RXD=PB.12 and TXD=PB.13 */
    SYS->GPB_MFPH = (SYS->GPB_MFPH & ~(SYS_GPB_MFPH_PB12MFP_Msk | SYS_GPB_MFPH_PB13MFP_Msk)) |
                    (SYS_GPB_MFPH_PB12MFP_UART0_RXD | SYS_GPB_MFPH_PB13MFP_UART0_TXD);

	#if 1	// I2C0
    CLK_EnableModuleClock(I2C0_MODULE);

    SYS->GPC_MFPL = (SYS->GPC_MFPL & ~(SYS_GPC_MFPL_PC0MFP_Msk | SYS_GPC_MFPL_PC1MFP_Msk )) |
                    (SYS_GPC_MFPL_PC0MFP_I2C0_SDA | SYS_GPC_MFPL_PC1MFP_I2C0_SCL);
    
	#else	// I2C1
    CLK_EnableModuleClock(I2C1_MODULE);

    SYS->GPB_MFPL = (SYS->GPB_MFPL & ~(SYS_GPB_MFPL_PB0MFP_Msk | SYS_GPB_MFPL_PB1MFP_Msk )) |
                    (SYS_GPB_MFPL_PB0MFP_I2C1_SDA | SYS_GPB_MFPL_PB1MFP_I2C1_SCL);    
    
	#endif


    /* Update System Core Clock */
    /* User can use SystemCoreClockUpdate() to calculate SystemCoreClock. */
    SystemCoreClockUpdate();

    /* Lock protected registers */
    SYS_LockReg();
}


/*
 * This is a template project for M251 series MCU. Users could based on this project to create their
 * own application without worry about the IAR/Keil project settings.
 *
 * This template application uses external crystal as HCLK source and configures UART0 to print out
 * "Hello World", users may need to do extra system configuration based on their system design.
 */

int main()
{
    SYS_Init();

	GPIO_Init();
	UART0_Init();
	TIMER1_Init();

    SysTick_enable(1000);
    #if defined (ENABLE_TICK_EVENT)
    TickSetTickEvent(1000, TickCallback_processA);  // 1000 ms
    TickSetTickEvent(5000, TickCallback_processB);  // 5000 ms
    #endif

	I2Cx_Init();

    /* Got no where to go, just loop forever */
    while(1)
    {
        loop();

    }
}

/*** (C) COPYRIGHT 2017 Nuvoton Technology Corp. ***/
