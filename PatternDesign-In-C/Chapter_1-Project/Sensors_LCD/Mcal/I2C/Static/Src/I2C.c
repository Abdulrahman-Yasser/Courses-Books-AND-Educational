/**********************************************************************************************************************
 *  FILE DESCRIPTION
  *  -----------------------------------------------------------------------------------------------------------------*/
/**        \file  I2C.c
  *        \brief  I2C Driver
  *
  *      \details  This file contains the static implementation of the driver
  *                  it contains all the function implementation logic
  *
  *
  *********************************************************************************************************************/

/**********************************************************************************************************************
 *  INCLUDES
 *********************************************************************************************************************/

#include <Mcal/I2C/Static/inc/I2C.h>
#include <Mcal/I2C/Dynamic/inc/I2C_Cfg.h>
#include "../../../../Common/Std_Types.h"


#include "../../../../Common/Mcu_Hw.h"
#include "../../../../Common/CPU_resources.h"
#include "../../../../Common/DataStructure/Queue/static/inc/Queue.h"

/**********************************************************************************************************************
 *  Variables Definitions
 *********************************************************************************************************************/
#define I2C_RepeatedStart_ON          1
#define I2C_RepeatedStart_OFF      0

/**********************************************************************************************************************
 *  LOCAL VARIABLES
 *********************************************************************************************************************/
static Queue_uint8_DYNAMIC_Type* I2C_Queue_Buffer[4*2];

#define I2C_Transmit_Buffer_Mask   1
#define I2C_Receive_Buffer_Mask    0


static uint8 I2C_MasterModulesUsed = 0;
static uint8 I2C_SlaveModulesUsed = 0;

/**********************************************************************************************************************
 *  LOCAL Functions Definition
 *********************************************************************************************************************/


/**********************************************************************************************************************
 *  LOCAL Functions Definition
 *********************************************************************************************************************/

static uint32 get_address(I2C_ChannelType I2C_n);

static void MasterMultibleTransmitLoop(uint32 base,uint8 n, Queue_uint8_DYNAMIC_Type* Data);
static void MasterMultibleReceiveLoop(uint32 base,uint8 n, Queue_uint8_DYNAMIC_Type* Data);


uint16 I2C_MasterSingleTransmit(I2C_ChannelType my_I2C, uint8 address, I2C_RepeatedStartType RepeatedStart);
uint16 I2C_MasterTransmitMultibleBytes(I2C_ChannelType my_I2C, uint8 address, uint8 n, I2C_RepeatedStartType RepeatedStart);
uint16 I2C_MasterTransmitAllQueueBytes(I2C_ChannelType my_I2C, uint8 address, I2C_RepeatedStartType RepeatedStart);

uint16 I2C_MasterSingleReceive(I2C_ChannelType my_I2C, uint8 address, I2C_RepeatedStartType RepeatedStart);
uint16 I2C_MasterReceiveMultibleBytes(I2C_ChannelType my_I2C, uint8 address, uint8 n, I2C_RepeatedStartType RepeatedStart);
uint16 I2C_MasterReceiveAllQueueBytes(I2C_ChannelType my_I2C, uint8 address, I2C_RepeatedStartType RepeatedStart);


void I2C_SlaveTransmit_one_Byte(I2C_ChannelType my_I2C);
void I2C_SlaveReceive_one_Byte(I2C_ChannelType my_I2C);



void __attribute__((weak)) App_I2C0_Tx_CallBack(void);
void __attribute__((weak)) App_I2C0_Rx_CallBack(void);
void __attribute__((weak)) App_I2C1_Tx_CallBack(void);
void __attribute__((weak)) App_I2C1_Rx_CallBack(void);
void __attribute__((weak)) App_I2C2_Tx_CallBack(void);
void __attribute__((weak)) App_I2C2_Rx_CallBack(void);
void __attribute__((weak)) App_I2C3_Tx_CallBack(void);
void __attribute__((weak)) App_I2C3_Rx_CallBack(void);

/**********************************************************************************************************************
 *  LOCAL Functions Implementation
 *********************************************************************************************************************/


/******************************************************************************
* \Syntax          : static uint32 get_address(I2C_ChannelType I2C_n)
* \Description     : The Function Returns the I2C module Channel's Base Address
*
* \Parameters (in) : I2C_n          The I2C Channel Number
* \Parameters (out): None
* \Return value:   : uint32         The I2C Channel Module base Address
*******************************************************************************/
static uint32 get_address(I2C_ChannelType I2C_n){
    switch(I2C_n){
    case I2C_NUM_0:
        return (uint32)I2C0_BASE_ADDERSS;
    case I2C_NUM_1:
        return (uint32)I2C1_BASE_ADDERSS;
    case I2C_NUM_2:
        return (uint32)I2C2_BASE_ADDERSS;
    case I2C_NUM_3:
        return (uint32)I2C3_BASE_ADDERSS;
    default:
        return 0;
    }
}


