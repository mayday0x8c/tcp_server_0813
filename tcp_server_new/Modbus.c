/******************************************************************
 CopyRight:   H3C ALL rights reserved
 File name:   Modbus.c
 Description: 
 Author:      Zhang Jiawei
 Version:     V01
 Data:        2024-8
 *****************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "inc/Modbus.h"

volatile ushort Fun_Code = 0;
volatile ushort Start_address = 0;
volatile ushort reg_num = 0;
volatile ushort Modbus_Length = 0;
volatile ushort End_address = 0;

Modbus_TCP stModbus_TCPHead;
Modbus_TCP rxModbus_TCPHead;

ushort ModbusProcFlag = 0;
ushort ModbusProcLength = 0;
ushort wYear = 2024;
ushort wMonth = 0;
ushort wDay = 0;
ushort w3xLen;
ushort w4xLen;

const  CanSt3X Modbus_3xArray[]=
{
 {30000,1,(pGet)sGetYear},
 {30001,1,(pGet)sGetMonth},
 {30002,1,(pGet)sGetDay},
 {32080,2,(pGet)sGetActivePower},
 {32082,2,(pGet)sGetReactivePower}
};
const  CanSt4X Modbus_4xArray[]=
{
 {30000,1,2000,2100,(pSet)sSetYear,(pGet)sGetYear},
 {30001,1,1,12,(pSet)sSetMonth,(pGet)sGetMonth},
 {30002,1,1,31,(pSet)sSetDay,(pGet)sGetDay},
 {32080,2,-300000,300000,(pSet)sSetActivePower,(pGet)sGetActivePower},
 {32082,2,-300000,300000,(pSet)sSetReactivePower,(pGet)sGetReactivePower},
 {40012,1,0,65535,(pSet)sFlashControl,(pGet)sFlashGet},

};

//*****************************************************************************
//函数功能：Modbus数据初始化
//入参：无
//出参：无
//*****************************************************************************
void Modbus_Initial()
{
    w3xLen = sizeof(Modbus_3xArray) / sizeof(CanSt3X);
    w4xLen = sizeof(Modbus_4xArray) / sizeof(CanSt4X);
}

//*****************************************************************************
//函数功能：解析4x数据
//入参：arrar4X——数据指针;length——包长;pdata——数据指针
//出参：无
//*****************************************************************************
void sSetParamData(const CanSt4X* arrar4X,ushort length,uchar *pdata)
{
    ushort i = 0;
    ushort AddrBiasNow = 0;//
    uint SetValue = 0;

    for(i=0;i<(length);i++)
    {
        if((Start_address) <= arrar4X[i].Addr)//1.Find the start address in Array
        {
            while(arrar4X[i].Addr <= End_address)//2.call corresponding function to set prarmeter
            {
                AddrBiasNow = (arrar4X[i].Addr-Start_address)<<1;

                if(arrar4X[i].Length == 1)
                {
                    SetValue = iDataCom_8to16(pdata + AddrBiasNow);
                    if((SetValue >= arrar4X[i].imin) && (SetValue <= arrar4X[i].imax))
                    {
                        arrar4X[i].iSet((ushort)SetValue);
                    }
                }
                else if(arrar4X[i].Length == 2)
                {
                    SetValue = iDataCom_8to32(pdata + AddrBiasNow);
                    if((SetValue >= arrar4X[i].imin) && (SetValue <= arrar4X[i].imax))
                    {
                        arrar4X[i].iSet(SetValue);
                    }
                }

                i++;
            }

            break;
        }
    }
}

//*****************************************************************************
//函数功能：打包3x数据
//入参：arrar3X——数据指针;length——包长;pdata——数据指针
//出参：无
//*****************************************************************************
void sPackage3xData(const CanSt3X * arrar3X,ushort length,uchar *pdata)
{
    short i = 0;
    short AddrBiasNow = 0;

    for(i=0;i<(length);i++)
    {
        if((Start_address) <= arrar3X[i].Addr)//1.Find the start address in Array
        {
            while(arrar3X[i].Addr <= End_address)//2.put the data in send buffer
            {
                AddrBiasNow = (arrar3X[i].Addr - Start_address)<<1;
                if(arrar3X[i].Length == 1)
                {
                    iDataCom_16to8((ushort)(arrar3X[i].iGet()),pdata + AddrBiasNow);
                }
                else if(arrar3X[i].Length == 2)
                {
                    iDataCom_32to8(arrar3X[i].iGet(),pdata + AddrBiasNow);
                }
                i++;
            }

            break;
        }
    }
}

//*****************************************************************************
//函数功能：解析数据
//入参：*pdata——数据指针;len——包长
//出参：无
//*****************************************************************************
ushort Modbus_Proc_Data(uchar *pdata, const ushort len)
{
    ushort CRC = (*(pdata + (len - 1)) << 8)  + *(pdata + (len - 2));
    unsigned char senddata[128];
    senddata[0] = 1;
    CAN_FAULT_WORD Fault_Word;
    CAN_YX_WORD YX_Word;
    YX_Word.YX_WORD = 0x028;
    CAN_YX_WORD YX_answer;
    uchar uc02Num = 0;
    uchar j = 0;
    memcpy(&senddata[1], pdata, (len - 2));
    if (CRC != CRC16_MODBUS(&senddata[0], len - 1))
    {
        return (0);
    }
    memset(&senddata[0], 0, 128);
    Fun_Code = *(pdata);

    switch(Fun_Code)
    {
    default:
        break;
    case CAN_Fun_Code_03:
        Start_address = iDataCom_8to16(pdata + 1);
        Modbus_Length = iDataCom_8to16(pdata + 3);
        End_address = Start_address + Modbus_Length - 1;
        sPackage3xData((CanSt3X*)&Modbus_3xArray, w3xLen, (uchar*)&senddata);
        // CAN_transmit_Data((uchar *)&senddata, 2 * Modbus_Length, TX_MSG_OBJ_ID);
        break;
    case CAN_Fun_Code_04:
        break;
    case CAN_Fun_Code_05:
        Fault_Word.FAULT_WORD = *(pdata + 3);
        memcpy(&senddata[1], (pdata + 2), (len - 2));
        // CAN_transmit_Data((uchar *)&senddata, 2, TX_MSG_OBJ_ID);
        break;
    case CAN_Fun_Code_06:
        Start_address = iDataCom_8to16(pdata + 1);
        Modbus_Length = 1;
        End_address = Start_address + Modbus_Length - 1;
        sSetParamData((CanSt4X*)&Modbus_4xArray, w4xLen, pdata + 3);
        // CAN_transmit_Data(pdata + 3, 2 * Modbus_Length, TX_MSG_OBJ_ID);
        break;
    case CAN_Fun_Code_10:
        Start_address = iDataCom_8to16(pdata + 1);
        Modbus_Length = iDataCom_8to16(pdata + 3);
        End_address = Start_address + Modbus_Length - 1;
        sSetParamData((CanSt4X*)&Modbus_4xArray, w4xLen, pdata + 6);
        // CAN_transmit_Data((uchar *)&senddata, 2 * Modbus_Length, TX_MSG_OBJ_ID);
        break;

    }
    return (1);
}

void TCP_Modbus_Send(ushort FunCode, ushort startaddr, ushort len, unsigned char * SendData)
{
	unsigned char *tempdata = (unsigned char *)malloc(512);
	stModbus_TCPHead.usSerialNum ++;
	stModbus_TCPHead.usRes = 0;
	stModbus_TCPHead.usLength = len;
	stModbus_TCPHead.unit_id = 1;
	memcpy(tempdata,(unsigned char *)&stModbus_TCPHead, 7);
	switch(FunCode)
    {
    default:
        break;
    case CAN_Fun_Code_03:
		Start_address = startaddr;
        End_address = Start_address + len - 1;
        sPackage3xData((CanSt3X*)&Modbus_3xArray, w3xLen, tempdata + 7);
        printf("tempdata[7] = %d\n",tempdata[7]);
        printf("tempdata[8] = %d\n",tempdata[8]);
        printf("tempdata[9] = %d\n",tempdata[9]);
        printf("tempdata[10] = %d\n",tempdata[10]);
        // memcpy(tempdata, (unsigned char *)&stModbus_TCPHead, sizeof(stModbus_TCPHead));
        break;
    case CAN_Fun_Code_04:
        break;
	}
	memcpy(SendData, tempdata, len + 7);
}
//*****************************************************************************
//函数功能：获取unsigned char指针指向数据的长度
//入参：*ustr-数据指针
//出参：数据长度
//*****************************************************************************
ushort strlen_uc(const unsigned char* ustr) {
    if (ustr == NULL) {
        return 0;
    }
    const unsigned char* s = ustr;
    while (*s) {
        ++s;
    }
    return s - ustr;
}
//*****************************************************************************
//函数功能：从Modbus-TCP报文中获取TCP头信息并拆除
//入参：*data-数据指针，*head-TCP头结构体指针
//出参：修改后的TCP头结构体指针
//*****************************************************************************
void get_MBAP_FromTCPdata(unsigned char* data,Modbus_TCP* head) {
    // 确保传入的数据长度足够
    ushort slen =strlen_uc(data);
    if (slen <= 7) 
    {
        return;
    }
    // 提取Modbus TCP头部信息
    head->usSerialNum = (data[0] << 8) | data[1];
    head->usRes = (data[2] << 8) | data[3];
    head->usLength = (data[4] << 8) | data[5];
    head->unit_id = data[6];
}
//*****************************************************************************
//函数功能：从Modbus-TCP报文中获取Modbus报文数据
//入参：*src-源数据指针，*pdata_modbus-modbus报文指针,*res-modbus数据结构体指针
//出参：修改后的modbus报文指针、数据结构体指针
//*****************************************************************************
void TCP_Modbus_Analyze(unsigned char* src,unsigned char* pdata_modbus,SModbus_TCP_DataUnit* res)
{
    ushort slen =strlen_uc(src);
    get_MBAP_FromTCPdata(src,&rxModbus_TCPHead);
    memcpy(pdata_modbus,(void*)(src+7),slen-7);
    ushort src_len =strlen_uc(pdata_modbus);
    if(strlen_uc(pdata_modbus) < 1)
    {
        return;
    }
    //modbus功能码
    res->modbus_funcode =pdata_modbus[0];
    //modbus数据域
    ushort slenofdata = src_len-1>MAX_LEN_MODBUSTCPDATA?MAX_LEN_MODBUSTCPDATA:src_len-1;
    memcpy(res->data,(void*)(pdata_modbus+1),slenofdata); 
}

//读离散输入寄存器
void Read_bit_Act(SModbus_TCP_DataUnit* RxMsg)//0x02
{



}
//*****************************************************************************
//函数功能：连续uchar合成int
//入参：*arr-源数组，arr_len-数组长度，start_index-起始下标
//出参：int
//*****************************************************************************
int combine_bytes_to_int(const uchar *arr, int arr_len, int start_index) {
    if (arr == NULL || start_index < 0 || start_index + 3 >= arr_len) {
        return 0;
    }

    int result = 0;

    result |= ((int)arr[start_index] << 24);
    result |= ((int)arr[start_index + 1] << 16);
    result |= ((int)arr[start_index + 2] << 8);
    result |= (int)arr[start_index + 3];

    return result;
}
//*****************************************************************************
//函数功能：连续uchar合成uint
//入参：*arr-源数组，arr_len-数组长度，start_index-起始下标
//出参：uint
//*****************************************************************************
uint combine_bytes_to_uint(const uchar *arr, int arr_len, int start_index) {
    if (arr == NULL || start_index < 0 || start_index + 3 >= arr_len) {
        return 0;
    }

    uint result = 0;

    result |= ((unsigned int)arr[start_index] << 24);
    result |= ((unsigned int)arr[start_index + 1] << 16);
    result |= ((unsigned int)arr[start_index + 2] << 8);
    result |= (unsigned int)arr[start_index + 3];

    return result;
}
//*****************************************************************************
//函数功能：连续uchar合成short
//入参：*arr-源数组，arr_len-数组长度，start_index-起始下标
//出参：short
//*****************************************************************************
short combine_bytes_to_short(const uchar *arr, int arr_len, int start_index) {
    if (arr == NULL || start_index < 0 || start_index + 1 >= arr_len) {
        // Invalid input, return some default value or handle error
        return 0;
    }

    short result = 0;

    result |= (arr[start_index] << 8);
    result |= (arr[start_index + 1]);

    return result;
}
//*****************************************************************************
//函数功能：连续uchar合成ushort
//入参：*arr-源数组，arr_len-数组长度，start_index-起始下标
//出参：ushort
//*****************************************************************************
ushort combine_bytes_to_ushort(const uchar *arr, int arr_len, int start_index) {
    if (arr == NULL || start_index < 0 || start_index + 1 >= arr_len) {
        // Invalid input, return some default value or handle error
        return 0;
    }

    ushort result = 0;

    result |= (arr[start_index] << 8);
    result |= (arr[start_index + 1]);

    return result;
}
//*****************************************************************************
//函数功能：连续uchar合成ushort
//入参：*arr-源数组，arr_len-数组长度，start_index-起始下标
//出参：ushort
//*****************************************************************************
void split_data_to_array(void* src, ushort elem_size, int len, unsigned char** des) {
    // 检查输入参数
    if(src == NULL || len <= 0 || des == NULL || elem_size <= 0) {
        return;
    }

    // 如果 des 是 NULL，分配新内存
    if (*des == NULL) {
        *des = (unsigned char*)malloc(len * sizeof(unsigned char));
        if (*des == NULL) {
            // 分配失败，退出函数
            return;
        }
    } else {
        // 字符串长度检查，并重新分配内存
        *des = (unsigned char*)realloc(*des, len * sizeof(unsigned char));
        if (*des == NULL) {
            // 重新分配失败，退出函数
            return;
        }
    }

    // 将数据从 src 复制到 des
    unsigned char* src_bytes = (unsigned char*)src;
    for(int i = 0; i < len; i++) {
        unsigned char* element = src_bytes + i * elem_size;
        (*des)[i] = *element;
    }
}

//读保持寄存器
void Read_HoldingReg_Act(SModbus_TCP_DataUnit* RxMsg,SModbus_TCP_DataUnit* TxMsg)//0x03
{

    Start_address = combine_bytes_to_ushort(TxMsg->data,MAX_LEN_MODBUSTCPDATA,0);//首字节
    reg_num = combine_bytes_to_ushort(TxMsg->data,MAX_LEN_MODBUSTCPDATA,2);//第3个字节
    Modbus_Length = combine_bytes_to_ushort(RxMsg->data,MAX_LEN_MODBUSTCPDATA,0);//首字节
    End_address   = Start_address + Modbus_Length - 1;
    int recVal    =  0;
    for(int i = 0 ;i < Modbus_Length;i++)
    {
        for(int j = 0 ;j < w4xLen;j++)
        {
            int count = Modbus_4xArray[j].Length;
        if((Start_address+count*i == Modbus_4xArray[j].Addr))
        {
            recVal = combine_bytes_to_int(RxMsg->data,MAX_LEN_MODBUSTCPDATA,1+count*i);//第2个字节后，两个寄存器内容合成一个int
            recVal = Modbus_4xArray[j].imax>recVal?recVal:Modbus_4xArray[j].imax;
            recVal = Modbus_4xArray[j].imin<recVal?recVal:Modbus_4xArray[j].imin;
            Modbus_4xArray[j].iSet(recVal);
            printf("Modbus_4xArray[%d].val=%d",i,recVal);
        }
     }
     
   }
   
}
//读输入寄存器
void Read_InputReg_Act(SModbus_TCP_DataUnit* RxMsg, SModbus_TCP_DataUnit* TxMsg) {
    // 读输入寄存器
    unsigned short Start_address = combine_bytes_to_ushort(TxMsg->data, MAX_LEN_MODBUSTCPDATA, 0);
    unsigned short reg_num = combine_bytes_to_ushort(TxMsg->data, MAX_LEN_MODBUSTCPDATA, 2);
    unsigned short End_address = Start_address + reg_num - 1;

    int regValue = 0;
    int byteCount = 0;

    // 准备返回消息结构
    RxMsg->head = TxMsg->head;
    RxMsg->modbus_funcode = 0x04;  // 功能码 04

    // 遍历需要读取的寄存器
    for (int i = 0; i < w3xLen; i++) {
        if (Modbus_3xArray[i].Addr >= Start_address && Modbus_3xArray[i].Addr <= End_address) {
            regValue = Modbus_3xArray[i].iGet();
            // 处理数据（高字节和低字节）
            RxMsg->data[byteCount++] = (regValue >> 8) & 0xFF;
            RxMsg->data[byteCount++] = regValue & 0xFF;
        }
    }
    RxMsg->data[0] = byteCount;
    // 调整 Modbus 长度字段
    RxMsg->head.usLength = byteCount + 2;
}

//写单个线圈寄存器
void Write_bit_Act(SModbus_TCP_DataUnit* TxMsg, ushort slaveid, ushort addr, ushort val) {
    TxMsg->head.unit_id = slaveid;
    TxMsg->head.usSerialNum = rxModbus_TCPHead.usSerialNum++;
    TxMsg->modbus_funcode = 0x05; // 设置功能码为0x05

    // 地址2个字节
    split_data_to_array(&addr, sizeof(ushort), 1, TxMsg->modbus_addr);

    // 填充数据，在Modbus协议里，0xFF00表示打开线圈，0x0000表示关闭线圈
    TxMsg->data[0] = (val >> 8) & 0xFF;
    TxMsg->data[1] = val & 0xFF;

    // 设置Modbus头的长度字段，功能码1字节 + 2字节地址 + 2字节数据，总共5字节
    TxMsg->head.usLength = 6;  // 单位：字节，注意这里还包括单元标识符1个字节但不包括自身
}
void Write_SingleHoldingReg_Act(SModbus_TCP_DataUnit* RxMsg, SModbus_TCP_DataUnit* TxMsg) {
    // 提取地址和要写入的值
    ushort address = combine_bytes_to_ushort(RxMsg->data, MAX_LEN_MODBUSTCPDATA, 0);
    ushort value = combine_bytes_to_ushort(RxMsg->data, MAX_LEN_MODBUSTCPDATA, 2);

    // 检查地址是否合法
    // if (address >= NUM_HOLDING_REGS) {
    //     printf("Error: Invalid holding register address\n");
    //     return;
    // }

    // 更新保持寄存器数据


    // 构建响应消息
    TxMsg->head = RxMsg->head;
    TxMsg->modbus_funcode = 0x06;
    memcpy(TxMsg->data, RxMsg->data, 4);  // 复制前4个字节（地址和值）

    // 设置Modbus头信息长度字段
    TxMsg->head.usLength = 6;  // 功能码1字节 + 2字节地址 + 2字节数据，总共5字节 + 单元标识符1字节
}
void Write_MultiHoldingReg_Act(SModbus_TCP_DataUnit* RxMsg, SModbus_TCP_DataUnit* TxMsg) {
    // 提取起始地址和寄存器数量
    ushort start_address = combine_bytes_to_ushort(RxMsg->data, MAX_LEN_MODBUSTCPDATA, 0);
    ushort num_registers = combine_bytes_to_ushort(RxMsg->data, MAX_LEN_MODBUSTCPDATA, 2);
    uchar byte_count = RxMsg->data[4];

    // 检查地址和数量是否合法
    // if (start_address + num_registers > NUM_HOLDING_REGS) {
    //     printf("Error: Invalid holding register address or quantity\n");
    //     return;
    // }

    // 写入保持寄存器数据
    for (int i = 0; i < num_registers; i++) {
        ushort value = combine_bytes_to_ushort(RxMsg->data, MAX_LEN_MODBUSTCPDATA, 5 + 2 * i);
    }

    // 构建响应消息
    TxMsg->head = RxMsg->head;
    TxMsg->modbus_funcode = 0x10;
    TxMsg->data[0] = (start_address >> 8) & 0xFF;
    TxMsg->data[1] = start_address & 0xFF;
    TxMsg->data[2] = (num_registers >> 8) & 0xFF;
    TxMsg->data[3] = num_registers & 0xFF;

    // 设置Modbus头信息长度字段
    TxMsg->head.usLength = 6;  // 功能码1字节 + 2字节起始地址 + 2字节寄存器数量，总共5字节 + 单元标识符1字节
}

//响应收到的报文
void ModbusRsData_Act(SModbus_TCP_DataUnit* RxMsg)
{
  ResponseAct pAct;
  switch (RxMsg->modbus_funcode)
  {
    case CAN_Fun_Code_02:
        pAct = &Read_bit_Act;
        break;
    case CAN_Fun_Code_03:
        pAct = &Read_HoldingReg_Act;
        break;
    case CAN_Fun_Code_04:
        pAct = &Read_InputReg_Act;
        break;
    case CAN_Fun_Code_05:
        pAct = &Write_bit_Act;
        break;
    case CAN_Fun_Code_06:
        pAct = &Write_SingleHoldingReg_Act;
        break;
    case CAN_Fun_Code_10:
        pAct = &Write_MultiHoldingReg_Act;
        break;
    default:
        break;
  }
  pAct(RxMsg);
}
//回复读bit请求
void getAnswer_Read_bit(SModbus_TCP_DataUnit* RxMsg,SModbus_TCP_DataUnit* ans)//0x02
{
  
}
//回复读保持寄存器请求
void getAnswer_Read_HoldingReg(SModbus_TCP_DataUnit* RxMsg,SModbus_TCP_DataUnit* ans)//0x03
{
     // 功能码 02 示例处理: 读取离散输入
    ans->modbus_funcode = 0x02;
    // 假设读取 n 个离散输入，举例回复 2 个字节数据
    ans->data[0] = 0x02;
    ans->data[1] = 0x01;  // 举例：第1字节的值
    ans->data[2] = 0x00;  // 第2字节的值
    ans->head.usLength = htons(3+2); // TCP头部6字节，功能码和数据内容3字节
}
//回复写输入寄存器请求
void getAnswer_Write_InputReg(SModbus_TCP_DataUnit* RxMsg,SModbus_TCP_DataUnit* ans)//0x04
{
    // 功能码 04 示例处理: 读取输入寄存器
    ans->modbus_funcode = 0x04;
    // 假设读取 n 个保持寄存器，举例回复 4 个字节数据
    ans->data[0] = 0x04; // 字节数
    ans->data[1] = 0x00; // 第1个输入寄存器高字节
    ans->data[2] = 0x03; // 第1个输入寄存器低字节
    ans->data[3] = 0x00; // 第2个输入寄存器高字节
    ans->data[4] = 0x07; // 第2个输入寄存器低字节
    ans->head.usLength = htons(5+2); // 功能码1字节+数据5字节+modbus头部2字节
}
//回复写bit请求
void getAnswer_Write_bit(SModbus_TCP_DataUnit* RxMsg,SModbus_TCP_DataUnit* ans)//0x05
{
 // 功能码 05 示例处理: 写单个线圈
    ans->modbus_funcode = 0x05;
    // 举例复制同样的数据
    ans->data[0] = RxMsg->data[0];
    ans->data[1] = RxMsg->data[1];
    ans->data[2] = RxMsg->data[2];
    ans->data[3] = RxMsg->data[3];
    ans->head.usLength = htons(4+2); // TCP头部6字节，功能码和数据内容4字节
}
//回复写单个保持寄存器请求
void getAnswer_Write_HoldingReg(SModbus_TCP_DataUnit* RxMsg,SModbus_TCP_DataUnit* ans)//0x06
{
   // 功能码 06 示例处理: 写单个寄存器
    ans->modbus_funcode = 0x06;
    // 举例复制同样的数据
    ans->data[0] = RxMsg->data[0];
    ans->data[1] = RxMsg->data[1];
    ans->data[2] = RxMsg->data[2];
    ans->data[3] = RxMsg->data[3];
    ans->head.usLength = htons(4+2); // TCP头部6字节，功能码和数据内容4字节
}
//回复写多个保持寄存器请求
void getAnswer_Write_MultiHoldingReg(SModbus_TCP_DataUnit* RxMsg,SModbus_TCP_DataUnit* ans)//0x010
{
   // 功能码 10 示例处理: 写多个寄存器
    ans->modbus_funcode = 0x10;
    // 举例复制写入的起始地址和数量
    ans->data[0] = RxMsg->data[0];
    ans->data[1] = RxMsg->data[1];
    ans->data[2] = RxMsg->data[2];
    ans->data[3] = RxMsg->data[3];
    ans->head.usLength = htons(4+2); // TCP头部6字节，功能码和数据内容4字节
}

//生成回复报文
void AnswerSwitch(SModbus_TCP_DataUnit* RxMsg,uchar* ans)
{
    GetAnswerFrame pAns = NULL;
    switch (RxMsg->modbus_funcode)
    {
    case CAN_Fun_Code_02:
        pAns = &getAnswer_Read_bit;
        break;
    case CAN_Fun_Code_03:
        pAns = &getAnswer_Read_HoldingReg;
        break;
    case CAN_Fun_Code_04:
        pAns = &getAnswer_Write_InputReg;
        break;
    case CAN_Fun_Code_05:
        pAns = &getAnswer_Write_bit;
        break;
    case CAN_Fun_Code_06:
        pAns = &getAnswer_Write_HoldingReg;
        break;
    case CAN_Fun_Code_10:
        pAns = &getAnswer_Write_MultiHoldingReg;
        break;
    default:
        break;
    pAns(RxMsg,ans);
  }
}
void TCP_Modbus_Recv()
{
	// unsigned char *tempdata = (unsigned char *)malloc(512);
	// stModbus_TCPHead.usSerialNum ++;
	// stModbus_TCPHead.usRes = 0;
	// stModbus_TCPHead.usLength = len;
	// stModbus_TCPHead.unit_id = 1;
	// memcpy(tempdata, (unsigned char *)&stModbus_TCPHead, 7);
	// switch(FunCode)
    // {
    // default:
    //     break;
    // case CAN_Fun_Code_03:
	// 	Start_address = startaddr;
    //     End_address = Start_address + len - 1;
    //     sPackage3xData((CanSt3X*)&Modbus_3xArray, w3xLen, tempdata + 7);
    //     printf("tempdata[7] = %d\n",tempdata[7]);
    //     printf("tempdata[8] = %d\n",tempdata[8]);
    //     printf("tempdata[9] = %d\n",tempdata[9]);
    //     printf("tempdata[10] = %d\n",tempdata[10]);
    //     // memcpy(tempdata, (unsigned char *)&stModbus_TCPHead, sizeof(stModbus_TCPHead));
    //     break;
    // case CAN_Fun_Code_04:
    //     break;
	// }
	// memcpy(SendData, tempdata, len + 7);
}

////*****************************************************************************
////函数功能：连续8位合成16位
////入参：*pdata——数据指针
////出参：Data值
////*****************************************************************************
ushort iDataCom_8to16(uchar *pdata)
{
    ushort Data = ((ushort)*pdata << 8) | (ushort)*(pdata + 1);
    return Data;
}

////*****************************************************************************
////函数功能：连续8位合成32位
////入参：*pdata——数据指针
////出参：Data值
////*****************************************************************************
uint iDataCom_8to32(uchar *pdata)
{
    uint Data = (((uint)*pdata) << 24) | ((uint)*(pdata + 1) << 16) |\
            ((uint)*(pdata + 2) << 8) | ((uint)*(pdata + 3));
    return Data;
}

////*****************************************************************************
////函数功能：16位拆成8位
////入参：Selfdata——自身数据,*pdata——数据指针
////出参：无
////*****************************************************************************
void iDataCom_16to8(ushort Selfdata ,uchar *pdata)
{
    *(pdata + 1) = (uchar)(Selfdata & 0xff);
    *(pdata + 0) = (uchar)((Selfdata >> 8) & 0xff);
}

////*****************************************************************************
////函数功能：16位长数据拆成8位
////入参：Selfdata——自身数据,*pdata——数据指针,len——长度
////出参：无
////*****************************************************************************
void iLongDataCom_16to8(ushort *Selfdata ,uchar *pdata ,ushort len)
{
    ushort *temp = Selfdata;
    ushort i = 0;
    for(;i < len / 2; i++)
    {
        *(pdata + 2 * i + 1) = (uchar)(*(temp + i) & 0xff);
        *(pdata + 2 * i) = (uchar)((*(temp + i) >> 8) & 0xff);
    }
}

////*****************************************************************************
////函数功能：32位拆成8位
////入参：Selfdata——自身数据,*pdata——数据指针
////出参：无
////*****************************************************************************
void iDataCom_32to8(uint Selfdata ,uchar *pdata)
{
    *(pdata + 3) = (uchar)(Selfdata & 0xff);
    *(pdata + 2) = (uchar)((Selfdata >> 8) & 0xff);
    *(pdata + 1) = (uchar)((Selfdata >> 16) & 0xff);
    *(pdata + 0) = (uchar)((Selfdata >> 24) & 0xff);
}

void sSetYear(ushort value)
{
    wYear = value;
}
ushort sGetYear()
{
    return wYear;
}

void sSetMonth(ushort value)
{
    wMonth = value;
}
ushort sGetMonth()
{
    return wMonth;
}

void sSetDay(ushort value)
{
    wDay = value;
}
ushort sGetDay()
{
    return wDay;
}

void sSetActivePower(uint value)
{
//    wActivePower = value;
}
uint sGetActivePower()
{
    // return wActivePower;
}

void sSetReactivePower(uint value)
{
    // wReactivePower = value;
}
uint sGetReactivePower()
{
    // return wReactivePower;
}



void sFlashControl(ushort value)
{
//     uint ret = 0;
//     uchar pdata[2];
//     switch(value)
//        {
//        default:
//            break;
//        case FLASH_MODBUS_WRITE:
//            ret = Flash_Write_Calibration_to_Flash();
//            break;
//        case FLASH_MODBUS_READ:
//            ret = Flash_Read_Calibration_from_Flash();
//            break;
//        case FLASH_MODBUS_CLEAR:
//            ret = true;
//            memset(&stGlobal_Cali,0,sizeof(stGlobal_Cali));
//            break;
// //       case FLASH_MODBUS_ERASE:
// //           Flash_Erase_Chip();
// //           break;
//        }
//     if(false == ret)
//     {
//         pdata[1] = FLASH_MODBUS_FAIL;
//     }
//     else
//     {
//         pdata[1] = (uchar)value;
//     }
//     pdata[0] = 0;
//     // CAN_transmit_Data((uchar *)&pdata, 2 * Modbus_Length, TX_MSG_OBJ_ID);
//     Modbus_Length = 0;
}

ushort sFlashGet()
{
    return 0xFF;
}

//*****************************************************************************
//函数功能：CRC16位校验
//入参：*puchMsg——数据指针;usDataLen——数据包长度
//出参：crc校验值
//*****************************************************************************
ushort CRC16_MODBUS(uchar *puchMsg, uint usDataLen)
{
    ushort uCRC = 0xffff;
    uchar num;
    uchar x;
    for(num=0 ; num<usDataLen ; num++)
    {
        uCRC = (*puchMsg++)^uCRC;
        for(x=0;x<8;x++){
            if(uCRC&0x0001){
                uCRC = uCRC>>1;
                uCRC = uCRC^0xA001;
            }else{
                uCRC = uCRC>>1;
            }
        }
    }
    return uCRC;
}