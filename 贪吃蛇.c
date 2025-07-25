#include "LPC2200.h"
#include "config.h"
// 点阵屏控制引脚定义：使用 P0.17 ~ P0.20
#define LATIC_SCK  (1 << 17)  // SCK: 串行时钟
#define LATIC_SI_X (1 << 18)  // SI_X: 行数据（蛇身图案）
#define LATIC_SI_Y (1 << 19)  // SI_Y: 列地址（扫描哪一行）
#define LATIC_RCK  (1 << 20)  // RCK: 锁存时钟
#define SPEAKER_PIN (1 << 24)
volatile uint8 game_running = 0;
uint32 matrix[16] = {0};
uint32 score = 0;
uint32 key_scan_bit = 0;
// 数码管显示编码
const uint8 display_code[16] = 
{
    0xFC, 0x60, 0xDA, 0xF2, 0x66, 0xB6, 0xBE, 0xE0,
    0xFE, 0xE6, 0xEE, 0x3E, 0x9C, 0x7A, 0x9E, 0x8E
};

// 移位寄存器控制引脚初始化
void Led_Init() 
{
    // 初始化SI引脚
    PINSEL1 &= ~(1 << 4);     // P0.18作为GPIO
    PINSEL1 &= ~(1 << 5);
    IO0DIR |= (1<<18);

    // 初始化RCK引脚
    PINSEL1 &= ~(1 << 8);     // P0.20作为GPIO
    PINSEL1 &= ~(1 << 9);
    IO0DIR |= (1<<20);

    // 初始化SCK引脚
    PINSEL1 &= ~(1 << 2);     // P0.17作为GPIO
    PINSEL1 &= ~(1 << 3);
    IO0DIR |= (1<<17);
}

// 发送半字节数据到移位寄存器
void Send_HalfWord(uint8 pos, uint8 data) 
{
    uint8 i;
    IO0CLR = (1<<20);        // 准备锁存数据

    // 发送位置信息（8位）
    for (i = 0; i < 8; i++) 
    {
        IO0CLR = (1<<17);
        if (pos & 0x01) 
        {
            IO0SET = (1<<18);
        } 
        else 
        {
            IO0CLR = (1<<18);
        }
        IO0SET = (1<<17);    // 上升沿移位
        pos >>= 1;
    }

    // 发送数据信息（8位）
    for (i = 0; i < 8; i++) 
    {
        IO0CLR = (1<<17);
        if (data & 0x01) 
        {
            IO0SET = (1<<18);
        } 
        else 
        {
            IO0CLR = (1<<18);
        }
        IO0SET = (1<<17);    // 上升沿移位
        data >>= 1;
    }
    IO0SET = (1<<20);        // 锁存输出数据
}

void delay(uint32 t)
{
    uint32 i,j;
    for(i=0;i<t;i++)
    {
        for(j=0;j<100000;j++)
        {
        }
    }
}
void Beep_on()
{
	IO0CLR = (1<<24);
}
void Beep_off()
{
	IO0SET = (1<<24);
}
void Beep_init(void)
{
	PINSEL1 &= ~(1<<16);
	PINSEL1 &= ~(1<<17);
	IO0DIR |= SPEAKER_PIN;
	Beep_off();
}

void gpio_init(void)
{
    // 设置 P0.17 ~ P0.20 为 GPIO
    PINSEL1 &= 0xFFFFFC03;
    IO0DIR |= (LATIC_SCK | LATIC_RCK | LATIC_SI_X | LATIC_SI_Y);
}
void latic_send(uint32 row, uint32 col)
{
    uint32 i;
    IO0CLR = LATIC_RCK;
    for (i = 0; i < 16; i++)
    {
        IO0CLR = LATIC_SCK;

        // 发送行数据
        if (row & 0x8000)
        {
            IO0SET = LATIC_SI_X;
        }
        else
        {
            IO0CLR = LATIC_SI_X;
        }

        // 发送列数据
        if (col & 0x8000)
        {
            IO0SET = LATIC_SI_Y;
        }
        else
        {
            IO0CLR = LATIC_SI_Y;
        }

        IO0SET = LATIC_SCK;  // 上升沿送数据
        row <<= 1;
        col <<= 1;
    }
    IO0SET = LATIC_RCK;  // 锁存输出
}

void display_frame(uint32 *frame)
{
    uint32 i;
    for (i = 0; i < 16; i++)
    {
        latic_send(frame[i], ~(1 << i));
    }
}

void clear_display(void)
{
    uint32 i;
    for (i = 0; i < 16; i++)
    {
        matrix[i] = 0x0000;
    }
    display_frame(matrix);
}

void init_keyboard_gpio(void)
{

    // P0.4
    PINSEL0 &= ~(1<<8);
    PINSEL0 &= ~(1<<9);

    // P0.7
    PINSEL0 &= ~(1<<14);
    PINSEL0 &= ~(1<<15);

    // P0.6
    PINSEL0 &= ~(1<<12);
    PINSEL0 &= ~(1<<13);

    // P0.5 GPIO输入(0)功能，检测按键
    PINSEL0 &= ~(1<<10);
    PINSEL0 &= ~(1<<11);

    IO0DIR |= (1 << 6) | (1 << 7) | (1 << 4);
    IO0DIR &= ~(1 << 5);
}