/******************************************************************************
* \Syntax          : static void MasterMultibleTransmitLoop(uint32 base,uint8 n, Queue_uint8_DYNAMIC_Type* Data)
* \Description     : The Function Transmits n Bytes to the Slave
* \Parameters (in) : base          The I2C Channel base Address
*                    n             The number of bytes i want to send
*                    Data          The Data buffer of the I2C channel
* \Parameters (out): None
* \Return value:   : None
*******************************************************************************/
static void MasterMultibleTransmitLoop(uint32 base,uint8 n, Queue_uint8_DYNAMIC_Type* Data){
    uint32 RegisterCheck;
    uint8 i;
    for(i = 0; i < n; i++){
        /* 0_001 RUN Bit is HIGH, STOP and START are LOW
         * That would generate a Transmitting multiple bytes signal */
        REG_CLEAR_SPECIFIC_BIT_PTR((base + I2C_MCS_REG_OFFSET), 0X16);
        REG_WRITE_BIT_PTR(base + I2C_MCS_REG_OFFSET, 0);
        /*BUSY Checking "if the I2C module is BUSY (Finished Transmission or not )" */
        do{
            RegisterCheck = (*(volatile uint32 *)(base + I2C_MCS_REG_OFFSET));
        }while((RegisterCheck & (1 << I2C_MCS_BUSY_MASK)));
        /* Error Checking */
        RegisterCheck = (*(volatile uint32 *)(base + I2C_MCS_REG_OFFSET));
        if(RegisterCheck & (1 << I2C_MCS_ERROR_MASK)){
            if(RegisterCheck & (1 << I2C_MCS_ARBLST_MASK)){
            }else{
                /* That is going to be a non-detected error */
                /* 0_100 */
                REG_CLEAR_SPECIFIC_BIT_PTR((base + I2C_MCS_REG_OFFSET), 0X13);
                REG_WRITE_BIT_PTR(base + I2C_MCS_REG_OFFSET, 2);
            }
        }else{
            /* Write the Data*/
            /* Write the Data, Freeze if the Queue is Empty */
            while(Data->isEmpty(Data));
            REG_WRITE_32_BIT_PTR((base + I2C_MDR_REG_OFFSET), Data->remove(Data));
        }
    }
}


/******************************************************************************
* \Syntax          : static void MasterMultibleReceiveLoop(uint32 base,uint8 n, Queue_uint8_DYNAMIC_Type* Data)
* \Description     : The Function Receives n Bytes of the Slave
* \Parameters (in) : base          The I2C Channel base Address
*                    n             The number of bytes i want to receive
*                    Data          The Data buffer of the I2C channel
* \Parameters (out): None
* \Return value:   : None
*******************************************************************************/
static void MasterMultibleReceiveLoop(uint32 base,uint8 n, Queue_uint8_DYNAMIC_Type* Data){
    uint32 RegisterCheck;
    uint8 i;
    for(i = 0; i < n-1; i++){
        /* __01001 RUN  and ACK Bits are HIGH
         *         STOP and START Bits are LOW
         * That would generate a Transmitting multiple bytes signal */
        REG_CLEAR_SPECIFIC_BIT_PTR(base + I2C_MCS_REG_OFFSET, 0x16);
        REG_WRITE_32_BIT_PTR(base + I2C_MCS_REG_OFFSET, 0x9);
        /*BUSY Checking "if the I2C module is BUSY (Finished Transmission or not )" */
        do{
            RegisterCheck = (*(volatile uint32 *)(base + I2C_MCS_REG_OFFSET));
        }while((RegisterCheck & (1 << I2C_MCS_BUSY_MASK)));
        /* Error Checking */
        RegisterCheck = (*(volatile uint32 *)(base + I2C_MCS_REG_OFFSET));
        if(RegisterCheck & (1 << I2C_MCS_ERROR_MASK)){
            if(RegisterCheck & (1 << I2C_MCS_ARBLST_MASK)){
            }else{
                /* That is going to be a non-detected error */
                /* 0_100 */
                REG_CLEAR_SPECIFIC_BIT_PTR(base + I2C_MCS_REG_OFFSET, 0x13);
                REG_WRITE_32_BIT_PTR(base + I2C_MCS_REG_OFFSET, 0x4);
            }
        }else{
            /* Read the Data, Freeze if the Queue is Full */
            RegisterCheck = (uint32)(*(volatile uint32 *)(base + I2C_MDR_REG_OFFSET));
            while(Data->isFull(Data));
            Data->insert(Data, RegisterCheck);
        }
    }
}


