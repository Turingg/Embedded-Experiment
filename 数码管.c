#include "config.h"
#include "LPC2200.h"

#define SI_BIT (1<<18)
#define RCK_BIT (1<<20)
#define SCK_BIT (1<<17)
#define LED 16

#define SI_KEY (1<<6)
#define RCK_KEY (1<<7)
#define SCK_KEY (1<<4)
#define KEY_IN (1<<5)//P0.5

typedef unsigned short U16;
typedef unsigned char U8;
typedef unsigned int U32;
U8 ii=0;
U8 jj=0;
U8 key_scan_bit = 0; 
U8 display_buffer[8]; 

const U8 code[LED]=
{
    //0x3F,0x06,0x5b,0x4F,0x66,0x6D,0x7D,0x07,
    //0x7F,0x6F,0x77,0x7C,0x39,0x5E,0x79,0x71
    //data & (0x800O);
    //data = (data<<1);

    0xFC, 0x60, 0xDA, 0xF2, 0x66, 0xB6, 0xBE, 0xE0,
    0xFE, 0xE6, 0xEE, 0x3E, 0x9C, 0x7A, 0x9E, 0x8E
    //data & (0x00O1);
    //data = (data>>1);
};

void Init(void)
{
    // P0.18
    PINSEL1 &= ~(1<<4);
    PINSEL1 &= ~(1<<5);
    IO0DIR |= SI_BIT;

    // P0.20
    PINSEL1 &= ~(1<<8);
    PINSEL1 &= ~(1<<9);
    IO0DIR |= RCK_BIT;

    // P0.17
    PINSEL1 &= ~(1<<2);
    PINSEL1 &= ~(1<<3);
    IO0DIR |= SCK_BIT;
}

void Init_key()
{
    // P0.4
    PINSEL0 &= ~(1<<8);
    PINSEL0 &= ~(1<<9);
    IO0DIR |= SCK_KEY;

    // P0.7
    PINSEL0 &= ~(1<<14);
    PINSEL0 &= ~(1<<15);
    IO0DIR |= RCK_KEY;

    // P0.6
    PINSEL0 &= ~(1<<12);
    PINSEL0 &= ~(1<<13);
    IO0DIR |= SI_KEY;

    // P0.5 GPIO输入(0)功能，检测按键
    PINSEL0 &= ~(1<<10);
    PINSEL0 &= ~(1<<11);
    IO0DIR &= ~KEY_IN;
    //置0，设置为输入
    //IO0DIR |= KEY_IN;
    ////置1，设置为输出
}

void delay(U32 num)
{
    U32 j;
    for(; num>0; num--)
    {
        for(j=0; j<500; j++)
        {
        }
    }
}

void Send_HalfWord(U8 select, U8 data)
{
    U8 i;
    // RCK拉低，准备数据
    IO0CLR = RCK_BIT;

    // 低八位的片选
    for(i=0; i<8; i++)
    {
        IO0CLR = SCK_BIT;
        if(select & (0x0001))
        {
            // SI为1
            IO0SET = SI_BIT;
        }
        else
        {
            // SI为0
            IO0CLR = SI_BIT;
        }
        // SCK拉高，传输数据
        IO0SET = SCK_BIT;
        // 数据右移
        select = (select>>1);
        IO0CLR = SCK_BIT;
    }
    for(i=0; i<8; i++)
    {
        IO0CLR = SCK_BIT;
        if(data & (0x0001))
        {
            IO0SET = SI_BIT;
        }
        else
        {
            IO0CLR = SI_BIT;
        }
        IO0SET = SCK_BIT;
        data = (data>>1);
        IO0CLR = SCK_BIT;
    }
    
    IO0SET = RCK_BIT;
}

//键盘发送函数
void SendKEY_Word(U16 data)
{
    U8 i=0;
    IO0CLR = RCK_KEY;
    for(i=0; i<16; i++)
    {
        IO0CLR = SCK_KEY;
        //if(data & 0x0001)
        if(data & 0x8000)
        {
            IO0SET = SI_KEY;
        }
        else
        {
            IO0CLR = SI_KEY;
        }
        IO0SET = SCK_KEY;
        //data >>= 1;
        data <<= 1;
        
    }
    IO0CLR = SCK_KEY;
    IO0SET = RCK_KEY;
}

U8 Scan_KEY(void)   
{

    U16 select_word; 
    select_word = 0xFFFF & (~(1 << key_scan_bit));

    SendKEY_Word(select_word);
     
    if(0 == (IO0PIN & KEY_IN))
    {

        U8 pressed_key = key_scan_bit; 
        key_scan_bit++;
        if (key_scan_bit == 16) 
        {
        	key_scan_bit = 0;    
        }
        return pressed_key; 
    }
    key_scan_bit++;
    if (key_scan_bit == 16) 
    {
    	key_scan_bit = 0;
    }
    return 0xFF; 
}

int main(void)
{
    U8 ret_key = 0xFF;    
    U8 i;                 

    Init();
    Init_key();

     
    for(i = 0; i < 8; i++) 
    {
    	display_buffer[i] = 0x00;
    }
    
    while(1)
    {
        
        ret_key = Scan_KEY(); 
        if (ret_key != 0xFF) 
        {  
          
	        for(i = 0; i < 8; i++) 
	   		{
	        	display_buffer[i] = code[ret_key];
	    	}  
        } 
        else 
        {   
          
            for(i = 0; i < 8; i++) 
            {
                display_buffer[i] = 0x00;
            }
        }
        
        Send_HalfWord(0xFF & (~(1 << jj)), display_buffer[jj]);
  
        jj++;
        if(jj == 8)
        {
            jj = 0;
        }
    }
    return 0;
}