//键盘发送函数，按下键盘时扫描对应键码的键盘
void send_keyboard_word(uint32 data)
{
    uint32 i;
    IO0CLR = (1 << 7);
    for (i = 0; i < 16; i++)
    {
        IO0CLR = (1 << 4);
        if (data & 0x8000)
        {
            IO0SET = (1 << 6);
        }
        else
        {
            IO0CLR = (1 << 6);
        }
        IO0SET = (1 << 4);
        data <<= 1;
    }
    IO0SET = (1 << 7);
}

uint32 read_direction(void)
{

    uint32 select_word = ~(1 << key_scan_bit);
    //发送键码，选通对应的键盘，扫描对应位
    send_keyboard_word(select_word);

    if (!(IO0PIN & (1 << 5)))
    {
    	uint32 pressed_key = key_scan_bit;
        //有按键按下
        key_scan_bit = (key_scan_bit + 1) % 16;
        switch (pressed_key)
        {
            case 10:
                return 10; // 下
            case 11:
                return 11; // 上
            case 14:
                return 14; // 左
            case 15:
                return 15; // 右
            default:
                return 255;
        }
    }
    else
    {
        //无按键按下
        key_scan_bit = (key_scan_bit + 1) % 16;
        return 255;
    }
}

// ---- Snake Logic ----

#define MAX_LENGTH 64
uint32 snake_x[MAX_LENGTH], snake_y[MAX_LENGTH];
uint32 length = 3;
uint32 dir = 15; // 初始向右
uint32 food_x, food_y;

void init_snake(void)
{
	uint32 i;
    length = 3;
    for (i = 0; i < length; i++)
    {
        snake_x[i] = 5 - i;
        snake_y[i] = 5;
    }
    dir = 15;
}

void generate_food(void)
{
    //食物位置，通过定时器0伪随机生成
    //T0TC，定时计数器0的TC值
    food_x = 1 + (T0TC % 14);
    food_y = 1 + (T0TC / 14 % 14);
}

void set_direction(uint8 d)
{
    if ((dir == 10 && d != 11) || (dir == 11 && d != 10) ||(dir == 14 && d != 15) || (dir == 15 && d != 14))
    {
        dir = d;
    }
}

uint8 update_snake(void)
{
	uint32 i;
    //蛇头的新位置
    uint32 new_x = snake_x[0];
    uint32 new_y = snake_y[0];
    if (dir == 10)
    {
        new_y--;
    }
    else if (dir == 11)
    {
        new_y++;
    }
    else if (dir == 14)
    {
        new_x--;
    }
    else if (dir == 15)
    {
        new_x++;
    }
    //检查是否撞到边界
    if (new_x > 15 || new_y > 15 || new_x < 0 || new_y <0 )
    {
        return 0;
    }
    //检查是否撞到自身
    for (i = 0; i < length; i++)
    {
        if (snake_x[i] == new_x && snake_y[i] == new_y)
        {
            return 0;
        }
    }
    matrix[snake_y[length-1]] &= ~(1<<(snake_x[length - 1]));
    //更新蛇的位置
    for (i = length; i > 0; i--)
    {
        snake_x[i] = snake_x[i - 1];
        snake_y[i] = snake_y[i - 1];
    }
    snake_x[0] = new_x;
    snake_y[0] = new_y;
    if (new_x == food_x && new_y == food_y)
    {
    	Beep_on();
        if (length < MAX_LENGTH - 1)
        {
            length++;
        }
        score++;
        delay(10);
        Beep_off();
        matrix[food_y] &= ~(1<<(food_x));
        generate_food();
    }
    return 1;
}

//渲染蛇和食物的位置，以及分数
void render_snake(void)
{
    uint32 i;
    for (i = 0; i < length; i++)
    {
        matrix[snake_y[i]] |= (1 << (snake_x[i]));
    }
    matrix[food_y] |= (1 << (food_x));
    //显示具体实现应由数码管实现
}


void __irq TIMER0_IRQHandler(void)
{
    uint32 i,j,k,f;
    Send_HalfWord(0xFF,display_code[score]);
    if (game_running)
    {
        f=update_snake();
        if (f)
        {
        	render_snake();
            for(i=0;i<2000;i++)
            {
                display_frame(matrix);
            }
            
        }
        else
        {
            //游戏结束
            game_running = 0;
            //闪烁显示当前状态，提示游戏结束
            for (i = 0; i < 3; i++)
            {
            	Beep_on();
                for (j = 0; j < 700; j++)
                {
                    for(k=0;k<16;k++)
                    {
                         latic_send(0x0000, ~(1 << j));
                    }
                }
                Beep_off();
                for (j = 0; j < 700; j++)
                {
                   display_frame(matrix);
                }
            }
        }
    }
    //清除定时器0中断标志，写“1”有效，复位清除中断；写“0”无效
    T0IR = 1;
    VICVectAddr = 0;
}

void init_timer0(void)
{
    //预分频
    T0PR = 0;
    T0MR0 = Fpclk/1000;
    //产生中断，TC复位
    T0MCR = 3;
    //置0，定时器0IRQ
    VICIntSelect &= ~(1 << 4);
    VICVectCntl0 = (1 << 5) | 4;
    VICVectAddr0 = (uint32)TIMER0_IRQHandler;
    //使能中断
    VICIntEnable = 1 << 4;
    //使能定时器0
    T0TCR = 1;
}

int main(void)
{
	uint32 i;
    gpio_init();
    Beep_init();
    Led_Init();
    clear_display();
    init_snake();
    init_timer0();
    generate_food();
    init_keyboard_gpio();
    game_running = 1;
	
    while (1)
    {
        uint8 dir = read_direction();
        if (dir != 255)
        {
            set_direction(dir);
        }
    }
}