/******************************************************************************
* \Syntax          : uint16 I2C_MasterSingleTransmit(I2C_ChannelType my_I2C, uint8 address, I2C_RepeatedStartType RepeatedStart)
* \Description     : The Function Transmits a single bytes to the slave
* \Parameters (in) : my_I2C          A struct that contains ( I2C Channel number, Slave Address )
*                    RepeatedStart   Flag that determine whether the transmission is a repeatedStart or not
* \Parameters (out): None
* \Return value:   : uint16     I2C_RETURN_ERROR
*                               I2C_RETURN_FINE
*******************************************************************************/
uint16 I2C_MasterSingleTransmit(I2C_ChannelType my_I2C, uint8 address, I2C_RepeatedStartType RepeatedStart){
    uint32 base, RegisterCheck;
    // Check if the I2C is Configured as Master
    if(! (I2C_MasterModulesUsed & (1 << my_I2C)) ){
        /* error, the I2C module is not configured as a Master */
        return I2C_RETURN_ERROR;
    }
    base = get_address(my_I2C);

    /* Write the Slave's Address, Bit 0 'R/S' is LOW for transmit */
    REG_WRITE_32_BIT_PTR((base + I2C_MSA_REG_OFFSET), address << 1);
    REG_CLEAR_BIT_PTR(base + I2C_MSA_REG_OFFSET, 0);
    /* Write the Data, Freeze if the Queue is Empty */
    while(I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]->isEmpty(I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]));
    REG_WRITE_32_BIT_PTR((base + I2C_MDR_REG_OFFSET),I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]->remove(I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]));
    /* Check if the I2C BUS is free,
     * If this is a repeated start request do not check the bus busy because it's used by the I2C module */
    if(RepeatedStart == I2C_RepeatedStart_OFF){
        do{
            RegisterCheck = (*(volatile uint32 *)(base + I2C_MCS_REG_OFFSET));
        }while(RegisterCheck & (1 << I2C_MCS_BUSBSY_MASK));
    }
    /* 00111
     * STOP, RUN, START Bits are HIGH
     * ACK, HS are low
    * That would generate a MasterSingleTransmit signal */
    REG_CLEAR_SPECIFIC_BIT_PTR((base + I2C_MCS_REG_OFFSET), 0X18);
    REG_WRITE_32_BIT_PTR((base + I2C_MCS_REG_OFFSET), 0X3);
    /* Check if the I2C module is BUSY (Finished Transmission or not )*/
    do{
        RegisterCheck = (*(volatile uint32 *)(base + I2C_MCS_REG_OFFSET));
    }while((RegisterCheck & (1 << I2C_MCS_BUSY_MASK)));
    /* Check if Error was occurred or not*/
    RegisterCheck = (*(volatile uint32 *)(base + I2C_MCS_REG_OFFSET));
    if(RegisterCheck & (1 << I2C_MCS_ERROR_MASK)){
        return I2C_RETURN_ERROR;
    }else{
        return I2C_RETURN_FINE;
    }
}


/******************************************************************************
* \Syntax          : uint16 I2C_MasterTransmitMultibleBytes(I2C_ChannelType my_I2C, uint8 address, uint8 n, I2C_RepeatedStartType RepeatedStart )
* \Description     : The Function Transmits Multiple bytes to the slave
* \Parameters (in) : my_I2C          A struct that contains ( I2C Channel number, Slave Address )
*                    n               Number of the bytes i need to transmit
*                    RepeatedStart   Flag that determine whether the transmission is a repeatedStart or not
* \Parameters (out): None
* \Return value:   : uint16     I2C_RETURN_ERROR
*                               I2C_RETURN_FINE
*******************************************************************************/
uint16 I2C_MasterTransmitMultibleBytes(I2C_ChannelType my_I2C, uint8 address, uint8 n, I2C_RepeatedStartType RepeatedStart ){
    uint32 base, RegisterCheck;
    // Check if the I2C is Configured as Master
    if(! (I2C_MasterModulesUsed & (1 << my_I2C))){
        /* error, the I2C module is not configured as a Master */
        return I2C_RETURN_ERROR;
    }
    base = get_address(my_I2C);

    /* Write the Slave's Address, Bit 0 'R/S' is LOW for transmit */
    REG_WRITE_32_BIT_PTR((base + I2C_MSA_REG_OFFSET), address << 1);
    REG_CLEAR_BIT_PTR(base + I2C_MSA_REG_OFFSET, 0);
    /* Write the Data, Freeze if the Queue is Empty */
    while(I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]->isEmpty(I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]));
    REG_WRITE_32_BIT_PTR((base + I2C_MDR_REG_OFFSET), I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]->remove(I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]));
    /* BUSBUSY Checking "Check if the I2C BUS is free or used by another MASTER" */
    if(RepeatedStart == I2C_RepeatedStart_OFF){
        do{
            RegisterCheck = (*(volatile uint32 *)(base + I2C_MCS_REG_OFFSET));
        }while(RegisterCheck & (1 << I2C_MCS_BUSBSY_MASK));
    }
    /* 0_011
     * RUN, START Bits are HIGH
     * STOP, ACK, HS are low *
     * That would generate a Transmitting multiple bytes signal */
    REG_CLEAR_SPECIFIC_BIT_PTR((base + I2C_MCS_REG_OFFSET), 0X14);
    REG_WRITE_32_BIT_PTR((base + I2C_MCS_REG_OFFSET), 0X3);
    /* BUSY Checking "if the I2C module is BUSY (Finished Transmission or not )" */
    do{
        RegisterCheck = (*(volatile uint32 *)(base + I2C_MCS_REG_OFFSET));
    }while(RegisterCheck & (1 << I2C_MCS_BUSY_MASK));
    /* Error Checking */
    RegisterCheck = (*(volatile uint32 *)(base + I2C_MCS_REG_OFFSET));
    if(RegisterCheck & (1 << I2C_MCS_ERROR_MASK)){
        /* Error Handling, it will return after that */
        if(RegisterCheck & (1 << I2C_MCS_ARBLST_MASK)){
        }else{
            /* 0_100
             * STOP Bits are HIGH
             * RUN, START, HS are low *
             * That would generate a STOP signal */
            REG_CLEAR_SPECIFIC_BIT_PTR((base + I2C_MCS_REG_OFFSET), 0X18);
            REG_WRITE_BIT_PTR(base + I2C_MCS_REG_OFFSET, 2);
        }
        return I2C_RETURN_ERROR;
    }else{
        /* Write the Data */
        /* Write the Data, Freeze if the Queue is Empty */
        while(I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]->isEmpty(I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]));
        REG_WRITE_32_BIT_PTR((base + I2C_MDR_REG_OFFSET), I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]->remove(I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]));
    }
    /* Transmit n Bytes */
    MasterMultibleTransmitLoop(base, n, I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]);
    /* 0_101
     * RUN, STOP Bits are HIGH
     * START, HS are low *
     * That would generate a Last Transmission signal */
    REG_CLEAR_BIT_PTR(base + I2C_MCS_REG_OFFSET, 4);
    REG_CLEAR_BIT_PTR(base + I2C_MCS_REG_OFFSET, 1);
    REG_WRITE_32_BIT_PTR((base + I2C_MCS_REG_OFFSET), 0X05);
    /*BUSY Checking "if the I2C module is BUSY (Finished Transmission or not )" */
    do{
        RegisterCheck = (*(volatile uint32 *)(base + I2C_MCS_REG_OFFSET));
    }while((RegisterCheck & (1 << I2C_MCS_BUSY_MASK)));
    /* Error Checking */
    RegisterCheck = (*(volatile uint32 *)(base + I2C_MCS_REG_OFFSET));
    if(RegisterCheck & (1 << I2C_MCS_ERROR_MASK)){
        return I2C_RETURN_ERROR;
    }
    return I2C_RETURN_FINE;
}

/******************************************************************************
* \Syntax          : uint16 I2C_MasterTransmitAllQueueBytes(I2C_ChannelType my_I2C, uint8 address, I2C_RepeatedStartType RepeatedStart)
* \Description     : The Function Transmits All the bytes in the I2C module Queue
* \Parameters (in) : my_I2C          A struct that contains ( I2C Channel number, Slave Address )
*                    RepeatedStart   Flag that determine whether the transmission is a repeatedStart or not
* \Parameters (out): None
* \Return value:   : uint16     I2C_RETURN_ERROR
*                               I2C_RETURN_FINE
*******************************************************************************/
uint16 I2C_MasterTransmitAllQueueBytes(I2C_ChannelType my_I2C, uint8 address, I2C_RepeatedStartType RepeatedStart){
    uint8 byteCnt;
    byteCnt = I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]->getSize(I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]);
    if(byteCnt == 0){
        /* No data in the Queue */
        return I2C_RETURN_ERROR ;
    }else if(byteCnt == 1){
        I2C_MasterSingleTransmit(my_I2C, address, RepeatedStart);
    }else{
        I2C_MasterTransmitMultibleBytes(my_I2C, address, byteCnt , RepeatedStart);
    }
    return I2C_RETURN_FINE ;
}


/******************************************************************************
* \Syntax          : uint16 I2C_MasterSingleReceive(I2C_ChannelType my_I2C, uint8 address, I2C_RepeatedStartType RepeatedStart)
* \Description     : The Function Receives a single byte from the Slave
* \Parameters (in) : my_I2C          A struct that contains ( I2C Channel number, Slave Address )
*                    RepeatedStart   Flag that determine whether the transmission is a repeatedStart or not
* \Parameters (out): None
* \Return value:   : uint16     I2C_RETURN_ERROR
*                               I2C_RETURN_FINE
*******************************************************************************/
uint16 I2C_MasterSingleReceive(I2C_ChannelType my_I2C, uint8 address, I2C_RepeatedStartType RepeatedStart){
    uint32 base, RegisterCheck;
    // Check if the I2C is Configured as Master
    if(! (I2C_MasterModulesUsed & (1 << my_I2C))){
        /* error, the I2C module is not configured as a Master */
        return I2C_RETURN_ERROR;
    }
    base = get_address(my_I2C);

    /* Write the Slave's Address, Bit 0 'R/S' is HIGH for receive */
    REG_WRITE_32_BIT_PTR((base + I2C_MSA_REG_OFFSET), address << 1);
    REG_WRITE_BIT_PTR(base + I2C_MSA_REG_OFFSET, 0);
    /* Check if the I2C BUS is Busy or free */
    if(RepeatedStart == I2C_RepeatedStart_OFF){
        do{
            RegisterCheck = (*(volatile uint32 *)(base + I2C_MCS_REG_OFFSET));
        }while(RegisterCheck & (1 << I2C_MCS_BUSBSY_MASK));
    }
    /* 00111
     * STOP, RUN, START Bits are HIGH
     * ACK, HS are low
    * That would generate a MasterSingleReceive signal */
    REG_CLEAR_SPECIFIC_BIT_PTR((base + I2C_MCS_REG_OFFSET), 0X18);
    REG_WRITE_32_BIT_PTR((base + I2C_MCS_REG_OFFSET), 0X7);
    /* Check if the I2C module is BUSY (Finished Receiving or not ) */
    do{
        RegisterCheck = (*(volatile uint32 *)(base + I2C_MCS_REG_OFFSET));
    }while((RegisterCheck & (1 << I2C_MCS_BUSY_MASK)));
    /* Check if Error was occurred or not */
    RegisterCheck = (*(volatile uint32 *)(base + I2C_MCS_REG_OFFSET));
    if(RegisterCheck & (1 << I2C_MCS_ERROR_MASK)){
        return I2C_RETURN_ERROR;
    }else{
        /* Read the Data, Freeze if the Queue is Full */
        RegisterCheck = (*(volatile uint32 *)(base + I2C_MCS_REG_OFFSET));
        while(I2C_Queue_Buffer[(2*my_I2C)+I2C_Receive_Buffer_Mask]->isFull(I2C_Queue_Buffer[(2*my_I2C)+I2C_Receive_Buffer_Mask]));
        I2C_Queue_Buffer[(2*my_I2C)+I2C_Receive_Buffer_Mask]->insert(I2C_Queue_Buffer[(2*my_I2C)+I2C_Receive_Buffer_Mask], RegisterCheck);
        return I2C_RETURN_FINE;
    }
}


/******************************************************************************
* \Syntax          : uint16 I2C_MasterReceiveMultibleBytes(I2C_ChannelType my_I2C, uint8 address, uint8 n, I2C_RepeatedStartType RepeatedStart)
* \Description     : The Function Receives Multiple Bytes from the Slave
* \Parameters (in) : my_I2C          A struct that contains ( I2C Channel number, Slave Address )
*                    n               the number of data i need to receive
*                    RepeatedStart   Flag that determine whether the transmission is a repeatedStart or not
* \Parameters (out): None
* \Return value:   : uint16     I2C_RETURN_ERROR
*                               I2C_RETURN_FINE
*******************************************************************************/
uint16 I2C_MasterReceiveMultibleBytes(I2C_ChannelType my_I2C, uint8 address, uint8 n, I2C_RepeatedStartType RepeatedStart){
    uint32 base, RegisterCheck;
    // Check if the I2C is Configured as Master
    if(! (I2C_MasterModulesUsed & (1 << my_I2C))){
        /* error, the I2C module is not configured as a Master */
        return I2C_RETURN_ERROR;
    }
    base = get_address(my_I2C);

    /* Write the Slave's Address, Bit 0 'R/S' is HIGH for receive */
    REG_WRITE_32_BIT_PTR((base + I2C_MSA_REG_OFFSET), address << 1);
    REG_WRITE_BIT_PTR(base + I2C_MSA_REG_OFFSET, 0);
    /* BUSBUSY Checking "Check if the I2C BUS is free or used by another MASTER" */
    if(RepeatedStart == I2C_RepeatedStart_OFF){
        do{
            RegisterCheck = (*(volatile uint32 *)(base + I2C_MCS_REG_OFFSET));
        }while(RegisterCheck & (1 << I2C_MCS_BUSBSY_MASK));
    }
    /* 01011
     * RUN, START, ACK Bits are HIGH
     * STOP is low *
     * That would generate a Receive multiple bytes signal */
    REG_CLEAR_SPECIFIC_BIT_PTR(base + I2C_MCS_REG_OFFSET, 0x14);
    REG_WRITE_32_BIT_PTR(base + I2C_MCS_REG_OFFSET, 0x0B);
    /* BUSY Checking "if the I2C module is BUSY (Finished Transmission or not )" */
    do{
        RegisterCheck = (*(volatile uint32 *)(base + I2C_MCS_REG_OFFSET));
    }while((RegisterCheck & (1 << I2C_MCS_BUSY_MASK)));
    /* Error Checking */
    RegisterCheck = (*(volatile uint32 *)(base + I2C_MCS_REG_OFFSET));
    if(RegisterCheck & (1 << I2C_MCS_ERROR_MASK)){
        /* Error Handling, it will return after that */
        if(RegisterCheck & (1 << I2C_MCS_ARBLST_MASK)){
        }else{
            /* 0_100
             * STOP is HIGH
             * RUN, START are low
             * That would generate a STOP Signal */
            REG_CLEAR_SPECIFIC_BIT_PTR(base + I2C_MCS_REG_OFFSET, 0x13);
            REG_WRITE_32_BIT_PTR(base + I2C_MCS_REG_OFFSET, 0x04);
        }
        return I2C_RETURN_ERROR;
    }else{
        /* Read the Data, Freeze if the Queue is Full */
        RegisterCheck = (uint32)(*(volatile uint32 *)(base + I2C_MDR_REG_OFFSET));
        while(I2C_Queue_Buffer[(2*my_I2C)+I2C_Receive_Buffer_Mask]->isFull(I2C_Queue_Buffer[(2*my_I2C)+I2C_Receive_Buffer_Mask]));
        I2C_Queue_Buffer[(2*my_I2C)+I2C_Receive_Buffer_Mask]->insert(I2C_Queue_Buffer[(2*my_I2C)+I2C_Receive_Buffer_Mask], RegisterCheck);
    }
    MasterMultibleReceiveLoop(base, n, I2C_Queue_Buffer[(2*my_I2C)+I2C_Receive_Buffer_Mask]);
    /* 00101
     * RUN, STOP is HIGH
     * START is low
     * That would generate a Master Last Receive Signal */
    REG_CLEAR_SPECIFIC_BIT_PTR(base + I2C_MCS_REG_OFFSET, 0x1A);
    REG_WRITE_32_BIT_PTR(base + I2C_MCS_REG_OFFSET, 0x5);
    /*BUSY Checking "if the I2C module is BUSY (Finished Transmission or not )" */
    do{
        RegisterCheck = (*(volatile uint32 *)(base + I2C_MCS_REG_OFFSET));
    }while((RegisterCheck & (1 << I2C_MCS_BUSY_MASK)));
    /* Error Checking */
    RegisterCheck = (*(volatile uint32 *)(base + I2C_MCS_REG_OFFSET));
    if(RegisterCheck & (1 << I2C_MCS_ERROR_MASK)){
        return I2C_RETURN_ERROR;
    }
    /* Read the Data, Freeze if the Queue is Full */
    RegisterCheck = (uint32)(*(volatile uint32 *)(base + I2C_MDR_REG_OFFSET));
    while(I2C_Queue_Buffer[(2*my_I2C)+I2C_Receive_Buffer_Mask]->isFull(I2C_Queue_Buffer[(2*my_I2C)+I2C_Receive_Buffer_Mask]));
    I2C_Queue_Buffer[(2*my_I2C)+I2C_Receive_Buffer_Mask]->insert(I2C_Queue_Buffer[(2*my_I2C)+I2C_Receive_Buffer_Mask], RegisterCheck);
    return I2C_RETURN_FINE;
}

/******************************************************************************
* \Syntax          : uint16 I2C_MasterReceiveAllQueueBytes(I2C_ChannelType my_I2C, uint8 address, I2C_RepeatedStartType RepeatedStart)
* \Description     : The Function Receives All the bytes in the I2C module Queue
* \Parameters (in) : my_I2C          A struct that contains ( I2C Channel number, Slave Address )
*                    RepeatedStart   Flag that determine whether the transmission is a repeatedStart or not
* \Parameters (out): None
* \Return value:   : uint16     I2C_RETURN_ERROR
*                               I2C_RETURN_FINE
*******************************************************************************/
uint16 I2C_MasterReceiveAllQueueBytes(I2C_ChannelType my_I2C, uint8 address, I2C_RepeatedStartType RepeatedStart){
    uint8 byteCnt;
    byteCnt = I2C_Queue_Buffer[(2*my_I2C)+I2C_Receive_Buffer_Mask]->getSize(I2C_Queue_Buffer[(2*my_I2C)+I2C_Receive_Buffer_Mask]);
    if(I2C_Queue_Buffer[(2*my_I2C)+I2C_Receive_Buffer_Mask]->isFull(I2C_Queue_Buffer[(2*my_I2C)+I2C_Receive_Buffer_Mask])){
        /* No size in the Queue */
        return I2C_RETURN_ERROR ;
    }else{
        I2C_MasterReceiveMultibleBytes(my_I2C, address, byteCnt , RepeatedStart);
    }
    return I2C_RETURN_FINE ;
}


void I2C_SlaveTransmit_one_Byte(I2C_ChannelType my_I2C){

}
void I2C_SlaveReceive_one_Byte(I2C_ChannelType my_I2C){

}


void __attribute__((weak)) App_I2C0_Tx_CallBack(void){ }
void __attribute__((weak)) App_I2C0_Rx_CallBack(void){ }
void __attribute__((weak)) App_I2C1_Tx_CallBack(void){ }
void __attribute__((weak)) App_I2C1_Rx_CallBack(void){ }
void __attribute__((weak)) App_I2C2_Tx_CallBack(void){ }
void __attribute__((weak)) App_I2C2_Rx_CallBack(void){ }
void __attribute__((weak)) App_I2C3_Tx_CallBack(void){ }
void __attribute__((weak)) App_I2C3_Rx_CallBack(void){ }

/**********************************************************************************************************************
 *  GLOBAL FUNCTIONS
 *********************************************************************************************************************/



/******************************************************************************
* \Syntax          : void I2C_init(void)
* \Description     : The Function Initialize all the I2C we are going to
*                    use in our program
*
* \Sync\Async      : Synchronous
* \Reentrancy      : Non Reentrant
* \Parameters (in) : None
* \Parameters (out): None
* \Return value:   : None
*******************************************************************************/

void I2C_init(void){
    sint8 i, j;
    uint32 base;
    volatile sint32 TimeCalculation;
    volatile uint32 RegisterCheck;


    /******************************************************************************
     * Dealing with "I2C_ConfigType" that contains the I2C module Slaves and Masters.
     * So we need to check over everything in the implementation
    *******************************************************************************/

    for(i = 0; i < I2C_CONFIGURED_NUMBER; i++){
        base = get_address(I2C_Container[i].I2C_num);


        /*
         * 1 .. Initialize the I2C clock using RCGCI2C Register
         */
        /* Check if the I2C module is already configured and the RCGCI2C already has a value of 1 */

        RegisterCheck = (uint32)SYSCTL_RCGCI2C;
        if(RegisterCheck & (1 << (uint8)I2C_Container[i].I2C_num)){
            /* the I2C module is already configured !!!! */
            continue;
        }else{
            REG_WRITE_BIT(SYSCTL_RCGCI2C, (uint8)I2C_Container[i].I2C_num);
            // For delay purpose
            RegisterCheck = (uint32)SYSCTL_RCGCI2C;
        }

        // Create a buffer for it
        I2C_Queue_Buffer[(2 * I2C_Container[i].I2C_num) + I2C_Transmit_Buffer_Mask] = Queue_Create_DYNAMIC_uint8(16);
        I2C_Queue_Buffer[(2 * I2C_Container[i].I2C_num) + I2C_Receive_Buffer_Mask] = Queue_Create_DYNAMIC_uint8(16);

        /*
         * 2 .. Initialize the I2C Mode whether it was Master or Slave
         * using the bits MasterEnable and SlaveEnable in MCR (Master Configuration Register)
         * we also will Set the Interrupt Mask as long we are checking it's master or slave
         */
        if(I2C_Container[i].I2C_Mode == I2C_ModeMaster){
/* ================================================ Master I2c ==================================== */

            // Check if the same I2C has been used as Slave
            if( I2C_SlaveModulesUsed & (1 << I2C_Container[i].I2C_Mode)){
                continue;
            }else{
            // Save it in Transmit Variable
                I2C_MasterModulesUsed |= (1 << I2C_Container[i].I2C_Mode);
            }

            // I2CMCR For SlaveDisable and MasterEnable
            REG_WRITE_BIT_PTR(base + I2C_MCR_REG_OFFSET, 4);
            REG_CLEAR_BIT_PTR(base + I2C_MCR_REG_OFFSET, 5);

            // I2CMCR For GlitchSupression
            if(I2C_Container[i].I2c_GlitchSupression == I2C_GlitchSuppression_ON){
                REG_WRITE_BIT_PTR(base + I2C_MCR_REG_OFFSET, 6);
                REG_WRITE_32_BIT_PTR((base + I2C_MCR2_REG_OFFSET), I2C_Container[i].I2C_GlitchFilterPulseWidth);
            }else{
                REG_CLEAR_BIT_PTR(base + I2C_MCR_REG_OFFSET, 6);
            }

            // I2CMCR For Loop Back
            if(I2C_Container[i].I2C_LoopBack == I2C_LoopBack_ON){
                REG_WRITE_BIT_PTR(base + I2C_MCR_REG_OFFSET, 0);
            }else{
                REG_CLEAR_BIT_PTR(base + I2C_MCR_REG_OFFSET, 0);
            }

            // Clock Low Time Out
            REG_WRITE_32_BIT_PTR((base + I2C_MCLKOCNT_REG_OFFSET), I2C_Container[i].I2C_Clock_Low_Timeout_Count);

            // I2CIM For Setting the Master Interrupt mask Register
            for(j = 0; j < I2C_ALLOWED_INTERRUPTS_NUMBER; j++){
                switch(I2C_Container[i].I2C_MasterISRMode[j]){
                case I2C_MasterISR_None :
                    break;
                case I2C_MasterISR_BusTimeout:
                    REG_WRITE_BIT_PTR(base + I2C_MIMR_REG_OFFSET, 1);
                    break;
                case I2C_MasterISR_TransactionComplete:
                case I2C_MasterISR_ArbitrationLost:
                case I2C_MasterISR_TransactionError:
                    REG_WRITE_BIT_PTR(base + I2C_MIMR_REG_OFFSET, 0);
                    break;
                default :
                    break;
                }
            }

            /*
             * 3 .. Set the I2C Clock using the formula
             * TPR = (System Clock/(2*(SCL_LP + SCL_HP)*SCL_CLK))-1;
             * TPR = (20MHz/(2*(6+4)*100000))-1;
             * TPR = 9
             * Using the register I2CMTPR
             */

            switch(I2C_Container[i].I2C_Speed){
            case I2cSpeed_SM_100Kbps:
                TimeCalculation = (CPU_CLOCK*1000/((20)*100))-1;
                break;
            case I2cSpeed_FM_400Kbps:
                TimeCalculation = (CPU_CLOCK*1000/((20)*400))-1;
                break;
            case I2cSpeed_FMP_1Mbps:
                TimeCalculation = (CPU_CLOCK*1000/((20)*1000))-1;
                break;
            case I2cSpeed_HS_3Mbps:
                TimeCalculation = (CPU_CLOCK*10000/((6)*33300))-1;
                REG_WRITE_BIT_PTR(base + I2C_MTPR_REG_OFFSET, 7);
                break;
            default :
                break;
            }
            if(TimeCalculation > 0 && TimeCalculation <= 128){
                REG_CLEAR_32_BIT_PTR((base + I2C_MTPR_REG_OFFSET));
                REG_WRITE_32_BIT_PTR((base + I2C_MTPR_REG_OFFSET), (uint32)TimeCalculation);
            }else{
                /* error */
                return ;
            }

        }else if(I2C_Container[i].I2C_Mode == I2C_ModeSlave){
/* ================================================ Slave I2c ==================================== */
            // Check if the same I2C has been used as Transmit
            if( I2C_MasterModulesUsed & (1 << I2C_Container[i].I2C_Mode)){
                continue;
            }else{
            // Save it in Slave Variable
                I2C_SlaveModulesUsed |= (1 << I2C_Container[i].I2C_Mode);
            }

            // Create a buffer for it
            I2C_Queue_Buffer[I2C_Container[i].I2C_Mode] = Queue_Create_DYNAMIC_uint8(16);

            // I2CMCR For SlaveEnable and MasterDisable
            REG_WRITE_BIT_PTR(base + I2C_MCR_REG_OFFSET, 5);
            REG_CLEAR_BIT_PTR(base + I2C_MCR_REG_OFFSET, 4);

            // Setting it's Address 0
            REG_WRITE_32_BIT_PTR((base + I2C_SOAR_REG_OFFSET), (0x7F & I2C_Container[i].I2C_SlaveAddress1));

            // Setting it's Second Address "If used"
            if(I2C_Container[i].I2C_SlaveAddress2 > 0){
                REG_WRITE_32_BIT_PTR((base + I2C_SOAR2_REG_OFFSET), (0x7F & I2C_Container[i].I2C_SlaveAddress2));
                REG_WRITE_BIT_PTR(base + I2C_SOAR2_REG_OFFSET, 7);
            }


            // I2CIM For Setting the Master Interrupt mask Register
            for(j = 0; j < I2C_ALLOWED_INTERRUPTS_NUMBER; j++){
                switch(I2C_Container[i].I2C_SlaveISRMode[j]){
                case I2C_SlaveISR_None :
                    break;
                case I2C_SlaveISR_TransactionReceived:
                case I2C_SlaveISR_TransactionRequested:
                    REG_WRITE_BIT_PTR(base + I2C_SIMR_REG_OFFSET, 0);
                    break;
                case I2C_SlaveISR_StartCondition:
                    REG_WRITE_BIT_PTR(base + I2C_SIMR_REG_OFFSET, 1);
                    break;
                case I2C_SlaveISR_StopCondition:
                    REG_WRITE_BIT_PTR(base + I2C_SIMR_REG_OFFSET, 2);
                    break;
                default :
                    break;
                }
            }

            // Activate the Slave Operations
            REG_WRITE_BIT_PTR(base + I2C_SCSR_REG_OFFSET, 0);
        }
    }
}



/******************************************************************************
* \Syntax          : void I2C_MasterPoke_to_transmit(I2C_ChannelType my_I2C, uint8 address, uint8 Bytes_Cnt)
* \Description     : The Function Transmits the number of the Bytes_Cnt of the I2C module's Queue
* \Parameters (in) : my_I2C          The I2C Channel module number
*                    Bytes_Cnt       The number of the bytes i need to transmit
*                       Bytes_Cnt == 0 that's mean that i need to send all the data in the Queue
*                       Bytes_Cnt == 1 that's mean that i need to send Only one Byte
*                       Bytes_Cnt == n that's mean that i need to send n bytes from the Queue
* \Parameters (out): None
* \Return value:   : None
*******************************************************************************/
void I2C_MasterPoke_to_transmit(I2C_ChannelType my_I2C, uint8 address, uint8 Bytes_Cnt){
    if(Bytes_Cnt == 0){
        I2C_MasterTransmitAllQueueBytes(my_I2C, address, (I2C_RepeatedStartType)I2C_RepeatedStart_OFF);
    }else if(Bytes_Cnt == 1){
        I2C_MasterSingleTransmit(my_I2C, address , (I2C_RepeatedStartType)I2C_RepeatedStart_OFF);
    }else{
        I2C_MasterTransmitMultibleBytes(my_I2C, address, Bytes_Cnt, (I2C_RepeatedStartType)I2C_RepeatedStart_OFF);
    }
}



/******************************************************************************
* \Syntax          : void I2C_MasterPoke_to_receive(I2C_ChannelType my_I2C, uint8 address, uint8 Bytes_Cnt)
* \Description     : The Function Receives the number of the Bytes_Cnt in the I2C module's Queue
* \Parameters (in) : my_I2C          The I2C Channel module number
*                    Bytes_Cnt       The number of the bytes i need to transmit
*                       Bytes_Cnt == 0 that's mean that i need to receive all the data in the Queue
*                       Bytes_Cnt == 1 that's mean that i need to receive Only one Byte
*                       Bytes_Cnt == n that's mean that i need to receive n bytes from the Queue
* \Parameters (out): None
* \Return value:   : None
*******************************************************************************/
void I2C_MasterPoke_to_receive(I2C_ChannelType my_I2C, uint8 address, uint8 Bytes_Cnt){
    if(Bytes_Cnt == 0){
        I2C_MasterReceiveAllQueueBytes(my_I2C, address, (I2C_RepeatedStartType)I2C_RepeatedStart_OFF);
    }else if(Bytes_Cnt == 1){
        I2C_MasterSingleReceive(my_I2C, address, (I2C_RepeatedStartType)I2C_RepeatedStart_OFF);
    }else{
        I2C_MasterReceiveMultibleBytes(my_I2C, address, Bytes_Cnt, (I2C_RepeatedStartType)I2C_RepeatedStart_OFF);
    }
}



/******************************************************************************
* \Syntax          : void I2C_Push_to_Transmit(I2C_ChannelType my_I2C, uint8 x, uint8 must_be_pushed)
* \Description     : The Function Push one Byte to the I2C module Queue to transmit it later
* \Parameters (in) : my_I2C             A struct that contains ( I2C Channel number, Slave Address )
*                    x                  The number of the bytes i need to transmit
*                    must_be_pushed     whether it might be lost or not
* \Parameters (out): None
* \Return value:   : None
*******************************************************************************/
void I2C_Push_to_Transmit(I2C_ChannelType my_I2C, uint8 x, uint8 must_be_pushed){
    /* Check if the I2C Channel Queue is Empty or not */
    if(must_be_pushed == 1){
        /* it will stuck here until it get free */
        while((I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]->isFull(I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]) ));
        I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]->insert(I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask], x);
    }else{
        I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]->insert(I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask], x);
    }
}

/******************************************************************************
* \Syntax          : void I2C_Pop_the_Received(I2C_ChannelType my_I2C, uint8* data)
* \Description     : The Function Push one Byte to the I2C module Queue to transmit it later
* \Parameters (in) : my_I2C             A struct that contains ( I2C Channel number, Slave Address )
* \Parameters (out): data               The data that have been received and saved in the Queue
* \Return value:   : None
*******************************************************************************/
void I2C_Pop_the_Received(I2C_ChannelType my_I2C, uint8* data){
    uint8 i = 0;
    /* Check if the I2C Channel Queue is Empty or not */
    /* it will stuck here until there is some data */
    while((I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]->isEmpty(I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]) ));
    do{
        data[i] = I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]->remove(I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]);
        i++;
    }while(! (I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]->isEmpty(I2C_Queue_Buffer[(2*my_I2C)+I2C_Transmit_Buffer_Mask]) ));
}



void I2C0_handler(void);

void I2C1_handler(void);

void I2C2_handler(void);

void I2C3_handler(void);






